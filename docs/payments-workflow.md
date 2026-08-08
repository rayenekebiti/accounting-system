# Payments & Settlement Workflow

UI integration of the existing, proven settlement engine. **No settlement semantics,
accounting model, or storage format changed** — every payment/allocation/reversal is authored
through the `AuditJournal`, and outstanding balances stay derived. This is the fourth fully
event-authored vertical (Customers · Invoices · Suppliers · **Payments**) and the reference
implementation for future operational workflows (Expenses, Purchase Invoices).

## Architecture review

The settlement engine (`storage/AuditJournal`) was already complete and verified:
`recordPayment` / `allocatePayment` / `reverseAllocation` (authoritative events);
`settledFor` / `outstandingFor` / `unallocatedFor` / `settledAt` (derived reads); allocation
lineage; period interaction; replay; `verifyAll` / compatibility / projection verification;
crash safety (`alloc-crash`). This phase added only **read-only UI accessors** — projections of
the private `payments_`/`allocations_` indices, no new semantics:

| Accessor | Returns |
|----------|---------|
| `listPayments()` | all payments (id, customer, amount, date) |
| `allocationsForPayment(id)` / `allocationsForInvoice(id)` | allocation lineage (incl. reversed) |
| `totalPaidByCustomer(id)` / `creditForCustomer(id)` | Σ payments / Σ unallocated for a customer |

## Settlement mapping (events → derived state)

| User action | Authoritative event | Derived effect |
|-------------|---------------------|----------------|
| Record a payment | `PaymentRecorded [pid, customer, amount, date]` | `unallocatedFor(pid) = amount` (a credit until applied) |
| Allocate to an invoice | `PaymentAllocated [aid, pid, invoice, amount, date]` | `settledFor(inv) += amount` → `outstandingFor(inv) = total − settled` ↓; `unallocatedFor(pid) ↓` |
| Reverse an allocation | `AllocationReversed [aid]` | that allocation stops counting → outstanding restored, unallocated restored (history is **never deleted**) |

"Paid" is never a stored flag; every balance above is recomputed from the log and is
replay-equivalent at any seq.

## Workflow — what a user can do

Record a payment (customer + date + amount) → the app flows straight into the **allocation
editor** for that payment. There the user can: allocate the whole or **part** of the payment to
one or more of the customer's open invoices; **leave** any remainder unallocated (a credit);
**review** the payment's existing allocations; and **reverse** an allocation (with a
confirmation) — which restores the invoice's outstanding balance via an append-only reversal.
The Payments screen lists every payment with a searchable list, customer/status filter, a
derived status (Unallocated / Partial / Allocated), an unallocated (credit) indicator, and a
summary (payments count, total received, total unallocated). All values come from the settlement
engine; nothing mutates an invoice balance directly.

## UI flow & screens

- **`PaymentsScreen.qml`** ← `PaymentsViewModel` / `PaymentListModel` / `PaymentFilterProxy` —
  list + summary + search + All/Unallocated/Partial/Allocated chips + `+ New Payment`.
- **`PaymentEditor.qml`** ← `PaymentEditorViewModel` — customer `Select` + date + amount, live
  validation, dirty-guard; `commit()` → `audit().recordPayment(...)`, then opens →
- **`AllocationEditor.qml`** ← `PaymentAllocationViewModel` — an unallocated banner; the
  customer's open invoices with an amount input + **Allocate** (each → `audit().allocatePayment`);
  the payment's existing allocations with **Reverse** behind a `ConfirmDialog` (→
  `audit().reverseAllocation`).
- **Surfacing** — invoice rows show **derived outstanding** + a Paid/Partial badge
  (`audit().outstandingFor/settledFor`); the customer summary shows **Paid** + **Credit**
  (`totalPaidByCustomer` / `creditForCustomer`). Keyboard nav, RTL, localization (`qsTr`),
  validation, and confirmation dialogs are inherited from the shared `Theme` + component set.

## Integration (every operation participates in)

Authoritative event history · deterministic replay · **projection verification** (`verifyAll`
covers the settlement-affected projections; the accessors are replay-stable — proven in
`ptest`) · **compatibility verification** (`ACCT_COMPAT_VERIFY` full-model equivalence holds; no
new event types) · derived settlement state · financial reporting (revenue postings from the
invoice remain; trial balance stays 0 through all settlement activity). No duplicated accounting
logic; **no repository regains write authority** — the itest asserts each op appends exactly one
authoritative event.

## Required invariants (maintained)

Authoritative event history · deterministic replay · **immutable settlement history** (reversal
is append-only, never a delete) · **derived** outstanding (`outstandingFor`, never stored) ·
replay-equivalent allocations · projection disposability (`rebuildProjections` reproduces
identical accessor output) · compatibility guarantees · crash safety (`alloc-crash`).

## Known limitations (genuine gaps, not built this phase)

- **Payment reference / notes** — `PaymentRecorded` has no such field; adding one is a
  storage-format change (out of scope). The editor deliberately omits it rather than silently
  dropping input.
- **Refunds** (negative payments), **recurring payments**, **online payment gateways**, **bank
  reconciliation / statement import**, **payment import**, **receipt printing** — none
  implemented; each is a future feature.
- The surfaced customer **Paid/Credit** come from the settlement engine; the legacy
  `computeCustomerAggregates` "balance" (invoice totals − repo payments) is a separate,
  pre-settlement figure shown as the customer "Outstanding" — the two are distinct sources and
  a future pass should unify the customer balance on the settlement engine.

## Future enhancements

Unify the customer balance on the settlement engine; refunds as signed settlement; an invoice
detail view listing its linked allocations; auto-suggest allocation (oldest-invoice-first);
statement/receipt export. All build on — not around — the authoritative event pipeline.

## Verification

`tools/itest.sh` — a **payments** suite (record; one payment → many invoices; many payments →
one invoice; partial; full settlement; unallocated credit; reversal restoring outstanding;
trial balance 0; `verifyAll` replay-stable; each op = one authoritative event). `tools/ptest.sh`
— `settlement UI accessors` (correctness + replay-stability) + the existing `alloc-crash` family.
`ACCT_COMPAT_VERIFY` — full-model equivalence. Launch → the nav shows Invoices · Customers ·
Suppliers · **Payments**; record + allocate + reverse through the UI.
