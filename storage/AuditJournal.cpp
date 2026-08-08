#include "AuditJournal.h"
#include "EventTypes.h"
#include "PostingPolicy.h"
#include "SemanticMigration.h"
#include "FaultInjection.h"
#include "CustomerRepository.h"
#include "SupplierRepository.h"
#include "InvoiceRepository.h"
#include "InvoiceLineRepository.h"
#include "ExpenseRepository.h"
#include "../core/Customer.h"
#include "../core/Supplier.h"
#include "../core/Invoice.h"
#include "../core/InvoiceLine.h"
#include "../core/Expense.h"
#include "../core/TaxCode.h"

#include <algorithm>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <vector>

#ifdef _WIN32
#  include <io.h>
#  define AJ_FSYNC(fd) _commit(fd)
#else
#  include <unistd.h>
#  define AJ_FSYNC(fd) ::fsync(fd)
#endif

// Crash hooks for the divergence windows BETWEEN the authoritative event and the
// projection/cursor. (The windows INSIDE the event append are tested by EventLog.)
static inline void ajMaybeCrash(const char* point)
{
    const char* want = std::getenv("ACCT_CRASH_POINT");
    if (want && std::strcmp(want, point) == 0) { std::fflush(nullptr); std::_Exit(99); }
}

// Forward declaration: the journal-entry encoder is defined (in this TU's anonymous namespace)
// alongside the ledger code below, but recordPayment() — earlier in the file — now authors a
// balanced posting atomically with the settlement event, so it needs the encoder here.
namespace { std::vector<char> encodeJournalEntry(uint32_t, uint32_t, IsoDate,
                                                 const std::vector<AuditJournal::PostingInput>&); }

AuditJournal::AuditJournal(std::string logPath, std::string cursorPath,
                           CustomerRepository* customers,
                           InvoiceRepository* invoices,
                           InvoiceLineRepository* lines,
                           SupplierRepository* suppliers,
                           ExpenseRepository* expenses)
    : log_(std::move(logPath))
    , cursorPath_(cursorPath)
    , snapshotPath_(cursorPath + ".ledgersnap")
    , customers_(customers)
    , suppliers_(suppliers)
    , invoices_(invoices)
    , lines_(lines)
    , expenses_(expenses)
{
    readCursor();
    rebuildPeriodIndex();
    rebuildCorrectionIndex();
    rebuildExpenseCorrectionIndex();
    rebuildSettlementIndex();
    rebuildLedgerIndex();
    rebuildTaxIndex();
    rebuildGovernanceIndex();
}

void AuditJournal::readCursor()
{
    applied_ = 0;
    FILE* f = std::fopen(cursorPath_.c_str(), "rb");
    if (!f) return;
    uint64_t v = 0;
    if (std::fread(&v, sizeof(v), 1, f) == 1) applied_ = v;
    std::fclose(f);
    // The cursor can never legitimately exceed the committed log.
    if (applied_ > log_.lastSeq()) applied_ = log_.lastSeq();
}

void AuditJournal::writeCursor(uint64_t seq)
{
    // Fault injection: the cursor advance fails AFTER the event is committed + projected.
    // On reopen the cursor is behind the log → reconcile() replays the tail (idempotent
    // upsert) and heals the projection. Proves reconcile recovery, not OS emulation.
    if (acctFaultAt("cursorWrite"))
        throw std::runtime_error("AuditJournal: injected fault at cursorWrite for " + cursorPath_);

    FILE* f = std::fopen(cursorPath_.c_str(), "wb");
    if (!f) throw std::runtime_error("AuditJournal: cannot write cursor " + cursorPath_);
    const bool ok = std::fwrite(&seq, sizeof(seq), 1, f) == 1;
    if (ok) std::fflush(f);
    if (ok) AJ_FSYNC(fileno(f));
    std::fclose(f);
    if (!ok) throw std::runtime_error("AuditJournal: cursor write failed " + cursorPath_);
    applied_ = seq;
}

// Encode an invoice transaction: [Invoice 96][u16 lineCount][InvoiceLine 128]*.
static std::vector<char> encodeInvoiceEvent(const Invoice& inv, const std::vector<InvoiceLine>& lines)
{
    const std::size_t n = lines.size();
    std::vector<char> buf(INVOICE_RECORD_SIZE + 2 + n * INVOICE_LINE_RECORD_SIZE);
    inv.serialize(buf.data());
    const uint16_t cnt = static_cast<uint16_t>(n);
    std::memcpy(buf.data() + INVOICE_RECORD_SIZE, &cnt, 2);
    char* p = buf.data() + INVOICE_RECORD_SIZE + 2;
    for (const InvoiceLine& l : lines) { l.serialize(p); p += INVOICE_LINE_RECORD_SIZE; }
    return buf;
}

// Project an invoice event (idempotent + deterministic). Lines are addressed by their
// STABLE id embedded in the event — never by position. A correction tombstones the
// invoice's live lines that are not in the new set, then upserts the new set.
static void applyInvoiceEvent(InvoiceRepository& invoices, InvoiceLineRepository& lineRepo,
                              EventType type, const std::vector<char>& payload)
{
    if (payload.size() < INVOICE_RECORD_SIZE + 2)
        throw std::runtime_error("AuditJournal: truncated Invoice event payload");
    Invoice inv;
    inv.deserialize(payload.data());
    uint16_t cnt = 0;
    std::memcpy(&cnt, payload.data() + INVOICE_RECORD_SIZE, 2);

    std::vector<InvoiceLine> newLines(cnt);
    const char* p = payload.data() + INVOICE_RECORD_SIZE + 2;
    for (uint16_t i = 0; i < cnt; ++i) { newLines[i].deserialize(p); p += INVOICE_LINE_RECORD_SIZE; }

    invoices.upsertAt(inv);

    if (type == EventType::InvoiceCorrected) {
        // Tombstone live lines of this invoice that the correction no longer includes.
        for (const InvoiceLine& cur : lineRepo.findByInvoice(inv.getId())) {
            const bool kept = std::any_of(newLines.begin(), newLines.end(),
                [&](const InvoiceLine& nl){ return nl.getId() == cur.getId(); });
            if (!kept) lineRepo.remove(cur.getId());
        }
    }
    for (const InvoiceLine& l : newLines) lineRepo.upsertAt(l);
}

// Project one event (idempotent + deterministic — no wall-clock/random — so replay
// rebuilds an identical projection). Invoice events are skipped when inv/lns are null
// (e.g. a customer-only verification/reconstruction).
void AuditJournal::apply(CustomerRepository& cust, SupplierRepository* sup,
                         InvoiceRepository* inv, InvoiceLineRepository* lns,
                         ExpenseRepository* exp, const EventRecord& r)
{
    switch (static_cast<EventType>(r.type)) {
    case EventType::CustomerCreated:
    case EventType::CustomerRenamed:
    case EventType::CustomerUpdated: {
        if (r.payload.size() < CUSTOMER_RECORD_SIZE)
            throw std::runtime_error("AuditJournal: truncated Customer event payload");
        Customer c;
        c.deserialize(r.payload.data());
        cust.upsertAt(c);
        break;
    }
    case EventType::SupplierCreated:
    case EventType::SupplierUpdated: {
        if (sup) {   // null → not projecting suppliers in this pass
            if (r.payload.size() < SUPPLIER_RECORD_SIZE)
                throw std::runtime_error("AuditJournal: truncated Supplier event payload");
            Supplier s;
            s.deserialize(r.payload.data());
            sup->upsertAt(s);
        }
        break;
    }
    case EventType::InvoiceCreated:
    case EventType::InvoiceCorrected:
        if (inv && lns) applyInvoiceEvent(*inv, *lns, static_cast<EventType>(r.type), r.payload);
        break;   // null repos → not projecting invoices in this pass (customer-only)
    case EventType::PeriodClosed:
    case EventType::PeriodReopened:
        break;   // period events don't affect entity projections — the closed-period
                 // index is a separate disposable projection (rebuildPeriodIndex()).
    case EventType::InvoiceVoided:
        if (inv && r.payload.size() >= 4) {
            uint32_t tid = 0; std::memcpy(&tid, r.payload.data(), 4);
            Invoice t = inv->load(tid);
            t.setStatus(INVOICE_VOID);     // mark not-effective in place
            inv->upsertAt(t);
        }
        break;
    case EventType::ExpenseCreated:
    case EventType::ExpenseCorrected:
        if (exp) {   // null → not projecting expenses in this pass
            if (r.payload.size() < EXPENSE_RECORD_SIZE)
                throw std::runtime_error("AuditJournal: truncated Expense event payload");
            ::Expense e;   // ::Expense — the class (unqualified Expense = AccountType::Expense enumerator)
            e.deserialize(r.payload.data());
            exp->upsertAt(e);
        }
        break;
    case EventType::ExpenseVoided:
        if (exp && r.payload.size() >= 4) {
            uint32_t tid = 0; std::memcpy(&tid, r.payload.data(), 4);
            ::Expense t = exp->load(tid);
            t.setStatus(EXPENSE_VOID);     // mark not-effective in place
            exp->upsertAt(t);
        }
        break;
    case EventType::ExpenseReversed:   // link fact only — the negating expense is a separate ExpenseCreated
    case EventType::InvoiceReversed:
    case EventType::PaymentRecorded:
    case EventType::PaymentAllocated:
    case EventType::AllocationReversed:
    case EventType::AccountOpened:
    case EventType::JournalEntryPosted:
        break;   // settlement / ledger facts — derived indices (rebuildSettlementIndex,
                 // rebuildLedgerIndex), never entity-projection mutations.
    case EventType::EngineVersionStamp:
        break;   // governance fact — separate disposable index (rebuildGovernanceIndex),
                 // never an entity-projection mutation. Known type → not "newer than build".
    case EventType::TaxCodeCreated:
        break;   // tax-policy fact — separate disposable index (rebuildTaxIndex), never an
                 // entity-projection mutation. Known type → not "newer than build".
    default:
        // An event type this build does not understand = history written by a newer
        // version. Refuse rather than silently drop authoritative history.
        throw std::runtime_error("AuditJournal: unknown event type "
                                 + std::to_string(r.type) + " (newer than this build?)");
    }
}

uint64_t AuditJournal::recordCustomerCreated(Customer& customer, int64_t timestampMs)
{
    // Deterministic id = next projection slot. Embedded in the event → idempotent.
    customer.setId(static_cast<uint32_t>(customers_->count()));

    char buf[CUSTOMER_RECORD_SIZE];
    customer.serialize(buf);

    const uint64_t seq = log_.append(static_cast<uint16_t>(EventType::CustomerCreated),
                                     1, timestampMs, buf, CUSTOMER_RECORD_SIZE);
    ajMaybeCrash("afterEventBeforeProject");   // event committed, projection not yet updated

    EventRecord r; r.type = static_cast<uint16_t>(EventType::CustomerCreated); r.seq = seq;
    r.payload.assign(buf, buf + CUSTOMER_RECORD_SIZE);
    apply(*customers_, suppliers_, invoices_, lines_, expenses_, r);
    ajMaybeCrash("afterProjectBeforeCursor");  // projection updated, cursor not advanced

    writeCursor(seq);
    return seq;
}

uint64_t AuditJournal::recordCustomerRenamed(uint32_t customerId, const std::string& newName, int64_t timestampMs)
{
    Customer c = customers_->load(customerId);
    c.setName(newName.c_str());

    char buf[CUSTOMER_RECORD_SIZE];
    c.serialize(buf);

    const uint64_t seq = log_.append(static_cast<uint16_t>(EventType::CustomerRenamed),
                                     1, timestampMs, buf, CUSTOMER_RECORD_SIZE);
    ajMaybeCrash("afterEventBeforeProject");

    EventRecord r; r.type = static_cast<uint16_t>(EventType::CustomerRenamed); r.seq = seq;
    r.payload.assign(buf, buf + CUSTOMER_RECORD_SIZE);
    apply(*customers_, suppliers_, invoices_, lines_, expenses_, r);
    ajMaybeCrash("afterProjectBeforeCursor");

    writeCursor(seq);
    return seq;
}

