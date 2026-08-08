# Release Engineering, Deployment & Distribution (Phase C3)

This is the layer that turns the Occountant accounting engine into an installable, updatable,
supportable, releasable Windows product. **No accounting semantics live here.** Everything in this
document sits *above* `StorageService` — in `app/` (C2 layer), `tools/`, and `installer/` — and the
engine (EventLog, replay, governance, compatibility, ledger, tax, statements) is untouched and
remains byte-identical and replay-equivalent across releases.

Product: **Occountant** · Publisher: **RIO&JHK Technologies Co.** · Installer: `Occountant-<version>-Setup.exe`.

---

## 1. Architecture

Every transition in the release lifecycle is deterministic and network-free:

```
Developer → Build → Sign → Package → Publish → Install → Activate → Update → Backup → Recover → Support
```

| Stage | Owner | Where |
|---|---|---|
| Build | CMake + `app/BuildInfo` (embedded provenance) | `CMakeLists.txt`, `app/BuildInfo.{h,cpp}` |
| Sign | payload sig (HMAC, v1) + Authenticode (exe/installer) | `app/Signature`, `tools/sign-authenticode.sh` |
| Package | one command → installer + portable ZIP + manifests | `tools/release.sh`, `tools/stage-runtime.sh`, `installer/Occountant.iss` |
| Publish | copy `dist/release/<v>-<chan>/` to the download host | (out of tree; artifacts are self-verifying) |
| Install | Inno Setup installer, ARP, shortcuts, upgrade path | `installer/Occountant.iss` |
| Activate | licensing (trial/grace/editions, signed tokens) | `app/LicenseManager` |
| Update | check → verify sig → stage → apply on restart, per channel | `app/UpdateManager` |
| Backup | scheduled + manual restore points | `app/BackupScheduler` |
| Recover | crash-consistent journal replay + staged restore | engine + `main_quick` startup |
| Support | one `SupportBundle.zip`, no accounting data | `app/SupportBundle` |

Identity is defined in exactly one place — `app/AppInfo.h` — so the installer, updater, crash
reporter, support bundle, and About screen can never disagree about version, channel, or vendor.

---

## 2. Build reproducibility & `BuildInfo`

`app/BuildInfo` embeds the build's provenance as compile-time constants and emits it as
`BuildInfo.json`:

```
product, vendor, version, versionCode, channel, buildId,
gitCommit, gitDirty, buildTimestamp, buildEpoch, compiler, qtVersion, platform, arch, abi
```

- **The timestamp is the git COMMIT time, never a wall clock.** CMake captures `git show -s
  --format=%ct HEAD` and passes it as `-DACCT_BUILD_EPOCH`; `BuildInfo` formats it as UTC ISO-8601.
  So the recorded metadata for a given commit is reproducible and diff-stable.
- `channel` and `buildId` are stamped by `release.sh` via `-DACCT_RELEASE_CHANNEL` /
  `-DACCT_RELEASE_BUILD_ID`; a bare local build falls back to `dev`/`stable`.
- Emit it headlessly with `ACCT_BUILDINFO=<path>`; `stage-runtime.sh` lays a copy next to the binary.
- Reproducibility boundary: the recorded *metadata* is deterministic. Byte-identical *binaries*
  additionally require a fixed toolchain and `-ffile-prefix-map`; that is out of scope for C3 and
  documented as a known gap.

---

## 3. Release channels

Four channels, ordered by instability: **Development ⊃ Beta ⊃ RC ⊃ Stable**. A user tracking a
less-stable channel also receives everything from the more-stable ones (a Beta user sees Beta + RC
+ Stable builds), because a more-stable build is always safe to offer.

- The channel a build was cut for lives in `BuildInfo`/`AppInfo` (compile-time).
- The channel a user *tracks* is a runtime setting (`SettingsViewModel.updateChannel`, persisted to
  `QSettings update/channel`), defaulting to the build's channel.
