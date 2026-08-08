# Design Review: Projection Verification & Historical Reconstruction v1

Status: **implemented + verified** (`tools/ptest.sh` — 76 in-process assertions
incl. verification + reconstruction, plus a cross-process kill during verification).
Builds on the authoritative audit log (`docs/audit-journal.md`). Customer domain is
the proven vertical; the mechanism is domain-general.

> Once projections are disposable, the platform must *prove* they are correct,
> *detect* drift, and *reconstruct* historical state — otherwise replay authority is
> only theoretical. This is accounting-correctness infrastructure, not tooling.

---

## 1. Verification architecture

```
            verify(scratch)
 live repo ─────────┐
                    ├─ compare CRC-32 fingerprints ─▶ ok / DRIFT
 EventLog ─replay─▶ scratch repo (disposable, separate files)
```

`AuditJournal::verify(scratch)` rebuilds the **entire** history into a *separate
scratch projection* and compares its content fingerprint to the live projection's.
Deterministic (CRC over the record region in id order), measurable (two `uint32`
hashes + a bool), and **non-destructive** — the live projection and the log are never
touched. Exposed app-side as `StorageService::verifyAuditProjection()` (scratch under
`dataDir/.verify/`) and `ACCT_VERIFY=1` at startup.

## 2. Drift-detection model

**Authoritative drift = `liveHash ≠ historyHash`.** The live projection's bytes no
longer equal what replaying authoritative history produces. Causes caught: projection
file corruption / bit-rot / external tampering (proven: a single flipped byte in the
record region is detected), a projection-logic bug, or replay nondeterminism. On
drift the result carries both hashes and the head `seq`; the app logs it at
`acct.integrity` **critical** — never silently continues (the *No Silent Drift*
invariant).

## 3–4. Deterministic & historical reconstruction model

`reconstructInto(scratch, uptoSeq)` replays events `seq ∈ [1, uptoSeq]` into a
disposable scratch projection and returns its fingerprint — *"the books as of seq N"*.
Determinism comes from the audit layer: id-from-order, idempotent upsert, no
wall-clock/random → identical streams yield byte-identical projections (proven:
live == full rebuild).

## 5. Projection fingerprint strategy

CRC-32 over the **record region only** (offset 32…end, in id order, including
soft-delete tombstones — they are part of the deterministic state). The container
header is excluded (it is metadata, not accounting content). Empty region → fixed 0.
`BinaryRecordFile::contentHash()`; `CustomerRepository::contentHash()` exposes it.

## 6. Replay verification strategy

Verification *is* a replay: `verify()` = `reconstructInto(scratch, ∞)` + compare. This
also validates replay determinism end-to-end (if replay were nondeterministic, the
rebuilt hash would diverge from the live one that an earlier replay produced).

## 7. Snapshot / checkpoint policy

A snapshot would be a `(seq, projection-content)` pair — and it is **disposable**
(rebuildable from history, never authoritative), so it can never become hidden
authority. **v1 reconstructs from genesis** (correct, and bounded for desktop history
volumes). Snapshots are the documented scaling lever (start reconstruction from the
latest snapshot with `seq ≤ N`, replay forward) — deliberately deferred; no premature
optimization, no speculative infrastructure.

## 8. Recovery semantics

Verification and reconstruction are **read-only with respect to authority**. They write
only to scratch files; they never advance the cursor, mutate the live projection, or
touch the log. So "recovery" of an interrupted verification is trivial: discard the
orphan scratch (a fresh run recreates it). No verification state is persisted that
could be advanced incorrectly.

## 9. Event-version interaction & 10. Migration/replay interaction

