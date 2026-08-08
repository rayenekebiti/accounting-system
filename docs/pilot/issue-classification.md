# Issue Classification — Occountant Pilot (C11)

How every pilot issue is triaged and prioritized. This is the single source of truth for "what do we
do with this report?" It reconciles the two axes used across the project: a **severity/priority**
ladder (how fast) and the **A–E class** (what kind), and it defers feature classification to
`docs/product-decisions.md`.

---

## Two questions, in order

### Q1 — Is it a correctness or safety problem? (stop-the-line)
Before anything else, decide if the report indicates any of:
- a **wrong accounting result** (amount, total, tax, balance, report),
- a **data-safety / corruption** risk,
- a **crash** on normal use, or **data loss**.

If **yes → P0**. Halt other work, reproduce, capture the **Support Bundle**, fix, and add a regression
gate (`ACCT_HOSTILE` / `ptest` / `itest` / `c2test`) **before** shipping. A correctness bug is
existential for accounting software — one is enough to act.

### Q2 — Otherwise, classify by kind (A–E) and severity (1–5)

| Class | Kind | Typical action |
|---|---|---|
| **A** | Correctness (wrong result / unrepresentable) | Always P0/P1; fix + gate |
| **B** | Usability (can't complete a task, or reliably misreads it) | Fix if it **blocks** a core workflow; else batch |
| **C** | Sales (a named prospect won't buy without it / legally mandated) | Route to commercial; build only with a real driver |
| **D** | Nice-to-have | Defer; needs multi-business evidence |
| **E** | Reject (out of scope / excluded constraint) | Decline with a recorded reason |

Feature-type items (C/D/E) are logged and decided in **`docs/product-decisions.md`** using its
evidence bar — never built on a single request.

---

## Severity ladder (how fast)

| Sev | Meaning | Response |
|---|---|---|
| **1 — Critical** | Wrong numbers / data loss / crash / can't use the app | Same-day plan; drop other work |
| **2 — High** | A core daily workflow is blocked for a user | Fix within the pilot; prioritized |
| **3 — Medium** | Workflow possible but confusing / slow; frequent friction | Batch; fix if cheap or repeated |
| **4 — Low** | Cosmetic / rare / minor confusion | Backlog; fix opportunistically |
| **5 — Note** | Idea, preference, feature request | Log to product-decisions; no code |

**Priority order for fixing (matches the phase):** (1) wrong accounting results → (2) data-safety →
(3) crashes/data loss → (4) workflow blockers → (5) confusing UX. Nothing on the excluded list
(inventory, POS, cloud, AI, ERP) or the accounting core is touched during the pilot.

---

## Decision flow

```
        ┌─────────────────────────────────────────────┐
report →│ Wrong number? Data loss/corruption? Crash?  │─ yes ─▶ P0: reproduce → bundle → fix → GATE → ship
        └──────────────┬──────────────────────────────┘
                       │ no
                       ▼
        ┌─────────────────────────────────────────────┐
        │ Does it BLOCK a core workflow (can't do it)? │─ yes ─▶ Class B / Sev 2: fix within pilot (UI-only if possible)
        └──────────────┬──────────────────────────────┘
                       │ no
                       ▼
        ┌─────────────────────────────────────────────┐
        │ Is it a feature request?                     │─ yes ─▶ product-decisions.md (A–E + distinct-business count)
        └──────────────┬──────────────────────────────┘
                       │ no (confusion / cosmetic)
                       ▼
                 Class B/Sev 3–4: batch; prefer copy/onboarding fix over new UI
```

---

## Worked examples (from the C11 audit)

| Report | Q1? | Class | Sev | Decision |
|---|---|---|---|---|
| "I clicked New Invoice but there's no customer and no way to add one" | no | B (blocks first invoice) | 2 | **Fixed** — add-customer affordance (UI-only) |
| "I couldn't find my VAT report" | no | B (findability) | 3 | Batch — it's under *Ledger*; onboarding covers it; rename only on repeated evidence |
| "I marked an invoice Paid but my outstanding is wrong" | maybe | B + data-hygiene | 2–3 | Onboarding: record a payment, don't hand-set Paid; no engine change |
| "Can I manage my 30 clients' books here?" | no | E→ICP | 5 | Reject (single-company by design); it defines the ICP, not a fix |
| "The total on invoice #14 is wrong" | **YES** | A | 1 | **P0** — reproduce, bundle, fix, add gate before shipping |

---

## Golden rules
1. **Correctness first, always.** A P0 pre-empts everything.
2. **Fix only what blocks;** log everything else. Prefer the smallest change (copy > affordance > new
   screen > never a new subsystem).
3. **Never touch the accounting core** to satisfy a pilot request.
4. **Record every "no"** with a reason (`docs/product-decisions.md`) so patterns surface honestly.
