# Customer Feedback System — Occountant Pilot (Phase C9)

A structured, honest process for turning what 5–10 real businesses say into decisions. The pilot's
whole purpose is to **discover real-world problems** — this doc is how we capture, classify, and act
on them **without** letting requests pull us into feature sprawl.

> Guardrail: a request is not a mandate. We fix pilot **blockers** fast; we build **features** only
> on **multi-user evidence** (§ Decision rules). Protect the deterministic accounting core.

---

## 1. What we collect (every channel funnels into one log)

Collect the same seven signal types from every pilot user, whatever the channel (call, email,
in-app support bundle, screen-share):

1. **Installation problems** — install/first-launch/upgrade/uninstall friction or failure.
2. **First-use confusion** — where a new owner got stuck in the first hour.
3. **Accounting workflow confusion** — invoice → payment → expense → tax → report misunderstandings.
4. **Missing features requested** — "I need X to run my business."
5. **Bugs** — wrong behaviour, errors, crashes.
6. **Performance issues** — slowness, lag, memory, large-file behaviour.
7. **Trust concerns** — "will I lose my data?", "are these numbers right?", "is my data private?".

Each user also gets a **diagnostics bundle** ask when a bug is reported: *Settings → Diagnostics →
Create Support Bundle* (build info + logs + crash reports, **no accounting data** — proven by
`security-gate.sh`). That plus their words is a complete report.

---

## 2. Every issue gets exactly one classification

The classification decides what happens next. Assign one letter per issue:

| Class | Meaning | Default action |
|---|---|---|
| **A — Correctness problem** | Wrong accounting result, unbalanced ledger, wrong tax/total/balance | **Fix immediately.** Highest priority; a correctness bug is existential for accounting software. Add a regression gate (`ACCT_HOSTILE`/`ptest`/`itest`). |
| **B — Usability problem** | The result is correct but the user couldn't get to it, or misread it | Fix if it **blocks** the daily workflow; otherwise batch. Prefer copy/affordance fixes over new UI. |
| **C — Missing feature** | A capability the product doesn't have | **Do not build on one voice.** Log, tag the business type, wait for multi-user evidence. |
| **D — Commercial request** | Pricing, licensing, editions, invoicing-the-customer, support terms | Route to the commercial owner; never a code change by default. |
| **E — Not worth building** | Out of scope / contradicts the product / vanishingly rare | Record the decision **and the reason**, decline kindly. Honesty > a growing backlog. |

If an issue spans two (e.g. a confusing screen that also hides a wrong number), split it: the wrong
number is **A**, the confusion is **B**. Never let a **B** wrapper hide an **A**.

---

## 3. Priority order for fixing (pilot blockers only)

When something must be fixed during the pilot, this is the order — it matches the phase's fix list:

1. **Wrong accounting results** (A) — stop the line.
2. **Data safety issues** — anything that could corrupt or expose books.
3. **Crashes / data loss.**
4. **Workflow blockers** — a daily task can't be completed at all.
5. **Confusing UX** — the task is possible but the user needs help every time.

Everything below #5 waits for the post-pilot review. Nothing on the "do not build" list
(inventory, POS, cloud sync, AI, ERP) is built during the pilot regardless of priority.

---

## 4. The feedback log (one row per issue)

Keep a single append-only table (spreadsheet or `docs/pilot-feedback-log.md`). Columns:

```
ID | Date | Business (type) | Channel | Signal type (1–7) | Class (A–E) |
Title | What happened (their words) | Repro / bundle ref | Severity (1–5) |
Status (new/triaging/fixing/fixed/declined) | Resolution + evidence (gate/commit) | Decided by
```

Rules:
- **Their words, verbatim**, in "What happened" — don't pre-interpret; interpretation goes in Class.
- A **fixed** row must cite the **evidence** (the gate that now covers it, or the commit).
- A **declined** row (C/E) must cite the **reason** and who decided — so we can revisit if a second
  or third user asks for the same thing.

### Starter template (copy per issue)
```
ID:            PILOT-###
Business:      (Freelancer | Retail | Consultant | Repair | …)
Signal:        (1 install | 2 first-use | 3 workflow | 4 missing | 5 bug | 6 perf | 7 trust)
Class:         (A correctness | B usability | C missing | D commercial | E won't-build)
Severity:      (1 trivial … 5 blocker)
Title:
What happened: "<verbatim>"
Repro/bundle:  <steps / SupportBundle.zip ref>
Status:        new
Resolution:    —
```

---

## 5. Decision rules (how a **C** becomes a build — or doesn't)

A missing-feature request is only reconsidered for building when **all** hold:

1. **≥ 3 pilot businesses** independently ask for the same capability (not variations we stretched
   together), **or** it blocks a legally-required output (e.g. a tax authority's mandated format).
2. It can be built **without** touching the deterministic accounting core or its invariants.
3. It is **not** on the excluded list (inventory, POS, cloud sync, AI, ERP) — those need
   overwhelming, repeated evidence and an explicit scope decision, not a pilot patch.

Until then a **C** stays logged with its business-type tags. The pilot's success is measured by
**retention and correctness**, not by how many requests we closed.

---

## 6. Weekly rhythm during the pilot

- **Per issue:** log within the day; acknowledge to the user.
- **Weekly triage:** re-read the log, (re)assign Class + Severity, promote any A/data-safety/crash to
  immediate. Count how many distinct businesses hit each theme (that's your build signal for §5).
- **Per fix:** ship only through the full battery (`build/ptest/itest/fuzz/compat/hostile/pilot/`
  `c2test/i18n/cleanroom`) — a pilot fix that regresses the engine is worse than the original issue.
- **End of pilot:** roll the log into the final report — problems found, fixed, requested, and
  **explicitly rejected with reasons**.

---

## 7. What "good" looks like

The pilot is working if, over 30 days, the log shows: **zero open class-A issues**, data-safety and
crash counts at zero, workflow blockers driven to zero, and the class-C list **triaged, not
built** — while users keep entering real transactions. Feedback volume going *down* on confusion
themes (as copy/onboarding improves) and *staying flat-to-none* on correctness is the signal that
Occountant is ready to widen.
