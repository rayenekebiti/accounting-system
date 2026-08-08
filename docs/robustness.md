# Design Review: Deterministic Robustness, Fuzzing & Adversarial Validation v1

Status: **implemented + verified** (`tools/fuzz.sh` — structure-aware byte-mutation fuzzers
over every on-disk boundary + property-based invariants over randomized histories + real
cross-process fault injection; deterministic + seeded). This phase adds no features — it
proves the completed engine rejects-or-recovers under hostile inputs. **The fuzzer found and
we fixed a real defect** (snapshot integrity gap, below).

> Malformed inputs, corrupted storage, and interrupted execution must never produce silent
> corruption, undefined behavior, replay divergence, accounting inconsistency, or a crash
> without a diagnostic. This harness measures that under hostile conditions, not just normal
> operation.

---

## 1. Threat model / attack surface

The Quick product has **no network, RPC, or import/export** boundary (`Exporter` /
`InvoicePrinter` are Widgets-only). The untrusted surface is the **on-disk files** under the
data directory and **interrupted writes**:

| Boundary | File | Existing guard |
|----------|------|----------------|
| Authoritative history | `audit.log` (`EventLog`) | CRC per frame + gap-free seq + `payloadLen` bounded against `committedLength_` before allocation; header commit-point |
| Disposable projections | `*.dat` + `*.journal` (`BinaryRecordFile`) | magic/version/size-guard, refuse-newer, journal replay, atomic migration (~20 loud throws) |
| Replay accelerator | `*.ledgersnap` | magic + version + integrity hash → reject ⇒ genesis fallback |
| Version contract | `compat.manifest` | magic + fileFormat + CRC → reject ⇒ rebuild from log |
| Projection cursor | `audit.cursor` | bounded to the committed log on read |

The adversary is arbitrary corrupted bytes and a process killed at any write. Authority
(`audit.log`) is defended to **reject**; disposable artifacts are defended to **reject →
regenerate**.

## 2. Fuzzing framework (deterministic, seeded, reproducible)

`quick/fuzz.cpp` — a `splitmix64` PRNG + structure-agnostic byte mutators (bit-flip,
byte-set, truncate, grow, zero-run, duplicate-region). Each fuzzer builds a **valid**
artifact, applies 1–4 mutations, and asserts the boundary responds with exactly one of
**loud rejection** (throw / `false` / documented fallback) or **safe deterministic
recovery** — never a silent acceptance of corrupted committed state, never a fault without a
diagnostic. Every finding prints its seed (reproducible via `SEED=`).

- **EventLog frames** — reopen throws "corrupt committed history" or opens a valid gap-free
  prefix; `forEach` never exposes a malformed committed frame.
- **Ledger snapshot** — corrupt snapshot ⇒ `verifyLedgerSnapshot()==false` / `seq==0`, and
  `balanceUsingSnapshot == balanceAt` (the accelerator never returns a wrong balance).
- **Compat manifest** — `read()` is total; a successful read round-trips (no bad-CRC accept).
- **BinaryRecordFile** — deterministic rejection (throw) or journal-replay recovery; the
  record count stays file-bounded; no UB reading garbage records.
- **Compat classification** — thousands of random version vectors: `classify()` is total,
  deterministic, and order-correct (any-newer → Incompatible).

## 3. Property-based invariants

A seeded random **valid-history generator** issues random-but-valid `AuditJournal` ops
(customers, suppliers, atomic invoice+revenue, balanced entries, payments) into isolated
dirs; over many histories the invariants are asserted as *properties*:

- **P1** deleting every projection changes nothing (`rebuildProjections` → `verifyAll.ok`).
- **P2** `replay(replay(history)) == replay(history)`.
- **P3** snapshot + tail == genesis replay.
- **P4** trial balance always == 0.
- **P5** reconstruction is deterministic.
- **P6** every accepted stream → one deterministic verified ledger (`validateCompatibility`).

## 4. Fault injection (prove recovery, scoped to the persistence I own)

`storage/FaultInjection.h` (`acctFaultAt` + a one-shot armed variant) at **four** persistence
write points, each forcing the existing error path — used to prove **our recovery logic**,
not to emulate the OS:

| Fault point | Injured write | Recovery proven on reopen |
|-------------|---------------|---------------------------|
| `logCommit` | `EventLog::writeFileHeader` (commit point) | uncommitted group truncated → cleanly **absent** |
| `cursorWrite` | `AuditJournal::writeCursor` | cursor behind → `reconcile()` replays → **present** |
| `snapshotWrite` | snapshot install (rename) | snapshot absent → **genesis fallback** |
| `manifestWrite` | `CompatibilityManifest::write` | manifest rebuilt from `EngineVersionStamp` events |

A cross-process `faultwrite`/`faultverify` pair (shell-looped) proves that after any of
these, a fresh process reopens with `verifyAll.ok` + trial balance 0 + snapshot valid-or-
absent + governance intact.

