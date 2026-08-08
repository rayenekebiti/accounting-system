#ifndef STORAGE_SERVICE_H
#define STORAGE_SERVICE_H
#include "TransactionRepositories.h"
#include "AccountRepository.h"
#include "CategoryRepository.h"
#include "BudgetRepository.h"
#include "CustomerRepository.h"
#include "SupplierRepository.h"
#include "ProductRepository.h"
#include "InvoiceRepository.h"
#include "InvoiceLineRepository.h"
#include "PaymentRepository.h"
#include "ExpenseRepository.h"
#include "AuditJournal.h"
#include "CompatibilityManifest.h"
#include "SemanticMigration.h"
#include <QLockFile>
#include <QString>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <stdexcept>
#include <unordered_map>

// Layer 3 — StorageService (facade / singleton).
// Owns all repositories; the UI never touches a repository constructor directly.
//
// Usage:
//   if (!StorageService::instance().initialize("/path/to/data/dir")) { ... }
//   StorageService::instance().customers().save(cust);
class StorageService {
    std::unique_ptr<QLockFile>              lockFile_;
    std::unique_ptr<TransactionRepository>  transactions_;
    std::unique_ptr<AccountRepository>      accounts_;
    std::unique_ptr<CategoryRepository>     categories_;
    std::unique_ptr<BudgetRepository>       budgets_;
    std::unique_ptr<CustomerRepository>     customers_;
    std::unique_ptr<SupplierRepository>     suppliers_;
    std::unique_ptr<ProductRepository>      products_;
    std::unique_ptr<InvoiceRepository>      invoices_;
    std::unique_ptr<InvoiceLineRepository>  invoiceLines_;
    std::unique_ptr<PaymentRepository>      payments_;
    std::unique_ptr<ExpenseRepository>      expenses_;
    std::unique_ptr<AuditJournal>           audit_;
    std::string                             initError_;
    std::string                             dataDir_;
    uint64_t                                auditEvents_     = 0;
    uint64_t                                auditReconciled_ = 0;
    uint64_t                                auditBackfilled_ = 0;
    bool                                    auditTornTail_   = false;
    compat::Compatibility                   compatibility_   = compat::Compatibility::Compatible;
    GovernanceVersions                      governance_;
    bool                                    governanceAdopted_ = false;
    bool                                    incompatible_      = false;

    StorageService() = default;

    static int64_t nowMs()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch()).count();
    }

    // True if every governance axis that is BELOW this build (needs forward migration)
    // has a registered accounting-semantic migration path. An unregistered path must be
    // refused, never guessed. v1's registry is empty → any needed migration returns false.
    static bool allSemanticMigrationsRegistered(const GovernanceVersions& onDisk)
    {
        const GovernanceVersions code = compat::current();
        struct A { const char* name; uint16_t disk, code; };
        const A axes[] = {
            { "schema",         onDisk.schema,         code.schema },
            { "replay",         onDisk.replay,         code.replay },
            { "postingPolicy",  onDisk.postingPolicy,  code.postingPolicy },
            { "statement",      onDisk.statement,      code.statement },
            { "snapshot",       onDisk.snapshot,       code.snapshot },
            { "eventLogFormat", onDisk.eventLogFormat, code.eventLogFormat },
        };
        for (const A& a : axes)
            if (a.disk != 0 && a.disk < a.code && !semantic::hasPath(a.name, a.disk, a.code))
                return false;
        return true;
    }

