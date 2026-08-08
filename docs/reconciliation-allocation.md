# Design Review: Deterministic Reconciliation & Allocation Semantics v1

Status: **implemented + verified** (`tools/ptest.sh` — 123 in-process assertions + 17
cross-process crash recoveries incl. atomic settlement). Builds on the event-authored,
period-finalized, correction-aware architecture.

> "Paid" is the trap. This subsystem makes settlement a **derived state computed from
> allocation events** — never a mutable flag — so balances are deterministic,
> reconstructible at any seq, and auditable.

---

## 1. Allocation semantic model

| Concept | Definition |
|---------|-----------|
| **obligation** | an invoice's `total` |
| **payment** | money received (`PaymentRecorded{paymentId, customerId, amount, date}`) |
| **allocation** | applies part of a payment to an invoice (`PaymentAllocated{allocId, paymentId, invoiceId, amount, date}`) |
| **settled(invoice)** | Σ non-reversed allocations to it |
| **outstanding(invoice)** | `invoice.total − settled` (negative ⇒ overpaid/credit) |
| **unallocated(payment)** | `payment.amount − Σ its non-reversed allocations` |
| **settlement reversal** | `AllocationReversed{allocId}` — append-only un-apply |

No "paid" flag exists anywhere; every figure above is computed from events.

## 2. Settlement / reconciliation architecture

A **disposable settlement index** (`payments_`, `allocations_` maps) is rebuilt from
`PaymentRecorded`/`PaymentAllocated`/`AllocationReversed` events — exactly like the
period and correction indices. It is never authoritative; the events are. Queries
(`settledFor`, `outstandingFor`, `unallocatedFor`) read the index; the entity
projections are untouched by settlement (so projection verification is unaffected).

## 3. Allocation-event model & 10. stable identity

Payments and allocations get **stable monotonic ids** assigned at authoring and
embedded in the event (dense by creation order → deterministic on replay). Allocations
target **stable authoritative ids** (`paymentId`, `invoiceId`, `allocId`) — never
positional, never projection-relative.

## 4. Outstanding-balance semantics

`outstandingFor(invoice) = invoice.total − settledFor(invoice)`. Partial settlement →
positive remainder. Over-allocation of a *payment* is rejected (can't allocate beyond
`unallocated`); over-settlement of an *invoice* (overpayment) yields a negative
outstanding = credit (allowed). One payment → many invoices and one invoice → many
payments both fall out of summation (proven).

## 5. Historical settlement reconstruction

`settledAt(invoice, seqN)` replays allocation/reversal events with `seq ≤ N` →
the settlement as of that point. Proven: a partial allocation reads `$20` at its seq
and `$50` after a later payment — deterministic AR reconstruction at any seq.

## 6. Reversal interaction semantics

`AllocationReversed` is append-only: it flags the allocation reversed in the index, so
the amount returns to the payment's unallocated pool and the invoice's outstanding is
restored (proven: reverse a `$100` allocation → INV-1 outstanding back to `$100`,
payment unallocated back to `$100`). The original allocation event is never mutated;
both remain in history. Reversal-of-reversal is not modeled in v1 (a re-allocation is
the forward path); the lineage is fully in the log.

## 7. Replay / reconstruction interaction

`apply()` treats settlement events as **no-ops for entity projections** — they feed the
derived settlement index only. So entity rebuilds + content fingerprints are unchanged
by settlement, and the settlement index rebuilds deterministically (proven: a fresh
journal reproduces identical balances).

## 8. Projection interaction

Settlement never writes the invoice/line/customer projections; it is a separate derived
view. This keeps `verify()`/content-hash drift detection orthogonal to balances.

## 9. Period-close interaction

A settlement whose **effective date** (the payment/allocation date) is in a closed
period cannot be **reversed in place** → rejected; post a compensating allocation
instead (consistent with `docs/correction-semantics.md`). New allocations are always
allowed (settlement moves forward). Proven.

## 11. Crash / recovery guarantees (proven)

Allocation uses the same write-ahead/commit/cursor protocol. Killed at
`afterEventBeforeProject` → reconcile replays + rebuilds the index; at
`afterProjectBeforeCursor` → idempotent re-index. Either way the balance is **consistent**
— INV-1 is fully settled (outstanding 0) *or* unsettled ($100), never partial; the
payment's unallocated matches. No half-applied allocation, no orphan settlement, no
ambiguous balance. (Cross-process kill tests, both windows.)

## 12. Migration implications

No record-layout change — payments/allocations live entirely in events + a derived
index. Schema migration (record layer) stays orthogonal and green.

## 13. Deterministic validation harness (all green)

`ACCT_PTEST=suite`: outstanding of a new invoice; full settlement; partial; one payment
→ two invoices; over-allocation rejected; two payments → one invoice; `settledAt`
historical vs current; reversal restores outstanding + unallocated; closed-period
reversal rejected; deterministic index rebuild. Cross-process
`alloc-crash@afterEventBeforeProject/afterProjectBeforeCursor`.

## 14. Operator workflow model

The UI (next increment) reads `outstandingFor`/`unallocatedFor` to drive
apply-payment flows and shows `settledAt` for AR history. A rejected over-allocation or
closed-period reversal throws a precise message — mistakes surfaced, never silently
mutated.

## 15. Observability / diagnostics & rollout

Startup `acct.storage`: `… payments=P allocations=A`. Rollout: v1 lands payment +
allocation + reversal + historical settlement + period interaction at the `AuditJournal`
layer, proven. Next: surface settlement in the UI (apply-payment, AR aging) alongside
the Phase-3 invoice commit cutover; an AR/AP aging report reconstructed via `settledAt`
across seqs; a compensating-allocation helper for closed periods.

## 16. Explicit non-goals

No mutable "paid" flag; no hidden reconciliation cache that can become authority; no
destructive settlement editing; no multicurrency; no tax engine; no ERP workflows; no
reporting dashboards; no distributed/async. Local-first, deterministic, append-only.

---

## Critical design questions — answered

1. *Obligation?* an invoice total. 2. *Settlement?* Σ non-reversed allocations. 3. *One
   payment → many invoices?* yes (multiple `PaymentAllocated`). 4. *One invoice → many
   allocations?* yes. 5. *Partial?* allocation < total → remainder outstanding. 6.
   *Outstanding at seq N?* `total − settledAt(N)`. 7. *Reversals affect lineage?*
   `AllocationReversed` un-applies, append-only, both events kept. 8. *Post-close
   allocations?* allowed (forward); reversing a closed-period allocation rejected. 9.
   *Authoritative "paid"?* derived (`outstanding ≤ 0`), never a flag. 10. *Chains
   deterministic?* id-linked events + summation → identical balances on replay. 11.
   *Version mismatch?* refuse unknown event types. 12. *Projection hashes?* unaffected —
   settlement is a separate derived index. 13. *Operator mistakes?* over-allocation /
   closed-period reversal rejected with precise messages. 14. *AR/AP reports?*
   reconstruct via `settledAt` (+ `outstandingFor`) at any seq, deterministically.
