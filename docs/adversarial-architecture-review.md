# Adversarial Architecture, Storage & Accounting Audit — Phase C6

**Auditor stance:** Independent principal engineer + accounting-systems auditor. I did not build
Occountant. My mandate is to find reasons it should *not* be trusted with real business books, not
to confirm prior phases. Every prior "green gate" was treated as unproven until re-derived from the
source.

**Scope reviewed (by reading, not by trusting docs):** `storage/EventLog.{h,cpp}`,
`storage/AuditJournal.{h,cpp}` (~2.9k LOC), `storage/StorageService.h`, `storage/PostingPolicy.h`,
`storage/EventTypes.h`, the shipping Qt Quick view-models under `quick/`
(`PaymentEditorViewModel`, `PaymentAllocationViewModel`, `InvoiceEditorViewModel`,
`CustomerListModel`), and the commercial layer under `app/` (`Signature`, `UpdateManager`,
`BackupScheduler`).

---

## 1. Executive verdict

### ⛔ NOT READY for real business financial data.

This is **not** a verdict on the event-sourcing *foundation*, which is genuinely good: the append-only
CRC-framed log with a single commit-point (`committedLength`), atomic multi-frame groups, torn-tail
truncation, cursor-based projection recovery, refuse-newer governance, and disposable-projection
verification are all real, correctly-reasoned, and well-tested **in isolation**.

The verdict is driven by **where the money actually flows in the shipping app**. The audited engine
is sound; the *product wired on top of it* contains a **Critical, live, silent ledger-correctness
defect**: customer payments never reach the general ledger, and three different screens compute three
different, mutually-inconsistent answers to "what does this customer owe / how much cash do we have."
The system's headline correctness proof — *trial balance always sums to zero* — stays green the entire
time, because every individual entry is balanced. **A balanced-but-wrong ledger is exactly the failure
mode a trial-balance invariant cannot see, and this system is in that state the moment the first
invoice is paid.**

Confidence that the storage engine will not *lose or corrupt* committed events (on Windows): **high**.
Confidence that the numbers the app *shows the owner* are correct: **low**.

---

## 2. Critical findings

Severity legend: **Critical** = wrong financial numbers or data loss in normal use · **High** =
realistic path to wrong numbers, data loss, or compromise · **Medium** = correctness/scalability risk
under plausible conditions · **Low** = latent / defense-in-depth.

---

### 🔴 C1 — Customer payments never post to the general ledger (Critical, realistic, LIVE)

**Finding.** In the shipping Qt Quick app, receiving a customer payment updates only the *settlement
index*; it produces **no ledger posting**. Cash is never debited and Accounts Receivable is never
credited by a collection. The general ledger, balance sheet, income statement (cash effects), and tax
summary are therefore disconnected from every dollar a customer actually pays.

**Evidence.**
- `quick/PaymentEditorViewModel.cpp:39-68` — `commit()` calls **only**
  `storage.audit().recordPayment(...)`.
- `storage/AuditJournal.cpp:768-781` — `recordPayment()` appends a `PaymentRecorded` event and calls
  `rebuildSettlementIndex()`. It never calls `recordJournalEntry`/`postPaymentReceipt`; `apply()`
  (`AuditJournal.cpp:207-213`) explicitly treats `PaymentRecorded`/`PaymentAllocated` as
  ledger-*neutral*.
- `postPaymentReceipt()` (the correct `Dr Cash / Cr AR` posting, `AuditJournal.cpp:1317-1326`) has
  **no caller anywhere except tests** (`quick/ptest.cpp`, `quick/fuzz.cpp`).
- Net effect on the ledger: AR is only ever *debited* (invoices, `InvoiceEditorViewModel.cpp:276`),
  never credited; Cash is only ever *credited* (cash expenses, `expenseTaxed` in `PostingPolicy.h`),
  never debited. So on the balance sheet **AR = Σ all posted invoices forever**, and
  **Cash = −(cash-basis expenses)** — a negative cash balance that bears no relationship to the bank.

