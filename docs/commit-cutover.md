# Design Review: Deterministic Commit Cutover & Stable Child Identity v1

Status: **implemented + verified** (`tools/ptest.sh` — 89 in-process assertions +
11 cross-process crash recoveries; itest 36/36; live customer commits now author
events). Completes the event-authority transition for the Customer domain and
delivers the deterministic child-identity model that unblocks the Invoice cutover.

> The architecture moves from `draft → mutate projection → emit history` to
> `draft → commit() → authoritative event → project`. The first committed fact is
> the event; the projection is its consequence.

---

## 1. Commit cutover architecture

`CustomerEditorViewModel::commit()` no longer writes the repository directly — it calls
`StorageService::audit().recordCustomerCreated/Updated()`. The journal appends the
authoritative event (committed, fsync'd) and *then* its projector updates
`customers.dat`. The projection is never mutated outside the projector.

**Cutover safety — backfill.** A live cutover has a hazard: projection records that
predate the journal have no backing events, so a rebuild would lose them. On init,
`AuditJournal::backfillCustomers()` runs **once** (only when history is empty but the
projection has records) and authors a `CustomerCreated` per existing **slot**
(including soft-deleted ones — skipping any would shift ids and break invoice→customer
foreign keys). After backfill the log fully accounts for the projection. Proven: a
projection built by direct saves, after backfill, rebuilds **content-identical**.

## 2. Stable child identity model (the named highest risk)

Invoice lines get a **stable id = a global monotonic slot, assigned at authoring time
and embedded in the event** — never an index, vector position, insertion order, or
projection-relative order. The projector addresses lines by that embedded id
(`upsertAt`), so:
- replay reproduces every line id exactly → **byte-identical** (content) multi-record
  rebuild (proven for 2 invoices / 5 lines);
- corrections target **identity**: a correction keeps each surviving line's id,
  tombstones lines absent from the new set, and assigns fresh slots to new lines
  (input id `UINT32_MAX`). Proven: keep 0 / drop 1 / modify 2-by-id / add 5 →
  reconstructed identically.

## 3. Event-authoring lifecycle

```
recordInvoiceCreated(inv, lines):
  inv.id      = invoices.count()         # stable parent id
  line[k].id  = lines.count() + k        # stable child ids (monotonic)
  payload     = [Invoice 96][u16 n][InvoiceLine 128]*   # one atomic transaction
  seq = log.append(InvoiceCreated, payload)             # AUTHORITATIVE first
  apply(seq)                                            # then project
  cursor = seq
```

The whole invoice + its lines are **one event** (one commit = one transactional fact),
so a half-applied line set can never exist. `recordCustomerCreated/Updated` follow the
same shape for the single-record case.

## 4. Projection-application rules

`apply()` is idempotent + deterministic (upsert-at-stable-id; no wall-clock/random).
For `InvoiceCorrected` it (a) upserts the parent, (b) tombstones the invoice's live
lines not in the new set, (c) upserts the new set — all by stable id. Re-applying the
boundary event after a crash is a no-op in effect.

## 5–6. Deterministic ordering & replay equivalence

Ordering is by `seq`. Ids come from authoring order (embedded), not runtime/container
behavior. Result: identical event streams ⇒ identical projections, identical child
ids, identical correction targeting, and content-identical rebuilds (all proven).

## 7. Correction-targeting semantics

Corrections reference lines by **stable id**, never position. A reordering of lines in
the UI does not change identity; a modification names the line; a removal tombstones by
id; an addition mints a new id. This is what makes append-only correction workflows and
future period close safe.

## 8. Crash / recovery guarantees (proven)

Inherited from the audit layer and re-proven through the invoice path: kill at
`afterEventFrame` / `afterEventCommit` / `afterEventBeforeProject` /
`afterProjectBeforeCursor` → a fresh process reconciles the projection to exactly the
committed log. No orphan projection, no unstable/reordered child ids, no
partially-authored transaction (an invoice + its lines are one event).

## 9. Migration implications

No record-layout change was needed (line id already lives in the `InvoiceLine` record).
Schema migration (record layer) and event replay remain orthogonal and independently
tested. The cutover is additive: the 3-arg `AuditJournal` ctor still works
(invoice/line repos default null), so customer-only paths/tests are unchanged.

## 10. Projection verification interaction

`verify()`/`reconstructInto()` stay customer-scoped in v1 (invoice events are skipped
when invoice repos are null — a customer-only reconstruction). Extending the
verification/reconstruction *set* to invoices is mechanical (the fingerprint +
rebuild-from-history machinery already exists) and is part of the Invoice cutover
increment.

## 11. Replay-version interaction

An unknown event type on replay refuses (downgrade protection) on every path. Known
event types irrelevant to a given reconstruction (invoice events during a customer-only
rebuild) are skipped, not errored — you are reconstructing a specific projection.

## 12. Deterministic validation harness (all green)

`ACCT_PTEST=suite`: customer backfill (adopt + one-time + content-identical rebuild);
invoice-line identity (stable monotonic ids; invoices+lines content-identical rebuild;
correction by stable id with keep/drop/modify/add; idempotent reconcile). Cross-process
kill at every commit/projection window. itest 36/36 guards the live customer commit.

## 13. Failure-mode analysis

| Failure | Containment |
|---------|-------------|
| Crash between event commit and projection | reconcile replays the gap |
| Crash between projection and cursor | idempotent re-apply on reconcile |
| Pre-cutover record without an event | backfill adopts it before any rebuild |
| Skipping a soft-deleted slot on backfill | **avoided** — every slot is emitted (ids stay stable) |
| Correction referencing a stale position | impossible — corrections use stable ids |
| Half-authored invoice (parent w/o lines) | impossible — one atomic event per transaction |
| Unknown/newer event type | refuse (downgrade protection) |

## 14. Observability / diagnostics

Startup `acct.storage`: `audit history: events=N reconciled=M backfilled=K`. A cutover
adoption logs `acct.recovery: commit cutover: adopted K pre-audit record(s)`. Drift is
detectable via `ACCT_VERIFY=1` (customer projection).

## 15. Incremental rollout strategy

- **Phase 1 — Customer cutover (done):** live customer commits are event-authored; old
  state adopted via backfill; verified live (`events=4` after seeding) + itest.
- **Phase 2 — Deterministic line identity (done):** stable ids + invoice projector +
  correction semantics, proven by deterministic rebuild/correction tests.
- **Phase 3 — Invoice commit cutover (next):** route `InvoiceEditorViewModel::commit()`
  through `recordInvoiceCreated/Corrected` (it already builds Invoice + lines), backfill
  existing invoices+lines, and extend `verify()` to the invoice projection. itest is the
  regression guard. Deferred deliberately — not rewired in the same change that
  introduces the identity model, to preserve the verified invoice commit path.

## 16. Explicit non-goals

No snapshots; no reporting/reconciliation UI; no replay-throughput optimization; no
distributed/CQRS/event-bus/async; no speculative scaling. Local-first, deterministic,
single-process. This phase is authority completion + deterministic identity hardening.

---

## Critical design questions — answered

1. *Stable child ids?* global monotonic slot. 2. *When assigned?* at authoring, in the
   record method. 3. *Preserved across replay?* embedded in the event. 4. *Reorderings?*
   identity is independent of order. 5. *Corrections linked?* by stable id. 6. *Atomic
   batches?* one event = one invoice+lines transaction. 7. *Projection updates verified?*
   content fingerprint + rebuild compare. 8. *Migration for id introduction?* none needed
   (id already in the record). 9. *Deterministic exports?* content-identical rebuilds.
   10. *Version mismatch?* refuse unknown types. 11. *Multi-record byte-identical?* proven.
   12. *Projection disagrees with event identity?* drift detection (`verify`) surfaces it.
