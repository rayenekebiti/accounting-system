# Design Review: Accounting Period Closure & Historical Freezing v1

Status: **implemented + verified** (`tools/ptest.sh` — 98 in-process assertions + 13
cross-process crash recoveries incl. atomic period close). Builds on the
event-authored history (`docs/audit-journal.md`, `docs/commit-cutover.md`).

> The system could already *reconstruct* history; it could not declare historical
> *finality*. This adds temporal accounting authority: what is closed, what is frozen,
> and how post-close changes are represented — without ever rewriting history.

---

## Temporal axes (explicit — these are NOT interchangeable)

| Axis | Meaning | Source |
|------|---------|--------|
| **event seq** | total order of history; replay/reconstruction axis | `EventLog` (monotonic, gap-free) |
| **operational timestamp** | wall-clock when recorded; display only | event `timestampMs` |
| **accounting effective date** | the business date a transaction belongs to | e.g. an invoice's **issue date** |
| **period membership** | which period an entry falls in | by **effective date**, never seq/timestamp |
| **post-close adjustment date** | effective date of a compensating entry | an open period's effective date |

Period closure operates on the **effective date**; reconstruction operates on **seq**.
A backdated entry (recorded later, higher seq, earlier effective date) is governed by
its effective date.

## 1–2. Period model & closure semantics

A period is a labeled **`[startDate, endDate]` range over effective dates** —
configurable (monthly/quarterly/yearly are conventions, not hardcoded). Closing
appends a `PeriodClosed{label, start, end, closedAtSeq}` event, where `closedAtSeq` =
the history head at close. The closed-period set is a **disposable index** rebuilt by
scanning the log for `PeriodClosed`/`PeriodReopened` — never a hidden mutable authority.

## 3. Historical freezing rules

After close, a transaction whose effective date is in the period is **frozen**. History
is append-only by construction, so freezing is enforced at *command time*:
`recordInvoiceCorrected` rejects (throws) when the invoice's effective date (existing or
proposed) falls in a closed period. Nothing rewrites or reorders prior events.

## 4. Post-close correction model

A frozen transaction is not edited — the operator posts an **append-only adjustment** (a
new transaction dated in an open period). **Reopening** is itself an explicit,
append-only `PeriodReopened` event (fully auditable, replay-deterministic); after it,
corrections to that period are allowed again. Amendment (direct edit) and reversal/
adjustment (compensating entry) are thus distinct: amendment is *forbidden* on closed
periods; adjustment is the sanctioned post-close path.

## 5. Temporal accounting semantics (proven)

| Query | Mechanism |
|-------|-----------|
| books **as closed** for period P | `reconstructAllInto(scratch, closedAtSeqFor(P))` |
| books **before** an event X | `reconstructAllInto(scratch, X.seq − 1)` |
| **current** books incl. post-close adjustments | `reconstructAllInto(scratch, lastSeq)` |
| is entry frozen? | `isDateInClosedPeriod(effectiveDate)` |

Proven: with INV-1 (Jan, frozen) and INV-2 (Feb, posted after close), books-as-closed
shows exactly 1 invoice; current books show 2.

## 6. Event model additions

`PeriodClosed = 6` (`[label 32][start 10][end 10][u64 closedAtSeq]`), `PeriodReopened =
7` (`[label 32]`). Type numbers are permanent; an unknown type on replay refuses
(downgrade protection).

## 7. Replay / reconstruction interaction

`apply()` treats period events as **no-ops for entity projections** (they change only
the period index, rebuilt separately) — so a customer/invoice reconstruction is
unaffected by them, and the period index is derived purely from history. Reconstruction
remains deterministic before close, at close, and after adjustment.

## 8. Projection interaction

The closed-period index is a projection: **disposable, rebuilt from events** on open
and after each period event. No projection mutates history; entity projections are
unchanged by period events.

## 9. Verification / drift interaction

Period events don't touch entity projections, so projection verification (`verify()`)
is unaffected. Period state has its own determinism check: the index rebuilds
identically from history (proven by a fresh-journal rebuild assertion).

## 10. Migration implications

No record-layout change — periods live entirely in the event log + a derived index.
Schema migration (record layer) and the period index are orthogonal.

## 11. Crash / recovery guarantees (proven)

Close is **atomic**: it is one `EventLog.append` (its own write-ahead record). Killed at
`afterEventFrame` → the close event is uncommitted → truncated → *not closed*; killed at
`afterEventCommit` → committed → the index rebuilds *as closed* on reopen. Either way the
books are intact and the period is unambiguously closed-or-not — never partially closed,
no orphan adjustment, no mixed state. (Cross-process kill tests, both windows.)

## 12. Deterministic validation harness (all green)

`ACCT_PTEST=suite`: freeze by effective date; closed-period correction rejected vs
open-period allowed; books-as-closed vs current; reopen unfreezes; index rebuilds
deterministically. Cross-process: `pclose-crash@afterEventFrame/afterEventCommit`.

## 13. Observability / diagnostics

Startup `acct.storage`: `audit history: … closedPeriods=K`. A rejected correction throws
a precise message ("in a closed accounting period — post an adjustment"), captured by
the `acct.*` diagnostics layer.

## 14. Operator / support workflows

`isDateInClosedPeriod` / `isInvoiceInClosedPeriod` let the UI disable editing of frozen
entries and steer the operator to an adjustment. Close/reopen are auditable events;
"books as closed" is a deterministic, reproducible report basis.

## 15. Incremental rollout strategy

- **v1 (this):** period model, close/reopen events, freeze enforcement on the invoice
  correction path, books-as-closed reconstruction, deterministic index — all at the
  `AuditJournal` layer + proven.
- **Next:** surface close/reopen + the frozen-entry guard in the UI as part of the
  Phase-3 invoice commit cutover (`docs/commit-cutover.md`); a structured
  reversal/adjustment helper; period-aware reporting basis.

## 16. Explicit non-goals

No reporting dashboards/BI; no tax/regulatory logic; no ERP/period-management framework;
no replay-throughput optimization; no distributed/CQRS/async; no implicit timezone or
business-date assumptions (effective dates are explicit `IsoDate`s). Local-first,
deterministic, single-process.

---

## Critical design questions — answered

1. *Boundary?* labeled `[start,end]` over effective dates. 2. *Calendar/config/derived?*
   configurable range (calendar is a convention). 3. *Closes a period?* a `PeriodClosed`
   event. 4. *Freeze boundary?* `closedAtSeq` in that event. 5. *Legal post-close?*
   append-only adjustments in open periods. 6. *Reversal vs amendment?* amendment
   forbidden on closed; adjustment/reversal = new events. 7. *Surfaced historically?* as
   later events, visible in current books but not in books-as-closed. 8. *Reports as
   closed?* `reconstructAllInto(closedAtSeq)`. 9. *seq vs date?* orthogonal — date drives
   membership, seq drives reconstruction. 10. *Timezone/business date?* explicit `IsoDate`
   effective dates, no implicit tz. 11. *Reopen?* yes, append-only `PeriodReopened`.
   12. *Version mismatch?* refuse unknown event types. 13. *Verification affected?* no —
   period events don't touch entity projections. 14. *Correction chains?* preserved in
   history; books-as-closed reconstructs pre-adjustment state.
