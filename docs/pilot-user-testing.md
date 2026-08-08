# Pilot User Testing — Occountant (Phase C8)

Purpose: walk the first 5–10 real businesses through Occountant's daily workflow, find friction, and
confirm nothing blocks a non-technical owner from trusting it with real data. This is a usability
audit, not a feature plan. Every task below was exercised through the **real shipping ViewModels/QML**
(the same objects the UI binds to) and is covered by an automated regression (`ACCT_PILOT`, `ACCT_ITEST`,
`ACCT_HOSTILE`).

---

## Target users (personas)

| Persona | Shape of business | What they need to trust on day 1 |
|---|---|---|
| **Freelancer** | High-value service invoices, few customers, 20% VAT | Issue a clean branded invoice; see who owes them; VAT total for the return |
| **Small shop** | Many small cash sales, some VAT-exempt lines | Fast expense entry; cash vs. credit clarity; a backup they can restore |
| **Consultant** | Retainers + VAT, monthly cadence | Statements per client; period close at month-end; P&L export for the accountant |
| **Repair business** | Parts + labour multi-line invoices, mixed tax | Multi-line invoices that total correctly; outstanding balances at a glance |

These map to the six acceptance personas (`cafe/retail/freelancer/consultant/repair/clinic`), each of
which runs the full lifecycle green (25/0).

---

## Common tasks — walkthrough & friction

Legend: ✅ smooth · ⚠️ minor friction (usable) · ⛔ blocker (none remain).

### 1. Create a customer
- **Path:** Customers → New Customer → name/email → Save.
- **Result:** ✅ Immediate; validation is inline (name required, email format). Event-authored.
- **Friction:** ⚠️ None material. The balance column is derived and updates after invoicing/payment.

### 2. Issue an invoice
- **Path:** Invoices → New Invoice → pick customer, dates, add line(s) (desc/qty/unit/tax%) → set
  Posted → Save. Then **Export PDF** / **Export CSV** to deliver it.
- **Result:** ✅ Totals derive per-line in int64 cents (subtotal + tax = total exactly). Posting hits
  the ledger (Dr AR / Cr Revenue / Cr Tax). PDF is produced in the current UI language (EN/FR/AR, RTL
  for Arabic) with company identity, customer, line items, totals, and **Balance Due**.
- **Friction:** ⚠️ Export buttons appear only after the invoice exists (first Save) — intentional, but
  a first-time user may look for "print" before saving. Mitigated by the clear Save→Export order.

### 3. Receive a payment
- **Path:** Payments → New Payment → customer/amount/date → Save → open it → allocate to invoice(s).
- **Result:** ✅ The payment posts **Dr Cash / Cr AR** atomically with the settlement event (fixed in
  C6). The invoice's outstanding drops; the Customers-screen balance, ledger AR, and settlement agree.
- **Friction:** ⚠️ Recording and *allocating* are two steps. Correct for one-payment-many-invoices, but
  a user paying a single invoice does two actions. Acceptable; documented in the user guide.

### 4. Record an expense
- **Path:** Expenses → New Expense → date/amount/category, Cash or Credit, optional tax code → Save.
- **Result:** ✅ Posts Dr Expense (/ Dr Recoverable Tax) / Cr Cash|AP. Changing Cash↔Credit on a
  correction now reverses-and-reposts the funding side correctly (fixed in C6).
- **Friction:** ⚠️ "No supplier" is allowed for cash expenses (sensible); the supplier picker is
  optional and clearly labelled "— none —".

### 5. Export a report / communicate with a customer
- **Path:** Trial Balance / Tax Summary → **Export CSV**; Customers → **Outstanding balances**;
  Customer editor → **Statement**.
- **Result:** ✅ CSV for trial balance, P&L, and VAT summary; a per-customer **statement** (dated
  charges/payments with a running closing balance); an **outstanding balances** summary with a grand
  total. All derive from authoritative engine data — no duplicated math.
- **Friction:** ⚠️ CSV opens in the user's spreadsheet app (universal, no lock-in) but is not a styled
  PDF for reports (only invoices get PDF). Fine for the accountant hand-off; noted as a future nicety.