## 5. Failure taxonomy (how each corruption class is contained)

| Corruption class | Boundary | Outcome |
|------------------|----------|---------|
| Bad CRC / flipped payload | EventLog / snapshot / manifest / BRF journal | loud reject (throw / `false`) |
| Truncated file / partial record | all | reject or valid-prefix / journal recovery |
| Corrupt length / count | EventLog (`payloadLen` vs `committedLength_`), snapshot (`count`), BRF | bounded → reject; no over-allocation |
| Corrupt seq (snapshot) | snapshot | **now** covered by the integrity hash → reject → genesis (see §7) |
| Newer/unknown version | manifest, BRF schema, snapshot ver | refuse (downgrade protection) |
| Interrupted write | commit point / cursor / snapshot / manifest | truncate / reconcile / regenerate |

## 6. Undefined-behavior audit (reviewed hotspots + findings)

- **Length arithmetic** — `EventLog` reads `payloadLen` (`uint32_t`) and bounds `off +
  kFrameHeader + len` against `committedLength_` **before** any `resize`/allocation
  (`scanAndValidate`); an oversized `len` throws, never over-allocates or reads OOB. 64-bit
  offsets, no overflow at realistic sizes.
- **Casts** — `count()` → id `uint32_t` casts are bounded by file size; snapshot `count` is
  used only to read entries and is validated by the integrity hash (post-fix).
- **Serialization** — `apply()` guards every payload with `>= RECORD_SIZE` before
  `deserialize`; a short payload throws.
- **Aliasing / alignment** — all field access is via `memcpy` through `char*` buffers; no
  unaligned typed reads → no alignment UB.
- **Money** — authority is `int64_t` cents; no floating point in the ledger/statement path.
- **Iterator lifetime** — projections are rebuilt, not mutated during iteration; scratch
  repos are separate files.

## 7. Finding & fix — snapshot integrity did not cover the header

The snapshot fuzzer detected `snapshot mis-accept` on multiple seeds. Root cause: the
snapshot's integrity hash (`hashBalances`, stored at offset 24) covered **only the balance
entries** — not the header's `seq` (offset 16) or `count` (offset 28). A byte mutation to
`seq` therefore passed the file-integrity check, and `balanceUsingSnapshot` (which trusts
file validity for speed, not a genesis match) accelerated from the **wrong seq** and returned
a **wrong balance**. The genesis path (`balanceAt`) was always correct, but the accelerator
could be fooled.

**Fix** (`storage/AuditJournal.cpp`): a new `hashSnapshot(seq, count, balances)` binds the
header into the integrity hash; on-disk snapshot format bumped **v1 → v2** (an old/foreign
snapshot is rejected → genesis-regenerated — snapshots are disposable, so no data loss). A
corrupted `seq`/`count` now invalidates the snapshot → genesis fallback, so the accelerator
can never trust a wrong seq. Verified: the fuzzer is clean at the coverage that found it, and
`tools/ptest.sh` snapshot tests stay green.

## Required invariants (satisfied)

Never silently accept malformed authoritative history (EventLog rejects loudly) · never
produce different replay results from identical input (P2/P5) · never recover by guessing
(reject or replay committed history; no synthesis) · always fail deterministically (seeded,
throw-based) · every detected corruption has an observable diagnostic (throws carry path +
reason; the harness prints seeds) · no malformed input compromises historical accounting
meaning (authority is CRC+seq-guarded; disposable artifacts fall back to genesis).

## Validation

`bash tools/fuzz.sh` (exit 0 = robust): the fuzz+property suite (every mutation → loud reject
or safe recovery, zero silent-accepts; P1–P6 hold) + the four fault points recovering
cross-process. Deterministic; `ITERS=`/`SEED=` control depth/reproduction (fast CI default,
deep override). `tools/ptest.sh` + `tools/itest.sh` stay green (the fault hooks are inert
without the env; the snapshot fix is format-consistent).

## Non-goals (per the brief)

No new accounting features, event types, storage layers, cloud sync, networking, or ERP. The
one production change is the snapshot integrity hardening — a fix for a fuzzer-found defect,
not a feature.

## Honest limitations

- Fault injection covers the **persistence write points we own** (to prove recovery), not
  transparent OS-level `malloc`/`read`/`open` interception (out of scope on Windows without
  heavy machinery).
- Fuzzing is **structure-aware byte mutation + randomized valid histories**, deterministic and
  seeded — not coverage-guided (libFuzzer/AFL). Coverage is boundaries × mutation classes ×
  iterations; the `ITERS` override drives deeper campaigns.
- A mutation that yields a valid-CRC-but-wrong artifact requires the attacker to also
  recompute the CRC — outside random mutation. The snapshot `seq` gap was reachable because
  the CRC did **not** cover `seq`; it now does. Other integrity hashes already cover their
  full payload.
- A mutation triggering a genuine process crash (vs. a caught throw) is surfaced as a failing
  run, not swallowed — the intended signal.
