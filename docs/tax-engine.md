# Deterministic Tax Engine & VAT/GST Workflow

A tax subsystem that becomes authoritative by being **posted**, with no mutable state, no hidden
authority, and no runtime rule engine. It is the first real accounting-semantic evolution of the
platform — **posting policy V1 → V2** — integrated cleanly with the compatibility-governance layer.

## Architectural review (posting-as-authority; minimum, non-breaking extension)

The engine already had everything tax needs; the review found the minimum additions:

- **Posting is the authority.** An invoice's tax and an expense's recoverable tax are ledger
  postings (`JournalEntryPosted` events). Because events are immutable, the tax **amount** is
  baked in forever — replay is identical and a five-year-old invoice keeps its original tax.
- **No storage-format break (event payloads are immutable).** Invoice net/tax already exist
  (`InvoiceTotals::computeInvoiceTotals` → `{subtotal, tax, total}` with `subtotal+tax==total`;
  the `Invoice` header stores subtotal/taxAmount; lines store `taxRatePermille`). The `Expense`
  record's tax rate went into its **reserved bytes** (record size unchanged → old events decode
  as rate 0). Tax codes are a **new append-only event type** (`TaxCodeCreated = 22`) + a
  disposable index — the same pattern by which Suppliers/Expenses were added.
- **Single tax accounts** (per the spec): "Tax Payable" (Liability), "Recoverable Tax" (Asset),
  bootstrapped by `ensureChartOfAccounts`. Reports are just `balanceAt` of those two.

**No rules engine, scripting, plugin, mutable cache, or runtime heuristic** — the posting policy
and tax formula are fixed, inspectable C++.

## Accounting model

| Fact | Postings (policy V2) |
|------|----------------------|
| Taxable invoice | **Dr Accounts Receivable** (net+tax) / **Cr Revenue** (net) / **Cr Tax Payable** (tax) |
| Zero-rated / exempt invoice | Dr AR / Cr Revenue (the tax line is omitted at tax = 0) |
| Expense with recoverable tax | **Dr Expense** (net) / **Dr Recoverable Tax** (tax) / **Cr Cash \| AP** (net+tax) |
| Reversal / void | the sign-flipped compensating entry (same tax formula → cancels exactly) |

Every entry balances by construction (`subtotal + tax == total`; `taxOnNet` is a single truncating
integer formula), so the **trial balance is always structurally 0**. `taxOnNet(net, ratePermille)
= net·rate/1000` is used both when authoring a posting and when re-deriving it for a void/reversal,
so the compensating entry cancels to the cent.

## Deterministic guarantees

- **Tax derived from authoritative events.** No mutable "tax paid" flag; "collected" and
  "recoverable" are ledger balances, never stored or recomputed at reporting time.
- **Replay-identical.** Tax postings are `JournalEntryPosted` events; `taxSummaryAt` replays them
  and is byte-stable after `rebuildProjections` (proven in `ptest`).
- **Historical rule preservation.** Changing today's VAT 15% → 17% authors a **new version** of a
  tax code family; the resolver (`resolveRateAt(family, date)`) returns the rate effective on a
  date, but authoring **captures** the resolved rate onto the line/posting — so no historical
  invoice is ever reinterpreted.
- **Trial balance == 0** through creation, correction, void, reversal, and closing.

## Policy evolution (append-only, effective-dated)

A `TaxCode` is `{id, family, version, type (VAT/GST/Sales Tax/Zero-rated/Exempt), ratePermille,
effectiveDate, name}`. `recordTaxCode` is append-only; re-recording a name records a **new
version** of that family. `resolveRateAt(family, asOf)` = the highest-version code effective on
or before `asOf` — deterministic, no runtime rules. Defaults (Standard 15% / Zero-rated / Exempt)
are bootstrapped idempotently.

## Historical compatibility (governance)

Splitting revenue is a real change to the fixed posting mapping, so it **bumps
`postingPolicy` V1 → V2** — the first entry in the semantic-migration registry
(`Migration{Accounting,"postingPolicy",1,2}`). The migration is a **proven no-op for historical
data**: every pre-V2 posting is a persisted event and replays byte-for-byte unchanged; V2 only
maps NEW facts. On open, `adoptVersionTransition` appends a V2 stamp when the build's contract
exceeds the head stamp on a registered axis, so an existing V1 book advances to V2 and classifies
**Compatible**; a book from a *newer* build (V3) is still **refused** (downgrade protection). The
replay-equivalence gate (`ACCT_COMPAT_VERIFY`) proves no historical balance moved.

## Replay semantics (report reconstruction)

`taxSummaryAt(uint64 seq)` → `{collected, recoverable, netPayable}`:
`collected = −balanceAt(TaxPayable, seq)` (credit-normal output tax),
`recoverable = balanceAt(RecoverableTax, seq)` (debit-normal input tax),
`netPayable = collected − recoverable`. This is `reportAt(seq)`: pass any seq for a historical
report, or a closed period's `closedAtSeqFor` for books-as-closed. Nothing is cached.

## UI workflow

- **Ledger workspace → Tax tab** (`TaxSummaryScreen`): the VAT/GST report (net payable /
  collected / recoverable) + the tax-code registry with a **New tax code** action
  (`TaxCodeEditor`). Tax Payable + Recoverable Tax appear automatically in Accounts / Journal /
  Trial Balance (the B3 explorer, unchanged).
- **Invoice lines** already assign a per-line tax rate → the posting splits it.
- **Expense editor**: a tax-code picker + a net/tax/total summary.

## Known limitations (documented — NOT built)

- **Single Tax Payable / Recoverable Tax account** — per-type (VAT vs GST) **ledger** separation
  and per-rate/period **report breakdowns** are roadmap; v1 gives the aggregate VAT-return numbers.
- Tax on invoice lines is captured as a **rate** (`taxRatePermille`), not a stored code id (the
  record format is immutable), so reports aggregate the posted tax, not code lineage.
- Truncating cent rounding (no bankers'/half-up schemes beyond the existing per-line rounding).
- No multi-jurisdiction, reverse-charge, partial exemption, or tax-filing/e-submission.

## Future roadmap

Per-type tax accounts (VAT/GST/Sales Tax); per-rate + per-period report breakdowns; a filing
export (VAT return); reverse-charge and partial-exemption schemes. All build on the same
posting-as-authority, immutable-event pipeline.

## Verification

`ptest` `testTaxEngine` (taxable/exempt/zero-rated/mixed invoice; expense recovery; tax reversal;
correction/void; historical statements; closed period; replay-identical reports; snapshot; trial
balance 0) + `testGovernanceTransition` (a V1 book adopts V2 on open, historical values unchanged,
idempotent). `itest` drives the tax UI. `fuzz` `fuzzTaxCodes` (corrupt tax log rejected or a
bounded deterministic index) + `fuzzClassify` over `postingPolicy` versions. `ACCT_COMPAT_VERIFY`
holds with `postingPolicy=2`. See `docs/regression-matrix.md`.