uint64_t AuditJournal::recordCustomerUpdated(const Customer& customer, int64_t timestampMs)
{
    char buf[CUSTOMER_RECORD_SIZE];
    customer.serialize(buf);

    const uint64_t seq = log_.append(static_cast<uint16_t>(EventType::CustomerUpdated),
                                     1, timestampMs, buf, CUSTOMER_RECORD_SIZE);
    ajMaybeCrash("afterEventBeforeProject");

    EventRecord r; r.type = static_cast<uint16_t>(EventType::CustomerUpdated); r.seq = seq;
    r.payload.assign(buf, buf + CUSTOMER_RECORD_SIZE);
    apply(*customers_, suppliers_, invoices_, lines_, expenses_, r);
    ajMaybeCrash("afterProjectBeforeCursor");

    writeCursor(seq);
    return seq;
}

uint64_t AuditJournal::recordSupplierCreated(Supplier& supplier, int64_t timestampMs)
{
    if (!suppliers_) throw std::logic_error("AuditJournal: supplier repository not configured");
    // Deterministic id = next projection slot. Embedded in the event → idempotent replay.
    supplier.setId(static_cast<uint32_t>(suppliers_->count()));

    char buf[SUPPLIER_RECORD_SIZE];
    supplier.serialize(buf);

    const uint64_t seq = log_.append(static_cast<uint16_t>(EventType::SupplierCreated),
                                     1, timestampMs, buf, SUPPLIER_RECORD_SIZE);
    ajMaybeCrash("afterEventBeforeProject");

    EventRecord r; r.type = static_cast<uint16_t>(EventType::SupplierCreated); r.seq = seq;
    r.payload.assign(buf, buf + SUPPLIER_RECORD_SIZE);
    apply(*customers_, suppliers_, invoices_, lines_, expenses_, r);
    ajMaybeCrash("afterProjectBeforeCursor");

    writeCursor(seq);
    return seq;
}

uint64_t AuditJournal::recordSupplierUpdated(const Supplier& supplier, int64_t timestampMs)
{
    if (!suppliers_) throw std::logic_error("AuditJournal: supplier repository not configured");
    char buf[SUPPLIER_RECORD_SIZE];
    supplier.serialize(buf);

    const uint64_t seq = log_.append(static_cast<uint16_t>(EventType::SupplierUpdated),
                                     1, timestampMs, buf, SUPPLIER_RECORD_SIZE);
    ajMaybeCrash("afterEventBeforeProject");

    EventRecord r; r.type = static_cast<uint16_t>(EventType::SupplierUpdated); r.seq = seq;
    r.payload.assign(buf, buf + SUPPLIER_RECORD_SIZE);
    apply(*customers_, suppliers_, invoices_, lines_, expenses_, r);
    ajMaybeCrash("afterProjectBeforeCursor");

    writeCursor(seq);
    return seq;
}

uint64_t AuditJournal::backfillCustomers(int64_t timestampMs)
{
    // One-time adoption of pre-cutover projection state into authoritative history.
    // Only runs when the log is empty but the projection already holds records (e.g.
    // an upgrade from a pre-audit build, or non-event seeding). After this the log
    // fully accounts for the projection, so a rebuild can never lose those records.
    if (log_.lastSeq() != 0) return 0;
    const std::size_t n = customers_->count();
    uint64_t filled = 0;
    for (std::size_t i = 0; i < n; ++i) {
        // Emit one event per SLOT (including soft-deleted records) so ids stay stable
        // and a rebuild reproduces the projection exactly — skipping any slot would
        // shift later ids and break invoice→customer foreign keys.
        Customer c = customers_->load(static_cast<uint32_t>(i));
        char buf[CUSTOMER_RECORD_SIZE];
        c.serialize(buf);
        const uint64_t seq = log_.append(static_cast<uint16_t>(EventType::CustomerCreated),
                                         1, timestampMs, buf, CUSTOMER_RECORD_SIZE);
        // Project (idempotent: the record already exists at this id → update, same bytes).
        EventRecord r; r.type = static_cast<uint16_t>(EventType::CustomerCreated); r.seq = seq;
        r.payload.assign(buf, buf + CUSTOMER_RECORD_SIZE);
        apply(*customers_, suppliers_, invoices_, lines_, expenses_, r);
        writeCursor(seq);
        ++filled;
    }
    return filled;
}

// True if history already carries at least one event of `type` (per-entity cutover gate).
bool AuditJournal::hasEventOfType(uint16_t type)
{
    bool found = false;
    log_.forEach([&](const EventRecord& r) {
        if (r.type == type) { found = true; return false; }   // stop at the first
        return true;
    });
    return found;
}

uint64_t AuditJournal::backfillSuppliers(int64_t timestampMs)
{
    // Adopt only if history carries NO SupplierCreated event yet, but the repo has records
    // (an old direct-persistence path). Emit one event per SLOT so ids stay stable.
    if (!suppliers_) return 0;
    if (hasEventOfType(static_cast<uint16_t>(EventType::SupplierCreated))) return 0;
    const std::size_t n = suppliers_->count();
    uint64_t filled = 0;
    for (std::size_t i = 0; i < n; ++i) {
        Supplier s = suppliers_->load(static_cast<uint32_t>(i));
        char buf[SUPPLIER_RECORD_SIZE];
        s.serialize(buf);
        const uint64_t seq = log_.append(static_cast<uint16_t>(EventType::SupplierCreated),
                                         1, timestampMs, buf, SUPPLIER_RECORD_SIZE);
        EventRecord r; r.type = static_cast<uint16_t>(EventType::SupplierCreated); r.seq = seq;
        r.payload.assign(buf, buf + SUPPLIER_RECORD_SIZE);
        apply(*customers_, suppliers_, invoices_, lines_, expenses_, r);   // idempotent (same bytes at same id)
        writeCursor(seq);
        ++filled;
    }
    return filled;
}

uint64_t AuditJournal::backfillInvoices(int64_t timestampMs)
{
    // Adopt directly-persisted invoices (+ their lines) into history as InvoiceCreated
    // events, one per invoice slot. After this the caller canonicalises the projection
    // (rebuildProjections) so live == a pure replay of history.
    if (!invoices_ || !lines_) return 0;
    if (hasEventOfType(static_cast<uint16_t>(EventType::InvoiceCreated))) return 0;
    const std::size_t n = invoices_->count();
    uint64_t filled = 0;
    for (std::size_t i = 0; i < n; ++i) {
        Invoice inv = invoices_->load(static_cast<uint32_t>(i));
        std::vector<InvoiceLine> lns = lines_->findByInvoice(inv.getId());
        const std::vector<char> payload = encodeInvoiceEvent(inv, lns);
        const uint64_t seq = log_.append(static_cast<uint16_t>(EventType::InvoiceCreated),
                                         1, timestampMs, payload.data(),
                                         static_cast<uint32_t>(payload.size()));
        EventRecord r; r.type = static_cast<uint16_t>(EventType::InvoiceCreated); r.seq = seq; r.payload = payload;
        apply(*customers_, suppliers_, invoices_, lines_, expenses_, r);
        writeCursor(seq);
        ++filled;
    }
    return filled;
}

uint64_t AuditJournal::reconcile()
{
    readCursor();
    uint64_t count = 0;
    log_.forEachAfter(applied_, [&](const EventRecord& r) {
        apply(*customers_, suppliers_, invoices_, lines_, expenses_, r);
        writeCursor(r.seq);     // advance per event so a crash mid-catch-up resumes cleanly
        ++count;
        return true;
    });
    rebuildPeriodIndex();
    rebuildCorrectionIndex();
    rebuildSettlementIndex();
    rebuildLedgerIndex();
    rebuildGovernanceIndex();
    return count;
}

uint64_t AuditJournal::rebuildProjections()
{
    customers_->clear();
    if (suppliers_) suppliers_->clear();
    if (invoices_) invoices_->clear();
    if (lines_)    lines_->clear();
    if (expenses_) expenses_->clear();
    writeCursor(0);
    log_.forEach([&](const EventRecord& r) { apply(*customers_, suppliers_, invoices_, lines_, expenses_, r); return true; });
    writeCursor(log_.lastSeq());
    rebuildPeriodIndex();
    rebuildCorrectionIndex();
    rebuildExpenseCorrectionIndex();
    rebuildSettlementIndex();
    rebuildLedgerIndex();
    rebuildTaxIndex();
    rebuildGovernanceIndex();
    return log_.lastSeq();
}

uint64_t AuditJournal::recordInvoiceCreated(Invoice& invoice, std::vector<InvoiceLine>& lines, int64_t timestampMs)
{
    if (!invoices_ || !lines_)
        throw std::logic_error("AuditJournal: invoice repositories not configured");

    // Assign STABLE ids at authoring: invoice id = next invoice slot; each line id =
    // next monotonic line slot. Embedded in the event → replay reproduces them exactly.
    invoice.setId(static_cast<uint32_t>(invoices_->count()));
    uint32_t nextLineId = static_cast<uint32_t>(lines_->count());
    for (InvoiceLine& l : lines) {
        l.setInvoiceId(invoice.getId());
        l.setId(nextLineId++);
    }

    const std::vector<char> payload = encodeInvoiceEvent(invoice, lines);
    const uint64_t seq = log_.append(static_cast<uint16_t>(EventType::InvoiceCreated),
                                     1, timestampMs, payload.data(),
                                     static_cast<uint32_t>(payload.size()));
    ajMaybeCrash("afterEventBeforeProject");

    EventRecord r; r.type = static_cast<uint16_t>(EventType::InvoiceCreated); r.seq = seq; r.payload = payload;
    apply(*customers_, suppliers_, invoices_, lines_, expenses_, r);
    ajMaybeCrash("afterProjectBeforeCursor");

    writeCursor(seq);
    return seq;
}

uint64_t AuditJournal::recordInvoiceCorrected(Invoice& invoice, std::vector<InvoiceLine>& lines, int64_t timestampMs)
{
    if (!invoices_ || !lines_)
        throw std::logic_error("AuditJournal: invoice repositories not configured");

    // Historical finality: a transaction whose effective (issue) date falls in a closed
    // period is FROZEN. Correcting it is forbidden — the operator posts an append-only
    // adjustment in an open period instead. We never mutate or reopen history here.
    if (isInvoiceInClosedPeriod(invoice.getId()) || isDateInClosedPeriod(invoice.getIssueDate()))
        throw std::runtime_error(
            "AuditJournal: invoice " + std::to_string(invoice.getId())
            + " is in a closed accounting period — post an adjustment, do not correct it");

    // Existing lines keep their STABLE id; NEW lines (id == UINT32_MAX) get the next
    // monotonic slot. Correction targets identity, never position.
    uint32_t nextLineId = static_cast<uint32_t>(lines_->count());
    for (InvoiceLine& l : lines) {
        l.setInvoiceId(invoice.getId());
        if (l.getId() == UINT32_MAX) l.setId(nextLineId++);
    }

    const std::vector<char> payload = encodeInvoiceEvent(invoice, lines);
    const uint64_t seq = log_.append(static_cast<uint16_t>(EventType::InvoiceCorrected),
                                     1, timestampMs, payload.data(),
                                     static_cast<uint32_t>(payload.size()));
    ajMaybeCrash("afterEventBeforeProject");

    EventRecord r; r.type = static_cast<uint16_t>(EventType::InvoiceCorrected); r.seq = seq; r.payload = payload;
    apply(*customers_, suppliers_, invoices_, lines_, expenses_, r);
    ajMaybeCrash("afterProjectBeforeCursor");

    writeCursor(seq);
    return seq;
}

// ── Accounting period closure ────────────────────────────────────────────────
namespace {
constexpr std::size_t kLabelLen     = 32;
constexpr std::size_t kClosedBytes  = kLabelLen + 10 + 10 + 8;   // label, start, end, seq

void writeLabel(char* dst, const std::string& label)
{
    std::memset(dst, 0, kLabelLen);
    std::strncpy(dst, label.c_str(), kLabelLen - 1);
}
std::string readLabel(const char* src)
{
    char tmp[kLabelLen]; std::memcpy(tmp, src, kLabelLen); tmp[kLabelLen - 1] = '\0';
    return std::string(tmp);
}
void writeDate(char* dst, IsoDate d)
{
    std::memset(dst, 0, 10);
    const std::string s = d.toString();
    std::memcpy(dst, s.data(), s.size() < 10 ? s.size() : 10);
}
IsoDate readDate(const char* src)
{
    char tmp[11] = {}; std::memcpy(tmp, src, 10);
    return IsoDate::fromString(tmp).value_or(IsoDate{});
}
} // namespace

