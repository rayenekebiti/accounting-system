# AccountingPro — Handoff Document

Snapshot of the project as of 2026-06-07. Aimed at whoever picks this up next (including future you).

---

## TL;DR

A Qt 6 / C++26 desktop accounting application. **~62% complete.** The UI is fully built and runs against in-memory test data. Most data flows end-to-end through proper Qt model adapters; what's missing is persistence (Storage Layer 2), three form dialogs, a few wired filters, and some root-level cleanup.

Build with CMake + Ninja. Runs as a single Win32 `.exe`.

---

## How to Build & Run

```powershell
cd C:\Users\rayan\Videos\Captures\PROJECTS\accounting-system\accounting-system
cmake -S . -B build -G Ninja
cmake --build build --target AccountingSystem
./build/AccountingSystem.exe
```

**Toolchain:** MSYS2 UCRT64 / GCC 15.2 / Qt 6.x / C++26.
The build is `compile_commands.json`-aware so clangd in VS Code picks up the right flags. clangd path is hardcoded to `C:/msys64/ucrt64/bin/clangd.exe` in `.vscode/settings.json`.

---

## Project Structure

```
accounting-system/
├── main.cpp                 ← QApplication bootstrap, theme apply, MainWindow
├── constants.h              ← Record sizes, enums (TransactionType, AccountType)
├── CMakeLists.txt           ← single executable; CORE/MODEL/DIALOG/UI source groups
│
├── core/                    ← Entity classes (FileManager-compatible)
│   ├── Category.{h,cpp}     ← 48-byte record
│   ├── Budget.{h,cpp}       ← 32-byte record
│   ├── Customer.{h,cpp}     ← 128-byte record
│   ├── Supplier.{h,cpp}     ← 128-byte record
│   ├── Product.{h,cpp}      ← 192-byte record
│   ├── Invoice.{h,cpp}      ← 96-byte record + InvoiceStatus enum
│   ├── Payment.{h,cpp}      ← 64-byte record + PaymentMethod, PartyType enums
│   └── AccountingSystem.{h,cpp}  ← in-memory coordinator (O(1) id lookups)
│
├── transaction/             ← Polymorphic transaction hierarchy (pre-existing, untouched here)
│   ├── transaction.{h,cpp}          ← Base + TransactionData struct
│   ├── ExpenseTransaction.{h,cpp}
│   ├── IncomeTransaction.{h,cpp}
│   ├── RecurringExpense.{h,cpp}
│   └── RecurringIncome.{h,cpp}
│
├── storage/                 ← Storage layer (partial)
│   ├── ARCHITECTURE.md             ← Layer design (5-layer storage stack)
│   ├── BinaryRecordFile.h          ← Layer 1: fixed-size byte I/O (production-grade)
│   └── BinaryRecordFile_REVIEW.md  ← Deep-dive on every defect that was fixed
│
├── Account.{cpp,h}          ← Pre-existing account hierarchy (still has bugs — see Known Issues)
├── BankAccount.{cpp,h}
├── CashAccount.{cpp,h}
├── CheckingAccount.cpp
├── SavingAccount.cpp
├── SavingsAccount.h         ← header-only, never implemented
│
└── src/ui/
    ├── app/                 ← Application shell
    │   ├── MainWindow.{h,cpp}      ← Registers all 8 pages, status bar, toolbar
    │   ├── NavigationPanel.{h,cpp} ← Collapsible 210/48px sidebar + NavBadge
    │   ├── GlobalToolBar.{h,cpp}   ← New dropdown + search + period combo
    │   ├── PageHeader.{h,cpp}      ← Title + action buttons
    │   └── PageRouter.{h,cpp}      ← Page registry + lifecycle
    │
    ├── theme/ThemeManager.{h,cpp}  ← Dark/Light, full QSS stylesheet
    │
    ├── pages/               ← 8 pages (all extend Page or ListPage)
    │   ├── base/{Page,ListPage}.{h,cpp}
    │   ├── dashboard/DashboardPage.{h,cpp}    ← KPI strip + 2-column layout
    │   ├── customers/CustomersPage.{h,cpp}    ← Model+proxy+search+filter+dialog
    │   ├── suppliers/SuppliersPage.{h,cpp}    ← Model+proxy+search (no filter wired)
    │   ├── products/ProductsPage.{h,cpp}      ← Model+proxy+search (no filter wired)
    │   ├── invoices/InvoicesPage.{h,cpp}      ← Model+proxy+search+status tabs+dialog
    │   ├── payments/PaymentsPage.{h,cpp}      ← Model+proxy+search (no filter wired)
    │   ├── reports/ReportsPage.{h,cpp}        ← Catalog UI built; execution stubbed
    │   └── settings/SettingsPage.{h,cpp}      ← Full UI + QSettings persistence
    │
    ├── models/              ← QAbstractTableModel adapters
    │   ├── CustomerTableModel.{h,cpp}
    │   ├── SupplierTableModel.{h,cpp}
    │   ├── ProductTableModel.{h,cpp}
    │   ├── InvoiceTableModel.{h,cpp}
    │   └── PaymentTableModel.{h,cpp}
    │
    ├── dialogs/             ← Modal form editors
    │   ├── CustomerEditorDialog.{h,cpp}   ← Add + Edit modes, validation
    │   └── InvoiceEditorDialog.{h,cpp}    ← Add + Edit, status combo, total auto-calc
    │
    ├── common/UiTypes.h     ← PageId enum
    │
    └── components/          ← Reusable widgets
        ├── display/         ← KpiCard, MiniBarChart, MoneyLabel, StatusBadge
        ├── tables/          ← DataTableView, PaginationFooter, RowActionsDelegate
        ├── inputs/          ← SearchBar (debounced), FilterBar
        ├── forms/           ← FormRow (label+field+error), SectionHeader
        └── feedback/        ← BusyOverlay, EmptyStateWidget, ConfirmDialog
```

