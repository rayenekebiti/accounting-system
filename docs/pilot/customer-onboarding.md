# Customer Onboarding — Occountant Pilot (C11)

The customer-facing onboarding path for a pilot business. Operator-side steps are in
`admin-checklist.md`; the deeper operations runbook is `docs/pilot-operation-manual.md`. Keep this
one short — it's what you actually walk a real owner through.

**Target first-run outcome:** within ~15 minutes the owner has issued **one real invoice** and made
**one backup**, unaided by the end.

---

## Before you arrive (operator, per machine) — see `admin-checklist.md`
Installation verified, backup verified, license/trial confirmed, and the first-run wizard reachable.

## Step 1 — First launch → company setup (5 min)
The **onboarding wizard** appears on first run. Help the owner set:
- **Business name** (required), address, tax/VAT number, currency, fiscal-year start, language.
- Everything except the name defaults; fill them anyway so the **first invoice's header is complete**.
- Confirm the language (EN/FR/AR); Arabic renders right-to-left.

This writes **settings only — no accounting events**. They can change it later in Settings → Company.

## Step 2 — Add your first customer (2 min)
- Go to **Customers → New Customer**. Name is required; email is validated.
- *(You can also do this from inside the invoice editor — if you start an invoice with no customers,
  it now offers **Add customer** right there.)*

## Step 3 — Issue your first invoice (5 min)
- **Invoices → New Invoice.** The invoice number, today's date, and a +30-day due date are
  **pre-filled** — you don't invent anything.
- Pick the customer, add a line (description, qty, unit price, tax %). The total updates live.
- Set **Status → Posted** when it's a real invoice (Posted records it in your books). Leave **Draft**
  if it's not final. **Do not** set "Paid" by hand — that's what recording a payment is for (Step 4).
- **Save**, then **Export PDF** to send it, or **Export CSV** for your accountant.

## Step 4 — Record a payment when it arrives (2 min)
- **Payments → New Payment** → customer, amount, date → **Save**.
- The **allocation** screen opens automatically — tick the invoice(s) this payment settles. The
  invoice's outstanding drops and your customer balance updates.

## Step 5 — Record an expense (2 min)
- **Expenses → New Expense** → date, amount, category, **Cash or Credit**, optional tax code → Save.
- Supplier is optional ("— none —") for a cash expense.

## Step 6 — See your numbers
- **Ledger → Trial Balance / Tax** for your reports (export as CSV for the accountant).
- **Ledger → Trust** to confirm your books are balanced, verified, and backed up.
- *(Reports live under **Ledger** — that's the accounting term for your books.)*

## Step 7 — Make a backup, and know how to restore (3 min) — do this together
- **Settings → Backup → Back Up Now.** Occountant also backs up automatically every hour.
- **Verify** it (seconds). Note **where** backups live; if you have a backup drive/cloud folder, copy
  that folder there periodically — Occountant never uploads anything.
- Understand: a restore takes effect after you **reopen** the app, and a corrupt backup is refused so
  it can't overwrite good data.

## Step 8 — Your Support ID + how to get help
- **Settings → Diagnostics → Support** shows your **Support ID** (e.g. `OCC-3F9A-2B71`). Quote it when
  you contact support. For a bug, click **Create support bundle** and send the file — it contains app
  health only, **never your accounting data**.
- Support contact: `[SUPPORT CHANNEL]`. See `docs/pilot-operation-manual.md` §7.

---

## Month-end (when you get there)
- **Ledger → Periods → Close a period** freezes a filed month/quarter so its entries can't change by
  accident. Do this with your accountant at first; corrections after close are posted as reversals.

## Privacy, in one line
Everything is on **your computer**. No cloud, no account, no data leaves your machine unless **you**
choose to send a (data-free) support bundle.
