# Occountant User Guide

This guide explains every screen in Occountant. It assumes no accounting knowledge — where an
accounting term appears, it's explained in plain language.

Occountant is a desktop accounting app for small businesses. It keeps a complete, tamper-evident
history of your books so every figure can be traced back to what actually happened.

---

## The sidebar

The left sidebar switches between screens: **Invoices, Customers, Suppliers, Payments, Expenses,
Ledger, Settings**. The language button is in the top corner (English, French, Arabic — Arabic
displays right-to-left).

You can operate the whole app from the keyboard: **Tab** moves between controls, **Enter/Space**
activates, **Esc** closes a dialog. See the Keyboard Shortcuts guide.

---

## Invoices

Create and track what customers owe you.

- **New Invoice** opens the editor. Pick a customer, set issue/due dates, and add one or more
  lines (description, quantity, unit price, VAT %). Totals update live.
- **Status** — *Draft* (not yet counted) or *Posted* (counted as revenue). Posting an invoice
  automatically records the matching ledger entries.
- The list shows each invoice's status and how much is still outstanding.
- Filter by status (Draft / Posted / Overdue / Paid) and search by number or customer.

## Customers

Your customer list. **New Customer** needs only a name; email, phone, and tax number are optional.
Opening a customer shows their balance (what they currently owe).

## Suppliers

The businesses you buy from. Works like Customers. Suppliers are used when you record expenses.

## Payments

Record money received from customers and match it to invoices.

- **New Payment** — choose the customer, date, amount, and method.
- After saving, Occountant lets you **allocate** the payment: apply it to one invoice, or split it
  across several. One payment can settle many invoices; many payments can settle one invoice.
- Allocations can be **reversed** if you make a mistake — the full history is kept, nothing is
  erased.

## Expenses

Record what your business spends.

- **New Expense** — date, amount, category, and payment method (supplier optional).
- Posting an expense records the matching ledger entries automatically.
- An expense can be **voided** (in an open period). Voiding posts a compensating entry rather than
  deleting anything, so your history stays complete and auditable.

## Ledger

The accountant's view — everything above, expressed as double-entry bookkeeping. You never have to
touch this to run your business, but it's there for you and your accountant.

- **Accounts** — every account and its balance.
- **Journal** — every entry, in order. Click one to inspect its postings. Reversals link back to
  the original entry in both directions.
- **Trial Balance** — proves the books balance: total debits always equal total credits. If it
  ever doesn't, Occountant tells you loudly.
- **Tax** — your VAT summary: output tax you collected on sales, recoverable tax on purchases, and
  the net amount owed to (or refundable by) the tax authority.

## Settings

- **General** — currency symbol and date format.
- **Company** — your business details (shown on invoices/reports).
- **Backup** — create, verify, and restore backups (see the Backup and Restore guides).
- **Diagnostics** — a health check of your books (event count, database size, trial-balance
  status) and a one-click **Run Verification** that re-proves your entire history is consistent.
  You can also generate a **support bundle** here to send to support — it contains diagnostics and
  logs only, never your customers, invoices, or amounts.
- **About** — your version and channel, license status and activation, and update checks.

---

## How Occountant protects your work

- **Always balanced.** Every invoice, payment, and expense posts balanced double-entry records
  automatically. You cannot accidentally unbalance the books.
- **Complete history.** Corrections are added as new entries; nothing is silently deleted or
  overwritten. Your books can be reconstructed from their history at any time.
- **Crash-safe.** If your computer loses power mid-save, Occountant recovers cleanly on the next
  launch — you'll never find half-written data.
- **Private and offline.** Your books never leave your computer. No account, no cloud, no
  telemetry.
