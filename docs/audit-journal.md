# Design Review: Immutable Audit Journal v1

Status: **implemented + verified** (`tools/ptest.sh` — 67 in-process assertions
including the audit subsystem, plus real cross-process crash recovery at every
event/projection window). Customer domain is the proven vertical; the event log is
domain-agnostic and ready for Invoice/Payment events.

> This is accounting infrastructure, not application logging. The event log is the
> authoritative historical record; the existing repositories become disposable
> projections of it.

---

## 1. Architecture & authority boundaries (no hybrid ambiguity)

```
            record*()                  reconcile()/rebuild()
 commit ──▶ AuditJournal ──append──▶  EventLog  ──replay──▶ Projector ──▶ repositories
            (authority)              (TRUTH, append-only)               (disposable cache)
                 │                                                            ▲
                 └──────────────── cursor (applied seq) ─────────────────────┘
```

| Component | Owns | Authoritative? | Rebuildable? |
|-----------|------|----------------|--------------|
| `EventLog` | the immutable, ordered history of facts | **YES** | no (it *is* truth) |
| repositories (`*.dat`) | fast current-state reads | no | **YES** (from the log) |
| `cursor` (one uint64) | how far the projection has applied | no (derived) | yes |
| `AuditJournal` | the write path + reconciliation + invariant enforcement | — | — |

Deliberately **one class**, not an event bus / CQRS framework / async pipeline:
local-first, single-process, synchronous, deterministic, inspectable.

## 2. Event model (granularity decision)

**Transaction-grained domain events, not field-level deltas.** The platform already
has `commit()` as the single atomic mutation boundary — the natural event is *one
commit*. Fine-grained deltas (`LineAdded`, `LineRemoved`) would explode replay cost
and let a half-applied set violate an invoice's invariants. Each event is therefore a
**complete, consistent domain fact**. v1 implements the Customer domain:

