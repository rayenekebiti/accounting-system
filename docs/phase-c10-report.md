# Phase C10 — Controlled Pilot Operation: Final Report

**Scope:** stand up the machinery to run a controlled pilot (5–10 businesses), make only the quality
improvements real support actually needs, and give an honest market-validation verdict. Not a feature
phase; the deterministic accounting core is untouched.

**Evidence rule:** every claim cites what was run or produced.

---

## 0. The honest caveat, up front (again)

**Number of real pilot users: 0.** This phase ran in a development environment. No real small
businesses were recruited, installed the software, or entered their books. That is a hard limit of
where this work happened — I cannot manufacture customers, install sessions, or willingness-to-pay.

Consequently, the deliverables split cleanly into two kinds:
- **Real and complete:** the operational/decision/commercial **instruments** (docs), and the two
  **code** improvements §5 genuinely required — all built, verified, and gated.
- **Structurally impossible here:** the **customer-discovery data** and **conversion evidence** — the
  logs exist and are ready, but their rows are **empty by design** until real users fill them.

Read §6 (metrics) and §7 (verdict) with that boundary explicit.

---

## 1. What was delivered

### Instruments (docs)
- `docs/pilot-operation-manual.md` — the operator runbook: onboarding, install checklist, backup,
  support, data recovery, update, and bug-reporting procedures.
- `docs/pilot-discovery-log.md` — per-business record template + roll-up table + honestly-labelled
  simulated baseline (empty of real data).
- `docs/product-decisions.md` — A–E evidence-based classification, decision principles, and a
  pre-classified candidate register (nothing approved).
- `docs/pilot-agreement.md` — plain-language pilot participation agreement template (data-ownership,
  no-warranty, support, feedback, price, termination) — flagged for lawyer review.
- Commercial mechanics reused from C9: `docs/commercial-pilot.md` (pricing hypothesis, trial→paid,
  support, onboarding), `docs/user/license-activation.md` (activation workflow).

### Code — only what running a real support process required (§5, "add only if necessary")
Two genuine, evidence-motivated gaps were found and closed — both in the app/UI layer, **no engine
change, no telemetry**:

1. **Non-PII Support Identifier.** `app/SupportId.{h,cpp}` issues a stable `OCC-XXXX-XXXX` token
   (random v4-UUID-derived — **not** a hardware/PII fingerprint), persisted locally, shown in
   **Settings → Diagnostics → Support**, and recorded in the support bundle's manifest so a support
   person can correlate reports. Nothing is transmitted.
2. **User-reachable diagnostics export.** The support bundle previously could only be produced
   headlessly (an env var) or in tests — a non-technical user could not make one, even though
   `main_quick` *claimed* the Diagnostics screen exposed it. Added
   `DiagnosticsViewModel::exportSupportBundle()` + a **Create support bundle** button (and the Support
   ID with a Copy control) to the Diagnostics screen. The bundle still contains **app health only —
   no accounting data** (allowlist-enforced).

Both fully localized EN/FR/AR. **Considered and NOT built** (adequate as-is): "better error
reporting" (editors already surface `saveFailed`/`actionFailed`; logs redact money) and
"non-sensitive crash info" (CrashReporter already ships versions + module names only).

---

## 2. Verification battery (final, all green)