void AuditJournal::rebuildPeriodIndex()
{
    closedPeriods_.clear();
    log_.forEach([&](const EventRecord& r) {
        if (static_cast<EventType>(r.type) == EventType::PeriodClosed &&
            r.payload.size() >= kClosedBytes) {
            ClosedPeriod p;
            const std::string label = readLabel(r.payload.data());
            p.start = readDate(r.payload.data() + kLabelLen);
            p.end   = readDate(r.payload.data() + kLabelLen + 10);
            std::memcpy(&p.closedAtSeq, r.payload.data() + kLabelLen + 20, 8);
            p.open = false;
            closedPeriods_[label] = p;             // latest close wins
        } else if (static_cast<EventType>(r.type) == EventType::PeriodReopened &&
                   r.payload.size() >= kLabelLen) {
            const std::string label = readLabel(r.payload.data());
            auto it = closedPeriods_.find(label);
            if (it != closedPeriods_.end()) it->second.open = true;
        }
        return true;
    });
}

uint64_t AuditJournal::closePeriod(const std::string& label, IsoDate start, IsoDate end, int64_t timestampMs)
{
    char buf[kClosedBytes];
    writeLabel(buf, label);
    writeDate(buf + kLabelLen, start);
    writeDate(buf + kLabelLen + 10, end);
    const uint64_t closedAtSeq = log_.lastSeq();   // freeze the books as of the current head
    std::memcpy(buf + kLabelLen + 20, &closedAtSeq, 8);

    const uint64_t seq = log_.append(static_cast<uint16_t>(EventType::PeriodClosed),
                                     1, timestampMs, buf, static_cast<uint32_t>(kClosedBytes));
    ajMaybeCrash("afterEventBeforeProject");
    writeCursor(seq);                 // period events don't touch entity projections
    rebuildPeriodIndex();
    return seq;
}

uint64_t AuditJournal::reopenPeriod(const std::string& label, int64_t timestampMs)
{
    char buf[kLabelLen];
    writeLabel(buf, label);
    const uint64_t seq = log_.append(static_cast<uint16_t>(EventType::PeriodReopened),
                                     1, timestampMs, buf, static_cast<uint32_t>(kLabelLen));
    writeCursor(seq);
    rebuildPeriodIndex();
    return seq;
}

bool AuditJournal::isDateInClosedPeriod(IsoDate d) const
{
    if (!d.isValid()) return false;
    for (const auto& [label, p] : closedPeriods_) {
        (void)label;
        if (!p.open && p.start.isValid() && p.end.isValid() && d >= p.start && d <= p.end)
            return true;
    }
    return false;
}

bool AuditJournal::isInvoiceInClosedPeriod(uint32_t invoiceId)
{
    if (!invoices_) return false;
    return isDateInClosedPeriod(invoices_->load(invoiceId).getIssueDate());
}

uint64_t AuditJournal::closedAtSeqFor(const std::string& label) const
{
    auto it = closedPeriods_.find(label);
    return (it != closedPeriods_.end() && !it->second.open) ? it->second.closedAtSeq : 0;
}

std::size_t AuditJournal::closedPeriodCount() const
{
    std::size_t n = 0;
    for (const auto& [label, p] : closedPeriods_) { (void)label; if (!p.open) ++n; }
    return n;
}

void AuditJournal::reconstructAllInto(CustomerRepository& cust, InvoiceRepository& inv,
                                      InvoiceLineRepository& lns, uint64_t uptoSeq)
{
    cust.clear(); inv.clear(); lns.clear();
    log_.forEach([&](const EventRecord& r) {
        if (r.seq > uptoSeq) return false;
        apply(cust, nullptr, &inv, &lns, nullptr, r);   // suppliers/expenses not part of this reconstruction view
        return true;
    });
}

// ── Structured corrections: void vs reversal ─────────────────────────────────
void AuditJournal::rebuildCorrectionIndex()
{
    corrections_.clear();
    log_.forEach([&](const EventRecord& r) {
        const EventType t = static_cast<EventType>(r.type);
        if ((t == EventType::InvoiceVoided || t == EventType::InvoiceReversed)
            && r.payload.size() >= 8) {
            uint32_t tid = 0, rid = 0;
            std::memcpy(&tid, r.payload.data(), 4);
            std::memcpy(&rid, r.payload.data() + 4, 4);
            if (t == EventType::InvoiceVoided)   corrections_[tid].voided     = true;
            else                                 corrections_[tid].reversedBy = rid;
        }
        return true;
    });
}

uint64_t AuditJournal::recordInvoiceVoided(uint32_t targetInvoiceId, int64_t timestampMs)
{
    if (!invoices_) throw std::logic_error("AuditJournal: invoice repositories not configured");
    // Voiding changes a transaction's standing in place → forbidden once frozen.
    if (isInvoiceInClosedPeriod(targetInvoiceId))
        throw std::runtime_error(
            "AuditJournal: invoice " + std::to_string(targetInvoiceId)
            + " is in a closed period — post a reversal, do not void it");

    char buf[8];
    std::memcpy(buf + 0, &targetInvoiceId, 4);
    const uint32_t none = 0xFFFFFFFFu; std::memcpy(buf + 4, &none, 4);

    const uint64_t seq = log_.append(static_cast<uint16_t>(EventType::InvoiceVoided),
                                     1, timestampMs, buf, 8);
    ajMaybeCrash("afterEventBeforeProject");

    EventRecord r; r.type = static_cast<uint16_t>(EventType::InvoiceVoided); r.seq = seq;
    r.payload.assign(buf, buf + 8);
    apply(*customers_, suppliers_, invoices_, lines_, expenses_, r);   // project status = VOID
    ajMaybeCrash("afterProjectBeforeCursor");

    writeCursor(seq);
    rebuildCorrectionIndex();
    return seq;
}

uint64_t AuditJournal::recordInvoiceReversal(uint32_t originalInvoiceId, Invoice& reversalInvoice,
                                             std::vector<InvoiceLine>& reversalLines, int64_t timestampMs)
{
    if (!invoices_ || !lines_)
        throw std::logic_error("AuditJournal: invoice repositories not configured");

    // The reversal is a NEW negating transaction — append-only, allowed even when the
    // original is in a closed period (this is the sanctioned post-close path). The reversal
    // invoice AND the original→reversal link are ONE fact: author them atomically so a crash
    // can never leave a negating invoice without its lineage link (or vice versa).
    reversalInvoice.setId(static_cast<uint32_t>(invoices_->count()));
    uint32_t nextLineId = static_cast<uint32_t>(lines_->count());
    for (InvoiceLine& l : reversalLines) { l.setInvoiceId(reversalInvoice.getId()); l.setId(nextLineId++); }
    const uint32_t reversalId = reversalInvoice.getId();

    const std::vector<char> invPayload = encodeInvoiceEvent(reversalInvoice, reversalLines);
    char link[8];
    std::memcpy(link + 0, &originalInvoiceId, 4);
    std::memcpy(link + 4, &reversalId, 4);

    std::vector<EventLog::FrameSpec> group;
    group.push_back({ static_cast<uint16_t>(EventType::InvoiceCreated),  1, timestampMs, invPayload });
    group.push_back({ static_cast<uint16_t>(EventType::InvoiceReversed), 1, timestampMs,
                      std::vector<char>(link, link + 8) });
    const std::vector<uint64_t> seqs = log_.appendAtomic(group);

    { EventRecord r; r.type = static_cast<uint16_t>(EventType::InvoiceCreated); r.seq = seqs[0];
      r.payload = invPayload; apply(*customers_, suppliers_, invoices_, lines_, expenses_, r); }   // project the reversal invoice
    ajMaybeCrash("afterProjectBeforeCursor");

    writeCursor(seqs.back());
    rebuildCorrectionIndex();   // scans InvoiceReversed → original→reversal lineage
    return seqs.back();
}

bool AuditJournal::isVoided(uint32_t invoiceId) const
{
    auto it = corrections_.find(invoiceId);
    return it != corrections_.end() && it->second.voided;
}

uint32_t AuditJournal::reversedBy(uint32_t invoiceId) const
{
    auto it = corrections_.find(invoiceId);
    return it != corrections_.end() ? it->second.reversedBy : 0xFFFFFFFFu;
}

std::size_t AuditJournal::correctionCount() const
{
    std::size_t n = 0;
    for (const auto& [id, info] : corrections_) {
        (void)id;
        if (info.voided) ++n;
        if (info.reversedBy != 0xFFFFFFFFu) ++n;
    }
    return n;
}

// ── Reconciliation & allocation ──────────────────────────────────────────────
void AuditJournal::rebuildSettlementIndex()
{
    payments_.clear();
    allocations_.clear();
    log_.forEach([&](const EventRecord& r) {
        const EventType t = static_cast<EventType>(r.type);
        if (t == EventType::PaymentRecorded && r.payload.size() >= 26) {
            uint32_t pid = 0, cid = 0; int64_t amt = 0;
            std::memcpy(&pid, r.payload.data() + 0, 4);
            std::memcpy(&cid, r.payload.data() + 4, 4);
            std::memcpy(&amt, r.payload.data() + 8, 8);
            payments_[pid] = { cid, amt, readDate(r.payload.data() + 16) };
        } else if (t == EventType::PaymentAllocated && r.payload.size() >= 30) {
            uint32_t aid = 0, pid = 0, iid = 0; int64_t amt = 0;
            std::memcpy(&aid, r.payload.data() + 0, 4);
            std::memcpy(&pid, r.payload.data() + 4, 4);
            std::memcpy(&iid, r.payload.data() + 8, 4);
            std::memcpy(&amt, r.payload.data() + 12, 8);
            allocations_[aid] = { pid, iid, amt, readDate(r.payload.data() + 20), false };
        } else if (t == EventType::AllocationReversed && r.payload.size() >= 4) {
            uint32_t aid = 0; std::memcpy(&aid, r.payload.data(), 4);
            auto it = allocations_.find(aid);
            if (it != allocations_.end()) it->second.reversed = true;
        }
        return true;
    });
}

uint32_t AuditJournal::recordPayment(uint32_t customerId, int64_t amountCents, IsoDate effectiveDate, int64_t timestampMs)
{
    const uint32_t pid = static_cast<uint32_t>(payments_.size());   // stable monotonic id
    char buf[26];
    std::memcpy(buf + 0, &pid, 4);
    std::memcpy(buf + 4, &customerId, 4);
    std::memcpy(buf + 8, &amountCents, 8);
    writeDate(buf + 16, effectiveDate);

    // AUTHORITY: the settlement fact AND its accounting posting are ONE atomic event group — a
    // received customer payment is Dr Cash / Cr Accounts Receivable (posting-policy paymentReceipt),
    // so a crash can never leave the payment recorded without its ledger effect. The posting is
    // included iff the amount is non-zero, the Cash/AR roles are bound, and the date is open.
    std::vector<EventLog::FrameSpec> group;
    group.push_back({ static_cast<uint16_t>(EventType::PaymentRecorded), 1, timestampMs,
                      std::vector<char>(buf, buf + 26) });

    const bool willPost = amountCents != 0 && roleCash_ != 0xFFFFFFFFu && roleReceivable_ != 0xFFFFFFFFu
                       && !isDateInClosedPeriod(effectiveDate);
    if (willPost) {
        const uint32_t postEntryId = static_cast<uint32_t>(entries_.size());
        posting::Roles roles;
        roles.receivable = roleReceivable_; roles.revenue = roleRevenue_; roles.cash = roleCash_;
        roles.expense = roleExpense_; roles.payable = rolePayable_;
        roles.taxPayable = roleTaxPayable_; roles.recoverableTax = roleRecoverableTax_;
        const auto plines = posting::paymentReceipt(posting::kCurrentPostingPolicyVersion, amountCents, roles);
        std::vector<PostingInput> postings;
        for (const auto& l : plines) postings.push_back({ l.accountId, l.amountCents });
        std::vector<char> postPayload = encodeJournalEntry(postEntryId, 0xFFFFFFFFu, effectiveDate, postings);
        group.push_back({ static_cast<uint16_t>(EventType::JournalEntryPosted),
                          1, timestampMs, std::move(postPayload) });
    }

    const std::vector<uint64_t> seqs = log_.appendAtomic(group);
    ajMaybeCrash("afterEventBeforeProject");
    rebuildSettlementIndex();
    if (willPost) rebuildLedgerIndex();
    writeCursor(seqs.back());
    return pid;
}

