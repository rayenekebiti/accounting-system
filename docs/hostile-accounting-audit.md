# Hostile Accounting-Correctness Audit — Occountant (Phase C6)

**Stance.** Adversarial. I assume prior tests are incomplete and I trust nothing that isn't proven by
driving the **actual shipping ViewModel workflow path** (the QObjects QML binds to), not engine APIs.
The goal is to find places where the UI says one thing and the ledger says another, where projections
disagree, where a financial event produces no posting, and where the trial balance is zero while the
books are economically wrong.

**New executable evidence.** I added a hostile end-to-end suite that drives the real editors
(`CustomerEditorViewModel`, `InvoiceEditorViewModel` + `InvoiceDraftLinesModel`,
`PaymentEditorViewModel`, `PaymentAllocationViewModel`, `ExpenseEditorViewModel`) and then asserts the
**cross-subsystem** invariants an accountant would demand. Each invariant encodes *correct* behaviour;
a violation is printed as a `FINDING`.

- Files: `quick/hostile_accept.{h,cpp}`, dispatched by `ACCT_HOSTILE=payments|expenses|all` in
  `quick/main_quick.cpp`; built into `AccountingQuick` (CMake).
- Run: `ACCT_DATA_DIR=<fresh> ACCT_HOSTILE=all QT_QPA_PLATFORM=windows ./AccountingQuick.exe`
- **Result on the current build: `18 invariant(s) held, 8 CONFIRMED FINDING(s), 0 setup failure(s)`.**
  0 setup failures proves the harness really drives the workflows (customers, invoices, and expenses
  all post correctly); the 8 findings isolate the defects to *payments* and the *expense funding side*.

The suite is designed so a correct build returns **0**. It returns non-zero today by design — that is
the audit result, reproducible on demand.

---

## Workflow-by-workflow verdict

For each workflow, the five required checks: **(1)** correct authoritative event? **(2)** correct
ledger posting? **(3)** all projections from the same authority? **(4)** replay reproduces state?
**(5)** can trial balance be 0 while economically wrong?

| Workflow | (1) Event | (2) Posting | (3) One authority | (4) Replay | (5) TB=0 yet wrong | Verdict |
|---|---|---|---|---|---|---|
| Invoice creation | ✅ `InvoiceCreated` | ✅ Dr AR / Cr Rev / Cr Tax | ✅ | ✅ | — | **OK** |
| Invoice correction | ✅ `InvoiceCorrected` | ✅ delta on fixed roles | ✅ | ✅ | — | **OK** (roles fixed → delta safe) |
| Customer payment | ✅ `PaymentRecorded` | ❌ **none** | ❌ settlement only | ✅ | ❌ **YES** | **CRITICAL C1** |
| Payment allocation | ✅ `PaymentAllocated` | ❌ **none** | ❌ settlement only | ✅ | ❌ **YES** | **CRITICAL C1** |
| Payment reversal | ✅ `AllocationReversed` | ❌ none (nothing to reverse) | ❌ settlement only | ✅ | ❌ follows C1 | **CRITICAL C1** |
| Supplier creation | ✅ `SupplierCreated` | ✅ N/A (not financial) | ✅ | ✅ | — | **OK** |
| Expense creation | ✅ `ExpenseCreated` | ✅ Dr Exp / Dr RecTax / Cr Cash\|AP | ✅ | ✅ | — | **OK** |
| Expense correction | ✅ `ExpenseCorrected` | ⚠️ delta on **new** credit acct only | ❌ ledger vs record | ✅ | ❌ **YES** | **HIGH H‑EXP** |
| Expense void | ✅ `ExpenseVoided` | ✅ compensating (normal); ⚠️ residue if method was changed | ⚠️ | ✅ | ⚠️ edge | **OK / edge → H‑EXP** |
| Tax posting | ✅ within invoice/expense | ✅ Tax Payable / Recoverable Tax | ✅ | ✅ | — | **OK** |
| VAT reporting | ✅ derived | ✅ from ledger balances | ✅ | ✅ | — | **OK** (accrual basis — see note) |
| Period closing | ✅ `PeriodClosed` (engine) | N/A | ✅ (engine) | ✅ | — | **MEDIUM M‑PERIOD** (no UI path) |

