# Production Readiness Review — AccountingPro (Qt6 Desktop)

## Executive Summary

This is a competent C++ course/portfolio project that demonstrates a clean 4-layer storage architecture, a polished Qt6 UI, and a reasonable separation between domain entities, repositories, and table models. It is **not** a production accounting system, and it is several months of work away from being one. The persistence layer is a hand-rolled fixed-size binary file with no crash safety, no schema versioning, no concurrency control, and a hard 65,535-record ceiling per entity. Several user-visible features that look implemented are dead UI: Export, Print, and the Reports "Export ▾" button are unconnected `QAction`/`QPushButton` instances. Invoices have no line items — they are header-only — so the Products entity, the Settings page numbering counters, and ten "report types" are decorative more than functional. Storage layer errors propagate as `std::runtime_error` but are *never caught* outside of editor-dialog form validation, so any I/O failure on save/update/load — disk full, antivirus lock, file locked by another process, corrupt file — terminates the application via `std::terminate`. The 4-layer ARCHITECTURE.md explicitly acknowledges these limits as "not for a real product"; this review answers the literal question — what would need to change before real users could rely on it — and the gap is large.

The codebase is consistent, readable, and the binary format choices are *internally* defensible. The issues below are about the gap between that and a system that handles money for real businesses.

---

## BLOCKING

Issues that will cause data loss, crashes, or prevent shipping to real users.

### 1. Storage exceptions terminate the app — UI never catches them

- **Where:** `src/ui/pages/customers/CustomersPage.cpp:120-122`, `invoices/InvoicesPage.cpp:154-158, 177-180, 205-209`, `payments/PaymentsPage.cpp:133-136, 163-166`, and every other page that calls `.save()` / `.update()` / `.loadAll()`.
- **Failure mode:** `BinaryRecordFile::append()` throws `std::runtime_error` on disk-full, antivirus lock, file-locked-by-other-process, and write failure (`BinaryRecordFile.h:59, 71, 77`). The append/update return-value contract is documented in ARCHITECTURE.md ("I/O failure ... throw `std::runtime_error`. UI catches and shows `ConfirmDialog`"). **The UI never catches it.** A `grep` for `catch` across `src/ui/pages` returns zero hits. The only `catch` blocks in UI code are inside the five editor dialogs and they only catch domain-construction errors. A failed write reaches `QApplication::exec()` and calls `std::terminate`. The user loses their entire session — including unsaved edits in other dialogs, scroll position, and undo state.
- **Required:** Wrap every `StorageService::instance().<repo>().save/update/remove/loadAll` call in `try { ... } catch (const std::exception& e) { ConfirmDialog::show(...); }`. The mechanism (`ConfirmDialog`) already exists at `src/ui/components/feedback/ConfirmDialog.h`. It is not wired in.

### 2. No crash safety on writes — partial writes corrupt the file silently

- **Where:** `storage/BinaryRecordFile.h:107-125` (`update`) and `:53-81` (`append`).
- **Failure mode:** Update does an **in-place overwrite** with `std::fstream::write` + `flush`. If the process is killed (power loss, OS crash, OOM kill, user clicks "End Task") between `seekp` and `write`, or mid-write, the on-disk record is partially overwritten with arbitrary bytes from the new buffer. There is no write-ahead log, no shadow file, no `fsync`-and-rename pattern, no checksum. `flush()` on a `std::fstream` does not call `fsync`/`FlushFileBuffers`; the OS may still have the bytes in its page cache. On Windows the situation is worse: NTFS journaling protects filesystem metadata, not user-file content. Next launch: a record's status byte may read as garbage (e.g. value 17), `Invoice::deserialize` will map it to `INVOICE_STATUS_UNKNOWN` (Invoice.cpp:105), the record will fail `isValid()`, and `loadAll()` silently drops it. The user sees an invoice "disappear." Money disappears with it.
- **Required:** At minimum, write to a temp file and atomically rename, OR add a header checksum per record and a separate journal file, OR switch to SQLite (which the architecture doc admits is the right tool). For a binary-format course assignment, document this explicitly and add a "always back up data folder" warning at startup. Do not ship to users.

### 3. `StorageService::initialize` return value is ignored — app runs with no storage