uint32_t AuditJournal::allocatePayment(uint32_t paymentId, uint32_t invoiceId, int64_t amountCents,
                                       IsoDate effectiveDate, int64_t timestampMs)
{
    if (amountCents <= 0)
        throw std::runtime_error("AuditJournal: allocation amount must be positive");
    if (amountCents > unallocatedFor(paymentId))
        throw std::runtime_error("AuditJournal: allocation exceeds the payment's unallocated balance");

    const uint32_t aid = static_cast<uint32_t>(allocations_.size());
    char buf[30];
    std::memcpy(buf + 0, &aid, 4);
    std::memcpy(buf + 4, &paymentId, 4);
    std::memcpy(buf + 8, &invoiceId, 4);
    std::memcpy(buf + 12, &amountCents, 8);
    writeDate(buf + 20, effectiveDate);
    const uint64_t seq = log_.append(static_cast<uint16_t>(EventType::PaymentAllocated), 1, timestampMs, buf, 30);
    ajMaybeCrash("afterEventBeforeProject");
    writeCursor(seq);
    rebuildSettlementIndex();
    return aid;
}

uint64_t AuditJournal::reverseAllocation(uint32_t allocationId, int64_t timestampMs)
{
    auto it = allocations_.find(allocationId);
    if (it == allocations_.end())
        throw std::runtime_error("AuditJournal: no such allocation " + std::to_string(allocationId));
    if (it->second.reversed)
        throw std::runtime_error("AuditJournal: allocation " + std::to_string(allocationId) + " already reversed");
    // A settlement whose effective date is frozen can't be reversed in place — post a
    // compensating allocation instead (consistent with closed-period correction rules).
    if (isDateInClosedPeriod(it->second.effectiveDate))
        throw std::runtime_error(
            "AuditJournal: allocation " + std::to_string(allocationId)
            + " is in a closed period — post a compensating allocation, do not reverse it");

    char buf[4]; std::memcpy(buf, &allocationId, 4);
    const uint64_t seq = log_.append(static_cast<uint16_t>(EventType::AllocationReversed), 1, timestampMs, buf, 4);
    ajMaybeCrash("afterEventBeforeProject");
    writeCursor(seq);
    rebuildSettlementIndex();
    return seq;
}

int64_t AuditJournal::settledFor(uint32_t invoiceId) const
{
    int64_t s = 0;
    for (const auto& [aid, a] : allocations_) { (void)aid; if (a.invoiceId == invoiceId && !a.reversed) s += a.amountCents; }
    return s;
}

int64_t AuditJournal::outstandingFor(uint32_t invoiceId)
{
    const int64_t total = invoices_ ? invoices_->load(invoiceId).getTotal().cents() : 0;
    return total - settledFor(invoiceId);
}

int64_t AuditJournal::unallocatedFor(uint32_t paymentId) const
{
    auto pit = payments_.find(paymentId);
    if (pit == payments_.end()) return 0;
    int64_t allocated = 0;
    for (const auto& [aid, a] : allocations_) { (void)aid; if (a.paymentId == paymentId && !a.reversed) allocated += a.amountCents; }
    return pit->second.amountCents - allocated;
}

// ── Read-only settlement enumeration (UI-facing projections) ─────────────────
std::vector<AuditJournal::PaymentRow> AuditJournal::listPayments() const
{
    std::vector<PaymentRow> out; out.reserve(payments_.size());
    for (const auto& [id, p] : payments_)
        out.push_back({ id, p.customerId, p.amountCents, p.effectiveDate });
    return out;
}

std::vector<AuditJournal::AllocationRow> AuditJournal::allocationsForPayment(uint32_t paymentId) const
{
    std::vector<AllocationRow> out;
    for (const auto& [id, a] : allocations_)
        if (a.paymentId == paymentId)
            out.push_back({ id, a.paymentId, a.invoiceId, a.amountCents, a.effectiveDate, a.reversed });
    return out;
}

std::vector<AuditJournal::AllocationRow> AuditJournal::allocationsForInvoice(uint32_t invoiceId) const
{
    std::vector<AllocationRow> out;
    for (const auto& [id, a] : allocations_)
        if (a.invoiceId == invoiceId)
            out.push_back({ id, a.paymentId, a.invoiceId, a.amountCents, a.effectiveDate, a.reversed });
    return out;
}

int64_t AuditJournal::totalPaidByCustomer(uint32_t customerId) const
{
    int64_t s = 0;
    for (const auto& [id, p] : payments_) { (void)id; if (p.customerId == customerId) s += p.amountCents; }
    return s;
}

int64_t AuditJournal::creditForCustomer(uint32_t customerId) const
{
    // Money received not yet applied to an invoice = credit (Σ unallocated of the customer's payments).
    int64_t credit = 0;
    for (const auto& [pid, p] : payments_)
        if (p.customerId == customerId) credit += unallocatedFor(pid);
    return credit;
}

int64_t AuditJournal::settledAt(uint32_t invoiceId, uint64_t uptoSeq)
{
    // Historical settlement: replay allocation/reversal events with seq ≤ uptoSeq.
    std::map<uint32_t, AllocationInfo> tmp;
    log_.forEach([&](const EventRecord& r) {
        if (r.seq > uptoSeq) return false;
        const EventType t = static_cast<EventType>(r.type);
        if (t == EventType::PaymentAllocated && r.payload.size() >= 30) {
            uint32_t aid = 0, pid = 0, iid = 0; int64_t amt = 0;
            std::memcpy(&aid, r.payload.data() + 0, 4);
            std::memcpy(&pid, r.payload.data() + 4, 4);
            std::memcpy(&iid, r.payload.data() + 8, 4);
            std::memcpy(&amt, r.payload.data() + 12, 8);
            tmp[aid] = { pid, iid, amt, IsoDate{}, false };
        } else if (t == EventType::AllocationReversed && r.payload.size() >= 4) {
            uint32_t aid = 0; std::memcpy(&aid, r.payload.data(), 4);
            auto it = tmp.find(aid);
            if (it != tmp.end()) it->second.reversed = true;
        }
        return true;
    });
    int64_t s = 0;
    for (const auto& [aid, a] : tmp) { (void)aid; if (a.invoiceId == invoiceId && !a.reversed) s += a.amountCents; }
    return s;
}

// ── Double-entry ledger ──────────────────────────────────────────────────────
namespace {
std::vector<char> encodeJournalEntry(uint32_t entryId, uint32_t reverses, IsoDate date,
                                     const std::vector<AuditJournal::PostingInput>& postings)
{
    const uint16_t n = static_cast<uint16_t>(postings.size());
    std::vector<char> buf(20 + 12u * n);
    std::memcpy(buf.data() + 0, &entryId, 4);
    std::memcpy(buf.data() + 4, &reverses, 4);
    writeDate(buf.data() + 8, date);
    std::memcpy(buf.data() + 18, &n, 2);
    char* p = buf.data() + 20;
    for (const auto& post : postings) {
        std::memcpy(p + 0, &post.accountId, 4);
        std::memcpy(p + 4, &post.amountCents, 8);
        p += 12;
    }
    return buf;
}
} // namespace

void AuditJournal::rebuildLedgerIndex()
{
    accounts_.clear(); entries_.clear(); balances_.clear();
    log_.forEach([&](const EventRecord& r) {
        const EventType t = static_cast<EventType>(r.type);
        if (t == EventType::AccountOpened && r.payload.size() >= 36) {
            uint32_t aid = 0; std::memcpy(&aid, r.payload.data(), 4);
            const uint8_t type = static_cast<uint8_t>(r.payload[4]);
            char name[32] = {}; std::memcpy(name, r.payload.data() + 5, 31);
            accounts_[aid] = { type, std::string(name) };
        } else if (t == EventType::JournalEntryPosted && r.payload.size() >= 20) {
            uint32_t eid = 0, rev = 0; uint16_t n = 0;
            std::memcpy(&eid, r.payload.data() + 0, 4);
            std::memcpy(&rev, r.payload.data() + 4, 4);
            std::memcpy(&n,   r.payload.data() + 18, 2);
            EntryInfo e; e.effectiveDate = readDate(r.payload.data() + 8); e.reverses = rev;
            const char* p = r.payload.data() + 20;
            for (uint16_t i = 0; i < n && p + 12 <= r.payload.data() + r.payload.size(); ++i) {
                PostingInput post;
                std::memcpy(&post.accountId,  p + 0, 4);
                std::memcpy(&post.amountCents, p + 4, 8);
                e.postings.push_back(post);
                balances_[post.accountId] += post.amountCents;
                p += 12;
            }
            entries_[eid] = std::move(e);
        }
        return true;
    });
}

// ── Read-only ledger enumeration (UI-facing projections of the disposable index) ─────
std::vector<AuditJournal::AccountRow> AuditJournal::listAccounts() const
{
    std::vector<AccountRow> out; out.reserve(accounts_.size());
    for (const auto& [id, a] : accounts_)
        out.push_back({ id, a.type, a.name, balanceFor(id) });
    return out;
}

uint32_t AuditJournal::reversedByOf(uint32_t entryId) const
{
    // The (at most one) entry whose `reverses` points back at entryId. Derived, not stored.
    for (const auto& [id, e] : entries_)
        if (e.reverses == entryId) return id;
    return 0xFFFFFFFFu;
}

std::vector<AuditJournal::JournalEntryRow> AuditJournal::listJournalEntries() const
{
    std::vector<JournalEntryRow> out; out.reserve(entries_.size());
    for (const auto& [id, e] : entries_)
        out.push_back({ id, e.effectiveDate, e.reverses, reversedByOf(id), e.postings });
    return out;
}

std::vector<AuditJournal::JournalEntryRow> AuditJournal::entriesForAccount(uint32_t accountId) const
{
    std::vector<JournalEntryRow> out;
    for (const auto& [id, e] : entries_) {
        bool touches = false;
        for (const auto& p : e.postings) if (p.accountId == accountId) { touches = true; break; }
        if (touches)
            out.push_back({ id, e.effectiveDate, e.reverses, reversedByOf(id), e.postings });
    }
    return out;
}

AuditJournal::JournalEntryRow AuditJournal::entryById(uint32_t entryId) const
{
    auto it = entries_.find(entryId);
    if (it == entries_.end()) return {};   // absent → empty postings
    return { entryId, it->second.effectiveDate, it->second.reverses,
             reversedByOf(entryId), it->second.postings };
}

// ── Tax codes (event-authored, append-only policy) ───────────────────────────
void AuditJournal::rebuildTaxIndex()
{
    taxCodes_.clear();
    log_.forEach([&](const EventRecord& r) {
        if (static_cast<EventType>(r.type) == EventType::TaxCodeCreated
            && r.payload.size() >= TAX_CODE_PAYLOAD_SIZE) {
            TaxCode c;
            c.deserialize(r.payload.data());
            taxCodes_[c.id] = c;
        }
        return true;
    });
}

uint32_t AuditJournal::recordTaxCode(uint8_t type, const std::string& name, int32_t ratePermille,
                                     IsoDate effectiveDate, int64_t timestampMs)
{
    TaxCode c;
    c.id   = static_cast<uint32_t>(taxCodes_.size());   // deterministic id = next slot
    c.type = type;
    c.ratePermille = ratePermille;
    c.effectiveDate = effectiveDate;
    c.setName(name.c_str());
    // Family groups versions of the same-named logical code; a re-recorded name is a new version.
    uint16_t family = 0; uint16_t maxFamily = 0; uint16_t famVersion = 0;
    bool found = false;
    for (const auto& [id, ex] : taxCodes_) { (void)id;
        if (ex.family > maxFamily) maxFamily = ex.family;
        if (std::strcmp(ex.name, c.name) == 0) { found = true; family = ex.family;
            if (ex.version > famVersion) famVersion = ex.version; }
    }
    c.family  = found ? family : static_cast<uint16_t>(maxFamily + 1);
    c.version = found ? static_cast<uint16_t>(famVersion + 1) : uint16_t{1};

    char buf[TAX_CODE_PAYLOAD_SIZE];
    c.serialize(buf);
    const uint64_t seq = log_.append(static_cast<uint16_t>(EventType::TaxCodeCreated),
                                     1, timestampMs, buf, TAX_CODE_PAYLOAD_SIZE);
    ajMaybeCrash("afterEventBeforeProject");
    writeCursor(seq);
    rebuildTaxIndex();
    return c.id;
}

