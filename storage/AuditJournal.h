#ifndef STORAGE_AUDIT_JOURNAL_H
#define STORAGE_AUDIT_JOURNAL_H

#include <cstdint>
#include <string>

#include <map>
#include <vector>

#include "EventLog.h"
#include "CompatibilityManifest.h"
#include "../core/IsoDate.h"
#include "../core/TaxCode.h"

class CustomerRepository;
class SupplierRepository;
class InvoiceRepository;
class InvoiceLineRepository;
class ExpenseRepository;
class Invoice;
class InvoiceLine;
class Supplier;
class Expense;

// ─────────────────────────────────────────────────────────────────────────────
// AuditJournal — the authoritative history + its projection reconciler.
//
// Authority model (no hybrid ambiguity):
//   • The EventLog is TRUTH. Events are immutable; corrections append new events.
//   • The repositories are DISPOSABLE projections (read caches), rebuildable from
//     the log. They are never authoritative.
//   • A `cursor` (one durable uint64) records how far the projection has applied.
//
// record*() is the write path: assign id → append event (authoritative, committed)
// → project into the repo → advance the cursor. A crash anywhere leaves the log
// consistent and the projection at-or-behind it; reconcile() (run at startup) replays
// the gap. Projection apply is idempotent (upsert-at-id), so re-applying the boundary
// event is harmless. This is the same write-ahead + recover-on-open discipline the
// record journal already earns trust with — here the LOG is the write-ahead record.
//
// Deliberately ONE class, not an event-bus / CQRS framework: local-first, single
// process, deterministic, inspectable.
// ─────────────────────────────────────────────────────────────────────────────
class AuditJournal {
public:
    AuditJournal(std::string logPath, std::string cursorPath,
                 CustomerRepository* customers,
                 InvoiceRepository* invoices = nullptr,
                 InvoiceLineRepository* lines = nullptr,
                 SupplierRepository* suppliers = nullptr,
                 ExpenseRepository* expenses = nullptr);

    // ── Write path (authoritative). timestampMs is for display; ordering is by seq. ──
    uint64_t recordCustomerCreated(class Customer& customer, int64_t timestampMs);
    uint64_t recordCustomerUpdated(const class Customer& customer, int64_t timestampMs);
    uint64_t recordCustomerRenamed(uint32_t customerId, const std::string& newName, int64_t timestampMs);

    // Suppliers mirror customers exactly (Full Domain Cutover): create/update are
    // authoritative events, projected into the disposable supplier repo.
    uint64_t recordSupplierCreated(class Supplier& supplier, int64_t timestampMs);
    uint64_t recordSupplierUpdated(const class Supplier& supplier, int64_t timestampMs);

    // Invoice = parent + its child lines, authored as ONE atomic event. Line ids are
    // STABLE: assigned here (monotonic) and embedded — never index/order at apply.
    // recordInvoiceCreated assigns inv + line ids; recordInvoiceCorrected keeps each
    // existing line's id and assigns ids to NEW lines (id == UINT32_MAX on input).
    uint64_t recordInvoiceCreated(Invoice& invoice, std::vector<InvoiceLine>& lines, int64_t timestampMs);
    uint64_t recordInvoiceCorrected(Invoice& invoice, std::vector<InvoiceLine>& lines, int64_t timestampMs);

    // Atomic business transaction: author an invoice (create or correct) AND its ledger
    // revenue posting as ONE indivisible fact via EventLog::appendAtomic — a crash can never
    // leave the operational invoice committed without its financial interpretation (or vice
    // versa). The posting (posting-policy v2) splits recognised revenue into net + tax:
    // Dr AR (net+tax) / Cr Revenue (net) / Cr Tax Payable (tax). `netDeltaCents`/`taxDeltaCents`
    // are the signed changes in recognised net revenue and output tax (full amounts on create;
    // new − old on a correction). The posting frame is included only when a delta is non-zero,
    // roles are bound, and the date is open — decided BEFORE authoring (crash-atomic). Returns
    // the invoice event's seq.
    uint64_t recordInvoiceWithRevenue(Invoice& invoice, std::vector<InvoiceLine>& lines,
                                      bool correction, int64_t netDeltaCents, int64_t taxDeltaCents,
                                      IsoDate effectiveDate, int64_t timestampMs);

