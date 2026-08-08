# Regression Defense Matrix

Five independent layers now defend the app. Each targets a class of regression the
others structurally miss. Run all five before a release (or wire into CI).

| Layer | Tool | Catches | Gates on |
|-------|------|---------|----------|
| **Visual rendering** | `tools/shots.sh` | RTL mirroring, layout breakage, clipping, alignment, typography | manual/CI visual diff of `build/baselines/<lang>/<size>/` |
| **Interaction behavior** | `tools/itest.sh` | QML semantic bugs: property-setter-as-method, stale `model` refs, broken signal wiring, validation/persistence flows | exit code (0 = pass) |
| **Runtime console cleanliness** | `tools/itest.sh` (built in) | ReferenceError, TypeError, binding loops, unknown properties, delegate/model scope errors, null derefs | any captured QML error → non-zero exit |
| **Translation + RTL semantics** | `tools/i18n-check.sh` | hardcoded literals, missing/empty translations, wrong plural counts, concatenation, alignment hacks, stale catalogs. **Full-i18n phase closed the standing freshness debt: catalogs synced at 471 messages × EN/FR/AR, 0 unfinished, all 7 checks green.** C++ `tr()` in the 5 UI-facing ViewModels/models is now extracted (added to the `lupdate` scan) + retranslated live; enum keys are mapped key→`qsTr` at display. See `docs/i18n-completion.md`. | exit code (0 = pass) |
| **Persistence + integrity + crash safety** | `tools/ptest.sh` | journal replay/corruption/staleness/tearing, partial-write recovery, lost/duplicated records, `total ≠ Σ lines` rounding drift, schema-migration correctness + crash-safety, downgrade protection, **audit-log replay determinism + byte-identical rebuild + event/projection crash reconciliation + committed-history corruption detection, **projection drift detection (live vs history fingerprint), historical reconstruction (effective-at-seq), non-destructive + crash-safe verification, **commit cutover + backfill adoption, stable monotonic child (line) identity, content-identical multi-record rebuild, correction-by-stable-id, **period closure / historical freezing (post-close correction rejected, books-as-closed reconstruction, append-only reopen, atomic close), **structured corrections (void in-place open-only, reversal append-only closed-OK, deterministic lineage, atomic void), **deterministic allocation/reconciliation (derived settlement no-paid-flag, partial/multi, over-allocation rejected, historical settledAt, settlement reversal, atomic allocation), **double-entry ledger (balanced postings enforced, trial-balance-always-0, historical balanceAt, reversal lineage, closed-period rejection, atomic posting), **financial statements (income statement, balance-sheet-always-balances, historical-at-seq, closing entries → retained earnings, post-close adjustment isolation, deterministic rebuild), **business-event→ledger posting authority (fixed policy Dr AR/Cr Revenue + Dr Cash/Cr AR, balanced, reversal-compensating, role-config-independent deterministic replay), **deterministic snapshotting (snapshot+tail == genesis, verify-vs-genesis, corruption→absent→fallback, disposable, atomic create)**, **historical compatibility & evolution governance (version-manifest round-trip + CRC rejection, classification compatible/newer→incompatible/older→migration-required/below-floor, EngineVersionStamp adoption+idempotence, replay-equivalence gate [genesis + snapshot + trial-balance + determinism], posting-policy attribution + role-config-independent replay, manifest-write crash complete-or-absent, downgrade refused)**, **full domain event-sourcing cutover (supplier event-authoring + disposable-projection rebuild, per-entity backfill adoption [suppliers + invoices] + idempotence, verifyAll full-model live==history + tampered-projection drift, supplier/invoice event-commit crash reconciliation)**, **atomic business transaction semantics (grouped commit — appendAtomic contiguous seqs + survives-reopen, atomic invoice+revenue lands both, draft omits posting, atomic reversal invoice+lineage, three-interruption-point crash proof [afterTxnFirstFrame→absent / afterTxnCommit / afterProjectBeforeCursor→complete] = books always absent-or-complete never split, verifyAll clean in both)**, real cross-process crash durability | exit code (0 = pass) |
| **Performance & scalability** | `tools/perf.sh` | throughput/latency regressions in the event-sourcing engine at scale (startup index build, ledger queries, statement generation, snapshot, replay unit cost, memory & disk per event). Deterministic seeded synthetic books 10k–1M events; size-stable rate thresholds with ~50% headroom. **Measured finding: log-folds are sub-second at 1M; projection materialization is 18 ms/record (journal-churn-bound) — the one bottleneck. See `docs/performance.md`.** | exit code (0 = within thresholds) |
| **Adversarial robustness** | `tools/fuzz.sh` | malformed/corrupted on-disk artifacts (EventLog frames, ledger snapshot, compat manifest, BinaryRecordFile, journals) accepted silently / UB / crash-without-diagnosis; replay divergence; broken accounting invariants under randomized histories; failed persistence writes (commit-point/cursor/snapshot/manifest) not recovered. **Found + fixed a real defect: snapshot integrity hash did not cover the header `seq`/`count` → accelerator could mis-accelerate (now format v2).** Deterministic, seeded; `ITERS`/`SEED` control depth. | exit code (0 = robust) |
| **Deployment self-sufficiency** | `tools/deploy-deps.sh` + stripped-`PATH` run | missing MinGW third-party DLLs (`windeployqt` gap), unbundled plugins/QML modules → `STATUS_DLL_NOT_FOUND` on a clean machine | closure verified; clean-room exit 0 |
| **Clean-room smoke** | `tools/cleanroom.ps1` | deployment-viability regressions: startup, translation load, RTL, persistence, editor open — all on bare `PATH` | 7 checks (0 = deployable) |
| **Upgrade safety** | `tools/upgrade-test.sh` | data/journal/settings loss across an upgrade restart | exit code (0 = pass) |
| **Runtime warning capture** | `ACCT_DIAGTEST=1` + `ACCT_ENDURE=N` | unclassified/uncounted Qt-QML warnings; warning DRIFT + memory growth over a long session | self-test PASS; drift==0 |
| **Accessibility tree** | `ACCT_A11Y=1` | interactive controls missing a screen-reader role/name (real `QAccessible` tree, not QML source) | named/unnamed report (manual NVDA still required) |
| **Commercial infrastructure (C2)** | `ACCT_C2TEST=<dir>` | licensing (expired / tampered-signature / corrupt-cache / grace / first-run trial), updater (interrupted download, staged rollback, apply-on-restart, recovery after interrupted staging — DB untouched), backup scheduler (due / create / verify / retention), crash-report `.zip` generation (build+governance, no accounting data), production-log rotation, and the aggregated startup-diagnostics report. All deterministic (injectable clock) + network-free; lives entirely ABOVE `StorageService`. **15 assertions.** See `docs/commercial-infrastructure.md`. | exit code (0 = all passed) |

