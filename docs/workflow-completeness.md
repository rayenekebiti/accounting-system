# Operational Workflow Completeness

Where the application stands as a *usable accounting app*, audited from an accountant's
perspective. This phase (**B1 v1**) completed the **Suppliers** vertical end-to-end and
established this gap analysis + the repeatable completion template. No accounting semantics,
storage architecture, or event model changed — Suppliers reuse the **existing** event authority.

## Per-entity audit

The product is the **Quick app** (`AccountingQuick`). "Event-authored" = every write flows
through `AuditJournal::record*` (no repository is a direct write authority). ✅ = present,
❌ = absent, — = not an entity here.

| Entity | Create | Edit | Search/Filter | Archive/Delete | Export | Event-authored | UI screen |
|--------|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| **Customers** | ✅ | ✅ | ✅ | ❌ | ❌ | ✅ `recordCustomer*` | ✅ |
| **Invoices + lines** | ✅ | ✅ (correction) | ✅ | void/reverse (storage) | ❌ | ✅ atomic + ledger post | ✅ |
| **Suppliers** | ✅ | ✅ | ✅ | ❌ | ❌ | ✅ `recordSupplier*` | ✅ |
| **Payments / settlement** *(this phase, B2)* | ✅ record | allocate / reverse | ✅ | reverse (append-only) | ❌ | ✅ `recordPayment`/`allocatePayment`/`reverseAllocation` | ✅ |
| **Accounts / ledger** *(B3: read-only explorer)* | bootstrap only | — (read-only) | ✅ search/filter | — | ❌ | ✅ `AccountOpened`/`JournalEntryPosted` | ✅ inspect |
| **Expenses** *(B4: full lifecycle + posting)* | ✅ | ✅ (correction) | ✅ search/filter | void/reverse | ❌ | ✅ `ExpenseCreated/Corrected/Voided/Reversed` + atomic ledger post | ✅ |
| **Tax (VAT/GST)** *(B5: engine + workflow)* | ✅ codes | — (append-only versions) | — | reversal/void reverse tax | ❌ | ✅ `TaxCodeCreated` + posting-policy v2 tax split (Tax Payable / Recoverable Tax) | ✅ Tax tab |
| Products | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ (repo-only) | ❌ |
| Categories | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ (repo-only) | ❌ |
| Budgets | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ (repo-only) | ❌ |
| Transactions (income/expense) | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ (repo-only) | ❌ |
| Quotes | — | — | — | — | — | — | ❌ (not implemented) |
| Purchase invoices | — | — | — | — | — | — | ❌ (not implemented) |
| Tax configuration | — | — | — | — | — | — | ❌ (not implemented) |
| Company settings *(C1: display prefs)* | ✅ | ✅ | — | — | — | n/a (QSettings, not the store) | ✅ Settings › Company |
| Opening balances | — | — | — | — | — | — | ❌ (not implemented) |

## Implemented workflows (fully usable + event-authored)

- **Customers** — list + summary (owing / outstanding / at-risk) + search/filter; create/edit
  via a modal editor with live validation, keyboard flow, dirty-guard discard; every write →
  `audit().recordCustomerCreated/Updated`.
- **Invoices** — list + summary + status filters + search; create/edit (correction) with
  stable line identity; the commit is **one atomic authoritative fact** (invoice + ledger
  revenue posting) via `recordInvoiceWithRevenue`. Feeds ledger + statements.
- **Suppliers** *(this phase)* — `SupplierListModel` / `SupplierFilterProxy` /
  `SuppliersViewModel` / `SupplierEditorViewModel` + `SuppliersScreen.qml` / `SupplierEditor.qml`,
  wired into the nav rail and `Main.qml`. Payables list + summary (payable / owing) +
  search/filter; create/edit with the same validation/keyboard/RTL behavior; every write →
  `audit().recordSupplierCreated/Updated` (proven by the interaction suite: a commit appends
  exactly one authoritative event — **no repository bypass**). Storage/replay/verify/compat
  were already in place from the Full Domain Cutover phase; this phase added the missing UI.