- `UpdateManager::check()` offers a build only when `appinfo::channelVisibleTo(buildChannel,
  userChannel)` **and** it is newer. Switching channel re-gates immediately (`updateChannelChanged`
  → `setChannel` → `check`).
- Manifests carry a `channel` field; a manifest without one is treated as `stable` (visible to all),
  so old manifests keep working.

---

## 4. Signing

Two independent signatures, by design:

1. **Update-payload signature** (`app/Signature`, HMAC-SHA256 v1 → Ed25519 v2). The updater verifies
   the payload bytes against the manifest `sig` before staging; a one-byte tamper is rejected.
   `release.sh` computes it via `ACCT_SIGN=<file>` and writes it into `manifest.json`.
2. **Authenticode** (`tools/sign-authenticode.sh`) over the exe + installer, in three tiers selected
   purely by environment — no prompts:
   - **Unsigned** (development) — no `OCCOUNTANT_SIGN_CERT`: files unchanged, exit 0.
   - **Test certificate** — a self-signed `.pfx`: exercises the whole pipeline offline.
   - **Production certificate** — the real Authenticode `.pfx` (EV/HSM), plus
     `OCCOUNTANT_SIGN_TS_URL` for an RFC3161 countersignature.

   `release.sh` Authenticode-signs the installer *before* computing checksums and the update
   signature, so what ships is exactly what was hashed and signed.

---

## 5. Packaging — one command

```
bash tools/release.sh [--version X.Y.Z] [--channel stable|rc|beta|development] [--build-id ID]
                      [--skip-gates a,b,c] [--no-build] [--gates-only] [--package-only]
                      [--smoke] [--out DIR]
```

Pipeline: **build (stamped) → gates (fail-fast) → stage clean tree → package → [smoke]**. Output
lands in `dist/release/<version>-<channel>/`:

| Artifact | Purpose |
|---|---|
| `Occountant-<v>-Setup.exe` | Inno installer (produced only if `iscc` is present) |
| `Occountant-Setup.exe` | stable, unversioned alias of the signed installer (for hardcoded links / smoke) |
| `Occountant-<v>-portable.zip` | portable app (staged tree + `Occountant.portable` marker) |
| `BuildInfo.json` | embedded provenance (copy) |
| `RELEASE_NOTES.md` | release notes (copied from repo root if present, else generated) |
| `manifest.json` | **updater** manifest: version, versionCode, channel, payload, size, **sig** |
| `release-manifest.json` | human/index manifest: every artifact with size + sha256 |
| `SHA256SUMS` | checksums for all artifacts |

**Graceful degradation** (matches "Sign if key available"): the portable ZIP, manifests, and
checksums are always produced with only the repo toolchain. The Inno installer is built only if
`iscc` is found; Authenticode signing runs only if a cert is configured. Missing *optional* tools
are a warning; a failing *gate* is always fatal.

`tools/stage-runtime.sh` assembles the one clean runtime tree (`dist/Occountant`) both the installer
and the portable ZIP are built over, so they carry byte-for-byte the same payload. It:

- runs `windeployqt` (wired as an `AccountingQuick` POST_BUILD step) + `tools/deploy-deps.sh` to
  collect Qt's DLLs/plugins **and** the MinGW third-party closure `windeployqt` leaves behind;
- ships the application's own **`App` QML module** (`App/qmldir`). This is load-bearing:
  `main_quick.cpp` does `loadFromModule("App","Main")`, and although the QML is embedded in the exe
  (`prefer :/App/`), the engine still *discovers* the module through the on-disk `App/qmldir`. Omit
  it and the app exits `-1` with an empty root object. Staging fails loudly if it is missing;
- enforces an **exclusion guard**: staging aborts if `license_gen*`, `*.key`, `*.pem`, `*.pfx`, or
  other private-key material ever lands in the shipping tree. (Belt-and-suspenders — the release
  build already sets `-DACCT_DEV_SIGNING=OFF`, so `license_gen` isn't built and no signing secret is
  compiled into the binary. See §4 and §12.)