| Type (#) | Payload | Meaning |
|----------|---------|---------|
| `CustomerCreated` (1) | `Customer` record (id embedded) | a customer entered the books |
| `CustomerRenamed` (2) | full `Customer` snapshot after rename | correction-style provenance |

The TYPE conveys intent (for history/reporting); the PAYLOAD conveys the resulting
state. `InvoiceCreated`/`InvoiceCorrected` (3/4) are reserved — see §15.

## 3. Storage model

A dedicated append-only file (`audit.log`), **not** `BinaryRecordFile` (fixed-size,
random-access, soft-delete — wrong shape for immutable variable-length history).
32-byte file header with an authoritative `committedLength`; frames packed back to
back:

```
[u32 payloadLen][u64 seq][u16 type][u16 schema][i64 timestampMs][u32 crc][payload…]
```

`seq` is 1-based, monotonic, gap-free. The log is its own journal (§6).

## 4. Projection interaction rules

- The Projector is the **only** writer to the repositories on the event path.
- `apply()` is **idempotent + deterministic**: upsert-at-id, no wall clock, no random.
  Re-applying the boundary event after a crash is a no-op in effect.
- Ids are assigned from projection order at record time and **embedded in the event**,
  so replay reproduces identical ids → byte-identical projections.

## 5. Replay semantics & 6. Deterministic ordering

Ordering is **by `seq`** assigned by the log — never by timestamp (wall clocks are not
monotonic; timestamps are display-only). Replaying an identical event stream produces
a byte-identical projection (proven: live projection bytes == rebuilt projection
bytes). `forEachAfter(seq)` powers incremental catch-up; `forEach()` powers full
rebuild.

## 7. Event versioning / migration

Two independent axes: the **frame `schema`** versions each event type's payload, so a
payload can evolve and be migrated at replay time without rewriting history; the
**type number** is permanent (never renumbered/reused). An unknown type on replay =
history newer than this build → **refuse** (don't silently drop authoritative facts) —
the same downgrade-protection stance as the schema-migration layer.

## 8. Correction semantics

Corrections **append** (`CustomerRenamed`, and later `InvoiceCorrected`) — history is
never mutated. The projection reflects the latest, while the log retains the full
chain (proven: a 2→rename→rename chain projects the final value, with all three events
preserved).

## 9. Observability

`StorageService` exposes `auditEventCount() / auditReconciled() / auditTornTail()`;
`main` logs `acct.storage: audit history: events=N reconciled=M` and, when non-zero,
`acct.recovery: audit reconcile: replayed K event(s)`. Corruption/refusal throws with
a precise message, captured by the `acct.*` diagnostics layer.

## 10. Failure-mode analysis & 13. Crash guarantees

| Failure | Containment |
|---------|-------------|
| Crash mid-frame-write | tail past `committedLength` truncated on open (event never acked) |
| Crash after frame, before commit (`afterEventFrame`) | truncated → event rolled back |
| Crash after event commit, before projection (`afterEventBeforeProject`) | reconcile() replays the gap |
| Crash after projection, before cursor (`afterProjectBeforeCursor`) | reconcile() re-applies (idempotent) |
| Power loss during catch-up | cursor advanced per event → resumes cleanly |
| Bit-flip inside committed history | CRC mismatch on open → **refuse** (loud), not silent divergence |
| Reordered/gap seq | detected on open (gap-free check) → refuse |
| Event type newer than code | refuse (downgrade protection) |

The invariant **no committed projection state without its authoritative events** holds:
the event is committed *before* the projection, and reconcile heals the reverse gap.

## 11. Validation / test harness (all green)

`ACCT_PTEST=suite`: EventLog (replay, **determinism**, torn-tail recovery,
committed-region corruption detection); AuditJournal (head catch-up, **byte-identical
rebuild**, idempotent reconcile, correction chains). Cross-process real-crash
(`tools/ptest.sh`): the process is **killed** at `afterEventFrame`, `afterEventCommit`,
`afterEventBeforeProject`, `afterProjectBeforeCursor`; a fresh process reconciles the
projection to exactly the committed log every time.

## 12. Runtime diagnostics

Reuses the `acct.*` categories. Startup line reports event count + reconcile count;
torn-tail recovery and any refusal are surfaced. No new noise channel.

## 14. Projection rebuild

`rebuildProjections()`: clears the repository (`BinaryRecordFile::clear()`), resets the
cursor, replays the entire log. Proven byte-identical to the live projection — the
operational proof that projections are caches, not truth, and recoverable from history.

## 15. Performance considerations

Append = 2 fsyncs (frame + header commit) + 1 (cursor) — bounded, and accounting
writes are low-frequency. Open validates the whole committed log (CRC + seq); fine for
desktop volumes. If history grows large, the future lever is **periodic snapshots**
(a projection checkpoint + a starting seq) so replay/validation need not scan from
genesis — deliberately deferred (no premature optimization).

## 16. Explicit non-goals

No event bus, no CQRS framework, no async/eventual consistency, no distributed log, no
enterprise abstraction layers, no snapshots yet, no per-keystroke events, no mutable
audit rows / before-after blobs / JSON dumps. Local-first, deterministic, synchronous.

---

## Integration status & next increment

The journal is **live**: `StorageService` constructs it, validates the log, and
reconciles on every startup, with full observability. The `record*()` write path +
projector + reconciler are proven end-to-end on real entities.

**Deliberately staged (next increment), to avoid rewiring verified commit paths in the
same change that introduces the event store:** route the live ViewModel `commit()`
paths through `AuditJournal.record*()` (Customer first — the proven vertical), then add
the `InvoiceCreated`/`InvoiceCorrected` projector with **deterministic line ids**
(today's append-assigned line ids would break byte-identical rebuild for multi-record
invoices — the one open determinism detail, scoped out of v1 on purpose). itest is the
regression guard for that cutover. After it, period locking and append-only invoice
corrections build directly on this log.
