# Pilot Usage Scenarios — Occountant (Phase C9)

Four representative small businesses, walked end-to-end through Occountant's **real shipping
ViewModels/QML** and covered by the automated **acceptance** suite (`tools/acceptance.sh`, one
persona per business). Each scenario lists: the daily workflow, where the numbers land in the
ledger, the friction a real owner meets, and the automated evidence that the lifecycle stays correct.

> These simulate the *shape* of real pilot use so we know the workflows hold before real customers
> touch them. They are **not** a substitute for real pilot users — that is the honest limitation
> recorded in the final report.

Persona map (phase business → acceptance persona):

| Phase business | Persona | Shape |
|---|---|---|
| A — Freelancer | `freelancer` | High-value service invoices, few customers, 20% VAT |
| B — Small retail shop | `retail` | Many customers, frequent payments, daily expenses, 20% VAT |
| C — Consultant / service company | `consultant` | VAT invoices, retainers, monthly reporting |
| D — Repair business | `repair` | Multi-line parts+labour invoices, customer balances, expenses |

(The suite also runs `cafe` and `clinic` — cash/VAT-exempt shapes — so exempt-tax handling is
exercised too. All six run green.)

---

## Business A — Freelancer

**Who:** a sole trader billing a few clients high-value service invoices; needs a clean branded
invoice, a clear "who owes me", and a VAT total for the return.

**Daily workflow:**
1. **Customers → New Customer** — add the client (name required; email validated inline).
2. **Invoices → New Invoice** — pick the client, set issue/due dates, add a line (e.g. 1 × $1,500 @
   20% VAT), set **Posted**, **Save**.
3. **Export PDF / CSV** — deliver a professional invoice (company header, line items, totals,
   **Balance Due**) in the current UI language.
4. **Payments → New Payment** — record the client's payment; open it and **allocate** to the invoice.
5. **Ledger → Tax** — read the VAT summary for the return; **Export CSV** for the accountant.

**Where it lands:** posting the invoice hits **Dr Accounts Receivable / Cr Revenue / Cr Output Tax**;
the receipt posts **Dr Cash / Cr AR**. Totals derive per line in int64 cents (subtotal + tax = total
exactly).

**Friction a real freelancer meets:** the Export buttons appear only after the first Save (Save →
Export order); recording and *allocating* a payment are two steps. Both are documented in the user
guide; neither blocks the daily cycle.

**Automated evidence (`acceptance freelancer`):** create customers → issue high-value 20%-VAT
invoices → receive payments → record expenses → run reports; trial balance stays 0, AR and tax
reconcile.

---

## Business B — Small retail shop

**Who:** many small sales, frequent payments, daily expenses; needs fast entry, cash-vs-credit
clarity, and a backup they can trust.

**Daily workflow:**
1. **Customers** — add walk-in/regular customers as needed (or use a single "Cash sales" customer).
2. **Invoices** — issue 20%-VAT sales quickly; **Posted**; repeat through the day.
3. **Payments** — record frequent receipts; allocate to the open invoice(s).
4. **Expenses → New Expense** — enter daily costs (stock, utilities), choosing **Cash** or **Credit**
   and an optional tax code; "— none —" supplier is allowed for cash expenses.
5. **Settings → Backup** — **Back Up Now** (hourly automatic backups also run); **Verify** a backup.

**Where it lands:** each expense posts **Dr Expense (/ Dr Recoverable Tax) / Cr Cash|AP**; switching
an expense between Cash and Credit on a correction reverses-and-reposts the funding side correctly.

**Friction a real shopkeeper meets:** high transaction *count* is the pressure — entry is
keyboard-friendly but there's no bulk/import path (by design for the pilot). Restore requires a
restart (files are locked while running); the status banner explains it.

**Automated evidence (`acceptance retail`):** multiple customers/suppliers, repeated VAT invoices,
frequent payments, daily expenses; books reconcile; backup captures and verifies the authoritative
`audit.log`.

---

## Business C — Consultant / service company

**Who:** retainer + project billing with VAT, on a monthly cadence; needs per-client statements,
month-end period close, and a P&L the accountant accepts.

**Daily / monthly workflow:**
1. **Invoices** — issue retainer/project invoices with VAT through the month.
2. **Payments** — record and allocate receipts against the right invoices.
3. **Customer editor → Statement** — send a client a dated statement (charges/payments + running
   closing balance); **Customers → Outstanding balances** for the whole book.
4. **Ledger → Trial Balance / Tax** — monthly reporting; **Export CSV** (trial balance, P&L, VAT).
5. **Ledger → Periods → Close a period** *(new in C9)* — at month/quarter-end, freeze the filed
   period (label + start/end). Later edits/voids in that period are refused; corrections go through
   **reversals**. Reopen by label if a filing needs to change.

**Where it lands:** all reports derive from authoritative engine data (no duplicated math); closing a
period is an authoritative, append-only event (verified: it increments the closed-period count and
freezes in-period edits).

**Friction a real consultant meets:** period close is a deliberate month-end action, usually done
with the accountant; the new form makes it a button instead of a hidden capability. Reports export as
**CSV**, not styled PDF (invoices get PDF) — fine for the accountant hand-off.

**Automated evidence (`acceptance consultant`):** retainers + VAT invoices → payments → monthly
reports; plus `ACCT_PILOT=periods` proves the close/reopen VM path and `ACCT_HOSTILE=period` proves a
closed period rejects in-period edits.

---

## Business D — Repair business

**Who:** parts + labour jobs on multi-line invoices with mixed tax; needs invoices that total
correctly and a clear view of what each customer owes.

**Daily workflow:**
1. **Customers** — add the customer (repeat/reference jobs).
2. **Invoices → New Invoice** — add **multiple lines** (parts and labour), each with its own
   quantity, unit price, and tax %; the subtotal and total recompute per line as you edit.
3. **Posted → Save → Export PDF** — hand the customer a clear, itemised invoice.
4. **Payments** — take part-payments; allocate; the customer's **outstanding balance** updates.
5. **Expenses** — record parts bought and shop costs (Cash/Credit).
6. **Customers → Outstanding balances** — see who still owes, with a grand total; per-customer
   **Statement** for chasing balances.

**Where it lands:** multi-line totals are computed per line in int64 cents so parts + labour + mixed
tax total **exactly**; part-payments settle against AR and the derived customer balance tracks them.

**Friction a real repair shop meets:** multi-line entry is the core interaction — it's content-sized
and keyboard-navigable, but there's no saved "parts catalogue" (a candidate class-C request to watch,
not build on one voice). Outstanding balances are a summary, not a dunning workflow.

**Automated evidence (`acceptance repair`):** multi-line parts+labour invoices with mixed tax →
part-payments → expenses → outstanding balances; line totals and customer balances reconcile.

---

## Cross-scenario invariants (hold for all four)

- **Always balanced:** trial balance totals to 0 after every lifecycle (`acceptance`, `ACCT_HOSTILE`).
- **Exact money:** per-line int64 cents; totals = Σ lines, never aggregate-double drift (`ptest`).
- **Safe books:** backups capture + verify the authoritative history; a corrupt/history-less restore
  is refused with live data untouched (`ACCT_PILOT=safety`).
- **Localized:** every screen and both invoice documents render in EN/FR/AR with RTL for Arabic
  (`i18n-check`, `ACCT_ITEST` language switching).
- **Trustable:** the Ledger → Trust panel shows balance, verification, integrity, last backup — all
  read-only (`ACCT_PILOT=trust`).

**Coverage line:** `tools/acceptance.sh` runs all six personas (incl. the four above) end-to-end;
all pass. See the final report for the exact run counts.
