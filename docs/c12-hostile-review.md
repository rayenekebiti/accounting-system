# C12-R — Hostile Product Integrity Review

**Stance:** senior engineer trying to prove the previous work wrong before the first external Early
Access customers. Not a validation pass. Assume tests have blind spots; try to break the system
logically and operationally. Every finding has reproduction + evidence. Confirmed defects were fixed;
the accounting engine, storage, replay, posting, tax, governance, and compatibility systems were **not**
touched.

**Result: 9 findings (2 High, 4 Medium, 3 Low). All confirmed defects fixed and regression-gated.**
Two deeper items are documented as pre-GA (not Early-Access blockers).

---

## Reviewed areas
1. i18n of customer-facing support (Early Access notice, Support Center, ticket creation, diagnostics
   export, error messages).
2. Hostile update-infrastructure audit (check → download → verify → stage → restart → apply → rollback;
   12 break scenarios).
3. Licensing/update interaction.
4. Commercial trust review.

---

## Findings

### F1 — Status labels don't retranslate on a live language switch — **HIGH** — FIXED
- **Area:** 1. **Impact:** a FR/AR user who switches language while looking at Invoices/Payments sees
  the status **badges** (Draft/Posted/Paid/Overdue/Void) — and Support ticket status — **stuck in the
  previous language**. Core, customer-facing screens.
- **Reproduction:** open **Invoices** in English (badges read "Draft"/"Paid"), switch to Arabic via the
  top-right language switcher → badges stay English until the list rebuilds.
- **Evidence:** `StatusBadge.qml:32` `text: root.statusLabel(root.status)` is a binding; `statusLabel`
  contains `qsTr` but the binding depends only on `status`. Qt's `retranslate()` re-evaluates only
  bindings whose text contains `qsTr` *directly*, so a `qsTr`-in-a-function binding is never refreshed.
  A scan found **6** such helpers lacking an `i18n.language` dependency.
- **Root cause:** missing language dependency in translated helper functions (the same reason
  `AboutScreen.licenseLabel` deliberately reads `i18n.language`).
- **Fix:** added a bare `i18n.language` reference inside `StatusBadge.statusLabel`,
  `SupportCenter.statusLabel`, `ExpenseEditor.fieldError`, `TaxCodeEditor.fieldError`, and
  `PeriodCloseScreen.friendly`.
- **Regression test:** **new `i18n-check` rule 8** — flags any non-`on*` QML function that contains
  `qsTr(` but not `i18n.language`. This guards the whole codebase against the entire bug class, not
  just today's instances. (The existing language-switch itest had a blind spot here: it checked that
  editor fields translate, but never asserted a *status badge* retranslated.)

### F2 — Reward eligibility note shown to the customer in English only — **MEDIUM** — FIXED
- **Area:** 1/4. **Impact:** the "Your reports" list rendered `★ Valuable feedback — Eligible: 19%
  discount for 6 months (manual grant pending)` — internal English operator wording surfaced to an
  AR/FR customer (mixed-language, unprofessional, and an English-only fallback in customer-facing
  support).
- **Reproduction:** mark a ticket valuable → view Support Center → Your reports in FR/AR.
- **Evidence:** `SupportCenterViewModel::markValuable` stores an English `QStringLiteral`;
  `SupportCenter.qml` embedded it via `.arg(modelData.rewardNote)`. **Blind spot:** `i18n-check` scans
  QML + a few VMs, but **not** `SupportCenterViewModel.cpp`, so the English literal was invisible to it.
- **Fix:** the UI now shows a fully-translated line (`★ Your feedback was marked valuable — thank
  you.`); the detailed note stays **internal** operator metadata (still recorded for manual granting).
- **Regression:** `i18n-check` coverage; `ACCT_EARLY_ACCESS` still asserts the operator eligibility
  record exists.

