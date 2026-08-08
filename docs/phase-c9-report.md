# Phase C9 — Real Customer Pilot: Final Report

**Scope:** prepare Occountant for its first paying pilot (5–10 small businesses), build the feedback
and commercial process, complete the remaining practical items, and give an honest readiness verdict.
No feature expansion; protect the deterministic accounting core.

**Evidence rule:** every claim below cites what was actually run or produced. No optimism, no
pessimism.

---

## 0. The one honest caveat, up front

**Number of real pilot users tested: 0.** This phase was executed in a development environment; no
actual small businesses were recruited or ran the software. Every "usage scenario" below is
**simulated** through the automated **acceptance** suite (six business-shaped personas), which drives
the *real* shipping ViewModels/QML end-to-end. That proves the **workflows are correct and complete**;
it does **not** prove real-world adoption, trust, or willingness to pay. Those require real users and
are the explicit purpose of the pilot this phase *prepares for* — not something this phase can claim.

Read the verdict (§7) with that boundary in mind.

---

## 1. What was delivered

### Code (surgical, off the accounting core)
1. **Period-close on-screen form** (the C7/C8 flagged should-fix, "F4/W2"). New `PeriodCloseScreen.qml`
   under **Ledger → Periods**, bound to the existing `periodVm` (`closePeriod`/`reopenPeriod`/
   `closedCount`). No engine change — it exposes an existing authoritative capability. Fully localized
   EN/FR/AR. Gated by new `ACCT_PILOT=periods` (11 assertions).
2. **Asymmetric update signing (Ed25519)** — closes ship-checklist WARNING #6. Updates now verify with
   an embedded **public** key (OpenSSL `EVP_PKEY_ED25519`); the **secret** compiles in only under
   `-DACCT_DEV_SIGNING` (default ON for gates; **OFF** for release artifacts, wired into
   `tools/release.sh`). Licensing intentionally stays symmetric HMAC (local trial mint = local-trust).
   New `app/Ed25519Signature.cpp`; `docs/update-signing.md`. Gated by 7 new `c2test` assertions.
   **Proven:** the `ACCT_DEV_SIGNING=OFF` build compiles/links and contains the **public key only —
   secret bytes absent** (byte-scan of the linked exe).

### Documentation (the heart of a pilot phase)
- `docs/pilot-release-guide.md` — cut/validate/hand-over a pilot build (build, DLL closure,
  clean-room, installer, first-launch, upgrade, uninstall-keeps-data, full checklist).
- `docs/customer-feedback.md` — 7 signal types, A–E classification, priority order, feedback log,
  decision rules for turning a request into a build (or not).
- `docs/pilot-usage-scenarios.md` — the four businesses (freelancer / retail / consultant / repair),
  each mapped to an acceptance persona with workflow, ledger effects, friction, and evidence.
- `docs/commercial-pilot.md` — pricing hypothesis, trial→paid flow, customer FAQ, support workflow,
  onboarding pointers; retention-first, no marketing optimization.
- `docs/update-signing.md` — the asymmetric scheme, trust-model split, key management for GA.

### Verified (not just built)
- **DLL closure** now includes `libcrypto-3-x64.dll` (`tools/deploy-deps.sh`), and the **clean-room
  smoke test passed** on a bare-Windows PATH → adding OpenSSL did **not** break clean-machine
  deployability.

---

## 2. Verification battery (final, all green)

Run on the modified build (commit dirty — dev tree):