### 6. Backup / restore
- **Path:** Settings → Backup → Back up now / Verify / Restore. Automatic hourly backups also run.
- **Result:** ✅ Backups capture the authoritative `audit.log` and are verified. **Restore refuses a
  corrupt or history-less backup and leaves live data untouched** (fixed in C7); the apply-on-restart
  swap is atomic (temp→rename).
- **Friction:** ⚠️ Restore requires a restart (the live files are locked while running) — explained in
  the status banner ("Close and reopen Occountant to complete it").

### 7. Close a period (month/quarter end)
- **Path:** `periodVm.closePeriod(label, start, end)` (engine capability exposed in C7).
- **Result:** ✅ Freezes the period; later in-period edits/voids are refused; reversals remain the
  sanctioned post-close correction.
- **Friction:** ⚠️ There is a working VM + regression (`ACCT_HOSTILE=period`) but **no dedicated
  on-screen form yet** — the first close (≈day 30, usually with the accountant) needs the action wired
  to a button. Tracked as a should-fix, not a 30-day blocker.

### 8. First run (new install)
- **Path:** Launch → onboarding wizard (business name/address/tax number/currency/fiscal-year/language)
  → Create company (or explicit Skip).
- **Result:** ✅ A complete company profile before entering the app; **writes settings only, authors no
  accounting events** (verified). Localized and RTL-correct. Existing users never see it.
- **Friction:** ⚠️ Only business name is mandatory (everything else defaults) — deliberate, to avoid a
  wall of required fields on first launch.

### 9. Confidence / "is my data OK?"
- **Path:** Ledger → **Trust** tab.
- **Result:** ✅ One read-only panel: trial balance, book verification, data integrity (event count +
  seq), last successful backup, license, updates, plus an on-demand deep verification (projection ==
  history, deterministic replay). Authors nothing.
- **Friction:** ⚠️ Deep verification is on-demand (correct — it's O(history)); the resting state shows
  "Not yet run" until clicked, which a nervous user might misread. Copy is neutral ("Verify now").

---

## Friction summary & disposition

| # | Friction point | Severity | Disposition |
|---|---|---|---|
| F1 | Export buttons only appear after first Save | Low | Keep (Save→Export order is correct); cover in guide |
| F2 | Payment record + allocate are two steps | Low | Keep (needed for many-invoice allocation) |
| F3 | Reports export CSV, not styled PDF (invoices get PDF) | Low | Future nicety; CSV is the accountant format |
| F4 | Period close has a VM + tests but no on-screen form | Medium | Should-fix before month-end of pilot |
| F5 | Restore needs a restart | Low | Inherent (file lock); banner explains it |
| F6 | Trust "deep verification" resting state reads "Not yet run" | Low | Neutral copy; acceptable |

No ⛔ blockers remain. The C6/C7 correctness and data-safety blockers are fixed and regression-gated;
C8 adds the missing "deliver + trust + first-run" surface.

---

## What was validated automatically (so friction findings are grounded, not guessed)

- `ACCT_PILOT=all` — 50/0: onboarding (no events), invoice PDF/HTML EN/FR/AR + RTL, statement,
  outstanding summary, trust dashboard read-only, backup/restore refusal.
- `ACCT_ITEST` — 123/0 (seeded): drives the Trust tab + invoice PDF through real QML; no QML errors in
  any workflow across EN↔FR↔AR.
- `ACCT_HOSTILE=all` — 0 findings (payments/expenses/period ledger correctness).
- Acceptance — all 6 personas 25/0.
- Screenshots — EN/FR/AR × standard/min, incl. new onboarding + Trust captures; AR renders RTL.
- i18n-check — PASSED, 525/525 strings translated EN/FR/AR.

---

## Recommendation

A first paying pilot (5–10 businesses) can run the full daily cycle — create customer, issue & deliver
an invoice, take payment, record expense, export reports/statements, back up and safely restore, and
watch a trust panel — without developer intervention. The one item to schedule during the pilot (before
their first month-end) is wiring the **period-close form** (F4); the engine and VM already exist.
