# Security & Trust-Boundary Review

An **architectural trust-boundary audit** of the desktop application ahead of a first SMB
release — not a penetration test. The goal: find where malformed input, local corruption,
filesystem behavior, or UI misuse could compromise **correctness / durability / integrity /
confidentiality / availability**, document every finding, and implement **only the fixes that
are clearly warranted**. No accounting semantics, storage formats, or engine architecture were
changed; no cryptography, authentication, or encryption was added (see threat model).

## Threat model

**AccountingPro is a single-user, local, desktop application.** The realistic adversary is:

- **Malformed / corrupt on-disk artifacts** — a bit-rotted, truncated, partially-written, or
  deliberately-crafted data file (event log, record file, journal, snapshot, manifest), e.g.
  from disk failure, a bad backup/restore, or a synced file.
- **Absurd or hostile UI input** — enormous text, huge/negative/non-finite numbers, malformed
  dates, RTL/bidi edge cases, pathological filters.
- **Crash timing** — a process kill or power loss at any point in a write.

**Out of the threat model** (stated explicitly, per the review's rules):

- **No remote / network attacker** — the app has no network trust boundary.
- **No multi-user / privilege escalation** — the OS user owns the data directory; anyone who can
  plant files or symlinks inside it already controls the machine.
- **No confidentiality via cryptography** — data is protected by OS file permissions; adding
  encryption/authentication is explicitly out of scope for a single-user desktop tool. This is a
  deliberate design decision, not an oversight.

## Trust boundaries reviewed

| # | Boundary | What was reviewed | Verdict |
|---|----------|-------------------|---------|
| 1 | **Filesystem** | `EventLog`, `BinaryRecordFile`, ledger snapshot, `compat.manifest`, journals, temp files, atomic rename, locking, concurrent opens, symlinks, path traversal | Hardened; **F1**, **F2** fixed |
| 2 | **Input validation** | every editor ViewModel: string lengths, numeric overflow, negatives, UTF-8, RTL, dates, duplicate ids | Hardened; **F3** fixed |
| 3 | **Event integrity** | frame parsing, replay, CRC, snapshot + manifest loading, compatibility classification | Robust (CRC-framed, integrity-hashed, fail-loud); reinforced by F1 |
| 4 | **UI / ViewModel authority** | every write path from a ViewModel | Robust — no bypass |
| 5 | **Configuration** | data path resolution, settings, export/import, installer assumptions | Robust (no import surface; app-chosen paths) |
| 6 | **Denial-of-service** | enormous text, huge numbers, pathological filters, oversized histories, malformed files | Hardened; F1/F3 close the gaps |
| 7 | **Memory safety** | QObject ownership, model/VM lifetimes, context-property pointers, model indices | Sound |
| 8 | **Crash recovery** | every write path re-examined for partial commits / split authority / replay divergence | Impossible by construction |

### 1. Filesystem
`EventLog` is append-only and CRC-framed, with a two-step fsync'd commit (frame durable → commit
point advanced) and torn-tail truncation on open; committed frames are re-validated (CRC +
gap-free `seq`). `BinaryRecordFile` fronts every write with a CRC'd write-ahead journal that
covers the record **and** its `targetId`/`writeId`, uses a monotonic `writeId`, refuses a
newer-schema file, and enforces a nonzero record size (no divide-by-zero). Snapshot and manifest
rewrites, schema migration, and compaction all use the same discipline: **write temp → fflush →
fsync → atomic rename**, with crash-window recovery on open. Concurrency is guarded by a
stale-aware `QLockFile` with a clear "already running" message. All file paths are
`dataDir + "/<fixed>.dat"` — record fields (names, invoice numbers) are **never** used as
filenames, so there is no path-traversal surface. Two gaps found → **F1**, **F2**.