| Gate | Result | Δ vs. baseline |
|---|---|---|
| build (AccountingQuick) | PASS (0 errors) | — |
| ptest (persistence + real cross-process crash recovery) | **268 / 0** | unchanged |
| itest (real QML interaction, EN↔FR↔AR) | **123 / 0** | unchanged |
| fuzz (structure-aware mutation + property) | **13 / 0** (ROBUST) | unchanged |
| ACCT_COMPAT_VERIFY (replay-equivalence) | **PASS** | unchanged |
| ACCT_HOSTILE=all (accounting correctness) | **0 findings** | unchanged |
| ACCT_PILOT=all (safety/export/onboarding/docs/comms/trust/**periods**) | **61 / 0** | +11 (period close) |
| ACCT_C2TEST=all (licensing + **Ed25519 updates** + support bundle) | **28 / 0** | +7 (asymmetric signing) |
| acceptance (6 personas × 25) | **150 / 0** (ALL PERSONAS PASSED) | unchanged |
| i18n-check (7 checks) | **PASSED** (546/546 EN/FR/AR) | +21 strings, still fully translated |
| cleanroom.ps1 (deploy-tree self-contained) | **DEPLOYABLE** | now incl. libcrypto |

Nothing regressed; the two new capabilities added their own gates and both pass.

---

## 3. Problems discovered

Because there were **no real users**, "problems" here means issues surfaced by the readiness audit and
the prior phases' punch-list — not field reports:

| # | Problem | Class | Status |
|---|---|---|---|
| P1 | Period close had an engine + VM but **no on-screen form** (couldn't be done from the UI) | B usability / workflow | **Fixed** (Ledger → Periods form) |
| P2 | Update-payload signing was **symmetric HMAC** (forgeable — key in binary) | security (pre-GA) | **Fixed** (Ed25519, public-key-only in release) |
| P3 | Adding OpenSSL risked a **STATUS_DLL_NOT_FOUND** on clean machines | packaging risk | **Fixed + verified** (deploy-deps bundles libcrypto; cleanroom PASS) |
| P4 | `deploy_quick` bundled a stale exe on its first invocation after a config change | ops trap | Mitigated (guide mandates deploy after the final build; direct re-run completes closure) |

No **class-A (wrong accounting result)**, data-safety, or crash problems were discovered — the
correctness/safety gates (`ACCT_HOSTILE` 0 findings, `ptest` crash recovery, `ACCT_PILOT=safety`) all
hold.

---

## 4. Problems fixed

P1, P2, P3 fully fixed and regression-gated (see §1–§2). P4 mitigated by process (documented in the
pilot release guide) and by the idempotent closure tool. Every fix went through the full battery.

---

## 5. Features requested vs. rejected

No real users → no real requests. What follows are **candidate class-C items** identified from the
scenario analysis, logged for the pilot to confirm or refute with evidence (per
`docs/customer-feedback.md` §5). **None were built** — that is the correct pilot posture.

**Watch-list (build only on ≥3-business evidence):**
- Saved **parts/products catalogue** for repair/retail multi-line invoices (recurring line items).
- **Styled PDF** for reports (trial balance / P&L / VAT), matching the invoice PDF (today: CSV).
- Bulk/**import** entry for high-count retail transaction days.
- A dunning/reminder workflow on top of outstanding balances.

**Rejected outright (excluded by phase constraints; need overwhelming, repeated evidence + an explicit
scope decision — not a pilot patch):**
- **Inventory**, **POS**, **cloud sync**, **AI features**, **ERP modules**.

**Explicitly not built though listed ("if users request"):** report **PDF export**. Only invoice PDF
exists today; reports export CSV, which is the accountant hand-off format. With **zero** user evidence
requesting styled report PDFs, building it now would violate the phase's evidence rule. Logged as a
watch-list class-C item instead.

---

## 6. What was deliberately protected

- The **deterministic accounting core** and its invariants — untouched. All code changes are in the
  UI (`quick/qml`), the commercial/app layer (`app/`), and test harnesses.
- No cloud, no ERP, no POS, no inventory, no AI.
- The always-balanced ledger, int64-cent money, event-sourced history, and crash-safety guarantees —
  all still green.

---

## 7. Honest commercial-readiness verdict

**Technically pilot-ready: YES.** A controlled pilot build can be cut, validated, and handed to a
small business today. The daily workflow (customer → invoice → payment → expense → tax → reports) is
**ledger-correct**, the books are **safe** (verified backups; corrupt-restore refused with live data
intact), everything is **localized** (EN/FR/AR, RTL), the first-run **onboarding** guides setup, and a
**Trust** panel lets an owner confirm their data. Updates now carry a **real asymmetric signature
boundary**, and the build is **clean-machine deployable** (verified). All 10 gates are green.

**Commercially proven: NOT YET — and this phase cannot prove it.** The success criteria
("a non-technical owner installs it; completes daily accounting without help; trusts their data;
keeps using it after the trial; some will pay") are **statements about real people**, and **no real
people were tested**. This phase de-risked everything *within* our control and built the machinery to
learn from real users; it did not, and honestly could not, demonstrate adoption, retention, or
willingness to pay.

**Remaining before taking public money (commercial/ops, not software defects):**
1. Production **code-signing certificate** (unsigned artifacts trip SmartScreen) — ship-checklist #1/#3.
2. **License key issuance/sales** pipeline (the app validates keys; someone must mint+sell them).
3. Finalise **legal** (Privacy/Terms) and **pricing** placeholders — ship-checklist #4.
4. For public *updates*: production Ed25519 keypair generated offline + **HTTPS hosting** (the
   mechanism is done; the production key + hosting are not) — ship-checklist #2, `docs/update-signing.md`.
5. Manual **screen-reader / extreme-OS-accessibility** pass on target hardware — ship-checklist #5.

**Recommendation:** proceed to a **small, high-touch, controlled pilot** (5–10 businesses, hand-held
support, updates delivered manually) using this build and the feedback process in
`docs/customer-feedback.md`. Do **not** widen or take public payment until the pilot produces evidence
of retention and at least some willingness to pay, and the five commercial/ops items above are closed.
The honest one-liner: **the software is ready to be tried; the business case still has to be earned
from real users.**