---

## Findings

### 🔴 C1 — Customer payments and allocations produce NO ledger posting (Critical)

**What.** Receiving and applying a customer payment updates only the settlement index. It never posts
`Dr Cash / Cr Accounts Receivable`. Cash and AR in the general ledger — and therefore the balance
sheet — are blind to every dollar collected.

**Where.**
- `quick/PaymentEditorViewModel.cpp:39-68` → calls only `audit().recordPayment(...)`.
- `storage/AuditJournal.cpp:768-781` `recordPayment()` appends `PaymentRecorded` + rebuilds the
  settlement index; `apply()` treats `PaymentRecorded`/`PaymentAllocated` as ledger-neutral
  (`AuditJournal.cpp:207-213`).
- The correct posting primitive `postPaymentReceipt()` (`AuditJournal.cpp:1317-1326`) has **no caller
  outside tests**.

**Reproduction (executable).** `ACCT_HOSTILE=payments` → post a $100 invoice, record + allocate a $100
payment through the VMs, then observe:
```
>>> FINDING C1  customer payment must DEBIT Cash by $100 in the ledger      (Cash stays $0)
>>> FINDING C1  customer payment must CREDIT AR to $0 in the ledger         (AR stays $100)
    hold    TB  trial balance is zero                                       (the blind spot)
```
**Manual UI repro:** create a customer → post a $100 invoice → Payments: record $100, allocate to the
invoice → open the Ledger/Trial-Balance screen: **Cash = $0, AR = $100** though the customer paid.

**Accounting impact.** The balance sheet permanently **overstates AR** and (because cash expenses *do*
credit Cash) shows a **negative, meaningless Cash** balance. Bank reconciliation is impossible; AR
aging and any lender/tax report built on the ledger are wrong. This is the exact "balanced but wrong"
state a trial-balance-zero gate cannot detect.

**Minimal fix (no redesign).** In `recordPayment`/`allocatePayment`, author the settlement event **and**
its `paymentReceipt` posting as one `appendAtomic` group — the pattern `recordInvoiceWithRevenue`
already uses (`AuditJournal.cpp:1404-1427`). Decide the AR-clearing leg at **allocation** time (so
unapplied cash lands in a customer-credit/unearned liability, not revenue). Make `reverseAllocation`
post the reversing entry symmetrically.

**Regression test required (added).** `quick/hostile_accept.cpp::auditPayments` — asserts Cash/AR move
and that ledger AR == settlement outstanding. Must reach 0 findings.

---

### 🔴 C2 — Three unreconciled "customer balance" answers; two ignore payments (Critical)

**What.** "How much does this customer owe?" is computed three ways that disagree once a payment
exists:
1. **Settlement** (`outstandingFor`, Payments screen) — reflects the payment (**correct**).
2. **Ledger AR** (`balanceFor(receivable)`, Trial Balance / balance sheet) — never reduced by payments
   (C1).
3. **Customers-screen aggregate** (`StorageService::computeCustomerAggregates`,
   `StorageService.h:390-416`, consumed by `quick/CustomerListModel.cpp:62`) — subtracts
   `payments_->loadAll()`, a repository the Quick app **never writes** (payments go through
   `audit().recordPayment`; `payments().save()` exists only in the legacy Widgets
   `src/ui/pages/payments/PaymentsPage.cpp:156`). So the subtraction is a no-op.

Result: the Customers list and the balance sheet both show the customer still owing $100 after they
paid; only the Payments screen shows $0.

**Reproduction (executable).** Same `ACCT_HOSTILE=payments` run:
```
>>> FINDING C2  ledger AR must equal settlement outstanding for the customer
>>> FINDING C2  Customers-screen balance must equal settlement outstanding (payment reflected)
```