---

## What Works

### Application shell (100%)
- Navigation panel with collapse/expand, section headers, NavBadge ("7" on Invoices)
- Page routing with lifecycle (`onActivated`/`onDeactivated`)
- Dark/Light theme with live switching
- Status bar with company/period/version
- Global "+ New" dropdown (Invoice / Payment / Customer / Supplier / Product)

### Pages with real model wiring
| Page | Model | Search | Status filter | Dialog (Add/Edit) |
|---|---|---|---|---|
| Dashboard | hardcoded QStandardItemModel (curated) | — | — | — |
| Customers | `CustomerTableModel` | ✅ | ✅ Active/Inactive | ✅ |
| Suppliers | `SupplierTableModel` | ✅ | ❌ (cosmetic) | ❌ |
| Products | `ProductTableModel` | ✅ | ❌ (cosmetic) | ❌ |
| Invoices | `InvoiceTableModel` | ✅ | ✅ tabs (Draft/Posted/Overdue/Paid) | ✅ |
| Payments | `PaymentTableModel` | ✅ | ❌ (cosmetic) | ❌ |
| Reports | catalog UI only | — | — | — |
| Settings | n/a | — | — | n/a |

### Settings persistence
Uses `QSettings` (Windows registry: `HKCU\Software\AccountingPro\AccountingPro`). All 19 fields across Company / Preferences / Numbering / Tax / Appearance load on construction and save on click. Save/Revert buttons properly track dirty state via `m_loading` flag.

### Storage Layer 1 (`BinaryRecordFile`)
Fixed-size binary record I/O. Supports `append/read/update/count` with flush-on-write durability, uint16_t overflow protection, and buffer-zeroing on failed reads. **Note:** the repo's current `BinaryRecordFile.h` may have been reverted to an earlier (buggy) version — verify against `BinaryRecordFile_REVIEW.md` before consuming it from Layer 2.

