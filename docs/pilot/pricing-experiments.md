# Pricing Experiments — Occountant (C11)

**Do not decide pricing on assumptions.** This defines *experiments* to run with real pilot users to
learn what model and price they'll actually pay. The output is evidence, not a decreed price. Pricing
is a **business decision** informed by the pilot — no code changes here; the app already reads editions
from the license key.

> Reminder: the phase goal is to find out **if** customers will pay and **why they choose Occountant**
> — not to maximize a funnel. First prove someone pays with a clear reason.

---

## Candidate models (to test, not to assume)

| Model | Fits whom | Pro | Con / risk |
|---|---|---|---|
| **A. Free trial → one-time paid license** *(current hypothesis)* | Subscription-averse solo owners (the ICP) | Matches "own it, no monthly fees"; aligns with offline/local positioning | Lower recurring revenue; update-renewal question |
| **B. Monthly subscription** | Users who prefer low upfront cost | Predictable revenue | Contradicts the anti-subscription wedge; needs a licensing/billing loop the app doesn't have |
| **C. Annual license** | Businesses used to yearly renewals | Recurring-ish without monthly friction | Still a renewal; must justify yearly value |
| **D. Per-business pricing** | Owners with 1 business (most ICP) | Simple, honest ("per company") | Multi-company isn't supported anyway → little upsell |
| **E. Accountant edition** | Multi-client firms | Higher ACV | **Structurally impossible today** (single-company); do **not** offer until multi-company exists |

**Prior from positioning:** Model **A (one-time license)** is the differentiator against subscription
incumbents and matches the ICP. Treat A as the hypothesis to confirm; test B/C only if A clearly fails
on willingness-to-pay.

---

## Experiments

### E1 — Willingness-to-pay elicitation (every pilot user, Day 30)
- **Method:** ask them to name a fair price **first** (no anchor), then probe model preference
  (one-time vs monthly vs yearly). Record the number, the model, and the **reason**.
- **Learn:** the distribution of acceptable prices and which model they *think* in.
- **Success signal:** ≥1 user gives a concrete number + reason + intent (phase bar); ideally a
  cluster around a defensible one-time price.

### E2 — Van Westendorp-lite (4 price questions)
Ask each user, for a one-time Personal license:
1. At what price would it be **too expensive** to consider?
2. At what price would it be **expensive but worth considering**?
3. At what price is it a **good deal**?
4. At what price is it **so cheap** you'd doubt its quality?
- **Learn:** an acceptable price band and a psychological ceiling/floor from real owners.

### E3 — Model preference A/B (framed offer)
- Present two honest offers to different subsets: (a) one-time **[X]**, or (b) **[X/12]**/month.
- **Learn:** do subscription-averse owners actively prefer one-time even at a higher total? (Confirms
  or kills the wedge.)
- **Guardrail:** don't over-index on a handful of responses; use it directionally.

### E4 — Editions test (Personal vs Business)
- Describe both editions (Business adds priority support) at hypothesized prices.
- **Learn:** does the ICP see enough value in "Business" to pay the delta, or is one edition enough?

### E5 — Anchor to their status quo
- Ask what they pay today (SaaS subscription? nothing?) and compute their **2-year cost** of the
  alternative.
- **Learn:** frame Occountant's one-time price against their real incumbent spend; find the switch point.

---

## What we are NOT doing
- **No live price A/B on a website / funnel optimization** — retention isn't proven yet; that's premature.
- **No discounting games** beyond a possible one-time pilot-participant thank-you (`pilot-agreement.md`).
- **No accountant/multi-seat pricing** until the product supports multi-company.
- **No auto-renewing subscription plumbing** — the app has no billing loop; adding one is a product
  decision requiring evidence (`product-decisions.md`).

---

## Decision rule (after the pilot)
Set a first real price **only if**:
1. ≥1 (ideally several) ICP users show **genuine willingness to pay** with a number + reason (E1/E2),
   **and**
2. a **defensible one-time price band** emerges from E2/E5, **and**
3. retention (`product-metrics.md` #4) is real — people pay for what they keep using.

If willingness-to-pay is weak or absent, the answer is **not** "lower the price" — it's "the value or
the ICP is wrong," which is itself the most valuable finding of the pilot. Record it in
`docs/c11-pilot-readiness-report.md` and stop before more engineering.