**Accounting impact.** Contradictory receivables across three screens with no indication which is
authoritative → wrong dunning, credit decisions, and period-end AR.

**Minimal fix.** Make the ledger authoritative for AR (fix C1) and derive the Customers-screen balance
from the settlement engine / ledger instead of the dead `payments.dat` path — delete the empty-repo
subtraction in `computeCustomerAggregates`. No schema change.

**Regression test required (added).** The two C2 invariants in `auditPayments` (three-way agreement).

---

### 🟠 H‑EXP — Expense correction that changes the funding method mis-posts the credit side (High)

**What.** `recordExpenseWithPosting` posts only the **net/tax delta** against the credit account of the
**new** payment method (`AuditJournal.cpp:1467-1484`). When a correction changes the method
(Cash↔Credit), the original credit-side entry is never unwound:
- **Pure method change** (same amount): `netDelta = taxDelta = 0` → `willPost = false` → **no posting at
  all**. The expense record now reads "Credit/AP" while the ledger still credits **Cash**.
- **Amount + method change** ($100 Cash → $150 Credit): only the +$50 delta posts, against AP. Ledger
  ends at Cash −$100 / AP −$50 / Expense +$150 — but a $150 credit purchase is Cash $0 / AP −$150.

Expense **void** inherits the same flaw if a method-changing correction happened first: it compensates
the current amount entirely against the current method, leaving residue on the other account
(`AuditJournal.cpp:1515-1531`).

**Reproduction (executable).** `ACCT_HOSTILE=expenses`:
```
>>> FINDING H-EXP  re-classifying cash→credit must UNWIND the Cash credit   (Cash stays −$100)
>>> FINDING H-EXP  re-classifying cash→credit must CREDIT Accounts Payable  (AP stays $0)
>>> FINDING H-EXP  amount+method change must UNWIND the original $100 Cash credit
>>> FINDING H-EXP  amount+method change must leave $150 owed on Accounts Payable
```

**Accounting impact.** Cash and Accounts Payable are both wrong after a routine re-classification (very
common: "I paid that on the card, not cash"). AP under/over-stated → supplier balances and cash
position wrong. Trial balance stays 0.

**Minimal fix (no redesign).** For corrections/voids where the credit account (or any posted account
role) changes, post a **full reversal of the prior posting + a full new posting**, rather than a net
delta — i.e. reverse-and-repost when the account set changes; keep the delta fast-path only when the
accounts are identical. The invoice path is unaffected because its roles are fixed.

**Regression test required (added).** `auditExpenseMethodChange` + `auditExpenseAmountAndMethodChange`.

---

### 🟡 M‑PERIOD — Period closing is not reachable from the shipping UI (Medium)

**What.** The engine has correct historical-finality machinery — `closePeriod`/`reopenPeriod` events,
edits/voids refused in a closed period, reversal as the sanctioned post-close path
(`AuditJournal.cpp:567-680`, `1378-1384`, `1450-1452`). But **no ViewModel or QML in the Quick app
invokes it** (grep: only `quick/ptest.cpp` references `closePeriod`). A user cannot close a period.

**Accounting impact.** Historical finality is unenforceable in practice: any past invoice/expense
remains editable indefinitely, so filed periods can be silently altered. The protection exists but is
dead from the user's perspective — a real risk for audit integrity and tax compliance.

**Reproduction.** Search the shipping UI for any "Close Period" action — there is none. Engine control:
calling `aj.closePeriod(...)` directly does correctly block a later correction (works in `ptest`),
proving the mechanism is sound and only the UI wiring is missing.

**Minimal fix (no redesign, no new engine feature).** Add a Settings/Reports action that calls the
existing `closePeriod`/`reopenPeriod`. Purely UI wiring onto an existing engine capability.

**Regression test required.** Once the VM exists: drive it to close a period, then assert an
`InvoiceEditorViewModel`/`ExpenseEditorViewModel` correction with an in-period date is refused
(`saveFailed`).

---

## Notes (reviewed, not findings)