public:
    static StorageService& instance()
    {
        static StorageService inst;
        return inst;
    }

    // Returns the human-readable reason the last initialize() call failed.
    const std::string& lastInitError() const { return initError_; }

    // Opens (or creates) all data files under dataDir.
    // Acquires an exclusive lock to prevent two instances from running simultaneously.
    // Returns false and sets lastInitError() if the lock cannot be acquired
    // or any file fails to open.
    bool initialize(const std::string& dataDir)
    {
        initError_.clear();

        // Acquire an exclusive file lock so a second instance fails fast.
        const QString lockPath = QString::fromStdString(dataDir + "/accountingpro.lock");
        lockFile_ = std::make_unique<QLockFile>(lockPath);
        if (!lockFile_->tryLock(300)) {
            lockFile_.reset();
            initError_ =
                "AccountingPro is already running, "
                "or the data folder is locked by another process.";
            return false;
        }

        dataDir_ = dataDir;
        try {
            transactions_  = std::make_unique<TransactionRepository>(dataDir + "/transactions.dat");
            accounts_      = std::make_unique<AccountRepository>    (dataDir + "/accounts.dat");
            categories_    = std::make_unique<CategoryRepository>   (dataDir + "/categories.dat");
            budgets_       = std::make_unique<BudgetRepository>     (dataDir + "/budgets.dat");
            customers_     = std::make_unique<CustomerRepository>   (dataDir + "/customers.dat");
            suppliers_     = std::make_unique<SupplierRepository>   (dataDir + "/suppliers.dat");
            products_      = std::make_unique<ProductRepository>    (dataDir + "/products.dat");
            invoices_      = std::make_unique<InvoiceRepository>    (dataDir + "/invoices.dat");
            invoiceLines_  = std::make_unique<InvoiceLineRepository>(dataDir + "/invoicelines.dat");
            payments_      = std::make_unique<PaymentRepository>    (dataDir + "/payments.dat");
            expenses_      = std::make_unique<ExpenseRepository>     (dataDir + "/expenses.dat");

            // Authoritative audit history. Opening validates the committed log (throws
            // on corruption → init fails loudly). reconcile() catches the projection up
            // to history, healing any crash that left it behind the log.
            audit_ = std::make_unique<AuditJournal>(
                dataDir + "/audit.log", dataDir + "/audit.cursor",
                customers_.get(), invoices_.get(), invoiceLines_.get(), suppliers_.get(),
                expenses_.get());
            auditTornTail_   = audit_->tornTail();
            // One-time cutover: adopt any pre-audit projection state into history so the
            // log is authoritative for everything the projection holds. now() is fine —
            // timestamps are display-only; ordering is by seq.
            auditBackfilled_ = audit_->backfillCustomers(nowMs());
            auditReconciled_ = audit_->reconcile();

            // ── Full domain event-sourcing cutover ──
            // Adopt any entities still written by an old direct-persistence path
            // (suppliers, invoices) into authoritative history. If anything was adopted,
            // canonicalise the live projection to a pure replay (collapses stale line-id
            // gaps from old in-place edits) so live == history exactly.
            const uint64_t supAdopted = audit_->backfillSuppliers(nowMs());
            const uint64_t invAdopted = audit_->backfillInvoices(nowMs());
            if (supAdopted > 0 || invAdopted > 0)
                audit_->rebuildProjections();
            auditBackfilled_ += supAdopted + invAdopted;

            // Default chart of accounts (AR / Revenue / Cash / Expenses / AP / Tax Payable /
            // Recoverable Tax role accounts) so invoice/expense commits post to the ledger.
            // Idempotent; binds the roles on every open.
            audit_->ensureChartOfAccounts(nowMs());
            // Default tax policy (Standard / Zero-rated / Exempt) — idempotent bootstrap.
            audit_->ensureDefaultTaxCodes(nowMs());

            // ── Historical compatibility & evolution governance ──
            // Adopt/backfill a governance stamp so history explicitly records which engine
            // versions authored it (new books AND pre-governance books both get one).
            governanceAdopted_ = audit_->ensureGovernanceStamp(nowMs());
            // Forward-migration adoption: if this build's contract exceeds the head stamp on a
            // migratable axis (e.g. postingPolicy 1→2 for the tax engine), append a transition
            // stamp so existing books advance cleanly to the current version and classify
            // Compatible (a no-op for historical postings — they are immutable events).
            audit_->adoptVersionTransition(nowMs());
            governance_        = audit_->currentGovernance();

            // The authoritative contract is the log's stamp (governance_). The on-disk
            // manifest normally agrees, but if a NEWER build wrote it (a rollback/tamper),
            // treat the books as that-newer too. Effective on-disk version = per-axis max
            // of (log stamp, manifest) — so a newer manifest OR a newer log both refuse.
            GovernanceVersions manifestV;
            if (CompatibilityManifest::read(dataDir + "/compat.manifest", manifestV)) {
                governance_.schema         = std::max(governance_.schema,         manifestV.schema);
                governance_.replay         = std::max(governance_.replay,         manifestV.replay);
                governance_.postingPolicy  = std::max(governance_.postingPolicy,  manifestV.postingPolicy);
                governance_.statement      = std::max(governance_.statement,      manifestV.statement);
                governance_.snapshot       = std::max(governance_.snapshot,       manifestV.snapshot);
                governance_.eventLogFormat = std::max(governance_.eventLogFormat, manifestV.eventLogFormat);
            }

            // Classify the on-disk contract against THIS build. Older-but-supported would
            // migrate forward; NEWER (a downgrade) or below-floor is REFUSED — historical
            // meaning is never silently reinterpreted (mirrors BinaryRecordFile refuse-newer
            // and apply() unknown-type refusal).
            std::string reason;
            compatibility_ = compat::classify(governance_, reason);
            if (compatibility_ == compat::Compatibility::MigrationRequired
                && !allSemanticMigrationsRegistered(governance_)) {
                // A required forward migration with no registered accounting-semantic path
                // is unsafe to guess at. v1 ships an empty registry, so refuse loudly.
                compatibility_ = compat::Compatibility::Incompatible;
                reason += " (no registered semantic-migration path)";
            }
            if (compatibility_ == compat::Compatibility::Incompatible) {
                incompatible_ = true;
                initError_ = "Incompatible data version — " + reason
                    + ".\n\nThis copy of AccountingPro is older than these books. "
                      "Upgrade to open them.";
                lockFile_.reset();
                return false;
            }

            // Persist the fast-read manifest projection (crash-safe temp→fsync→rename). It
            // is NOT authority — rebuildable from the stamp events — so a write fault is
            // non-fatal; it just means the next open rebuilds it.
            try { CompatibilityManifest::write(dataDir + "/compat.manifest", governance_); }
            catch (const std::exception&) { /* disposable projection — ignore */ }

            auditEvents_ = audit_->lastSeq();
            return true;
        } catch (const std::exception& e) {
            initError_ = e.what();
            lockFile_.reset();
            return false;
        }
    }

    TransactionRepository& transactions()
    {
        if (!transactions_) throw std::logic_error("StorageService not initialized");
        return *transactions_;
    }
    AccountRepository& accounts()
    {
        if (!accounts_) throw std::logic_error("StorageService not initialized");
        return *accounts_;
    }
    CategoryRepository& categories()
    {
        if (!categories_) throw std::logic_error("StorageService not initialized");
        return *categories_;
    }
    BudgetRepository& budgets()
    {
        if (!budgets_) throw std::logic_error("StorageService not initialized");
        return *budgets_;
    }
    CustomerRepository& customers()
    {
        if (!customers_) throw std::logic_error("StorageService not initialized");
        return *customers_;
    }
    SupplierRepository& suppliers()
    {
        if (!suppliers_) throw std::logic_error("StorageService not initialized");
        return *suppliers_;
    }
    ProductRepository& products()
    {
        if (!products_) throw std::logic_error("StorageService not initialized");
        return *products_;
    }
    InvoiceRepository& invoices()
    {
        if (!invoices_) throw std::logic_error("StorageService not initialized");
        return *invoices_;
    }
    InvoiceLineRepository& invoiceLines()
    {
        if (!invoiceLines_) throw std::logic_error("StorageService not initialized");
        return *invoiceLines_;
    }
    PaymentRepository& payments()
    {
        if (!payments_) throw std::logic_error("StorageService not initialized");
        return *payments_;
    }
    ExpenseRepository& expenses()
    {
        if (!expenses_) throw std::logic_error("StorageService not initialized");
        return *expenses_;
    }

    // Authoritative audit history (events → projections). The commit paths route
    // customer creation/rename through here; other entities are being migrated onto it.
    AuditJournal& audit()
    {
        if (!audit_) throw std::logic_error("StorageService not initialized");
        return *audit_;
    }

    // Startup observability for the audit subsystem.
    uint64_t auditEventCount()   const { return auditEvents_; }     // committed events on disk
    uint64_t auditReconciled()   const { return auditReconciled_; } // events replayed into the projection on open
    uint64_t auditBackfilled()   const { return auditBackfilled_; } // records adopted into history on cutover
    bool     auditTornTail()     const { return auditTornTail_; }   // an uncommitted tail was discarded

    // Milliseconds since epoch — display timestamp for events (ordering is by seq).
    static int64_t now() { return nowMs(); }

    // Verify the live customer projection against authoritative history. Rebuilds into
    // a disposable scratch projection under dataDir/.verify/ and compares fingerprints
    // — NON-DESTRUCTIVE to the live projection and the log. ok=false means DRIFT.
    AuditJournal::VerifyResult verifyAuditProjection()
    {
        if (!audit_ || !customers_) throw std::logic_error("StorageService not initialized");
        const std::string scratchDir = dataDir_ + "/.verify";
        std::error_code ec;
        std::filesystem::create_directories(scratchDir, ec);
        CustomerRepository scratch(scratchDir + "/customers.dat");
        return audit_->verify(scratch);
    }

    // ── Historical compatibility governance (observability + on-demand deep gate) ──
    compat::Compatibility     compatibilityStatus() const { return compatibility_; }
    const GovernanceVersions& governanceVersions()  const { return governance_; }
    bool                      governanceAdopted()   const { return governanceAdopted_; }
    // True when the last initialize() refused because the books are newer than this build
    // (or below a migration floor). Lets the app hard-refuse rather than run degraded.
    bool                      refusedIncompatible() const { return incompatible_; }

    // Deep replay-equivalence gate. Rebuilds into a disposable scratch projection under
    // dataDir/.compat and proves history reconstructs to the same accounting MEANING —
    // non-destructive to the live projection and the log. ok=false ⇒ history would be
    // reinterpreted (the caller must refuse to open, loudly).
    AuditJournal::CompatibilityResult validateCompatibility()
    {
        if (!audit_ || !customers_) throw std::logic_error("StorageService not initialized");
        const std::string scratchDir = dataDir_ + "/.compat";
        std::error_code ec;
        std::filesystem::create_directories(scratchDir, ec);
        CustomerRepository    sc(scratchDir + "/customers.dat");
        SupplierRepository    ss(scratchDir + "/suppliers.dat");
        InvoiceRepository     si(scratchDir + "/invoices.dat");
        InvoiceLineRepository sl(scratchDir + "/invoicelines.dat");
        ExpenseRepository     se(scratchDir + "/expenses.dat");
        return audit_->validateCompatibility(sc, ss, si, sl, &se);
    }

    // Operator-visible compatibility report (deliverable 8). runValidation=true runs the
    // deep replay-equivalence gate (O(history)); false reports versions + classification only.
    CompatibilityReport compatibilityReport(bool runValidation)
    {
        CompatibilityReport rep;
        rep.versions       = governance_;
        rep.classification = compatibility_;
        rep.headSeq        = audit_ ? audit_->lastSeq() : 0;
        const auto& hist   = audit_->governanceHistory();
        rep.migrationCount = hist.empty() ? 0 : hist.size() - 1;   // version transitions after genesis
        if (runValidation) {
            const auto cr = validateCompatibility();
            rep.validationRun           = true;
            rep.replayValidated         = cr.genesisReplayOk;
            rep.snapshotValidated       = cr.snapshotOk;
            rep.trialBalanceZero        = cr.trialBalanceZero;
            rep.historicalDeterministic = cr.historicalDeterministic;
        }
        return rep;
    }

    // Derived balance: customer's startingBalance + Σ posted/overdue/paid invoice totals
    // - Σ payments received from this customer.
    Money computeCustomerBalance(uint32_t customerId)
    {
        Money bal;
        for (const auto& c : customers_->loadAll())
            if (c.getId() == customerId && !c.getIsDeleted()) { bal = c.getBalance(); break; }

        for (const auto& inv : invoices_->loadAll()) {
            if (inv.getIsDeleted() || inv.getCustomerId() != customerId) continue;
            const auto s = inv.getStatus();
            if (s == INVOICE_POSTED || s == INVOICE_OVERDUE || s == INVOICE_PAID)
                bal += inv.getTotal();
        }
        // Payments are AUTHORITATIVE in the event-sourced settlement engine, NOT the legacy
        // payments repository (which the Quick app never writes). Deriving from the stale repo left
        // the balance frozen when a customer paid; derive it from history instead.
        if (audit_) bal -= Money::fromCents(audit_->totalPaidByCustomer(customerId));
        return bal;
    }

    // Per-customer derived aggregates, computed in a SINGLE pass over each table.
    // The per-id computeCustomerBalance() rescans all invoices+payments on every
    // call (O(n·m) across a customer list); this batch form is O(n+m+p) and is the
    // authoritative source the Customers screen consumes — the UI never re-derives
    // money. balance uses the same formula as computeCustomerBalance().
    struct CustomerAggregate {
        Money balance;
        bool  hasOverdue = false;   // ≥1 overdue invoice → "at-risk"
    };
    std::unordered_map<uint32_t, CustomerAggregate> computeCustomerAggregates()
    {
        std::unordered_map<uint32_t, CustomerAggregate> agg;

        // Seed each customer's starting balance.
        for (const auto& c : customers_->loadAll())
            agg[c.getId()].balance = c.getBalance();

        // Add posted/overdue/paid invoice totals; flag overdue customers.
        for (const auto& inv : invoices_->loadAll()) {
            if (inv.getIsDeleted()) continue;
            const auto s = inv.getStatus();
            if (s == INVOICE_POSTED || s == INVOICE_OVERDUE || s == INVOICE_PAID)
                agg[inv.getCustomerId()].balance += inv.getTotal();
            if (s == INVOICE_OVERDUE)
                agg[inv.getCustomerId()].hasOverdue = true;
        }

        // Subtract customer payments received — from the AUTHORITATIVE settlement engine (a single
        // pass over its payment projection), NOT the legacy payments repository the Quick app never
        // writes. This keeps the Customers screen in agreement with the ledger and settlement.
        if (audit_) {
            for (const auto& p : audit_->listPayments())
                agg[p.customerId].balance -= Money::fromCents(p.amountCents);
        }

        return agg;
    }

    bool isInitialized() const { return customers_ != nullptr; }

    const std::string& dataDir() const { return dataDir_; }

    // Non-copyable, non-movable singleton.
    StorageService(const StorageService&)            = delete;
    StorageService& operator=(const StorageService&) = delete;
};

#endif
