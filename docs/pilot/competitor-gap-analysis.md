# Competitor Gap Analysis — Occountant (C11)

An honest read of where Occountant stands against typical small-business accounting products
(cloud SaaS like the well-known subscription tools, and simpler invoicing apps). The goal is **not**
to copy anyone — it's to find where Occountant has a **defensible, unique advantage** and where it is
genuinely behind, so we recruit pilots who value the former and aren't blocked by the latter.

> Comparison is by **category behaviour**, not named vendors, and reflects what Occountant *actually*
> does today (verified in the C11 audit) — not roadmap.

---

## Dimension-by-dimension

| Dimension | Typical competitor | Occountant today | Read |
|---|---|---|---|
| **Bookkeeping workflow** | Double-entry hidden behind "money in/out"; cloud DB | **Event-sourced, always-balanced double-entry**; immutable ledger; deterministic replay | **Parity+ on integrity**, arguably stronger guarantees; less hand-holding |
| **Invoicing** | Templates, auto-numbering, recurring, online-pay links | Auto-numbered, VAT lines, **PDF (EN/FR/AR, RTL)** + CSV; **no recurring**, no pay links | **Behind** on recurring + online payments; **ahead** on multilingual/RTL PDF |
| **Customer management** | CRM-ish, portals, reminders | Customers + balances + **statements** + outstanding summary; **no inline-add before C11 fix**, no dunning | **Behind** on reminders/portal; core is present |
| **Reporting** | Rich dashboards, styled PDF reports, drill-down | Trial balance, P&L, VAT — **CSV export**, under a "Ledger" menu; on-demand integrity verification | **Behind** on presentation; **ahead** on verifiable correctness (replay-equivalence) |
| **Backup** | Vendor's cloud (you don't control it) | **User-owned**, verified, crash-safe backups; corrupt-restore refused | **Ahead** — the user *owns* and can *verify* their safety net |
| **Trust / security** | "Trust us, it's in the cloud"; data on vendor servers | **Local-only**, no account, **Trust dashboard**, Ed25519-signed updates, integrity self-verify | **Ahead** — verifiable, self-custodied trust |
| **Offline capability** | Requires internet; degraded/none offline | **Fully offline**, always | **Ahead** — a real differentiator for some segments |
| **Pricing model** | **Recurring subscription** (monthly, forever) | **One-time per-computer** license (hypothesis) | **Different** — a wedge for subscription-averse owners |
| **Multi-company / accountant** | Multi-client dashboards, firm portals | **Single company per install**, no switcher | **Behind** — structurally not for multi-client accountants (see ICP) |
| **Ecosystem** | Bank feeds, app marketplace, payroll, integrations | None (by design) | **Behind** on breadth; **by choice** (focus + privacy) |

---

## Where Occountant is genuinely behind (name it honestly)
1. **Recurring/retainer invoices** — re-entered by hand (hurts consultants).
2. **Bank feeds / reconciliation automation** — none; manual entry.
3. **Multi-company / accountant-firm** use — single company only.
4. **Report presentation** — CSV, not styled PDF dashboards; buried under "Ledger."
5. **Online payment collection & reminders** — none.

None of these are on the "build now" list. They define **who Occountant is *not* for** (subscription
lovers who want an all-in-one cloud suite, bank-feed-dependent bookkeepers, multi-client firms).

## Where Occountant has a real, defensible advantage
These are not cosmetic — they're hard for a cloud-SaaS incumbent to copy without abandoning their model:

1. **Own your data, locally.** No account, nothing on a vendor's servers, works with no internet.
   A genuine answer to "I don't want my books in someone's cloud."
2. **Verifiable correctness.** Event-sourced immutable ledger + on-demand replay-equivalence
   verification + a Trust dashboard: the user can *prove* their books are intact — most tools ask you
   to take it on faith.
3. **User-owned, verified backups + crash-safety.** Safety you control and can check, not a cloud
   restore you hope works.
4. **No subscription.** One-time purchase resonates with owners tired of monthly SaaS creep.
5. **First-class EN/FR/AR incl. RTL** invoices/UI — underserved by mainstream English-first tools.

**The wedge:** *"Private, offline, one-time-purchase accounting whose correctness you can verify —
in your language, including Arabic."* That is a real position, not a me-too.

---

## Strategic implication
Do **not** try to close the SaaS feature gap (recurring, bank feeds, portals, multi-company) — that's
a losing race against their model and would blow the constraints. **Win the customers who want what
the incumbents structurally can't offer:** privacy/ownership, offline, verifiable correctness,
no-subscription, multilingual. The pilot must test whether *enough* such customers exist and will pay
(see `ideal-customer-profile.md`, `pricing-experiments.md`).
