# Operational Hardening — Findings & Fixes

The phase where the build stops being a developer artifact and becomes deployable
professional desktop software: it must run on a clean machine, be operable without a
mouse, be legible to a screen reader, and be diagnosable in the field. Everything
here was **verified against real runtime behaviour**, not assumed.

Gates added/extended this phase:

```bash
bash tools/deploy-deps.sh build   # complete the clean-machine DLL closure
# clean-room proof: run the exe with PATH stripped to bare Windows (see §1)
bash tools/itest.sh               # 36 interaction assertions, 0 QML errors
bash tools/i18n-check.sh          # new accessible strings translated (fr/ar)
```

---

## 1. Deployment — the headline finding (CRITICAL, fixed + proven)

> Premise: *“works on the dev machine” ≠ deployable.* Verified by running the exe
> with `PATH` stripped to `System32` only — no MSYS2.

**Result, before fix:** the app **failed to start** — exit `0xC0000135`
(`STATUS_DLL_NOT_FOUND`). `windeployqt` on MinGW/MSYS2 copies Qt's own DLLs, plugins
and QML modules, but **not** the MinGW third-party libraries they link
(ICU, harfbuzz, freetype, PCRE2, glib, zstd, libpng, libjpeg, libffi, …). On the dev
machine those silently resolve from `ucrt64/bin` on `PATH`; on any machine without
MSYS2 the process dies at load. **20 exe-level + further plugin/QML-module deps were
missing — the app was not shippable.**

**Fix:** `tools/deploy-deps.sh` walks the PE import tables (via `objdump -p`,
recursively) of the exe **and every deployed plugin / QML-module DLL**, and copies
the full `ucrt64` closure into the deploy dir. Wired as the CMake `deploy_quick`
target. It also recovered the QtQuick **style-module DLLs** that `--no-quick-import`
had skipped.

**Proof (after fix):** same stripped-`PATH` run → **exit 0**, QML engine loads,
embedded translations resolve, Arabic RTL (`layoutDir=1`) and French both work,
customer editor + live validation function. Clean-machine deployable, demonstrated.

**Engineering note (why the script looks the way it does):** `ldd` was abandoned —
MSYS2 `ldd` shells out to `ntldd`, which **reads stdin**, so batch scans were
non-deterministic (it silently scanned one file). `objdump -p` reads the import
table directly and is deterministic.

**Validation matrix run:**

