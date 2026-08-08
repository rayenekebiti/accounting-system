# Commercial Product Infrastructure (Phase C2)

This phase turns Occountant into a product that can be **licensed, updated, backed up, diagnosed,
and supported** as a commercial desktop application — while adding **zero** accounting semantics.
The accounting engine (`core/` + `storage/`, behind `StorageService`) is **byte-identical and
replay-equivalent**; C2 lives entirely in a new `app/` layer *above* it.

> **Hard isolation (verified):** `ACCT_COMPAT_VERIFY` still proves full-model replay-equivalence,
> and `ptest`/`fuzz`/`itest` are unchanged. No EventLog / replay / governance / posting / statement
> / storage-format change. No cloud sync, online accounting, subscriptions, telemetry, analytics,
> login, database server, or plugins were added.

## 1. Architecture & trust boundaries

```
             ┌──────────────────────────── QML UI ────────────────────────────┐
             │  About (license + updates) · Diagnostics · Backup · …           │
             └───────────────▲───────────────────────────▲────────────────────┘
                             │ read-only status           │ read-only status
             ┌───────────────┴──────────  app/ (C2 layer) ┴────────────────────┐
             │ LicenseManager  UpdateManager  BackupScheduler  CrashReporter    │
             │ StartupDiagnostics  Logging  Signature  MiniZip  AppInfo         │
             └───────────────▲────────────────────────────────────────────────┘
                             │ READS ONLY (governance, compat, seq, dataDir)
             ┌───────────────┴──────────  StorageService  ──────────────────────┐
             │            EventLog · AuditJournal · repositories (UNTOUCHED)     │
             └─────────────────────────────────────────────────────────────────┘
```

**Trust boundaries**

- **The engine trusts nothing from `app/` and knows nothing about it.** `app/` never calls a
  mutating StorageService method; it only reads `governanceVersions()`, `compatibilityStatus()`,
  `audit().lastSeq()/ledgerSnapshotSeq()/trialBalanceTotal()`, `dataDir()`, `isInitialized()`.
- **Signature boundary.** Licenses and update payloads carry a detached signature verified by
  `sig::verify` before they are trusted. **v1** uses HMAC-SHA256 with an embedded vendor key
  (`app/Signature.cpp`) — *obfuscation-grade*: it detects tampering + casual forgery and gates the
  honest-user licensing flow, but a determined attacker who extracts the key can mint tokens. The
  call sites are algorithm-agnostic; **v2** swaps in Ed25519 (asymmetric — only the *public* key
  ships) with no caller change. This is a deliberate, documented v1 boundary, not an oversight.
- **Data boundary.** Backups/restores/updates operate on *copies* and *staging dirs*; the live data
  directory is only ever read or swapped at a cold start (before `StorageService::initialize`).
- **Privacy boundary.** Logs are redacted of monetary values; crash reports contain only versions,
  platform, and module names — **never** accounting data — and are never transmitted (the user
  chooses to send the `.zip`). No telemetry, ever.

## 2. Licensing lifecycle (`app/LicenseManager`)

States: **Trial · Personal · Business · Expired · Invalid** (`app/LicenseTypes.h`).

```
first run ─▶ issue signed Trial (30d) ─▶ [Trial]
                                           │ expiry passes
                                           ▼
[Trial/Personal/Business] ── within grace (7d) ──▶ usable + inGrace
                                           │ past grace
                                           ▼
                                        [Expired] (not usable)

activate(paid token) ─▶ verify signature ─▶ [Personal|Business]  (exp=0 ⇒ perpetual)
bad signature / unparseable / missing ─────────────────────────▶ [Invalid]
```

- **Offline-first, deterministic.** Validation is 100% local; all time comes from an injectable
  clock, so every transition is reproducible with no wall-clock or network dependency.
- **Cached.** The validated status is written to a signed `license.cache` for continuity + tamper
  detection. The **token is the source of truth**; a corrupt/tampered cache is detected (signature
  mismatch), logged, and never trusted — the state is re-derived from the token.
