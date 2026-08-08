# Design Review: Deterministic Ledger & Double-Entry Posting Semantics v1

Status: **implemented + verified** (`tools/ptest.sh` — 135 in-process assertions + 18
cross-process crash recoveries incl. atomic balanced posting). Builds on the
event-authored, period-finalized, settlement-aware architecture.

> The financial-state foundation. Balances are **derived from postings, never a stored
> running balance**; every journal entry **balances by construction** (Σ debits = Σ
> credits), so the trial-balance total is always 0 and no imbalance can persist.

---

## 1–3. Ledger / account / journal-entry model

| Concept | Definition |
|---------|-----------|
| **account** | `AccountOpened{accountId, type, name}` — event-authored, stable id |
| **posting** | a signed amount on an account: **DEBIT = +, CREDIT = −** |
| **journal entry** | `JournalEntryPosted{entryId, reversesEntryId, effectiveDate, [postings]}` — a balanced set |
| **balance(account)** | derived: Σ its postings (signed) |
| **trial balance** | all balances; global Σ ≡ 0 (an invariant, not a check) |

Signed-amount postings make double-entry a single deterministic rule: **balanced ⟺ Σ
amounts == 0**, and account balance = Σ signed postings. Display sign by account type is
a presentation concern; the internal representation is uniform.

## 4. Double-entry balancing (enforced, not advisory)

`recordJournalEntry` computes `Σ amountCents` and **rejects** any entry that is not 0
(proven: a `Dr 100 / Cr 50` entry is refused). So no unbalanced entry ever reaches the
log, and `trialBalanceTotal()` is 0 at every seq (proven across postings + a reversal).

## 5. Historical balance reconstruction

`balanceAt(accountId, seqN)` replays `JournalEntryPosted` events with `seq ≤ N`,
summing that account's postings — the balance as of any point (proven: Cash was 0
before its entry, $100 after). A full trial balance as of N is the same replay across
accounts. This is the deterministic basis for as-of financial statements.

## 6. Posting authority (the key decision)

**Journal entries are first-class authoritative events**, balanced at authoring — *not*
implicitly derived from business events. Auto-posting (e.g. `InvoiceCreated` → `Dr AR /
Cr Income`) is a **mapping layer that would emit journal entries**, kept out of the core
because the debit/credit doctrine per transaction type is exactly the premature
specialization the brief forbids. The ledger guarantees integrity of whatever is
posted; the mapping is a future, separately-testable layer.

## 7. Reversal / adjustment postings

`reverseJournalEntry(entryId, date)` posts a **new** entry with every posting
sign-flipped (still balanced) and `reversesEntryId = original` for lineage —
append-only, the original untouched (proven: reversing nets Cash to 0, 3 entries on the
books, trial balance still 0). Adjustments are ordinary new balanced entries.

## 8–9. Replay / projection interaction

`apply()` treats `AccountOpened`/`JournalEntryPosted` as **no-ops for entity
projections** — the ledger is a separate **disposable index** (`accounts_`, `entries_`,
`balances_`) rebuilt from events (`rebuildLedgerIndex`). So entity rebuilds + content
fingerprints are unaffected, and the ledger rebuilds deterministically (proven: a fresh
journal reproduces identical balances, trial balance 0).

## 10. Verification / drift interaction

The ledger index is subordinate to the events and rebuilt from them, so it cannot drift
without the log drifting (which `EventLog`'s committed-region CRC already detects).
Entity `verify()`/content-hash is orthogonal (the ledger touches no entity projection).

## 11. Period-close carry-forward

A journal entry whose **effective date** is in a closed period is **rejected** (post an
adjusting entry in an open period) — proven. Carry-forward in v1 is the deterministic
`balanceAt(closedAtSeq)` (balance-sheet accounts carry as their as-of-close balance);
explicit closing entries (zeroing income/expense into equity) are the documented
next extension, expressible as ordinary balanced journal entries.

## 12. Crash / recovery guarantees (proven)

A posting is one `EventLog.append` (its own write-ahead record) + an index rebuild.
Killed at `afterEventBeforeProject` → reconcile replays + rebuilds the index. The entry
is fully present or fully absent (Cash 0 or $100), and **the trial balance is always 0**
— a balanced entry can never be half-applied (it is one atomic event). The ledger is
index-only, so there is no second projection window. (Cross-process kill test.)

## 13. Migration implications

No record-layout change — accounts/entries/balances live entirely in events + a derived
index. Schema migration (record layer) stays orthogonal and green.

## 14. Deterministic validation harness (all green)

`ACCT_PTEST=suite`: accounts opened; balanced entry posts Dr/Cr; **unbalanced rejected**;
trial balance 0 across entries; `balanceAt` historical; reversal nets to 0 + lineage +
append-only; closed-period posting rejected; deterministic rebuild (trial balance 0).
Cross-process `ledger-crash@afterEventBeforeProject`.

## 15. Observability / diagnostics & rollout

Startup `acct.storage`: `… accounts=N ledgerEntries=E trialBalance=0`. A non-zero
`trialBalance` would be a loud red flag (it is structurally impossible, so it is a
canary). Rollout: v1 lands accounts + balanced postings + trial balance + historical
balances + reversal + period interaction at the `AuditJournal` layer, proven. Next: the
business-event → posting mapping layer; explicit period-close closing entries; an as-of
trial-balance / financial-statement reconstruction surface.

## 16. Explicit non-goals

No mutable running balances; no hidden balance cache as authority; no multicurrency; no
tax engine; no GAAP/IFRS specialization; no ERP workflows; no reporting dashboards; no
distributed/async. Local-first, deterministic, append-only, double-entry-correct.

---

## Critical design questions — answered

1. *Account?* `AccountOpened` event (id, type, name). 2. *Identity?* stable monotonic
   id. 3. *Journal entry?* a balanced `JournalEntryPosted` (Σ == 0). 4. *Debit/credit?*
   signed posting amount (Dr +, Cr −). 5. *Balancing enforced?* rejected at authoring if
   Σ ≠ 0. 6. *Derived or authored?* authored events; mapping layer is future. 7.
   *Reversals?* a new sign-flipped balanced entry, lineage `reversesEntryId`. 8.
   *Closed-period adjustments?* rejected in the closed period; post in an open one. 9.
   *Balances at seq N?* `balanceAt` replay. 10. *Carry-forward?* `balanceAt(closedAtSeq)`
   (closing entries = future). 11. *Trial balance deterministic?* Σ derived balances ≡ 0
   on every replay. 12. *Version mismatch?* refuse unknown event types. 13. *Projection
   hashes?* unaffected — ledger is a separate index. 14. *Operator mistakes?* unbalanced
   / closed-period postings rejected with precise messages.
