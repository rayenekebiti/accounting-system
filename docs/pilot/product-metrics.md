# Product Metrics — Occountant Pilot (C11)

Manual, **privacy-preserving** metrics. **No telemetry** — the app never phones home, collects no
usage data, and sends nothing automatically. Every metric below is gathered **by the operator** from
onboarding notes, weekly check-ins, interviews (`interview-script.md`), and support contacts — the
same way you'd run a hand-measured pilot. If a number can only be gotten by instrumenting the app,
**we don't collect it.**

> This preserves the core promise (your data stays yours) while still letting us learn. The trade-off
> is honest: metrics are small-N and self/operator-reported, not automatic — appropriate for a
> 5–10 business pilot, and we treat them as directional, not precise.

---

## The metrics (definition · how measured · target hypothesis)

| # | Metric | Definition | How measured (manual) | Target (hypothesis) |
|---|---|---|---|---|
| 1 | **Activation rate** | % of installs that complete onboarding **and** post ≥1 real invoice | Operator onboarding log (`pilot-discovery-log.md`) | **≥ 80%** of installs |
| 2 | **Time to first invoice** | Minutes from first launch to first saved+posted invoice | Timed during/after onboarding call | **≤ 15 min** with light guidance |
| 3 | **Invoices per week** | Real invoices entered per active user per week | Weekly check-in; ask the count (or view their list together) | **≥ 2/wk** sustained (varies by business) |
| 4 | **Retention after 30 days** | % still entering real transactions in week 4, unprompted | Day-30 interview + their own transaction count | **≥ 50%** of activated users |
| 5 | **Support requests** | Count + type per user; correctness/crash vs. how-to | Support log + `customer-feedback.md` (class A–E) | Trend **down** on how-to; **0** correctness/crash |
| 6 | **Failed workflows** | Times a user **couldn't** complete a core task (⛔ on the feedback form) | Feedback form Q3 + interviews | Trend to **0** blockers |
| 7 | **Willingness to pay** | # of users who give a **yes + a price + a reason** at day 30 | Day-30 interview (ask for a number first) | **≥ 1** genuine yes (phase success bar) |

### Supporting signals (qualitative, still no telemetry)
- **Trust:** self-reported "do you trust the numbers / that your data is safe?" (yes/unsure/no + quote).
- **Replacement:** what Occountant replaced (spreadsheet / other app / accountant) and the pain (1–10)
  of losing it.
- **Referral intent:** would they recommend it, and to **which business type** (sharpens the ICP).

---

## Collection cadence
- **At onboarding:** install success, time-to-first-invoice, time-to-first-payment, business type,
  Support ID → `pilot-discovery-log.md`.
- **Weekly:** invoices/payments/expenses count, any blockers, support contacts.
- **Day 7 / 14 / 30 interviews:** retention, failed workflows, trust, willingness-to-pay + reason.
- **Any time:** feedback-form submissions; support bundles for bugs (contain no accounting data).

## Anti-metrics (what we deliberately do NOT measure)
- **No** in-app event tracking, screen-view counts, click heatmaps, or crash auto-upload.
- **No** unique-device fingerprinting. (The **Support ID** is a random, user-visible correlation label
  that only travels if the user sends a bundle — see `docs/pilot-operation-manual.md`.)
- **No** phone-home of any kind. If we ever want automatic metrics, that's a **product decision with
  user consent**, logged in `product-decisions.md` — not a quiet addition.

## Reading the numbers honestly (small-N caution)
With 5–10 businesses, these are **directional**, not statistically significant. A single strong,
retained, willing-to-pay ICP customer with a clear reason is **more informative** than an aggregate
percentage. Weight **depth of evidence** over headline rates, and never round a "maybe" up to a "yes."