void AuditJournal::ensureDefaultTaxCodes(int64_t timestampMs)
{
    if (!taxCodes_.empty()) return;   // idempotent — only bootstrap a fresh policy
    const IsoDate epoch = IsoDate::fromString("2000-01-01").value();
    recordTaxCode(TAX_TYPE_VAT,        "Standard Rate", 150, epoch, timestampMs);  // 15.0%
    recordTaxCode(TAX_TYPE_ZERO_RATED, "Zero-rated",      0, epoch, timestampMs);
    recordTaxCode(TAX_TYPE_EXEMPT,     "Exempt",          0, epoch, timestampMs);
}

std::vector<TaxCode> AuditJournal::listTaxCodes() const
{
    std::vector<TaxCode> out; out.reserve(taxCodes_.size());
    for (const auto& [id, c] : taxCodes_) { (void)id; out.push_back(c); }
    return out;
}

TaxCode AuditJournal::taxCodeById(uint32_t id) const
{
    auto it = taxCodes_.find(id);
    return it != taxCodes_.end() ? it->second : TaxCode{};
}

int32_t AuditJournal::resolveRateAt(uint16_t family, IsoDate asOf) const
{
    // Deterministic effective-dated lookup: the highest-version code in the family whose
    // effectiveDate ≤ asOf. (Authoring captures the rate onto the line/posting, so this is only
    // consulted at authoring — history stays immutable regardless of later versions.)
    const TaxCode* best = nullptr;
    for (const auto& [id, c] : taxCodes_) { (void)id;
        if (c.family != family) continue;
        if (c.effectiveDate.isValid() && asOf.isValid() && !(c.effectiveDate <= asOf)) continue;
        if (!best || c.version > best->version) best = &c;
    }
    return best ? best->ratePermille : 0;
}

uint32_t AuditJournal::recordAccount(uint8_t type, const std::string& name, int64_t timestampMs)
{
    const uint32_t aid = static_cast<uint32_t>(accounts_.size());
    char buf[36] = {};
    std::memcpy(buf, &aid, 4);
    buf[4] = static_cast<char>(type);
    std::strncpy(buf + 5, name.c_str(), 30);
    const uint64_t seq = log_.append(static_cast<uint16_t>(EventType::AccountOpened), 1, timestampMs, buf, 36);
    ajMaybeCrash("afterEventBeforeProject");
    writeCursor(seq);
    rebuildLedgerIndex();
    return aid;
}

uint32_t AuditJournal::recordJournalEntry(IsoDate effectiveDate, const std::vector<PostingInput>& postings, int64_t timestampMs)
{
    if (postings.empty())
        throw std::runtime_error("AuditJournal: journal entry has no postings");
    int64_t sum = 0;
    for (const auto& p : postings) sum += p.amountCents;
    if (sum != 0)
        throw std::runtime_error("AuditJournal: journal entry does not balance (Σ debits != Σ credits, off by "
                                 + std::to_string(sum) + " cents)");
    if (isDateInClosedPeriod(effectiveDate))
        throw std::runtime_error("AuditJournal: journal entry date is in a closed period — post an adjusting entry in an open period");

    const uint32_t eid = static_cast<uint32_t>(entries_.size());
    const std::vector<char> buf = encodeJournalEntry(eid, 0xFFFFFFFFu, effectiveDate, postings);
    const uint64_t seq = log_.append(static_cast<uint16_t>(EventType::JournalEntryPosted),
                                     1, timestampMs, buf.data(), static_cast<uint32_t>(buf.size()));
    ajMaybeCrash("afterEventBeforeProject");
    writeCursor(seq);
    rebuildLedgerIndex();
    return eid;
}

uint32_t AuditJournal::reverseJournalEntry(uint32_t entryId, IsoDate effectiveDate, int64_t timestampMs)
{
    auto it = entries_.find(entryId);
    if (it == entries_.end())
        throw std::runtime_error("AuditJournal: no such journal entry " + std::to_string(entryId));
    if (isDateInClosedPeriod(effectiveDate))
        throw std::runtime_error("AuditJournal: reversal date is in a closed period");

    // A reversal is itself a balanced entry (the original balances, negated stays balanced).
    std::vector<PostingInput> neg;
    for (const auto& p : it->second.postings) neg.push_back({ p.accountId, -p.amountCents });

    const uint32_t eid = static_cast<uint32_t>(entries_.size());
    const std::vector<char> buf = encodeJournalEntry(eid, entryId, effectiveDate, neg);
    const uint64_t seq = log_.append(static_cast<uint16_t>(EventType::JournalEntryPosted),
                                     1, timestampMs, buf.data(), static_cast<uint32_t>(buf.size()));
    ajMaybeCrash("afterEventBeforeProject");
    writeCursor(seq);
    rebuildLedgerIndex();
    return eid;
}

int64_t AuditJournal::balanceFor(uint32_t accountId) const
{
    auto it = balances_.find(accountId);
    return it != balances_.end() ? it->second : 0;
}

int64_t AuditJournal::balanceAt(uint32_t accountId, uint64_t uptoSeq)
{
    int64_t bal = 0;
    log_.forEach([&](const EventRecord& r) {
        if (r.seq > uptoSeq) return false;
        if (static_cast<EventType>(r.type) == EventType::JournalEntryPosted && r.payload.size() >= 20) {
            uint16_t n = 0; std::memcpy(&n, r.payload.data() + 18, 2);
            const char* p = r.payload.data() + 20;
            for (uint16_t i = 0; i < n && p + 12 <= r.payload.data() + r.payload.size(); ++i) {
                uint32_t acct = 0; int64_t amt = 0;
                std::memcpy(&acct, p + 0, 4); std::memcpy(&amt, p + 4, 8);
                if (acct == accountId) bal += amt;
                p += 12;
            }
        }
        return true;
    });
    return bal;
}

int64_t AuditJournal::trialBalanceTotal() const
{
    int64_t total = 0;
    for (const auto& [aid, b] : balances_) { (void)aid; total += b; }
    return total;
}

// ── Financial statements ─────────────────────────────────────────────────────
namespace {
std::map<uint32_t, int64_t> replayBalances(EventLog& log, uint64_t uptoSeq)
{
    std::map<uint32_t, int64_t> bal;
    log.forEach([&](const EventRecord& r) {
        if (r.seq > uptoSeq) return false;
        if (static_cast<EventType>(r.type) == EventType::JournalEntryPosted && r.payload.size() >= 20) {
            uint16_t n = 0; std::memcpy(&n, r.payload.data() + 18, 2);
            const char* p = r.payload.data() + 20;
            for (uint16_t i = 0; i < n && p + 12 <= r.payload.data() + r.payload.size(); ++i) {
                uint32_t acct = 0; int64_t amt = 0;
                std::memcpy(&acct, p + 0, 4); std::memcpy(&amt, p + 4, 8);
                bal[acct] += amt; p += 12;
            }
        }
        return true;
    });
    return bal;
}
} // namespace

AuditJournal::IncomeStatement AuditJournal::incomeStatementAt(uint64_t uptoSeq)
{
    const auto bal = replayBalances(log_, uptoSeq);
    IncomeStatement s;
    for (const auto& [aid, info] : accounts_) {
        auto it = bal.find(aid);
        if (it == bal.end()) continue;
        if (info.type == Income)       s.income  += -it->second;   // revenue (credit) magnitude
        else if (info.type == Expense) s.expense +=  it->second;   // expense (debit) magnitude
    }
    s.netIncome = s.income - s.expense;
    return s;
}

AuditJournal::BalanceSheet AuditJournal::balanceSheetAt(uint64_t uptoSeq)
{
    const auto bal = replayBalances(log_, uptoSeq);
    BalanceSheet b;
    int64_t income = 0, expense = 0;
    for (const auto& [aid, info] : accounts_) {
        auto it = bal.find(aid);
        if (it == bal.end()) continue;
        const int64_t v = it->second;
        switch (info.type) {
        case Asset:     b.assets      += v;  break;
        case Liability: b.liabilities += -v; break;
        case Equity:    b.equity      += -v; break;
        case Income:    income  += -v;       break;
        case Expense:   expense +=  v;       break;
        default: break;
        }
    }
    b.netIncome = income - expense;
    b.equity   += b.netIncome;     // current-period earnings flow into equity for the statement
    b.balances  = (b.assets == b.liabilities + b.equity);   // invariant: trial balance is 0
    return b;
}

AuditJournal::TaxSummary AuditJournal::taxSummaryAt(uint64_t uptoSeq)
{
    // Derived entirely from two ledger balances at seq — no cached tax, no reporting-time
    // recomputation. Tax Payable is a credit-normal liability (output tax → −balance);
    // Recoverable Tax is a debit-normal asset (input tax → +balance). Reconstructible at any
    // seq (books-as-closed: pass the closed seq).
    TaxSummary s;
    if (roleTaxPayable_     != 0xFFFFFFFFu) s.collected   = -balanceAt(roleTaxPayable_, uptoSeq);
    if (roleRecoverableTax_ != 0xFFFFFFFFu) s.recoverable =  balanceAt(roleRecoverableTax_, uptoSeq);
    s.netPayable = s.collected - s.recoverable;
    return s;
}

uint32_t AuditJournal::recordClosingEntry(uint32_t retainedEarningsAccountId, IsoDate effectiveDate, int64_t timestampMs)
{
    // Zero every income/expense account into retained earnings — a real balanced entry.
    std::vector<PostingInput> postings;
    int64_t offset = 0;
    for (const auto& [aid, info] : accounts_) {
        if (info.type == Income || info.type == Expense) {
            const int64_t b = balanceFor(aid);
            if (b != 0) { postings.push_back({ aid, -b }); offset += -b; }
        }
    }
    if (postings.empty())
        throw std::runtime_error("AuditJournal: nothing to close (no income/expense activity)");
    postings.push_back({ retainedEarningsAccountId, -offset });   // balances the entry → Σ == 0
    return recordJournalEntry(effectiveDate, postings, timestampMs);   // enforces balance + period
}

// ── Posting authority: deterministic business-event → ledger mapping ─────────
void AuditJournal::setPostingAccounts(uint32_t receivable, uint32_t revenue, uint32_t cash,
                                      uint32_t expense, uint32_t payable,
                                      uint32_t taxPayable, uint32_t recoverableTax)
{
    roleReceivable_ = receivable; roleRevenue_ = revenue; roleCash_ = cash;
    roleExpense_ = expense; rolePayable_ = payable;
    roleTaxPayable_ = taxPayable; roleRecoverableTax_ = recoverableTax;
}

// Build the full role set for the posting policy (all bound role account ids).
static posting::Roles rolesFrom(uint32_t ar, uint32_t rev, uint32_t cash, uint32_t exp,
                                uint32_t pay, uint32_t taxPay, uint32_t recTax)
{
    posting::Roles r; r.receivable = ar; r.revenue = rev; r.cash = cash; r.expense = exp;
    r.payable = pay; r.taxPayable = taxPay; r.recoverableTax = recTax; return r;
}

// Map policy lines → PostingInput. The policy VERSION authoring new facts is
// posting::kCurrentPostingPolicyVersion; historical postings are persisted events and
// replay unchanged regardless of the current policy (see PostingPolicy.h).
static std::vector<AuditJournal::PostingInput> toPostings(const std::vector<posting::Line>& lines)
{
    std::vector<AuditJournal::PostingInput> out;
    out.reserve(lines.size());
    for (const posting::Line& l : lines) out.push_back({ l.accountId, l.amountCents });
    return out;
}