    // One-time cutover adoption: if the log is empty but the projection has records,
    // author CustomerCreated events for them so history fully backs the projection.
    // Returns the number of records adopted. No-op once history is non-empty.
    uint64_t backfillCustomers(int64_t timestampMs);

    // Per-entity cutover adoption: if the log carries NO <Entity>Created event yet but the
    // repo already holds records (written by an old direct-persistence path), author them
    // into history so the log fully backs the projection. Returns the number adopted.
    // Unlike backfillCustomers (whole-log-empty gated), these key off "no such event yet",
    // so they run correctly on an already-non-empty log (mixed-authority cutover).
    uint64_t backfillSuppliers(int64_t timestampMs);
    uint64_t backfillInvoices(int64_t timestampMs);

    // ── Accounting period closure & historical freezing ──
    // A period is a labeled [start,end] range over ACCOUNTING EFFECTIVE DATES (an
    // invoice's issue date), distinct from event seq (ordering) and the operational
    // timestamp. Closing freezes it: closedAtSeq records the history head at close, so
    // "books as closed" = reconstruct at that seq. Close/reopen are append-only events.
    struct ClosedPeriod { IsoDate start; IsoDate end; uint64_t closedAtSeq = 0; bool open = false; };

    uint64_t closePeriod(const std::string& label, IsoDate start, IsoDate end, int64_t timestampMs);
    uint64_t reopenPeriod(const std::string& label, int64_t timestampMs);

    bool        isDateInClosedPeriod(IsoDate effectiveDate) const;     // membership by effective date
    bool        isInvoiceInClosedPeriod(uint32_t invoiceId);          // by the invoice's issue date
    uint64_t    closedAtSeqFor(const std::string& label) const;       // 0 if not currently closed
    std::size_t closedPeriodCount() const;                            // currently-closed periods

    // "Books as closed at seq N": reconstruct the FULL transaction set up to uptoSeq
    // into disposable scratch repos (read-only w.r.t. authority).
    void reconstructAllInto(CustomerRepository& cust, InvoiceRepository& inv,
                            InvoiceLineRepository& lns, uint64_t uptoSeq);

    // ── Structured corrections: void vs reversal ──
    // VOID marks a transaction not-effective IN PLACE (status → VOID). It is an
    // amendment of standing → forbidden once the period is closed (reverse instead).
    // REVERSAL appends a NEW negating transaction and links original → reversal; it is
    // the sanctioned append-only path and is allowed even for a closed-period original.
    // History is never rewritten; the original event remains. Lineage lives in a
    // disposable index rebuilt from the void/reversal events.
    uint64_t recordInvoiceVoided(uint32_t targetInvoiceId, int64_t timestampMs);
    uint64_t recordInvoiceReversal(uint32_t originalInvoiceId, Invoice& reversalInvoice,
                                   std::vector<InvoiceLine>& reversalLines, int64_t timestampMs);

    bool        isVoided(uint32_t invoiceId) const;     // marked void in place
    uint32_t    reversedBy(uint32_t invoiceId) const;   // the reversal entry's id, or UINT32_MAX
    std::size_t correctionCount() const;                // void + reversal entries recorded

    // ── Expenses (event-authored operational entity — mirrors the invoice lifecycle) ──
    // recordExpenseWithPosting authors ExpenseCreated/Corrected AND its balanced ledger posting
    // (posting-policy v2: Dr Expense (net) / Dr Recoverable Tax (tax) / Cr Cash|AP (net+tax) —
    // payment method decides the credit side) as ONE atomic fact. `netDeltaCents`/`taxDeltaCents`
    // are the signed changes in the net expense and recoverable input tax; the posting is omitted
    // iff both are 0, roles are unbound, or the date is closed. Void marks status VOID in place
    // (open-only) AND posts a sign-flipped compensating entry; reversal appends a NEW negating
    // expense (allowed post-close) + the compensating entry.
    uint64_t recordExpenseWithPosting(Expense& expense, bool correction,
                                      int64_t netDeltaCents, int64_t taxDeltaCents,
                                      IsoDate effectiveDate, int64_t timestampMs);
    uint64_t recordExpenseVoided(uint32_t targetExpenseId, int64_t timestampMs);
    uint64_t recordExpenseReversal(uint32_t originalExpenseId, Expense& reversalExpense,
                                   IsoDate effectiveDate, int64_t timestampMs);