## What each layer alone would MISS (why all four exist)

- Screenshots pass while a button does nothing (interaction bug) → caught by `itest`.
- `itest` + C++ tests pass while the layout is visually ragged (alignment) → caught by `shots` + `i18n-check`.
- All code-level tests pass while a new string is untranslated → caught by `i18n-check`.
- Everything "builds" while a delegate emits a runtime `ReferenceError` only on edit →
  caught by `itest`'s console capture.
- The whole UI works while a power cut silently drops the last save, or an invoice's
  total disagrees with its lines by a cent → caught by `ptest` (crash recovery + the
  `total == Σ lines` invariant). See `docs/persistence.md`.
- Everything passes on the dev machine while the app won't even start on a clean
  machine (missing MinGW DLLs `windeployqt` never copied) → caught by the stripped-
  `PATH` clean-room run. See `docs/operational-hardening.md` §1.

The Invoice-editor `setCustomerId` regression is the canonical example: it passed
screenshots **and** C++ VM probes, and was only caught once the interaction layer drove
the real `Select.activated` handler.

## Running the suite

```bash
bash tools/i18n-check.sh     # static: translation + RTL invariants (fast)
bash tools/itest.sh          # interaction + runtime-console (builds, seeds, drives QML)
bash tools/ptest.sh          # persistence + integrity + real cross-process crash recovery
bash tools/shots.sh          # visual: regenerate EN/FR/AR baselines for diff
# pwsh tools/i18n-extract.ps1  # only when qsTr strings changed (refresh catalogs)
```

All exit non-zero on failure. `itest.sh`, `ptest.sh`, and `shots.sh` use isolated,
seeded `ACCT_DATA_DIR` sandboxes — they never touch real user data.

## Coverage today (interaction suite, `quick/itest.cpp`)

- **Invoice creation** — open → select customer (real `onActivated`) → dates → line items
  (`setCell`) → `commit()` → persisted, `saved()` emitted, zero QML errors. A posted invoice
  routes through the event log and posts a balanced ledger revenue entry (trial balance 0);
  the income statement recognises it (Full Domain Cutover).