**Why existing tests missed it.** `ptest.cpp` builds its ledger scenarios by calling **both**
`postInvoiceRevenue` **and** `postPaymentReceipt` by hand (e.g. `ptest.cpp:777-778`), so the ledger
suite exercises a Cash/AR settlement flow that the real UI never triggers. There is no end-to-end test
that drives a payment through the *same API the UI uses* (`recordPayment`) and then asserts that Cash
rose and AR fell. The trial-balance-zero gate cannot catch it because every entry that *does* post is
individually balanced. The verification gates (`verify`, `validateCompatibility`) compare the live
projection to a replay of the *same* events — they prove the projection faithfully reflects history,
not that history reflects reality.

**Customer impact.** Every balance sheet and cash figure is materially wrong as soon as any invoice is
paid. An SMB owner reconciling to their bank statement will find "Cash" on the books is negative and
unrelated to their actual balance; AR aging shows customers owing money they have already paid (in the
ledger view). Cash-basis tax would be computed on the wrong base. This single defect makes the ledger
untrustworthy for filing or lending.

**Mitigation.** Route `recordPayment` (and `allocatePayment`, for the AR-clearing leg) through the
posting policy so a receipt authors its `Dr Cash / Cr AR` `JournalEntryPosted` **atomically** with the
`PaymentRecorded` event via `appendAtomic` — exactly as `recordInvoiceWithRevenue` already does for
invoices (`AuditJournal.cpp:1370-1436`). Add an end-to-end invariant test: *after any UI-level payment,
`balanceFor(Cash)` and `balanceFor(AR)` change by the posted amount, and settlement-derived AR equals
ledger AR.*

---

### 🔴 C2 — Three parallel, unreconciled notions of "customer balance" (Critical, realistic, LIVE)

**Finding.** "How much does customer X owe / how much have they paid" is computed **three different
ways** by three different screens, and no two agree once a payment exists. There is no reconciliation
invariant binding them.

1. **Ledger AR** (balance sheet / `balanceFor(receivable)`): invoices only; never reduced by payments
   (see C1).
2. **Settlement engine** (`AuditJournal::outstandingFor`, Payments/allocation UI): `invoice.total −
   Σ allocations`. This is the *only* view that reflects real payments — but it is invisible to the
   ledger and the statements.
3. **Customers-screen aggregate** (`StorageService::computeCustomerAggregates`,
   `StorageService.h:390-416`, consumed by `quick/CustomerListModel.cpp:62`): `startingBalance +
   Σ invoice totals − Σ payments from the payments **repository**`. But in the Quick app **nothing
   ever writes the payments repository** — payments go through `audit().recordPayment`
   (`PaymentEditorViewModel`), while `payments().save()` is only called by the **legacy Widgets**
   `src/ui/pages/payments/PaymentsPage.cpp:156`. So `payments_->loadAll()` is empty and the subtraction
   is a no-op: **the Customers list shows balances that never go down when a customer pays.**

**Evidence.** `StorageService.h:409-413` subtracts `payments_->loadAll()`; grep confirms the only
writers of that repo are in the superseded `src/ui` Widgets tree, not `quick/`.

