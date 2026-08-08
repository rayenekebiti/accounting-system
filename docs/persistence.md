# Persistence, Integrity & Crash Safety

The promise of accounting software is that **data you saved is still there, and the
numbers still add up, after anything** — a crash, an app-kill, a power cut, a
half-written file. A pretty UI that loses or corrupts a single invoice is a failed
product. This document records the audit of the storage layer, the defects found,
the fixes, and the deterministic test suite that proves the guarantees hold.

Everything here is **measured, not assumed**. Run it yourself:

```bash
bash tools/ptest.sh          # in-process suite + real cross-process crash recovery
```

---

## 1. How persistence works (the protocol under audit)

`storage/BinaryRecordFile.cpp` is a fixed-record flat file with a 32-byte header
(`ACCTPRO\0`, version 2, record size, `lastWriteId`) followed by `id`-indexed
records. Repositories (`InvoiceRepository`, `CustomerRepository`, …) serialize
entities into records; money is stored as `int64` cents (`core/Money.h`) — never
`double`.

Durability rests on a **write-ahead journal**. Every `append`/`update`:

| Step | Action | Durability |
|------|--------|-----------|
| 1 | Write `[record][targetId][crc][writeId]` to `<file>.journal` | **fsync'd** (FILE\*/`_commit`) |
| 2 | Write the record into the main file at its offset | flush → OS cache |
| 3 | Advance `lastWriteId` in the header | flush → OS cache |
| 3b | **fsync the main file** (record + header) | **fsync'd** (new — see §3) |
| 4 | Delete the journal | — |

