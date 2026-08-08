# Final Platform Hardening — Findings & Results

Closing the operational-risk gaps so the platform is distributable, diagnosable,
accessible, upgrade-safe, and long-session stable — *before* broader accounting
features. Everything below was verified under real runtime conditions, not inferred
from a successful build.

New gates:

```bash
bash tools/package-win.sh [version]        # → dist/AccountingPro-Setup-<v>.exe (NSIS)
powershell tools/cleanroom.ps1             # stripped-PATH smoke (7 checks)
bash tools/upgrade-test.sh                 # data/journal survive an upgrade restart
ACCT_DIAGTEST=1 AccountingQuick.exe        # warning-capture self-test
ACCT_ENDURE=<N> AccountingQuick.exe        # long-session memory + warning-drift
ACCT_A11Y=1 AccountingQuick.exe            # QAccessible tree audit
```

---

## 1. Installer & upgrade (objective #1)

**Built:** `tools/installer.nsi` + `tools/package-win.sh` → an NSIS installer
(`dist/AccountingPro-Setup-1.0.0.exe`, 34 MB). The packager completes the DLL
closure, stages a **clean runtime tree by allowlist** (no `*_autogen`, CMake files,
`.obj`, test scratch, the widgets exe, baselines, qmltooling — 1599 runtime files),
then runs `makensis`.