---

## What Doesn't Work

### Bottleneck #1 — Storage Layers 2/3 don't exist
Pages use in-memory test data. No `CustomerRepository`, `InvoiceRepository`, etc. No `StorageService` facade. When the app closes, every Add/Edit is lost. **This is the single biggest gap.** See `storage/ARCHITECTURE.md` for the four-layer design.

### Bottleneck #2 — 3 of 5 dialogs missing
Only Customer and Invoice editors exist. Supplier / Product / Payment pages have an "Add" action but it does nothing.

### Filters and pagination
- Status/Method/Party combos on Suppliers/Products/Payments are decorative — selecting an option doesn't filter (no slot wired).
- Invoice's "Customer" filter combo has no options and no behavior.
- `PaginationFooter` prev/next buttons emit signals but nothing listens. The footer's "Total records" label tracks the *filtered* count, which is slightly misleading.

### Pre-existing defects (untouched)
- `CheckingAccount::canWithdraw()` logic is inverted (`money >= MAX_OVERDRAFT_LIMIT`, which is `-20000` — always true).
- `SavingsAccount` has a header but no `.cpp`.
- `Account.cpp` empty / placeholder.
- `CategoryReport` declares a static `categoryTotals` vector at namespace scope — scope/lifetime confusion.
- `MonthlyReport::exportToFile()` is a TODO comment.

### Other minor items
- Sort indicator is hidden but column sorting is enabled — clicking a header sorts but users can't tell. Fix: `setSortIndicatorShown(true)` in `DataTableView.cpp:123`.
- `computeNextId()` in both wired pages overflows silently when an entity hits id 65535.
- Filter classes compare against displayed text ("Active"/"Paid") rather than the underlying enum — fragile to label changes.
- `m_proxy` in `CustomersPage.h` is typed as `QSortFilterProxyModel*` but actually points to `CustomerFilterProxy`; `rebuildFilter` uses `static_cast` to recover the derived type. Works, but no compile-time guard.

---

## Architecture Decisions to Preserve

1. **Fixed-size binary records** (not SQLite, not JSON). Each entity has a record size defined as `inline constexpr` and a hand-rolled byte-offset `serialize/deserialize`. Position-based IDs (`offset = id * recordSize`). Soft delete via `isDeleted` byte in the record.

2. **TransactionData pattern.** Every entity has a plain-data struct (`CustomerData`, `InvoiceData`, etc.) with `const char*` strings used only for clean construction. The class itself stores `char[N]` and copies via `strncpy` + null-fill. A default constructor produces a zero-state object for FileManager read-back; a validating constructor throws `std::invalid_argument` on bad input. Every entity has `isValid()` and `serialize()` throws `std::logic_error` if invalid.

3. **Repository pattern (planned).** One `*Repository` per entity, owns a `BinaryRecordFile`, handles polymorphism via a type byte for transactions/accounts. `StorageService` is the singleton facade the UI talks to. See `storage/ARCHITECTURE.md`.

4. **Qt model adapter layer.** Each entity has a `*TableModel` (`QAbstractTableModel` subclass) that holds `std::vector<Entity>`. Pages wrap that in `QSortFilterProxyModel` (sometimes a custom subclass) for search and column filters. **Always `mapToSource(proxy->index(row, 0)).row()`** before calling `m_model->at(row)`.

5. **Single-threaded.** All I/O on the Qt UI thread. Fine for desktop with sub-100k records.

6. **No CLAUDE.md, no AI metadata in commits** — the user prefers their own name on commits.

---

## Files Map (the important ones)

