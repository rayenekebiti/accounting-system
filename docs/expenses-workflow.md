# Expenses & Purchasing Workflow

The **expense side** of the business — a fully event-authored operational workflow that mirrors
the Invoice lifecycle. With it the application supports **both revenue and expense flows**. This
is a genuine engine extension (new authoritative event types + a fixed posting policy + a
projection), not a UI-only phase — but it reuses the existing infrastructure wherever possible
and changes **no accounting-core semantics and no persistence model**.

## Architecture review (the minimum extension)

The engine already had everything an expense needs; the review found the minimum additions:

| Existing capability (reused) | Expense extension |
|------------------------------|-------------------|
| `recordInvoiceWithRevenue` — atomic operational event + `JournalEntryPosted` (`appendAtomic`) | `recordExpenseWithPosting` — same pattern |
| `recordInvoiceVoided` / `recordInvoiceReversal` + `corrections_` lineage | `recordExpenseVoided` / `recordExpenseReversal` + `expenseCorrections_` |
| `SupplierRepository` (external event-authored projection) | `ExpenseRepository` (mirrors it) |
| `posting::invoiceRevenue` (fixed C++ policy) | `posting::expensePosting` |
| `verifyAll` / `validateCompatibility` (full-model replay-equivalence) | gain an optional expense scratch repo → the expense projection is byte-verified |
| `apply()` routing; `ensureChartOfAccounts` role binding | 4 expense event cases; per-account idempotent chart adds Expenses + Accounts Payable |

**No rules engine, DSL, plugin, or workflow framework** — the posting policy is fixed,
inspectable C++.

## Event lifecycle

Four append-only authoritative events (`EventTypes.h` 18–21):

- **ExpenseCreated** — a new expense (full record, id embedded).
- **ExpenseCorrected** — an append-only correction (full snapshot; keeps the id).
- **ExpenseVoided** — marks the expense not-effective **in place** (open period only).
- **ExpenseReversed** — links an original to a **new negating expense** (allowed post-close).

Each is projected into the disposable `ExpenseRepository`; void/reversal lineage lives in a
disposable `expenseCorrections_` index rebuilt from history. Nothing is ever a repository write.

**Compatibility:** adding event types at the end is backward-compatible — an older build refuses
data it doesn't understand via `apply()`'s unknown-type throw, and existing data (no expense
events) replays unchanged. So there is **no governance-version bump** (a bump would mark every
existing dataset migration-required against an empty registry). This is exactly how Suppliers
(events 16/17) were added.

## Posting semantics (fixed, balanced by construction)

`posting::expensePosting(version, amount, onCredit, roles)` — the payment method decides the
credit side:

| Fact | Postings |
|------|----------|
| Immediate expense (cash) | **Dr Expenses / Cr Cash** |
| Credit purchase | **Dr Expenses / Cr Accounts Payable** |
| Correction | posts the signed **delta** (Dr/Cr the change) |
| Void (open) | **sign-flipped compensating entry** (undoes the original) |
| Reversal (post-close) | the negating expense's amount is already negative → a **sign-flipped compensating entry** |

Every entry balances (Σ = 0) at authoring, so the **trial balance is always 0**. A single
"Expenses" account carries all expense debits (category is entity metadata, not a sub-account —
see limitations); the credit side is Cash or Accounts Payable. All post through the atomic
`appendAtomic` group with the operational event, so a crash can never leave an expense without
its financial interpretation (or vice versa). Period closure is enforced: correcting or voiding
a closed-period expense is rejected; **reversal is the sanctioned post-close path**.

## Replay model (one atomic fact → many derived views)

An expense is authored **once**. From that authoritative history, everything else is derived and
never recomputed in the UI:

- the **Expense projection** (`ExpenseRepository`) — the list you see;
- the **ledger** — the Expenses + Accounts Payable accounts and the journal entries appear in
  the B3 Ledger Explorer with **zero** changes there;
- the **trial balance** — stays 0;
- **financial statements** — `incomeStatementAt` picks up the Expenses account (net income
  drops); the balance sheet reflects Cash↓ or Accounts Payable↑;
- **ledger snapshots** and **`balanceAt`** historical reconstruction include expense postings.

`verifyAll` reconstructs the expense projection from history into a disposable scratch repo and
byte-compares it to live; `ACCT_COMPAT_VERIFY` now reports
`customers+suppliers+invoices+lines+expenses + snapshot + trial-balance`.

## UI workflow

Nav gains **Expenses** (💸) in the operational group (Invoices · Customers · Suppliers ·
Payments · **Expenses**); the Ledger workspace stays grouped separately.

- **ExpensesScreen** (`ExpensesViewModel` / `ExpenseListModel` / `ExpenseFilterProxy`) — the
  list with a summary (**Spent** / **Cash** / **On credit** — all engine-derived), search, and
  category/method chips. Active rows offer **Void** (open period → status VOID + compensating
  entry); voided rows show a Void badge, reversals a Reversal badge.
- **ExpenseEditor** (`ExpenseEditorViewModel`) — supplier picker, date, amount, category,
  payment method (Cash / Credit), memo; live validation; dirty-state discard guard; RTL +
  localization. `commit()` authors the atomic expense-with-posting; edit issues a correction
  (the payment method is fixed on a correction so the delta posting stays balanced against the
  same credit account).

## Known limitations (documented — NOT built this phase)

- **Not built:** inventory, purchase orders, stock management, tax engine, OCR / receipt
  scanning, recurring expenses, approval workflows.
- **Single "Expenses" ledger account** — every expense debits it; the category (Office / Rent /
  Utilities / Travel / Other) is entity metadata for filtering/reporting, not a per-category
  sub-account (per-category expense accounts are a roadmap item).
- **UI scope** — the editor covers create / edit(correct) / void. **Reversal** is the engine
  post-close path (ptest/itest-proven), not a UI button.
- **Payments don't post cash to the ledger** (a pre-existing model boundary), so synthetic data
  can show Cash in a credit (overdrawn) position — the accounts row now labels the side by the
  balance's actual sign so this reads correctly.

## Roadmap

Per-category expense accounts; purchase orders + supplier bills → an Accounts-Payable ageing
view; recurring expenses; approval workflows; a UI affordance for reversal. All build on — not
around — the authoritative event pipeline.

## Verification

`tools/ptest.sh` — `testExpenseLifecycle` (cash + credit posting, correction delta, void +
compensating entry, append-only reversal, period-freeze rejection, income-statement effect,
trial balance 0 throughout, **replay-equivalence incl. the expense projection**, snapshot,
deterministic rebuild). `tools/itest.sh` — `expensesWorkflow` (drive the editor VM → one atomic
authoritative group, ledger posting, income statement, void, `verifyAll` byte-identical — **no
repo bypass**, zero QML errors). `ACCT_COMPAT_VERIFY` — full-model equivalence including
expenses. Launch → record an expense and watch it appear in the Ledger, Trial Balance, and
income statement (EN + AR-RTL).