uint32_t AuditJournal::postInvoiceRevenue(int64_t totalCents, IsoDate effectiveDate, int64_t timestampMs)
{
    // FIXED, versioned policy: an invoice recognises revenue and a receivable. Dr AR / Cr Revenue.
    if (roleReceivable_ == 0xFFFFFFFFu || roleRevenue_ == 0xFFFFFFFFu)
        throw std::runtime_error("AuditJournal: posting accounts not configured (setPostingAccounts)");
    const posting::Roles roles{ roleReceivable_, roleRevenue_, roleCash_ };
    return recordJournalEntry(effectiveDate,
        toPostings(posting::invoiceRevenue(posting::kCurrentPostingPolicyVersion, totalCents, roles)),
        timestampMs);   // balanced + period-checked + crash-safe
}

uint32_t AuditJournal::postPaymentReceipt(int64_t amountCents, IsoDate effectiveDate, int64_t timestampMs)
{
    // FIXED, versioned policy: receiving a payment clears the receivable into cash. Dr Cash / Cr AR.
    if (roleCash_ == 0xFFFFFFFFu || roleReceivable_ == 0xFFFFFFFFu)
        throw std::runtime_error("AuditJournal: posting accounts not configured (setPostingAccounts)");
    const posting::Roles roles{ roleReceivable_, roleRevenue_, roleCash_ };
    return recordJournalEntry(effectiveDate,
        toPostings(posting::paymentReceipt(posting::kCurrentPostingPolicyVersion, amountCents, roles)),
        timestampMs);
}

int AuditJournal::accountIdByName(const std::string& name) const
{
    for (const auto& [id, info] : accounts_)
        if (info.name == name) return static_cast<int>(id);
    return -1;
}

AuditJournal::RoleAccounts AuditJournal::ensureChartOfAccounts(int64_t timestampMs)
{
    // Ensure each default role account exists as an authoritative AccountOpened event — so it
    // rebuilds + replays like anything else. Idempotent PER ACCOUNT (create only the missing
    // ones), so a chart that predates the expense role accounts gains "Expenses" + "Accounts
    // Payable" on first open with this build. Names are the stable role keys.
    auto ensure = [&](AccountType type, const char* name) {
        if (accountIdByName(name) < 0)
            recordAccount(static_cast<uint8_t>(type), name, timestampMs);
    };
    ensure(Asset,     "Accounts Receivable");
    ensure(Income,    "Revenue");
    ensure(Asset,     "Cash");
    ensure(Expense,   "Expenses");            // single expense account (all expenses debit it)
    ensure(Liability, "Accounts Payable");    // credit purchases
    ensure(Liability, "Tax Payable");         // output tax collected on sales (posting-policy v2)
    ensure(Asset,     "Recoverable Tax");     // input tax paid on purchases (posting-policy v2)

    RoleAccounts roles;
    auto bind = [&](const char* name, uint32_t& slot) {
        const int id = accountIdByName(name);
        if (id >= 0) slot = static_cast<uint32_t>(id);
    };
    bind("Accounts Receivable", roles.receivable);
    bind("Revenue",             roles.revenue);
    bind("Cash",                roles.cash);
    bind("Expenses",            roles.expense);
    bind("Accounts Payable",    roles.payable);
    bind("Tax Payable",         roles.taxPayable);
    bind("Recoverable Tax",     roles.recoverableTax);
    setPostingAccounts(roles.receivable, roles.revenue, roles.cash, roles.expense, roles.payable,
                       roles.taxPayable, roles.recoverableTax);
    return roles;
}

uint64_t AuditJournal::recordInvoiceWithRevenue(Invoice& invoice, std::vector<InvoiceLine>& lines,
                                                bool correction, int64_t netDeltaCents, int64_t taxDeltaCents,
                                                IsoDate effectiveDate, int64_t timestampMs)
{
    if (!invoices_ || !lines_)
        throw std::logic_error("AuditJournal: invoice repositories not configured");

    // ── Validate + assign stable ids BEFORE authoring anything (fail atomically) ──
    if (correction) {
        // Historical finality: a correction to a frozen period is forbidden (as
        // recordInvoiceCorrected). Reject before the group is built — nothing authored.
        if (isInvoiceInClosedPeriod(invoice.getId()) || isDateInClosedPeriod(invoice.getIssueDate()))
            throw std::runtime_error(
                "AuditJournal: invoice " + std::to_string(invoice.getId())
                + " is in a closed accounting period — post an adjustment, do not correct it");
        uint32_t nextLineId = static_cast<uint32_t>(lines_->count());
        for (InvoiceLine& l : lines) {
            l.setInvoiceId(invoice.getId());
            if (l.getId() == UINT32_MAX) l.setId(nextLineId++);   // existing keep id; new get one
        }
    } else {
        invoice.setId(static_cast<uint32_t>(invoices_->count()));
        uint32_t nextLineId = static_cast<uint32_t>(lines_->count());
        for (InvoiceLine& l : lines) { l.setInvoiceId(invoice.getId()); l.setId(nextLineId++); }
    }

    const EventType invType = correction ? EventType::InvoiceCorrected : EventType::InvoiceCreated;
    const std::vector<char> invPayload = encodeInvoiceEvent(invoice, lines);

    // ── Build the frame group deterministically ──
    // The ledger posting is part of the SAME fact iff there is a non-zero recognised-revenue
    // delta, the posting roles are bound, and the effective date is open. This decision is
    // made here (before authoring), so the committed group is fixed — a crash can only leave
    // the whole group present or the whole group absent.
    std::vector<EventLog::FrameSpec> group;
    group.push_back({ static_cast<uint16_t>(invType), 1, timestampMs, invPayload });

    const bool willPost = (netDeltaCents != 0 || taxDeltaCents != 0)
                       && roleReceivable_ != 0xFFFFFFFFu && roleRevenue_ != 0xFFFFFFFFu
                       && !isDateInClosedPeriod(effectiveDate);
    if (willPost) {
        const uint32_t postEntryId = static_cast<uint32_t>(entries_.size());
        const posting::Roles roles = rolesFrom(roleReceivable_, roleRevenue_, roleCash_,
            roleExpense_, rolePayable_, roleTaxPayable_, roleRecoverableTax_);
        // v2: Dr AR (net+tax) / Cr Revenue (net) / Cr Tax Payable (tax). Tax line omitted at tax 0.
        const auto plines = posting::invoiceRevenueTaxed(
            posting::kCurrentPostingPolicyVersion, netDeltaCents, taxDeltaCents, roles);
        std::vector<PostingInput> postings;
        postings.reserve(plines.size());
        for (const auto& l : plines) postings.push_back({ l.accountId, l.amountCents });
        std::vector<char> postPayload =
            encodeJournalEntry(postEntryId, 0xFFFFFFFFu, effectiveDate, postings);
        group.push_back({ static_cast<uint16_t>(EventType::JournalEntryPosted),
                          1, timestampMs, std::move(postPayload) });
    }

    // ── ONE indivisible commit (EventLog advances its single commit point once) ──
    const std::vector<uint64_t> seqs = log_.appendAtomic(group);

    // Project the committed group. Idempotent + reconcile-healed if a crash lands in this
    // window (the group is already committed history, so recovery replays it whole).
    EventRecord ir; ir.type = static_cast<uint16_t>(invType); ir.seq = seqs.front(); ir.payload = invPayload;
    apply(*customers_, suppliers_, invoices_, lines_, expenses_, ir);
    if (willPost) rebuildLedgerIndex();   // the ledger index picks up the new posting
    ajMaybeCrash("afterProjectBeforeCursor");

    writeCursor(seqs.back());
    return seqs.front();
}

// ── Expenses (event-authored operational entity; mirrors the invoice lifecycle) ───────
// (::Expense — the class; an unqualified `Expense` here would name the AccountType enumerator.)
uint64_t AuditJournal::recordExpenseWithPosting(::Expense& expense, bool correction,
                                                int64_t netDeltaCents, int64_t taxDeltaCents,
                                                IsoDate effectiveDate, int64_t timestampMs)
{
    if (!expenses_) throw std::logic_error("AuditJournal: expense repository not configured");

    // Assign a stable id BEFORE authoring (create → next slot; correction keeps its id).
    if (correction) {
        if (isDateInClosedPeriod(effectiveDate))
            throw std::runtime_error("AuditJournal: expense " + std::to_string(expense.getId())
                + " is in a closed accounting period — post an adjustment, do not correct it");
    } else {
        expense.setId(static_cast<uint32_t>(expenses_->count()));
    }

    const EventType expType = correction ? EventType::ExpenseCorrected : EventType::ExpenseCreated;
    char recBuf[EXPENSE_RECORD_SIZE];
    expense.serialize(recBuf);
    const std::vector<char> expPayload(recBuf, recBuf + EXPENSE_RECORD_SIZE);

    // The ledger posting is part of the SAME atomic fact iff there is a non-zero amount to post,
    // the roles are bound, and the effective date is open (decided before authoring).
    std::vector<EventLog::FrameSpec> group;
    group.push_back({ static_cast<uint16_t>(expType), 1, timestampMs, expPayload });

    const bool onCreditNew = expense.getPaymentMethod() == EXPENSE_PAY_CREDIT;
    const uint32_t creditAcctNew = onCreditNew ? rolePayable_ : roleCash_;

    // On a correction, detect whether the posting accounts change (the funding method flipped
    // Cash<->Credit, so the credit side moves Cash<->Accounts Payable). The delta-only posting is
    // ONLY sound when the accounts are unchanged; if they change it would leave the ORIGINAL
    // credit-side account unreversed. In that case reverse the previous posting IN FULL and post
    // the new one IN FULL (two balanced entries) instead of a net delta.
    bool     onCreditOld = false;
    bool     rolesChanged = false;
    int64_t  oldNetFull = 0, oldTaxFull = 0;
    if (correction) {
        const ::Expense prev = expenses_->load(expense.getId());   // pre-correction (projection not yet updated)
        onCreditOld  = prev.getPaymentMethod() == EXPENSE_PAY_CREDIT;
        oldNetFull   = prev.getAmount().cents();
        oldTaxFull   = taxOnNet(oldNetFull, prev.getTaxRatePermille());
        rolesChanged = (onCreditOld != onCreditNew);
    }
    const uint32_t creditAcctOld = onCreditOld ? rolePayable_ : roleCash_;
    const int64_t  newNetFull = expense.getAmount().cents();
    const int64_t  newTaxFull = taxOnNet(newNetFull, expense.getTaxRatePermille());

    const bool rolesBound = roleExpense_ != 0xFFFFFFFFu && creditAcctNew != 0xFFFFFFFFu
                         && (!rolesChanged || creditAcctOld != 0xFFFFFFFFu);
    const bool willPost = rolesBound && !isDateInClosedPeriod(effectiveDate)
                       && (rolesChanged || netDeltaCents != 0 || taxDeltaCents != 0);
    if (willPost) {
        const uint16_t ver = posting::kCurrentPostingPolicyVersion;
        const posting::Roles roles = rolesFrom(roleReceivable_, roleRevenue_, roleCash_,
            roleExpense_, rolePayable_, roleTaxPayable_, roleRecoverableTax_);
        uint32_t nextEntryId = static_cast<uint32_t>(entries_.size());
        auto pushEntry = [&](const std::vector<posting::Line>& plines) {
            std::vector<PostingInput> postings;
            for (const auto& l : plines) postings.push_back({ l.accountId, l.amountCents });
            std::vector<char> postPayload = encodeJournalEntry(nextEntryId++, 0xFFFFFFFFu, effectiveDate, postings);
            group.push_back({ static_cast<uint16_t>(EventType::JournalEntryPosted),
                              1, timestampMs, std::move(postPayload) });
        };
        if (rolesChanged) {
            // Reverse the original posting completely, then post the new one completely.
            pushEntry(posting::expenseTaxed(ver, -oldNetFull, -oldTaxFull, onCreditOld, roles));
            pushEntry(posting::expenseTaxed(ver,  newNetFull,  newTaxFull, onCreditNew, roles));
        } else {
            // Accounts unchanged → the fast delta path is exact.
            // v2: Dr Expense (net) / Dr Recoverable Tax (tax) / Cr Cash|AP (net+tax); line omitted at 0.
            pushEntry(posting::expenseTaxed(ver, netDeltaCents, taxDeltaCents, onCreditNew, roles));
        }
    }

    const std::vector<uint64_t> seqs = log_.appendAtomic(group);
    { EventRecord er; er.type = static_cast<uint16_t>(expType); er.seq = seqs.front(); er.payload = expPayload;
      apply(*customers_, suppliers_, invoices_, lines_, expenses_, er); }
    if (willPost) rebuildLedgerIndex();
    ajMaybeCrash("afterProjectBeforeCursor");
    writeCursor(seqs.back());
    return seqs.front();
}