| File | Purpose | Status |
|---|---|---|
| `CMakeLists.txt` | Single executable, AUTOMOC on, includes `core/` and `src/ui/` | ✅ wired |
| `main.cpp` | Bootstrap, theme apply, set org/app name for QSettings | ✅ |
| `constants.h` | Record sizes for Transaction/Account/Category/Budget; enums | ✅ — don't grow this for new entities; define in entity header |
| `core/*.{h,cpp}` | 8 entity classes (7 records + 1 coordinator) | ✅ |
| `src/ui/models/*.{h,cpp}` | 5 Qt model adapters | ✅ |
| `src/ui/dialogs/*.{h,cpp}` | 2 of 5 dialogs (Customer + Invoice) | ⚠️ 3 missing |
| `storage/BinaryRecordFile.h` | Layer 1, fixed-size record I/O | ⚠️ may need re-fixing |
| `storage/ARCHITECTURE.md` | Long-form layered storage design | ✅ reference doc |
| `storage/BinaryRecordFile_REVIEW.md` | Detailed defect-by-defect review of Layer 1 | ✅ reference doc |
| `src/ui/pages/settings/SettingsPage.{h,cpp}` | QSettings-backed full settings panel | ✅ |
| `src/ui/pages/dashboard/DashboardPage.{h,cpp}` | KPI strip + recent invoices + overdue + summary | ✅ uses curated mock |

---

## What To Do Next (in priority order)

1. **Verify `storage/BinaryRecordFile.h`** matches the fixed version described in `BinaryRecordFile_REVIEW.md`. The repo may currently hold the earlier buggy version (silent write errors, no flush, uint16_t overflow). Re-apply fixes #1-4 from the review doc before consuming it from Layer 2.

2. **Build Storage Layer 2 (Repositories).** One per entity in `/storage/`. The pattern is in `ARCHITECTURE.md`. `CustomerRepository` first; the other 6 are mechanical copies. ~3-4 hours if you understand the pattern, ~6-8 hours learning it.

3. **Build `StorageService` (Layer 3 / facade).** Singleton with one accessor per repository. Replace `loadTestData()` in each page with `StorageService::instance().customers().loadAll()`.

4. **Add the 3 missing dialogs.** Mirror `CustomerEditorDialog` for Supplier, Product, Payment. ~30 min each.

5. **Wire the cosmetic filter combos.** Each page needs a custom `*FilterProxy` subclass (or extend the existing one). Pattern is shown in `CustomersPage::CustomerFilterProxy`.

6. **Fix pre-existing root-level bugs:** `CheckingAccount::canWithdraw`, missing `SavingsAccount.cpp`, scope bug in `CategoryReport`.

7. **Reports execution.** `ReportsPage::onRunClicked` currently just toggles UI state. Implement Monthly and Category reports against the repositories.

8. **Either wire pagination or hide the buttons.** True pagination requires a row-range proxy layered on top of the filter proxy.

---

## Acknowledged Compromises

- **Test data is hardcoded in each page's `loadTestData()`.** When storage lands, replace per page in ~10 lines each.
- **Pagination is decorative** — the footer shows numbers but doesn't slice the visible rows. The architecture supports it (a custom QSortFilterProxyModel that limits row count); just hasn't been built.
- **No `compact()` operation** on `BinaryRecordFile`. Soft-deleted rows accumulate on disk forever. Acceptable for a desktop app under normal use; add a "Compact database" Settings action if it ever matters.
- **Polymorphic transaction storage is designed but not built.** The architecture handles it via a type byte at a fixed offset; the existing transaction classes already serialize this way. Just need the repository's `makeFromBuffer()` factory.

---

## Stats

- **~5,400 lines** of C++ across `/core`, `/src/ui/models`, `/src/ui/dialogs`, and the page rewrites.
- **17 reusable Qt components** in `src/ui/components/`.
- **8 entity classes** in `/core` (7 records + 1 coordinator), all with validating constructors and `isValid()` checks.
- **0 warnings** with `-Wall -Wextra` (after deprecation fixes).
- **1 architecture doc** (`storage/ARCHITECTURE.md`).
- **1 deep-dive review doc** (`storage/BinaryRecordFile_REVIEW.md`).