On open, `replayJournal()` reads any leftover journal, verifies the CRC, and applies
it iff `writeId > lastWriteId` (otherwise it's stale/already-committed). `writeId` is
a monotonic counter, so replay is **idempotent**: re-applying a committed write is a
no-op.

---

## 2. Audit findings

> Premise: *do not assume the journal is correct — pressure-test it.* It was mostly
> right, with three real defects.

| # | Finding | Severity | Status |
|---|---------|----------|--------|
| F1 | **Journal CRC covered only the record, not `targetId`/`writeId`.** A corrupted target offset would replay the record to the **wrong slot** undetected — silent cross-record corruption. | High | **Fixed** |
| F2 | **No `fsync` on the main file.** The journal was durable but the record + header were only flushed to OS cache before the journal was deleted. App-kill was safe (cache survives); a **power cut** between the journal delete and the cache flush could lose a just-confirmed write. | High | **Fixed** (§3) |
| F3 | **No invoice-number uniqueness.** Two invoices could share a number, making them indistinguishable in reports/reconciliation. | Med | **Fixed** |
| F4 | **Invoice header totals were rounded from aggregate doubles** (`fromDouble(Σsub)`, `fromDouble(Σtax)`, `fromDouble(Σtotal)` independently), while line totals round per line. Result: `total ≠ Σ line totals` and `subtotal + tax ≠ total` could drift by a cent. | High (financial) | **Fixed** |
| — | Partial/torn main-file writes are correctly ignored by `count()` (size-derived) and rebuilt from the journal. | — | Correct as-found |
| — | Headerless v0/v1 files migrate in place via a temp file + atomic rename. | — | Correct as-found |

### F4 in detail — the rounding drift

Three lines of `1 × $0.10 @ 25%`: each line is `0.125 → $0.13` (half-away-from-zero),
so the lines sum to **$0.39**. The old aggregate path computed `0.375 → $0.38` — a
**1-cent discrepancy between the invoice and its own lines.** Fix: a single source of
truth, `core/InvoiceTotals.h::computeInvoiceTotals()`, derives every header figure by
summing the already-rounded per-line `Money`, so both invariants hold *by
construction* with exact integer-cent arithmetic. `commit()` now uses it.

---

## 3. The fsync barrier (F2)

`fstream` exposes no file descriptor, so `BinaryRecordFile::syncToDisk()` opens a
second handle to the same path and `fsync`s it — the OS page cache is per-file, so
flushing any handle flushes the file's dirty pages. It runs **after** the header
commit and **before** the journal is deleted, closing the power-loss window: once the
journal is gone, the data is already durable.

It's **best-effort**: if the durable handle can't be opened it degrades to the stream
flush (still crash-consistent — the journal protects against torn records — but the
last write may not survive an abrupt cut). The suite proves it is *not* degraded on
this platform: the `concurrent rb+ handle opens` assertion confirms the barrier is
live.

**Honest scope of what's proven:**

- **App-kill (process death):** proven durable — the cross-process test below kills
  the writer with `std::_Exit` at every write step and recovers the data every time.
- **Power-loss:** strengthened by the fsync barrier. The residual assumption is that
  the 32-byte header write is atomic within a disk sector (`lastWriteId` lives at a
  fixed offset well inside the first sector) — true on mainstream hardware. A torn
  header would be caught on the next open by the version/size checks, not silently
  trusted.

---

## 4. Test suite

`quick/ptest.cpp`, driven by `tools/ptest.sh`. Deterministic, isolated (own scratch
dir under `ACCT_DATA_DIR`), reproducible, no mocks — it drives the **real**
`BinaryRecordFile` and crafts journals with the **same** `crc32` production uses.

### Layer 1 — in-process suite (`ACCT_PTEST=suite`), 38 assertions

| Group | What it proves |
|-------|----------------|
| Round-trip | 200 records survive save → close → reopen byte-for-byte |
| Journal replay (valid) | a valid leftover journal is applied; flag set; journal cleared |
| Corrupt CRC | a bad-CRC journal is ignored; original record preserved |
| Stale journal | `writeId ≤ lastWriteId` (the `afterHeader` case) is **not** re-applied — idempotency |
| Torn journal | a truncated journal is rejected without crashing |
| Partial main write | a half-written record + valid journal is **completed** on reopen |
| Integrity totals | `total == Σ lines` and `subtotal + tax == total`, exactly; the old aggregate path is shown drifting to 38¢ |
| Money determinism | exact cents; `0.1 + 0.2 == 0.3`; repeated builds identical |
| Duplicate number | `findIdByNumber` detects the first holder; the `commit()` guard relies on it |
| Durable-sync handle | the fsync barrier's second handle actually opens (not degraded) |
| Stability | 200 independent open/append/close cycles → 200 records intact |

### Layer 2 — real cross-process crash recovery

A child process appends a sentinel and is **hard-killed** (`std::_Exit(99)`) at a
crash point injected via `ACCT_CRASH_POINT`; a fresh process then reopens and must
find the sentinel. This is the only honest test of write-ordering across an actual
process death — no in-process mock can prove it.

| Crash point | State at death | Recovery |
|-------------|----------------|----------|
| `afterJournal` | journal durable, main file untouched | replay restores the record |
| `afterMainWrite` | record written, header not committed | replay re-applies (idempotent) |
| `afterHeader` | committed, journal not yet deleted | data intact; stale journal dropped |

A **clean-write baseline** (no crash) also writes and verifies, proving the test isn't
trivially passing because every reopen "recovers."

### Results

```
38 passed, 0 failed                    # in-process suite
[PASS] crash@afterJournal   : writer killed (99), sentinel recovered
[PASS] crash@afterMainWrite : writer killed (99), sentinel recovered
[PASS] crash@afterHeader    : writer killed (99), sentinel recovered
[PASS] clean write completes (0) and verifies (0)
== ALL PERSISTENCE TESTS PASSED ==
```

---

## 5. Logging & diagnosability

`quick/logging.h` defines four structured categories, quiet in steady state:

| Category | Default | Use |
|----------|---------|-----|
| `acct.storage` | warning | open/close, paths, init failures |
| `acct.persistence` | warning | save/update errors (the data-loss-critical path) |
| `acct.recovery` | info | journal replay reported at startup |
| `acct.integrity` | info | accounting-invariant failures |

Startup **recovery reporting** is wired in `main_quick.cpp`: after init, if any
repository replayed a journal (`recoveredOnOpen()`), it logs one `acct.recovery` line.
Enable selectively without code changes:

```bash
QT_LOGGING_RULES="acct.recovery=true;acct.persistence=true" ./AccountingQuick.exe
```

---

## 6. Recommendations / future work

1. **Multi-record transactions.** An invoice + its lines are saved as *separate*
   single-record writes. A crash between them can leave an invoice with missing
   lines. Each write is individually crash-safe, but the *set* is not atomic. A
   batch-journal (one journal spanning N records, applied all-or-nothing) would make
   invoice+lines transactional. — *largest remaining gap.*
2. **Soft-delete + duplicate numbers.** `findIdByNumber` ignores soft-deleted rows,
   so a number freed by deletion can be reused. Confirm that matches the product rule.
3. **Backup/restore validation.** Fold `BackupService` into this harness: back up a
   seeded set, restore into a fresh dir, assert byte-identical + invariants hold.
4. **Periodic integrity sweep.** A background check asserting `total == Σ lines` and
   balances reconcile across the whole book, surfacing `acct.integrity` on drift.