### 5.1 Install smoke test — `tools/smoke-install.ps1`

The end-to-end acceptance of the *shipped artifact*, run the way a customer experiences it:

```
fresh environment → INSTALL → LAUNCH → create EMPTY COMPANY → ACTIVATE LICENSE → (uninstall preserves data)
```

```
powershell -ExecutionPolicy Bypass -File tools/smoke-install.ps1        # auto-discovers dist/*Setup.exe
bash tools/release.sh --smoke                                            # package, then smoke it
```

| Phase | Assertion |
|---|---|
| **Install** | `Occountant-Setup.exe` installs silently (`/VERYSILENT /DIR=<scratch>`) into an isolated scratch prefix and lays down `AccountingQuick.exe` + its full runtime closure. (Elevated like any Program Files install: silent on an already-elevated CI runner; one UAC prompt on an interactive non-admin box. The shipped installer is unchanged.) |
| **Launch + empty company** | the *installed* exe runs with a **stripped PATH** (System32 only → proves self-containment), provisions fresh empty books (`totalCount=0`, `*.dat` written) and auto-issues a 30-day **Trial** (read from the health line in `<data>/logs/occountant.log`). |
| **Activate license** | a vendor **Business** key minted by the developer-only `license_gen` (Ed25519 private key) is installed exactly as *Enter license key* does — the `OCCLIC-` token written to `<config>/license.key` — and on relaunch the installed **release** binary verifies it with only its embedded *public* key and flips Trial→Business. |
| **Uninstall** | the install tree is removed but the data dir (books) and config dir (license) survive — an uninstall never destroys accounting records. |

**Degradation, matching the rest of the pipeline:** with no Inno Setup on the machine, the test runs
against the staged tree (the byte-identical payload the installer ships) copied into a scratch
"install" dir, and says so. If no signing generator/key is available, license activation is reported
`SKIPPED` (a warning, not a failure) unless `-StrictLicense` is given; pass a pre-minted key with
`-LicenseKey OCCLIC-…`. A real elevated Program Files install is `-IntoProgramFiles`.

This is the only check that exercises the *staged/installed* tree rather than `build/`; it is what
caught the `App`-module staging gap above (which `tools/cleanroom.ps1`, defaulting to `build/`,
could not).

---

## 6. Automatic release validation (gates)

`release.sh` runs these fail-fast — the **first** failure aborts the release and nothing is packaged:

`ptest` · `itest` · `fuzz` · `perf` · `c2test` · `compat-verify` · `security-gate` · `i18n-check` ·
`shots` · `upgrade-test`.

- `compat-verify` (`ACCT_COMPAT_VERIFY=1`) proves history reconstructs to the same accounting
  meaning — the guarantee that the engine is **replay-equivalent** across the release.
- `security-gate` proves no data file (name *or* bytes) can enter the support/crash artifacts.
- Any gate can be skipped for a hotfix with `--skip-gates`, but that is an explicit, logged choice.

---

## 7. Support workflow

`SupportBundle.zip` (`app/SupportBundle`, or `ACCT_SUPPORT_BUNDLE=<path>`, or the Diagnostics
screen) is composed by **allowlist** from operator-facing signals only:

```
buildinfo.json · environment.txt · startup-diagnostics.txt · compatibility-report.txt
logs/*.log (money redacted at write time) · crash/*.zip (versions + modules only) · bundle-manifest.txt
```

The guardrail is structural: only `*.log` / `*.zip` / `*.txt` are pulled from the logs and crash
dirs, and never the data dir. No `*.dat`, `compat.manifest`, journal, customer, invoice, payment, or
ledger byte can enter it. Nothing is transmitted — the user chooses whether to send the file.

Support flow: reproduce → **generate SupportBundle** → user attaches it → triage from `buildinfo`
(exact commit/channel) + `startup-diagnostics` (storage/license/verification health) + `crash`
(stack + governance line).

