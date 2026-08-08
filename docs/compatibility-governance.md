# Design Review: Deterministic Historical Compatibility & Evolution Governance v1

Status: **implemented + verified** (`tools/ptest.sh` — 177 in-process assertions incl.
the compatibility-governance section, plus a real cross-process crash during a
manifest write). Builds on every prior phase; the event log remains the sole authority.

> The core is feature-complete. The remaining risk is not functionality — it is
> **historical correctness while the software itself evolves.** Future releases may
> change schemas, posting policy, replay, statement derivation, or the snapshot format.
> The accounting *meaning* of existing books must never silently change. This phase makes
> software evolution itself deterministic and auditable.

---

## 0. The core principle (enforced, not aspirational)

> Replaying historical books on a future **compatible** build must produce the same
> accounting **meaning** — same balances, settlement state, trial balance, and financial
> statements — not merely the same records.

Why it already half-holds, and how we make it total: postings and statements are
**derived from persisted events, never recomputed from mutable config** (see
`docs/posting-authority.md`, `docs/ledger-double-entry.md`). Old events therefore replay
unchanged. This phase makes that guarantee **explicit** (every axis is a version),
**gated** (an incompatible version refuses to open), and **proven** (a replay-equivalence
validator fails loudly on any drift).

## 1. Replay compatibility — the three boundaries

Compatibility is classified per axis into exactly three outcomes
(`compat::classify`, `storage/CompatibilityManifest.cpp`):

| Outcome | Meaning | Action |
|---------|---------|--------|
| **Compatible** | on-disk version == this build (per axis) | open directly |
| **MigrationRequired** | on-disk < build, ≥ floor, a registered semantic migration exists | forward-migrate, then open |
| **Incompatible** | on-disk **> build** (downgrade) **or** below a migration floor **or** MigrationRequired with no registered path | **refuse to open, loudly** |

Examples mapped: an *implementation refactor* keeps every version equal → Compatible; a
*schema/posting-policy revision* that only adds new behavior bumps one axis and ships a
migration → MigrationRequired; opening books written by a **newer** build → Incompatible
(the exact mirror of `BinaryRecordFile`'s refuse-newer and `AuditJournal::apply()`'s
unknown-event-type refusal). A field value of `0` means "unset / pre-governance" and is
read as the baseline — never a downgrade.

## 2. Posting-policy evolution

The policy is a **fixed, hardcoded, inspectable** table (`storage/PostingPolicy.h`;
`kCurrentPostingPolicyVersion = 1`), NOT a configurable DSL. Versioning it does not make
it configurable — it gives the policy an identity so that:

- **old events retain their original policy**: a generated posting is a persisted
  `JournalEntryPosted` event; replay never re-invokes the policy, so a historical posting
  can never be reinterpreted;
- **replay chooses no policy at all** for historical facts (they are events); only NEW
  operational facts are authored with `kCurrentPostingPolicyVersion`;
- a future **V2** maps new facts differently while V1's history stands untouched. Adding
  V2 obligates a `postingPolicy` axis bump + a semantic-migration entry + the §7
  equivalence proof. `postInvoiceRevenue`/`postPaymentReceipt` now route through the
  versioned table (no behavior change in v1).

## 3. Statement compatibility

Financial-statement derivation carries a `statement` version. Boundary: statements are a
pure function of the ledger postings at a seq (`incomeStatementAt`/`balanceSheetAt`), so a
statement reconstructed after an upgrade is **equivalent** iff the postings and their
classification are unchanged — which they are, because postings are immutable events. The
§7 validator asserts historical determinism of reconstruction; a `statement` bump that
would move a historical statement fails that gate.

## 4. Snapshot compatibility

The ledger snapshot already carries its own format version and is **self-verifying against
genesis** (`docs/snapshotting.md`). Governance adds the `snapshot` axis to the manifest so
a format evolution is declared. Rules unchanged and reaffirmed: a snapshot is a disposable
accelerator, **never authority**; a foreign/older/corrupt snapshot is ignored and the
genesis path is taken; a snapshot at seq S is permanently valid because events ≤ S are
immutable. Invalidation = delete the file → genesis fallback.

## 5. Semantic-migration governance — three distinct tiers

Conflating these is how meaning gets corrupted, so they are separate code paths
(`storage/SemanticMigration.h`):

| Tier | What changes | Owner |
|------|--------------|-------|
| **Binary** | byte encoding, *same* meaning (double → int64 cents) | `MigrationV1` |
| **Schema** | record *layout* (add/reinterpret a field) | `BinaryRecordFile` (forward-only, atomic, downgrade-refused) |
| **Accounting-semantic** | how *events are interpreted* (posting policy, statements, replay) | this registry |

The governing rule for the accounting tier: a migration may only add interpretation for
**new** events and must leave **every pre-migration balance / statement / settlement
unchanged** — enforced by the §7 replay-equivalence gate, which refuses on any historical
change. v1 ships the machinery with an **empty registry** (no semantic migration has been
needed yet); a MigrationRequired axis with no registered path is treated as Incompatible.

## 6. The Compatibility Manifest

A machine-readable version vector — `schema · replay · postingPolicy · statement ·
snapshot · eventLogFormat` (+ an informational `engineBuild`). Two representations:

- **Authoritative**: an `EngineVersionStamp` event (type 15) in the log, appended at
  genesis (adoption/cutover — new *and* pre-governance books both get one) and at every
  version transition. This is the compatibility contract, and it is immutable history.