uint64_t AuditJournal::recordExpenseVoided(uint32_t targetExpenseId, int64_t timestampMs)
{
    if (!expenses_) throw std::logic_error("AuditJournal: expense repository not configured");
    ::Expense target = expenses_->load(targetExpenseId);
    const IsoDate d = target.getDate();
    // Voiding changes a transaction's standing in place → forbidden once frozen (reverse instead).
    if (isDateInClosedPeriod(d))
        throw std::runtime_error("AuditJournal: expense " + std::to_string(targetExpenseId)
            + " is in a closed period — post a reversal, do not void it");

    char link[8];
    std::memcpy(link + 0, &targetExpenseId, 4);
    const uint32_t none = 0xFFFFFFFFu; std::memcpy(link + 4, &none, 4);

    // Mark VOID + post a sign-flipped compensating entry (undo the original Dr Expenses / Cr
    // Cash|AP) as ONE atomic fact.
    std::vector<EventLog::FrameSpec> group;
    group.push_back({ static_cast<uint16_t>(EventType::ExpenseVoided), 1, timestampMs,
                      std::vector<char>(link, link + 8) });

    const bool onCredit = target.getPaymentMethod() == EXPENSE_PAY_CREDIT;
    const uint32_t creditAcct = onCredit ? rolePayable_ : roleCash_;
    const int64_t amt = target.getAmount().cents();
    const int64_t tax = taxOnNet(amt, target.getTaxRatePermille());   // same formula as authoring → exact cancel
    const bool willPost = amt != 0 && roleExpense_ != 0xFFFFFFFFu && creditAcct != 0xFFFFFFFFu;
    if (willPost) {
        const uint32_t postEntryId = static_cast<uint32_t>(entries_.size());
        const posting::Roles roles = rolesFrom(roleReceivable_, roleRevenue_, roleCash_,
            roleExpense_, rolePayable_, roleTaxPayable_, roleRecoverableTax_);
        const auto plines = posting::expenseTaxed(   // negated net + tax = compensating entry
            posting::kCurrentPostingPolicyVersion, -amt, -tax, onCredit, roles);
        std::vector<PostingInput> postings;
        for (const auto& l : plines) postings.push_back({ l.accountId, l.amountCents });
        std::vector<char> postPayload = encodeJournalEntry(postEntryId, 0xFFFFFFFFu, d, postings);
        group.push_back({ static_cast<uint16_t>(EventType::JournalEntryPosted),
                          1, timestampMs, std::move(postPayload) });
    }

    const std::vector<uint64_t> seqs = log_.appendAtomic(group);
    { EventRecord er; er.type = static_cast<uint16_t>(EventType::ExpenseVoided); er.seq = seqs.front();
      er.payload.assign(link, link + 8); apply(*customers_, suppliers_, invoices_, lines_, expenses_, er); }
    if (willPost) rebuildLedgerIndex();
    ajMaybeCrash("afterProjectBeforeCursor");
    writeCursor(seqs.back());
    rebuildExpenseCorrectionIndex();
    return seqs.back();
}

uint64_t AuditJournal::recordExpenseReversal(uint32_t originalExpenseId, ::Expense& reversalExpense,
                                             IsoDate effectiveDate, int64_t timestampMs)
{
    if (!expenses_) throw std::logic_error("AuditJournal: expense repository not configured");

    // A reversal is a NEW negating expense (negative amount) — append-only, allowed even when
    // the original is in a closed period (the sanctioned post-close path). Negating expense +
    // original→reversal link + compensating posting are ONE atomic fact.
    reversalExpense.setId(static_cast<uint32_t>(expenses_->count()));
    const uint32_t reversalId = reversalExpense.getId();

    char recBuf[EXPENSE_RECORD_SIZE];
    reversalExpense.serialize(recBuf);
    const std::vector<char> expPayload(recBuf, recBuf + EXPENSE_RECORD_SIZE);

    char link[8];
    std::memcpy(link + 0, &originalExpenseId, 4);
    std::memcpy(link + 4, &reversalId, 4);

    std::vector<EventLog::FrameSpec> group;
    group.push_back({ static_cast<uint16_t>(EventType::ExpenseCreated),  1, timestampMs, expPayload });
    group.push_back({ static_cast<uint16_t>(EventType::ExpenseReversed), 1, timestampMs,
                      std::vector<char>(link, link + 8) });

    const bool onCredit = reversalExpense.getPaymentMethod() == EXPENSE_PAY_CREDIT;
    const uint32_t creditAcct = onCredit ? rolePayable_ : roleCash_;
    const int64_t amt = reversalExpense.getAmount().cents();   // negative → sign-flipped compensating entry
    const int64_t tax = taxOnNet(amt, reversalExpense.getTaxRatePermille());   // negative too
    const bool willPost = amt != 0 && roleExpense_ != 0xFFFFFFFFu && creditAcct != 0xFFFFFFFFu
                       && !isDateInClosedPeriod(effectiveDate);
    if (willPost) {
        const uint32_t postEntryId = static_cast<uint32_t>(entries_.size());
        const posting::Roles roles = rolesFrom(roleReceivable_, roleRevenue_, roleCash_,
            roleExpense_, rolePayable_, roleTaxPayable_, roleRecoverableTax_);
        const auto plines = posting::expenseTaxed(
            posting::kCurrentPostingPolicyVersion, amt, tax, onCredit, roles);
        std::vector<PostingInput> postings;
        for (const auto& l : plines) postings.push_back({ l.accountId, l.amountCents });
        std::vector<char> postPayload = encodeJournalEntry(postEntryId, 0xFFFFFFFFu, effectiveDate, postings);
        group.push_back({ static_cast<uint16_t>(EventType::JournalEntryPosted),
                          1, timestampMs, std::move(postPayload) });
    }

    const std::vector<uint64_t> seqs = log_.appendAtomic(group);
    { EventRecord er; er.type = static_cast<uint16_t>(EventType::ExpenseCreated); er.seq = seqs.front();
      er.payload = expPayload; apply(*customers_, suppliers_, invoices_, lines_, expenses_, er); }
    if (willPost) rebuildLedgerIndex();
    ajMaybeCrash("afterProjectBeforeCursor");
    writeCursor(seqs.back());
    rebuildExpenseCorrectionIndex();
    return seqs.back();
}

void AuditJournal::rebuildExpenseCorrectionIndex()
{
    expenseCorrections_.clear();
    log_.forEach([&](const EventRecord& r) {
        const EventType t = static_cast<EventType>(r.type);
        if ((t == EventType::ExpenseVoided || t == EventType::ExpenseReversed)
            && r.payload.size() >= 8) {
            uint32_t tid = 0, rid = 0;
            std::memcpy(&tid, r.payload.data(), 4);
            std::memcpy(&rid, r.payload.data() + 4, 4);
            if (t == EventType::ExpenseVoided)  expenseCorrections_[tid].voided     = true;
            else                                expenseCorrections_[tid].reversedBy = rid;
        }
        return true;
    });
}

bool AuditJournal::isExpenseVoided(uint32_t expenseId) const
{
    auto it = expenseCorrections_.find(expenseId);
    return it != expenseCorrections_.end() && it->second.voided;
}

uint32_t AuditJournal::expenseReversedBy(uint32_t expenseId) const
{
    auto it = expenseCorrections_.find(expenseId);
    return it != expenseCorrections_.end() ? it->second.reversedBy : 0xFFFFFFFFu;
}

std::size_t AuditJournal::expenseCorrectionCount() const
{
    std::size_t n = 0;
    for (const auto& [id, info] : expenseCorrections_) {
        (void)id;
        if (info.voided) ++n;
        if (info.reversedBy != 0xFFFFFFFFu) ++n;
    }
    return n;
}

// ── Deterministic snapshotting (replay acceleration) ─────────────────────────
namespace {
constexpr char kSnapMagic[8] = { 'A','C','C','T','S','N','A','P' };

uint32_t hashBalances(const std::map<uint32_t, int64_t>& bal)
{
    std::vector<char> buf(bal.size() * 12);
    char* p = buf.data();
    for (const auto& [acct, b] : bal) { std::memcpy(p, &acct, 4); std::memcpy(p + 4, &b, 8); p += 12; }
    return BinaryRecordFile::crc32(buf.data(), buf.size());
}

// File-integrity hash covering the HEADER (seq + count) AND the entries. hashBalances alone
// left seq/count unprotected: a corrupted `seq` passed the integrity check, and
// balanceUsingSnapshot (which trusts file validity, not a genesis match) then accelerated
// from the wrong seq and returned a wrong balance. Found by the snapshot fuzzer; the hash now
// binds the seq the balances belong to. (On-disk snapshot format v2.)
uint32_t hashSnapshot(uint64_t seq, uint32_t count, const std::map<uint32_t, int64_t>& bal)
{
    std::vector<char> buf(12 + bal.size() * 12);
    std::memcpy(buf.data() + 0, &seq, 8);
    std::memcpy(buf.data() + 8, &count, 4);
    char* p = buf.data() + 12;
    for (const auto& [acct, b] : bal) { std::memcpy(p, &acct, 4); std::memcpy(p + 4, &b, 8); p += 12; }
    return BinaryRecordFile::crc32(buf.data(), buf.size());
}

struct SnapData { uint64_t seq = 0; std::map<uint32_t, int64_t> balances; bool valid = false; };

SnapData loadSnap(const std::string& path)
{
    SnapData s;
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return s;
    char hdr[32];
    if (std::fread(hdr, 1, 32, f) != 32 || std::memcmp(hdr, kSnapMagic, 8) != 0) { std::fclose(f); return s; }
    uint16_t ver = 0; std::memcpy(&ver, hdr + 8, 2);
    if (ver != 2) { std::fclose(f); return s; }   // v2: integrity hash now covers the header
    uint32_t storedHash = 0, count = 0;
    std::memcpy(&s.seq, hdr + 16, 8);
    std::memcpy(&storedHash, hdr + 24, 4);
    std::memcpy(&count, hdr + 28, 4);
    bool okread = true;
    for (uint32_t i = 0; i < count; ++i) {
        char e[12];
        if (std::fread(e, 1, 12, f) != 12) { okread = false; break; }
        uint32_t a = 0; int64_t b = 0; std::memcpy(&a, e, 4); std::memcpy(&b, e + 4, 8);
        s.balances[a] = b;
    }
    std::fclose(f);
    // Integrity check over seq + count + entries — a corrupted seq/count now invalidates the
    // snapshot (→ genesis fallback), so the accelerator can never trust a wrong seq.
    if (okread) s.valid = (hashSnapshot(s.seq, count, s.balances) == storedHash);
    return s;
}
} // namespace

