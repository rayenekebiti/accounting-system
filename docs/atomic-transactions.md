# Design Review: Deterministic Atomic Business Transaction Semantics v1

Status: **implemented + verified** (`tools/ptest.sh` — 194 in-process assertions incl. the
atomic-transaction section + a 3-point cross-process crash family proving the two-state
guarantee; `tools/itest.sh` — 39). The last authority split is closed with one minimal
primitive; every prior invariant is preserved unchanged.

> A single logical accounting operation must be **one indivisible authoritative fact** —
> not database transactions, but preserved business *meaning*. This is the final
> consolidation: no operation's accounting meaning can be temporarily split across
> independently committed events.

---

## 1. The gap (and the rigorous conclusion)

After the Full Domain Cutover, an invoice commit authored **two** authoritative events —
`InvoiceCreated` then `JournalEntryPosted` (revenue) — via two separate `EventLog::append()`
calls. `recordInvoiceReversal` had the same shape (reversal `InvoiceCreated` +
`InvoiceReversed`). A crash strictly between the two left the operational fact committed and
its financial interpretation absent: a real, if narrow, authority split.

**Is the architecture already sufficient?** Its *design* is — its *API* was not. The
EventLog has a **single commit point**: `committedLength_` in the header. `append()` writes a
frame durably *past* that point (①), then advances `committedLength_` once and fsyncs the
header (② — the commit); on open, any durable-but-uncommitted tail past `committedLength_` is
truncated (`openOrCreate()`). This already gives all-or-nothing for **one** event. The split
existed only because nothing wrote **several** frames before that single advance.

## 2. Chosen model — grouped commit (the smallest change)

`EventLog::appendAtomic(std::vector<FrameSpec>)`: write **all** N frames durably past
`committedLength_`, then advance `committedLength_` **once** past the whole group. The seqs
are contiguous and gap-free. This is the entire mechanism — no new event type, envelope,
commit marker, or event-store change.

**Rejected alternatives:** a *composite event* (`InvoiceCommitted` carrying entity +
postings) changes the event model and forces `rebuildLedgerIndex`/replay/compat to
special-case it; a *commit marker / transaction envelope* adds a new event type and replay
rule for state the commit point already encodes. Grouped commit keeps the **exact same
committed events**, so replay, snapshot, compatibility, and reconstruction are untouched.

## 3. Failure-mode analysis — two observable states, nothing between

| Interruption point | Disk state | On reopen | Observable |
|--------------------|-----------|-----------|------------|
| before first frame | nothing written | — | **ABSENT** |
| **after first frame, before commit** (`afterTxnFirstFrame`) | frame(s) durable, `committedLength_` unmoved | uncommitted tail **truncated** | **ABSENT** |
| **after the single commit** (`afterTxnCommit`) | all frames ≤ `committedLength_` | all valid; `reconcile` projects them | **COMPLETE** |
| after projection, before cursor (`afterProjectBeforeCursor`) | committed | idempotent `reconcile` | **COMPLETE** |

The critical row is the middle one: even though the first event's bytes are physically on
disk, the group is **not committed** until `committedLength_` advances, so it is absent. No
interruption yields "invoice without posting" (or vice versa).

## 4. Authoring the multi-event operations atomically

`AuditJournal::recordInvoiceWithRevenue(inv, lines, correction, revenueDeltaCents,
effectiveDate, ts)` validates + assigns stable ids (failing before authoring), then builds
the frame group **deterministically**: the invoice frame (`InvoiceCreated`|`InvoiceCorrected`)
plus, iff `revenueDeltaCents != 0` && roles bound && the date is open, the
`JournalEntryPosted` frame (Dr AR / Cr Revenue via the versioned `PostingPolicy`). One
`appendAtomic`, then project the invoice + `rebuildLedgerIndex`, then advance the cursor. The
include/omit decision is made *before* authoring, so the committed group is fixed —
draft/closed-period invoices deterministically author an invoice-only group (a business rule,
not a crash split). `recordInvoiceReversal` is likewise refactored to `appendAtomic` its two
frames. The editor calls the one method.

