# C11 Pilot Readiness Report — Occountant

**Framing:** written as a skeptical product-validation engineer, not an architect. The job was to
challenge the product and validate whether it solves a real accounting problem — not to add features.
The verdict deliberately refuses to say "ready" for market, because **no real customer evidence
exists yet**. What follows separates what is **proven** from what is **assumed**.

---

## 0. The unavoidable caveat

**Real pilot users to date: 0.** This phase ran in a development environment; I cannot recruit real
businesses, run real installs, or gather real satisfaction/pricing data. So this report validates two
things I *can*: (1) **technical readiness** (verified by gates), and (2) **first-time-user
readiness** (verified by a hostile audit of the *actual* shipping UI, one blocker fixed). It does
**not** — and cannot — validate market demand. That requires the real pilot this package prepares.

---

## 1. Technical readiness — **PROVEN (in test)**

Full battery, green on the C11 build (UI affordance + i18n only; engine untouched):

| Gate | Result |
|---|---|
| build | PASS (0 errors) |
| ptest (persistence + real cross-process crash recovery) | 268 / 0 |
| itest (real QML interaction, EN↔FR↔AR) | 123 / 0 |
| fuzz (structure-aware + property) | 13 / 0 (ROBUST) |
| ACCT_COMPAT_VERIFY (replay-equivalence) | PASS |
| ACCT_HOSTILE=all (accounting correctness) | 0 findings |
| ACCT_PILOT=all (safety/export/onboarding/docs/comms/trust/periods/support) | 67 / 0 |
| ACCT_C2TEST=all (licensing + Ed25519 updates + support bundle) | 29 / 0 |
| security-gate (no accounting data escapes support artifacts) | PASSED |
| acceptance (6 personas × 25) | 150 / 0 |
| i18n-check (7 checks) | PASSED (557/557 EN/FR/AR) |
| cleanroom.ps1 (deploy-tree self-contained) | DEPLOYABLE |

**Honest reading:** the engine, storage, crash-safety, correctness, security, packaging, and i18n are
**strong and gated**. But "passes our tests" ≠ "survives real users on real machines with real data
volumes." Treat this as *necessary, not sufficient*.

---

## 2. User readiness — **PARTIALLY VALIDATED (hostile audit, not real users)**

I went through the **real** shipping UI as four personas (`docs/pilot/first-time-user-audit.md`).

**What held up:** invoice `beginNew` auto-fills number/dates/line (low friction); live int64-cent
totals; guided payment→allocation; explicit expense void; verifiable Trust dashboard.

**One real blocker — FIXED:** the app's own first-run CTA ("New Invoice" from the empty state) opened
an editor with a **required, empty Customer picker and no way to add a customer** — a dead-end at the
exact first action. Fixed UI-only: an **Add customer** affordance now appears when no customers exist
(opens the customer editor over the sheet; draft preserved; localized; `itest` green).

**Findings recorded, not "fixed" (no speculative churn):**
- Reports/tax live under **"Ledger"** (accounting jargon a non-accountant may not search under).
- Status dropdown lets a user hand-set **"Paid"** (invites AR-vs-payment desync) — covered in
  onboarding; not changed (borders on posting semantics).
- No dashboard/home; void-via-status is non-obvious.

**Reading:** the *first-invoice* path is now clear enough to test with real users. Whether real
non-technical owners actually get through it unaided is **still unmeasured**.

---

## 3. Market uncertainty — **UNVALIDATED (this is the whole risk)**

Everything about demand is hypothesis:
- **Who it's for** (`docs/pilot/ideal-customer-profile.md`): privacy-minded, low-volume,
  VAT-registered **solo owners** (freelancer = strongest fit). **Explicitly NOT** multi-client
  accountants — the product is **single-company per install** (structural, audit Persona D).
- **Why they'd switch** (`competitor-gap-analysis.md`): own-your-data/offline, no-subscription,
  verifiable correctness, EN/FR/AR — real advantages incumbents can't easily copy. **Unproven that
  enough people value them enough to pay.**
- **What they'd pay** (`pricing-experiments.md`): one-time license hypothesis; **no willingness-to-pay
  data exists.**

**We do not know if a viable market segment exists.** That is the single biggest open question, and
no amount of engineering answers it.

---

## 4. Biggest risks (ranked)

1. **No demand / no willingness to pay.** The product may be excellent and still unwanted at a price.
   *Mitigation:* run the real pilot; elicit willingness-to-pay before any more engineering.
2. **ICP too narrow or wrong.** Single-company + no bank feeds + no recurring may shrink the audience
   below viability. *Mitigation:* recruit primarily freelancers/solo; measure retention honestly.
3. **"Passes tests" ≠ "works in the wild."** Real machines, real data volumes, real user error will
   find things gates didn't. *Mitigation:* high-touch pilot, Support IDs + bundles, fast P0 loop.
4. **Trust paradox.** The offline/no-cloud pitch attracts the privacy-minded but denies us telemetry —
   we learn slowly, by hand. *Mitigation:* accept manual, small-N metrics; weight depth over rate.
5. **Commercial prerequisites unmet:** unsigned installer (SmartScreen), no key-sales pipeline,
   placeholder legal/pricing, no HTTPS update hosting. *Mitigation:* close before taking public money
   (tracked; not pilot-blocking for a hand-delivered pilot).

---

## 5. What NOT to build (hold the line)

Do **not** build any of these during/after the pilot without **strong, repeated, real-customer
evidence** — most are excluded outright:
- **Multi-company / accountant-firm mode**, **bank feeds / reconciliation**, **recurring invoices**,
  **online payment collection / dunning**, **styled report PDFs / dashboards** — tempting from the
  competitor gap, but each is a SaaS-race we lose and/or scope creep.
- **Inventory, POS, cloud sync, AI, ERP** — excluded constraints.
- **Telemetry / phone-home** — would break the core promise; only ever with explicit user consent as a
  logged product decision.
- **Any change to the accounting core** to satisfy a request.

The correct response to most requests is to **record and count** them (`docs/product-decisions.md`),
not build them.

---

## 6. Next experiment (the one that matters)

**Run the real controlled pilot with 5–10 primary-ICP businesses** (bias to freelancers/solo
consultants, VAT-registered, low volume), using this package:
- Onboard with `docs/pilot/customer-onboarding.md` + `admin-checklist.md`.
- Track manually (no telemetry): activation, time-to-first-invoice, invoices/week, 30-day retention,
  failed workflows, support load — `product-metrics.md`.
- Interview at day 7/14/30 (`interview-script.md`); elicit **willingness-to-pay first, unanchored**
  (`pricing-experiments.md`).
- Triage strictly (`issue-classification.md`); fix only blockers; build no features without counted
  evidence.

**Decision gate at day 30:** proceed only if correctness held, no data loss, users worked mostly
unaided, **and ≥1 ICP customer gives a genuine willing-to-pay yes with a number and a reason.**

---

## 7. Verdict

**Technically:** ready to be *tried* — every correctness/safety/security/i18n/deploy gate is green,
and the one real first-run blocker found in the hostile audit is fixed.

**As a validated product/market:** **NOT proven — and honestly, unknown.** There is **zero real
customer evidence**. Occountant is *ready to be put in front of real pilot customers*; it is **not**
"ready to scale," and I will not call it so without customers who install it, use it unaided, trust
it, keep using it, and pay.

**Bottom line:** **Ship it to a small, controlled pilot and let real businesses judge it.** The
engineering has done its job; the open question is now entirely a **market** question, and the only
way to answer it is real usage. The expected C11 outcome is met: we either get evidence businesses
want it, or clear evidence of what must change — *before* investing more engineering time.