- **Where:** `main.cpp:26`, `storage/StorageService.h:44-60`.
- **Failure mode:** `initialize()` swallows the exception and returns `false`. `main.cpp` discards the return: `StorageService::instance().initialize(dataPath.toStdString());`. If the data directory is unwritable (read-only mount, permissions, antivirus), the app shows an empty UI. `isInitialized()` returns `false`, so pages like `CustomersPage::loadFromStorage` silently no-op (CustomersPage.cpp:65-68), `onAddClicked` does not save (CustomersPage.cpp:120-121), but the row still appears in the model (`m_model->appendRow(cust)` runs). The user adds 30 customers, sees them in the table, closes the app, and loses everything. No error message at any point.
- **Required:** Check the return of `initialize()` in `main.cpp`; show a fatal-error dialog and exit non-zero if it returns `false`. Also: re-throw inside `initialize` instead of swallowing in the catch at `StorageService.h:57-59`, so the failure reason is visible.

### 4. No file locking — second instance corrupts the database

- **Where:** `storage/BinaryRecordFile.h:35-47` (`open()`).
- **Failure mode:** Two copies of `AccountingPro.exe` running simultaneously (user double-clicks the shortcut twice, runs from Start menu and Desktop, or has the app installed for two Windows users sharing a roaming profile) both open `customers.dat` `in|out|binary`. There is no `OPEN_EXISTING|FILE_SHARE_NONE`, no advisory `flock`, no lockfile. Both write at file end with `seekp(0, std::ios::end)`. Both will get the same `id`. The second write overwrites the first record. IDs collide. The `loadAll` skip-soft-deleted scan will see two records with identical IDs but different content; downstream `unordered_map<uint16_t, QString>` lookups will pick whichever loads last. Books no longer balance.
- **Required:** Acquire a Win32 exclusive lock on every `.dat` file at open (`CreateFileW` with `dwShareMode = 0`). On lock failure, surface a dialog: "AccountingPro is already running." `std::fstream` doesn't expose this — either drop down to the Win32 API, use `QLockFile` on a sentinel file, or both. For multi-user/network share access (which a real accounting product needs), this entire layer must be replaced.

### 5. Stack-allocated record buffers tied to entity constants that can grow — silent overflow if a record size constant is increased after fields are added

- **Where:** every repository (`CustomerRepository.h:20, 27, 34, 43, 53`; `InvoiceRepository.h:20, 27, 34, 43, 53`; `PaymentRepository.h:20, 27, 34, 43, 53`; `SupplierRepository.h`; `TransactionRepositories.h`).
- **Failure mode:** `char buf[CUSTOMER_RECORD_SIZE]` is the safe pattern *today*. But the discipline that keeps it safe is fragile: the offsets in `Customer::serialize` (Customer.cpp:52-65) are hardcoded numeric literals (e.g. `buffer + 122`), and the record size is just `128` (Customer.h:5). There are **no `static_assert`s** anywhere proving that `122 + sizeof(unsigned char) <= CUSTOMER_RECORD_SIZE`, that field offsets are non-overlapping, or that the deleted-flag offset constant in `CustomerRepository.h:10` (`CUST_DELETED_OFFSET = 122`) matches the offset used by `Customer::serialize`. A junior engineer adding a `notes` field, bumping `CUSTOMER_RECORD_SIZE` to e.g. 160, and updating the serialize offsets but forgetting to update `CUST_DELETED_OFFSET` in the repository is *almost guaranteed* to flag the wrong byte as the soft-delete bit and either resurrect deleted records or silently delete live ones during the next save.
- **Required:** Add `static_assert`s that pin every offset constant in every repository to the same compile-time value as the entity defines. Better, declare the layout in one place (the entity header) and have repositories reference *that* constant — never duplicate the offset.

### 6. Hard 65,535-record ceiling silently breaks a real business

- **Where:** `storage/BinaryRecordFile.h:25, 63-66`.
- **Failure mode:** A real small business issues ~1,000 invoices/year. A 10-year-old shop hits the limit on `invoices.dat` and `append()` starts throwing `std::length_error` — which, per blocking issue #1, terminates the app. There is no compaction. Soft-deleted records *still* count against the ceiling. A user who creates and deletes 65,535 invoices over the product's life cannot create the 65,536th. For payments (one invoice + multiple payments) the ceiling is hit faster. For products in a retail/wholesale business the ceiling is hit on day one.
- **Required:** Either widen IDs to `uint32_t` (which doubles file size for tiny entities and changes the on-disk format), or implement a compaction pass that rewrites the file dropping soft-deleted records and reassigns IDs (which breaks every foreign-key reference in invoices/payments because IDs are file-position-derived). The honest answer is: this format is fundamentally inappropriate for a multi-year accounting system. Use SQLite.

