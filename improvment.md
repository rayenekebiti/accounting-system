# AccountingPro — Infrastructure Improvement Plan & Target Architecture

Audit date: 2026-06-10. Snapshot: branch `main` @ `fa5845b` ("Fix blocking and major issues").
Scope: backend (domain + service), UI (Qt6 widgets), storage (persistence). Goal: take this from a portfolio-grade desktop app to a **defensible small-business v1** without throwing the existing structure away.

---

## 1. Where the project actually stands today

Confirmed by reading the tree (not by trusting older docs):

| Layer | State |
|---|---|
| Domain entities (`core/*`, `transaction/*`, root-level `Account*`) | Built; fixed-size binary records; `isValid()` + serialize/deserialize; **duplicate root-level files** still present (`Account.cpp`, `BankAccount.*`, `Budget.cpp`, `CashAccount.*`, `CheckingAccount.*`, `SavingAccount.*`, `SavingsAccount.h` header-only). |
| Storage L1 — `BinaryRecordFile` | Built, hardened (length check, flush, gcount, buffer zeroing). Still: no fsync, no atomic write, no header/version, no compaction. |
| Storage L2 — Repositories | **All 9 exist** (`Customer/Supplier/Product/Invoice/Payment/Transaction/Account/Category/Budget`). Header-only, hand-rolled offset constants. |
| Storage L3 — `StorageService` | Built; singleton; `QLockFile` acquired on init; init error surfaced in `main.cpp` → `QMessageBox::critical` → exit. |
| Storage L4 — Qt models | 5 of 9 (`Customer/Supplier/Product/Invoice/Payment`). Transactions/Accounts/Budgets/Categories have no model adapter. |
| UI shell | Full: `MainWindow`, `NavigationPanel`, `GlobalToolBar`, `PageHeader`, `PageRouter`, `ThemeManager`. |
| Pages | All 8 pages compile. Dashboard uses curated `QStandardItemModel`. Reports has 10 runners but is sync. Settings persists via `QSettings`. |
| Dialogs | All 5 entity editors exist. |
| Build | `CMakeLists.txt` is C++20 + Qt6 + `windeployqt` post-build (fixes the old C++26/no-deploy issues). No installer. No tests. No CI. |

**What was actually fixed in `fa5845b`:** error handling around storage calls, date parsing direction, missing stubs (likely Account/Reports), `QLockFile`-based concurrency.

**What's still missing or fragile** (the real list to plan around):

1. Invoices are header-only — no line items, no product link, totals are user-typed.
2. No write durability beyond `flush()` (no `fsync`/`FlushFileBuffers`, no atomic-rename, no journal).
3. No schema version / magic bytes in `.dat` files.
4. Dates stored as localized `"d MMM yyyy"` text — French/English machines round-trip as invalid.
5. No audit trail.
6. No backup / restore / data export.
7. UI thread does all I/O; Reports re-`loadAll()` 2–3 times per click.
8. Customer balance is editable and drifts from invoice/payment totals.
9. Export and Print buttons are decorative.
10. 65,535-record ceiling per entity, no compaction.
11. No encryption-at-rest, no user accounts.
12. Duplicate root-level entity files alongside `core/` versions.
13. Settings numbering panel is decorative — invoice/payment editors ignore it.
14. No tests, no CI.

---

## 2. Guiding principles for the improvement

- **Preserve the 4-layer split.** It's already correct: `BinaryRecordFile` → Repository → `StorageService` → Qt model. Improvements slot inside each layer, not across them.
- **Two horizons.** Track A keeps the binary format (portfolio honesty, low churn). Track B introduces SQLite as a parallel backend (real-world durability). Run both behind the same repository interface; switch via build flag.
- **No mock data left in production code paths.** Every page should read from `StorageService` only.
- **Errors propagate as typed exceptions, UI converts to dialogs at one boundary** (a `UiErrorBus` or page base-class helper). No `catch` blocks scattered through every slot.
- **Money is a value type, not a `double`.** A new `Money` (int64 cents) class removes float drift in totals and balances.
- **Dates are ISO `YYYY-MM-DD` on disk, `QDate` in memory.** Period.
- **Single writer thread per file.** UI thread enqueues work; a per-repository worker drains it. Lets the UI stay responsive without making `BinaryRecordFile` thread-safe internally.