---

## 8. Recovery workflow

Data lives in `%LOCALAPPDATA%\Occountant`, entirely separate from the install dir, so install /
upgrade / uninstall cannot lose books.

- **Crash recovery:** an interrupted write is healed by journal replay on the next launch
  (`ptest`/`upgrade-test` prove this across a real process kill).
- **Staged restore:** `BackupViewModel.restore()` stages a chosen backup under `.pending-restore/`;
  the swap happens only at a cold start, so it is crash-consistent.
- **Interrupted update:** a partial staged update is either a complete signed bundle or is cleaned
  on startup; the live DB is never touched.
- **Downgrade refusal:** if books were written by a newer build, an older build **refuses to open**
  them rather than risk reinterpreting history — mirrored by the installer's downgrade refusal.

---

## 9. Release checklist

1. `git` clean at the release commit; bump version in `app/AppInfo.h` if needed.
2. `bash tools/release.sh --version X.Y.Z --channel <chan>` — must end `release: OK`.
3. Confirm `dist/release/<v>-<chan>/`: installer (if building for release), portable ZIP,
   `SHA256SUMS` verifies, `manifest.json` sig round-trips.
4. Authenticode: set `OCCOUNTANT_SIGN_CERT` (+ `_PASS`, `_TS_URL`) and re-run, or sign artifacts
   with `tools/sign-authenticode.sh`.
5. Smoke-test the installer on a clean VM: install → run → upgrade over it → uninstall (data
   survives) → downgrade attempt (refused).
6. Publish `manifest.json` + payload to the channel's update source; publish artifacts + checksums.

---

## 10. Incident response

- **Bad release shipped:** publish a *higher* versionCode on the same channel; the updater rolls
  users forward. Never delete history; never ship a lower versionCode (the downgrade gate blocks it).
- **Signing key compromised:** rotate the key (v2 Ed25519 keeps the public key in the app, private
  key with the vendor); bump and re-sign; old signatures stop verifying.
- **Corruption report:** ask for a `SupportBundle.zip`; `compatibility-report.txt` +
  `startup-diagnostics.txt` classify the books and whether verification passed, with zero exposure of
  the customer's data.

---

## 11. Regression coverage (required verification)

| Guarantee | Test |
|---|---|
| installer upgrade preserves data | `tools/installer-test.sh` §1, `tools/upgrade-test.sh` |
| installer downgrade refusal | `installer-test.sh` §3 + `ACCT_C2TEST` #15 (`appinfo::isDowngrade`) |
| uninstall preserves data | `installer-test.sh` §2, `smoke-install.ps1` phase 4 |
| staged/installed tree is self-contained + launches | `smoke-install.ps1` phase 1–2 (clean PATH) |
| license activation on the installed binary | `smoke-install.ps1` phase 3 (Trial→Business) |
| shipping tree excludes `license_gen`/keys | `stage-runtime.sh` exclusion guard |
| portable mode works | `installer-test.sh` §4 |
| channel switching | `ACCT_C2TEST` #12 |
| release manifest verification | `tools/release-test.sh` §3 (SHA256SUMS + metadata) |
| signed package verification | `ACCT_C2TEST` #13, `release-test.sh` §3 (sig round-trip) |
| support bundle generation | `ACCT_C2TEST` #14 (+ no accounting data) |
| release script failure paths | `release-test.sh` §1–2 (fail-fast, nothing packaged) |
| deterministic build metadata | `ACCT_C2TEST` #11 |

---

## 12. Constraints honoured

Does **not** modify EventLog, StorageService, replay, governance, compatibility, ledger, tax, or
financial statements. No cloud, no SaaS, no telemetry, no analytics. Everything is local-first,
deterministic, and works with no internet. The accounting engine remains byte-identical and
replay-equivalent; C3 is strictly the distribution layer above it.
