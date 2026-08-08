# Design Review: Deterministic Business-Event → Ledger Posting Authority v1

Status: **implemented + verified** (`tools/ptest.sh` — 150 in-process assertions + 18
cross-process crash recoveries; generated postings inherit the proven ledger path).
Closes the loop between the operational layer and the ledger
(`docs/ledger-double-entry.md`, `docs/financial-statements.md`).

> The operational→financial mapping was external/manual. This makes it a **fixed,
> hardcoded, inspectable policy** that emits **real authoritative posting events** —
> not a configurable rules engine, DSL, or hidden heuristic.

---

## 1–4. Architecture, classification, mapping, policy model

A **role registry** binds account *roles* (Receivable, Revenue, Cash) to concrete
account ids (`setPostingAccounts`). A **fixed policy** maps each operational fact to a
**balanced** journal entry against those roles:

| Operational fact | Policy posting |
|------------------|----------------|
| invoice (total) | `postInvoiceRevenue` → **Dr AR / Cr Revenue** |
| payment (amount) | `postPaymentReceipt` → **Dr Cash / Cr AR** |
| reversal | `reverseJournalEntry(entryId)` → sign-flipped compensating entry |

The policy is **C++ code, not data** — no scripting, no configuration DSL, no plugin
system (explicitly out of scope). Each generated posting is a real `JournalEntryPosted`
event recorded through `recordJournalEntry`, so it is **balanced, period-checked, and
crash-safe** like any entry.

## 5. Reversal-posting semantics

A generated posting is reversed by `reverseJournalEntry` → a new sign-flipped balanced
entry linked by `reversesEntryId` (append-only, original untouched). Proven: reversing
an invoice posting nets AR + Revenue to 0.

## 6–8. Replay / reconstruction / projection interaction

**Determinism of the ledger comes from the persisted posting events, not the role
config.** A second journal with no `setPostingAccounts` reproduces identical balances by
replaying the stored postings (proven). Statements reconstruct from those postings
(proven: an operational invoice yields revenue in the income statement; payment receipt
settles AR; the balance sheet balances). The posting events feed the disposable ledger
index; entity projections are untouched, so `verify()`/content-hash is unaffected.

## 9. Period-close interaction

Generated postings go through `recordJournalEntry`, which **rejects a closed-period
effective date** — so a post-close posting must be an adjusting entry in an open period,
exactly like manual entries. Reversals are new entries (allowed forward).

## 10–11. Verification & stable identity

Generated postings reference accounts by **stable role-bound ids**; entries get stable
monotonic ids. No positional assumptions. The ledger index is subordinate to the events
and rebuilt from them.

## 12. Crash / recovery guarantees (proven)

A generated posting IS a journal entry — one atomic `EventLog.append` + index rebuild.
The `ledger-crash@afterEventBeforeProject` test covers it: killed mid-post → reconcile
replays; the entry is fully present or absent and the trial balance is always 0. No
orphan/half-generated posting.

## 13. Migration implications

None — the policy and role registry are code + runtime config; all financial state is
events + derived indices. Schema migration stays orthogonal.

## 14. Deterministic validation harness (all green)

`ACCT_PTEST=suite`: business event → Dr AR / Cr Revenue; revenue in the income
statement; balanced (trial 0); reversal nets to 0; payment receipt settles AR; balance
sheet balances; deterministic rebuild without the role config. Crash-safety via
`ledger-crash`.

## 15. Observability / diagnostics & rollout & "if the policy evolves" (Q9)

Startup already reports `trialBalance=0` (canary). **Policy evolution is safe by
construction**: existing postings are persisted events and replay unchanged; only NEW
operational facts use the new policy. A policy version stamp on future generated entries
(to attribute postings to a policy revision) is the documented next step. Rollout: v1
lands roles + invoice/payment policies + reversal at the `AuditJournal` layer, proven.
Next: wire the live invoice/payment commit paths to auto-post via the policy (Phase-3
cutover), add tax/discount lines as additional policy postings, and a policy-version
stamp.

## 16. Explicit non-goals

No rules engine / accounting DSL / scripting / plugins; no tax engine; no multicurrency;
no ERP workflow orchestration; no mutable posting rewrites; no hidden heuristics; no
distributed/async. Local-first, deterministic, hardcoded-and-inspectable.

---

## Critical design questions — answered

1. *Which events post?* invoice, payment (v1); each via a fixed policy. 2.
   *Hardcoded/declarative/hybrid?* **hardcoded** (code, inspectable) — no DSL. 3.
   *Posting identity?* stable entry ids; role-bound account ids. 4. *Reversals?*
   `reverseJournalEntry` (sign-flipped, linked). 5. *Corrections → ledger?* a correction
   posts a new policy entry / reversal. 6. *Post-close adjustments?* new entries in open
   periods (closed rejected). 7. *Linked historically?* posting events in seq order +
   `reversesEntryId`. 8. *Statements from operational history?* postings → ledger →
   `*At(seq)`. 9. *Policy evolves?* old postings replay unchanged; new facts use new
   policy (version stamp = next). 10. *Version mismatch?* refuse unknown event types. 11.
   *Projection hashes?* unaffected — postings feed a separate index. 12. *Operator
   mistakes?* unbalanced/closed-period/unconfigured-roles rejected with precise messages.
   13. *Historical exports?* deterministic via `*At(seq)`. 14. *Postings verified vs
   events?* both are authoritative events; the ledger rebuilds deterministically and the
   trial balance is structurally 0.
