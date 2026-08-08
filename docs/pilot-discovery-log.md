# Pilot Discovery Log — Occountant (Phase C10)

The instrument for recording **what actually happens** with each pilot business. One record per
business, filled during onboarding and updated through the pilot. This is how §6 success metrics get
answered with evidence instead of opinion.

> **Status: 0 real businesses recorded.** No customers have been onboarded in this environment. The
> template and the classification below are ready; the data rows are **empty on purpose** — they can
> only be filled by real pilot users. The "simulated baseline" (§4) is a reference from the automated
> acceptance personas, **not** real-usage data, and is labelled as such.

---

## 1. What to record per business

For every pilot user, capture:

- **Business type:** one of `freelancer` · `consultant` · `small shop` · `repair business` ·
  `service company`.
- **Support ID:** their `OCC-XXXX-XXXX` (from Settings → Diagnostics) — the correlation key.
- **Installation success:** yes / no (+ error + whether a Support Bundle was captured).
- **Time until first invoice:** wall-clock from first launch to first saved+posted invoice.
- **Time until first payment recorded:** from first launch to first recorded+allocated payment.
- **Accounting tasks completed:** which of {customer, invoice, PDF export, payment, expense, report/
  CSV, statement, backup, period close} they did **unaided**.
- **Confusing screens:** where they hesitated, asked, or did the wrong thing (verbatim + screen).
- **Missing workflows:** something they needed to do and couldn't.
- **Requested features:** their words → later classified in `docs/product-decisions.md`.
- **Trust signals:** did they believe the numbers / believe their data is safe? (quote).
- **Retention:** still entering real transactions at week 1 / 2 / 4? Activated a paid key? (yes/no).

---

## 2. Per-business record template (copy one per user)

```
────────────────────────────────────────────────────────────────────
Business:            #__   type: (freelancer|consultant|small shop|repair|service)
Support ID:          OCC-____-____
Onboarded (date):    ____________     Operator: __________
Windows version:     ____________

INSTALL
  Install success:   [ ] yes  [ ] no      Error / bundle ref: __________
  First-launch wizard reached: [ ] yes    Company details set: [ ] yes

TIME-TO-VALUE (wall clock from first launch)
  First invoice:     ______ min          First payment recorded: ______ min

TASKS COMPLETED UNAIDED  (tick what they did without help)
  [ ] create customer   [ ] issue invoice   [ ] export PDF
  [ ] receive+allocate payment   [ ] record expense
  [ ] run report / export CSV    [ ] customer statement
  [ ] back up + verify           [ ] close a period

FRICTION
  Confusing screens:   __________________________________________
  Missing workflows:   __________________________________________
  Requested features:  __________________________________________   → product-decisions ID: ____

TRUST
  "Do you trust these numbers?"      quote: ______________________
  "Do you trust your data is safe?"  quote: ______________________

RETENTION / CONVERSION
  Still using at:  wk1 [ ]  wk2 [ ]  wk4 [ ]
  Willing to pay:  [ ] yes  [ ] maybe  [ ] no     Why: ______________
  Activated paid key: [ ] yes  [ ] no
────────────────────────────────────────────────────────────────────
```

---

## 3. Roll-up table (fill as businesses onboard)

| # | Type | Install OK | 1st invoice (min) | 1st payment (min) | Tasks unaided | Confusing screens | Missing workflows | Trusts data | Willing to pay | Retained wk4 |
|---|------|-----------|-------------------|-------------------|---------------|-------------------|-------------------|-------------|----------------|--------------|
| 1 | — | — | — | — | — | — | — | — | — | — |
| 2 | — | — | — | — | — | — | — | — | — | — |
| 3 | — | — | — | — | — | — | — | — | — | — |
| 4 | — | — | — | — | — | — | — | — | — | — |
| 5 | — | — | — | — | — | — | — | — | — | — |
| … | | | | | | | | | | |

Target coverage: at least one business of each type (freelancer / consultant / small shop / repair /
service) so the findings aren't skewed to one shape.

---

## 4. Simulated baseline (reference only — NOT real usage)

To sanity-check that the *workflows themselves* are complete before real users arrive, the automated
**acceptance** suite drives six business shapes through the full lifecycle. This is a **capability**
baseline (the tasks are possible and correct end-to-end), **not** a usability or adoption measurement
— an automated harness has no "confusion" or "time-to-first-invoice" in the human sense.

| Persona (≈ business type) | Lifecycle driven | Result |
|---|---|---|
| `freelancer` (freelancer) | customers → high-value 20%-VAT invoices → payments → expenses → reports | 25/25 |
| `consultant` (service company) | retainers + VAT → payments → monthly reports | 25/25 |
| `retail` (small shop) | many customers → frequent VAT invoices/payments → daily expenses | 25/25 |
| `repair` (repair business) | multi-line parts+labour invoices → part-payments → balances | 25/25 |
| `cafe` (cash small shop) | cash, VAT-exempt lines | 25/25 |
| `clinic` (exempt services) | VAT-exempt services | 25/25 |

**Reading:** the daily workflows are correct and complete for these shapes (150/150 assertions). What
the simulation **cannot** tell us — and what the pilot exists to find — is whether a real
non-technical owner *reaches* those outcomes unaided, understands them, and keeps coming back. Those
rows (§3) are the ones that matter, and they are empty until real users fill them.

---

## 5. How this feeds the decision

- **Confusing screens** → usability items (class B) in `docs/product-decisions.md`.
- **Missing workflows / requested features** → class A/B/C/D/E in `docs/product-decisions.md`; only
  built on the multi-user rule there.
- **Trust + retention + willing-to-pay** → the business half of the §6 success metrics and the final
  verdict in `docs/phase-c10-report.md`.