**Upgrade-safety model (the load-bearing decision):** the installer writes ONLY
`$INSTDIR` (Program Files) + `HKLM` registry. **User data lives in
`%LOCALAPPDATA%\AccountingPro`** (`QStandardPaths::AppDataLocation`), which the
installer never reads, writes, or deletes. So install / upgrade / uninstall cannot
lose books. On upgrade the previous version's uninstaller runs silently first (no
stale DLL/plugin shadows the new Qt runtime) — data-safe because data is elsewhere.
Uninstall intentionally **leaves user data** (uninstalling an app must not destroy a
business's records).

**Verified** (`tools/upgrade-test.sh`, without touching Program Files/registry —
faithful because an install only ever touches `$INSTDIR`+HKLM):

```
[PASS] persistence preserved across upgrade        (25 invoices survive the restart)
[PASS] journal recovered after upgrade restart     (crash mid-save → recovered=yes)
[PASS] books intact after crash+recovery           (still 25 invoices)
```

**Residual risk:** the system-level install (elevation, Start-Menu, ARP entry) was
built but not executed here (it modifies the machine + needs admin); a CI clean-VM
that runs the .exe and asserts the installed tree launches is the remaining step.

## 2. Automated clean-environment validation (objective #2)

`tools/cleanroom.ps1` strips `PATH` to bare Windows (no MSYS2/Qt/ucrt64), runs the
`ACCT_PROBE` harness, and asserts **7 smoke checks**: clean exit, startup+engine
load, persistence/invoice load, Arabic translation load, RTL applied, customer editor
opens, language list present. Hard-error dialogs suppressed + timeout-kill so a
missing dependency can never hang CI. **Result: all 7 pass → DEPLOYABLE.** This turns
last phase's manual stripped-PATH check into a repeatable regression gate.

## 3. Runtime warning capture (objective #3 — was mandatory)

`quick/diagnostics.cpp` installs a Qt message handler (FIRST in `main`, before any Qt
object) that **classifies** every framework/QML diagnostic — `binding` (TypeError,
ReferenceError, Unable-to-assign, binding loop), `i18n`, `focus`, `layout`,
`resource` (load/module failures) — **counts** it, **timestamps** it, tags it
**startup vs runtime**, and writes one **low-noise** line (consecutive duplicates
collapsed). It is a terminal sink (never routes through `qC*` → no recursion) and
**excludes our own `acct.*` logs** from defect counts, so "warning drift" measures
real problems, not our diagnostics. Optional `ACCT_DIAG_VERBOSE=1`.

**Verified** (`ACCT_DIAGTEST=1`): representative messages classify and count exactly
(`binding=2 i18n=1 layout=1 resource=1 other=2`, dedup collapses repeats, `acct.*`
excluded). It also caught **real** shutdown TypeErrors during the a11y audit (see §4).

## 4. Accessibility validation (objective #4)

**Honest boundary first:** NVDA/Narrator cannot run in this environment, so the
spoken experience is a required manual pass. What I *did* automate is stronger than
"`Accessible.name` is present in QML": `quick/a11y.cpp` (`ACCT_A11Y=1`) walks the real
**`QAccessible` tree a screen reader consumes** and asserts every interactive control
exposes role + name.

**Found & fixed:**
- Search fields announced as **unnamed EditableText** → added `Accessible.name`
  (now "Search invoices…" / "Search customers…").
- The audit surfaced **shutdown TypeErrors**: `i18n` (and VM) context objects were
  destroyed before the QML engine, so bindings re-evaluated against null during
  teardown. Root-caused to stack-destruction order and fixed by declaring
  `LocaleController` *before* the engine (it now outlives it; engine tears down while
  context objects are alive). Shutdown is now clean — verified by re-running the audit.

**Result:** 9 of 11 realized interactive controls named, **2 unnamed buttons remain**
(a documented field-level item) — and the spoken-experience NVDA/Narrator pass is
still outstanding. Dialog-internal controls aren't realized headlessly, so they need
the manual pass too.

## 5 & 6. Keyboard endurance & long-session stability (objectives #5, #6)

`quick/endure.cpp` (`ACCT_ENDURE=<N>`) drives the sustained workflows a keyboard
session triggers — open editor → fill → **save**, filter/search churn, **language
switching**, periodic customer add — measuring working-set memory and the
warning-drift counters, draining `DeferredDelete` before each sample.

**2000-cycle run:**

| Metric | Result |
|--------|--------|
| Saves | 2000 / 2000 succeeded, 0 failed |
| **Warning drift** | **0 → 0** (zero warnings across all save/filter/retranslate churn) |
| Memory | 154 → 196 MB, **plateaus at ~195 MB after cycle 750** (ramp settling, not a linear leak) |
| Verdict | **PASS** (failed==0, drift==0, growth<64 MB) |

The flat memory after the initial ramp + zero warning drift is the evidence for
"calm and predictable after hours." **Boundary:** this exercises the
VM/persistence/i18n layers; literal key-event focus traversal needs an exposed window
under `exec()` and remains a visual/manual check.

## 7. Observability (objective #7)

The startup line now carries the warning summary, and shutdown logs final counts so
**drift is visible from logs alone**:

```
[0.41s][up][info:acct.startup] AccountingPro 1.0 | Qt 6.10.1 | data=… (writable=yes) | lang=en dir=LTR | invoices=0 | startup: init 13ms + engine 373ms | diag: warn=0 crit=0 fatal=0 (…)
[t][rt][info:acct.startup] shutdown | diag: warn=0 …
```

Plus the existing `acct.storage/recovery/persistence/integrity` categories and the
non-writable-data-dir warning. The app is now diagnosable from logs without a debugger.

---

## Constraints honoured

Architecture, persistence guarantees, measured performance, mutation discipline,
no-object-churn (proven by zero warning drift over 2000 retranslates), and
Money-as-cents preserved. No speculative rewrites; the only structural change was
moving one declaration to fix teardown order.

## Remaining operational risks (before feature expansion)

1. **System-install clean-VM smoke** — run the actual `.exe` on a fresh Windows VM and
   assert the installed tree launches + upgrades. (Mechanics built; execution pending.)
2. **NVDA/Narrator manual pass** + name the 2 remaining buttons + dialog-internal
   field labels (`FieldInput`/`Select` `Accessible.name`).
3. **Literal keyboard focus-traversal test under `exec()`** (focus order, no traps).
4. **Code-signing** the installer/exe (SmartScreen/AV trust) before public release.

With these closed, the platform layer is production-grade and ready for Suppliers /
Products / Expenses / Reporting on a hardened base.