void AuditJournal::writeLedgerSnapshot(uint64_t atSeq)
{
    if (atSeq > log_.lastSeq()) atSeq = log_.lastSeq();
    const auto bal = replayBalances(log_, atSeq);
    const uint32_t count = static_cast<uint32_t>(bal.size());
    const uint32_t h = hashSnapshot(atSeq, count, bal);   // integrity hash binds seq + count

    std::vector<char> buf(32 + 12ull * count, 0);
    std::memcpy(buf.data(), kSnapMagic, 8);
    const uint16_t ver = 2; std::memcpy(buf.data() + 8, &ver, 2);   // on-disk snapshot format v2
    std::memcpy(buf.data() + 16, &atSeq, 8);
    std::memcpy(buf.data() + 24, &h, 4);
    std::memcpy(buf.data() + 28, &count, 4);
    char* p = buf.data() + 32;
    for (const auto& [acct, b] : bal) { std::memcpy(p, &acct, 4); std::memcpy(p + 4, &b, 8); p += 12; }

    // Crash-safe: write a temp, fsync it, atomically rename over the snapshot. A crash
    // leaves the OLD snapshot (or none) — never a partial one. The snapshot is not
    // authority, so even total loss only forces a genesis replay.
    const std::string tmp = snapshotPath_ + ".tmp";
    FILE* f = std::fopen(tmp.c_str(), "wb");
    if (!f) throw std::runtime_error("AuditJournal: cannot create snapshot temp " + tmp);
    const bool ok = std::fwrite(buf.data(), 1, buf.size(), f) == buf.size();
    if (ok) std::fflush(f);
    if (ok) AJ_FSYNC(fileno(f));
    std::fclose(f);
    if (!ok) { std::error_code ec; std::filesystem::remove(tmp, ec);
               throw std::runtime_error("AuditJournal: snapshot write failed"); }

    ajMaybeCrash("afterSnapshotTmp");   // temp durable, not yet installed → old/no snapshot stands
    // Fault injection: the snapshot install fails. The snapshot is disposable, so on reopen
    // ledgerSnapshotSeq()==0 and balanceUsingSnapshot falls back to the genesis replay.
    if (acctFaultAt("snapshotWrite")) {
        std::error_code ec2; std::filesystem::remove(tmp, ec2);
        throw std::runtime_error("AuditJournal: injected fault at snapshotWrite (install)");
    }
    std::error_code ec;
    std::filesystem::rename(tmp, snapshotPath_, ec);
}

uint64_t AuditJournal::ledgerSnapshotSeq()
{
    const SnapData s = loadSnap(snapshotPath_);
    return s.valid ? s.seq : 0;
}

int64_t AuditJournal::balanceUsingSnapshot(uint32_t accountId, uint64_t uptoSeq)
{
    const SnapData s = loadSnap(snapshotPath_);
    if (!s.valid || s.seq > uptoSeq) return balanceAt(accountId, uptoSeq);   // genesis fallback

    int64_t bal = 0;
    auto it = s.balances.find(accountId);
    if (it != s.balances.end()) bal = it->second;
    // Replay only the tail (s.seq, uptoSeq].
    log_.forEach([&](const EventRecord& r) {
        if (r.seq <= s.seq) return true;
        if (r.seq > uptoSeq) return false;
        if (static_cast<EventType>(r.type) == EventType::JournalEntryPosted && r.payload.size() >= 20) {
            uint16_t n = 0; std::memcpy(&n, r.payload.data() + 18, 2);
            const char* p = r.payload.data() + 20;
            for (uint16_t i = 0; i < n && p + 12 <= r.payload.data() + r.payload.size(); ++i) {
                uint32_t acct = 0; int64_t amt = 0;
                std::memcpy(&acct, p + 0, 4); std::memcpy(&amt, p + 4, 8);
                if (acct == accountId) bal += amt;
                p += 12;
            }
        }
        return true;
    });
    return bal;
}

bool AuditJournal::verifyLedgerSnapshot()
{
    const SnapData s = loadSnap(snapshotPath_);
    if (!s.valid) return false;                       // missing or file-corrupt
    const auto genesis = replayBalances(log_, s.seq);
    return hashBalances(genesis) == hashBalances(s.balances);   // matches authority at S
}

// ── Verification & historical reconstruction ─────────────────────────────────
uint32_t AuditJournal::liveFingerprint()
{
    return customers_->contentHash();
}

uint32_t AuditJournal::reconstructInto(CustomerRepository& scratch, uint64_t uptoSeq)
{
    // Build "the books as of uptoSeq" in a disposable scratch projection. This NEVER
    // touches the live projection, the cursor, or the log — reconstruction is
    // read-only w.r.t. authority, so an interrupted reconstruction can only orphan
    // scratch files, never corrupt history or current state.
    scratch.clear();
    log_.forEach([&](const EventRecord& r) {
        if (r.seq > uptoSeq) return false;              // events are seq-ordered → stop
        apply(scratch, nullptr, nullptr, nullptr, nullptr, r);   // customer-only reconstruction
        ajMaybeCrash("duringReconstruct");              // crash window: scratch partial, authority untouched
        return true;
    });
    return scratch.contentHash();
}

AuditJournal::VerifyResult AuditJournal::verify(CustomerRepository& scratch)
{
    VerifyResult vr;
    vr.seq         = log_.lastSeq();
    vr.historyHash = reconstructInto(scratch, UINT64_MAX);   // full rebuild from history
    vr.liveHash    = customers_->contentHash();
    vr.ok          = (vr.liveHash == vr.historyHash);
    return vr;
}

// ── Historical compatibility & evolution governance ──────────────────────────
void AuditJournal::rebuildGovernanceIndex()
{
    governance_.clear();
    log_.forEach([&](const EventRecord& r) {
        if (static_cast<EventType>(r.type) == EventType::EngineVersionStamp
            && r.payload.size() >= 14) {
            GovernanceVersions v;
            std::memcpy(&v.schema,         r.payload.data() + 0,  2);
            std::memcpy(&v.replay,         r.payload.data() + 2,  2);
            std::memcpy(&v.postingPolicy,  r.payload.data() + 4,  2);
            std::memcpy(&v.statement,      r.payload.data() + 6,  2);
            std::memcpy(&v.snapshot,       r.payload.data() + 8,  2);
            std::memcpy(&v.eventLogFormat, r.payload.data() + 10, 2);
            std::memcpy(&v.engineBuild,    r.payload.data() + 12, 2);
            governance_.push_back({ v, r.seq });   // seq-ordered → latest is back()
        }
        return true;
    });
}

uint64_t AuditJournal::recordEngineVersionStamp(const GovernanceVersions& v, int64_t timestampMs)
{
    char buf[14];
    std::memcpy(buf + 0,  &v.schema,         2);
    std::memcpy(buf + 2,  &v.replay,         2);
    std::memcpy(buf + 4,  &v.postingPolicy,  2);
    std::memcpy(buf + 6,  &v.statement,      2);
    std::memcpy(buf + 8,  &v.snapshot,       2);
    std::memcpy(buf + 10, &v.eventLogFormat, 2);
    std::memcpy(buf + 12, &v.engineBuild,    2);

    const uint64_t seq = log_.append(static_cast<uint16_t>(EventType::EngineVersionStamp),
                                     1, timestampMs, buf, 14);
    ajMaybeCrash("afterEventBeforeProject");   // stamp committed, index not yet rebuilt
    writeCursor(seq);                           // governance events don't touch entity projections
    rebuildGovernanceIndex();
    return seq;
}

bool AuditJournal::ensureGovernanceStamp(int64_t timestampMs)
{
    // Adoption / cutover: stamp the current contract into history exactly once, so that a
    // new dataset AND a pre-governance one both carry an explicit authoring-version record.
    if (!governance_.empty()) return false;
    recordEngineVersionStamp(compat::current(), timestampMs);
    return true;
}

bool AuditJournal::adoptVersionTransition(int64_t timestampMs)
{
    // Forward-migration adoption. Compare THIS build's contract to the head stamp per GATING
    // axis (engineBuild is informational → never triggers a stamp). If every axis where code >
    // head has a registered semantic-migration path, append one stamp recording the transition;
    // if any axis is a downgrade or lacks a path, do nothing (the caller's classify → refuse).
    // Safe because a registered accounting-semantic migration is a proven no-op for historical
    // postings (immutable events) — only NEW facts use the new mapping.
    if (governance_.empty()) return false;
    const GovernanceVersions head = currentGovernance();
    const GovernanceVersions code = compat::current();
    struct Axis { const char* n; uint16_t h, c; };
    const Axis axes[] = {
        { "schema",         head.schema,         code.schema },
        { "replay",         head.replay,         code.replay },
        { "postingPolicy",  head.postingPolicy,  code.postingPolicy },
        { "statement",      head.statement,      code.statement },
        { "snapshot",       head.snapshot,       code.snapshot },
        { "eventLogFormat", head.eventLogFormat, code.eventLogFormat },
    };
    bool anyUp = false;
    for (const Axis& a : axes) {
        if (a.c > a.h) { anyUp = true; if (!semantic::hasPath(a.n, a.h, a.c)) return false; }
        else if (a.c < a.h) return false;   // downgrade on some axis → not our job (refused by classify)
    }
    if (!anyUp) return false;
    recordEngineVersionStamp(code, timestampMs);   // records the transition; later opens classify Compatible
    return true;
}

GovernanceVersions AuditJournal::currentGovernance() const
{
    return governance_.empty() ? GovernanceVersions{} : governance_.back().v;
}

AuditJournal::VerifyAllResult AuditJournal::verifyAll(CustomerRepository& cust, SupplierRepository& sup,
                                                      InvoiceRepository& inv, InvoiceLineRepository& lns,
                                                      ExpenseRepository* exp)
{
    // Rebuild EVERY event-authored projection from history into disposable scratch repos,
    // then compare each content fingerprint to live. Same mechanism as verify(), now the
    // whole model — so drift in any projection is caught. Read-only w.r.t. authority.
    // `exp` is optional: when passed, the expense projection is byte-verified too.
    cust.clear(); sup.clear(); inv.clear(); lns.clear();
    if (exp) exp->clear();
    log_.forEach([&](const EventRecord& r) {
        apply(cust, &sup, &inv, &lns, exp, r);
        return true;
    });
    VerifyAllResult vr;
    vr.seq         = log_.lastSeq();
    vr.customersOk = (!customers_ || customers_->contentHash() == cust.contentHash());
    vr.suppliersOk = (!suppliers_ || suppliers_->contentHash() == sup.contentHash());
    vr.invoicesOk  = (!invoices_  || invoices_->contentHash()  == inv.contentHash());
    vr.linesOk     = (!lines_     || lines_->contentHash()     == lns.contentHash());
    vr.expensesOk  = (!expenses_  || !exp || expenses_->contentHash() == exp->contentHash());
    vr.ok = vr.customersOk && vr.suppliersOk && vr.invoicesOk && vr.linesOk && vr.expensesOk;
    return vr;
}

AuditJournal::CompatibilityResult AuditJournal::validateCompatibility(
    CustomerRepository& sc, SupplierRepository& ss, InvoiceRepository& si, InvoiceLineRepository& sl,
    ExpenseRepository* se)
{
    CompatibilityResult cr;
    cr.seq = log_.lastSeq();

    // 1. Genesis replay-equivalence across the FULL accounting model (customers +
    //    suppliers + invoices + lines + expenses): every live projection must equal its rebuild.
    const VerifyAllResult va = verifyAll(sc, ss, si, sl, se);
    cr.genesisReplayOk = va.ok;
    cr.liveHash        = customers_->contentHash();   // representative fingerprint
    cr.historyHash     = sc.contentHash();

    // 2. Ledger invariant: the trial balance is structurally 0 (every entry balances).
    cr.trialBalanceZero = (trialBalanceTotal() == 0);

    // 3. Snapshot replay: a present ledger snapshot must equal a genesis replay at its seq.
    cr.snapshotOk = (ledgerSnapshotSeq() == 0) || verifyLedgerSnapshot();

    // 4. Historical determinism: a second full rebuild reproduces the same match on every
    //    projection (no wall-clock / ordering nondeterminism).
    const VerifyAllResult va2 = verifyAll(sc, ss, si, sl, se);
    cr.historicalDeterministic = va.ok && va2.ok
        && va.customersOk == va2.customersOk && va.suppliersOk == va2.suppliersOk
        && va.invoicesOk  == va2.invoicesOk  && va.linesOk     == va2.linesOk
        && va.expensesOk  == va2.expensesOk;

    cr.ok = cr.genesisReplayOk && cr.trialBalanceZero && cr.snapshotOk && cr.historicalDeterministic;
    cr.detail = cr.ok
        ? std::string("replay-equivalence held (full model: customers+suppliers+invoices+lines+expenses + snapshot + trial-balance) at seq ")
              + std::to_string(cr.seq)
        : std::string("replay-equivalence FAILED —")
              + (cr.genesisReplayOk         ? "" : " projection-drift")
              + (cr.trialBalanceZero        ? "" : " trial-balance!=0")
              + (cr.snapshotOk              ? "" : " snapshot-diverged")
              + (cr.historicalDeterministic ? "" : " nondeterministic-replay");
    return cr;
}