    bool        isExpenseVoided(uint32_t expenseId) const;    // marked void in place
    uint32_t    expenseReversedBy(uint32_t expenseId) const;  // the negating expense's id, or MAX
    std::size_t expenseCorrectionCount() const;               // void + reversal entries recorded

    // ── Reconciliation & allocation: settlement of obligations ──
    // An obligation is an invoice's total. A payment carries money; an allocation
    // applies part of a payment to an invoice. "Paid" is NEVER a flag — it is derived:
    //   settled(invoice)     = Σ non-reversed allocations to it
    //   outstanding(invoice) = invoice.total − settled   (negative = overpaid/credit)
    //   unallocated(payment) = payment.amount − Σ its non-reversed allocations
    // All by stable id, append-only, reconstructible at any seq.
    uint32_t recordPayment(uint32_t customerId, int64_t amountCents, IsoDate effectiveDate, int64_t timestampMs);  // returns stable paymentId
    uint32_t allocatePayment(uint32_t paymentId, uint32_t invoiceId, int64_t amountCents,
                             IsoDate effectiveDate, int64_t timestampMs);   // returns allocationId
    uint64_t reverseAllocation(uint32_t allocationId, int64_t timestampMs); // rejected if in a closed period

    int64_t  settledFor(uint32_t invoiceId) const;                 // Σ net allocations (cents)
    int64_t  outstandingFor(uint32_t invoiceId);                   // total − settled (cents)
    int64_t  unallocatedFor(uint32_t paymentId) const;            // payment − Σ its net allocations
    int64_t  settledAt(uint32_t invoiceId, uint64_t uptoSeq);       // historical settlement as of seq N (replays)
    std::size_t paymentCount() const { return payments_.size(); }
    std::size_t allocationCount() const { return allocations_.size(); }

    // ── Read-only settlement enumeration (UI-facing) ──
    // Projections of the disposable settlement indices — the UI reads these; it never
    // mutates settlement state (writes go through recordPayment/allocatePayment/
    // reverseAllocation). No semantic change; outstanding stays DERIVED.
    struct PaymentRow    { uint32_t id = 0; uint32_t customerId = 0; int64_t amountCents = 0; IsoDate date; };
    struct AllocationRow { uint32_t id = 0; uint32_t paymentId = 0; uint32_t invoiceId = 0;
                           int64_t amountCents = 0; IsoDate date; bool reversed = false; };
    std::vector<PaymentRow>    listPayments() const;                       // all payments (id-ordered)
    std::vector<AllocationRow> allocationsForPayment(uint32_t paymentId) const;
    std::vector<AllocationRow> allocationsForInvoice(uint32_t invoiceId) const;
    int64_t totalPaidByCustomer(uint32_t customerId) const;   // Σ payment amounts for this customer
    int64_t creditForCustomer(uint32_t customerId) const;     // Σ unallocated across this customer's payments

    // ── Double-entry ledger: accounts + balanced journal entries ──
    // A posting moves a signed amount on an account (DEBIT positive, CREDIT negative).
    // Every journal entry must balance: Σ amounts == 0 — enforced at authoring, so no
    // unbalanced entry can ever persist and the trial-balance total is always 0. An
    // account balance is DERIVED (Σ its postings), never a stored running balance.
    struct PostingInput { uint32_t accountId = 0; int64_t amountCents = 0; };

    uint32_t recordAccount(uint8_t type, const std::string& name, int64_t timestampMs);   // returns accountId
    uint32_t recordJournalEntry(IsoDate effectiveDate, const std::vector<PostingInput>& postings, int64_t timestampMs); // returns entryId; throws if unbalanced/closed
    uint32_t reverseJournalEntry(uint32_t entryId, IsoDate effectiveDate, int64_t timestampMs);   // negated entry; returns new entryId