Reconstruction replays through the same `apply()` used live, so payload-schema handling
is identical and an unknown event type refuses (downgrade protection) during
reconstruction too. Schema migration (of the *projection's* `BinaryRecordFile`) and
event replay are orthogonal: a scratch projection is created at the current code's
schema and filled by replay — there is no old-schema scratch to migrate. The
record-layer migration tests and the replay tests both stay green.

## 11. Temporal-query semantics (explicit — no "latest value" ambiguity)

| Query | Definition |
|-------|------------|
| **current effective state** | `reconstructInto(·, ∞)` ≡ the live projection |
| **effective-at-seq N** | `reconstructInto(·, N)` |
| **before correction X** | `reconstructInto(·, X.seq − 1)` |
| **corrected state** | `reconstructInto(·, X.seq)` (or current) |
| **at period close** | `reconstructInto(·, periodCloseEvent.seq)` (future event) |

Proven: a create→rename→create stream reconstructs `Alpha` at seq 1, `Beta` at seq 2,
`Beta + Gamma` at seq 3 — and the live projection still reads the latest value.

## 12. Performance strategy

`verify()`/full reconstruction is **O(history)** (one replay + two CRC scans) and
**off the normal path** — on-demand or `ACCT_VERIFY=1`, not per-startup. The
fingerprint itself is one linear scan of the projection. The future lever for large
histories is §7 snapshots; deferred until measured necessary.

## 13. Observability / diagnostics

`acct.integrity`: `projection verified against history at seq N`, or, on drift,
**critical** with both fingerprints + seq. `acct.storage` already reports event count
+ reconcile count at startup. Drift is loud and captured by the diagnostics layer.

## 14. Crash-safety guarantees (proven)

Process **killed** during verification (`duringReconstruct`): the live projection and
the log are byte-unchanged, the cursor is unmoved, and a fresh `verify()` passes — no
corruption, no mutated history, no incorrectly-advanced verification state, no mixed
historical view. (The orphan scratch is irrelevant and recreated on the next run.)

## 15. Deterministic validation harness (all green, `tools/ptest.sh`)

- **Verify equivalence**: a built projection verifies (live == history hashes).
- **Drift detection**: a flipped byte in the live projection → detected, hashes differ.
- **Historical reconstruction**: effective-at-seq 1/2/3, before-correction, current.
- **Non-destructive**: live projection + log byte-identical before/after reconstruction.
- **Crash kill** at `duringReconstruct` (cross-process) → authority intact + verify passes.
- Inherited: replay determinism + byte-identical rebuild (audit layer), corruption
  detection in committed history (EventLog).

## 16. Explicit non-goals

No reporting UI / dashboards / analytics / BI; no distributed systems, CQRS framework,
event bus, async/eventual consistency, opaque background sync; no snapshot
infrastructure yet; no projection cache that can become hidden authority. Local-first,
deterministic, single-process, synchronous.

---

## Critical design questions — answered

1. *Deterministic verification?* CRC fingerprint of records; rebuild into scratch +
   compare. 2. *Authoritative drift?* `liveHash ≠ historyHash`. 3. *Historical
   snapshots?* `(seq, content)`, **disposable**. 4. *Authoritative?* never. 5.
   *Reconstruction boundaries?* any `seq` (and event-relative: `X.seq−1`). 6. *Replay/
   version mismatch?* refusal on unknown type; drift on hash mismatch. 7. *Event-
   version upgrades?* same `apply()` path as live. 8. *Correction chains?* replay to
   the chosen `seq` resolves them historically. 9. *Replay cost over years?* §7
   snapshots (deferred). 10. *Byte-identical rebuilds?* proven (record-region CRC).
   11. *Hashes/version markers?* CRC-32 content hash; seq is the version marker. 12.
   *Operator diagnostics?* `acct.integrity` verified/DRIFT lines + `ACCT_VERIFY=1`.

## Integration status & next increment

Live: `StorageService` exposes `verifyAuditProjection()`; `ACCT_VERIFY=1` runs the deep
check at startup and logs the result. Same staged cutover as the audit layer applies:
routing live `commit()` through the journal (Customer first) and the Invoice projector
with deterministic line ids remain the next increment; after them, period close becomes
a first-class reconstruction boundary (`effective-at periodClose.seq`).