| Gate | Result | Note |
|---|---|---|
| build (AccountingQuick) | PASS (0 errors) | + `-DACCT_DEV_SIGNING=OFF` release build also compiles (C9) |
| ptest (persistence + real crash recovery) | **268 / 0** | unchanged |
| itest (real QML interaction, EN↔FR↔AR) | **123 / 0** | new Diagnostics Support card instantiates clean |
| fuzz (structure-aware + property) | **13 / 0** (ROBUST) | unchanged |
| ACCT_COMPAT_VERIFY (replay-equivalence) | **PASS** | unchanged |
| ACCT_HOSTILE=all (accounting correctness) | **0 findings** | unchanged |
| ACCT_PILOT=all (…/periods/**support**) | **67 / 0** | +6 (support id + bundle via the real VM) |
| ACCT_C2TEST=all (licensing + Ed25519 + **support bundle w/ id**) | **29 / 0** | +1 (support id in manifest) |
| security-gate (no accounting data escapes support/crash artifacts) | **PASSED** | re-verified after adding the support-id field |
| acceptance (6 personas × 25) | **150 / 0** | unchanged |
| i18n-check (7 checks) | **PASSED** (554/554 EN/FR/AR) | +8 support strings, fully translated |
| cleanroom.ps1 (deploy-tree self-contained) | **DEPLOYABLE** | no new DLL deps in C10 |

Nothing regressed; the new support capability added its own gates (`ACCT_PILOT=support` +6, `c2test`
+1) and both pass, and the **security gate confirms the support ID did not open a data-leak path**.

---

## 3. Problems discovered

Again, with **no real users**, "problems" = gaps surfaced by preparing the pilot operation, not field
reports:

| # | Problem | Class | Status |
|---|---|---|---|
| Q1 | No user-reachable way to produce a diagnostics bundle (only headless/env) | B usability (support-blocking) | **Fixed** (Diagnostics → Create support bundle) |
| Q2 | No stable identifier to correlate a user's reports/bundles | B usability (support) | **Fixed** (non-PII Support ID) |
| Q3 | `main_quick` comment claimed the UI exposed the bundle — it didn't (doc/behaviour drift) | correctness-of-claim | **Fixed** (now true) |

No **class-A (wrong accounting result)**, data-safety, or crash problems were discovered — the
correctness/safety/security gates all hold (`ACCT_HOSTILE` 0 findings, `ptest` crash recovery,
`security-gate` PASSED).

## 4. Fixes made

Q1–Q3 fixed and regression-gated (see §1–§2), each through the full battery. No core change.

## 5. Features requested vs. rejected

**Requested by real users: none — 0 users.** The evidence-based register
(`docs/product-decisions.md`) is set up with a **candidate** list (pre-classified, not approved) so
that when real requests arrive the bar is explicit:

- **Deferred pending evidence (class C/D):** parts/products catalogue, styled report PDF, bulk/import,
  recurring invoices, dunning reminders, multi-currency, multi-seat.
- **Rejected by constraint (class E):** inventory, POS, cloud sync, AI features, ERP modules — and any
  change to the deterministic core.

**Built this phase (need was proven, not speculative):** the two support items in §1 — required to
*run* the pilot, in the app/UI layer, no core change.

## 6. Success metrics — status

| Dimension | Metric | Status |
|---|---|---|
| **Technical** | Zero accounting-correctness failures | **Met** (hostile 0 findings; acceptance 150/0; compat replay-equivalence) |
| **Technical** | Zero data-loss incidents | **Met in test** (ptest cross-process crash recovery; restore refuses corrupt/history-less backups) — *no real-usage sample* |
| **Product** | Users complete daily accounting unaided | **Unknown** — requires real users; workflows proven *possible/correct* via acceptance, not proven *usable* by real owners |
| **Product** | Users understand invoices/payments/reports | **Unknown** — requires real observation |
| **Business** | ≥1 customer willing to pay | **Unknown** — 0 users |
| **Business** | Clear reason they choose Occountant | **Hypothesised** (offline/private, always-balanced, trustworthy, EN/FR/AR) — **unvalidated** |

The technical bar is met and gated. The product and business bars are **unanswered by construction** —
they are exactly what a real pilot must measure, and this phase built the instruments to measure them.

---

## 7. Final report — the numbers the phase asked for

- **Number of real pilot users:** **0** (development environment; none recruited).
- **Industries tested:** none in reality. Six business *shapes* were exercised in simulation
  (freelancer, consultant, small shop/retail, repair, café, clinic) via the acceptance suite —
  capability coverage, not market coverage.
- **Problems discovered:** 3 (support-usability gaps Q1–Q3); **0** correctness/data-safety/crash.
- **Fixes made:** 3 (user-reachable diagnostics export, non-PII support ID, corrected the drifted
  claim) — all gated, no core change.
- **Features requested:** 0 real; a pre-classified candidate register exists.
- **Features rejected:** the constraint set (inventory, POS, cloud, AI, ERP) + core changes; several
  candidates deferred pending multi-business evidence.
- **Conversion willingness:** **unknown** — cannot be measured without real users.

### Honest verdict

**Need more validation.**

Not "product-market fit unclear" (that would imply we tested the market and got a murky signal — we
did not test it at all), and not "ready to scale" (we have zero paying or retained users). The precise
truth: **Occountant is technically and operationally ready to be *put in front of* real pilot
customers** — the software passes every correctness/safety/security/i18n/deploy gate, and the full
operating machinery (onboarding, support with IDs + one-click diagnostics, recovery, updates,
feedback, decisions, agreement) now exists. **Whether real small businesses can use it unaided, trust
it, keep using it, and pay — the actual market-validation questions — remains unmeasured, because no
real pilot has run.**

**Recommendation:** execute the controlled pilot for real using `docs/pilot-operation-manual.md`;
fill `docs/pilot-discovery-log.md` and `docs/product-decisions.md` from live usage; and re-evaluate
this verdict against real data. Do not widen distribution or take public payment until that real
pilot shows correctness held, no data loss, unaided daily use, and at least one customer willing to
pay — and until the remaining commercial/ops prerequisites (production code-signing cert, license-key
sales pipeline, finalised legal/pricing, production Ed25519 key + HTTPS update hosting, manual a11y
pass) are closed.