### 2. Input validation
Text fields serialize into fixed-size records via a bounded `strncpy` helper (oversized input is
truncated, never overflows). Dates go through `IsoDate::fromString` (validated `optional`). Ids
are engine-assigned and monotonic (never user-supplied → no duplicate-id corruption). The one
real gap was numeric: an out-of-range/non-finite amount reaching `Money::fromDouble` → **F3**.

### 3. Event integrity
Every on-disk parser is **total and fail-safe**: the manifest is a fixed 36-byte CRC-checked
read that returns `false` (→ rebuild from the authoritative `EngineVersionStamp` events) on any
defect; the snapshot verifies a magic + version + an integrity hash binding `seq`/`count`/entries
and falls back to a genesis replay on any mismatch (both already fuzzer-hardened); an unknown
event type in `apply()` is **refused loudly** (never silently dropped). Malformed input is
rejected loudly and never partially applied. F1 removes the last length-trust gap here.

### 4. UI / ViewModel authority
Every mutating ViewModel commit routes through `AuditJournal::record*` — the engine assigns ids,
enforces balanced postings, rejects closed-period edits, and authors events atomically. **No
ViewModel writes a repository directly** (repositories are disposable projections; `verifyAll`
proves live == replay). Editor validation is a **UX affordance**, not a second source of truth —
the authority is the engine, so nothing is "validated in two places" with a chance to disagree.

### 5. Configuration
The data directory is resolved by the app (a writable per-user location or an explicit
`ACCT_DATA_DIR`), not from untrusted runtime input. There is **no import path** (a would-be
trust boundary); backup is export-only (copies out). No privilege elevation, no world-writable
defaults, no reliance on the current working directory for data.

### 6. Denial-of-service
Filters use substring `contains` / `setFilterFixedString` — **no regex, no ReDoS**. Replay is
O(history) and snapshot-accelerated. Enormous text is truncated at the record boundary. The
allocation-from-untrusted-length vectors (**F1** event log, **F3** money) are closed; oversized
input is now bounded before it can allocate or overflow.

### 7. Memory safety
Models/ViewModels are owned as stack locals in `runQuickApp` that are declared **before** the
`QQmlApplicationEngine` and therefore destroyed **after** it — so the raw context-property
pointers the QML binds to never dangle during teardown (the same discipline the `LocaleController`
comment documents). Models emit `beginResetModel`/`endResetModel` around refreshes, so no stale
`QModelIndex` outlives its rows. Ownership is unambiguous (parented `QObject`s or stack lifetimes);
no manual `new`/`delete` churn in the hot paths.

### 8. Crash recovery
Re-examined against the existing `alloc-crash` / atomic-transaction / migration crash tests: a
crash between an event frame and its commit point truncates the frame (event **absent**, never
partial); a grouped `appendAtomic` is all-or-nothing (the whole business fact is present or
absent); the projection cursor is disposable, clamped to the log, and `reconcile()`-healed; the
record-file journal makes every append/update atomic. Partial commits, split authority
(operational without financial, or vice-versa), and replay divergence are impossible by
construction — and continuously proven by `ptest`/`fuzz` crash injection.

## Findings

