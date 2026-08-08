# Chart of Accounts & Ledger Explorer

A complete **read-only** accounting workspace over the already-authoritative double-entry
engine. **No accounting semantics and no storage format changed** — every displayed value is
derived directly from `AuditJournal` / the ledger / the trial balance, with no duplicated
calculation, no cached balance, and no recomputation in the UI. This is the fifth vertical
(Customers · Invoices · Suppliers · Payments · **Ledger**) and introduces **no new write
authority**.

## Architecture review

The engine (`storage/AuditJournal`) was already complete and verified: `AccountOpened` /
`JournalEntryPosted` events; deterministic replay; derived account balances (`balanceFor`);
historical balances (`balanceAt(accountId, seq)`); the trial-balance invariant
(`trialBalanceTotal() == 0`, enforced because every entry balances at authoring); financial
statements (`incomeStatementAt` / `balanceSheetAt`); fixed posting authority (invoices post
Dr AR / Cr Revenue); compatibility + projection verification. This phase added only **read-only
enumerators** — projections of the private `accounts_`/`entries_`/`balances_` index, no new
semantics:

| Accessor | Returns |
|----------|---------|
| `listAccounts()` | every account (id, type, name, **derived** balance) |
| `listJournalEntries()` | every entry (id, date, postings, **reversal lineage**) |
| `entriesForAccount(id)` | the entries touching an account |
| `entryById(id)` | one entry (for the inspector) |

Reversal lineage is derived both directions: an entry's `reverses` (the entry it negates) is in
the payload; its `reversedBy` (the entry that negates it) is computed from the index.
`balanceAt` + `lastSeq` (already public) back historical inspection.

## Sign convention (surfaced consistently)

Postings are signed **DEBIT +, CREDIT −**, so an account's `balanceFor` is a signed Σ. The UI
shows the natural magnitude with its normal side: Asset/Expense are **debit-normal** (positive
Σ → "Dr"); Liability/Equity/Income are **credit-normal** (negative Σ → "Cr"). The Trial Balance
places each account's magnitude in the Debit column (Σ>0) or Credit column (Σ<0); the two
column totals are always equal because the trial balance is 0.

## UI flow & screens

One **Ledger** nav item opens a workspace (`LedgerWorkspace.qml`) with an internal tab bar:

- **Accounts** (`AccountsScreen.qml` ← `AccountsViewModel` / `AccountsListModel` /
  `AccountsFilterProxy`) — the chart of accounts: searchable, filterable by type, each row a
  derived balance + type badge + normal side; a summary with per-class rollups and a live
  **Balanced ✓ $0.00** indicator. A row **scopes the Journal to that account**.
- **Journal** (`LedgerExplorer.qml` ← `LedgerExplorerViewModel` → `LedgerEntriesModel`) — every
  balanced journal entry (date, description, Dr/Cr total, reversal/reversed badges), searchable,
  optionally **scoped to one account** (then each row shows that account's signed movement). A
  row opens the inspector.
- **Trial Balance** (`TrialBalanceScreen.qml` ← `TrialBalanceModel`) — one row per account with
  its Debit/Credit column and a totals row where **Σ debits == Σ credits**.
- **Journal Entry Inspector** (`JournalEntryInspector.qml`, a modal ← `ledgerVm.currentEntry`) —
  an entry's date, **balanced ✓** check, reversal lineage (*Reversal of #M* / *Reversed by #K*),
  and every posting (account · type · debit · credit). Each posting's account is **clickable →
  re-scopes the Journal** to it.

## Account navigation (business event → postings)

From an invoice row, a **📒 View revenue in ledger** action opens the Journal **scoped to the
Revenue account** — tracing the operational fact to the ledger accounts it moved. This is
deliberately **account-level**: `JournalEntryPosted` carries no back-reference to the
originating invoice/payment (see limitations), so the app navigates to the affected account
rather than fabricating a per-entry link. Within the accounting workspace, navigation is fully
connected: Accounts → Journal (scoped), inspector posting → Journal (re-scoped), clear-scope →
full journal.

## Posting inspection & reversal lineage

The inspector shows a real `JournalEntryPosted` as authored: balanced postings, and — for a
reversal — a back-pointer to the original (and, on the original, a forward-pointer to its
negation). Reversals are **append-only**; the original entry is never altered. The itest
authors a real reversal and asserts both lineage directions surface through `inspect()`.

## Historical balance inspection

`LedgerExplorerViewModel::balanceAtText(accountId, seq)` exposes the engine's already-supported
`balanceAt(accountId, seq)` — the account's balance reconstructed as of any history point. The
current head seq is `headSeq` (`lastSeq()`). (A scrubber UI over this is a natural follow-up;
the read path is proven by both test suites.)

## Integration (every value participates in)

Authoritative event history · deterministic replay · **projection verification** (`verifyAll`;
the accessors are replay-stable — proven in `ptest`) · **compatibility verification**
(`ACCT_COMPAT_VERIFY` full-model equivalence holds; **no new event types**) · derived ledger
state · financial reporting (trial balance stays 0 through all activity). No duplicated
accounting logic; **no repository or UI class is a write authority** — every B3 class only
*reads* `audit()`.

## Required invariants (maintained)

Read-only (no `record*`/mutation from any B3 class) · every value engine-derived (no cached or
recomputed balances) · trial balance stays 0 · deterministic replay + projection disposability
(`rebuildProjections` reproduces identical accessor output) · compatibility (no new event types
or format) · immutable reversal lineage (shown, never edited).

## Known limitations (genuine gaps, not built this phase)

- **No per-entry back-reference from a posting to its business event.** `JournalEntryPosted` is
  `[entryId][reversesEntryId][date][postings]` — adding a source ref is a storage-format change
  (out of scope). Operational→ledger navigation is therefore **account-level** (invoice → the
  Revenue account), not invoice → the one exact entry.
- **Payments do not post to the ledger** in this model — only invoice revenue does
  (`postInvoiceRevenue`, Dr AR / Cr Revenue). The Journal shows revenue postings + reversals;
  cash-receipt postings are not generated (a future accounting extension, not a UI one).
- **Read-only by design** — no account editing, posting editing, manual journal entries,
  reconciliation changes, tax logic, or reporting redesign (all explicit non-goals).
- **Historical balance** is exposed as a query (`balanceAtText`), not yet a seq scrubber UI.

## Future extensions

A `sourceRef` field on `JournalEntryPosted` (a governed storage evolution) to drill an invoice
to its exact entry; cash-receipt postings so payments appear in the Journal; dedicated
Income-Statement / Balance-Sheet screens on the existing `incomeStatementAt` / `balanceSheetAt`;
a history seq scrubber over `balanceAt`. All build on — not around — the authoritative ledger.

## Verification

`tools/itest.sh` — a **ledger-explorer** suite (the UI models mirror `listAccounts`/`balanceFor`
exactly; `TrialBalanceModel` Σdebit == Σcredit; the Journal count matches `entryCount`; account
scoping matches `entriesForAccount`; a real reversal's lineage surfaces through `inspect()` both
directions; trial balance stays 0; zero QML errors). `tools/ptest.sh` — `ledger UI accessors`
(enumerators match the index + reversal lineage + **replay-stable** after `rebuildProjections`).
`ACCT_COMPAT_VERIFY` — full-model equivalence. Launch → the nav shows the **Ledger** workspace;
Accounts · Journal · Trial Balance · the entry inspector, all populated from the engine (EN +
AR-RTL).