    int64_t     balanceFor(uint32_t accountId) const;                 // Σ postings (signed)
    int64_t     balanceAt(uint32_t accountId, uint64_t uptoSeq);      // historical balance as of seq N (replays)
    int64_t     trialBalanceTotal() const;                           // Σ all balances — invariant 0
    std::size_t accountCount() const { return accounts_.size(); }
    std::size_t entryCount()   const { return entries_.size(); }

    // ── Read-only ledger enumeration (UI-facing) ──
    // Projections of the disposable ledger index — the UI reads these; it never mutates
    // ledger state (writes go through recordAccount/recordJournalEntry/reverseJournalEntry).
    // No semantic change; balances stay DERIVED (balanceFor). AccountType is the u8 enum.
    struct AccountRow      { uint32_t id = 0; uint8_t type = 0; std::string name; int64_t balanceCents = 0; };
    struct JournalEntryRow { uint32_t id = 0; IsoDate effectiveDate;
                             uint32_t reverses = 0xFFFFFFFFu;    // the entry this one negates (or MAX)
                             uint32_t reversedBy = 0xFFFFFFFFu;  // the entry that negates this one (or MAX)
                             std::vector<PostingInput> postings; };
    std::vector<AccountRow>      listAccounts() const;                          // id-ordered, derived balance
    std::vector<JournalEntryRow> listJournalEntries() const;                    // id-ordered, with reversal lineage
    std::vector<JournalEntryRow> entriesForAccount(uint32_t accountId) const;   // entries touching the account
    JournalEntryRow              entryById(uint32_t entryId) const;             // single entry (id 0 postings = absent)

    // ── Financial statements (derived, reconstructible at any seq) ──
    // Account classification drives statement membership. Postings are signed
    // (DEBIT +, CREDIT −); income/expense are income-statement accounts, the rest are
    // balance-sheet accounts. Net income (current, un-closed) = −(Σ income+expense
    // balances). The balance sheet ALWAYS balances because the trial balance is 0.
    enum AccountType : uint8_t { Asset = 1, Liability = 2, Equity = 3, Income = 4, Expense = 5 };
    struct IncomeStatement { int64_t income = 0; int64_t expense = 0; int64_t netIncome = 0; };
    struct BalanceSheet    { int64_t assets = 0; int64_t liabilities = 0; int64_t equity = 0;
                             int64_t netIncome = 0; bool balances = true; };

    IncomeStatement incomeStatementAt(uint64_t uptoSeq);   // activity through seq N
    BalanceSheet    balanceSheetAt(uint64_t uptoSeq);      // position as of seq N (equity incl. net income)

    // ── Tax codes (event-authored, append-only policy) ──
    // A tax code is a TaxCodeCreated fact; never mutated. A rate change is a NEW version of the
    // same `family` (deterministic effective-dated lookup). The tax AMOUNT an invoice/expense
    // pays is captured in its posting (immutable), so changing a rate never reinterprets history.
    uint32_t recordTaxCode(uint8_t type, const std::string& name, int32_t ratePermille,
                           IsoDate effectiveDate, int64_t timestampMs);   // returns taxCodeId
    void     ensureDefaultTaxCodes(int64_t timestampMs);   // idempotent bootstrap (Standard/Zero-rated/Exempt)
    std::vector<TaxCode> listTaxCodes() const;             // id-ordered
    TaxCode  taxCodeById(uint32_t id) const;               // {} if absent (id 0 name empty)
    int32_t  resolveRateAt(uint16_t family, IsoDate asOf) const;  // effective rate of a family at a date
    std::size_t taxCodeCount() const { return taxCodes_.size(); }

    // ── Tax reports (derived from the ledger, reconstructible at any seq) ──
    // collected   = output tax on sales   = −balanceAt(Tax Payable)      (a credit-normal liability)
    // recoverable = input tax on purchases =  balanceAt(Recoverable Tax) (a debit-normal asset)
    // netPayable  = collected − recoverable (what is owed to the tax authority).
    struct TaxSummary { int64_t collected = 0; int64_t recoverable = 0; int64_t netPayable = 0; };
    TaxSummary taxSummaryAt(uint64_t uptoSeq);   // books-as-closed / reportAt(seq)