| ID | Severity | Boundary | Finding | Status |
|----|----------|----------|---------|--------|
| **F1** | Medium | Filesystem / event integrity | `EventLog::openOrCreate` trusted the header's `committedLength` even when it exceeded the actual file size; `scanAndValidate` then sized `crcbuf(24 + len)` from an untrusted frame length bounded only by that inflated value → a ~60-byte crafted/corrupt log could force a multi-hundred-MB/GB allocation (local DoS / OOM) before the short read was caught. | **Fixed** |
| **F2** | Low | Filesystem | `BinaryRecordFile::replayJournal` wrote `recordSize_` bytes at `dataOffset(targetId)` with a CRC-valid but **unbounded** `targetId` → a crafted/corrupt journal could write at an arbitrary offset (sparse-file / disk-fill). | **Fixed** |
| **F3** | Medium | Input validation / overflow | `Money::fromDouble` did an unguarded `static_cast<int64_t>(rounded)`; a non-finite or out-of-range amount (e.g. field text `"1e400"` → `inf`, which passed the editors' `> 0` check) is an out-of-range double→int cast = **undefined behavior**. | **Fixed** |

**No critical or high-severity issue was found.** That is an evidence-based confidence result
from a real review of every boundary above — the write paths, parsers, and authority model were
already strongly hardened by prior work (CRC framing, integrity hashes, atomic rename, crash
injection, and adversarial fuzzing).

## Implemented fixes

- **F1** — `storage/EventLog.cpp`: on open, clamp `committedLength_` to the actual file size
  (`committedLength_ > actual → committedLength_ = actual; tornTail_ = true`). A legitimate commit
  point never exceeds the file, so valid logs are unaffected; a corrupt/crafted header now bounds
  every frame length to real bytes → the tail is rejected **loudly**, with no length-driven
  allocation.
- **F2** — `storage/BinaryRecordFile.cpp`: in `replayJournal`, after the CRC + `writeId` checks,
  discard a journal whose `targetId > count()` (a legitimate in-flight write targets `[0, count]`).
- **F3** — `core/Money.cpp`: a `saturateToInt64` helper clamps non-finite / out-of-range doubles
  to a **defined** result (INT64_MIN/MAX/0) in `fromDouble` + `scaledBy` — the last-line UB
  guard. At the UI boundary, the Payment/Expense editors and the invoice line model now reject
  non-finite or absurd magnitudes (a `$1e12` fence, far inside int64 cents) with a clear
  "Amount is too large." message.

All three are minimal and behavior-preserving for valid input (verified: `itest` 104, compat
gate, and byte-identical valid parsing all hold).

## Deferred / accepted risks (with justification)

- **Symlinks / paths inside the data directory** — accepted. The data dir is the OS user's own;
  planting a symlink or hostile file there requires the very access the threat model already
  grants to the legitimate user. All app paths are fixed; record fields are never filenames.
- **UTF-8 truncation at a fixed byte boundary** — accepted. The bounded `strncpy` can split a
  multibyte sequence at a field's capacity; on read, `QString::fromUtf8` renders the trailing
  partial byte as U+FFFD. This is a cosmetic truncation artifact for pathologically long names,
  never a crash or memory-safety issue.
- **Confidentiality of data at rest** — accepted by design. Out of scope by the review's rules
  (no crypto/auth on a single-user desktop app); the OS file-permission model is the protection.
- **A locally-crafted journal/log by the machine's own user** — the F1/F2 fixes harden against
  *corruption* and accidental/hostile crafted files; a user with write access to their own data
  dir can still delete or replace their books entirely. That is inherent to a local single-user
  tool and outside a meaningful trust boundary.

## Guarantees preserved (nothing regressed)

Event-authored authority (every write is an `AuditJournal` event; no repository is a write
authority) · deterministic replay + full-model replay-equivalence (`verifyAll` /
`ACCT_COMPAT_VERIFY`) · trial balance always 0 · immutable append-only correction/reversal
lineage · crash atomicity (all-or-nothing commits) · compatibility governance (no format or
event-type change). **No accounting semantics changed.** Production confidence increases through
evidence: three real gaps closed, every boundary reviewed, and a security regression suite that
fails if any fix is removed.

## Verification

- `tools/ptest.sh` — `security: bounded parsing` (F1 rejects an inflated `committedLength`;
  F2 discards a crafted out-of-range journal with no file growth; F3 `Money::fromDouble` is
  defined on `inf`/`nan`/`1e300`). **248 passed, 0 failed.**
- `tools/itest.sh` — the expense editor rejects non-finite / absurd amounts. **104 passed.**
- `tools/fuzz.sh` — the deterministic F1 inflation case joins the randomized event-frame corpus;
  the snapshot/manifest/record-file fuzzers are unchanged. **ROBUST.**
- `ACCT_COMPAT_VERIFY` — full-model replay-equivalence still holds (valid input is byte-identical
  through the fixes).
