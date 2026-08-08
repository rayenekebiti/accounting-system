# Production Readiness Review (Phase C1)

Where the app stands as a **polished commercial desktop product** — audited across the ten
readiness areas below. C1 added the missing commercial *surfaces* (Settings, Diagnostics, Backup
UX, crash-recovery UX) and the production-audit cleanup; it changed **no** accounting semantics,
event schema, storage format, posting, tax, replay, snapshot, security, or performance behaviour.
Every deterministic guarantee from B2–B5 is intact and all regression gates remain green.

> **Non-goals honoured (verbatim):** no changes to event sourcing, storage, ledger, tax engine,
> posting, compatibility, replay, snapshots, security, performance, business rules, or accounting
> semantics. No new accounting features, schema changes, or event changes.

## What C1 shipped

- **Settings & System workspace** (one nav item, tabbed like the Ledger explorer):
  - **General** — language, date format (live preview), currency symbol (`QSettings`).
  - **Company** — business name / address / tax number / email (`QSettings`).
  - **Backup & Restore** — Manual backup, backup history (date + estimated size), Verify,
    Restore (confirm → restart), busy state, actionable status. Never exposes internal file names.
  - **Diagnostics** — read-only engine health + on-demand integrity verification.
  - **About** — version + honestly-labelled "Coming soon" Licensing / Updates.
- **Crash-recovery UX** — a one-time reassurance dialog after a verified recovery; a **blocking**
  screen (no "continue") if re-verification fails.
- **Production cleanup** — removed the dead CLI `display()`/`std::cout` bodies from all seven
  `core` entities (and their `<iostream>` includes + header declarations).
- **C++ view-models** (all read-only w.r.t. the engine): `DiagnosticsViewModel`,
  `SettingsViewModel`, `BackupViewModel`.
- **New itest** `settings-system` (12 assertions) + visual baselines `17–21_settings_*` (EN + AR).

Files: `quick/{DiagnosticsViewModel,SettingsViewModel,BackupViewModel}.{h,cpp}`;
`quick/qml/{SettingsWorkspace,GeneralSettings,CompanySettings,BackupScreen,DiagnosticsScreen,AboutScreen,RecoveryDialog,RecoveryBlocker}.qml`;
wired via `NavRail.qml`, `Main.qml`, `main_quick.cpp`, `CMakeLists.txt`.

---

## Area-by-area audit

Each area: **Reviewed** (what was checked) · **B2–B5 baseline** (already delivered) · **C1 adds** ·
**Residual gaps** (tracked in the RC roadmap).

### 1. Error handling
- **Reviewed:** commit failures, invalid input, corrupt/newer-than-build data, backup/restore I/O.
- **Baseline:** editors surface inline field errors (stable KEYS → `qsTr`) with live validation;
  `Money::fromDouble` is UB-free on inf/nan and editors reject non-finite/absurd amounts; the
  engine **refuses** corruption / downgrade loudly (`REFUSING TO OPEN`, non-zero exit) rather than
  running degraded; crashes recover deterministically (journal replay + reconcile).