**Why existing tests missed it.** `computeCustomerAggregates` is unit-tested (`bench.cpp:91`) and the
settlement engine is unit-tested (`ptest.cpp`), but each is tested against its *own* inputs. No test
asserts *cross-subsystem equality* ("Customers-screen balance == settlement outstanding == ledger AR
per customer"). The subsystems were built and verified independently and never reconciled.

**Customer impact.** The owner sees contradictory receivables on the Customers screen, the Payments
screen, and any GL/aging report, with no indication which is authoritative. Dunning, credit decisions,
and period-end AR will all be wrong.

**Mitigation.** Pick one authority (the ledger, fed by C1's payment postings) and *derive* the other
two from it. Delete the dead `payments.dat` subtraction path or make `recordPayment` also project into
that repo. Add a reconciliation self-check surfaced in Startup Diagnostics.

---

### 🟠 H1 — Update packages are authenticated by a symmetric key embedded in the binary (High)

**Finding.** License **and update** signatures use HMAC-SHA256 with a **symmetric key hard-coded in the
shipped binary**. The comment correctly accepts this for license *DRM* ("honest-user licensing"), but
the **same key is the trust root for software updates** — which is a code-execution boundary, not a
DRM one.

**Evidence.** `app/Signature.cpp:11-17` — `kVendorKey` is a plaintext `QByteArrayLiteral` signing both
"license+update". `app/UpdateManager.cpp:106,164` — update payloads are accepted iff
`sig::verify(payload, sigHex)` passes, i.e. against that same extractable key.

**Why existing tests missed it.** Signature tests verify that a *correctly-signed* package is accepted
and a *tampered* one is rejected — which is true. They do not model the actual threat: an attacker who
runs `strings` on the `.exe`, extracts the key, and signs their **own** malicious update. Functional
tests can't fail on "the key is where the attacker can read it."

**Customer impact.** Anyone who extracts the key (trivial — it is a literal string in the binary) can
forge an update package that `UpdateManager` will treat as authentic and hand to the installer, which
"replaces the binary and relaunches" (`UpdateManager.cpp:173-175`). That is remote-ish code execution /
supply-chain compromise on every customer machine, gated only by delivery. License forgery is the
*accepted* risk here; **forgeable auto-update is not.**

**Mitigation.** Split the trust roots. Sign updates with an **asymmetric** key (Ed25519/RSA); ship only
the public half. Keep the symmetric HMAC for license tamper-detection if desired. Pin the update signer
independently of licensing.

---

### 🟠 H2 — The commit pointer and file header have no integrity check; corruption is either total lock-out or *silent truncation of committed history* (High)

**Finding.** The 32-byte `EventLog` file header — including the 8-byte `committedLength` that *defines*
what is committed — carries **no CRC**. Only individual frames are CRC-protected. A single bit-flip in
`committedLength` is undetectable and has two outcomes, both bad:
- flip it **larger** than the file → clamps and reports a torn tail loudly (acceptable); or
- flip it **smaller** to a value that lands on an earlier frame boundary → `openOrCreate()`
  **physically truncates the file to that offset** (`EventLog.cpp:82-86, 120-123`) and opens
  "successfully," **silently discarding every later committed event.**

Separately, a bit-flip *inside* a committed frame fails CRC in `scanAndValidate()`
(`EventLog.cpp:162,170-173`), which `throw`s — and there is **no repair / partial-recovery tool**. One
corrupt byte in event #3 of two million renders the entire company history un-openable
(`StorageService::initialize` catches the throw and returns `false`, `StorageService.h:218-222`).

**Evidence.** `EventLog.cpp:57-80` writes the header with no checksum; `EventLog.cpp:88-135` trusts
`committedLength` (only sanity-clamping against physical size) and truncates on any excess;
`scanAndValidate` (`EventLog.cpp:137-176`) is all-or-nothing.

**Why existing tests missed it.** Crash-injection tests target the *designed* windows
(`afterEventFrame`, `afterEventCommit`, `logCommit`) where the header is written correctly. They inject
crashes, not **bit-rot** — a flipped bit in an already-committed region or in the header length field is
outside the fault model. The tests prove the *protocol* is crash-atomic; they don't prove the *bytes*
survive a decade on disk.

**Customer impact.** After years of accumulated history, a single latent media error either locks the
owner out of all their books with no built-in recovery, or (worse) silently amputates recent history
while appearing healthy. Because backups are byte copies (see L1), an old corruption can propagate into
the backup rotation before it is noticed.

**Mitigation.** CRC (or keyed MAC) the header, including `committedLength`, and refuse to truncate on a
header whose checksum fails. Provide an offline `fsck`-style tool that recovers the longest valid
gap-free prefix and quarantines the tail, instead of throwing. Consider periodic full-log verification
surfaced to the user.

---

### 🟠 H3 — Durability guarantee does not hold on macOS (`fsync` ≠ `F_FULLFSYNC`) (High)

**Finding.** The commit-point design depends on the frame being *physically* durable before
`committedLength` advances. On non-Windows the durability primitive is `::fsync`
(`EventLog.cpp:15-19`, `AuditJournal.cpp:31-34`). On **macOS — a shipping target platform** — `fsync()`
flushes to the drive but **not through the drive's write cache**; Apple requires `fcntl(fd,
F_FULLFSYNC)` for true power-loss durability. No `F_FULLFSYNC` exists anywhere in the tree (grep
confirmed).

**Evidence.** `EventLog.cpp:17` `#define EVTLOG_FSYNC(fd) ::fsync(fd)`; no `F_FULLFSYNC`/`fcntl` in
`storage/` or `app/`.

**Why existing tests missed it.** Crash tests use `std::_Exit(99)` to simulate a crash — a *process*
death, which preserves the page cache and the drive cache. They never model **power loss with a
populated drive write-cache**, which is the only scenario where the macOS `fsync` weakness manifests.
The test harness cannot pull the plug.

**Customer impact.** On a Mac, a power failure in the seconds after "Save" can lose events the app
already acknowledged as committed history — the exact guarantee the product markets. Also note there is
no parent-directory `fsync` after the crash-safe `rename` of snapshot/manifest temps
(`AuditJournal.cpp:1729-1731`), a secondary POSIX durability gap.

**Mitigation.** Use `F_FULLFSYNC` on macOS for the log frame + header writes and the cursor; `fsync`
the containing directory after `rename`. Gate the "crash-safe" marketing claim on the platform where
it actually holds.

---

## 3. Accounting correctness review ("balanced but wrong")

I attempted to construct books that are internally balanced yet financially false. It was easy.

| Scenario | Result | Notes |
|---|---|---|
| **Invoice → revenue** | ✅ Correct | `recordInvoiceWithRevenue` posts `Dr AR / Cr Revenue / Cr Tax` atomically; totals derived per-line in int64 cents (`InvoiceEditorViewModel.cpp:249-256`), no aggregate-double rounding. |
| **Invoice correction / void via editor** | ✅ Correct | Status→VOID recognises 0, posts `new − old` delta (`InvoiceEditorViewModel.cpp:258-277`). |
| **Expense + void + reversal** | ✅ Correct | Posts compensating balanced entries (`recordExpenseWithPosting`/`recordExpenseVoided`, `AuditJournal.cpp:1442-1591`). |
| **Customer payment received** | ❌ **Wrong (C1)** | No ledger effect. Cash/AR never move. |
| **Customer overpayment / prepayment (credit)** | ❌ **Wrong** | `creditForCustomer` tracks unallocated cash in the settlement index, but the ledger has **no customer-credit / unearned-revenue liability** — the money exists in neither Cash nor a liability. Invisible to the balance sheet. |
| **Aging / "who owes what"** | ❌ **Inconsistent (C2)** | Three answers, none reconciled. |
| **Tax rate change over time** | ✅ Sound design | Tax *amount* is frozen in the immutable posting; a rate change is a new `TaxCodeCreated` version of the family (`EventTypes.h:38-41`, `resolveRateAt`). Historical postings never reinterpreted. |
| **Period close freezing** | ✅ Sound | Close/reopen are append-only events; `recordJournalEntry` refuses closed-period dates (`AuditJournal.cpp:1118-1119`); reversals are the sanctioned post-close path. |
| **Balance-sheet "balances" flag** | ⚠️ Misleading | `balanceSheetAt` sets `balances = (assets == liabilities + equity)` (`AuditJournal.cpp:1243`). Because the trial balance is *structurally* 0, this flag is **always true by construction** — it certifies internal consistency, not correctness, and will read "balanced" while C1 makes it wrong. |

**Positive:** money is int64 cents end-to-end; the double-entry authoring gate (`Σ postings == 0`,
`AuditJournal.cpp:1114-1117`) genuinely makes an unbalanced entry impossible to persist; per-line tax
rounding is correct. The accounting *primitives* are trustworthy. The **integration** is not.

---

## 4. Storage review

**EventLog (append-only log).**
- ✅ Commit protocol (write frame → fsync → advance `committedLength` → fsync) is correct and the
  atomic-group path (`appendAtomic`) is a genuine single commit point.
- ✅ Torn/uncommitted tail truncation is correct; oversized-`payloadLen` is bounded to physical file
  size before allocation (`EventLog.cpp:124-132`) — no OOM from a crafted length.
- ❌ Header/`committedLength` unprotected → **H2** (silent truncation / total lock-out, no repair tool).
- ⚠️ **No cryptographic integrity (M1).** Frames use **CRC-32**, which is *not* a security primitive.
  Anyone with write access to `audit.log` can insert/modify/delete events, recompute the linear CRC-32s
  and `committedLength`, and the engine will accept the result as authoritative, immutable history.
  There is **no HMAC, no hash chain, no signature** (grep confirmed). "Immutable authoritative history"
  holds against *accidental* corruption, **not against tampering** — a strong claim for an *audit*
  journal that real audits would reject. A "maliciously crafted valid CRC" is trivial.

**Snapshot system.**
- ✅ Design is correct: `temp → fsync → rename`, disposable, self-verifying against a genesis replay,
  integrity hash now binds `seq + count` (the v2 fix after the fuzzer found a trusted-bad-seq bug,
  `AuditJournal.cpp:1648-1661`). A missing/corrupt/stale snapshot correctly falls back to genesis.
- ❌ **Unwired in production (M2).** `writeLedgerSnapshot` has **no non-test caller** (grep: only
  `ptest`/`fuzz`/`perf`). The shipping app never writes a snapshot, so `balanceUsingSnapshot` always
  hits the genesis-fallback branch. The entire acceleration path — and the crash-safety testing around
  it — protects a feature the product does not use. Every historical/as-of query is full-history replay.
- Can the app show a wrong number while believing the snapshot valid? Not via the snapshot (the
  seq-binding fix closed that) — but it *can* show a wrong number for the more basic reason in C1, which
  no snapshot check covers.

**Projection system.**
- ✅ Rebuild correctness is sound: projections are pure folds over the log; `apply()` is idempotent
  upsert-at-id; the cursor + `reconcile()` heal any crash between event and projection; `verify` /
  `verifyAll` rebuild into scratch repos and byte-compare. Projection *drift* is genuinely detectable.
- ⚠️ Detection is not *automatic* in normal operation — `verify` is on-demand. A projection silently
  corrupted by an unrelated bug would not be noticed until someone runs the gate.

---

## 5. Event-sourcing review

**Are events true business facts or implementation details?** Mostly facts (InvoiceCreated,
PaymentRecorded, JournalEntryPosted, PeriodClosed). But **stable ids are assigned from projection
cardinality** — `id = accounts_.size()` / `entries_.size()` / `payments_.size()`
(`AuditJournal.cpp:770,791,1096,1110`). The identity of an authoritative fact therefore depends on the
*current size of a rebuilt in-memory index*, not on anything in the event itself. This is deterministic
today, but it is an implementation detail leaking into the authoritative record (see M4/future traps).

**Is important information missing from events?** Yes in one place that matters: the per-frame `schema`
field is written but **never dispatched on at replay** — `apply()` and all `rebuild*` scans switch on
`type` only and validate payloads with `size() >= FIXED_RECORD_SIZE` (`AuditJournal.cpp:145-226`).
`EventTypes.h:6-9` promises "a payload can evolve independently, migrated at replay time," but there is
no schema switch to do that migration. A future record-layout change that keeps size `>=` the old size
will be **silently misread** by current code. The evolution seam is documented but not implemented.

**Could two histories create the same state?** Yes (e.g. create-then-update vs create-with-final-value
fold to the same projection) — expected and harmless.

**Could one history create a *different* state after a code change?** Yes — balances and statements are
**derived by code at read time** (`incomeStatementAt`, `balanceSheetAt`, sign conventions). The account
*type* is safely frozen in `AccountOpened` events, and the governance axes (`statement`, `postingPolicy`)
plus the refuse-newer gate (`apply()` default-throws on unknown types, `AuditJournal.cpp:220-224`) are a
credible defense. But the guarantee is only as strong as the discipline of bumping a governance axis on
every semantic change — the v1 semantic-migration registry is empty (`StorageService.h:83-86`), so the
first real migration is untested in anger.

**Hidden migration trap:** the unused `schema` field (above) + size-`>=` payload checks mean the *first*
time someone appends a field to, say, the Customer record, old builds will accept the longer payload and
misinterpret it rather than refuse. That is the dangerous direction (silent), not the safe one (loud).

---

## 6. Scalability review

Evidence base is "1M events tested." The architecture has structural ceilings well below the 10M–100M
targets:

- **Startup is ≥7 full log scans.** The `AuditJournal` constructor runs `readCursor` + **seven**
  independent `log_.forEach` passes (`rebuildPeriodIndex`, `rebuildCorrectionIndex`,
  `rebuildExpenseCorrectionIndex`, `rebuildSettlementIndex`, `rebuildLedgerIndex`, `rebuildTaxIndex`,
  `rebuildGovernanceIndex`, `AuditJournal.cpp:59-67`), then `StorageService::initialize` runs
  `reconcile` + backfills. `forEachAfter` reads **every frame's full payload from offset 32 regardless
  of `afterSeq`** (`EventLog.cpp:265-289`) — so even "catch-up" is O(whole log) I/O, and the cursor
  buys nothing at the read layer. At 100M events (multi-GB) this is minutes of disk I/O on **every
  launch**.
- **Unbounded in-memory indices.** Every account, journal entry (with its full postings vector), payment,
  allocation, tax code, closed period, correction, and governance stamp lives in a `std::map` **forever**
  (`AuditJournal.h:387-410`). Memory is O(all entities ever created), not O(working set). A POS or
  high-volume shop will exhaust RAM long before 100M events.
- **No log compaction / no snapshot of indices.** The only snapshot is ledger *balances*, and it is
  unwired (M2). Nothing ever shrinks the replay cost. History grows monotonically with no truncation
  horizon.
- **Every "as-of" query is a full scan.** `balanceAt`, `incomeStatementAt`, `balanceSheetAt`,
  `settledAt`, and `taxSummaryAt` (which calls `balanceAt` twice) each `forEach` the whole log
  (`AuditJournal.cpp:1160-1258`). A trial balance "as of" over N accounts with per-account calls is
  O(N · history).

**Verdict:** comfortable to ~1M events; degraded but usable in the low millions; **not viable at 10M+**
without index snapshots and a bounded-replay startup. Multi-company datasets multiply this (below).

---

## 7. Future POS / ERP / multi-company impact

A future engineer adding the roadmap features hits the architecture head-on:

- **Multi-company.** `StorageService` is a **singleton** owning one data dir and one `AuditJournal`
  (`StorageService.h:89-93, 102-137`); ids are assigned from **global** index cardinality. There is no
  company/tenant dimension in the event, the id, or the service. Two companies cannot be open at once,
  and ids collide across datasets. Retrofitting requires either N processes or a tenant key threaded
  through every event, id, and index — a rewrite of the identity model.
- **Cloud sync (a stated product direction).** The log is a **single-writer, gap-free `seq`** stream
  with a physical `committedLength` pointer. It is fundamentally **not mergeable**: two machines that
  both append while offline produce colliding `seq` values and divergent `committedLength`s with no
  vector clock, no conflict resolution, and no rebase. `QLockFile` (M4) does not prevent cross-host
  concurrency on a synced folder. "Local + cloud sync" is not supported by this log design; it needs a
  per-replica event id space + a merge/CRDT or a server-authoritative sequencer.
- **POS.** High event rate collides directly with the 7-scan startup, unbounded RAM indices, and
  full-scan reads (Section 6).
- **Payroll / inventory.** Feasible as new event types (the type registry + refuse-newer is a good
  seam), *provided* the schema-dispatch gap (Section 5) is fixed first — otherwise the first payload
  evolution silently misreads.

---

### 🟡 M4 — Single-writer assumption is enforced only by `QLockFile` (Medium)

`EventLog` assumes a single writer (`EventLog.h:47`). Enforcement is a `QLockFile` in the data dir
(`StorageService.h:107-115`). On a **network share or cloud-synced folder** — precisely the SMB /
local+cloud persona the product targets — `QLockFile`'s hostname+PID staleness model is unreliable, and
two hosts can both acquire and append. Interleaved appends past the same `committedLength` corrupt the
log. **Why tests missed it:** all tests are single-process on a local FS. **Mitigation:** detect
non-local/synced data dirs and refuse, or move to a real cross-host lock / server sequencer before
shipping cloud sync.

---

## 8. Accepted risks (reasonable as-is, documented here for completeness)

- **License forgery via symmetric key** — explicitly accepted for a v1 desktop product
  (`Signature.cpp:8-10`). Fine **for licensing**; not fine for updates (H1).
- **Refuse-newer / refuse-unmigratable** — opening books written by a newer build, or needing an
  unregistered semantic migration, hard-refuses (`StorageService.h:194-208`). Correct, conservative.
- **Snapshot loss** — always safe (genesis is truth). The only issue is it is never *written* (M2).
- **Display timestamps are wall-clock and unvalidated** (L3) — ordering is by `seq`, so a wrong system
  clock only mis-labels display times; it cannot reorder history. Acceptable, worth a note.

### Lower-severity items
- **L1 — Backups/data are unencrypted plaintext copies** (`BackupScheduler.cpp:13-19`,
  `BackupService.cpp:19`). Anyone who copies the folder reads all financials; no at-rest protection. For
  a paid financial product, at-rest + backup encryption is expected.
- **L2 — Index-cardinality ids** collapse if a rebuild ever drops a short/corrupt event (`payments_[pid]
  = …` would overwrite a prior record). Only reachable under corruption, but it converts corruption into
  *silent* wrong-attribution.

---

## 9. Final confidence score

| Dimension | Confidence | One-line basis |
|---|---|---|
| Storage engine won't lose/corrupt committed events (Windows) | **High (8/10)** | Commit protocol + torn-tail + CRC frames are correct. |
| …same on macOS under power loss | **Low (3/10)** | `fsync` ≠ `F_FULLFSYNC` (H3). |
| History is tamper-evident (audit-grade) | **Low (2/10)** | CRC-32 only, forgeable (M1). |
| Recoverability after latent corruption | **Low (3/10)** | Fail-closed, no repair tool, unprotected header (H2). |
| **Ledger/statement correctness in the shipping app** | **Very Low (2/10)** | Payments never post; three unreconciled balances (C1/C2). |
| Event-sourcing foundation (design) | **High (8/10)** | Sound authority model, replay, governance, verification. |
| Scales to 10M+ events / multi-company / cloud | **Low (3/10)** | 7-scan startup, unbounded RAM, unmergeable log (§6/§7). |
| Update/supply-chain trust boundary | **Low (3/10)** | Symmetric embedded update key (H1). |

**Overall: NOT READY for real business financial data.**

The engineering underneath is better than most — the event-sourcing core would survive a much harsher
review than the product on top of it does. But an accounting system is judged on whether the numbers it
shows the owner are *true*, and today they are not: **a customer pays an invoice and the general ledger
never hears about it, while three screens disagree about the balance and the trial-balance gate reports
"all correct."** That is the specific, demonstrable way these books are *balanced but wrong*. Fix C1/C2
(wire settlement into the ledger and reconcile the three balance sources) before any pilot with real
money; address H1–H3 and M1 before any claim of "audit-grade," "crash-safe on Mac," or "secure
auto-update"; and re-architect identity + the log for merge before "multi-company" or "cloud sync"
appear on a roadmap.

*A successful adversarial review is one that finds the serious flaw before the customer does. The
serious flaw is C1. It is live, it is silent, and every prior correctness gate is green while it is
true.*