### 7. Invoices have no line items — financial totals are user-entered free text

- **Where:** `core/Invoice.h:36-44`, `src/ui/dialogs/InvoiceEditorDialog.cpp:41-56`.
- **Failure mode:** The Invoice record stores `subtotal`, `taxAmount`, `total` as three independent `double` fields. The user types them in. The dialog computes total via `recomputeTotal()` (InvoiceEditorDialog.cpp:126-129) as `subtotal + tax` — but nothing prevents the user from saving a record where `total != subtotal + tax` (e.g. by editing via deserialize-and-rewrite from a corrupt file). There are no Invoice line items, no product references, no quantities, no per-line tax. An "invoice" in this system is a header with three numbers. The Products entity (`core/Product.h`) exists but is never referenced from Invoice. The Reports "Profit & Loss" cannot compute cost-of-goods-sold because no invoice line ever links to a product cost. **You cannot run a business on this.**
- **Required:** A new `InvoiceLine` entity with `(invoiceId, productId, quantity, unitPrice, lineTax)`, a `InvoiceLineRepository`, and a rewritten editor dialog that derives subtotal/tax/total from lines. This is a multi-week feature, not a polish item.

### 8. ID assignment is racy and inconsistent between repository and UI

- **Where:** `src/ui/pages/customers/CustomersPage.cpp:104-112` and `:117`, `invoices/InvoicesPage.cpp:123-131` and `:151`, and the corresponding `save()` overrides in repositories (`CustomerRepository.h:16-23`, `InvoiceRepository.h:16-23`).
- **Failure mode:** Each page computes "next id" by scanning the in-memory model (`computeNextId`) — `maxId + 1`. The dialog then constructs an entity with that ID. On save, the repository **overwrites the id** with `file_.count()` (`CustomerRepository.h:18`). So the dialog's nice "next id = 1001" is silently discarded and replaced with whatever the file position implies. Worse, `CustomersPage::computeNextId` starts at 1000 (CustomersPage.cpp:106) but `Invoice` uses 0 + auto-bump — so first-customer-ever has dialog id 1001 but on-disk id 0. Any FK reference written with the dialog's id is now broken. This is a latent bug that only manifests once the customer record's id is used as a foreign key by Invoice.customerId — which the InvoiceEditorDialog *does* via `m_customerCombo->itemData(custIdx).toInt()` reading `c.getId()` (InvoiceEditorDialog.cpp:122), so it uses the *repository's* id, not the dialog's. That's accidental correctness; a single change in either place breaks it.
- **Required:** Pick one source of truth for IDs and document it. Either: (a) the repository owns ID assignment, the dialog asks for a placeholder; or (b) the dialog computes the ID and the repository accepts it. The current split — dialog computes, repo overwrites — is a trap.

---

## MAJOR

Issues that significantly limit real-world usability.

### 9. Export and Print are dead UI

- **Where:** every list page (e.g. `src/ui/pages/customers/CustomersPage.cpp:101` `new QAction("Export", this)`), `src/ui/pages/reports/ReportsPage.cpp:112-122` (Export, Print buttons), `src/ui/pages/invoices/InvoicesPage.cpp:117, 119`.
- **Failure mode:** The "Export" actions are bare `QAction` objects with no `connect()` call and no slot. Clicking them does nothing. Same for the Reports "Export ▾" button. `QPrinter`, `QPrintDialog`, CSV writers — none of these are referenced anywhere in the source tree (verified by grep on `QPrint|QPrinter|writeCsv|toCsv|saveAs`). Real accountants live and die by exporting to Excel/CSV and printing invoices. Shipping these buttons in a disabled state would be acceptable; shipping them visible and silent is a UX trap and looks dishonest.
- **Required:** Either hide the actions, disable them with a tooltip "Coming in v1.1", or implement CSV export (the easy one, ~2 hours per page) and printable PDF invoices (a real day of work using `QTextDocument` + `QPrinter`).

### 10. Soft-deleted records accumulate forever — file bloat and slowdown