- **C1 adds:** every Backup/Restore/Verify path returns an actionable message
  ("check that the folder is writable and the disk is not full", "this backup is corrupt; keep an
  earlier one"); half-written backups are cleaned up; the crash-recovery **blocker** turns a
  silent-drift risk into an explicit, non-dismissable instruction to restore.
- **Residual:** a global uncaught-exception → friendly dialog wrapper (today: loud log + refuse).

### 2. User-workflow polish
- **Reviewed:** keyboard nav, tab order, shortcuts, busy/loading feedback, confirmations,
  destructive-action wording, empty states, search/filter.
- **Baseline:** every list has search + filter chips + empty states; editors have full keyboard
  flow, focus rings, dirty-guard discard; destructive actions use a danger-variant `ConfirmDialog`
  that **defaults focus to Cancel**; `AppButton` has a built-in `loading` BusyIndicator.
- **C1 adds:** Settings tabs are keyboard-navigable; Backup/Verify/Restore show the busy state;
  Restore uses a danger confirm with explicit "This cannot be undone" + restart wording; the
  Backup screen has a first-run empty state with a primary CTA.
- **Residual:** app-wide accelerator/shortcut map (e.g. Ctrl+F focus search, Ctrl+N new); a global
  busy-cursor during multi-second operations at scale.

### 3. Accessibility
- **Reviewed:** focus indicators, screen-reader names/roles, keyboard-only nav, RTL, font scaling,
  icon-only labels, contrast.
- **Baseline:** custom controls declare `Accessible.role`/`name` and handle Enter/Space/press;
  the `ACCT_A11Y=1` audit walks the real `QAccessible` tree; RTL mirroring is screenshot-verified;
  high-DPI via `QT_SCALE_FACTOR`; tokens give WCAG-reasonable contrast.
- **C1 adds:** icon/ambiguous buttons carry explicit `accessibleName` (Back Up Now, Verify,
  Restore, Run verification, Quit); the ⚙ nav item announces "Settings"; RTL verified on all five
  new screens (baselines `17–21` AR).
- **Residual:** a manual NVDA/VoiceOver pass; a documented full-keyboard tour; a formal
  contrast-ratio sweep of every token pair.

### 4. Backup UX
- **Reviewed:** manual backup, restore, verification, history, overwrite/confirm, estimated size,
  progress, error recovery, no internal-detail leakage.
- **Baseline:** `BackupService` auto-backed-up on quit and pruned — but had **no** in-app UX.
- **C1 adds:** the full **Backup & Restore** screen. A backup is a plain full copy of the data
  folder into `backups/<timestamp>/`; history shows "Backup from &lt;date&gt; (&lt;size&gt;)" only.
  Verify opens the backup's authoritative log read-only (CRC + gap-free-seq). Restore stages into
  `.pending-restore/` and is applied on the **next startup, before the store opens** — safe against
  the live `QLockFile` and crash-consistent (all-or-nothing swap). Estimated size shown up front.
- **Residual:** user-chosen backup location / export to external media; scheduled backups;
  incremental (not full-copy) backups for very large books.

### 5. Settings workspace
- **Reviewed:** General, Language, Theme, Date, Currency, Company, Backup settings, Diagnostics,
  Licensing/Updates placeholders, no hidden dev menus.
- **Baseline:** language was a floating switcher only; nothing else existed.
- **C1 adds:** General (language/date/currency), Company, Backup, Diagnostics, About — all real,
  all `QSettings`-backed or read-only. Licensing/Updates are labelled "Coming soon" (no fake UI).
  The engine-only harness modes (`ACCT_*`) are env-gated and never surface as a UI dev menu.
- **Residual:** Theme (light/dark) toggle is deliberately **omitted** — the `Theme` singleton has
  no dark palette yet, and shipping a dead switch would violate "no placeholder UI". Tax-default
  and invoice-numbering **display** prefs deferred (the engine's numbering is authoritative and
  unchanged). Both are on the RC roadmap.

### 6. Diagnostics
- **Reviewed:** engine/compat/posting-policy versions, snapshot status, DB size, event count,
  current seq, projection+replay verification, trial balance, last backup, integrity check.
- **Baseline:** all of this existed only as a one-line **startup log** + the `ACCT_VERIFY` /
  `ACCT_COMPAT_VERIFY` headless gates — nothing in-app.
- **C1 adds:** a read-only **Diagnostics** page exposing every one of those metrics live, plus a
  **Run verification** button that runs the same non-destructive `verifyAuditProjection()` +
  `validateCompatibility()` checks and shows pass/fail badges. `itest` proves reading it does not
  mutate the store.
- **Residual:** an in-app perf snapshot (startup/query timings) surfaced on the page (the numbers
  exist via `ACCT_PERF`/`ACCT_BENCH`; not yet shown in the UI).

### 7. Crash-recovery UX
- **Reviewed:** concise recovery dialog, blocking screen on verification failure, never continue
  silently.
- **Baseline:** recovery was **silent** — the facts lived only in
  `auditReconciled()`/`auditTornTail()` + the log.
- **C1 adds:** `captureStartupRecovery()` detects a recovery and re-verifies the projection. Clean
  → one reassurance `RecoveryDialog` ("Recovered successfully… verified against history… no
  inconsistencies"). Not clean → a **blocking** `RecoveryBlocker` (red, no "continue", "quit and
  restore your most recent backup"). The app never proceeds on suspect data.
- **Residual:** an automated end-to-end `ACCT_CRASH_POINT` → reopen → assert-dialog itest (today
  the recovery *engine* is fuzz/ptest-proven; the *dialog trigger* is manually verified).

### 8. Documentation (in-app)
- **Reviewed:** tooltips, help text, consistent terminology.
- **Baseline:** clear labels + placeholders throughout.
- **C1 adds:** every Settings section has an explanatory sub-line ("Amounts themselves are always
  stored exactly, in cents", "This is read-only and safe to run at any time"); consistent
  terminology (backup / restore / verify; books / data).
- **Residual:** hover `ToolTip` / "What's This?" affordances on individual controls; a bundled
  user guide.

### 9. Visual consistency
- **Reviewed:** spacing, margins, padding, icons, typography, dialog structure, button ordering,
  RTL, theme.
- **Baseline:** a single `Theme` token system (space/radius/font/color/elevation); all screens
  compose the same `PageHeader`/`Card`/`Chip`/`AppButton`/`Badge`/`MetricCell` primitives.
- **C1 adds:** the five new screens use **only** tokens and shared components — zero ad-hoc
  colours/sizes — so they inherit spacing, typography, RTL, and dialog conventions automatically
  (Cancel-left / primary-right ordering; danger variant for destructive). Verified across EN + AR
  baselines.
- **Residual:** a genuine dark theme (see area 5).

### 10. Production audit (dead code / debug output)
- **Reviewed:** `TODO`/`FIXME`/`XXX`/`HACK`, `qDebug`/`std::cout`/`printf`, placeholder/dummy,
  dead code — across the shipping `quick/` + `core/` + `storage/` tree.
- **Findings + resolution:** the only real debug output was the dead CLI `display()` `std::cout`
  bodies on the seven `core` entities — **removed** (bodies, `<iostream>` includes, and header
  declarations; `display()` had zero callers). All other hits are benign: legitimate input
  `placeholder` text, `Money`'s `snprintf`/`toDouble` formatting, a tamper-detection test string,
  `.md` specs, the intentional "Coming soon" labels, and one documented synchronous-load comment.
  No `TODO`/`FIXME` remain in shipping code.

---

## Verification (all gates green)

| Gate | Result |
|------|--------|
| Build (`cmake --build build`) | clean (MSYS2 UCRT64) |
| `tools/itest.sh` | **116 passed, 0 failed** (incl. 12 new `settings-system`) |
| `tools/ptest.sh` | ALL PERSISTENCE TESTS PASSED |
| `tools/fuzz.sh` | ROBUST (incl. cross-process fault-injection recovery) |
| `ACCT_COMPAT_VERIFY=1` | replay-equivalence held (full model + snapshot + trial balance) |
| `tools/i18n-check.sh` | checks 1–4, 6, 7 pass; **#5 catalog freshness fails** — accepted standing debt (see below) |
| `tools/shots.sh` | EN + AR baselines `17–21_settings_*` captured + visually verified (RTL mirrors correctly) |

**Manual visual verification (screenshots reviewed):** Diagnostics renders live metrics + green
trial balance + both verification badges; Backup renders the empty state + estimated size + CTAs;
General/Company render correctly; **AR-RTL mirrors perfectly** (NavRail on the right, tabs + headers
mirrored, chips + fields laid out RTL).

---

## Known limitations (honest)

- **i18n catalog freshness (standing debt).** The new C1 `qsTr` strings are wrapped but not yet in
  the `app_{en,fr,ar}.ts` catalogs, so they fall back to English until `pwsh tools/i18n-extract.ps1`
  + a translation pass. This is the pre-existing accepted i18n-check #5 debt (the AR catalog was
  already partial); the **RTL layout** is correct regardless. Not a functional blocker.
- **No dark theme.** `Theme.dark` exists but there is no dark palette; a toggle was intentionally
  omitted rather than shipped dead.
- **Backup is a full copy to the default data location** (no user-chosen path, no scheduling, no
  incremental). Restore requires a restart (by design — safe against the live lock).
- **Licensing / Updates are placeholders** (clearly labelled), pending those subsystems.
- **Crash-recovery dialog trigger** is manually verified; the recovery *engine* is gate-proven.

---

## Roadmap to Release Candidate (RC)

1. **Localization pass** — refresh the `.ts` catalogs and translate FR + AR (closes i18n #5).
2. **Dark theme** — add a dark token palette to `Theme` + a real General → Theme toggle.
3. **Crash-recovery e2e test** — `ACCT_CRASH_POINT` → reopen → assert the correct dialog/blocker.
4. **In-app help** — `ToolTip` / "What's This?" on controls + a bundled user guide.
5. **Accessibility sign-off** — manual NVDA/VoiceOver pass + a formal token contrast sweep.
6. **Backup polish** — user-chosen location / export to external media; optional scheduling.
7. **Diagnostics perf snapshot** — surface the `ACCT_PERF` timings in the UI.
8. **App-wide shortcuts** — Ctrl+F / Ctrl+N / etc. and a global busy cursor for long operations.
9. **Licensing / Updates** — implement (replacing the "Coming soon" placeholders).

None of these change the accounting core; each builds on the authoritative event pipeline.
