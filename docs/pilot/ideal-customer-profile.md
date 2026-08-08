# Ideal Customer Profile — Occountant (C11)

Who to recruit for the pilot and, later, who to sell to first. Derived from the C11 first-time-user
audit (`first-time-user-audit.md`) and the competitor gap analysis — **not** from wishful targeting.
The discipline here is **narrowness**: a product that tries to serve everyone serves no one, and
Occountant's real advantages only matter to a specific kind of buyer.

> Everything below is a **hypothesis to validate in the pilot**, except the hard exclusions, which are
> **structural facts** about the product today.

---

## The primary ICP (recruit these first)

**A privacy-minded, low-volume, VAT-registered solo business owner who keeps their own books.**

| Attribute | Target |
|---|---|
| **Business size** | 1 person (sole trader / freelancer) or a micro-business of 2–3 |
| **Transactions/month** | **~5–40** invoices + a similar count of expenses (low volume) |
| **Current method** | Spreadsheets, a notebook, or a subscription tool they resent paying for |
| **Tax** | **VAT/GST-registered** — needs a clean VAT total and PDF invoices |
| **Tech comfort** | Can install a Windows app and follow a short guide; not an accountant |
| **Motivations** | Owns/controls their data; distrusts cloud; dislikes subscriptions; wants it in their language (esp. FR/AR) |
| **Best-fit persona** | **Freelancer** (audit: strongest fit), then **consultant/service** (low invoice count) |

### Why they'd switch to Occountant
- **Privacy/ownership:** "My books stay on my computer." (Cloud tools can't match without changing model.)
- **No subscription:** one-time purchase vs. paying monthly forever.
- **Verifiable trust:** they can *prove* the books are intact and *own* their backups.
- **Language:** first-class EN/**FR/AR (RTL)** invoices and UI.
- **Simplicity:** does invoices → payments → expenses → VAT without a bloated suite.

### Their pain points Occountant actually addresses
- Resentment of monthly SaaS fees for basic invoicing.
- Discomfort with financial data living on a vendor's servers.
- Needing a professional, correct, multilingual invoice + a VAT figure for the return — without an
  accounting degree.

### Maximum acceptable price (hypothesis — validate)
- A subscription-averse solo owner will weigh a one-time price against **~1–2 years of a cheap SaaS
  subscription**. Hypothesis: a one-time **Personal** license in the low-to-mid **[PLACEHOLDER: two
  figures]** range feels fair; **Business** modestly higher. *The Day-30 interview asks them for a
  number first — do not anchor.* (See `pricing-experiments.md`.)

---

## Secondary / stretch (test only if primary shows traction)
- **Repair / small shop with low-to-moderate volume** (audit: usable; high-volume cash retail is a
  weak fit — no bulk entry).
- **An accountant recruited as an *advisor*** to one owner's books (not to manage many clients in the
  app).

## Hard exclusions (structural — do NOT recruit or promise)
- **Accountants/bookkeepers managing multiple clients.** Occountant is **single-company per install**
  with no in-app switching (audit Persona D). This is architectural and out of scope.
- **High-volume cash retail / POS needs.** No bulk/fast entry, no POS (excluded constraint).
- **Businesses needing bank feeds / auto-reconciliation, online payment collection, payroll, or
  inventory.** None exist; all excluded.
- **Subscription-preferring buyers who want an all-in-one cloud suite** — wrong model fit.

---

## Recruiting guidance for the pilot (5–10 businesses)
Bias the mix toward the **primary ICP** so the signal is clean:
- **≥ 4 freelancers/solo consultants** (VAT-registered, low volume) — the core bet.
- **1–2 small service companies** (low invoice count).
- **1–2 small shops/repair** (moderate volume) — to test the edge, not the center.
- **0 multi-client accountants** as primary users (structural mismatch) — optionally 1 as an advisor.

**Selection screen (ask before recruiting):**
1. Roughly how many invoices/expenses a month? *(want ≤ ~40)*
2. VAT-registered? *(prefer yes)*
3. How do you do it now, and what bugs you about it? *(want: SaaS-fatigue or privacy concern)*
4. Comfortable installing a Windows app and following a short guide? *(want yes)*
5. Do you need bank feeds / to manage other people's books? *(if yes → not a fit)*

**The pilot's core question for the ICP:** do enough of these people exist, complete their daily books
unaided, trust it, keep using it, and pay a one-time price? If yes → we have a beachhead. If no → the
ICP is wrong or the product isn't compelling enough, and we learn that before more engineering.
