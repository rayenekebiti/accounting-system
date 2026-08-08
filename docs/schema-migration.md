# Design Review: Schema Versioning & Forward Migration

Status: **implemented + verified** (`tools/ptest.sh` — 53 in-process assertions + 3
real cross-process migration-crash recoveries).

---

## 1. The highest remaining risk (and why it precedes every semantic feature)

The storage layer **cannot evolve its record layout without losing every existing
user's data.** `BinaryRecordFile::readHeader()` threw `"record size mismatch"` the
instant the code's record size differed from the file's, with no migration path for a
versioned file. The "upgrade safety verified" guarantee from the previous phase tested
a *binary swap with identical schema* — it never changed the on-disk layout.

The announced next phase (Suppliers / Products / Expenses) and the listed semantic
goals (immutable **audit history**, **period locking**, **append-only corrections**)
*all* add fields or record types. The first such change ships catastrophic,
silent-until-upgrade data loss to the installed base. Therefore migration safety is
not one option among the semantic features — it is the **precondition** for shipping
any of them. This was measured from the code (the throw at `readHeader`), not assumed.

Why it beats the alternatives *as the next step*:
- **Audit history / period locking / append-only corrections** are higher *product*
  value but each requires new fields/records → each needs migration first.
- They are also larger semantic designs; shipping them on an unevolvable store would
  mean either never changing them again or breaking users. Migration unblocks all.

## 2. Architectural invariants (enforced, not documented-only)

1. **Two independent versions.** `fileVersion` = container format (header + journal),
   rarely changes. `schemaVersion` = record *layout*, bumped on every field change.
2. **Forward-only & monotonic.** Migrations run `file.schema → code.schema`. A file
   **newer** than the code is **refused** (downgrade protection) — old code must never
   write a layout it doesn't understand.
3. **Append-only layout discipline.** New fields are appended; existing byte offsets
   never move. The default migration is therefore a pure **zero-extend**, needing zero
   per-entity code. A custom `MigrateFn` is the escape hatch for the rare reinterpret.
4. **Same schema ⇒ same size.** A size change without a `schemaVersion` bump throws
   loudly (a developer error caught at open, never a silent misread).
5. **Atomic & crash-safe.** A migration leaves the file **fully-old or fully-new** —
   never partial — across power loss at any instant.
6. **Lossless & validated.** Record count is preserved; a `MigrateFn` returning the
   wrong size aborts the migration with the original intact.
7. **Legacy compatibility.** Files predating versioning carry `schemaVersion == 0`,
   read as the baseline `1` — existing books open with no migration.

## 3. Failure modes & how each is contained

| Failure | Containment |
|---------|-------------|
| Power loss after temp written, before backup | original still at `path`; temp discarded; migration re-runs |
| Power loss after `original→.bak`, before `temp→path` | `path` absent + temp present → **finish forward** (install temp) |
| Power loss during `temp→path` rename | atomic rename ⇒ either temp or path exists; defensive `.bak→path` restore otherwise |
| Power loss after install, before `.bak` cleanup | `path` migrated + stale `.bak` → just delete `.bak` |
| `MigrateFn` throws / wrong size | abort before any rename; original untouched |
| File newer than code | refuse to open (no corruption) |
| Layout changed, schema not bumped | throw at open (developer error surfaced) |
| Corrupt header (size 0) | throw, don't guess |
| Leftover journal + pending migration | journal replayed at the OLD size **before** migration |

## 4. Lifecycle / data-flow rules (open path)

```
ctor:
  recoverInterruptedMigration()   # finish/rollback a crashed migration FIRST
  open + read header              # recordSize_ ← file's size; schemaOnDisk_ ← file
  replayJournal()                 # at the on-disk size (journal entries are old-sized)
  maybeMigrateSchema()            # refuse-newer | pass-through | migrate-forward
```

Migration steps (`performSchemaMigration`), each durable before the next:

```
S1  write fully-migrated records to <path>.migrating, fflush + fsync
S2  rename <path> → <path>.migrate.bak        (original safe)
S3  rename <path>.migrating → <path>          (new in place)
S4  delete <path>.migrate.bak                 (cleanup) ; re-open at new layout
```

`recoverInterruptedMigration()` is the inverse map from "which artifacts exist" to the
correct completion (see §3). This is the same write-ahead + recover-on-open discipline
the journal already earns trust with.

## 5. Implementation architecture (subsystem boundaries)

- **`BinaryRecordFile` (container)** owns versioning, the migration driver, atomicity,
  and crash recovery. It is **layout-agnostic** — it moves bytes.
- **The record owner (repository/entity)** owns *meaning*: it declares its
  `schemaVersion` + record size to the ctor and, only for non-additive changes,
  supplies a `MigrateFn(fromSchema, oldBytes, oldSize, newSize) → newBytes`.
- The default (null `MigrateFn`) zero-extends — so the common case (add a field) is
  **just**: append the field in the struct, bump that repo's `schemaVersion`, grow its
  record size. No migration code.

Backward compatible: the new ctor params default to `schemaVersion = 1, migrate = {}`,
so every existing repository (3-arg construction) is unchanged and behaves identically
(verified: itest 36/36, upgrade-test green).

## 6. Migration / deployment implications

- The **field-upgrade path is now safe**: shipping a build with a bumped entity schema
  migrates existing `.dat` files in place on first open, atomically, preserving the
  journal counter. Proven by the legacy-`0`→`1` test (the exact installed-base case).
- A user who runs an **older** build against newer data is protected (refusal, not
  corruption) — relevant if installs are rolled back.
- Migration runs **before any UI**, inside `StorageService::initialize`, so a partially
  migrated state is never presented.

## 7. Observability

- `BinaryRecordFile::migratedOnOpen()` / `schemaVersion()` expose what happened.
- Repositories surface `migrated()`; `main_quick` logs `acct.recovery: schema
  migration applied on open — invoices …` (one line, like crash recovery).
- A migration that aborts throws with a precise message (sizes, versions, path) →
  captured by the `acct.*` diagnostics layer.

## 8. Deterministic test strategy (all green)

In-process (`ACCT_PTEST=suite`): additive zero-extend (100 records, content preserved
+ tail zeroed), idempotent reopen, custom transform, **downgrade refused**, size-guard
throw, empty-file, **legacy-`0` baseline**. Cross-process real-crash
(`tools/ptest.sh`): kill the process at `afterMigrationTmp` / `afterMigrationBackup` /
`afterMigrationRename`; a fresh process recovers the books fully migrated every time.

## 9. Edge cases explicitly covered

Empty file; trailing partial record (count truncates, journal owns in-flight writes);
journal + migration ordering; preserved `lastWriteId_` across migration; same-size
schema bump; multi-version jump (single pass via `fromSchema`); single-instance lock
(`QLockFile`) prevents concurrent migration.

## 10. Recommended next step (now unblocked)

**Immutable audit history** — an append-only change log (entity, id, field, old→new,
timestamp, actor) is now safe to add: it is a new record type behind a `schemaVersion`,
and it is the foundation for period locking and append-only corrections. With
migration in place, that work no longer risks the installed base.
