# Design Review: Deterministic Full Domain Event-Sourcing Cutover v1

Status: **implemented + verified** (`tools/ptest.sh` — 186 in-process assertions incl. the
domain-cutover section + supplier/invoice event-commit crash families; `tools/itest.sh` —
39, incl. the live editor→event→ledger→statement path). The event log remains the sole
authority; this phase makes that true for every accounting-relevant entity.

> The core was architecturally complete, but adoption was uneven — the platform mixed two
> persistence models. This phase converges on ONE authoritative lifecycle and eliminates
> the remaining direct-persistence bypass.

---

## 1. Current authority audit (measured, not assumed)

Grep of every write path in `quick/` (the product surface):

| Entity | Live write path | Before | After this phase |
|--------|-----------------|--------|------------------|
| **Customers** | `CustomerEditorViewModel::commit()` | event-authored ✓ | unchanged ✓ |
| **Invoices + lines** | `InvoiceEditorViewModel::commit()` | **direct `save()/.update()/.remove()` ✗** | **event-authored ✓** (`recordInvoiceCreated/Corrected` + ledger post) |
| **Suppliers** | *(none in the product yet)* | repo-backed, could bypass | **event-authored ✓** (`recordSupplierCreated/Updated`, backfill, verify) |
| Products, Categories, Budgets, Transactions, Accounts(hierarchy), Payments(repo) | **none** | repo-backed, dormant | repo-backed — **justified** (see §12) |

The one *live* accounting-relevant bypass was Invoices. It is now closed; Suppliers were
brought onto the model too so no entity *can* persist directly through the projected repos.

## 2. The single authority pipeline

Every mutable accounting entity now follows:
`UI → ViewModel::commit() → authoritative event → EventLog → projection → ledger (if
applicable) → statements → verification`. The invoice editor no longer calls
`invoices().save()`; it calls `StorageService::audit().recordInvoiceCreated/Corrected`.

## 3. Invoice cutover + stable line identity

`InvoiceEditorViewModel::commit()` routes new invoices through `recordInvoiceCreated`
(stable invoice + line ids assigned at authoring) and edits through
`recordInvoiceCorrected` (existing line ids kept, removed lines tombstoned, closed-period
edits rejected). The draft model (`InvoiceDraftLinesModel::Row`) now threads the stable
line id through `setFromInvoiceLines`/`buildLines` (new rows carry `UINT32_MAX`), so a
correction targets identity — not position — and edits no longer churn line ids.

## 4. Invoice → ledger posting (chart-of-accounts bootstrap)

`AuditJournal::ensureChartOfAccounts` opens three role accounts (Accounts Receivable /
Revenue / Cash) as `AccountOpened` events on first run (idempotent) and binds them via
`setPostingAccounts` on every open. On commit, the editor posts the **recognised-revenue
delta**: `recognized(status,total) = isRecognized(status) ? total : 0` (recognised =
POSTED/OVERDUE/PAID); create posts the full amount, edit posts `new − old`, void/draft
post the reversal — one rule, balanced Dr AR / Cr Revenue, closed-period-checked. Proven by
`itest`: a posted invoice yields exactly one ledger entry, trial balance stays 0, and the
income statement recognises the revenue.

## 5. Supplier event authoring

`SupplierCreated=16`/`SupplierUpdated=17` (append-only). `AuditJournal` gained a
`SupplierRepository*`, `recordSupplierCreated/Updated`, an `apply()` supplier route, and
projection/reconcile/rebuild threading. `SupplierRepository` gained the projection surface
(`count`/`contentHash`/`clear`/`upsertAt`) it was missing. No supplier UI exists yet, so
this is a storage-authority cutover — the record path is proven by the harness.

## 6. Backfill adoption (commit-cutover for existing data)

`backfillSuppliers`/`backfillInvoices` adopt entities written by an old direct path into
history — gated per-entity ("no `<Entity>Created` event yet"), so they run on an
already-non-empty log (unlike `backfillCustomers`, which is whole-log-empty gated). When
anything is adopted, `StorageService::initialize` runs `rebuildProjections()` once to
canonicalise the live projection to a pure replay (collapsing stale line-id gaps left by
old in-place edits), so `live == history` exactly afterward.

## 7. Verification extended to the whole model

`AuditJournal::verifyAll` reconstructs customers + suppliers + invoices + lines from history
into disposable scratch repos and compares each fingerprint to live. `validateCompatibility`
now uses it, so the compatibility gate (`ACCT_COMPAT_VERIFY`, the report) proves
replay-equivalence for the FULL accounting model. Proven: full-model `live == history` after
cutover, and a tampered invoice projection is caught as drift.