## Settings & System workspace *(C1 — production readiness)*

C1 added the first **non-accounting** commercial surfaces — a "Settings & System" area under one
nav item (⚙ Settings), built on the same tabbed-workspace pattern as the Ledger explorer. Every
value here is either a machine-global preference (`QSettings`, via `SettingsViewModel`) or a
**read-only** view of the engine (`DiagnosticsViewModel` / `BackupViewModel`). **Nothing on these
screens mutates the event store, ledger, tax, or posting** — they read the same authoritative
accessors the startup log uses.

- **General** — language (reuses `LocaleController`/`i18n`), date-format (live preview), currency
  symbol. Persisted to `QSettings`; amounts are always stored exactly (int64 cents) regardless.
- **Company** — business name / address / tax number / email (`QSettings`; for later document
  rendering). Auto-saved on field-exit.
- **Backup & Restore** — Manual **Back Up Now** (a full copy of the data folder into
  `backups/<timestamp>/`), **backup history** (date + estimated size — never internal file names),
  **Verify** (opens the backup's authoritative log read-only → CRC + gap-free-seq check), and
  **Restore** (confirm → stage into `.pending-restore/` → applied on next startup, before the store
  opens, so it is safe against the live lock). Busy state + actionable status on every action.
- **Diagnostics** — a read-only health page: engine / version-contract / posting-policy / snapshot
  status, database size, event count, current seq, ledger account+entry counts, trial-balance
  status, last backup, plus an on-demand **Run verification** (`verifyAuditProjection()` +
  `validateCompatibility()` — the same non-destructive replay-equivalence checks as the CI gate).
- **About** — product identity + honestly-labelled **Licensing** / **Updates** "Coming soon"
  areas (informational only — no fake controls).

**Crash-recovery UX** — on startup, if the engine recovered a crash-leftover journal (`auditReconciled>0`)
or discarded an uncommitted tail (`auditTornTail`), `SettingsViewModel::captureStartupRecovery()`
re-verifies the projection against history. If it matches, a one-time reassurance dialog
(`RecoveryDialog`) confirms the outcome; if it does **not**, a **blocking** `RecoveryBlocker` (no
"continue") tells the operator to quit and restore a backup. The app never continues silently on
suspect data.

## Architectural review — the completion pattern

Every completed entity follows the same vertical, and it is the template for the rest:

1. **`<Entity>ListModel`** — `QAbstractListModel`; `refresh()` reads the repository projection
   (`storage.<entity>().loadAll()`); exposes display roles.
2. **`<Entity>FilterProxy`** — `QSortFilterProxyModel`; search + category filtering.
3. **`<Entity>sViewModel`** — owns the proxy; summary counts + `setSearchText`/`refresh`.
4. **`<Entity>EditorViewModel`** — buffer props + live validation (stable error KEYS →
   `qsTr` in QML) + `beginNew`/`beginEdit`/`commit`/`discard`. **`commit()` routes through
   `audit().record<Entity>*` — never `<entity>().save()`.**
5. **`<Entity>sScreen.qml` + `<Entity>Editor.qml`** — mirror the list/summary/filter + modal
   editor; all strings `qsTr`, reusing `Theme` → RTL/keyboard/i18n for free.
6. **Wiring** — `NavRail` model + `Main.qml` `StackLayout` index + `Connections{onSaved:…}`;
   `main_quick.cpp` constructs the models + context properties; `CMakeLists` sources + QML.
7. **Verification** — an `itest` interaction test drives the real editor (validation,
   persistence, event-authoring, zero QML errors); storage is already covered by
   `ptest`/`fuzz`/`verifyAll`/compat.

**The invariant that holds across all completed entities:** no repository is a direct write
authority. The list models *read* projections; every *write* is an authoritative event.
`verifyAll` (customers + suppliers + invoices + lines) and `ACCT_COMPAT_VERIFY` prove the
projections equal a replay of that history.

## Remaining workflows (the roadmap)

Each is a repeat of the vertical above. Ordered by readiness (how much storage exists):

| Workflow | Storage status | Work to complete |
|----------|----------------|------------------|
| ~~**Payments / settlement UI**~~ *(done — B2)* | ✅ shipped: Payments screen + record editor + allocation editor, all `audit()`-authored; invoice/customer settlement surfacing. See `docs/payments-workflow.md`. | — |
| **Accounts / chart-of-accounts UI** | ✅ `AccountOpened` + bootstrap | UI only: list the role/chart accounts; add-account editor → `recordAccount`. |
| **Suppliers → payables ledger** | events exist; not posted | Post supplier bills to the ledger (mirror invoice→revenue posting) — an accounting extension, not just UI. |
| **Products** | ❌ repo-only | Add `ProductCreated/Updated` events + projection routing + backfill + `verifyAll` coverage (as Suppliers were added at storage level), then the UI vertical. |
| ~~**Expenses**~~ *(done — B4)* | ✅ shipped: `ExpenseCreated/Corrected/Voided/Reversed` + atomic Dr Expenses / Cr Cash\|AP posting; ExpensesScreen + editor. See `docs/expenses-workflow.md`. | — |
| **Categories / Budgets** | ❌ repo-only | Same as Products: event authority first, then the UI vertical. |
| **Quotes / Purchase invoices** | not implemented | New entity + events + projection + UI (quotes convert to invoices). |
| **Tax configuration / Company settings / Opening balances** | not implemented | Governance-adjacent config (opening balances are `JournalEntryPosted` seeds; tax is a posting-policy input). Design + UI. |

## Known limitations

- **One vertical this phase.** ~10 workflows remain (table above). The success criterion
  "functionally complete for an SMB" is met *incrementally* by repeating the proven vertical
  per entity — not by this phase alone.
- **Archive / delete / export are unimplemented across the Quick UI** (consistent for
  Customers, Invoices, Suppliers). Repositories carry a soft-delete flag and invoices support
  void/reversal in storage, but there is no UI or archival event (e.g. `SupplierArchived`) yet.
- **No export** in the Quick app (the Widgets app's `Exporter`/`InvoicePrinter` are not wired
  into the product).
- **Suppliers do not post to the ledger** — a supplier is a payable record, but no
  bill→payables journal entry is generated (invoice→revenue is, for sales). Documented above.
- New QML strings are `qsTr`-wrapped but not yet in the `.ts` catalogs — they fall back to the
  source language until `pwsh tools/i18n-extract.ps1` refreshes the catalogs. Not a blocker.

## Future enterprise extensions

Multi-currency; multi-entity/company books; role-based access + multi-user (needs the audit
log's single-writer assumption revisited); bank feeds / reconciliation import; recurring
billing; tax engines (VAT/GST regimes); PDF/CSV export + e-invoicing; a reporting/BI surface
over the deterministic statements. All are out of scope for this phase (and the current
non-goals) and build on — not around — the authoritative event pipeline.

## Verification

- `bash tools/itest.sh` — the full interaction suite (invoice / customer / supplier / payments /
  ledger / expenses / **settings-system** / language / line-items): **116 assertions, 0 failed**.
  The C1 `settings-system` block proves the Diagnostics/Settings/Backup screens are **read-only**
  over the store (reading them does not advance the sequence or append events) and that every
  Settings tab renders with zero QML errors.
- Launch → the nav rail shows **Invoices · Customers · Suppliers · Payments · Expenses · Ledger ·
  Settings**; every screen + editor renders (EN + AR-RTL). Settings baselines `17–21_settings_*`.
- `bash tools/ptest.sh` + `bash tools/fuzz.sh` (ROBUST) + `ACCT_COMPAT_VERIFY` (replay-equivalence
  held) — unchanged by C1; the Settings surfaces read the already-verified event path (no new
  storage risk). See `docs/production-readiness.md` for the full readiness review.