    // Closing entry: a REAL balanced journal entry that zeroes every income/expense
    // account into retained earnings (so net income flows to equity and the next
    // period starts fresh). Append-only, replayable; returns the entry id.
    uint32_t recordClosingEntry(uint32_t retainedEarningsAccountId, IsoDate effectiveDate, int64_t timestampMs);

    // ── Posting authority: deterministic business-event → ledger mapping ──
    // A FIXED, hardcoded, inspectable policy maps an operational fact to BALANCED ledger
    // postings against ROLE accounts — NOT a configurable DSL / rules engine. Each posting
    // is a real JournalEntryPosted event (authoritative, replayable, crash-safe). Reversing
    // a generated posting is reverseJournalEntry(entryId). Roles are set once (runtime config);
    // determinism of the LEDGER comes from the persisted posting events, not the role config.
    void     setPostingAccounts(uint32_t receivable, uint32_t revenue, uint32_t cash,
                                uint32_t expense = 0xFFFFFFFFu, uint32_t payable = 0xFFFFFFFFu,
                                uint32_t taxPayable = 0xFFFFFFFFu, uint32_t recoverableTax = 0xFFFFFFFFu);
    uint32_t postInvoiceRevenue(int64_t totalCents, IsoDate effectiveDate, int64_t timestampMs);   // Dr AR / Cr Revenue
    uint32_t postPaymentReceipt(int64_t amountCents, IsoDate effectiveDate, int64_t timestampMs);  // Dr Cash / Cr AR

    // Default chart-of-accounts bootstrap: ensure the three role accounts exist as
    // AccountOpened events (idempotent — no-op if ANY accounts already exist), resolve
    // their ids by name, and bind them via setPostingAccounts. Called on every open so the
    // live posting policy is always configured. Returns the bound role ids.
    struct RoleAccounts { uint32_t receivable = 0xFFFFFFFFu, revenue = 0xFFFFFFFFu, cash = 0xFFFFFFFFu,
                                   expense = 0xFFFFFFFFu, payable = 0xFFFFFFFFu,
                                   taxPayable = 0xFFFFFFFFu, recoverableTax = 0xFFFFFFFFu; };
    RoleAccounts ensureChartOfAccounts(int64_t timestampMs);
    int          accountIdByName(const std::string& name) const;   // -1 if none

    // ── Deterministic snapshotting (replay acceleration) ──
    // A snapshot memoizes the ledger balances at a seq boundary. Because history is
    // append-only, a snapshot at seq S is PERMANENTLY valid (events ≤ S never change), so
    // reconstruction can resume from it + replay only the tail (S, N] instead of from
    // genesis. A snapshot is NEVER authority: it is disposable, reproducible from genesis,
    // and self-verifying (its hash must match a genesis replay at S). A missing/corrupt/
    // stale snapshot is silently ignored — the genesis path is always the truth.
    void     writeLedgerSnapshot(uint64_t atSeq);          // crash-safe: temp → fsync → rename
    uint64_t ledgerSnapshotSeq();                          // seq the on-disk snapshot represents (0 = none/invalid)
    int64_t  balanceUsingSnapshot(uint32_t accountId, uint64_t uptoSeq);   // == balanceAt, accelerated when valid
    bool     verifyLedgerSnapshot();                       // false if missing/corrupt/diverged from genesis

    // ── Reconciliation ──
    uint64_t reconcile();            // catch the projection up to the log; returns #applied
    uint64_t rebuildProjections();   // wipe + replay all history; returns #replayed

    // ── Verification & historical reconstruction (read-only w.r.t. authority) ──
    struct VerifyResult {
        bool     ok          = false;   // false = DRIFT: live projection ≠ history
        uint32_t liveHash    = 0;
        uint32_t historyHash = 0;
        uint64_t seq         = 0;       // history head verified against
    };

    // Deterministic fingerprint of the LIVE projection.
    uint32_t liveFingerprint();