- **Invoice create/correct** are ledger-correct: totals derived per-line in int64 cents
  (`InvoiceEditorViewModel.cpp:249-256`), delta posted against **fixed** AR/Revenue/Tax roles, so the
  delta model is safe here (unlike expenses).
- **Expense create** and **normal expense void** post correctly; the void compensation exactly cancels
  when no method-changing correction intervened.
- **VAT reporting** derives from ledger `Tax Payable` / `Recoverable Tax` (`taxSummaryAt`), i.e.
  **accrual-basis** VAT (tax due at invoice, not at payment). Correct for accrual; if a cash-basis VAT
  scheme is ever offered it must be a distinct report — flagging the basis, not a bug.
- **Supplier creation** correctly has no posting (not an economic event).

---

## Summary

| ID | Severity | One line | Executable proof |
|---|---|---|---|
| C1 | Critical | Customer payments/allocations never post to the ledger (Cash/AR blind) | `ACCT_HOSTILE=payments` |
| C2 | Critical | Ledger AR & Customers screen ignore payments; three balances disagree | `ACCT_HOSTILE=payments` |
| H‑EXP | High | Expense correction/void mis-posts the credit side when the method changes | `ACCT_HOSTILE=expenses` |
| M‑PERIOD | Medium | Period-close machinery has no UI path; finality unenforceable | (UI/grep) |

The engine's double-entry authoring gate (`Σ postings == 0`) and the invoice/expense posting paths are
sound. The failures are all at **integration**: financial events (payments) that never become postings,
projections (Customers screen) reading a dead source, and a delta-posting shortcut that breaks when the
target account changes. All four are fixable as wiring/logic corrections without redesigning the
event-sourcing architecture — and the added `ACCT_HOSTILE` suite is the regression gate that proves
when they are fixed (target: **0 findings**).

---

## Remediation & verification (post-fix)

All four findings were fixed with minimal wiring/logic changes — no architecture change, no new
product features (M‑PERIOD only exposes an existing engine capability). Re-running the gate:
**`ACCT_HOSTILE=all` → 35 invariants held, 0 findings, 0 setup failures.**

| ID | Status | Fix (minimal) | Regression proof |
|---|---|---|---|
| C1 | ✅ Fixed | `recordPayment` now authors `PaymentRecorded` + `JournalEntryPosted(Dr Cash / Cr AR)` in one `appendAtomic` group (`storage/AuditJournal.cpp`). | `ACCT_HOSTILE=payments` — C1 invariants hold |
| C2 | ✅ Fixed | `computeCustomerBalance`/`computeCustomerAggregates` derive payments from the settlement engine (`totalPaidByCustomer`/`listPayments`); removed the dead `payments.dat` subtraction (`storage/StorageService.h`). | `ACCT_HOSTILE=payments` — C2 invariants hold |
| H‑EXP | ✅ Fixed | `recordExpenseWithPosting` reverses the prior posting in full + posts the new one in full when the credit account changes; keeps the delta fast-path only when accounts are unchanged (`storage/AuditJournal.cpp`). | `ACCT_HOSTILE=expenses` — H‑EXP invariants hold |
| M‑PERIOD | ✅ Fixed | New `PeriodCloseViewModel` (`quick/PeriodCloseViewModel.*`) exposes the existing `closePeriod`/`reopenPeriod`; registered as `periodVm`. | `ACCT_HOSTILE=period` — closed-period invoice edit + expense void refused |

**Full verification battery (all green):**
- `ACCT_HOSTILE=all` → 0 findings / 0 setup failures
- Acceptance (all 6 personas, cash/VAT/exempt/multi-line) → 25 passed, 0 failed each
- `ACCT_PTEST=all` (persistence / replay / ledger) → 268 passed, 0 failed
- `ACCT_FUZZ=all` (settlement + ledger + snapshot) → 13 passed, 0 failed
- `ACCT_COMPAT_VERIFY=1` + `ACCT_VERIFY=1` → replay-equivalence held (full model + snapshot + trial
  balance); live projection == authoritative history.