- **Customer create/edit** — live validation (required, invalid email) appears/clears →
  `commit()` → persisted → reopen verifies round-trip.
- **Supplier create/edit** — live validation → `commit()` → persisted → reopen round-trip;
  the commit appends exactly one authoritative event (event-authored, **no repo bypass**).
  See `docs/workflow-completeness.md`.
- **Payments & settlement** — record a payment; one payment → many invoices; many payments →
  one invoice; partial + full allocation; unallocated credit; allocation reversal restoring
  outstanding; trial balance 0; `verifyAll` replay-stable; each op = one authoritative event
  (no repo bypass). Plus `ptest` `settlement UI accessors` (correctness + replay-stability).
  See `docs/payments-workflow.md`.
- **Ledger explorer (read-only)** — the UI models mirror the engine exactly: account balances
  == `balanceFor`; `TrialBalanceModel` Σ debits == Σ credits; the Journal count == `entryCount`;
  account scoping == `entriesForAccount`; a real journal reversal's lineage surfaces through the
  inspector both directions; trial balance stays 0; no write authority introduced. Plus `ptest`
  `ledger UI accessors` (enumerators match the index + reversal lineage + replay-stability).
  See `docs/ledger-explorer-workflow.md`.
- **Expenses & purchasing** — the editor VM authors an expense as ONE atomic group
  (ExpenseCreated + JournalEntryPosted), posting Dr Expenses / Cr Cash|AP; correction posts the
  delta; void marks VOID in place + a compensating entry; reversal is append-only; period-freeze
  rejects void/correct; income statement + trial balance stay consistent (0); `verifyAll`
  byte-verifies the expense projection (no repo bypass). Plus `ptest` `testExpenseLifecycle`
  (full lifecycle + replay-equivalence + snapshot). See `docs/expenses-workflow.md`.
- **Security boundaries** (`ptest` `testSecurityBoundaries` + `fuzz` + `itest`) — untrusted
  on-disk lengths are bounded: an EventLog header claiming more than the file holds is rejected
  loudly without a length-driven allocation (F1); a CRC-valid journal targeting past the record
  count is discarded with no arbitrary-offset/sparse write (F2); `Money::fromDouble` is defined
  (no UB) on `inf`/`nan`/out-of-range and the editors reject non-finite/absurd amounts (F3).
  See `docs/security-review.md`.
- **Tax engine (VAT/GST)** (`ptest` `testTaxEngine` + `testGovernanceTransition`, `fuzz`
  `fuzzTaxCodes`, `itest`) — posting-policy v2 splits invoice tax (Dr AR / Cr Revenue / Cr Tax
  Payable) and expense recovery (Dr Expense / Dr Recoverable Tax / Cr Cash|AP); taxable / exempt /
  zero-rated / mixed invoices; reversal + void reverse the tax exactly; `taxSummaryAt(seq)`
  reconstructs (historical + books-as-closed); replay-identical after rebuild; snapshot
  equivalence; trial balance 0; an existing postingPolicy-v1 book adopts v2 on open with historical
  values unchanged (registered semantic migration). See `docs/tax-engine.md`.
- **Settings & System (C1, read-only)** (`itest` `settings-system`) — the Diagnostics VM surfaces
  live engine metrics (event count, database size, trial-balance-balanced) and its **Run
  verification** confirms projection==history + replay-equivalence; the **critical invariant** is
  asserted directly: reading Diagnostics / changing a Settings preference **does not advance the
  sequence or append any event** (the screens are read-only over the store). The Settings currency
  preference round-trips through `QSettings`; the Backup VM surfaces an estimated size; and every
  Settings tab (General / Company / Backup / Diagnostics / About) switches with zero QML errors.
  Visual baselines `17–21_settings_*` (EN + AR-RTL) cover the rendering + mirroring. See
  `docs/production-readiness.md`.
- **Language switching** — EN↔FR↔AR live; editor remains functional after switch; RTL
  direction + translations verified; no binding churn / QML errors.
- **Line-item editing** — `setCell` → totals recompute (subtotal/tax) → add/remove rows.

## Known boundaries (honest)

- Live caret/IME/paste in text fields = Qt framework behavior; needs a manual Arabic
  typing pass (a static grab can't show caret motion).
- `itest` runs without `exec()`, so it can drive direct popup-child controls (Select,
  TextField) and model invokables, but **not** lazily-realized `Repeater` delegate
  children (those need the window exposed under `exec()` — covered by `shots.sh`).
- Visual diffing is currently manual review of baselines; an automated pixel-diff step
  can be layered on later.