- **Grace period.** A just-expired license stays usable for `kGraceDays` (flagged `inGrace`) so a
  customer is never locked out mid-work by a lapse.
- **No hardware lock** in v1.
- **Read-only to the UI** via `PlatformController` → the About screen (state badge, days remaining,
  activation field).

## 3. Updater lifecycle (`app/UpdateManager`)

```
check() ─▶ [UpToDate] | [Available]
[Available] ─▶ downloadAndStage() ─▶ verify signature over payload
                       │ fail (interrupted/forged)         │ ok
                       ▼                                    ▼
                    discard (not staged)         write payload, then marker LAST ─▶ [Staged]
[Staged] ─▶ rollbackStaged()  (before restart)  OR  applyPendingAtStartup() on next launch
```

- **No auto-install while running.** A staged update is a self-contained installer bundle under
  `<config>/updates/.pending-update/`; it is only ever applied by `applyPendingAtStartup()` **before**
  `StorageService::initialize` — the DB is never touched by an update.
- **Crash-consistent staging.** The signed marker (`manifest.json`) is written *last*, so an
  interrupted stage can never look complete. Startup verifies payload-vs-signature; an
  incomplete/tampered bundle is cleaned (`Recovered`) and the app proceeds on the current build.
- **Offline/deterministic tests.** The "source" is a local path (network fetch is the documented
  production gap — wire the same `check()`/`downloadAndStage()` interface to an HTTPS manifest +
  QtNetwork; the state machine, signature verification, staging, rollback, and apply-on-restart are
  fully implemented and tested locally).

## 4. Backups (`app/BackupScheduler`)

