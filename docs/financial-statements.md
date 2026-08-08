# Design Review: Deterministic Financial Statement Reconstruction & Closing Semantics v1

Status: **implemented + verified** (`tools/ptest.sh` — 143 in-process assertions + 18
cross-process crash recoveries; closing entries inherit the proven ledger-posting path).
Builds on the double-entry ledger (`docs/ledger-double-entry.md`).

> Statements are **derived from postings at any seq** — never stored, never cached.
> Net income flows into equity via **real closing entries** (append-only events), so
> "as filed" statements reconstruct identically forever.

---

## 1–2. Statement architecture & account classification

Each account carries a `type`: **Asset / Liability / Equity** (balance-sheet) or
**Income / Expense** (income-statement). Postings stay signed (DEBIT +, CREDIT −).
Statements are computed by replaying `JournalEntryPosted` events up to a seq and
summing by classification — a **disposable derivation**, subordinate to the events.

## 3. Statement reconstruction rules

```
incomeStatementAt(seqN):  income  = Σ −balance(income accounts)     # revenue (credit) magnitude
                          expense = Σ  balance(expense accounts)    # expense (debit) magnitude
                          netIncome = income − expense
balanceSheetAt(seqN):     assets      = Σ balance(asset)
                          liabilities = Σ −balance(liability)
                          equity      = Σ −balance(equity) + netIncome   # current earnings into equity
                          balances    = (assets == liabilities + equity)
```

**The balance sheet ALWAYS balances** — not by a check but because the trial balance is
0: `assets + liab + equity + income + expense = 0` (internal) ⇒
`assets = liabilities + (−equity_internal − income − expense) = liabilities + equity`.
Proven before and after closing, and post-adjustment.

## 4. Closing-entry semantics & 9. explicit vs derived

**Explicit events, not derived magic.** `recordClosingEntry(retainedEarnings, date)`
builds a **real balanced journal entry** that zeroes every income/expense account
(posting `−balance` to each) with the net offset to retained earnings — then posts it
through `recordJournalEntry` (so it is balanced + period-checked + crash-safe like any
entry). After it, income/expense balances are 0 and the next period starts fresh
(proven: income/expense → 0, current net income → 0).

## 5. Retained-earnings flow

The closing entry's offsetting posting moves the period's net income into the retained
earnings (equity) account (proven: `$70` net income → retained earnings credit
`−7000`). Retained earnings is therefore a **real account balance reconstructed from
postings**, never a mutable cache.

## 6. Historical statement reconstruction

`incomeStatementAt`/`balanceSheetAt` take a seq, so any historical statement is exact
and reproducible (proven: net income was `$100` before the expense was posted). "As
closed" statements = reconstruct at the period's `closedAtSeq`; "current" = at
`lastSeq()`.

## 7. Post-close adjustment interaction

A post-close adjusting entry is an ordinary new balanced entry in an open period. It
affects the **current** income statement (proven: a post-close `$20` revenue shows net
income `$20` — only the new activity, since closing reset the prior period) but **not**
the as-closed reconstruction (that replays only up to `closedAtSeq`). History is never
rewritten.

## 8. Replay / reconstruction interaction

Statements are pure functions of the posting history (replay + classify). Identical
history ⇒ identical income statements, balance sheets, retained earnings, and as-of
statements (proven: a fresh journal reproduces retained earnings + a balancing sheet,
trial balance 0).

## 9–10. Projection & verification interaction

Statement derivation reads the ledger events only; it writes nothing. So entity
`verify()`/content-hash is untouched, and there is no statement projection to drift —
the figures are recomputed from authoritative events on demand.

## 11. Stable identity interaction

Accounts and entries use stable monotonic ids (from the ledger layer). Closing entries
reference the retained-earnings account by stable id; statement membership is by account
classification, never position.

## 12. Crash / recovery guarantees (proven)

A closing entry IS a journal entry — one atomic `EventLog.append` + index rebuild.
Killed mid-post (`ledger-crash@afterEventBeforeProject`) → reconcile replays; the entry
is fully present or absent and the **trial balance is always 0**, so retained earnings
and statements can never be left in a half-closed/inconsistent state. (Cross-process
kill test, inherited from the ledger.)

## 13. Migration implications

No record-layout change — accounts/entries/statements live entirely in events + derived
computation. Schema migration (record layer) stays orthogonal and green.

## 14. Deterministic validation harness (all green)

`ACCT_PTEST=suite`: income statement (income/expense/net); **balance sheet balances**;
historical income statement; closing resets income/expense; net income → retained
earnings; post-close balance sheet still balances; post-close adjustment shows only new
activity; deterministic rebuild. Closing crash-safety via `ledger-crash`.

## 15. Observability / diagnostics & rollout

Startup `acct.storage` already reports `trialBalance=0` (a canary — non-zero is
structurally impossible). Rollout: v1 lands classification + income statement + balance
sheet + closing entries + retained-earnings flow + historical statements at the
`AuditJournal` layer, proven. Next: a multi-period close ledger (per-period retained
earnings roll-forward), the business-event → posting mapping, and a read-only statement
UI/export surface (all reconstructed via `*At(seq)`).

## 16. Explicit non-goals

No dashboards/graphs/BI; no tax engine; no GAAP/IFRS specialization; no multicurrency
consolidation; no mutable retained-earnings cache; no hidden reporting snapshot as
authority; no ERP reporting layer; no distributed/async. Local-first, deterministic,
replay-correct.

---

## Critical design questions — answered

1. *Classification?* account `type` (Asset/Liability/Equity/Income/Expense). 2.
   *Reconstructed how?* replay postings ≤ seq, sum by class. 3. *Income-statement
   account?* Income/Expense. 4. *Balance-sheet account?* Asset/Liability/Equity. 5.
   *Closing entries?* real balanced journal entries (explicit events). 6. *Net income →
   equity?* the closing entry posts it to retained earnings. 7. *"As closed"?*
   reconstruct at `closedAtSeq`. 8. *Post-close adjustments?* new entries in current
   statements, excluded from as-closed. 9. *Explicit or derived?* explicit events. 10.
   *Retained earnings historically?* a real account balance via replay. 11. *Version
   mismatch?* refuse unknown event types. 12. *Projection hashes?* unaffected —
   statements are recomputed, not projected. 13. *Operator mistakes?* unbalanced /
   closed-period / nothing-to-close rejected with precise messages. 14. *Historical
   exports?* `incomeStatementAt`/`balanceSheetAt(seq)` — deterministic at any point.