- **Projection**: `<dataDir>/compat.manifest` — a 36-byte, CRC-checked, crash-safe
  (temp→fsync→rename) fast-read of the current contract, **rebuildable from the stamp
  events** (a missing/corrupt manifest is rebuilt, never fatal). It is also a *gate*: the
  effective on-disk version is the per-axis **max** of (log stamp, manifest), so a newer
  manifest copied in by a rollback/tamper is refused too.

## 7. Replay validation (the loud gate)

`AuditJournal::validateCompatibility(scratch)` proves, read-only w.r.t. authority, and
fails loudly on any mismatch:

- **genesis replay** — the live projection equals a full rebuild from history
  (`verify()`), i.e. same records *and* same bytes;
- **snapshot replay** — a present ledger snapshot equals a genesis replay at its seq
  (absent is fine);
- **historical replay under current rules** — reconstructing a boundary twice is
  byte-identical (determinism), and unknown event types / versions still refuse;
- **ledger invariant** — the trial balance is structurally 0.

Any failure is logged `acct.integrity` **critical** and the process refuses (exit ≠ 0) —
*never* a silent reinterpretation. Opt-in via `ACCT_COMPAT_VERIFY=1` (like `ACCT_VERIFY`);
classification-based refusal runs at every open.

## 8. Compatibility audit (operator-visible)

`StorageService::compatibilityReport(runValidation)` → the current versions,
classification, migration count (from the stamp events), replay- and snapshot-validation
status, posting-policy version, and a "historical guarantees satisfied" checklist.
Surfaced as an `acct.compat` startup line and a headless `ACCT_COMPAT_REPORT=<path>` dump.

---

## Required invariants (enforced)

- **Immutable historical meaning** — postings/statements are events, never recomputed;
  a change that would move a historical value fails §7 and refuses.
- **Replay equivalence** — compatible versions reconstruct byte-identical books (proven
  live == genesis rebuild).
- **Explicit compatibility** — every boundary is a declared version; nothing implicit.
- **Deterministic evolution** — bumping a version obligates a migration entry + an
  equivalence proof; a build cannot "accidentally" become compatible.
- **Snapshot subordination** — a snapshot is never authority; genesis always overrides.
- **Crash safety** — the manifest write is temp→fsync→rename (complete-or-absent); the
  authoritative stamp is a normal crash-safe event; an interrupted upgrade is never
  ambiguous.

## Validation (real runtime only — all green)

`ACCT_PTEST=suite`: manifest round-trip; corrupt-manifest CRC rejected; classify
compatible / newer→incompatible / older→migration-required / below-floor→incompatible /
unset→baseline; `EngineVersionStamp` adoption + idempotence + reopen; deep
replay-equivalence (genesis + snapshot + trial-balance + determinism); posting-policy
attribution + role-config-independent replay. Cross-process
`compat-crash@afterManifestTmp` (kill mid-write): the manifest is complete-or-absent and
the governance stamp survives in the log. End-to-end drills: `ACCT_COMPAT_VERIFY` passes;
`ACCT_COMPAT_REPORT` dumps all-satisfied; a corrupt manifest reopens by rebuilding; a
CRC-valid newer manifest **refuses to open (exit 3)** with a precise message.

## Explicit non-goals

No tax engines, ERP workflows, multicurrency, cloud sync, scripting, plugin systems, or
distributed replication. No configurable posting DSL. This phase preserves historical
accounting truth across decades of software evolution — nothing else.

## Honest limitations

- The semantic-migration registry ships **empty**; the first real cross-version migration
  is exercised only by synthetic classify fixtures, not a shipped policy change.
- The manifest governs cross-cutting **engine** versions; per-file record-layout migration
  stays owned by `BinaryRecordFile` (orthogonal, already proven).
- Migration **chains** are designed and unit-classified single-hop; N-hop chains and the
  first real accounting-semantic migration are the documented next increment.

## Critical design questions — answered

1. *What is replay compatibility?* per-axis Compatible / MigrationRequired / Incompatible
   (§1). 2. *Old events' policy?* retained — postings are events, never recomputed (§2).
   3. *Statement equivalence?* pure function of immutable postings; determinism gated (§3).
   4. *Snapshot evolution?* format-versioned, self-verifying, disposable (§4). 5. *Migration
   tiers?* binary / schema / accounting-semantic, kept distinct (§5). 6. *The contract?* an
   `EngineVersionStamp` event; the manifest is its crash-safe projection + gate (§6).
   7. *Proven how?* genesis + snapshot + historical replay equivalence, fail-loud (§7).
   8. *Operator visibility?* the compatibility report + `acct.compat` line (§8). 9.
   *Downgrade?* refused, loudly (mirrors existing refuse-newer). 10. *Corrupt governance
   metadata?* manifest rebuilt from the log; a torn manifest is complete-or-absent.

## Integration status & next increment

Live: `StorageService::initialize` adopts/stamps governance, classifies the on-disk
contract (max of log + manifest), refuses the incompatible, and writes the manifest
projection; `main_quick` logs `acct.compat` and honors `ACCT_COMPAT_VERIFY` /
`ACCT_COMPAT_REPORT`. Next: the first real accounting-semantic migration (posting-policy
V2 as the worked example) with its equivalence proof, N-hop migration chains, and
extending the manifest gate to the settlement/correction snapshot indices.
