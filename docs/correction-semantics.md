# Design Review: Structured Reversal & Adjustment Semantics v1

Status: **implemented + verified** (`tools/ptest.sh` — 109 in-process assertions + 15
cross-process crash recoveries incl. atomic void). Builds on the event-authored,
period-finalized architecture (`docs/audit-journal.md`, `docs/period-closure.md`).

> The system could already freeze periods and reject illegal mutation. It lacked
> *formal correction intent*: the difference between voiding, reversing, and amending
> was operator convention, not encoded fact. This makes intent a first-class,
> auditable, replayable event — distinct from "latest value wins."

---

## 1. Correction semantic model (the distinctions)

| Intent | What it is | Touches the original? | Closed period? |
|--------|-----------|-----------------------|----------------|
| **Amendment** (`InvoiceCorrected`) | edit the transaction's content in place | mutates its standing | **rejected** |
| **Void** (`InvoiceVoided`) | mark not-effective in place (`status → VOID`) | mutates its standing | **rejected** |
| **Reversal** (`InvoiceReversed`) | append a NEW negating transaction, linked to the original | original unchanged | **allowed** |

The dividing line is *standing vs compensation*: amendment and void change a
transaction's own standing (forbidden once frozen); a reversal is a **new compensating
transaction** that leaves the original on the books (the sanctioned post-close path).
Supersession/adjustment are the same shape (a new linked entry) and are the documented
next extensions (§15).

## 2. Reversal architecture

`recordInvoiceReversal(originalId, reversalInvoice, lines)` records the reversal as a
normal `InvoiceCreated` (its own stable id, deterministic line ids — §10) and then an
`InvoiceReversed{target=originalId, related=reversalId}` link. Two append-only events;
the original event is never touched. Allowed regardless of the original's period —
because it adds, it does not mutate. Proven: reversing a closed-period invoice succeeds,
the original stays `POSTED`, the books gain a 3rd (negating) transaction.

## 3. Adjustment-event model

Void/reversal payloads are uniform: `[u32 targetId][u32 relatedId]`. Void's `relatedId`
is `NONE`; reversal's is the negating entry's id. The *event type is the intent* — the
most explicit, auditable encoding (no ambiguous flags). A general compensating
adjustment is a reversal-shaped link to an independent entry (next increment).

## 4. Correction lineage representation

A **disposable index** (`corrections_`: `invoiceId → {voided, reversedBy}`) rebuilt by
scanning the void/reversal events — never a hidden mutable authority. `isVoided(id)`,
`reversedBy(id)`, `correctionCount()`. Proven to rebuild deterministically from history.

## 5. Historical interpretation rules

Void projects `status = VOID` into the invoice at its event's seq. So reconstruction is
exact: **before** the void the invoice was `POSTED`; **at/after** it is `VOID` (proven).
A reversal leaves the original visible at every seq and adds the negating entry from its
own seq forward — both "what happened" and "what corrected it" are preserved.

## 6. Replay / reconstruction interaction

`InvoiceVoided` is idempotent + deterministic (set `VOID`, upsert-at-stable-id);
`InvoiceReversed` is lineage-only (no entity change — the reversal entry is its own
`InvoiceCreated`). Replaying a correction chain reproduces identical projections,
identical statuses, and identical lineage (proven: content-identical rebuild +
index rebuild).

## 7. Projection interaction

Void mutates only the target invoice's `status` (a projection write, not a history
edit). The lineage index is a separate disposable projection. Entity-projection
verification (`verify()`) and content fingerprints are unaffected by lineage.

## 8. Period-close interaction (the crux)

Void/amendment of a closed-period transaction → **rejected** (changes frozen standing).
Reversal of a closed-period transaction → **allowed** (append-only compensation). This
is exactly how `docs/period-closure.md`'s freeze interacts with intent.

## 9. Reporting-state semantics

`status = VOID` is the effective current standing; the lineage index gives "reversed by
N". "Books as closed" (`reconstructAllInto(closedAtSeq)`) naturally reflects only the
voids/reversals that existed at the freeze point — later corrections appear in current
books, not in the closed snapshot.

## 10. Stable identity interaction

Corrections target **stable authoritative ids** (the invoice id; reversal entries get
fresh stable ids + deterministic line ids). Never positional, never projection-relative.
"Reversal of a reversal" is just a reversal whose target is the reversal entry's id —
the chain is id-linked and append-only.

## 11. Crash / recovery guarantees (proven)

Void uses the same write-ahead/commit/project/cursor protocol as every event. Killed at
`afterEventBeforeProject` → reconcile replays + projects; at `afterProjectBeforeCursor`
→ idempotent re-apply. Either way the projected `status` and the lineage index **agree**
(both void or both not) — no half-applied correction chain, no orphan compensation, no
ambiguous supersession. (Cross-process kill tests, both windows.)

## 12. Migration implications

No record-layout change — void reuses the existing `status` field; lineage lives in
events + a derived index. Schema migration (record layer) stays orthogonal and green.

## 13. Deterministic validation harness (all green)

`ACCT_PTEST=suite`: void in place; void rejected on closed period; reversal allowed on a
closed-period original; lineage (`reversedBy`); original unchanged by reversal;
historical interpretation (POSTED→VOID across the void seq); deterministic rebuild of
status + lineage. Cross-process `void-crash@afterEventBeforeProject/afterProjectBeforeCursor`.

## 14. Operator workflow model

The UI (next increment) reads `isInvoiceInClosedPeriod` + `isVoided` to offer the right
action: open period → amend or void; closed period → reverse only. A rejected void/amend
throws a precise message steering to a reversal — operator mistakes are surfaced, not
silently mutated.

## 15. Observability / diagnostics & rollout

Startup `acct.storage`: `… corrections=K`. Rollout: v1 lands void + reversal +
lineage + period interaction at the `AuditJournal` layer, proven. Next: a structured
**supersession** event (`target → replacement`, replacement effective, original
`status=VOID`) and a **compensating-adjustment** helper — both the same linked-entry
shape; then surface intent in the UI alongside the Phase-3 invoice commit cutover.

## 16. Explicit non-goals

No mutable amendment of frozen entries; no destructive editing; no generic undo; no
implicit auto-healing of accounting meaning; no tax/regulatory doctrine; no ERP
abstraction; no reporting dashboards; no distributed/async. Local-first, deterministic,
append-only.

---

## Critical design questions — answered

1. *Reversal vs adjustment vs supersession?* reversal = negating linked entry, original
   stays; void = in-place not-effective; supersession (next) = replacement becomes
   effective, original VOID. 2. *Symmetric or semantic?* semantic — a reversal is an
   intent-bearing linked event, not byte-negation. 3. *Chains?* id-linked append-only
   events + derived index. 4. *Linked to ids?* stable authoritative ids only. 5.
   *Effective current state?* the live projection (`status`) + lineage index. 6.
   *Books-as-closed after later adjustments?* reconstruct at `closedAtSeq` — later
   corrections excluded. 7. *Post-close adjustments surfaced?* as later events, in
   current books only. 8. *Reverse a reversal?* yes — target the reversal entry's id.
   9. *Voided-but-historical?* `status=VOID` from the void seq; earlier seqs show the
   prior status. 10. *Reporting across chains?* status + lineage, reconstructible at any
   seq. 11. *Version mismatch?* refuse unknown event types. 12. *Projection hashes?* void
   changes the invoice record (status) → reflected in the fingerprint; lineage is
   separate. 13. *Operator mistakes?* rejected with a precise message. 14. *Deterministic
   exports?* content-identical rebuilds + reconstructible-at-seq.