    // Reconstruct history (events seq 1..uptoSeq; UINT64_MAX = full) into `scratch`,
    // returning its fingerprint. `scratch` MUST be a separate repository from the live
    // one — it is cleared and rebuilt. NEVER mutates the live projection or the log.
    // This is also the temporal-query primitive: "the books as of seq N".
    uint32_t reconstructInto(CustomerRepository& scratch, uint64_t uptoSeq);

    // Full verification: rebuild from history into `scratch`, compare to live.
    VerifyResult verify(CustomerRepository& scratch);

    // Full-model verification (Domain Cutover): rebuild EVERY event-authored projection —
    // customers, suppliers, invoices, lines — from history into disposable scratch repos and
    // compare each content fingerprint to live. This is verify() generalised to the whole
    // accounting model, so drift in any projection is caught, not just customers.
    struct VerifyAllResult {
        bool ok = false, customersOk = false, suppliersOk = false, invoicesOk = false,
             linesOk = false, expensesOk = false;
        uint64_t seq = 0;
    };
    // `exp` is optional: when passed, the expense projection is reconstructed + byte-compared
    // too (else expensesOk is trivially true — "not checked in this pass", like a null live repo).
    VerifyAllResult verifyAll(CustomerRepository& cust, SupplierRepository& sup,
                              InvoiceRepository& inv, InvoiceLineRepository& lns,
                              ExpenseRepository* exp = nullptr);

    // ── Historical compatibility & evolution governance ──────────────────────────
    // The AUTHORITATIVE record of which engine versions authored this history is a
    // stream of EngineVersionStamp events; `governance_` is their disposable index
    // (rebuilt on open like the other indices). The on-disk compat.manifest is a
    // separate fast-read projection (CompatibilityManifest), rebuildable from these.
    struct GovernanceTransition { GovernanceVersions v; uint64_t seq = 0; };

    // Append a governance version stamp (authoritative, crash-safe). Records that events
    // from this seq forward were authored under `v`.
    uint64_t recordEngineVersionStamp(const GovernanceVersions& v, int64_t timestampMs);

    // One-time adoption/cutover: if history carries NO stamp yet (new books, or books
    // from a pre-governance build), append a genesis stamp at this build's versions so
    // the contract is explicit. No-op once any stamp exists. Returns true if it stamped.
    bool ensureGovernanceStamp(int64_t timestampMs);

    // Forward-migration adoption: if THIS build's version vector exceeds the head stamp on a
    // gating axis AND every such upward step has a registered semantic-migration path, append
    // one EngineVersionStamp(current) so history records the transition and later opens classify
    // Compatible. Refuses (returns false, no stamp) if any axis is a downgrade or lacks a path —
    // the migration is a proven no-op for historical postings (they are immutable events).
    bool adoptVersionTransition(int64_t timestampMs);

    bool                                  hasGovernance()     const { return !governance_.empty(); }
    GovernanceVersions                    currentGovernance() const;   // latest stamp, or all-zero
    const std::vector<GovernanceTransition>& governanceHistory() const { return governance_; }

    // Replay-equivalence gate (the loud proof). Reconstructs from authoritative history
    // and asserts the accounting MEANING is reproducible — not merely the records.
    struct CompatibilityResult {
        bool     ok                      = false;
        bool     genesisReplayOk         = false;   // live projection == full rebuild from history
        bool     snapshotOk              = false;   // snapshot == genesis at its seq (or absent)
        bool     trialBalanceZero        = false;   // ledger invariant
        bool     historicalDeterministic = false;   // reconstructing a boundary twice is identical
        uint32_t liveHash                = 0;
        uint32_t historyHash             = 0;
        uint64_t seq                     = 0;       // history head validated against
        std::string detail;
    };
    // Rebuild into disposable, SEPARATE scratch repos + compare (now the FULL model:
    // customers + suppliers + invoices + lines). Read-only w.r.t. authority. Any false
    // field means history would be reinterpreted → the caller refuses to open, loudly.
    // NEVER mutates the live projections or the log.
    CompatibilityResult validateCompatibility(CustomerRepository& scratchCust,
                                              SupplierRepository&  scratchSup,
                                              InvoiceRepository&   scratchInv,
                                              InvoiceLineRepository& scratchLns,
                                              ExpenseRepository*   scratchExp = nullptr);