- **Where:** `storage/BinaryRecordFile.h` (no `compact()`), every repository's `loadAll()`.
- **Failure mode:** Every list page calls `loadAll()` on activation (e.g. `CustomersPage.cpp:64-68`). `loadAll` reads *every* record in the file, including soft-deleted ones, and filters in memory. A business that deletes 80% of test invoices in year 1 still pays the read cost forever. Reports (`ReportsPage.cpp:322-323`, `:373-374`, `:454`, etc.) call `loadAll()` for both customers and invoices on every Run-Report click. With 10,000 records each (well under the 65,535 ceiling), each report is at least 2 full file scans on the UI thread, plus a third for payments. Expected to feel sluggish at 5k records, painful at 20k. No measurements have been done — the QStandardItemModel rebuild for the dashboard `onActivated` is another full scan.
- **Required:** A `compact()` method that rewrites the file skipping soft-deleted records. *But* this breaks position-based IDs (issue #6, #8) and every cross-entity reference. So really: an in-memory cache inside each repository that loads once at startup and invalidates on write. Or: SQLite.

### 11. UI thread blocks on all I/O — no async, no progress, no cancel

- **Where:** every page's `onActivated()`, `loadFromStorage()`, `onRunClicked` in ReportsPage.
- **Failure mode:** ARCHITECTURE.md says "Fine for a desktop app with sub-100k records." That's optimistic. `ReportsPage::runProfitAndLoss` (ReportsPage.cpp:620-663) loads invoices + payments in series, parses dates with `QDate::fromString` per row (expensive), bucket-sorts them. With 20k invoices + 20k payments the UI freezes for several seconds; with 60k it's tens of seconds. No `QtConcurrent`, no progress indicator (the `BusyOverlay` component exists but is only used cosmetically via `m_resultTable->showBusy(true)` before the synchronous compute starts, then `false` after — the overlay is never actually shown to the user because the UI thread is blocked).
- **Required:** Move long reports to a worker thread via `QtConcurrent::run` + `QFutureWatcher`, or paginate at the storage level. The current architecture cannot do this safely because `BinaryRecordFile` is not thread-safe (no mutex) and the architecture doc explicitly disallows it ("Single-threaded").

### 12. No audit trail — required for any real accounting system

- **Where:** the entire codebase. There is no `audit_log.dat`, no who-changed-what, no immutable history.
- **Failure mode:** Every accounting standard (SOX, IFRS, GAAP, GDPR Article 30, every national tax code) requires that financial transactions be auditable: who created/modified/voided an invoice, when, from where. This product overwrites records in place with no history. Voiding an invoice is just a status change — the original amount is gone. There is no concept of an immutable journal entry. A tax auditor walks in, asks "show me all changes to invoice INV-1024" — the answer is "we can show you the current state." This is disqualifying for any business that pays taxes.
- **Required:** Append-only audit log of every write, with timestamp and (if multi-user) user id. Even single-user, the tax inspector wants to see it.

### 13. Dates stored as `"d MMM yyyy"` text — locale-fragile and unsortable

- **Where:** `core/Invoice.cpp:42, 78`, `core/Payment.cpp:49`, `src/ui/dialogs/InvoiceEditorDialog.cpp:35, 39, 217, 219`, `src/ui/pages/reports/ReportsPage.cpp:33-36`.
- **Failure mode:** Issue dates are stored as 12-byte ASCII like `"5 Jun 2026"`. This format depends on the C/Qt locale of *the machine that wrote the record*. If the writer's locale was French ("5 juin 2026", 9 bytes) and the reader's is English, `QDate::fromString(s, "d MMM yyyy")` returns an invalid date. The whole record is then silently dropped from reports (ReportsPage.cpp:340 `if (!due.isValid()) continue;`). Date sort is alphabetic — "1 Apr" comes before "1 Jan" by string compare. The Transaction repo's `findByDateRange` uses `std::strncmp` (`TransactionRepositories.h:117`), which is wrong for any format other than `YYYY-MM-DD`.
- **Required:** Store dates as ISO `YYYY-MM-DD` text (10 bytes, locale-free, lexicographically sortable) or as `int64_t` Julian day numbers. This is a one-shot format change.

### 14. No password/authentication of any kind

- **Where:** missing.
- **Failure mode:** Anyone with file access reads `customers.dat`, `invoices.dat`, etc. — they are plain bytes in `%APPDATA%/AccountingPro/`. There is no encryption at rest, no Windows DPAPI use, no master password, no user accounts. A Windows guest user on a shared PC can rename `customers.dat` to `customers.dat.bak`, restart the app, and have a blank book — then restore the file later. For a single-user home-office app this is borderline acceptable; for any business handling third-party customer data (GDPR/CCPA implies) it is a violation.
- **Required:** At minimum, encrypt the data folder with DPAPI (one Win32 call). Better: a master password unlocking a key. Honestly: stop pretending this is suitable for storing personally identifiable customer data and contact info until that exists.

### 15. No backup, restore, or export of the whole dataset

- **Where:** missing.
- **Failure mode:** The user loses laptop / SSD dies / Windows reset wipes `%APPDATA%`. The data is gone. There is no "Backup now," no automatic backup, no "Export all to CSV," no cloud sync. For a tool that holds the only copy of a small business's accounts, this is reckless.
- **Required:** A "Backup..." menu item that ZIPs the data folder; ideally automatic daily backups to a user-chosen directory. This is a 1-day feature and there is no reason it isn't in v1.

### 16. Settings "Numbering" UI is decorative — Invoice number generation ignores it

- **Where:** `src/ui/pages/settings/SettingsPage.cpp:145-166` (Numbering panel), `src/ui/pages/invoices/InvoicesPage.cpp:133-146` (suggestNextNumber).
- **Failure mode:** Settings lets the user pick "Invoice Prefix = INV-" and "Next Invoice # = 1001", which is saved to `QSettings`. `InvoicesPage::suggestNextNumber` hardcodes `int maxNum = 1000;` and `QString("INV-%1")` — it never reads `QSettings`. The same is true for Payment numbering. The Tax panel ("VAT 19%", "TVA 7%") is also unread — invoices accept any user-typed tax amount. The Settings page presents a contract it does not honour.
- **Required:** Read the QSettings keys in `suggestNextNumber()` and the invoice/payment editors. Otherwise hide the panels.

### 17. No DLL bundling — the exe will not run outside the MSYS2 shell

- **Where:** `CMakeLists.txt:89-93`, `build/` directory (no `Qt6Widgets.dll`, no `Qt6Core.dll`, no `libgcc_s_seh-1.dll`, no `platforms/qwindows.dll`).
- **Failure mode:** The produced binary depends on Qt6 DLLs and the MSYS2 UCRT64 GCC runtime. Double-clicking `AccountingSystem.exe` from a fresh user account, or copying to another Windows machine, will fail with "Qt6Widgets.dll not found" or silently with no console output. There is no `windeployqt` invocation in CMake, no NSIS/WiX installer, no code signing. A user cannot install this product.
- **Required:** Add a `windeployqt` step to CMake (`add_custom_command(... windeployqt ...)`), build an installer (Inno Setup is 1 hour for the basic case), and obtain a code-signing certificate (or accept SmartScreen warnings forever).

### 18. Reports query loop reloads the database three times per Run click

- **Where:** `src/ui/pages/reports/ReportsPage.cpp:454, 631, 642, 675` etc.
- **Failure mode:** `runProfitAndLoss` calls `invoices().loadAll()` and `payments().loadAll()`; `runCashFlow` calls `payments().loadAll()`; running both back-to-back, plus a dashboard refresh, performs many full-file scans. No caching. Combined with issue #11 (single-threaded I/O) this gets painful fast.
- **Required:** Cache the loaded vector inside the repository, invalidate on write. Or — see SQLite refrain.

### 19. Customer balance is not derived from invoices/payments — manually editable, drifts

- **Where:** `core/Customer.h:29` (`balance` field), `src/ui/dialogs/CustomerEditorDialog.cpp` (no validation), `src/ui/pages/reports/ReportsPage.cpp:396` (uses `c.getBalance()` directly).
- **Failure mode:** Customer.balance is a user-editable double. Posting an invoice does not update the customer balance. Receiving a payment does not update the customer balance. The "Outstanding" column in Customer Statement (ReportsPage.cpp:381-385) is computed from invoices, but the "Balance" column (line 396) reads the manually-typed field. These will drift. A user sees two different totals on the same row and lose trust in the system instantly.
- **Required:** Either remove the manually editable balance (compute everywhere) or treat it as a "starting balance" with a clear distinction from "current balance" in the UI.

---

## MINOR

Polish and hardening items that should be fixed but won't block a portfolio demo.

### 20. `BinaryRecordFile::open()` truncates a corrupt file

- **Where:** `storage/BinaryRecordFile.h:39-46`.
- **Failure mode:** If the in/out open fails for reasons other than "file doesn't exist" (e.g. permission denied), the code falls through to `std::ios::out` which would truncate-on-create. The subsequent close-and-reopen-in-in/out then succeeds against a zero-byte file. The user's data is silently erased. The current logic *probably* doesn't hit this in practice because the first open generally fails only when the file is missing, but the safety is accidental, not designed.
- **Fix:** Distinguish "missing" from "permission denied" before deciding to create. On POSIX use `stat`/`errno`; on Windows use `GetFileAttributesW`.

### 21. `Invoice::isValid()` accepts `INVOICE_DRAFT` records with empty dates and zero customer ID

- **Where:** `core/Invoice.cpp:51-56`.
- **Failure mode:** `isValid` checks only that the number is non-empty, status is known, totals are non-negative. A draft with `customerId == 0` and empty `issueDate` is "valid" and gets saved. Reports skip drafts so it's mostly hidden, but the user can post the draft later and end up with a posted invoice attached to customer #0 with no issue date.
- **Fix:** Tier validation by status. Posted/Paid invoices require a customer, dates, and balanced totals.

### 22. `count()` is non-const and seeks the get pointer to end-of-file before every read in `loadAll`

- **Where:** `storage/BinaryRecordFile.h:130-139`, `loadAll` in each repository.
- **Failure mode:** Every `loadAll()` calls `count()` once, then `read()` for each row, and `read()` does its own seek. Not a correctness issue, but in a hot path (Reports refreshing on each click). Calling `count()` mutates the file's get-pointer state, which is why the comment at line 128 says "Non-const because it moves the get pointer." If a future change adds a seekg-aware read path that *assumes* the get pointer hasn't moved, this becomes a bug.
- **Fix:** Cache `count()` if the file isn't being externally mutated, or stat the file size directly. Document the side effect prominently.

### 23. `Customer::deserialize` and friends do not validate the unused padding bytes

- **Where:** `core/Customer.cpp:67-82`, `core/Invoice.cpp:89-109`, `core/Payment.cpp:93-114`.
- **Failure mode:** The padding bytes (e.g. Customer bytes 123..127) are written as zero by `serialize` (because of the leading `memset`). Deserialize ignores them. A file written by a hypothetical v2 that uses those bytes for new fields would load fine in v1, silently discarding those fields — no error, no warning. Same problem in reverse.
- **Fix:** Combined with the "no magic bytes / no versioning" gap in #25 below. A 4-byte file-header version is a one-line fix.

### 24. `BinaryRecordFile` has no destructor / RAII close, no `std::fstream::sync` on shutdown

- **Where:** `storage/BinaryRecordFile.h`.
- **Failure mode:** `std::fstream` closes on destruction. `StorageService::instance()` is a function-local static, so it destructs at program exit. Fine for normal exit. For abnormal exit (crash, kill), buffered writes may be lost — see issue #2. There is no opportunity to flush all open files before a controlled shutdown initiated by, say, Windows logoff.
- **Fix:** Add `StorageService::shutdown()` and call it from `MainWindow::closeEvent` / `QApplication::aboutToQuit`. The C++ standard library does not guarantee a clean flush for fstreams that exist at static-storage duration on all platforms.

### 25. No magic bytes, no schema version, no migration

- **Where:** all `.dat` files.
- **Failure mode:** Documented in ARCHITECTURE.md as a deliberate v1 choice. That is a defensible *scope* decision but a real *limitation*: any future change to a record layout silently corrupts existing data. There is no way to detect that `customers.dat` was written by a previous version with a different layout. A user who downloads v1.1 with a wider `email` field will load garbage from v1.0 files.
- **Fix:** A 16-byte file header per `.dat` file: magic ("ACCTPRO1"), version (uint16), record-size (uint16), reserved. Reject mismatches loudly.

### 26. `unsigned short int` typedef noise

- **Where:** every entity (e.g. `core/Customer.h:13, 24, 42`).
- **Failure mode:** Style nit, not a defect. `uint16_t` already exists and the rest of the storage layer uses it. Mixing the two is inconsistent and forces the reader to mentally check that they're the same width.
- **Fix:** Replace `unsigned short int` with `uint16_t` consistently.

### 27. `int` for `status`/`partyType`/`method` in the binary layout vs `enum` in C++

- **Where:** `core/Invoice.cpp:83`, `core/Payment.cpp:83, 87`.
- **Failure mode:** The binary layout pins these as `int` (typically 4 bytes on x86-64 GCC, but the standard doesn't guarantee). Building the project on a target with 2-byte `int` would silently change the on-disk format. Unlikely on modern desktop, but `int32_t` would be safer and self-documenting.
- **Fix:** Replace `int s = static_cast<int>(status); memcpy(..., &s, sizeof(s));` with explicit `int32_t`.

### 28. `loadAll()` returns by value, no move semantics or pagination

- **Where:** every repository.
- **Failure mode:** Returns `std::vector<Customer>` by value, requiring a full copy if the caller isn't using RVO/NRVO. Not a real issue with modern compilers, but ties the API to the assumption that the whole table fits in memory. There is no `findRange(offset, limit)`.
- **Fix:** Add a paginated query for very large tables. Combined with issue #6, this is academic until IDs widen.

### 29. `static const QRegularExpression numericTail` inside `suggestNextNumber`

- **Where:** `src/ui/pages/invoices/InvoicesPage.cpp:136`.
- **Failure mode:** Good — caches the regex. But the surrounding scan is O(n) over every row in the model on every call, just to compute the next sequence number. A counter persisted in `QSettings` (or — gasp — a database `MAX(id)`) would be O(1). Not painful at 100 rows; mildly painful at 10,000.
- **Fix:** Use the QSettings numbering counter that the Settings UI already pretends to manage (issue #16).

### 30. `CMakeLists.txt` sets `CMAKE_CXX_STANDARD 26`

- **Where:** `CMakeLists.txt:4`.
- **Failure mode:** C++26 is not yet a finalized standard as of 2026. The project compiles only because GCC 15's C++26 mode is a near-superset of C++23. Anyone building with an older toolchain (Qt's stock minGW is GCC 13) will fail at configure time. Pinning to a draft standard for no observable feature benefit is a portability cost with no gain.
- **Fix:** Drop to `CMAKE_CXX_STANDARD 20` unless a specific C++23/26 feature is in use; nothing reviewed required > C++17.

### 31. `core/budget.cpp` lowercase filename inconsistent with sibling `Budget.h`

- **Where:** `core/budget.cpp` vs `core/Budget.h`, also `Budget.cpp` in project root.
- **Failure mode:** On case-insensitive filesystems (Windows default, MSYS2) this works. On case-sensitive (Linux CI, macOS optionally) the inclusion `#include "Budget.h"` from a file called `budget.cpp` may or may not resolve depending on header-search order. The repo also has both `core/Budget.h` and a top-level `Budget.cpp` — duplicated state.
- **Fix:** Pick one location and case. Delete the orphaned root-level `Account*`, `Budget*`, `CashAccount*`, `CheckingAccount*`, `Sav*Account*` files now that `core/` is canonical, or move them all to `core/`.

### 32. `brf_test.dat` committed to the repo

- **Where:** project root.
- **Failure mode:** Binary artifact in source tree. Bloats the repo, will diff badly. Probably leftover from manual smoke-testing.
- **Fix:** `.gitignore` it; delete from the tree.

### 33. Compute-bound report runners allocate `new QStandardItem` per cell with no batching

- **Where:** `src/ui/pages/reports/ReportsPage.cpp:358-363, 391-397, 487-494`, etc.
- **Failure mode:** Each row in a report instantiates 5-7 `QStandardItem` objects. Memory allocator churn. Fine at 100 rows, mildly slow at 10k. The model is destroyed and replaced (`delete m_reportModel; m_reportModel = new ...`) at every Run-Report click — Qt has a `clear()` for this exact case to avoid teardown work.
- **Fix:** Reuse the model; just call `removeRows`.

### 34. The Reports "Group by" Customer option is silently downgraded to "Month" inside Tax/VAT/P&L/Cash Flow

- **Where:** `src/ui/pages/reports/ReportsPage.cpp:551-552, 588-589, 624-625, 669-670`.
- **Failure mode:** `m_groupCombo->currentText() == "Customer" ? "Month" : m_groupCombo->currentText()`. The user picks "Group by Customer," runs P&L, sees a Period-grouped report, and assumes their choice is broken or the data is wrong. There is no UI feedback that the choice was overridden.
- **Fix:** Either disable the "Customer" option when the selected report doesn't support it, or remove the silent downgrade and show "Period: not applicable."

### 35. Dashboard `onActivated` leaks the previous `QStandardItemModel`

- **Where:** `src/ui/pages/dashboard/DashboardPage.cpp:293, 327`.
- **Failure mode:** A new `QStandardItemModel` is created with `m_recentInvoices` as parent, set on the view. On subsequent activations, a *new* model is created with the same parent — the previous one stays alive because its parent (`m_recentInvoices`) is unchanged. They accumulate as the user navigates back and forth between Dashboard and other pages. Same in the overdue card. Slow leak, not a crash, but unbounded.
- **Fix:** `delete` the previous model before creating a new one, or reuse and `clear()`.

### 36. `QSettings` uses default organization+app — settings persist across uninstalls

- **Where:** `main.cpp:17-19`, `src/ui/pages/settings/SettingsPage.cpp:222-256`.
- **Failure mode:** `QSettings` defaults to the registry on Windows (`HKCU\Software\AccountingPro\AccountingPro`). Uninstalling the app does not remove these keys. Reinstalling later silently inherits prior preferences. For a real installer this becomes a support headache.
- **Fix:** Document the registry path, add a "Reset settings" button, ensure the installer can scrub it.

---

## Questionable Decisions

These are choices the author defends in `storage/ARCHITECTURE.md`; this section restates them under the "production" lens.

- **Custom binary format over SQLite.** Defensible as a course constraint, indefensible as a product choice. SQLite gives transactions, indexes, schema migrations, crash safety, concurrency, foreign keys, and is a single header drop-in. Every "blocking" issue above except #1 and #7 is either solved or trivialized by it.
- **Position-based IDs.** Cute, debug-friendly, but precludes compaction (issue #10), bounded at 65,535 (issue #6), and racy if the file is touched by anything else (issue #4). Surrogate IDs in a header would cost ~8 bytes per file.
- **Header-only repositories.** Increases compile times, makes ODR violations easy if repo code creeps beyond inline-friendly methods. Acceptable while they stay trivial; they will not stay trivial if real features are added.
- **Singleton `StorageService`.** Defended as Qt-idiomatic. Fine for now but makes unit testing the UI impossible — there's no way to inject a fake repository. There are no tests in the tree to confirm or deny this concern.

## What Will Break First?

In order of expected first failure under realistic use:

1. **First production user puts the data folder on a network share** — `std::fstream` works, no advisory locks, two users edit simultaneously, customers.dat is silently corrupted within an afternoon (issue #4).
2. **Antivirus locks `invoices.dat` during a save** — `append()` throws, UI doesn't catch, app crashes mid-edit, user loses the invoice they were entering (issue #1).
3. **User exports... nothing.** The Export button does nothing. Bug report filed within day one (issue #9).
4. **Tax inspector asks "show me all amendments to invoice INV-1024"** — no audit trail, the answer is "we can't" (issue #12).
5. **18 months in, ~10,000 invoices, the Reports tab takes 8 seconds to render** — every report does 2-3 full file scans on the UI thread (issues #10, #11, #18).
6. **At ~3 years, a Year-End P&L crashes** because someone created and voided enough draft invoices to pass 65,535 records (issue #6).
7. **First power cut while saving** — half-written record, on next launch one record's status byte is garbage, the invoice disappears from the list. The user is sure they entered it (issue #2).
8. **Customer with French Windows opens an invoice issued from English Windows** — date parses as invalid, invoice silently dropped from every report (issue #13).

## Final Verdict

**Reject for production. Approve as a portfolio / coursework artifact.**

This is a strong portfolio project: clean architecture, consistent code style, thoughtful separation of concerns, a polished Qt6 UI, and good intuition about layering. It demonstrates that the author understands C++ resource ownership, binary I/O, Qt model/view, and basic ERP domain concepts. It is *also* a long way from being software that holds a real business's books.

**Minimum work to close the gap to "small-business v1":**
1. Replace `BinaryRecordFile` with SQLite (1-2 weeks). Solves issues #2, #4, #6, #8, #10, #11 (partly), #18, #25, #28 at once.
2. Implement invoice line items + product linkage (1 week). Solves issue #7 and makes Products and Reports meaningful.
3. Catch all `std::exception` from storage in the UI; show user-facing dialogs (1 day). Solves issue #1.
4. Wire CSV export on every list page + printable PDF invoice (3-5 days). Solves issue #9.
5. Add a backup-to-zip menu and warn at startup before first use (1 day). Solves issue #15.
6. Audit trail: append-only log per write (2-3 days). Solves issue #12 minimally.
7. Switch dates to ISO YYYY-MM-DD throughout (1 day). Solves issue #13.
8. `windeployqt` + Inno Setup installer (1 day). Solves issue #17.

Total: **~5-6 weeks of focused work** to reach a defensible v1 for a sole-trader user. Multi-user / network / multi-tenant deployment is a separate, much larger conversation. Until that work lands, do not put this in front of users with real money on the line.
