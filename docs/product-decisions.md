# Product Decisions — Occountant (Phase C10)

The evidence-based decision register for every requested capability. The rule this document enforces:
**do not implement features because they sound useful.** A request becomes a build only when the
pilot produces the evidence the classification below requires — otherwise it is logged, classified,
and deferred or declined, *with the reason recorded*.

> **Status: 0 real feature requests recorded.** No pilot users have asked for anything yet. The
> entries in §3 are **candidate** items surfaced by scenario analysis (C9/C10), pre-classified so
> that when a real user raises one we can see whether the evidence bar is met. They are **not**
> approved work.

---

## 1. Classification (assign exactly one per request)

| Class | Meaning | Evidence bar to build | Default |
|---|---|---|---|
| **A. Required for correctness** | Without it, an accounting result is wrong or unrepresentable | **One** credible instance is enough — correctness is non-negotiable | **Build now**, with a regression gate |
| **B. Required for usability** | Users **cannot complete a daily task** unaided, or reliably misread it | **≥ 2** businesses blocked by the same thing | Build if it blocks the daily cycle; prefer copy/affordance over new UI |
| **C. Required for sales** | A named prospect **won't buy without it**, or it's legally mandated output | A **concrete** lost-sale/legal driver, not "would be nice" | Consider only with a real commercial driver |
| **D. Nice-to-have** | Genuine value, but the workflow works without it | **≥ 3** businesses independently ask **and** it doesn't touch the core | Defer; revisit post-pilot |
| **E. Reject** | Out of scope, contradicts the product, or excluded by constraint | — | Decline with a recorded reason |

**Hard constraints (auto-classify E unless overwhelming, repeated evidence + explicit scope
decision):** inventory, POS, cloud sync, AI features, ERP modules, or **any** change to the
deterministic accounting core / its invariants. These are not pilot-patch material.

**Process:** every request enters the log (§4) from the discovery/feedback logs; gets a class + a
one-line rationale; A/B that clear the bar go through the full battery before shipping; C/D/E are
recorded with the reason and revisited only if new evidence arrives.

---

## 2. Decision principles

1. **Correctness outranks everything.** An A is fixed before any B/C/D is even discussed.
2. **Count distinct businesses, not mentions.** Three variations we stretched into one theme is not
   three businesses.
3. **Prefer the smallest change.** A copy edit or an affordance beats a new screen; a new screen
   beats a new subsystem; a new subsystem is almost never right for a pilot.
4. **The core is sacred.** No request justifies changing double-entry, event-sourcing, int64-cent
   money, or crash-safety semantics.
5. **Record the "no."** A declined request with a reason is an asset — it stops us re-litigating and
   shows us when a pattern crosses the evidence bar.

---

## 3. Candidate register (pre-classified, NOT approved)

These are the plausible requests the four/six business shapes might raise, logged in advance so the
bar is explicit. **None is built.** Each needs real, counted evidence to move to "build."

| ID | Candidate | Likely class | Rationale / evidence bar | Status |
|----|-----------|--------------|--------------------------|--------|
| PD-01 | Saved **product/parts catalogue** for reusable invoice lines | D (→B if it blocks) | Repair/retail multi-line entry is workable without it; build only if ≥3 businesses say re-keying lines blocks them | Deferred — need evidence |
| PD-02 | **Styled PDF** for reports (trial balance / P&L / VAT), like invoices | C or D | Reports export CSV (the accountant format); build only if a real customer/accountant refuses CSV | Deferred — no request |
| PD-03 | **Bulk / import** entry for high-count retail days | D | Only if a real shop's daily volume makes one-by-one entry a genuine blocker (≥2 shops) | Deferred — need evidence |
| PD-04 | **Recurring invoices** (retainers) auto-generated | D | Consultants may want it; workable manually now; ≥3 asks | Deferred — need evidence |
| PD-05 | **Dunning / payment reminders** on outstanding balances | D | Outstanding summary + statement exist; reminders are a workflow addition, not correctness | Deferred — need evidence |
| PD-06 | **Multi-currency** invoicing | C (evaluate) / E-ish | Real complexity; only with a concrete cross-border customer who won't buy without it; must not touch money invariants | Deferred — needs commercial driver |
| PD-07 | **Second user / multi-seat** on one dataset | C | Single-user by design; only with a real customer needing it; big architectural weight — likely defer | Deferred — needs commercial driver |
| PD-08 | Inventory / stock tracking | **E** | Excluded constraint; needs overwhelming repeated evidence + explicit scope decision | Rejected (constraint) |
| PD-09 | POS / cash-register mode | **E** | Excluded constraint | Rejected (constraint) |
| PD-10 | Cloud sync / multi-device | **E** | Excluded constraint; contradicts the offline/privacy positioning | Rejected (constraint) |
| PD-11 | AI features (categorization, assistant) | **E** | Excluded constraint | Rejected (constraint) |
| PD-12 | ERP modules (payroll, purchasing, projects) | **E** | Excluded constraint | Rejected (constraint) |

### Already delivered when the need was proven (for reference)
- Period-close **UI** (C9): was a real workflow gap (engine existed, UI didn't) → built as a form, no
  core change. Example of a B-class item that cleared the bar.
- User-reachable **diagnostics bundle + support ID** (C10): required to run the support process →
  built in the app/UI layer, no core change, no telemetry.

---

## 4. Live decision log (append from real feedback)

One row per real request as the pilot produces them.

```
ID:            PD-###
Raised by:     Support ID / business type
Request:       "<verbatim>"
Class:         (A correctness | B usability | C sales | D nice-to-have | E reject)
Distinct businesses asking: ___
Touches core?  [ ] no  [ ] yes (→ almost certainly defer/reject)
Decision:      (build now | defer | reject)   Rationale: __________
If built:      gate/commit evidence: __________     Decided by: ______
```

*(Empty until real requests arrive — by design.)*

---

## 5. Review cadence

- **Weekly:** re-scan the feedback log; move any request that crossed its evidence bar to "build";
  keep counts of distinct businesses per theme.
- **End of pilot:** the requested/rejected lists in `docs/phase-c10-report.md` are generated from this
  register — every build justified by counted evidence, every rejection justified by a reason.