    uint64_t lastSeq()    const { return log_.lastSeq(); }
    uint64_t appliedSeq() const { return applied_; }
    bool     tornTail()   const { return log_.recoveredTornTail(); }

private:
    EventLog               log_;
    std::string            cursorPath_;
    std::string            snapshotPath_;     // cursorPath_ + ".ledgersnap"
    CustomerRepository*    customers_;
    SupplierRepository*    suppliers_;
    InvoiceRepository*     invoices_;
    InvoiceLineRepository* lines_;
    ExpenseRepository*     expenses_;
    uint64_t               applied_ = 0;
    std::map<std::string, ClosedPeriod> closedPeriods_;   // disposable index, rebuilt from events

    struct CorrectionInfo { bool voided = false; uint32_t reversedBy = 0xFFFFFFFFu; };
    std::map<uint32_t, CorrectionInfo> corrections_;         // disposable invoice-lineage index
    std::map<uint32_t, CorrectionInfo> expenseCorrections_;  // disposable expense-lineage index

    struct PaymentInfo    { uint32_t customerId = 0; int64_t amountCents = 0; IsoDate effectiveDate; };
    struct AllocationInfo { uint32_t paymentId = 0; uint32_t invoiceId = 0; int64_t amountCents = 0;
                            IsoDate effectiveDate; bool reversed = false; };
    std::map<uint32_t, PaymentInfo>    payments_;         // disposable settlement index
    std::map<uint32_t, AllocationInfo> allocations_;

    struct AccountInfo { uint8_t type = 0; std::string name; };
    struct EntryInfo   { IsoDate effectiveDate; uint32_t reverses = 0xFFFFFFFFu; std::vector<PostingInput> postings; };
    std::map<uint32_t, AccountInfo> accounts_;            // disposable ledger index
    std::map<uint32_t, EntryInfo>   entries_;
    std::map<uint32_t, int64_t>     balances_;            // derived: accountId → Σ postings
    uint32_t roleReceivable_ = 0xFFFFFFFFu, roleRevenue_ = 0xFFFFFFFFu, roleCash_ = 0xFFFFFFFFu;
    uint32_t roleExpense_ = 0xFFFFFFFFu, rolePayable_ = 0xFFFFFFFFu;
    uint32_t roleTaxPayable_ = 0xFFFFFFFFu, roleRecoverableTax_ = 0xFFFFFFFFu;

    std::map<uint32_t, TaxCode> taxCodes_;                // disposable tax-policy index (id → code)

    std::vector<GovernanceTransition> governance_;        // disposable governance index, rebuilt from stamps

    void rebuildPeriodIndex();      // scan history for PeriodClosed/Reopened → closedPeriods_
    void rebuildCorrectionIndex();  // scan history for InvoiceVoided/Reversed → corrections_
    void rebuildExpenseCorrectionIndex();  // scan history for ExpenseVoided/Reversed → expenseCorrections_
    void rebuildSettlementIndex();  // scan history for Payment/Allocation events → payments_/allocations_
    void rebuildLedgerIndex();      // scan history for AccountOpened/JournalEntryPosted → ledger index
    void rebuildTaxIndex();         // scan history for TaxCodeCreated → taxCodes_
    uint32_t reversedByOf(uint32_t entryId) const;   // derived: the entry that negates entryId (or MAX)
    void rebuildGovernanceIndex();  // scan history for EngineVersionStamp → governance_
    bool hasEventOfType(uint16_t type);   // true if history carries ≥1 event of `type`

    // Project one event (idempotent). Routes by type to the live or scratch repos.
    // `sup`/`inv`/`lns`/`exp` may be null (that entity is not projected in this pass).
    void apply(CustomerRepository& cust, SupplierRepository* sup,
               InvoiceRepository* inv, InvoiceLineRepository* lns,
               ExpenseRepository* exp, const EventRecord& r);
    void readCursor();
    void writeCursor(uint64_t seq);     // durable (fsync)
};

#endif // STORAGE_AUDIT_JOURNAL_H