### F3 — Trust dashboard "Updates" row is hardcoded English — **MEDIUM** — FIXED
- **Area:** 1/4. **Impact:** the **Trust** panel (the app's reassurance surface) showed the update
  status in **English only** for FR/AR users — undermining trust exactly where it's meant to build it.
- **Reproduction:** FR/AR user → Ledger → Trust → the *Updates* row reads English.
- **Evidence:** `TrustDashboard.qml:106` `value: platform.updateStatusText`; `updateStatusText` is built
  from untranslated C++ `QStringLiteral`s in `PlatformController.cpp` (no `tr()`).
- **Fix:** `TrustDashboard` now composes the status from the update **state** with direct `qsTr` (which
  retranslates), never the raw English property.
- **Regression:** `i18n-check` coverage + rule 8 (direct qsTr binding).

### F4 — Support "Submit report" fails silently — **LOW** — FIXED
- **Area:** 1. **Impact:** if the local ticket write fails (disk full / permissions), `submitReport`
  returned `"error"` but the UI showed **nothing** — the user believes their report was filed when it
  wasn't.
- **Reproduction:** make `<dataDir>/support` unwritable, submit a report → previously no feedback.
- **Fix:** the form now shows a translated error (`Couldn't save your report. Please check your disk
  space and try again.`) when the result is `"error"`.
- **Regression:** the failure path is now visible; `i18n-check` covers the string.

### F5 — "Restart to install" is false; the same update re-offers in a loop — **HIGH** — FIXED (message) + documented (deeper)
- **Area:** 2/4. **Impact:** the biggest trust defect. The user is told **"Update X is ready — restart
  to install."** but v1's `applyPendingAtStartup` is a **stub**: it verifies and **clears** the staged
  bundle **without replacing the binary**. After restart, `check()` re-detects the update and offers it
  **again** — the version never changes. A user can loop forever, or believe they upgraded when they
  didn't.
- **Reproduction:** About → Updates → Download & stage → "restart to install" → restart → `main_quick`
  applies (clears staging, no install) → About shows "Version X is available" again. Version unchanged.
- **Evidence:** `UpdateManager::applyPendingAtStartup` v1 comment ("verify + record + clear staging so
  the app proceeds on the current build"); call site `main_quick.cpp:236`; `AboutScreen.qml`
  "restart to install".
- **Fix (this phase):** honest messaging in **About** and **Trust** — *"Update X is downloaded and
  verified. Run the Occountant installer to finish updating."* (v1's payload **is** the installer; the
  user runs it). No more false "restart installs" promise.
- **Deeper (documented, pre-GA — NOT fixed here, out of scope):** wire the real installer hand-off
  (launch the staged setup + quit, per the design note in `UpdateManager.cpp`) so the in-app flow
  actually installs. For **Early Access this is acceptable**: updates are operator-delivered and the
  messaging is now truthful.
- **Regression:** itest (no QML errors); the message is now accurate and localized.

### F6 — Update errors are not surfaced to the user — **MEDIUM** — FIXED
- **Area:** 2. **Impact:** on a failed/tampered update (signature verification fails, download fails),
  `UpdateManager` enters `Error` state, but About showed the **generic** "Updates are checked
  locally…" — no indication anything went wrong.
- **Reproduction:** point the update source at a payload with a bad signature → state `Error` → no
  user-visible error previously.
- **Fix:** About + Trust now show a reassuring, translated error on `Error` state: *"We couldn't
  complete the update. Your current version and your data are unaffected — please try again later."*
- **Regression:** `i18n-check`; the underlying safety (tampered updates rejected) is already proven by
  `ACCT_C2TEST` (Ed25519 verify + tamper reject).

### F7 — `rollbackStaged` ignored removal failure (falsely reported success) — **LOW** — FIXED
- **Area:** 2. **Impact:** if the staged bundle couldn't be removed (locked file), `rollbackStaged`
  still returned `true` and set state `Available`, while the bundle **remained** and could be applied on
  the next start.
- **Evidence:** old `UpdateManager::rollbackStaged` discarded `removeRecursively()`'s result.
- **Fix:** honor the result — on failure, set `error_` and return `false`.
- **Regression:** `ACCT_C2TEST` "valid update stages then rolls back cleanly" still passes (29/0).

### F8 — Update manifest metadata is not signed (only the payload) — **LOW** — documented
- **Area:** 2. **Impact:** an attacker with write access to the update **source** could alter cosmetic
  manifest fields (displayed version string, notes) but **cannot** cause a forged payload to be applied
  — the payload requires a valid **Ed25519** signature against the embedded public key.
- **Evidence:** `UpdateManager` verifies `sig` over the payload bytes; the manifest's own fields are not
  signed.
- **Disposition:** LOW; the update source is a **local path** in Early Access. **Recommend** signing the
  manifest too before public HTTPS hosting. Not fixed (no exploit path to a forged install; would be
  speculative for Early Access).

### F9 — Licensing/update interaction: no corruption path (verified) — **INFO**
- **Area:** 3. **Findings:** the license (`configPath/license.key` + cache) and updates
  (`configPath/updates/pending`) are **separate**; v1 `applyPendingAtStartup` touches neither, and the
  production installer replaces **program files**, not the user-profile config → license is preserved.
  Expired / trial / business license and updates are **decoupled** (you can check/stage updates in any
  license state; updates never write the license file).
- **Disposition:** no defect. Verified by inspection. **Recommend** a regression assertion (license file
  bytes unchanged across a stage/apply cycle) **when** the real installer hand-off lands (F5 deeper);
  adding it now, against a no-op apply, would be speculative.

---

## Hostile questions — answered with evidence

| Question | Answer | Evidence |
|---|---|---|
| Can a failed update leave Occountant unusable? | **No.** v1 apply never replaces the running binary and never touches the data dir; corrupt/interrupted updates are discarded. | `applyPendingAtStartup` completeness check; `ACCT_C2TEST` (interrupted/forged → not staged; interrupted staging recovered on startup, live DB untouched). |
| Can an older version overwrite a newer one? | **No.** In-app updater only offers `versionCode > current`; installer refuses downgrades. | `appinfo::isDowngrade`; `ACCT_C2TEST` downgrade gate. |
| Can a corrupted package replace valid files? | **No.** Ed25519 signature must verify against the embedded public key before staging/apply; tamper rejected. (And v1 apply doesn't replace files.) | `ACCT_C2TEST` Ed25519 + signed-package tamper tests. |
| Is rollback guaranteed? | **Now honestly reported** (F7). "Rollback" = remove the staged bundle; failure is reported, not hidden. Nothing at the binary level to roll back in v1. | F7 fix; `ACCT_C2TEST` rollback test. |
| Are error messages understandable? | **Now yes** (F5, F6) — honest, reassuring, localized EN/FR/AR. | AboutScreen/TrustDashboard fixes; `i18n-check`. |

### 12 break scenarios — disposition
1. Corrupted package → rejected (sig). ✅ 2. Interrupted download → tmp discarded. ✅
3. Interrupted staging → incomplete bundle discarded on startup. ✅ 4. Power loss during apply → v1
apply is a no-op (never replaces binary/data) → nothing to corrupt. ✅ (F5 documents the deeper gap).
5. Failed rollback → now honestly reported (F7). ✅ 6. Downgrade → refused. ✅ 7. Version mismatch →
versionCode/channel gate. ✅ 8. Missing files → completeness check discards. ✅ 9. Modified manifest →
payload signed; metadata tamper only cosmetic (F8). ✅ 10. Signature bypass → impossible
(empty/short/tampered/wrong-key rejected). ✅ 11. Wrong channel → `channelVisibleTo` gate. ✅
12. Close app during update → partial staging discarded next start. ✅

---

## Area 4 — Commercial trust review

*"If I trust my invoices and accounting data to Occountant, what could make me lose confidence?"*

- **Misleading update flow (F5)** — "restart to install" that didn't → **fixed** (honest messaging).
- **English in the Trust panel (F3)** and **English reward note (F2)** — trust surfaces speaking the
  wrong language → **fixed**.
- **Silent report failure (F4)** — → **fixed**.
- **Dangerous default (carried from C11):** the invoice **Status** dropdown lets a user hand-set
  **"Paid"** without recording a payment (AR vs. payment desync). **Not** changed (borders on posting
  semantics, out of scope); covered in onboarding ("record a payment; don't hand-set Paid"). Logged.
- **Reassurance that holds up:** the diagnostics bundle clearly states "app health only — never your
  accounting data"; offline/private messaging is honest; the Trust dashboard (now localized) is a
  genuine confidence surface.

---

## Verification (full battery)

Run after the fixes: **build · ptest 268/0 · itest 123/0 · fuzz 13/0 · compat replay-equivalence PASS ·
hostile 0 findings · pilot 67/0 · early_access 24/0 · c2test 29/0 · security PASSED · i18n PASSED
(625/625, incl. new rule 8) · cleanroom DEPLOYABLE.** (Reconciled against `/tmp/c12r_out.txt`.)

The engine/correctness/safety gates are unchanged (no engine files touched); the changes are UI
messaging, i18n retranslate dependencies, one `app/UpdateManager` robustness fix, and one new static
i18n guard.

---

## Final verdict

**B) Ready after fixes.**

This review was supposed to prove the product wasn't as ready as assumed — and it did: it surfaced
**two High-severity defects that would have shipped** (status labels frozen in the old language on live
switch; a "restart to install" flow that never installs and loops), plus English-only leaks in the
Trust panel and reward note, silent failures, and a dishonest rollback return. **Those are now fixed
and regression-gated** (notably a new `i18n-check` rule that guards the whole retranslate bug class).

With these fixes applied, Occountant is ready to onboard the **first external Early Access users**,
**conditional on** the honest, operator-delivered update posture (the in-app updater now truthfully
tells users to run the installer; a real self-install hand-off — F5 deeper — and manifest signing (F8)
are documented **pre-GA** items, not Early-Access blockers) and the standing commercial prerequisites
from earlier phases (production code-signing cert, license-key sales pipeline, finalized legal/pricing,
HTTPS update hosting). I do **not** claim "ready all along" (A): real, shippable defects existed. I do
**not** claim "not ready" (C): every confirmed defect is fixed and gated, and the core is untouched and
green.
