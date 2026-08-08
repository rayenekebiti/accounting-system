# Pilot Release Guide — Occountant (Phase C9)

**Audience:** the vendor engineer/operator who cuts the pilot build and hands it to the first
5–10 paying small businesses. **Not** a customer-facing document (that's `docs/user/`).

**Goal of the pilot:** put a *stable, unchanged* Occountant in real hands and learn. This guide
covers only what it takes to **produce, validate, and hand over** a build that a non-technical
owner can install and trust. No feature work belongs in a pilot build.

> Golden rule: the pilot build must be **byte-for-byte the tested build**. Do not slip a "small
> fix" into the handover artifact without re-running the full battery (§6) against it.

---

## 0. One-page summary (the operator's checklist)

1. Clean tree, known commit, all gates green (`§6`).
2. `cmake --build build --target AccountingQuick` then `--target deploy_quick` (DLL closure).
3. `powershell -ExecutionPolicy Bypass -File tools/cleanroom.ps1` → **DEPLOYABLE**.
4. Build the installer from `installer/Occountant.iss` on an Inno Setup 6 machine.
5. Authenticode-sign the installer + exe (production cert).
6. Smoke-test **install → first-run → enter data → upgrade → uninstall** on a clean Win10 and
   Win11 VM. Confirm uninstall **keeps** the books.
7. Fill the **Pilot Release Checklist** (`§7`), attach the gate log, tag the commit, hand over.

---

## 1. Toolchain & provenance

- **Toolchain:** MSYS2 **UCRT64** — GCC 15.x, Qt 6.x, CMake + Ninja. The canonical repo is
  `…\Videos\Captures\PROJECTS\accounting-system\occountant` (the `C:\Users\rayan` home-dir tree is a
  stale copy — do not build from it).
- **Provenance is embedded, not stamped by hand.** `CMakeLists.txt` captures the git short-commit,
  a dirty flag, and the **commit** timestamp (not wall-clock) into `app/BuildInfo`. A given commit
  therefore produces a reproducible BuildInfo. Dump it any time with:
  ```
  ACCT_BUILDINFO=buildinfo.json ./build/AccountingQuick.exe
  ```
- **Release stamping:** `tools/release.sh` passes `ACCT_RELEASE_CHANNEL` / `ACCT_RELEASE_BUILD_ID`
  (and, for the customer artifact, `-DACCT_DEV_SIGNING=OFF` — see `§4`). A bare local build falls
  back to the `development` channel.

**Before you cut a pilot build:** working tree clean (`git status` empty), on the intended commit,
and `ACCT_GIT_DIRTY=0` in BuildInfo. A dirty pilot build is not reproducible and must not ship.

---

## 2. Build & Qt/runtime DLL deployment

Two steps. The first builds; the second **completes the DLL closure** that `windeployqt` leaves
incomplete on MinGW.

```bash
cd occountant
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --target AccountingQuick        # builds + runs windeployqt (Qt DLLs/plugins/QML)
cmake --build build --target deploy_quick           # bundles the MSYS2 third-party DLL closure
```

**Why `deploy_quick` is mandatory.** `windeployqt` copies *Qt's* DLLs, plugins, and QML modules —
but **not** the MinGW ecosystem libraries they transitively need (ICU, harfbuzz, freetype, PCRE2,
glib, zstd, libpng, libjpeg, …). On the dev machine those resolve from `ucrt64/bin` on `PATH`, so
the app "works"; on a clean customer machine without MSYS2 it dies at load with
**`STATUS_DLL_NOT_FOUND` (0xC0000135)** and no window ever appears. `tools/deploy-deps.sh` (invoked
by `deploy_quick`) walks the PE import tables of the exe **and every deployed plugin/QML-module
DLL**, recursively, via `objdump -p`, and copies every missing dependency into the deploy tree.
It is idempotent; exit 0 = closure complete.

> Do not skip `deploy_quick` because "the last build already has the DLLs." A Qt update can pull in
> a new transitive dependency. Always re-run it for the artifact you actually ship.

---

## 3. Verify Qt/runtime deployment — clean-room smoke test

This is the single most important packaging gate for a pilot. It proves the deploy tree is
self-contained.

```
powershell -ExecutionPolicy Bypass -File tools/cleanroom.ps1
```

`cleanroom.ps1` strips `PATH` to **bare Windows** (`System32` only — no MSYS2, no ucrt64, no Qt),
launches the app under the `ACCT_PROBE` harness, and asserts every real subsystem came up from the
deploy tree alone:

- process started **and exited cleanly (0)**;
- engine loaded and persistence is reachable (`totalCount=N`);
- the **Arabic** translation catalog loaded and **RTL** layout applied (proves the `.qm` catalogs
  and the Qt translation plugin deployed);
- the customer editor opened (proves the QuickControls style module deployed);
- the language list + endonyms are present.

**A missing DLL or QML module shows up here as a non-zero exit or a failed assertion** — never a
customer bug report. Treat any `FAILED` as a hard release blocker.

> This is a *clean-environment* test, not a *clean-machine* test — it runs on the dev box with a
> stripped `PATH`. It catches ~all missing-dependency defects. It does **not** substitute for a
> real VM smoke test of the **installer** (`§5`), which also exercises registry/ARP/shortcuts.

---

## 4. Update signing — pilot vs. public posture

As of C9 the update path uses **asymmetric Ed25519** detached signatures: the shipped binary embeds
only the **public** key and can *verify* an update but cannot *forge* one; the **secret** key lives
in a dev/release build only (`-DACCT_DEV_SIGNING`, default ON for gates, **OFF** for the customer
artifact) and never enters the shipped exe. See `docs/update-signing.md`.

- **Customer/pilot artifact:** build with `-DACCT_DEV_SIGNING=OFF`. Confirm the secret key is absent
  (`§7` checklist item). The app verifies updates; it cannot sign them.
- **The vendor** signs a release payload offline from a dev build:
  `ACCT_SIGN=<payload> ./AccountingQuick.exe` prints the detached hex signature to drop into
  `manifest.json`.
- **Local update source in v1:** the updater reads a **local path**, not HTTPS. For a controlled
  pilot this is fine (updates are hand-delivered or not offered). Do **not** advertise auto-update
  as a live channel until HTTPS hosting is stood up (tracked as a GA task, not a pilot blocker).

---

## 5. Installer, first-launch, upgrade, uninstall

### 5.1 Build & sign the installer
- Script: `installer/Occountant.iss` (Inno Setup 6). Produces `Occountant-<ver>-Setup.exe` with an
  ARP entry, Start-Menu shortcut, version metadata, and an `AppMutex` (blocks install while running).
- Build it on a machine with **Inno Setup 6** (`iscc`) — not available in the dev MSYS2 shell.
- **Authenticode-sign** the installer **and** the bundled exe with the production certificate (EV
  recommended for SmartScreen reputation). Pipeline: `tools/sign-authenticode.sh`.

### 5.2 Verify first-launch experience
On a clean VM, run the signed installer, then launch. Confirm:
- The **onboarding wizard** appears (business name/address/tax number/currency/fiscal-year/language).
  It **writes settings only — it authors no accounting events** (verified by `ACCT_PILOT`). Only
  business name is mandatory; everything else defaults.
- After Create (or explicit Skip), the app opens with an empty, balanced book: chart of accounts and
  default tax codes auto-bootstrap, every screen shows an empty state.
- Set company identity during onboarding so the **first exported invoice carries a complete header**.
- Switch language to Arabic once and confirm RTL renders (this also confirms catalogs deployed).

### 5.3 Verify the upgrade path
Automated coverage: `bash tools/installer-test.sh` (upgrade preserves data, **downgrade refused**,
uninstall preserves data, portable mode). On the VM, additionally do the human version:
1. Install pilot build **N**, create a customer + a posted invoice + a payment, close the app.
2. Install build **N** again (or **N+1** when you have one) over the top.
3. Relaunch → the customer, invoice, and payment are **all still present**; trial balance still 0.
4. Confirm the app refuses to **downgrade** onto newer books (compatibility governance) rather than
   silently reinterpreting them.

### 5.4 Verify uninstall keeps business data
This is a trust cornerstone for a pilot — a user must never fear that removing the app deletes their
books.
1. With data present, run **Uninstall** from ARP / Start Menu.
2. Confirm the program files are removed **but** the data directory (books, `audit.log`, backups)
   under the user profile is **left intact**.
3. Reinstall and relaunch → the previous books open (no onboarding wizard, because a company exists).

> `installer-test.sh` proves the uninstall-preserves-data contract in CI; the VM step confirms it on
> the real Inno artifact with the real data location.

---

## 6. Full verification battery (run before every pilot cut)

All must be green on the exact commit you ship. Run from `occountant/`:

| Gate | Command | What it proves |
|---|---|---|
| build | `cmake --build build --target AccountingQuick` | 0 errors |
| persistence/crash | `bash tools/ptest.sh` | round-trip + real cross-process crash recovery (journal, migration, period close, void, allocation, ledger, snapshot, atomic txn) |
| interaction | `bash tools/itest.sh` | real QML paths, VM state, no QML runtime errors, EN↔FR↔AR |
| fuzz | `bash tools/fuzz.sh` | structure-aware mutation + property suite on histories |
| compatibility | `ACCT_COMPAT_VERIFY=1 …/AccountingQuick.exe` | replay-equivalence (projection == history) |
| accounting correctness | `ACCT_HOSTILE=all …` | payments/expenses/period ledger correctness, 0 findings |
| pilot safety/export | `ACCT_PILOT=all …` | backup/restore refusal, onboarding authors nothing, invoice PDF/CSV, statements, trust |
| commercial | `ACCT_C2TEST=all …` | licensing, **update signature verify + tamper reject**, support-bundle allowlist |
| i18n | `bash tools/i18n-check.sh` | 7 checks; catalogs fully cover EN/FR/AR + RTL |
| clean-room | `powershell -File tools/cleanroom.ps1` | deploy tree is self-contained |

(Isolated env vars — `ACCT_DATA_DIR=<fresh dir>` — for the direct-exe gates. `tools/*.sh` handle this
for you.) Attach the consolidated log to the release record.

---

## 7. Pilot Release Checklist (fill for every handover)

```
Pilot Release — Occountant
Commit:            __________  (git short)     Dirty: [ ] no  (must be "no")
Channel/BuildID:   __________ / __________
Date / Operator:   __________ / __________

BUILD & DEPLOY
[ ] Clean tree, intended commit, BuildInfo ACCT_GIT_DIRTY=0
[ ] AccountingQuick built (0 errors)
[ ] deploy_quick run → DLL closure complete (deploy-deps exit 0)
[ ] Customer artifact built with -DACCT_DEV_SIGNING=OFF (secret key absent)

VERIFICATION BATTERY (attach log)
[ ] ptest  [ ] itest  [ ] fuzz  [ ] compat_verify  [ ] hostile=all
[ ] pilot=all  [ ] c2test  [ ] i18n-check  [ ] cleanroom.ps1 = DEPLOYABLE

INSTALLER (clean Win10 + Win11 VM)
[ ] Installer built from Occountant.iss (Inno Setup 6)
[ ] Installer + exe Authenticode-signed (production cert)
[ ] Fresh install → onboarding wizard appears; company set; app opens balanced
[ ] Arabic once → RTL renders
[ ] Create customer + posted invoice + payment; trial balance 0
[ ] Reinstall over the top → all data present; downgrade refused
[ ] Uninstall → program removed, BOOKS + BACKUPS INTACT
[ ] Reinstall → previous books reopen (no onboarding)

HANDOVER
[ ] Customer docs included (docs/user/: Quick Start, FAQ, Backup, Restore, Support)
[ ] Backup location + restore-needs-restart explained to the user
[ ] Feedback channel + support contact given (docs/customer-feedback.md)
[ ] Trial length + license terms communicated
[ ] Commit tagged; artifact hash recorded
```

---

## 8. What is explicitly NOT in a pilot build

Per the phase constraints, none of these ship without **multi-user evidence** from the pilot:
inventory, POS, cloud sync, AI features, ERP modules, or any change to the deterministic accounting
core. A pilot build is the tested engine plus the delivery/trust surface — nothing more.