## 5–9. Event-log / replay / snapshot / compatibility / reconstruction / ledger interaction

Because the **committed events are identical** to before (just grouped), all downstream
machinery is unchanged and simply observes the two events as always-both-or-neither:
- **Replay / reconstruction**: same `apply()` stream → byte-identical rebuilds; `verifyAll`
  (full model) passes in both the absent and complete states.
- **Snapshot**: the ledger snapshot memoizes balances at a seq; a group commits at a seq
  boundary, so a snapshot is never taken mid-group. Still disposable, genesis-verified.
- **Compatibility governance**: no version bump — no new event type or replay semantics.
  `ACCT_COMPAT_VERIFY` still proves full-model equivalence.
- **Ledger**: the posting can no longer lag the operational fact across a crash; the trial
  balance stays structurally 0 in every observed state.

## 10. Crash-recovery analysis

Recovery is the **existing** `openOrCreate()` truncation + `reconcile()` — no new recovery
code. Before the commit point: the group is a discardable tail → truncated. After: `reconcile`
replays the committed group whole into the disposable projection (idempotent). The projection
+ cursor self-heal exactly as for single events.

## 11. Verification (real process interruption)

`ACCT_PTEST=suite`: `appendAtomic` contiguous seqs + survives-reopen; atomic invoice+revenue
lands both; draft omits the posting; atomic reversal commits invoice + lineage together.
Cross-process `txn-crash@{afterTxnFirstFrame, afterTxnCommit, afterProjectBeforeCursor}`: the
follow-up process observes **absent** (0 invoices AND 0 entries) or **complete** (1 invoice
AND 1 entry, trial balance 0, revenue recognised) — never split — and `verifyAll` passes in
both. `itest` confirms the live editor still posts, now as one atomic fact.

## Required invariants (satisfied)

Atomic business meaning (fully committed or absent — proven) · immutable history (append-only,
no mutation/deletion) · deterministic replay (identical business state, ledger, balances,
statements regardless of interruption) · historical interpretability (same events; no hidden
state) · snapshot compatibility (disposable, subordinate) · compatibility preservation (no
version change, no reinterpretation) · crash safety (no orphan/duplicate/divergence, proven at
three interruption points).

## Constraints honoured

No database transactions, distributed coordination, workflow engine, event-store redesign, new
accounting concepts, or speculative abstractions. One primitive (grouped commit), reusing the
existing commit point.

## Honest limitations

- Atomicity is over the **authoritative log**; the disposable projection + cursor self-heal via
  `reconcile` (unchanged). The observable guarantee is exactly the two-state property.
- `appendAtomic` is single-process (the existing `QLockFile` single-writer assumption holds).
  It groups the small, bounded frame sets of the two real multi-event operations — it is not a
  general nested/large-batch transaction facility.
- Draft / closed-period / unconfigured-roles still deterministically **omit** the posting frame
  (decided before authoring, so no crash split). Retroactive posting of such invoices remains
  the documented next step.

## Critical design questions — answered

1. *Objective?* preserve business meaning, not DB transactions. 2. *Already sufficient?* the
   commit-point design is; the API wasn't — grouped commit closes it. 3. *Chosen model?*
   `appendAtomic` (grouped commit). 4. *Event-log implications?* one extra API; same committed
   events. 5. *Replay?* unchanged. 6. *Snapshot?* unchanged (group commits at a seq boundary).
   7. *Compatibility?* no version bump; full-model verify still holds. 8. *Reconstruction?*
   identical. 9. *Ledger?* posting never lags the invoice across a crash. 10. *Recovery?*
   existing truncation + reconcile. 11. *Proof?* three interruption points → absent-or-complete,
   verifyAll clean.

## Integration status & next increment

Live: the invoice editor authors invoice + revenue as one atomic fact; reversal is atomic;
`appendAtomic` is the single new primitive. The platform now has **one** authoritative history,
**one** transaction model, **one** replay model, **one** compatibility model, and **one** crash
model. Next: retroactive ledger posting for backfilled/closed-period invoices, and the same
grouped-commit primitive for any future multi-event operation.