## 8. Replay / historical reconstruction interaction

Migrated entities replay through the same `apply()` as the live path, so historical
reconstruction (`reconstructInto`, "books as of seq N") reproduces identical state.
Invoice/supplier events are seq-ordered facts; ids come from order → byte-identical rebuilds.

## 9. Ledger / statement interaction

Invoice commits generate real `JournalEntryPosted` events, so the ledger and statements are
derived from authoritative postings (not recomputed from mutable config). Trial balance is
structurally 0; income statement and balance sheet reconstruct at any seq.

## 10. Compatibility-governance / snapshot interaction

Governance and snapshotting operate on the log/ledger, so they extend to invoices/suppliers
for free: the compat manifest, downgrade refusal, and ledger snapshot all apply uniformly;
`validateCompatibility` now covers the whole model.

## 11. Crash safety

The new record paths reuse the proven `append → ajMaybeCrash → project → writeCursor`
discipline. New cross-process crash families (`sup-crash`, `inv-crash` at
`afterEventBeforeProject` / `afterProjectBeforeCursor`) prove a kill mid-commit reconciles
to exactly the log — the supplier is present-or-absent, the invoice + line are fully present
(total == Σ lines) or fully absent. No orphan projection, missing/duplicated event, or
partial ledger update.

## 12. Domains intentionally left repo-backed (justified)

Products, Categories, Budgets, Transactions, and the Account hierarchy have **no live write
path** in the product — nothing commits them. Event-authoring them now would be speculative
infrastructure for entities with no UI (against "consolidation, not expansion"). They remain
repo-backed with the Customer/Invoice/Supplier event pattern as the documented template for
when they gain an editor. The `PaymentRepository` (`payments.dat`) is distinct from the
event-sourced settlement `payments_`; it too has no live Quick write path.

## Required invariants (satisfied)

Single authority (the event log; repos are disposable projections) · deterministic replay
(byte-identical rebuilds) · cross-domain consistency (no migrated domain bypasses
events/projections/replay/compat) · projection disposability (every migrated projection
clears + rebuilds from history) · historical interpretability (every change is an event with
what/when/effect) · compatibility preservation (governance/snapshot/statement/posting intact)
· crash safety (proven per-domain).

## Non-goals

No new accounting features, tax, multicurrency, cloud sync, plugins, ERP workflow, or a
configurable posting DSL. Consolidation, not expansion.

## Honest limitations

- **Supplier** cutover is storage-authority only (no supplier editor yet); proven by the
  harness, not a live UI.
- **Ledger for backfilled invoices**: adoption authors the invoice ENTITY into history; it
  does not retroactively fabricate revenue postings for pre-cutover invoices (the ledger
  accrues from cutover forward). New invoices post normally.
- **Invoice entity + its ledger posting are two events**: a crash strictly between them
  leaves the invoice recorded but unposted (reconcile can't synthesise the posting) — a
  re-post fixes it. Closed-period / role-config edge cases make the posting best-effort so
  they never undo the authoritative entity commit.
- **Chart of accounts** is a fixed 3-role bootstrap by convention; a user-editable chart is
  out of scope.

## Critical design questions — answered

1. *Paths still bypassing the log?* Invoices (now cut over); the dormant repos (justified,
   §12). 2. *Already-conforming?* Customers. 3. *ViewModels needing cutover?*
   `InvoiceEditorViewModel`. 4. *Projections to rebuild?* invoices/lines/suppliers — all
   disposable + verified. 5. *Ledger-affecting domains?* invoices (payments = documented
   next). 6. *Operational-only?* products/categories/budgets/etc. (§12). 7. *Missing replay
   tests?* supplier/invoice event-authoring + `verifyAll` — added. 8. *Uncovered crash
   windows?* supplier + invoice commit — added. 9. *Compat extension?* `verifyAll` (full
   model). 10. *Snapshots?* ledger snapshot already covers invoice-derived postings. 11.
   *Diagnostics?* `acct.compat` + report now cover the full model. 12. *Obsolete APIs?* the
   invoice editor's direct `save()/.update()/.remove()` path is eliminated.

## Integration status & next increment

Live: the invoice editor is fully event-authored and ledger-posting; suppliers are
event-authored at the storage layer; `initialize` bootstraps the chart, backfills + adopts
legacy entities, and validates the full model. Next: route the payment/settlement UI (when
added) through the same pipeline; a supplier editor on the proven record path; retroactive
ledger reconstruction for backfilled invoices; and event-authoring the remaining repos when
they gain a write path.