Scheduling (min interval), retention (`keep` + `maxAgeDays`), restore points, per-backup
verification (opens the copy's authoritative `audit.log` read-only → CRC + gap-free seq), and
cleanup. Deterministic clock. A backup is a *copy* into `<dataDir>/backups/backup-<epoch>/`; the
scheduler never writes into the live books. Complements the manual Backup UX (C1) and the
stage-and-restart restore.

## 5. Crash reporting (`app/CrashReporter` + `app/MiniZip`)

Local-first. Signal/`std::terminate` handlers collect a **stack trace** (module+offset frames via
`RtlCaptureStackBackTrace`), **build**, **governance versions**, **platform**, and **loaded
modules** (`EnumProcessModules`) into a single `CrashReport_<ts>.zip` (a dependency-free store-only
zip). It contains **no accounting values**. Nothing is transmitted — the user explicitly chooses
whether to send the file.

## 6. Production logging (`app/Logging`)

Four levels (Info/Warning/Error/Critical), size-based **rolling files**
(`<dataDir>/logs/occountant.log` + `.1..N`), and **redaction** of currency-shaped tokens so a stray
amount can never reach a shipped log. Optionally chains Qt's `qWarning`/… into the rolling file
(redacted) for the real UI run; the dev diagnostics pipeline is unchanged.

## 7. Startup diagnostics (`app/StartupDiagnostics`)

**One** read-only health struct + report aggregating: storage (open, db size, events, seq),
compatibility + governance, snapshot, verification (trial balance), license, backup age + restore
points, pending/available update, and free disk. Surfaced via `PlatformController.healthReport`
and logged once at startup. Reads only — never mutates the engine or any manager.

## 8. Startup sequence (order matters)

1. Resolve `dataPath` (books) + `configPath` (machine-global: license + update staging).
2. `prodlog::init` (rolling log).
3. `UpdateManager::applyPendingAtStartup(configPath/updates)` — apply/clean a staged update
   **before the store opens**; never touches the DB.
4. Apply a user-staged **restore** (C1) — same cold-start swap.
5. `StorageService::initialize` — the engine opens (crash recovery / migration as before).
6. Build view-models; `LicenseManager::initialize` (offline, issues a trial on first run);
   `BackupScheduler`; `UpdateManager::check`; `crashreport::install` (with the engine's governance
   line); `PlatformController`.
7. Load QML; `prodlog::installQtHandler`; `BackupScheduler::start`; log the startup health report.

## 9. Failure modes & recovery paths

| Failure | Detection | Recovery |
|---|---|---|
| Expired / lapsed license | `deriveState` vs clock | grace window keeps usage; then `Expired` (read-only status; activate to restore) |
| Tampered/forged license | `sig::verify` fails | `Invalid`; app surfaces activation prompt |
| Corrupt license cache | cache signature mismatch | ignored + logged; state re-derived from the signed token |
| Interrupted / forged update download | payload-vs-signature mismatch | discarded; not staged |
| Interrupted staging (crash mid-promote) | marker missing/invalid at startup | `applyPendingAtStartup` cleans it (`Recovered`); DB untouched |
| Staged update no longer wanted | user action | `rollbackStaged()` before restart |
| Backup media/disk failure | copy error | half-written backup removed; error logged |
| Corrupt restore point | `EventLog` open throws | `verify()` reports it; excluded from trustworthy restore points |
| Process crash | signal/terminate handler | `CrashReport_*.zip` written locally; default handler re-raised so the OS still records it |
| Log growth | size threshold | rotation (`keep` N) |

## 10. Installer readiness (Windows) — review

- **Version stamping.** `app/AppInfo.h` is the single source of `version`/`buildId`/`channel`
  (CI stamps `-DACCT_BUILD_ID` / `-DACCT_CHANNEL`). `versionCode()` gates the updater.
- **Deployment.** `deploy_quick`/`windeployqt` closure + clean-room verification already exist
  (see `docs/deployment-packaging.md`). C2 adds no new third-party runtime dependency (pure Qt +
  the psapi already linked).
- **User data locations.** Books → `AppDataLocation` (overridable via `ACCT_DATA_DIR`); license +
  update staging → `AppConfigLocation` (overridable via `ACCT_CONFIG_DIR`); logs → `<dataDir>/logs`;
  crash reports → `<dataDir>/crash`.
- **Portable mode.** Point `ACCT_DATA_DIR` + `ACCT_CONFIG_DIR` at a folder next to the executable
  and everything (books, license, logs, backups, staging) stays self-contained on the stick.
- **Installer / shortcuts / file associations / signing.** Recommended next step: an Inno Setup /
  WiX package that installs the `windeployqt` closure, creates Start-menu + desktop shortcuts,
  (optionally) associates an `.occt` books bundle, and Authenticode-signs the binary + installer.
  The updater is designed to hand the staged installer this signed package. *(Packaging scripts are
  the documented remaining step; the app-side hooks are in place.)*

## 11. Verification (`ACCT_C2TEST`, deterministic, network-free)

`quick/c2test.cpp` — 15 assertions covering the 10 required scenarios + edges:
expired license · invalid signature · corrupted cache · update interrupted · staged rollback ·
backup scheduler (due/create/verify/retention) · crash-report generation · log rotation ·
startup diagnostics · recovery after interrupted update; plus grace period, first-run trial,
up-to-date, corrupt-restore-point rejection, and complete-staged-apply.

```
ACCT_C2TEST=<scratch>  ./AccountingQuick     # 15 passed, 0 failed
```

All existing gates remain green: `itest` 116/0, `ptest`, `fuzz` ROBUST, `ACCT_COMPAT_VERIFY`
(replay-equivalence held), `i18n-check` PASSED.

## 12. Future SaaS roadmap (out of scope here)

Asymmetric (Ed25519) license + update signing; HTTPS update manifest + delta downloads with an
Authenticode-signed installer hand-off; optional account-based license sync + seat management;
opt-in, anonymised, user-consented crash upload; cloud backup targets. Every one of these builds on
the C2 interfaces **above** the engine — none require touching the deterministic accounting core.
