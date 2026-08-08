# Phase C12 — Early Access Program, Feedback System & Learning Infrastructure: Final Report

**Scope:** build a professional early-access experience, an in-app support/feedback system, and a
local learning loop — **without** compromising the privacy-first, offline-first architecture, and
**without** touching the accounting engine. This was a real build phase, entirely above the engine.

**Evidence rule:** every claim cites what was run or produced.

---

## 1. Files changed

### New — data & app layer (above the engine)
- `app/SupportTicket.h` — local support-ticket value type (+ JSON), status enum
  Received→Reviewing→Confirmed→Fixed→Released.
- `app/SupportTicketStore.{h,cpp}` — file-backed local ticket store (`<dataDir>/support/tickets.json`,
  atomic writes). Creating/updating a ticket authors **no accounting events**.

### New — view models (Qt, above the engine)
- `quick/EarlyAccessViewModel.{h,cpp}` — welcome-notice state machine over QSettings (first launch /
  major-version / remind / suppress).
- `quick/SupportCenterViewModel.{h,cpp}` — file reports, attach privacy-safe diagnostics, expose the
  Support ID + ticket list, record reward eligibility.

### New — UI (QML)
- `quick/qml/EarlyAccessDialog.qml` — the Early Access welcome notice (Continue / Remind me later /
  Don't show again), professional and privacy-reassuring.
- `quick/qml/SupportCenter.qml` — Settings → Support Center (Support ID, diagnostics export, report
  form, local ticket list).

### New — test & docs
- `quick/early_access.{h,cpp}` — the `ACCT_EARLY_ACCESS` gate (24 checks).
- `docs/early-access-program.md`, `docs/support-system.md`, this report.

### Modified (no engine files)
- `CMakeLists.txt` — register new sources + QML.
- `quick/main_quick.cpp` — instantiate/register `earlyAccessVm` + `supportVm`; `ACCT_EARLY_ACCESS`
  dispatch.
- `quick/qml/Main.qml` — Early Access dialog instance + startup-show logic (only when onboarding
  isn't showing) + open-from-Settings.
- `quick/qml/SettingsWorkspace.qml` — new **Support** tab; forward the Early Access signal.
- `quick/qml/AboutScreen.qml` — **Early Access Program** card that reopens the notice.
- `quick/i18n/app_{en,fr,ar}.ts` (+`tools/i18n_fill.py`) — 59 new strings, fully translated EN/FR/AR.

**Reused from C10 (deliverable #3 already existed):** `app/SupportId.{h,cpp}` (stable non-PII
`OCC-XXXX-XXXX`) and `app/SupportBundle.{h,cpp}` (allowlist privacy-safe bundle) — surfaced in the new
Support Center, not rebuilt.

**Engine untouched:** no change to EventLog, storage format, posting, replay, governance,
compatibility, tax, or financial semantics.

---

## 2. Tests added

**`ACCT_EARLY_ACCESS` — 24 checks, all pass.** Verifies exactly the phase's required properties:
- **Notice does not appear repeatedly:** shows on first launch; **Continue** dismisses; acknowledgement
  persists across restart; a **major-version update** re-shows it; **Remind me later** keeps it for
  next launch; **Don't show again** suppresses it and persists.
- **Support ID stable:** well-formed `OCC-XXXX-XXXX`, identical across reads (persisted).
- **No accounting mutation:** filing a ticket, exporting diagnostics, and recording reward eligibility
  each author **NO** accounting events (audit head seq unchanged); **replay-equivalence and trial
  balance remain intact** before/after.
- **Bundle has no accounting data:** a seeded customer "leak canary" and all accounting data-file names
  are **absent** from the zip; the Support ID **is** present (for correlation).
- **Translation works:** the Arabic catalog loads from resources and an Early Access string is
  localized.

---

## 3. Discovered risks

| Risk | Assessment | Mitigation |
|---|---|---|
| Two overlays at once (Early Access + onboarding) | Real UX risk | Gated: the notice shows only when `!onboardingVm.needed` (fresh installs see onboarding first; the notice appears on the next, settled launch). |
| Support metadata living near the books | Privacy concern | Tickets are a **separate** local JSON file under `<dataDir>/support/`; they never enter EventLog and carry no accounting records. |
| Diagnostics bundle leaking data | The central privacy risk | Allowlist-only bundle; proven by `ACCT_EARLY_ACCESS` (leak canary), `security-gate.sh`, and `ACCT_C2TEST` — no data-file name or bytes escape. |
| Reward system implying auto-discounts | Commercial/expectation risk | Framework only: marking valuable records an **eligibility note**; **no discount is ever applied automatically** — documented in `early-access-program.md`. |
| i18n drift from many new strings | Regression risk | Full round-trip (lupdate + fill); `i18n-check` green; 616/616 EN/FR/AR translated. |

**No** accounting-correctness, data-safety, or crash risks were introduced — all engine/safety gates
remain green.

---

## 4. Full verification battery

| Gate | Result |
|---|---|
| build | PASS (0 errors) |
| ptest (persistence + crash recovery) | 268 / 0 |
| itest (real QML interaction, new screens instantiate) | 123 / 0 |
| fuzz | 13 / 0 (ROBUST) |
| compatibility (ACCT_COMPAT_VERIFY) | PASS |
| hostile (ACCT_HOSTILE=all) | 0 findings |
| pilot (ACCT_PILOT=all) | 67 / 0 |
| **early access (ACCT_EARLY_ACCESS)** | **24 / 0** |
| commercial (ACCT_C2TEST=all) | 29 / 0 |
| security-gate (no accounting data escapes artifacts) | PASSED |
| i18n-check | PASSED (616/616 EN/FR/AR) |
| cleanroom.ps1 (deploy-tree self-contained) | DEPLOYABLE |

(Full-battery summary reconciled against `/tmp/c12_out.txt`.)

---

## 5. Is Occountant ready for first external users?

**The system is ready to be put in front of first external users — with the same honest caveat that
has held since C9: product-market fit is unproven because no real customers have used it yet.**

**What is genuinely ready (built + gated this phase):**
- A **professional Early Access welcome** that appears sparingly (first launch / major update),
  reassures on privacy, and never nags — with **Continue / Remind me later / Don't show again**,
  reachable from About, fully localized EN/FR/AR + RTL.
- An **in-app Support Center**: categorized/severity-tagged problem reports stored **locally**, a
  privacy-safe **diagnostics bundle** (app health only), the anonymous **Support ID**, and a local
  ticket list with lifecycle status.
- A **feedback data model** ready for future support management, and a **reward eligibility** framework
  (manual, never auto-granted).
- Verified **privacy**: no telemetry, nothing transmitted, and proof (leak canary + security gate) that
  accounting data cannot escape a support bundle.

**What this phase does NOT claim:**
- It **prepares the system to collect feedback**; it does **not** invent customer feedback or claim
  product-market fit. Zero real external users have used it yet.
- Commercial prerequisites from earlier phases remain open before taking public money: production
  code-signing certificate, license-key sales pipeline, finalized legal/pricing, production Ed25519
  key + HTTPS update hosting.

**Verdict:** **Ready to onboard the first external Early Access users.** The onboarding, support,
feedback, and privacy machinery is complete, localized, and regression-gated, and the accounting core
is untouched and green. The open question remains **market validation**, which only real users can
answer — this phase gives them a professional way in and us a privacy-safe way to learn.