---

## 3. Target architecture (after the improvements land)

```
┌──────────────────────────────────────────────────────────────────────┐
│  Qt Widgets UI                                                        │
│  Pages → Dialogs → reusable components/                               │
│       │                                                                │
│       │ commands & queries (UiCommandBus)                              │
│       │ errors → UiErrorBus → ConfirmDialog                            │
│       │                                                                │
├───────▼──────────────────────────────────────────────────────────────┤
│  Layer 4 — Qt Model Adapters (QAbstractTableModel + ProxyModels)     │
│  Per entity, owns a snapshot vector + invalidates on repo signals.   │
├───────┬──────────────────────────────────────────────────────────────┤
│       │                                                                │
├───────▼──────────────────────────────────────────────────────────────┤
│  Layer 3 — StorageService (facade)                                    │
│  Owns all repos, lockfile, backup scheduler, audit log writer,        │
│  data-folder paths, schema-version negotiation.                       │
├───────┬──────────────────────────────────────────────────────────────┤
│       │                                                                │
├───────▼──────────────────────────────────────────────────────────────┤
│  Layer 2 — Repository Interface  (IRepository<T>, IInvoiceRepo, …)    │
│  Two implementations:                                                 │
│   • BinaryRepository<T>  ──► BinaryRecordFile (Track A, default)      │
│   • SqliteRepository<T>  ──► single accountingpro.sqlite (Track B)    │
│  Adds: in-memory cache w/ invalidation, paginated queries, audit hook.│
├───────┬──────────────────────────────────────────────────────────────┤
│       │                                                                │
├───────▼──────────────────────────────────────────────────────────────┤
│  Layer 1 — Storage primitives                                         │
│  Track A: BinaryRecordFile v2 — file header (magic+version+recsize),  │
│           atomic update via shadow record + crash-safe journal,       │
│           FlushFileBuffers/fsync, 32-bit IDs, compact() pass.         │
│  Track B: sqlite3 amalgamation drop-in (single header/source).        │
└──────────────────────────────────────────────────────────────────────┘

Cross-cutting:
  • Money (int64 cents) replaces `double` in all financial fields.
  • IsoDate (YYYY-MM-DD) replaces "d MMM yyyy" strings on disk.
  • AuditLog (append-only, hash-chained) records every write.
  • BackupService (zip data folder on startup + N-daily).
  • A single I/O worker thread per repository; UI never blocks > 16 ms.
```

---

## 4. Storage improvements (the load-bearing layer)

### 4.1 `BinaryRecordFile` v2 (Track A — keep the binary format)

**Layout change:** every `.dat` file gets a 32-byte header.

```
offset  size  field
   0     8    magic        "ACCTPRO\0"
   8     2    fileVersion  uint16  (currently 1)
  10     2    recordSize   uint16  (validated against the constant)
  12     4    recordCount  uint32  (live count; survives crash via journal replay)
  16     8    lastWriteId  uint64  (monotonic; helps audit)
  24     8    reserved     0
```

- **Reject mismatches loudly** on `open()` — refuse to operate on a file whose `recordSize` doesn't equal the build-time constant. Closes the "padding bytes drift" trap.
- **IDs widen to `uint32_t`** — file holds up to ~4 B records, formats stays compact. Existing 16-bit code paths need a typedef bump (`RecordId`).
- **Atomic update.** Replace in-place `seekp+write+flush` with:
  1. write new record bytes to a shadow file `path.journal`,
  2. `FlushFileBuffers` (Win32) or `fsync` (POSIX),
  3. write a 16-byte journal entry `{recordId, crc32}`,
  4. apply the journal entry to the main file,
  5. truncate the journal.
   On next `open()`, replay any non-empty journal. This is the cheapest path to crash safety without SQLite.
