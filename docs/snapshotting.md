# Design Review: Deterministic Snapshotting & Replay Acceleration Semantics v1

Status: **implemented + verified** (`tools/ptest.sh` — 159 in-process assertions + 19
cross-process crash recoveries incl. atomic snapshot creation). Accelerates ledger
reconstruction without weakening any guarantee.

> The danger is a snapshot becoming hidden authority. It cannot here, by construction:
> **history is append-only, so a snapshot at seq S is permanently valid** (events ≤ S
> never change). A snapshot is a *memoized prefix-fold* — disposable, reproducible from
> genesis, self-verifying, and always overridable by the genesis path.

---

## 1–2. Snapshot architecture & authority model

A snapshot captures the **ledger balances** (the costliest replay) at a seq boundary:
`ledger.snapshot = {seq S, balances map, crc}`, in `cursor.ledgersnap`. **The event log
remains the sole authority.** The snapshot is a derived accelerator: never written by
business logic, never read as truth, always reproducible by replaying genesis→S.

## 3. Replay continuation

`balanceUsingSnapshot(account, N)`: if a valid snapshot at `S ≤ N` exists, start from its
balance and replay **only the tail `(S, N]`**; else replay from genesis. It returns
**exactly `balanceAt(account, N)`** (proven for a longer tail and multiple accounts) —
the accelerator is observationally identical to the authoritative path.

## 4. Snapshot verification

`verifyLedgerSnapshot()`: (a) recompute the stored entries' CRC vs the file's stored
hash (file integrity), and (b) recompute `replayBalances(S)` from genesis and compare to
the snapshot's hash (matches authority). Either failing ⇒ the snapshot is rejected.
Proven against a clean snapshot and a tampered one.

## 5. Invalidation / versioning

- **Stale-from-new-events: impossible** — append-only means events ≤ S are immutable, so
  an older snapshot stays valid forever (proven: still verifies after 3 more entries).
- **Corruption:** detected by the integrity hash → treated as absent (proven: a flipped
  byte ⇒ `ledgerSnapshotSeq()==0`, `verify()==false`).
- **Incompatible format:** a magic + `formatVersion` header; a foreign/older snapshot is
  ignored. Schema/posting-policy evolution (§13) likewise just falls back to genesis.

## 6–8. Historical, replay & projection interaction

Reconstruction from genesis, from snapshot+tail, or from different boundaries all yield
identical balances (the tail replay is the same fold continued). The snapshot touches no
entity projection, so `verify()`/content-hash and statements are unaffected — they read
the same authoritative postings. Statements over `balanceUsingSnapshot` equal statements
over genesis (the balances are identical).

## 9. Period-close interaction

A snapshot is orthogonal to closure: it memoizes balances at a seq; "books as closed"
reconstruction can be accelerated from any snapshot with `S ≤ closedAtSeq`, replaying the
tail to `closedAtSeq` — same result.

## 10–11. Verification / drift & stable identity

Drift is structurally detected: a snapshot that does not match a genesis replay at its
seq fails `verify()`. Accounts/entries keep their stable ids; snapshot entries key on
the stable account id, never position.

## 12. Crash / recovery guarantees (proven)

Creation is **temp → fsync → atomic rename**. A crash leaves the OLD snapshot (or none) —
**never a partial one**; and because the snapshot is not authority, even total loss only
forces a genesis replay. Proven: killed at `afterSnapshotTmp`, a fresh process finds the
snapshot complete-or-absent and accelerated balances still equal genesis.

## 13. Migration implications

A snapshot is bound to the ledger event encoding; a `formatVersion` guards it, and a
schema migration of the record layer is orthogonal (the ledger lives in events). After a
posting-policy change, old postings replay unchanged, so an old snapshot stays valid;
when in doubt the system simply replays genesis (the snapshot is advisory).

## 14. Deterministic validation harness (all green)

`ACCT_PTEST=suite`: snapshot seq boundary; verifies vs genesis; **snapshot+tail ==
genesis**; accelerated balance correct; append-only keeps it valid; **corruption
detected → absent**; verify rejects it; corrupt/missing ⇒ genesis fallback still
correct. Cross-process `snap-crash@afterSnapshotTmp`.

## 15. Observability / diagnostics & rollout

`ledgerSnapshotSeq()` exposes the active boundary (0 = none). Rollout: v1 snapshots the
ledger balances + replay continuation + verification at the `AuditJournal` layer, proven.
Next: snapshot the settlement/correction indices too; an automatic snapshot at each
period close (`closedAtSeq`); a startup snapshot-then-tail reconstruction path for the
live projections (purely an accelerator, genesis-verified).

## 16. Explicit non-goals

No mutable/authoritative checkpoints; no hidden replay shortcuts that bypass
verification; no speculative cache; no distributed replication / clustering; no
storage-engine abstraction; no async replay pipeline; no throughput micro-optimization.
Local-first, deterministic, genesis-reproducible.

---

## Critical design questions — answered

1. *Captured?* ledger balances (account → Σ postings) at a seq. 2. *Validity boundary?*
   its `seq S` (valid forever — append-only). 3. *Resume how?* snapshot balance + replay
   tail `(S, N]`. 4. *Determinism verified?* genesis replay at S must hash-match the
   snapshot. 5. *Stale/incompatible detected?* integrity hash + magic/version; corruption
   ⇒ ignored. 6. *Survive schema migration?* format-versioned; orthogonal record migration;
   else genesis. 7. *Policy evolution?* old postings replay unchanged → snapshot stays
   valid. 8. *Projection hashes?* unaffected. 9. *Statements after continuation?* identical
   (same balances). 10. *Invalidated safely?* delete the file → genesis fallback (proven
   disposable). 11. *Crash window?* temp→fsync→rename ⇒ complete-or-absent. 12.
   *Historical accelerated safely?* snapshot with `S ≤ targetSeq` + tail. 13. *Version
   upgrades?* `formatVersion` + refuse-unknown-events. 14. *Integrity failures surfaced?*
   `verify()` returns false; the genesis path always available.