| Check | Before | After |
|-------|--------|-------|
| Start with `PATH=System32` only | `0xC0000135` (dead) | exit 0, full UI |
| Embedded translations (`:/i18n`) load | n/a (didn't start) | en/fr/ar all resolve |
| Writable data path (AppData) | n/a | created, `writable=yes` |
| Plugin resolution (platform/imageformats/styles) | n/a | all load from deploy tree |

### Residual deployment risks (honest)

- **No installer yet.** The validated unit is the deploy *directory*. An NSIS/MSI/
  WiX installer + per-user vs per-machine writable-path choice is the next step.
- **`deploy_quick` is a manual target** (~1.5 min), not part of every build — dev
  builds stay fast but are *not* self-sufficient until it runs. CI/release must run it.
- **Upgrade/migration on a real install** is untested here (the storage layer's v2
  header + headerless migration is covered by `ptest`, but not an installed-bytes
  upgrade). 

---

## 2. Accessibility — audit & first implementation tranche

**Audit:** `Accessible.*` usage across all QML was **zero**. The custom controls
(`AppButton`, `IconButton`, `NavRail` items) are `Item` + `MouseArea` — which means
they were **invisible to screen readers AND impossible to operate by keyboard**
(a `MouseArea` handles only mouse; you cannot Tab to it or press it with Enter). For
the *primary button component*, used on every screen, this is a structural defect,
not a polish gap.

**Fixed this tranche (highest-leverage surface):**

| Component | Added |
|-----------|-------|
| `AppButton` | `Accessible.role=Button` + `name`, Tab focus (`activeFocusOnTab`), Enter/Space/press activation, visible focus ring (`Theme.color.focusRing`) |
| `IconButton` | same, plus a **required `accessibleName`** (a “✕” glyph is meaningless to SR) |
| `NavRail` items | role/name, `Current screen` description on the active item, keyboard activation + focus ring |
| Icon buttons at call sites | translatable names: **Close**, **Change language**, **Remove line %N** |
| `ConfirmDialog` | **had `focus` unset** → Esc/Tab/Enter didn't work in it; now grabs focus and seeds it on the *safe* button (Cancel for destructive actions) |
| `ModalSheet` | `initialFocusItem` hook so dialogs can land focus in their first field (the editors already do this themselves) |

The four new accessible strings were extracted (`lupdate`) and translated into FR + AR
so a screen reader announces them in the user's language; i18n gate stays green.

**Focus visibility:** a keyboard-only focus ring (shown on `activeFocus`, hidden for
mouse use) now exists on every fixed control — keyboard operators can always see
where they are.

**Verified:** build clean (qmlcachegen validates QML), `itest` 36/36 with zero QML
runtime errors across all screens, language switching, and editors.

### Accessibility — remaining (next tranche)

- **Form-field names.** `FieldInput`/`Select`/`SearchField` should expose
  `Accessible.role` + `name` bound to their visible labels, and link error text via
  description. (The editors already have strong Tab/Return key chains and initial
  focus — keyboard *traversal* is good; SR *labelling* of fields is the gap.)
- **Tab-order audit** across each screen, and list-row semantics (`Accessible.role`
  for invoice/customer rows).
- **Manual screen-reader pass** (NVDA / Windows Narrator) — an honest boundary:
  automated checks prove the semantics are *declared* and non-regressing, but only a
  real SR pass proves the *announced experience*. Same class of boundary as the
  Arabic caret/IME manual note in `docs/i18n.md`.

---

## 3. Keyboard & focus management

- The **editors already implement a real keyboard workflow** (documented in
  `InvoiceEditor.qml`): open → first field focused; Tab/Shift-Tab; Return advances
  fields and line-item cells; last cell → add line + focus it; Ctrl+S save; Esc
  (dirty-guarded). This was found working and preserved.
- The gap was **controls outside the editors** (buttons, nav, confirm dialog) — now
  keyboard-operable (§2), so a full create-invoice flow is mouse-free end to end.
- **Modal focus correctness:** `modal: true` traps focus within a dialog; Qt restores
  focus to the trigger on close. `ConfirmDialog` now actually participates (it didn't
  grab focus before). Initial focus is seeded safely.

### Remaining
- A scripted keyboard stress-test (rapid repeated editor open/save, filter switching,
  modal open/close cycles) under `exec()` to assert no focus dead-ends accumulate —
  the current `itest` runs headless without `exec()`, so it can't drive Tab traversal.

---

## 4. Runtime diagnostics & observability

Extends the `acct.*` logging categories (`quick/logging.h`). One **startup line**
(info, on the real-launch path only — not spam) makes a deployed install
self-describing for support:

```
acct.startup: AccountingPro 1.0 | Qt 6.10.1 | data=…\AppData (writable=yes) | lang=en dir=LTR | invoices=0 | startup: init 428ms + engine 763ms
```

- A non-writable data dir is surfaced loudly (`acct.storage` warning) — saves would
  otherwise fail silently.
- Startup crash-recovery already reports via `acct.recovery` (see `docs/persistence.md`).
- Off by default beyond info; enable per-area without a rebuild:
  `QT_LOGGING_RULES="acct.startup=true;acct.persistence=true"`.

### Remaining
- Route QML runtime warnings into an `acct.qml` category via a message handler for
  field diagnosability (today they go to the default Qt log).

---

## 5. Long-session stability

Carried from Phase 1 (measured, not assumed): the editor-open “leak” was shown to be
mostly the deferred-delete measurement artifact (~104 MB reclaimed by draining
`DeferredDelete`; ~50 KB/open residual). No new long-run regression was introduced —
the a11y additions are declarative properties + one focus-ring Rectangle per control,
no per-frame work, no object churn on retranslate. A dedicated multi-hour soak with
focus-correctness sampling remains a recommended next step.

---

## 6. Constraints honoured

Architecture, performance, persistence guarantees, single-mutation discipline,
no-object-churn-on-retranslate, and Money-as-cents were all preserved. No speculative
rewrites: the custom controls were *augmented* with semantics + key handling rather
than re-based onto `QtQuick.Controls` Button (which would have churned styling across
every screen). No framework abstraction expansion, no premature cloud/multi-user work.

---

## 7. Priority recommendations before broader feature work

1. **Make `deploy_quick` part of the release pipeline** + add an installer with a
   clean-VM smoke test (the clean-room run in §1, automated). Highest risk to “ships”.
2. **Finish field-level accessibility** + a manual NVDA/Narrator pass.
3. **Scripted keyboard/focus stress-test under `exec()`.**
4. **QML-warning capture** into the logging foundation.
5. Then resume business-entity feature work on this hardened base.