- **`compact(progressCallback)`** that rewrites the file dropping `isDeleted=true` records into a temp file and atomic-renames. Re-issues IDs **only if** the entity opts in (transactions/categories yes; invoices/customers no — because they're referenced by FK).
- **`fsync` policy:** flush+fsync on `append`/`update`/`compact`. Reads stay unbuffered-fast.

### 4.2 Repository interface and two backends

Define a thin abstract base so backends are swappable:

```cpp
template <class Entity>
class IRepository {
public:
    virtual ~IRepository() = default;
    virtual RecordId          save     (Entity& e)                   = 0;
    virtual bool              update   (const Entity& e)             = 0;
    virtual bool              remove   (RecordId id)                 = 0;
    virtual std::optional<Entity> find (RecordId id)                 = 0;
    virtual std::vector<Entity>   loadAll()                          = 0;
    virtual std::vector<Entity>   loadRange(size_t offset, size_t n) = 0;
    Signal<RecordId, ChangeKind>  changed;   // Qt signal in the QObject wrapper
};
```

Implementations:
- `BinaryRepository<Entity>` — current code, factored to avoid copy-pasting offset constants across files.
- `SqliteRepository<Entity>` — schema generated from the same entity metadata, prepared statements cached.

**Pin offsets in one place.** Every entity declares a `Layout` struct with `static constexpr` field offsets and a `static_assert(sizeof_fields == RECORD_SIZE)`. Repositories `#include` the entity header and reference `Entity::Layout::kDeletedOffset` — no more `CUST_DELETED_OFFSET = 122` magic numbers in the repo.

### 4.3 Schema discipline & migrations

- `core/Layout.h` per entity (offsets + `static_assert`s).
- `migrations/` folder: a sequence of `Migration_001_AddCustomerNotes.{h,cpp}` style scripts that read records with `fileVersion = N`, rewrite as `N+1`. Run automatically on startup with a "Backing up before migration…" dialog.
- Bump the header `fileVersion` once on each release where any record layout changes.

### 4.4 Cache + signals

- Each repository keeps a `std::vector<Entity>` cache populated on first `loadAll()` and mutated by `save`/`update`/`remove`. Eliminates the 2–3× full scans per Reports click.
- On any mutation, emit `changed(id, kind)`. The Qt table model listens and rebroadcasts `dataChanged()`/`rowsInserted()`/`rowsRemoved()`. Pages stop calling `reload()` manually.

### 4.5 Audit log

- New `storage/AuditLog.{h,cpp}` — single append-only file, fixed 128-byte records: `{ts, userId, entityType, entityId, op (CREATE/UPDATE/DELETE), beforeCrc, afterCrc, prevHash}`.
- Hash-chain (`prevHash = sha256(prevHash || thisRecord)`) makes tampering detectable.
- Repositories call `AuditLog::record(...)` inside every write, on the same I/O worker thread, before returning success to the UI.
- Settings → Advanced → "Export audit log" produces a CSV the tax inspector can read.

### 4.6 Backup & restore

- `storage/BackupService.{h,cpp}` — on `QApplication::aboutToQuit` and on a daily timer, zip the data folder to `%APPDATA%/AccountingPro/backups/YYYY-MM-DD_HHMM.zip`. Keep the last N (configurable, default 14).
- Settings → "Backup now" and "Restore from backup…" menu items. Restore is gated by a typed-confirmation dialog (`"type RESTORE to continue"`).

### 4.7 SQLite track (Track B, opt-in)

- Add `sqlite3.{c,h}` to the source tree (public domain, single file).
- `CMakeLists.txt` option: `option(ACCT_USE_SQLITE "Use SQLite backend" OFF)`. When ON, `StorageService` constructs `SqliteRepository<*>` instead of `BinaryRepository<*>`.
- Schema is just the entities mapped to tables with FKs, plus the `audit_log` table.
- Same `IRepository<Entity>` interface — no UI code changes.

Recommended default for v1: **Track A** (keep the binary format honest with the assignment, but harden it with the items above). Recommended for "AccountingPro v2": **Track B**.

### 4.8 Threading

- Each repository owns one `QThread` running an event loop.
- Mutating calls are dispatched via `QMetaObject::invokeMethod(repo, ..., Qt::QueuedConnection)` and return a `QFuture<RecordId>`.
- Reads from the in-memory cache stay synchronous on the UI thread (the cache is the source of truth post-startup).
- Initial `loadAll()` runs on the worker; pages show `BusyOverlay` until the first `cacheReady` signal.

---

## 5. Backend / domain improvements

### 5.1 Money & IsoDate

- `core/Money.{h,cpp}` — `int64_t cents`, `Currency currency`, arithmetic ops, `fromDouble()`, `toDouble()` only at UI boundaries, `format(locale)`. Replaces every `double subtotal/total/balance/amount` field across `Invoice`, `Payment`, `Customer`, `Supplier`, `Product`, `Account`, `Transaction`.
- `core/IsoDate.{h,cpp}` — wrapper over 10-byte `YYYY-MM-DD`. `toQDate()` / `fromQDate()` at the UI boundary. Replaces every char[12] `issueDate`/`dueDate`/`paymentDate` field.
- **One-shot migration script** to rewrite existing `.dat` files from `"5 Jun 2026"` and `double` cents-as-dollars to the new formats.

### 5.2 Invoice line items (the biggest functional gap)

New entity `InvoiceLine` + new repo:

```
InvoiceLine
  id            uint32
  invoiceId     uint32
  productId     uint32  (0 = ad-hoc)
  description   char[64]
  quantity      int32   (in 1/1000 units — supports 0.001 quantity)
  unitPrice     Money
  taxRatePermille int16 (e.g. 190 = 19.0%)
  lineTotal     Money   (derived; stored for audit)
```

- `core/Invoice` derives `subtotal/taxAmount/total` from its lines — the dialog stops being free text.
- `InvoiceEditorDialog` gets a `QTableView` for lines + Add/Remove buttons + product picker.
- Reports' "P&L cost-of-goods-sold" becomes computable.

### 5.3 Customer/supplier balance derivation

- Remove the editable `balance` field on the form. Internally keep a `startingBalance` (immutable after first save) and compute `currentBalance = startingBalance + Σ posted-invoice totals − Σ payments`.
- A new read-only `Customer::computeBalance(InvoiceRepository&, PaymentRepository&)` helper. Repository caches this per id, invalidates on related writes.

### 5.4 Numbering counters actually applied

- New `core/NumberingService.{h,cpp}` reads/writes the `QSettings` keys the Settings page already manages (`numbering/invoicePrefix`, `numbering/nextInvoiceNumber`, same for payments).
- `InvoiceEditorDialog::suggestNextNumber()` calls `NumberingService::reserveNextInvoiceNumber()` instead of scanning the model. O(1), survives across runs.

### 5.5 Pre-existing bugs to retire

- Delete duplicate root-level entity files (`Account.cpp`, `BankAccount.*`, `Budget.cpp`, `CashAccount.*`, `CheckingAccount.*`, `SavingAccount.*`, `SavingsAccount.h`). Canonical home is `core/`.
- Fix `CheckingAccount::canWithdraw()` (inverted comparison).
- Implement `SavingsAccount` source (header-only today).
- Fix `CategoryReport` namespace-scope static vector.

---

## 6. UI improvements

### 6.1 Unified error / busy boundary

- `src/ui/common/UiErrorBus.{h,cpp}` — a `QObject` singleton with `error(QString title, QString message, ErrorSeverity)`. Pages call `try { … } catch (const std::exception& e) { UiErrorBus::instance().error(...); }` exactly once at the slot boundary.
- `Page` base class gets a `runStorage<F>(F&& fn)` helper that wraps the try/catch + shows `BusyOverlay` for calls expected to take > 100 ms. Removes the "no `catch` blocks in pages" gap.

### 6.2 Replace `QStandardItemModel` everywhere

- Build the four missing model adapters: `TransactionTableModel`, `AccountTableModel`, `CategoryTableModel`, `BudgetTableModel`.
- Dashboard's curated `QStandardItemModel`s become projections of the real Invoice/Payment models filtered by recency/overdue. Fixes the leak at `DashboardPage.cpp:293,327`.
- Reports' per-click `delete m_reportModel; new QStandardItemModel(...)` becomes `m_reportModel->clear()` + `appendRows()`.

### 6.3 Filter proxies, typed

- Replace each page's `QSortFilterProxyModel*` field with the concrete derived type (`CustomerFilterProxy*`, `InvoiceFilterProxy*`, …). Removes the `static_cast` traps noted in HANDOFF.md.
- Filter on enum values via custom user roles (e.g. `InvoiceTableModel::StatusRole`), not on displayed text — labels can change without breaking filters.

### 6.4 Pagination — wire it or remove it

- New `PaginationProxy : QSortFilterProxyModel` that limits visible rows by `(pageIndex, pageSize)`. Stack: `RowSource → FilterProxy → PaginationProxy → DataTableView`. `PaginationFooter` listens to the proxy.
- "Total records" label switches between "of N filtered" and "of M total" via a toggle.

### 6.5 Export / Print, properly

- New `src/ui/services/Exporter.{h,cpp}` — `Exporter::toCsv(QAbstractItemModel*, QIODevice*)` and `Exporter::toXlsx(...)` (XLSX is a stretch; CSV ships first).
- Every `ListPage` gets a free Export action wired to its current model + visible columns.
- New `src/ui/services/InvoicePrinter.{h,cpp}` — builds a `QTextDocument` from an Invoice (header + lines + totals + company info from `QSettings`), feeds `QPrinter` or `QPrinter::PdfFormat`. Print and "Export → PDF" share the renderer.

### 6.6 Reports run off the UI thread

- `ReportsPage::onRunClicked` enqueues a `ReportTask` on a `QThreadPool`. `BusyOverlay` is shown for real (no synchronous compute on the UI thread anymore).
- Each report runner takes `const std::vector<…>&` snapshots pulled from the repository cache, so no I/O happens inside the runner.

### 6.7 Theme & accessibility polish

- Show the sort indicator on `DataTableView` (`setSortIndicatorShown(true)`).
- Audit colours for WCAG AA contrast in both themes; add a "High contrast" theme variant.
- Add keyboard shortcuts: Ctrl+N (new in current page), Ctrl+F (search), Ctrl+E (export), Ctrl+P (print), F5 (refresh).

### 6.8 Settings rationalisation

- Reset-to-defaults button.
- Numbering panel feeds `NumberingService` (see §5.4).
- "Backup now / Restore" actions in Advanced.
- Data folder picker (currently hardcoded to `QStandardPaths::AppDataLocation`).

---

## 7. Cross-cutting infrastructure

### 7.1 Testing

- Add `tests/` with [doctest](https://github.com/doctest/doctest) (single header).
- Unit tests:
  - `BinaryRecordFile` — round-trip, crash injection (kill mid-write via a fault-injection hook, verify replay), header validation, compaction.
  - Each repository — CRUD + cache invalidation + audit-log emission.
  - `Money` arithmetic, `IsoDate` parse/format round-trip.
- Integration tests:
  - Open service, write 1k invoices with lines + payments, run every report, assert totals.
- Qt UI smoke tests via `QTest::keyClicks` for one happy path per page.

### 7.2 CI

- GitHub Actions workflow: matrix `{windows-2022 + MSYS2 UCRT64, ubuntu-22.04 + Qt6, macos-14 + Qt6}`. Steps: configure, build, run tests, run `windeployqt` on Windows, upload artefact.
- Cache the Qt install (aqtinstall).
- Block merges on red.

### 7.3 Packaging

- Inno Setup script `installer/AccountingPro.iss` — installs the `windeployqt`-produced folder, creates Start Menu + Desktop shortcuts, registers an uninstaller that scrubs `HKCU\Software\AccountingPro`.
- Code signing (Authenticode) is a separate cost item — add the build hook now, leave the cert empty until purchased.

### 7.4 Telemetry & logging

- `core/Logger.{h,cpp}` — file sink at `%APPDATA%/AccountingPro/logs/app.log`, rotated daily, capped at 14 days. Use `qInstallMessageHandler` to capture Qt warnings.
- No remote telemetry without an explicit opt-in checkbox in Settings.

### 7.5 Build hygiene

- `.gitignore` `build/`, `*.dat`, `brf_test.dat`, `.cache/`, `*.pptx` (presentation belongs in a release asset, not the repo).
- `.clang-format` + `.clang-tidy` + a `format` CMake target.
- Delete `HANDOFF.md`, `REVIEW.md`, `finalissues.md`, `BinaryRecordFile_REVIEW.md` after porting their open items here, or move them to `docs/history/`.

---

## 8. File-system layout after the changes

```
accounting-system/
├── CMakeLists.txt              # adds tests, SQLite option, Inno Setup hook
├── installer/AccountingPro.iss
├── docs/
│   ├── ARCHITECTURE.md         # this doc, kept in sync
│   └── history/                # old REVIEW/HANDOFF/finalissues
├── core/
│   ├── Money.{h,cpp}             ← new
│   ├── IsoDate.{h,cpp}           ← new
│   ├── NumberingService.{h,cpp}  ← new
│   ├── Logger.{h,cpp}            ← new
│   ├── Customer.{h,cpp} + Layout.h (offsets + static_asserts)
│   ├── Supplier / Product / Invoice / InvoiceLine (new) / Payment …
│   ├── Account.{h,cpp} (moved from root)  +  BankAccount / CashAccount /
│   │   CheckingAccount / SavingsAccount (all moved into core/, duplicates deleted)
│   ├── Category / Budget
│   └── AccountingSystem.{h,cpp}
├── transaction/                  (unchanged; serializes through core/Money + IsoDate)
├── storage/
│   ├── BinaryRecordFile.h          (v2 — header + journal + 32-bit IDs)
│   ├── IRepository.h               ← new abstract base
│   ├── BinaryRepository.h          ← templated default backend
│   ├── SqliteRepository.h          ← Track B (opt-in)
│   ├── repositories/               ← per-entity repos (thin specialisations)
│   ├── StorageService.{h,cpp}      ← + backup + audit + numbering wiring
│   ├── AuditLog.{h,cpp}            ← new
│   ├── BackupService.{h,cpp}       ← new
│   ├── migrations/                 ← new
│   └── sqlite3.{c,h}               ← Track B only
├── src/ui/
│   ├── common/UiErrorBus.{h,cpp}   ← new
│   ├── services/
│   │   ├── Exporter.{h,cpp}        ← CSV (+ XLSX later)
│   │   └── InvoicePrinter.{h,cpp}  ← QTextDocument → QPrinter
│   ├── models/
│   │   ├── + TransactionTableModel
│   │   ├── + AccountTableModel
│   │   ├── + CategoryTableModel
│   │   └── + BudgetTableModel
│   ├── components/tables/PaginationProxy.h  (wire it up)
│   ├── pages/ …                    (each page rewritten to use UiErrorBus + cache)
│   └── theme/                      (+ high-contrast variant)
├── tests/
│   ├── doctest/doctest.h
│   ├── storage/  …                 ← per-layer tests
│   └── core/     …
└── .github/workflows/ci.yml
```

---

## 9. Phased delivery (6 weeks, one engineer, full-time)

| Phase | Weeks | Theme | Exit criteria |
|---|---|---|---|
| 0 | 0.5 | Hygiene & safety net | `.gitignore`, delete duplicate root entities, add doctest skeleton, GitHub Actions green on a no-op test. |
| 1 | 1.0 | Domain types | `Money` + `IsoDate` shipped; entities migrated; one-shot data-folder migration script run on launch with backup. |
| 2 | 1.5 | Storage hardening (Track A) | `BinaryRecordFile` v2 (header + journal + 32-bit IDs + compaction); `IRepository<T>` abstraction; per-repo cache; `AuditLog`. Storage tests pass with crash-injection. |
| 3 | 1.0 | Invoice lines | `InvoiceLine` entity + repo; editor dialog rewritten; reports updated to use lines; derived customer balance. |
| 4 | 1.0 | UI quality of life | `UiErrorBus`; missing table models; pagination wired; Export (CSV) + Print (PDF); Reports moved off UI thread; numbering panel honoured. |
| 5 | 0.5 | Packaging | `windeployqt` (already there) + Inno Setup installer; backup-on-quit; smoke test on a clean Windows VM. |
| 6 (opt) | +1.0 | Track B | SQLite backend behind the same `IRepository<T>` interface; build flag; migration tool from binary `.dat` → SQLite. |

Phase order is dependency-driven: Money/IsoDate must land before storage v2 (so the new header isn't written with the old date format), storage v2 before lines (so the new entity gets the new format), lines before UI polish (Reports change shape with lines).

---

## 10. Concrete first PR (un-blocks everything after it)

A single PR worth reviewing in one sitting, to set the tone:

1. Add `.gitignore` for `build/`, `*.dat`, `.cache/`, `*.pptx`. Remove `brf_test.dat` from tracking.
2. Move `Account*.{h,cpp}`, `BankAccount*`, `CashAccount*`, `CheckingAccount*`, `SavingAccount*`, `SavingsAccount.h`, `Budget*` (root copies) into `core/`. Delete the duplicates. Fix `CheckingAccount::canWithdraw`, add `SavingsAccount.cpp`.
3. Add `core/Money.{h,cpp}` and `core/IsoDate.{h,cpp}` with doctest unit tests.
4. Add `tests/` directory, doctest header, one passing test per new type. Wire into `CMakeLists.txt` behind `option(ACCT_BUILD_TESTS "..." ON)`.
5. Add `.github/workflows/ci.yml` building on Windows + Ubuntu and running the test target.

Nothing user-visible changes; the foundation is now in place for every subsequent phase.

---

## 11. Out of scope (deliberate)

- Multi-user / network-share deployment. Move directly to a client-server topology (separate `acct-server` daemon) the day this is needed; do not retrofit `std::fstream`.
- Cloud sync. Not for v1.
- Mobile companion app. Not for v1.
- Per-row encryption inside the binary format. If you need this, take Track B (SQLite + SQLCipher) instead.
- A reporting DSL or scripting hooks. Stay with the 10 hard-coded report runners until users actually ask.

---

## 12. Success metrics

- Zero unhandled exceptions across a 1-hour scripted UI walk-through with fault injection (read-only data folder, antivirus locking files mid-write, kill-9 during update).
- Report runs ≤ 500 ms for 10k invoices on a mid-tier laptop (currently several seconds + UI freeze).
- A power cut during `update` leaves the database recoverable on next launch with no manual intervention.
- `windeployqt`-produced folder runs on a fresh Windows 11 VM with no MSYS2 installed.
- `git log` between v1.0.0 and v1.1.0 shows a one-line migration entry and the `.dat` files round-trip cleanly through it.

---

**TL;DR for the next contributor:** the architecture diagram you should be implementing is the one in §3. The first PR is in §10. The hard parts are §4.1 (journal-based crash safety) and §5.2 (invoice lines). Everything else is mechanical once those two land.
