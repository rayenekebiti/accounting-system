# Production Readiness Review

**Project:** AccountingPro — Qt6/C++26 desktop accounting application
**Date:** 2026-06-07
**Scope:** Entire codebase EXCEPT `storage/` (excluded by request)
**Reviewer mode:** Senior Principal — adversarial, evidence-driven

---

## Summary

**Verdict: REJECT.**

The application compiles, runs, and demonstrates a coherent UI architecture. That is the entirety of what works. Beyond the surface, the codebase carries:

- **One unconditional crash** in `Account.cpp` when an Account-derived object is instantiated and used (no `Account.cpp::isValid`-like guard; `localtime` returning null is dereferenced).
- **Two trivially provable buffer-overflow / OOB-write bugs** in the legacy `BankAccount`/`SavingsAccount` chain that would cause memory corruption the moment the legacy account code path is exercised.
- **Multiple silent-truncation bugs** in dialog → entity flows where the form accepts more characters than the underlying `char[N]` can hold. The persisted record is silently shorter than what the user typed.
- **One use-after-destruction bug** in `CheckingAccount.cpp` where the constructor passes an uninitialised member (`bankName`) up to the base.
- A handful of **logic / contract violations** in Qt model code that will produce wrong UI behaviour the moment storage is wired up (uniqueness invariants, deletion semantics, index stale-on-sort).
- **Severe duplication** that turns every entity-level change into a multi-file edit.
- **No persistence layer** (acknowledged), but the UI side commits to no-op behaviours that *look* like they persist data — Save buttons return `true`, dialogs report success — making it possible to ship a build that loses every change without surfacing an error.

Severity distribution:

| Severity         | Count |
|------------------|-------|
| Critical (block) | 7     |
| Major            | 19    |
| Minor            | 14    |
| Questionable     | 8     |

The known issues listed in the brief are real; they are also each at least one severity tier worse than the brief implied. Detailed below with file + line.

---

## Critical Issues

### C-1. `Account` constructor dereferences a possibly-null `localtime` pointer and ignores the result of `strftime`. `bytesWritten` is unused; compiler warning suppressed.

**File:** `Account.cpp:11-19`

```cpp
std::time_t now = std::time(nullptr);
std::tm* localtime = std::localtime(&now);           // can return nullptr
size_t bytesWritten = std::strftime(
        this->createdAt,
        sizeof(this->createdAt),                      // sizeof = 12
        "%Y-%m-%d %H:%M:%S",                          // requires 20 chars + NUL
        localtime);                                   // ← passed possibly null
```

Two compounded failures:
1. `std::localtime` may return `nullptr` (per POSIX and the C standard) — Microsoft's CRT does on `time_t < 0` or shutdown. Passing the null pointer to `strftime` is undefined behaviour. On MSYS2/MSVCRT it crashes.
2. The format `"%Y-%m-%d %H:%M:%S"` produces a 19-character output plus NUL — 20 chars. `MAX_ACCOUNT_CREATION_DATE_LENGTH = 12`. `strftime` returns 0 (failure) and the buffer state is unspecified. `bytesWritten` is unused; no check.

`createdAt` is left containing whatever junk was there before (`Account.cpp:11` doesn't `memset` the buffer to zero), so the field is not even null-terminated. Subsequent serialization would write garbage.

**Fix:** Zero the buffer, get the `std::tm` by value, and use a format that fits.

```cpp
std::memset(createdAt, 0, sizeof(createdAt));
std::time_t now = std::time(nullptr);
std::tm  local{};
#if defined(_WIN32)
if (localtime_s(&local, &now) != 0) return;          // bail; createdAt stays zero
#else
if (!localtime_r(&now, &local)) return;
#endif
// "%Y-%m-%d" = 10 chars + NUL = 11 — fits in 12.
std::strftime(createdAt, sizeof(createdAt), "%Y-%m-%d", &local);
```

---

### C-2. `BankAccount::accountNumber` declared as `unsigned short int[MAX_ACCOUNT_NUM_LENGTH]` — likely meant to be `char[]`. 40 bytes are reserved; nothing writes it; the constructor uses an uninitialised local named `bankName` instead of the parameter `bankNname`.

**File:** `BankAccount.h:11`, `CheckingAccount.cpp:5-6`

`BankAccount.h:11`:
```cpp
unsigned short int accountNumber[MAX_ACCOUNT_NUM_LENGTH];   // 20 * 2 = 40 bytes — NEVER assigned
```

`CheckingAccount.cpp:5-6`:
```cpp
ChekingAccount::ChekingAccount(unsigned short id, const std::string& nname,
                               double iBalance, const std::string& bankNname)
   :BankAccount(id,nname,iBalance,bankName){};   // ← uses base-class field `bankName`, NOT param `bankNname`
```

`bankName` here resolves to the **member** of `BankAccount`, which is at that point completely uninitialised — the base `BankAccount` constructor has not yet run; the derived initialiser is what calls it. Reading `bankName` here is UB. Even if the chain ran in the other order, you'd be passing the *raw `char[]` decayed to `const char*`* to a constructor expecting `const std::string&`, and `std::string` would be constructed from indeterminate bytes — usually a crash.

The intended parameter was `bankNname` (matching the misspelling in `BankAccount.cpp`).

This bug means `CheckingAccount` cannot be constructed safely. Every test, every call site, every use will exhibit indeterminate behaviour, likely a crash or memory corruption.

**Fix:**
1. Rename the parameter (and matching `BankAccount` parameter) — the `Nname` typo is gratuitous.
2. Pass the constructor argument, not the base member.
3. Make `accountNumber` a `char[MAX_ACCOUNT_NUM_LENGTH]` (or remove it if unused).

```cpp
// BankAccount.h
char accountNumber[MAX_ACCOUNT_NUM_LENGTH];

// CheckingAccount.cpp
CheckingAccount::CheckingAccount(unsigned short id, const std::string& name,
                                 double initialBalance, const std::string& bankName)
   : BankAccount(id, name, initialBalance, bankName)
{}
```

(Also rename the class file: `ChekingAccount` typo throughout.)

---

### C-3. `SavingsAccount::withdraw` calls `canWithdraw` *without arguments*, treating a pointer-to-member-function as a bool.

**File:** `SavingAccount.cpp:9-12`

```cpp
void SavingsAccount::withdraw(double money){
    if(canWithdraw){               // ← bug: address of member function, always truthy
        balance -= money;
        withdrawalsThisMonth += money;
    }
```

`canWithdraw` without parens is a member function pointer in C++. Taking its boolean value is always `true`. The withdrawal branch *always* executes regardless of balance or monthly limit; the `else if`/`else` clauses below it are dead code, and *no overflow check is performed.* Balance goes negative, withdrawals exceed the limit silently.

This bug means `SavingsAccount` cannot enforce any of the rules described in its header.

**Fix:**

```cpp
if (canWithdraw(money)) {
    balance -= money;
    withdrawalsThisMonth += money;
} else if (withdrawalsThisMonth > MAX_WITHDRAWAL_LIMIT) {
    throw std::out_of_range("You have exceeded your monthly withdrawal limit.");
} else {
    throw std::out_of_range("The amount entered is beyond your actual balance.\n");
}
```

Also note `withdrawalsThisMonth` is declared but never initialised in any constructor — `SavingAccount.cpp:5-8`. On read it contains indeterminate bytes; the comparison in `canWithdraw` against `MAX_WITHDRAWAL_LIMIT` is therefore non-deterministic. Initialise it in the constructor.

---

### C-4. `CheckingAccount::canWithdraw` ignores `balance` entirely; allows unlimited overdraft.

**File:** `CheckingAccount.cpp:8-11`

```cpp
bool ChekingAccount::canWithdraw(double money){
    if (money >= MAX_OVERDRAFT_LIMIT){ return true; }   // MAX_OVERDRAFT_LIMIT == -20000
    else return false;
}
```

`MAX_OVERDRAFT_LIMIT` is `-20000` (`constants.h:20`). A withdrawal of any positive amount `money` is `>= -20000`, so `canWithdraw` returns `true` for any positive value. The balance is never inspected. Account can be drawn arbitrarily negative.

(This is the known-issue flagged in the brief — but the brief understated it. It is not just "inverted logic"; it is *no logic*. The constant `MAX_OVERDRAFT_LIMIT` is also a `unsigned short int` set to `-20000`, which wraps to `45536` at translation. See QD-1.)

**Fix:** Compare projected balance against the overdraft limit.

```cpp
bool CheckingAccount::canWithdraw(double amount){
    return (balance - amount) >= overdraftLimit;     // overdraftLimit must be a signed type
}
```

Also fix `MAX_OVERDRAFT_LIMIT`'s type — `constants.h:20` declares `const unsigned short int MAX_OVERDRAFT_LIMIT = -20000;` which is unsigned-wrapped to `0xB1E0` (45536). Change to `constexpr double MAX_OVERDRAFT_LIMIT = -20000.0;`.

---

### C-5. `CategoryReport::categoryTotals` is a *namespace-scope* `std::vector` initialised at static-init time, mutated by every report instance, never cleared.

**File:** `CategoryReport.cpp:9`

```cpp
std::vector<double> categoryTotals(100, 0.0);   // ← global variable, not a member
```

```cpp
void CategoryReport::generate(){
    for (const auto& transaction : transactions) {
        unsigned short categoryId = transaction->getCategoryId();
        if (categoryId < categoryTotals.size()) {
            categoryTotals[categoryId] += transaction->getAmount();    // mutation persists across reports
        }
    }
}
```

Symptoms:
1. Two `CategoryReport` instances generated in sequence produce **cumulative** totals — the second includes the first's transactions.
2. The vector lives at namespace scope so its constructor runs during static init. If another translation unit's static constructor calls `CategoryReport::generate` before this TU's `categoryTotals` is constructed, that's static-init-order UB.
3. Hard-coded capacity 100 silently drops every category with id >= 100. No warning.
4. Not thread-safe — even though the project is single-threaded, this is a bear trap for whoever introduces a worker thread.
5. The class header (`CategoryReport.h`) doesn't declare it as a member, so the symbol is invisible to clients reading the header.

**Fix:** Make it a member, clear it on `generate`, drop the hard-coded 100, use a `std::unordered_map<unsigned short, double>`.

```cpp
// CategoryReport.h
private:
    std::unordered_map<unsigned short, double> categoryTotals;

// CategoryReport.cpp
void CategoryReport::generate(){
    categoryTotals.clear();
    for (const auto& transaction : transactions)
        categoryTotals[transaction->getCategoryId()] += transaction->getAmount();
}
```

(Note: `CategoryReport.cpp:5-8` also iterates `transactions` to call `addTransaction(transaction.get())` — but the `MonthlyReport` ctor stores them in a *separate* `std::vector<std::shared_ptr<Transaction>>` (`MonthlyReport.h:18` shadows `Report::transactions`). Pick one container and use it consistently.)

---

### C-6. `Report.h` `#include "transaction.cpp"` (the implementation file, not the header).

**File:** `Report.h:9`

```cpp
#include "transaction.cpp"   // ← .cpp, not .h
```

Symptoms:
- Including a `.cpp` file pulls every translation unit that includes `Report.h` into a redefinition of `Transaction::getId`, `getDescription`, etc., causing linker errors. The only reason this hasn't broken the build is that `Report.{h,cpp}` and `CategoryReport.{h,cpp}` are not in `CMakeLists.txt`'s source list (`CMakeLists.txt:13-22`) — they're orphans. The class hierarchy is dead code.
- If someone ever adds them to the build, the link breaks immediately. If they fix the link by removing the include, the source files won't compile because `Report::transactions` is `std::vector<Transaction*>` and `Transaction` is incomplete.
- `transaction.cpp` is also at a different path than what `#include` would resolve to (`transaction/transaction.cpp`).

**Fix:**

```cpp
// Report.h:9
#include "transaction/transaction.h"
```

Also include `<memory>` since `MonthlyReport.h:18` uses `std::shared_ptr`. And add the source files to `CMakeLists.txt` — or delete them as known-dead.

---

### C-7. `Customer`, `Supplier`, `Product`, `Invoice`, `Payment` `setX` setters skip validation; a fully constructed entity can be transitioned into a state where `isValid()` returns false, then `serialize()` will throw.

**File:** `core/Customer.cpp:98`, `core/Supplier.cpp:97`, `core/Product.cpp:104`, `core/Invoice.cpp:134`, `core/Payment.cpp:140`

```cpp
void Customer::setName(const char* newName){ copyField(name, CUSTOMER_NAME_LENGTH, newName); }
```

`copyField` happily accepts `nullptr` or empty string and stores it. Now `isValid()` returns false, and the next `serialize()` call throws `std::logic_error`. The exception propagates to whoever called the setter much later, in code that has nothing to do with the malformed input.

This is the classic "invalid object" anti-pattern. The constructor *does* validate; the setters do not. Once invalid, the object is stuck in invalid until the caller notices and reconstructs it.

**Fix:** Setters that bypass validation should not exist on these classes. Either:
1. Remove the setters and provide a single `update(const XxxData&)` that re-validates, or
2. Make every setter either reject the change or throw on invalid input. Match the constructor's contract.

The same pattern affects `Invoice` (number can be cleared), `Payment` (number can be cleared, amount can be reduced via `setAmount` — which actually does reject negatives — but the others don't).

Pick one path and apply it uniformly. Code that says "we validate in the ctor" but then provides 11 setters that don't is worse than not validating at all — it's a contract you can't trust.

---

## Major Issues

### M-1. Silent truncation in every editor dialog — user enters more chars than the `char[N]` field holds; entity stores a truncated copy with no error.

**Files:**
- `src/ui/dialogs/CustomerEditorDialog.cpp:104-146`
- `src/ui/dialogs/SupplierEditorDialog.cpp:103-145`
- `src/ui/dialogs/ProductEditorDialog.cpp:118-152`
- `src/ui/dialogs/InvoiceEditorDialog.cpp:172-221`
- `src/ui/dialogs/PaymentEditorDialog.cpp:142-186`

Example: `CustomerEditorDialog`. The `name` field accepts unlimited UTF-8 (no `QLineEdit::setMaxLength` set). `Customer::Customer(CustomerData{})` → `copyField` (`Customer.cpp:7-13`) → `strncpy(dest, src, capacity-1)` → null-terminates at `capacity-1`. A 32-byte buffer (`CUSTOMER_NAME_LENGTH = 32`) holds 31 chars plus NUL. Enter "Acme International Industrial Sales Pty Ltd" (43 chars), and the saved record stores "Acme International Industrial S" silently.

Same in every other dialog. Email allows 48 (47 usable). Phone 16 (15). UTF-8 worse — a 31-byte ASCII string is 31 chars; a 31-byte UTF-8 string with multi-byte chars cuts mid-codepoint and you end up with a non-UTF-8 sequence in the record. Subsequent `QString::fromUtf8` on read produces a replacement character.

**Fix:** Apply `setMaxLength(N - 1)` to every `QLineEdit` in every dialog so the user *cannot* exceed the field. Use the constants from the entity header, not magic numbers.

```cpp
// CustomerEditorDialog::buildUi
m_nameEdit->setMaxLength(CUSTOMER_NAME_LENGTH - 1);
m_emailEdit->setMaxLength(CUSTOMER_EMAIL_LENGTH - 1);
m_phoneEdit->setMaxLength(CUSTOMER_PHONE_LENGTH - 1);
m_taxEdit->setMaxLength(CUSTOMER_TAX_LENGTH - 1);
```

For UTF-8 safety, also clip on the *byte* count of the encoded form, not the character count. The cleanest fix is to validate in `accept()` and refuse to accept if `toUtf8().size() >= N`.

---

### M-2. `Customer`/`Supplier`/`Product`/`Invoice`/`Payment` validating constructors throw `std::invalid_argument`; dialogs catch the exception and *show the error message on the wrong row*.

**File:** `src/ui/dialogs/CustomerEditorDialog.cpp:140-142` (and the four siblings)

```cpp
} catch (const std::exception& e) {
    m_nameRow->setError(QString::fromUtf8(e.what()));    // always name row
    return;
}
```

If the user enters a malformed email and the *next* validation step throws (e.g. a future validator throws from `setEmail`), the error appears under the **name** field. UX: confusing. Worse: the same handler is used for all entities, so all five dialogs share the same defect.

In `PaymentEditorDialog::accept` (`PaymentEditorDialog.cpp:180-182`) the catch puts the error on `m_numberRow` regardless of whether the failure was the party type, the method, or the amount.

**Fix:** Match the exception's `what()` to the offending field and show the error there, or — better — validate explicitly in the dialog *before* construction so the dialog knows which row to attribute the error to.

---

### M-3. `Invoice` allows `total != subtotal + taxAmount` to be persisted. Dialog auto-recomputes total from sub + tax, but the entity validator does not enforce this invariant.

**File:** `core/Invoice.cpp:51-56`, `src/ui/dialogs/InvoiceEditorDialog.cpp:112-115`

```cpp
bool Invoice::isValid() const {
    return invoiceNumber[0] != '\0'
        && isKnownStatus(status)
        && subtotal >= 0 && taxAmount >= 0 && total >= 0;
}
```

No constraint that `total == subtotal + taxAmount` (within floating-point tolerance). A storage path that bypasses the dialog (bulk import, repository test code) can persist an invoice with totals that don't add up. This is an accounting application — a misaligned total is a financial-integrity defect.

**Fix:** Either:
1. Derive `total` from `subtotal + taxAmount` (don't store it separately), or
2. Enforce `std::abs(total - (subtotal + taxAmount)) < 0.005` in `isValid()`.

Option 1 is cleaner. The serialised record is then 8 bytes smaller; on-disk size doesn't matter, but you eliminate an entire class of bugs.

---

### M-4. `Invoice::setStatus(INVOICE_STATUS_UNKNOWN)` silently no-ops; caller has no signal that the call failed.

**File:** `core/Invoice.cpp:168-171`

```cpp
void Invoice::setStatus(InvoiceStatus newStatus){
    if (!isKnownStatus(newStatus)) return;     // silent reject
    status = newStatus;
}
```

Same pattern in `Payment::setPartyType`, `Payment::setMethod`, `Product::setPrice/setCost`, `Invoice::setSubtotal/setTaxAmount/setTotal`, `Customer::setX (no-op on null)`. A return value of `bool` would communicate success; the current API hides invalid input and the caller doesn't know.

**Fix:** Return `bool` from validating setters, or throw `std::invalid_argument`. Caller code can then act on the failure.

---

### M-5. `CustomerTableModel::removeRow` (and four siblings) marks rows deleted but doesn't notify the proxy/view that the row's status changed; sort/filter on `ColStatus` may show stale "Active" badges.

**File:** `src/ui/models/CustomerTableModel.cpp:31-36`

```cpp
void CustomerTableModel::removeRow(int row)
{
    if (row < 0 || row >= static_cast<int>(rows_.size())) return;
    rows_[row].setIsDeleted(true);
    emit dataChanged(index(row, 0), index(row, ColCount - 1));
}
```

`dataChanged` does fire for the full row range — so the view repaints. But:
1. The filter proxy compares the cell's displayed text ("Active"/"Inactive") in `CustomerFilterProxy::filterAcceptsRow`. After `dataChanged`, `QSortFilterProxyModel` invalidates filter only for the changed range. If the user had "Active" filter selected, the deleted row stays visible until the next `invalidate()` or `setFilterRole` change. The proxy *does* re-evaluate `filterAcceptsRow` on `dataChanged` since Qt 5, but only for rows in the changed range — that's correct in this case. However, the sort isn't re-applied, so a deleted row stays in its old sorted position. Confusing.
2. The function is named `removeRow` — Qt's `QAbstractItemModel::removeRow` is a virtual that removes rows. Overriding the name without overriding the contract is confusing.
3. No `live` view of the table — the model still reports `rowCount() == rows_.size()`. `liveCount()` is a separate method that the pagination uses (`InvoicesPage.cpp:150` uses `liveCount`, but `CustomersPage.cpp:104` uses `m_proxy->rowCount()`). Inconsistent — pagination reflects different numbers across pages.

**Fix:**
1. Rename `removeRow` → `softDelete(int row)`.
2. Emit `dataChanged` with `Qt::DisplayRole` and `Qt::EditRole` roles list so the proxy invalidates cleanly.
3. Standardise on `liveCount()` (excluding soft-deleted) for pagination across all five pages.

---

### M-6. `CustomersPage::onSearch` rebuilds the regex on *every* keystroke even though `SearchBar` debounces input. `setFilterRegularExpression` triggers full proxy re-evaluation (O(N) per char of source data) even on debounced calls.

**File:** `src/ui/pages/customers/CustomersPage.cpp:92-98` and siblings

The debounce is a 300 ms timer (`SearchBar.cpp:29`), so each settled keystroke triggers `setFilterRegularExpression`. For 8 test rows this is fine. For 50k customers the UI will hitch on each keystroke. The filter also calls `m_pagination->setTotalRecords(m_proxy->rowCount())` immediately — `rowCount` after a filter change forces the proxy to fully build its mapping.

This is a scaling time bomb. The codebase claims "fine for sub-100k records" — at 100k records, each keystroke would block the UI for hundreds of ms.

**Fix:** Wrap the search update in `invalidateRowsFilter()` and run the count update asynchronously, or use `QSortFilterProxyModel::setFilterFixedString` for plain text (which is much faster than regex). For sub-100k records this is a *latent* major issue, not blocking, but it will be hit the day real data lands.

---

### M-7. `*FilterProxy::setXFilter` calls `invalidate()` — heavier than `invalidateRowsFilter()`. `Products` and `Payments` call `invalidate()` twice per filter combo change (once per filter).

**File:** `src/ui/pages/products/ProductsPage.cpp:83-89`, `payments/PaymentsPage.cpp:81-87`

```cpp
void ProductsPage::rebuildFilter()
{
    auto* proxy = static_cast<ProductFilterProxy*>(m_proxy);
    proxy->setCategoryFilter(m_filterBar->filterValue(0));   // invalidate #1
    proxy->setStockFilter(m_filterBar->filterValue(1));      // invalidate #2
    m_pagination->setTotalRecords(m_proxy->rowCount());
}
```

`invalidate()` resets *sorting* too. With sorting enabled (`DataTableView.cpp:117`), every filter change blows away the user's chosen sort column. The user picks "Name ascending", then changes a filter, then loses their sort.

**Fix:** Add `setFilters(QString cat, QString stock)` to the proxy that sets both fields and calls `invalidateRowsFilter()` once. Same fix for `PaymentsPage`.

---

### M-8. `InvoicesPage` doesn't connect `m_filterBar`'s `filterChanged` signal to anything. The "Customer" filter combo is wired into the bar but the slot is missing.

**File:** `src/ui/pages/invoices/InvoicesPage.cpp:41-69`

No call site for any `rebuildFilter` slot in `InvoicesPage`. The filter combo silently does nothing.

The brief calls this "known issue: Customer filter combo has no options and no behavior." That's an understatement — even if the brief lists "no options", the filter *bar* itself is connected to no slot in `InvoicesPage`, so adding options wouldn't change behaviour either.

**Fix:**
1. Populate the customer filter combo from the customer list (or hide it until the customer repository exists).
2. Connect `m_filterBar->filterChanged` to `InvoicesPage::rebuildFilter` and implement.

```cpp
connect(m_filterBar, &FilterBar::filterChanged, this, &InvoicesPage::rebuildFilter);
```

---

### M-9. `*FilterProxy::filterAcceptsRow` calls `QSortFilterProxyModel::filterAcceptsRow` (the base) as the final step, which means search-text filter is OR-ed *outside* the status filter. If the user has Status=Active and types "acme", a row that's Inactive but matches "acme" by name still gets the Active check applied — works correctly here because the status filter is checked first and returns false-or-continues. But the contract is fragile: if a future change reorders the checks, the search filter and status filter become semantically tangled.

**File:** all five `*FilterProxy::filterAcceptsRow` overrides.

Specifically: the proxies are also using `m_status.isEmpty()` to mean "no filter", but the filter bar inserts "All" as index 0 and `FilterBar::filterValue` returns empty when index is 0 (`FilterBar.cpp:32-34`). That works *now*, but the "Active"/"Inactive" filter on `CustomersPage` adds an empty string to the model: `m_filterBar->addFilter("Status", {"", "Active", "Inactive"});` (`CustomersPage.cpp:43`). The empty string is now item *1* in the combo. If the user picks "" (item 1), `filterValue` returns "" (because it's not the All item, but the string is empty), and the status filter is effectively disabled.

By contrast, `SuppliersPage.cpp:39` passes `{"Active","Inactive"}` (no empty string) — so the lists are not aligned across pages. Different pages will behave differently for the same UI gesture.

**Fix:**
1. Drop the empty string from `CustomersPage`'s filter list.
2. Move the search text filter into the proxy explicitly so the conjunction semantics are obvious.

```cpp
m_filterBar->addFilter("Status", {"Active", "Inactive"});    // no empty string
```

---

### M-10. `CustomersPage::onSearch` is defined in the class header as a private slot AND is named `onSearch` which **shadows** `ListPage::onSearch` — but `ListPage::onSearch` is the slot connected from the base's `setupListLayout`. When `setupListLayout()` runs at `CustomersPage.cpp:74`, the connection in `ListPage.cpp:33` connects the base's virtual `onSearch`, not the derived slot.

**File:** `src/ui/pages/base/ListPage.cpp:33`, `src/ui/pages/customers/CustomersPage.h:21`, `src/ui/pages/customers/CustomersPage.cpp:63-64`

```cpp
// ListPage.cpp:33
connect(m_searchBar, &SearchBar::searchChanged, this, &ListPage::onSearch);
```

`ListPage::onSearch` is a virtual `protected` function (not a slot). When the derived overrides it, Qt's signal/slot system will resolve to the derived implementation via vtable — that works.

**But:** `CustomersPage.cpp:63-64` *also* explicitly connects:

```cpp
connect(m_searchBar, &SearchBar::searchChanged,
        this, &CustomersPage::onSearch);
```

— pointing at the *private slot* `CustomersPage::onSearch(const QString&)`. So `m_searchBar->searchChanged` now fires the search handler **twice** per emit: once through the virtual override (called via base's connection), once through the explicit override (the duplicate connection here). The proxy's filter is set twice per keystroke; pagination is updated twice.

Same defect in `SuppliersPage.cpp:58`, `ProductsPage.cpp:68`, `InvoicesPage.cpp:62`, `PaymentsPage.cpp:66` — every page that overrides `onSearch` and re-connects gets a double-fire.

This is hidden by `QSortFilterProxyModel`'s caching, but it doubles the work per keystroke and may cause spurious `layoutChanged`-driven flickers.

**Fix:** Either:
1. Remove the explicit re-connect in each derived page (rely on the virtual override).
2. Disconnect the base's connection before re-connecting in derived class.

Option 1 is cleaner. Drop the explicit `connect(m_searchBar, &SearchBar::searchChanged, this, &CustomersPage::onSearch);` lines.

---

### M-11. `ListPage::onSearch(const QString&)` is declared `protected virtual` with `Q_UNUSED(text)`. It's not a `Q_SLOT`. Connecting a signal to a non-slot member is allowed in Qt5+, but the function won't appear in `meta-object` introspection. Combined with M-10, the dispatch is brittle — anyone refactoring may break the chain.

**File:** `src/ui/pages/base/ListPage.h:27-30`

```cpp
virtual void onSearch(const QString& text) { Q_UNUSED(text) }
virtual void onFilterChanged()             {}
virtual void onPageChanged(int page)       { Q_UNUSED(page) }
virtual void onRowDoubleClicked(int row)   { Q_UNUSED(row) }
```

These are virtuals with no `Q_SLOT` markup. They work *because* Qt5's new connect syntax doesn't require slot markers. But anyone who tries to do `QMetaObject::invokeMethod(page, "onSearch", ...)` will fail with "no such method". The codebase relies on a particular dispatch shape that isn't expressed in the type system.

**Fix:** Either declare them as `Q_SLOT` (for introspection), or rename them (`handleSearch`, `handleRowDoubleClicked`) to make clear they're virtual hooks, not slots.

---

### M-12. `DashboardPage` creates a `QStandardItemModel` parented to a child widget; never sets the existing model's parent or deletes the prior model. Calling `onActivated()` more than once leaks a model.

**File:** `src/ui/pages/dashboard/DashboardPage.cpp:227-258`

```cpp
auto* rim = new QStandardItemModel(0, 6, m_recentInvoices);
// … populated …
m_recentInvoices->setModel(rim);
```

`QTableView::setModel` does not take ownership of the model — it just sets a reference. The parent of the model (`m_recentInvoices`) does keep it alive. So far, OK. But:
1. Calling `onActivated()` twice creates a second model parented to the same `QWidget*`. Both live until `m_recentInvoices` is destroyed. Memory accumulates with each navigation.
2. `m_recentInvoices` is a `DataTableView`, not a `QStandardItemModel*`. Parenting a child to a `QWidget` parent is fine; just be aware nothing reclaims the previous model.

`PageRouter::navigateTo` calls `onActivated()` every time you navigate to the dashboard. Every navigation creates two models. Leak grows linearly.

**Fix:** Build the models once in `DashboardPage`'s constructor and call `setData` on revisit, or `deleteLater()` the existing model before replacing it:

```cpp
if (auto* old = m_recentInvoices->tableView()->model())
    old->deleteLater();
```

---

### M-13. `Report.cpp::~Report()` calls `transactions.clear()` on a vector of raw `Transaction*`. The pointers are not owned by `Report`. Calling `clear` is a no-op except for emptying the vector — fine. But the inconsistency with `MonthlyReport` (which uses `std::vector<std::shared_ptr<Transaction>>` shadowing the base member) suggests the ownership model was never thought through.

**File:** `Report.cpp:3-5`, `MonthlyReport.h:18`

```cpp
Report::~Report(){
    transactions.clear();
}
```

`MonthlyReport.h:18`:
```cpp
std::vector<std::shared_ptr<Transaction>> transactions;   // shadows Report::transactions
```

`MonthlyReport::generate` iterates `transactions` — *which one?* C++ resolves to the most-derived: `MonthlyReport::transactions`, the `shared_ptr` vector. The base's `transactions` (raw `Transaction*`) is never populated, never iterated, never cleared meaningfully.

If anyone ever calls `Report::addTransaction(Transaction*)` on a `MonthlyReport*` (cast to base), it adds to the *base* vector which is unused. The transaction is silently lost.

(Same defect in `CategoryReport.cpp:5-7` which calls `addTransaction(transaction.get())` — adding to the base. Then `generate` iterates `transactions` which resolves to the *base* in `CategoryReport` (since it doesn't shadow), but iterates the *derived* in `MonthlyReport`. Inconsistent.)

**Fix:** Pick one ownership model:
1. `Report` owns `std::vector<std::shared_ptr<Transaction>>`. Don't shadow in derived classes.
2. `Report` borrows via raw pointers (current design). Derived classes inherit the base member; don't shadow.

---

### M-14. `MonthlyReport::display` computes "Net Balance" as `totalIncome + totalExpenses`. But `ExpenseTransaction::ExpenseTransaction` (`ExpenseTransaction.cpp:13`) sets `amount = -info.amount`. So expenses are already negative. In `MonthlyReport::generate` (`MonthlyReport.cpp:9-15`):

```cpp
for (const auto& transaction : transactions) {
    if (transaction->getAmount() > 0) {
        totalIncome += transaction->getAmount();
    } else {
        totalExpenses += transaction->getAmount();    // adds a negative number
    }
}
```

`totalExpenses` is the sum of negative numbers — i.e., a negative value. Then `display` prints "Net Balance: totalIncome + totalExpenses" — which is correct (income + negative-expenses) — but the label "Total Expenses" is misleading: a user expects the absolute value. Also, `display` prints negative totals for expenses.

**Fix:** Either:
1. Use `std::abs(totalExpenses)` for display and report `Net = totalIncome - std::abs(totalExpenses)`.
2. Use `getEffectiveAmount` (which is the polymorphic API) instead of `getAmount`. They happen to return the same value here, but if `getEffectiveAmount` ever differs (for recurring transactions, you might want a per-period contribution), the math breaks.

Also: `MonthlyReport::exportToFile` is a TODO comment (`MonthlyReport.cpp:24`). Known issue.

---

### M-15. `RecurringExpense::isDueToday` and `RecurringIncome::isDueToday` use `localtime`, `mktime`, `sscanf` — none of which are checked for failure.

**File:** `transaction/RecurringExpense.cpp:16-33`, `transaction/RecurringIncome.cpp:16-33`

```cpp
bool RecurringExpense::isDueToday() const
{
    time_t now = time(nullptr);
    struct tm* todayTm = localtime(&now);              // may return nullptr
    todayTm->tm_hour = 0; ...                          // ← UB if null
    time_t todayMidnight = mktime(todayTm);

    int sy, sm, sd;
    std::sscanf(date, "%d-%d-%d", &sy, &sm, &sd);      // ← assumes ISO format; date is "dd MMM yyyy" elsewhere
    struct tm startTm = {};
    startTm.tm_year = sy - 1900;
    ...
```

Issues:
1. `localtime` may return nullptr. Dereferencing is UB.
2. `sscanf` is unchecked. If `date` is `"01 Jun 2026"` (the actual format used by Invoice and Payment dialogs — see `InvoiceEditorDialog.cpp:32`), `sscanf("%d-%d-%d", ...)` fails on the space, leaving `sy/sm/sd` uninitialised. `mktime` with garbage values returns -1 or a wild value. `diffDays` is garbage. `isDueToday` returns random bool.
3. `frequencyDays == 0` (the default for `RecurringTransactionData` if not initialised by caller) divides by zero (`diffDays % 0` is UB).

This entire class is fragile. It's only safe to call if the date is ISO ("YYYY-MM-DD") and `frequencyDays > 0` — neither of which is checked anywhere.

**Fix:** Validate `date` format and `frequencyDays > 0` in the constructor; use `localtime_s`/`localtime_r`; check `sscanf`'s return value.

---

### M-16. `RecurringExpense::serialize` and `RecurringIncome::serialize` write `frequencyDays` to offset 93 in a buffer of size `TRANSACTION_RECORD_SIZE = 128`. Then `endDate[MAX_END_DATE_LENGTH=12]` to offset 97. `97 + 12 = 109` — fits in 128. But the *base* `ExpenseTransaction::serialize` wrote `isDeleted` to offset 92. So `frequencyDays` at offset 93 overlaps the byte after `isDeleted` but the base ctor only wrote a `bool` (1 byte). Spotted: derived offsets are correct now. **But** there is no compile-time enforcement; if the base layout changes (e.g. status moves), the derived class silently corrupts data.

**File:** `transaction/RecurringExpense.cpp:51-56`, `transaction/RecurringIncome.cpp:51-56`

This is the same hazard called out in the agent-memory: hand-rolled offsets with no compile-time relation between them. The fix is to derive offsets from `sizeof` and `offsetof` constants in a header.

---

### M-17. `Budget`'s `monthlyLimit` field is `const unsigned short` in the orphan `Budget.h/Budget.cpp` at the repo root, but `double monthlyLimit` in the active `core/Budget.h`. The `unsigned short` version cannot hold values above 65535 — useless for any real budget. The orphan files exist on disk and could shadow the real ones if include order changes.

**File:** `Budget.h:16` (root, orphan), `core/Budget.h:19` (active)

```cpp
// root/Budget.h:16 — UNUSED, but in the repo
const unsigned short monthlyLimit;   // const, AND too small
```

```cpp
// core/Budget.h:19 — USED
double monthlyLimit;
```

Same name, different fields, different types, different files, no include guard collision because the root version is `BUDGET_H` and the core version is `CORE_BUDGET_H`. They will coexist.

**Fix:** Delete the root `Budget.{h,cpp}`. They are dead code that confuses readers.

---

### M-18. `PaymentsPage`'s "Party" filter compares using `QString::startsWith(m_party)`. The `ColParty` display string is `"Customer #1001"` etc. If the user picks "Customer" the filter accepts both `"Customer #1001"` and any hypothetical `"CustomerSupplier"` if such a string ever existed. The filter is therefore prefix-based — vulnerable if filter labels change to `"Cust"` vs `"Customer"` etc.

**File:** `src/ui/pages/payments/PaymentsPage.cpp:30-35`

```cpp
if (!m_party.isEmpty()) {
    const QString p = sourceModel()
        ->index(row, PaymentTableModel::ColParty, parent).data().toString();
    if (!p.startsWith(m_party)) return false;
}
```

Also: model's display string is constructed via `QString::asprintf("%s #%u", partyName(...), ...)` (`PaymentTableModel.cpp:80`). The party labels are "Customer"/"Supplier". If the filter combo is later localised to "Cliente"/"Proveedor" but the model's `partyName` still returns English, the filter silently matches nothing.

**Fix:** Filter against the enum, not the string. Add a `UserRole` data returning the `PartyType` from the model, and the proxy compares enum values.

---

### M-19. `PaginationFooter` emits `pageRequested` from `prevPage`/`nextPage` but no page subscribes. The buttons appear active but click does nothing visible (other than update the `0-25 of 8` label, which counts off the source rows). Combined with M-1 (truncation), the user could believe they're paginating but they're not.

**File:** `src/ui/components/tables/PaginationFooter.cpp:65-82`

```cpp
void PaginationFooter::nextPage(){
    const int pages = (m_totalRecords + m_pageSize - 1) / m_pageSize;
    if (m_currentPage < pages) {
        ++m_currentPage;
        updateLabels();
        emit pageRequested(m_currentPage);    // ← nobody listens
    }
}
```

`ListPage::onPageChanged(int page)` is the default-no-op virtual; no derived page overrides it.

The brief acknowledges "pagination is decorative" — but a *decorative pagination footer that the user thinks works* is worse than no pagination. Pressing "Next" shows "26-50 of 8" briefly, then nothing changes.

**Fix:** Hide the pagination footer until the storage layer is built and a row-range proxy is added. Or disable the prev/next buttons when no slot is connected.

---

## Minor Issues

### m-1. Class name typo `ChekingAccount` (missing `c`).
`CheckingAccount.h:4`, `CheckingAccount.cpp:5`. The filename is `CheckingAccount.{h,cpp}` but the class name lacks the second `c`. Rename the class.

### m-2. `Account::getId` and `Account::getBalance` are declared `virtual` (`Account.h:22-23`) but `CashAccount::getId` overrides without `override` keyword (`CashAccount.cpp:22-24`). Compiler warning. Adds the override keyword.

### m-3. `Account` has no virtual destructor.
`Account.h:5-26`. `Account` is an abstract base (pure virtual `getAccountType`, `canWithdraw`, `deposit`, `withdraw`) but has no virtual destructor. Any `delete (Account*)derived;` chain is UB. Add `virtual ~Account() = default;`.

### m-4. `Account` constructor doesn't zero `name` before `strncpy`. If `nname.size() >= MAX_NAME_LENGTH - 1`, `strncpy` doesn't null-terminate the buffer. The code does explicitly null-terminate at `MAX_NAME_LENGTH-1`, so this particular case is OK — but for cases where `nname.size() < MAX_NAME_LENGTH - 1`, the bytes after the null are whatever was on the stack. When serialised, garbage trailing bytes go to disk.

**Fix:** `Account.cpp:5-19` — `std::memset(name, 0, sizeof(name));` before `strncpy`.

### m-5. `Category::display`'s return type is `void const` — meaningless. `Category.h:22`, `Category.cpp:21`. `void const` is valid syntax for `const void` which means nothing for a non-pointer return. Remove the `const`.

### m-6. `Category` has no default constructor declared (`Category.h:11-27`), but `Category.cpp` only defines `getId/getName/getType/display`. There's no constructor at all. Default constructed `Category` has uninitialised `id`, `name`, `type` — reading those is UB. This *only* doesn't blow up because no one currently constructs a `Category` (this `Category` class at the root is orphan; the active one is `core/Category.{h,cpp}`).

### m-7. `Budget::isExceeded` returns `currentSpend > monthlyLimit` — `monthlyLimit` is `const unsigned short` in the orphan version. Implicit conversion of `double` to `unsigned short` truncates. Even in the *core* version (`core/Budget.h:19`), `currentSpend > monthlyLimit` is a `double > double` comparison — works — but the previous orphan version is bug-prone.

### m-8. `Customer::display`, `Supplier::display`, etc. write to `std::cout`. A GUI application has no console. On Windows MSYS2 with `WIN32_EXECUTABLE TRUE` (CMakeLists.txt:81), `std::cout` writes go to a dead handle. Calls succeed but no output appears. Worse, if a `freopen` redirects stdout to a file, every display call writes to that file. Replace with `qDebug()` or remove.

### m-9. `setSortIndicatorShown(false)` in `DataTableView.cpp:123` — sorting is enabled (`setSortingEnabled(true)` on line 117). User clicks a column header to sort but the indicator is hidden. They can't tell the sort changed. (Known issue from brief.)

### m-10. `CustomersPage` includes the empty string `""` as a filter option (`CustomersPage.cpp:43`). When user selects "" (item 1 below "All"), `filterValue` returns `""`, which is treated as "no filter". Confusing — user picks an option but nothing changes. (See M-9.)

### m-11. `SettingsPage` stores `m_themeCombo->currentIndex()` (`SettingsPage.cpp:303`) but also wires `currentTextChanged` to *apply* the theme immediately (`SettingsPage.cpp:196`). If user changes the theme but doesn't click Save, the *running* theme changes but is reverted on next launch. Inconsistent. Either apply on save (preferred for "Revert" to work meaningfully) or auto-save (then there is no dirty state for theme).

### m-12. `MainWindow::setupStatusBar` creates `m_statusCompany`, `m_statusPeriod`, etc. parented to `this` (MainWindow) but then adds them to the status bar via `sb->addPermanentWidget(lbl)`. `addPermanentWidget` reparents — so the `this` parent is redundant but harmless. Mention for clarity.

### m-13. `GlobalToolBar::newSupplierRequested` and `newProductRequested` actions are connected to nothing — the `Q_UNUSED(newSupplier) Q_UNUSED(newProduct)` line (`GlobalToolBar.cpp:39`) silences the warning. The menu items appear but do nothing. Hide them or wire them.

### m-14. `EmptyStateWidget`'s emoji `"📄"` is the only label set in `DataTableView::setEmptyMessage` (`DataTableView.cpp:212`). No `EmptyStateWidget::setIcon`. The icon is hardcoded for all empty states regardless of context.

---

## Known Issues (documented; flagged here with location)

These were called out in the brief. Each is included with file + line so they can be fixed in the same pass.

| # | Issue | File:Line | Severity |
|---|-------|-----------|----------|
| K-1 | `CheckingAccount::canWithdraw()` inverted | `CheckingAccount.cpp:8-11` | **Critical** — see C-4 |
| K-2 | `SavingsAccount` has header but no `.cpp` | `SavingAccount.cpp` exists, but `withdraw` is broken; class file naming inconsistent (`SavingsAccount.h` vs `SavingAccount.cpp`) | **Critical** — see C-3 |
| K-3 | `Account.cpp` empty/placeholder | `Account.cpp:1-26` — incomplete, lacks `~Account`, no `name` zero-fill | **Critical** — see C-1, m-3, m-4 |
| K-4 | `CategoryReport::categoryTotals` static at namespace scope | `CategoryReport.cpp:9` | **Critical** — see C-5 |
| K-5 | `MonthlyReport::exportToFile` is TODO stub | `MonthlyReport.cpp:24` | Major |
| K-6 | Sort indicator hidden | `DataTableView.cpp:123` | Minor — see m-9 |
| K-7 | Invoice "Customer" filter has no options and no behavior | `InvoicesPage.cpp:44`, no slot wired | Major — see M-8 |
| K-8 | `computeNextId()` overflows at id 65535 | `CustomersPage.cpp:121`, `SuppliersPage.cpp:106`, `ProductsPage.cpp:118`, `InvoicesPage.cpp:119`, `PaymentsPage.cpp:114` | Major |
| K-9 | `m_proxy` typed as base, cast to derived | five pages | Minor — see Questionable Decisions |
| K-10 | Filter proxies compare displayed text, not enums | five `*FilterProxy::filterAcceptsRow` | Major — see M-18 |

---

## Questionable Decisions

### QD-1. `MAX_OVERDRAFT_LIMIT` declared `const unsigned short int = -20000`. Type-wraps to `45536` (`0xB1E0`).

**File:** `constants.h:20`

```cpp
const unsigned short int MAX_OVERDRAFT_LIMIT     = -20000;
```

The intent is "overdraft can go down to -20000". The storage type is *unsigned*, so the literal `-20000` wraps. Any signed comparison against `MAX_OVERDRAFT_LIMIT` will see `45536`. `CheckingAccount.cpp:9` compares `money >= MAX_OVERDRAFT_LIMIT` — `money` is a `double`, `MAX_OVERDRAFT_LIMIT` promotes to `45536.0`, so any withdrawal below $45,536 is "allowed".

**Justification needed:** Why `unsigned short` for a signed quantity? Make it `constexpr double MAX_OVERDRAFT_LIMIT = -20000.0;`.

### QD-2. `MAX_WITHDRAWAL_LIMIT = 50000` is `unsigned short`. 50000 fits in 16 bits but barely. If a future business decision raises it to 80000, the field silently truncates.

**File:** `constants.h:19`

**Justification needed:** Why `unsigned short` for a currency quantity at all? Use `constexpr double`.

### QD-3. Mixing fixed-size `char[N]` records with `QString` everywhere in the UI layer.

`QString` is variable-width, supports full Unicode. `char[31]` truncates UTF-8 mid-codepoint silently. The architecture has chosen to use C strings *only at the persistence boundary* but the dialog layer constructs `QString`s, gets `.toUtf8()` bytes, then drops bytes silently into a truncating `strncpy`.

**Justification needed:** Was the binary record format chosen for compatibility with an existing FileManager? If so — document the constraint. If not, why not use length-prefixed `std::string` and a variable-record `BinaryRecordFile`? The fixed-size design forces every text field to have a hard maximum, and there's no codepath that *tells* the user when they've exceeded it.

### QD-4. Singleton `ThemeManager::instance()` with global state, mutated from inside `SettingsPage`. Not thread-safe (not needed today), but more importantly: changing the theme is a side effect of a UI control (the combo's `currentTextChanged`). If the user picks a theme and clicks Revert, the theme stays applied.

**File:** `src/ui/pages/settings/SettingsPage.cpp:196-199`

**Justification needed:** Why apply on combo-change instead of on Save? See m-11.

### QD-5. `DashboardPage::onActivated` rebuilds models every navigation.

**File:** `src/ui/pages/dashboard/DashboardPage.cpp:211-267`

**Justification needed:** If `onActivated` is meant to refresh data, then storing models as members is pointless. If it's a one-time setup, it shouldn't be in `onActivated`. Today it's called on every navigation and leaks. See M-12.

### QD-6. `ListPage` declares `m_table`, `m_searchBar`, `m_filterBar`, `m_pagination` as `protected` and creates them in the *constructor*, then has a *separate* `setupListLayout()` method that each derived class must call.

**File:** `src/ui/pages/base/ListPage.{h,cpp}`

Why two phases? If derived classes can choose not to call `setupListLayout`, then the widgets exist but aren't in any layout — they're invisible and leaked.

`CustomersPage.cpp:74` calls `setupListLayout()` at the end of its constructor — after `loadTestData()` and other setup. `InvoicesPage.cpp:65` calls it *and then* `insertWidget(0, m_statusTabs)` to inject extra UI. So the two-phase init is to let derived classes mutate the layout.

**Justification needed:** Why isn't this expressed as a `void addBefore(QWidget* w)`/`void addAfter(QWidget* w)` API on `ListPage`? The current pattern forces each derived to remember to call `setupListLayout()` — forgetting it crashes silently on first show.

### QD-7. `core/AccountingSystem` exists but is used by **nothing**.

**File:** `core/AccountingSystem.{h,cpp}`

It's a "coordinator" for Categories and Budgets, but no page uses it. Categories and Budgets aren't surfaced in the UI at all. The five wired entities (Customer, Supplier, Product, Invoice, Payment) each load test data directly into their `*TableModel`.

**Justification needed:** Why does this class exist? It's well-written but completely disconnected. If it's a vestigial design artefact, delete it. If it's planned to coordinate everything, then *why are Customer/Supplier/Product/Invoice/Payment not coordinated through it?*

### QD-8. Settings stored under `HKCU\Software\AccountingPro\AccountingPro` via `QSettings`.

Without an explicit `QSettings::setDefaultFormat(QSettings::IniFormat)`, this writes to the **registry** on Windows. The user has to use `regedit` to inspect or clear settings — and registry edits can't be backed up by a normal file copy.

**Justification needed:** For an accounting app, settings (company name, tax number, currency) are not user preferences — they are configuration that the user should be able to back up, share with their accountant, version-control. Registry storage works against that. Consider `QSettings::IniFormat` + a path under `%APPDATA%`.

---

## What Will Break First?

In order of likelihood under normal use:

1. **`CheckingAccount` instantiation.** Anyone exercising the legacy account code path will trip C-2 (uninitialised `bankName` parameter) and crash. The known-issue brief says "logic is inverted" — the reality is the class can't even be constructed safely.

2. **`SavingsAccount::withdraw`.** First call lets the user withdraw any amount, balance goes negative, no error. The next code path that reads `withdrawalsThisMonth` reads uninitialised memory (C-3). The bug is silent until someone checks the balance and finds it's $-50,000.

3. **Customer name with non-ASCII characters longer than ~10 chars.** The form accepts any length, `Customer.cpp::copyField` truncates at byte 31 — but a UTF-8 string can be cut mid-codepoint, producing invalid UTF-8. The next render of that name via `QString::fromUtf8` displays the replacement character. M-1 turns into a visible UX bug the moment a non-English user types in their name.

4. **Dashboard navigation leaks.** Every time the user clicks Dashboard, two more `QStandardItemModel`s are leaked (M-12). A power user navigating between pages 200 times in a session has 400 leaked models. Each holds ~6 `QStandardItem`s. Insignificant per leak; visible memory growth over hours.

5. **Filter combo + status change race.** User filters on `Status=Active`, then double-clicks a row to edit, then sets status to Inactive in the dialog. `m_model->updateRow(sourceRow, dlg.customer())` emits `dataChanged` (M-5). Proxy re-evaluates `filterAcceptsRow`. The row drops out of the visible set — but the selection model still points at the *proxy* index that no longer exists. Next keyboard interaction with the table view crashes or silently no-ops depending on Qt version.

6. **`CategoryReport` second run.** First report shows totals. User clicks Run again. Totals doubled (C-5). User assumes "the data got worse over time", spends an afternoon debugging the wrong layer.

7. **`computeNextId` at id 65535.** Currently the test data uses ids 1001-1008 (customers), 2001-2006 (suppliers), 1-7 (products), 1-8 (invoices), 1-6 (payments). The known-issue says "overflows silently at id 65535" — in production, the first user with a long-lived database hits this at year 5-10 depending on invoice volume. The next add starts overwriting existing records (because record offset is `id * recordSize`).

8. **`Invoice::total` desync.** Storage layer adds bulk import. Import populates `subtotal`, `taxAmount`, `total` independently. Importer has rounding error → `total != subtotal + tax`. `isValid()` passes. Persisted. UI shows wrong total on the receipt. Accounting reconciliation fails. M-3 surfaces only when storage lands.

---

## Final Verdict

**Reject.**

Top reasons (each on its own would warrant rejection):

1. **Memory corruption in legacy `CheckingAccount` / `SavingsAccount` constructors** (C-2, C-3). These classes cannot be instantiated safely. Even though the active UI doesn't use them, they are compiled into the binary and any caller that uses them — including future test code — triggers UB.

2. **Account dereferences a possibly-null pointer** (C-1) and writes more bytes than the field holds. Crashes on platforms where `localtime` returns null at construction; silently corrupts the `createdAt` field everywhere else.

3. **Silent string truncation across every editor dialog** (M-1). The UI accepts data the entity cannot hold; the entity silently truncates; the user never knows. For an accounting application that will store customer names, addresses, invoice descriptions, this is a data-integrity bug class.

4. **Filter proxies, search wiring, double-connect, and pagination footer** all silently produce wrong results or do nothing — and the codebase trusts them. The UI says "Active filter applied" or "Page 2 of 6" without it being true.

5. **Static / namespace-scope mutable state** in `CategoryReport` (C-5) and the orphan `Report.h` `#include "transaction.cpp"` (C-6) reveal a class hierarchy that was abandoned half-built and left in the source tree. Either remove it or fix it — leaving it as-is is a trap for the next maintainer.

6. **Setter contracts and constructor contracts disagree** (C-7). The constructor validates; the setters bypass validation. An object can be constructed valid, mutated invalid via a setter, then serialised — and the serialisation throws, far from the call site that made it invalid.

7. **`MAX_OVERDRAFT_LIMIT` declared `unsigned short = -20000`** (QD-1). This is the kind of bug that gives finance executives nightmares — a wrap-around in a quantity that controls how much money can be withdrawn.

This codebase is at the right shape for a v0.6 prototype — the architecture is clean, the layering is correct, the model/view discipline is mostly respected. It is not at the right shape for v1.0 or a production deployment. The known-issues list in the brief acknowledges some of these — but understates their severity. The legacy account hierarchy in particular is a minefield that has to be either deleted or rebuilt before any storage layer is built on top of it.

**Path to Conditionally Approve:**

- All 7 Critical Issues fixed and unit-covered.
- M-1 through M-19 fixed (or M-19 and M-12 deferred with explicit `Q_UNUSED`/`TODO(name): ...` markers).
- Constants in `constants.h` retyped to `double` for monetary values.
- `Account.cpp` rewritten (zero buffers, no leaked CRT calls).
- `BankAccount.h`'s `accountNumber` corrected to `char[]`.
- `CheckingAccount.cpp`'s base call passes the parameter, not the uninitialised member.
- `SavingAccount.cpp::withdraw` calls `canWithdraw(money)` with parens; `withdrawalsThisMonth` initialised.
- `CategoryReport` `categoryTotals` made a member, cleared on `generate`.
- All five dialogs apply `setMaxLength` matching the entity's char array length.
- Filter proxies switched to enum-based comparison and exposed via a `UserRole`.

**Path to Approve:**

Not on the current trajectory. The codebase needs the storage layer built, the legacy account hierarchy deleted or rebuilt, the model/view dispatch made unambiguous, and at minimum *one* end-to-end automated test (dialog → entity → serialize → deserialize → model → render) to catch the next regression.

---

## Fix Guide

This section is written so another agent can apply each fix without further context. Each item lists the file, the lines, what's wrong, and the corrected code.

---

### F-1. `Account.cpp` constructor — zero buffer, safe `localtime`, fitting format

**File:** `C:\Users\rayan\Videos\Captures\PROJECTS\accounting-system\accounting-system\Account.cpp`

**Replace the entire constructor** (lines 5-19) with:

```cpp
Account::Account(unsigned short int idIn, const std::string& nameIn, double initialBalance)
    : id(idIn), balance(initialBalance)
{
    std::memset(name,      0, sizeof(name));
    std::memset(createdAt, 0, sizeof(createdAt));

    std::strncpy(name, nameIn.c_str(), sizeof(name) - 1);

    std::time_t now = std::time(nullptr);
    std::tm  local{};
#if defined(_WIN32)
    if (localtime_s(&local, &now) != 0) return;
#else
    if (!localtime_r(&now, &local)) return;
#endif
    // "%Y-%m-%d" = 10 chars + NUL = 11 → fits in createdAt[12].
    std::strftime(createdAt, sizeof(createdAt), "%Y-%m-%d", &local);
}
```

Add `#include <ctime>` and `#include <cstring>` if not present. Remove the unused `bytesWritten` variable.

---

### F-2. `Account.h` — add virtual destructor, `override` keyword, drop `void const` on display methods

**File:** `Account.h`

Add a virtual destructor (after line 26):

```cpp
virtual ~Account() = default;
```

Also, in `CashAccount.h:11-15`, `BankAccount.h:14-17`, etc., add `override` to every overriding method:

```cpp
AccountType getAccountType() override;
bool        canWithdraw(double) override;
void        deposit(double) override;
void        withdraw(double) override;
```

---

### F-3. `BankAccount.h` — fix `accountNumber` type

**File:** `BankAccount.h:11`

Replace:
```cpp
unsigned short int accountNumber[MAX_ACCOUNT_NUM_LENGTH];
```
With:
```cpp
char accountNumber[MAX_ACCOUNT_NUM_LENGTH];
```

If `accountNumber` is genuinely unused, delete the field. Currently nothing reads or writes it.

---

### F-4. `CheckingAccount.cpp` — pass parameter to base, not uninitialised member

**File:** `CheckingAccount.cpp:5-6`

Replace:
```cpp
ChekingAccount::ChekingAccount(unsigned short id, const std::string& nname, double iBalance, const std::string& bankNname)
:BankAccount(id,nname,iBalance,bankName){};
```
With:
```cpp
CheckingAccount::CheckingAccount(unsigned short id,
                                 const std::string& accountName,
                                 double initialBalance,
                                 const std::string& bankName)
    : BankAccount(id, accountName, initialBalance, bankName) {}
```

Rename the class declaration in `CheckingAccount.h:4` from `ChekingAccount` to `CheckingAccount` everywhere (file, methods, base call). The class name and the file name should match.

---

### F-5. `CheckingAccount.cpp::canWithdraw` — actually inspect balance

**File:** `CheckingAccount.cpp:8-11`

Replace:
```cpp
bool ChekingAccount::canWithdraw(double money){
    if (money >= MAX_OVERDRAFT_LIMIT){ return true;}
 else return false;
}
```
With:
```cpp
bool CheckingAccount::canWithdraw(double amount){
    return (balance - amount) >= overdraftLimit;
}
```

`overdraftLimit` is the per-instance limit (currently initialised to `MAX_OVERDRAFT_LIMIT`).

---

### F-6. `constants.h` — retype currency constants to `double`

**File:** `constants.h:19-20`

Replace:
```cpp
const unsigned short int MAX_WITHDRAWAL_LIMIT    = 50000;
const unsigned short int MAX_OVERDRAFT_LIMIT     = -20000;
```
With:
```cpp
constexpr double MAX_WITHDRAWAL_LIMIT = 50000.0;
constexpr double MAX_OVERDRAFT_LIMIT  = -20000.0;
```

---

### F-7. `SavingAccount.cpp::withdraw` — call `canWithdraw` with parens; initialise `withdrawalsThisMonth`

**File:** `SavingAccount.cpp:9-18`

Replace:
```cpp
void SavingsAccount::withdraw(double money){
if(canWithdraw){
    balance -= money;
    withdrawalsThisMonth += money;
}
 else if(withdrawalsThisMonth > MAX_WITHDRAWAL_LIMIT){
    throw std::out_of_range("You have exceeded your monthly withdrawal limit.");
 }
 else throw std::out_of_range("The amount entered is beyond your actual balance.\n");
}
```
With:
```cpp
void SavingsAccount::withdraw(double money){
    if (canWithdraw(money)) {
        balance -= money;
        withdrawalsThisMonth += money;
    }
    else if (withdrawalsThisMonth > MAX_WITHDRAWAL_LIMIT) {
        throw std::out_of_range("Monthly withdrawal limit exceeded.");
    }
    else {
        throw std::out_of_range("Withdrawal exceeds current balance.");
    }
}
```

**Also**: Update the `SavingsAccount` constructor (lines 5-8) to initialise all members:

```cpp
SavingsAccount::SavingsAccount(unsigned short int id, const std::string& name,
                               double initialBalance, const std::string& bankName,
                               float intRate)
    : BankAccount(id, name, initialBalance, bankName),
      intrestRate(intRate),
      withdrawalsThisMonth(0.0) {}
```

Rename `SavingAccount.cpp` to `SavingsAccount.cpp` so it matches the header.

---

### F-8. `CategoryReport` — make `categoryTotals` a member; clear on `generate`

**File:** `CategoryReport.h`

Add to the class:
```cpp
private:
    std::unordered_map<unsigned short, double> categoryTotals;
```

Add `#include <unordered_map>`.

**File:** `CategoryReport.cpp`

Remove `std::vector<double> categoryTotals(100, 0.0);` at line 9.

Replace `generate()`:
```cpp
void CategoryReport::generate(){
    categoryTotals.clear();
    for (auto* transaction : transactions)
        categoryTotals[transaction->getCategoryId()] += transaction->getAmount();
}
```

Replace `display()`:
```cpp
void CategoryReport::display() {
    std::cout << "Category Report for " << month << "/" << year << std::endl;
    std::cout << "Category ID | Total Amount" << std::endl;
    for (const auto& [id, total] : categoryTotals)
        if (total != 0.0)
            std::cout << id << " | " << total << std::endl;
}
```

Also fix the constructor at lines 3-8 — it currently calls `addTransaction(transaction.get())` on the *base* `Report::transactions`, but `CategoryReport::generate` reads from the same base member, so the data path actually works. The bug is the duplicated member in `MonthlyReport.h:18`. See F-9.

---

### F-9. `MonthlyReport.h` — remove shadowing `transactions` member

**File:** `MonthlyReport.h:18`

Delete the line:
```cpp
std::vector<std::shared_ptr<Transaction>> transactions;
```

This is shadowing `Report::transactions`. Either:
1. Change `Report::transactions` to `std::vector<std::shared_ptr<Transaction>>` (then update every caller), or
2. Keep `Report::transactions` as raw pointers and have `MonthlyReport` use it.

Option 2 is the smaller change.

Update `MonthlyReport.cpp:3-6`:
```cpp
MonthlyReport::MonthlyReport(unsigned short month, unsigned short year,
                             std::vector<std::shared_ptr<Transaction>>& source)
    : Report(month, year)
{
    for (const auto& sp : source) addTransaction(sp.get());
}
```

Update `MonthlyReport::generate` to iterate `Report::transactions` (raw pointer vector).

---

### F-10. `Report.h` — include the correct header

**File:** `Report.h:9`

Replace:
```cpp
#include "transaction.cpp"
```
With:
```cpp
#include "transaction/transaction.h"
#include <memory>
```

Then add the four files to `CMakeLists.txt`'s `CORE_SOURCES`:
```cmake
Report.cpp
MonthlyReport.cpp
CategoryReport.cpp
transaction/transaction.cpp
transaction/ExpenseTransaction.cpp
transaction/IncomeTransaction.cpp
transaction/RecurringExpense.cpp
transaction/RecurringIncome.cpp
```

If the intent is to leave this code dead for now, delete `Report.{h,cpp}`, `MonthlyReport.{h,cpp}`, `CategoryReport.{h,cpp}` from the source tree.

---

### F-11. `MonthlyReport.cpp::display` — show signed total expenses correctly

**File:** `MonthlyReport.cpp:7-22`

Replace:
```cpp
void MonthlyReport::generate(){
    for (const auto& transaction : transactions) {
        if (transaction->getAmount() > 0) {
            totalIncome += transaction->getAmount();
        } else {
            totalExpenses += transaction->getAmount();
        }
    }
}
void MonthlyReport::display(){
    std::cout << "Monthly Report for " << month << "/" << year << std::endl;
    std::cout << "Total Income: " << totalIncome << std::endl;
    std::cout << "Total Expenses: " << totalExpenses << std::endl;
    std::cout << "Net Balance: " << (totalIncome + totalExpenses) << std::endl;
}
```
With:
```cpp
void MonthlyReport::generate(){
    totalIncome = 0.0;
    totalExpenses = 0.0;
    for (auto* transaction : transactions) {
        const double amount = transaction->getAmount();
        if (amount >= 0) totalIncome   += amount;
        else             totalExpenses += -amount;          // accumulate as positive
    }
}
void MonthlyReport::display(){
    std::cout << "Monthly Report for " << month << "/" << year << std::endl;
    std::cout << "Total Income:   " << totalIncome << std::endl;
    std::cout << "Total Expenses: " << totalExpenses << std::endl;
    std::cout << "Net Balance:    " << (totalIncome - totalExpenses) << std::endl;
}
```

Implement `exportToFile`:
```cpp
void MonthlyReport::exportToFile(const std::string& fileName){
    std::ofstream out(fileName);
    if (!out) throw std::runtime_error("Cannot open " + fileName);
    out << "Monthly Report for " << month << "/" << year << "\n"
        << "Total Income:   " << totalIncome   << "\n"
        << "Total Expenses: " << totalExpenses << "\n"
        << "Net Balance:    " << (totalIncome - totalExpenses) << "\n";
}
```

Add `#include <fstream>`.

---

### F-12. Editor dialogs — enforce `setMaxLength` on every `QLineEdit`

**Files:**
- `src/ui/dialogs/CustomerEditorDialog.cpp` (after line 27)
- `src/ui/dialogs/SupplierEditorDialog.cpp` (after line 26)
- `src/ui/dialogs/ProductEditorDialog.cpp` (after line 26)
- `src/ui/dialogs/InvoiceEditorDialog.cpp` (after line 26)
- `src/ui/dialogs/PaymentEditorDialog.cpp` (after line 26)

In each `buildUi()`:

```cpp
// CustomerEditorDialog::buildUi
m_nameEdit ->setMaxLength(CUSTOMER_NAME_LENGTH  - 1);
m_emailEdit->setMaxLength(CUSTOMER_EMAIL_LENGTH - 1);
m_phoneEdit->setMaxLength(CUSTOMER_PHONE_LENGTH - 1);
m_taxEdit  ->setMaxLength(CUSTOMER_TAX_LENGTH   - 1);
```

(Use the analogous constants in `SupplierEditorDialog`, `ProductEditorDialog`, etc.)

For UTF-8 safety, in `accept()`, additionally check:
```cpp
if (name.toUtf8().size() > CUSTOMER_NAME_LENGTH - 1) {
    m_nameRow->setError("Name too long.");
    return;
}
```

---

### F-13. Editor dialogs — attribute exception errors to the correct row

**File:** `src/ui/dialogs/CustomerEditorDialog.cpp:140-142` (and the four siblings)

Replace the single-row `catch`:
```cpp
} catch (const std::exception& e) {
    m_nameRow->setError(QString::fromUtf8(e.what()));
    return;
}
```
With:
```cpp
} catch (const std::exception& e) {
    const QString msg = QString::fromUtf8(e.what());
    if      (msg.contains("name",   Qt::CaseInsensitive)) m_nameRow ->setError(msg);
    else if (msg.contains("email",  Qt::CaseInsensitive)) m_emailRow->setError(msg);
    else                                                  m_nameRow ->setError(msg);
    return;
}
```

A more robust solution is to validate inside the dialog *before* construction so the dialog knows which field failed. The above is the minimal change.

---

### F-14. Filter proxies — compare against enum, not displayed text

**Pattern for `CustomersPage`:**

`CustomerTableModel.cpp` add a `data()` branch for `Qt::UserRole`:

```cpp
QVariant CustomerTableModel::data(const QModelIndex& idx, int role) const {
    if (!idx.isValid() || idx.row() >= static_cast<int>(rows_.size())) return {};
    const Customer& c = rows_[idx.row()];

    if (role == Qt::UserRole && idx.column() == ColStatus)
        return c.getIsDeleted();        // bool: true=inactive, false=active

    if (role != Qt::DisplayRole && role != Qt::EditRole) return {};
    // … existing switch …
}
```

`CustomersPage.cpp` `CustomerFilterProxy::filterAcceptsRow`:

```cpp
bool filterAcceptsRow(int row, const QModelIndex& parent) const override {
    if (m_status != StatusFilter::Any) {
        const bool deleted = sourceModel()
            ->index(row, CustomerTableModel::ColStatus, parent)
            .data(Qt::UserRole).toBool();
        const bool wantInactive = (m_status == StatusFilter::Inactive);
        if (deleted != wantInactive) return false;
    }
    return QSortFilterProxyModel::filterAcceptsRow(row, parent);
}
```

Where `StatusFilter` is `enum class { Any, Active, Inactive }`. The filter setter translates the combo's text once at signal time, not on every row.

Repeat for `SupplierFilterProxy`, `ProductFilterProxy`, `InvoiceFilterProxy`, `PaymentFilterProxy`.

---

### F-15. Remove duplicate signal connections in derived list pages

**File:** `src/ui/pages/customers/CustomersPage.cpp:63-64`

Delete:
```cpp
connect(m_searchBar, &SearchBar::searchChanged,
        this, &CustomersPage::onSearch);
```

The virtual `onSearch` is already dispatched via the base's `setupListLayout` connection (`ListPage.cpp:33`).

Apply the same deletion to `SuppliersPage.cpp:58`, `ProductsPage.cpp:68`, `InvoicesPage.cpp:62`, `PaymentsPage.cpp:66`.

Similarly: `CustomersPage.cpp:65-66` connects `m_filterBar->filterChanged` to `rebuildFilter` — but `ListPage.cpp:34` already connects `filterChanged` to `ListPage::onFilterChanged` (virtual). Either:
1. Rename `rebuildFilter` to `onFilterChanged` (matching base) and remove the explicit connection, or
2. Disconnect the base connection in derived constructor:
```cpp
disconnect(m_filterBar, &FilterBar::filterChanged, nullptr, nullptr);
connect   (m_filterBar, &FilterBar::filterChanged, this, &CustomersPage::rebuildFilter);
```

Option 1 is cleaner. Rename `rebuildFilter` to `onFilterChanged` (override) in all four pages.

---

### F-16. `InvoicesPage` — wire the filter bar's `filterChanged` to a slot

**File:** `src/ui/pages/invoices/InvoicesPage.cpp`

After `setupListLayout()` (line 65), connect:
```cpp
connect(m_filterBar, &FilterBar::filterChanged, this, [this]{
    // Customer filter: m_filterBar->filterValue(0) maps to a customer id or empty
    const QString sel = m_filterBar->filterValue(0);
    m_proxy->setCustomerFilter(sel);    // add this method to InvoiceFilterProxy
    m_pagination->setTotalRecords(m_proxy->rowCount());
});
```

Add the `setCustomerFilter` method to `InvoiceFilterProxy` and have it compare against the model's `ColCustomerId` via the customer name (which will require a customer name lookup once the repository exists). Until then, the customer combo should be populated from a placeholder list and the filter does name-contains.

A simpler interim fix is to hide the customer filter until the repository exists:
```cpp
// In InvoicesPage::InvoicesPage, after addFilter
m_filterBar->setVisible(false);
```

---

### F-17. `CustomersPage::loadTestData` — remove empty string from status combo

**File:** `src/ui/pages/customers/CustomersPage.cpp:43`

Replace:
```cpp
m_filterBar->addFilter("Status", {"", "Active", "Inactive"});
```
With:
```cpp
m_filterBar->addFilter("Status", {"Active", "Inactive"});
```

This aligns with `SuppliersPage.cpp:39` and removes the always-empty intermediate option.

---

### F-18. `PaymentsPage` `setMethodFilter`/`setPartyFilter` — fold into one `invalidateRowsFilter`

**File:** `src/ui/pages/payments/PaymentsPage.cpp:16-42`

Replace the proxy with:

```cpp
class PaymentFilterProxy : public QSortFilterProxyModel {
public:
    using QSortFilterProxyModel::QSortFilterProxyModel;
    void setFilters(const QString& method, const QString& party){
        m_method = method;
        m_party  = party;
        invalidateRowsFilter();
    }
protected:
    bool filterAcceptsRow(int row, const QModelIndex& parent) const override {
        if (!m_method.isEmpty()) {
            const QString m = sourceModel()->index(row, PaymentTableModel::ColMethod, parent)
                                            .data().toString();
            if (m != m_method) return false;
        }
        if (!m_party.isEmpty()) {
            const QString p = sourceModel()->index(row, PaymentTableModel::ColParty, parent)
                                            .data().toString();
            if (!p.startsWith(m_party + " ")) return false;        // explicit boundary
        }
        return QSortFilterProxyModel::filterAcceptsRow(row, parent);
    }
private:
    QString m_method, m_party;
};
```

`rebuildFilter`:
```cpp
void PaymentsPage::rebuildFilter(){
    static_cast<PaymentFilterProxy*>(m_proxy)->setFilters(
        m_filterBar->filterValue(0),
        m_filterBar->filterValue(1));
    m_pagination->setTotalRecords(m_proxy->rowCount());
}
```

Apply the same one-call-`invalidateRowsFilter` pattern to `ProductsPage::rebuildFilter`.

---

### F-19. `computeNextId` — handle overflow

Replace the implementation in each page with:

```cpp
unsigned short int CustomersPage::computeNextId() const
{
    unsigned short int maxId = 1000;
    for (int i = 0; i < m_model->rowCount(); ++i)
        maxId = std::max(maxId, m_model->at(i).getId());
    if (maxId >= 65534)
        throw std::overflow_error("Customer ID space exhausted");
    return static_cast<unsigned short int>(maxId + 1);
}
```

The caller (`onAddClicked`) should catch the exception and show an error dialog. Better long-term: change IDs to `std::uint32_t` everywhere (record sizes need to be revised, but it eliminates the entire class of bug).

---

### F-20. `Setters` — return `bool` on validating setters

**Files:** `core/Invoice.cpp:167-171`, `core/Payment.cpp:149-152`, `core/Payment.cpp:159-162`, `core/Payment.cpp:166-170`, `core/Product.cpp:113-117`, `core/Product.cpp:120-124`, `core/Invoice.cpp:146-150`, `core/Invoice.cpp:153-157`, `core/Invoice.cpp:160-164`, `core/Budget.cpp:90-94`, `core/Budget.cpp:97-101`.

Pattern: change `void setX(...)` to `bool setX(...)` and return `false` on rejection. Update headers and call sites.

Alternatively: throw `std::invalid_argument` on bad input.

Pick one — *don't* keep the silent no-op.

---

### F-21. `CustomerTableModel` (and siblings) — rename `removeRow`, broadcast role list

**File:** `src/ui/models/CustomerTableModel.{h,cpp}` and four siblings.

Rename `removeRow(int)` → `softDelete(int)`. `removeRow` is a Qt-virtual whose contract is to *remove* rows; overriding the name with different semantics confuses callers and tooling.

Update the `dataChanged` emission to broadcast the roles:
```cpp
void CustomerTableModel::softDelete(int row)
{
    if (row < 0 || row >= static_cast<int>(rows_.size())) return;
    rows_[row].setIsDeleted(true);
    emit dataChanged(index(row, 0), index(row, ColCount - 1),
                     {Qt::DisplayRole, Qt::EditRole, Qt::UserRole});
}
```

Update call sites accordingly.

---

### F-22. `DashboardPage::onActivated` — guard against repeated model rebuilds

**File:** `src/ui/pages/dashboard/DashboardPage.cpp:211-267`

Move model construction to the *constructor* (called once); leave only data-mutation in `onActivated`. Or, if data must be re-fetched per visit, delete the previous model before installing the new one:

```cpp
void DashboardPage::onActivated()
{
    if (auto* old = m_recentInvoices->tableView()->model())
        old->deleteLater();
    auto* rim = new QStandardItemModel(0, 6, m_recentInvoices);
    // … existing code …
    m_recentInvoices->setModel(rim);

    if (auto* old = m_overdueInvoices->tableView()->model())
        old->deleteLater();
    auto* oim = new QStandardItemModel(0, 4, m_overdueInvoices);
    // … existing code …
    m_overdueInvoices->setModel(oim);
    // KPI updates …
}
```

---

### F-23. `Customer::Customer(const CustomerData&)` — assign `id` before validation check, init in member-init list

**File:** `core/Customer.cpp:24-36`

Replace with:
```cpp
Customer::Customer(const CustomerData& info)
    : id(info.id),
      balance(info.balance),
      isDeleted(info.isDeleted)
{
    if (info.name == nullptr || info.name[0] == '\0')
        throw std::invalid_argument("Customer name cannot be empty");
    copyField(name,      CUSTOMER_NAME_LENGTH,  info.name);
    copyField(email,     CUSTOMER_EMAIL_LENGTH, info.email);
    copyField(phone,     CUSTOMER_PHONE_LENGTH, info.phone);
    copyField(taxNumber, CUSTOMER_TAX_LENGTH,   info.taxNumber);
}
```

This is mostly cosmetic, but member-init lists are clearer and avoid the "what if validation throws after some fields are set" issue. Apply the same pattern to `Supplier`, `Product`, `Invoice`, `Payment`.

---

### F-24. `Invoice` — enforce `total == subtotal + taxAmount`

**File:** `core/Invoice.cpp:51-56`

Replace `isValid` with:
```cpp
bool Invoice::isValid() const {
    if (invoiceNumber[0] == '\0')          return false;
    if (!isKnownStatus(status))            return false;
    if (subtotal < 0 || taxAmount < 0 || total < 0) return false;
    if (std::abs(total - (subtotal + taxAmount)) > 0.005) return false;
    return true;
}
```

Add `#include <cmath>` and update the constructor to also enforce this:
```cpp
if (std::abs(info.total - (info.subtotal + info.taxAmount)) > 0.005)
    throw std::invalid_argument("Invoice total must equal subtotal + tax");
```

---

### F-25. `RecurringExpense::isDueToday` and `RecurringIncome::isDueToday` — validate date format and frequency

**File:** `transaction/RecurringExpense.cpp:16-33`, `transaction/RecurringIncome.cpp:16-33`

Replace with:
```cpp
bool RecurringExpense::isDueToday() const
{
    if (frequencyDays <= 0) return false;

    int sy = 0, sm = 0, sd = 0;
    if (std::sscanf(date, "%d-%d-%d", &sy, &sm, &sd) != 3) return false;
    if (sy < 1970 || sm < 1 || sm > 12 || sd < 1 || sd > 31) return false;

    std::tm startTm{};
    startTm.tm_year = sy - 1900;
    startTm.tm_mon  = sm - 1;
    startTm.tm_mday = sd;
    const std::time_t startTime = std::mktime(&startTm);
    if (startTime == -1) return false;

    const std::time_t now = std::time(nullptr);
    std::tm  local{};
#if defined(_WIN32)
    if (localtime_s(&local, &now) != 0) return false;
#else
    if (!localtime_r(&now, &local)) return false;
#endif
    local.tm_hour = local.tm_min = local.tm_sec = 0;
    const std::time_t todayMidnight = std::mktime(&local);
    if (todayMidnight == -1) return false;

    const int diffDays = static_cast<int>(std::difftime(todayMidnight, startTime) / 86400);
    return diffDays >= 0 && (diffDays % frequencyDays) == 0;
}
```

Apply the same to `RecurringIncome::isDueToday`. Also enforce `frequencyDays > 0` in the `RecurringExpense`/`RecurringIncome` constructor:
```cpp
if (info.frequencyDays <= 0)
    throw std::invalid_argument("frequencyDays must be positive");
```

---

### F-26. Delete the orphan root-level files

**Files to delete:**
- `C:\Users\rayan\Videos\Captures\PROJECTS\accounting-system\accounting-system\Budget.h`
- `C:\Users\rayan\Videos\Captures\PROJECTS\accounting-system\accounting-system\Budget.cpp`
- `C:\Users\rayan\Videos\Captures\PROJECTS\accounting-system\accounting-system\Category.h`
- `C:\Users\rayan\Videos\Captures\PROJECTS\accounting-system\accounting-system\Category.cpp`

These duplicate the active versions in `core/`. They are out of sync, define different types for the same fields, and confuse readers.

---

### F-27. `setSortIndicatorShown(true)` in `DataTableView`

**File:** `src/ui/components/tables/DataTableView.cpp:123`

Replace:
```cpp
m_table->horizontalHeader()->setSortIndicatorShown(false);
```
With:
```cpp
m_table->horizontalHeader()->setSortIndicatorShown(true);
```

The `QHeaderView::up-arrow/down-arrow` styling in `ThemeManager.cpp:146` is `image: none; width: 0; height: 0;` — which still hides the arrow. Either remove that QSS rule or replace with a custom arrow.

---

### F-28. `GlobalToolBar` — wire or hide unused menu items

**File:** `src/ui/app/GlobalToolBar.cpp:32-39`

Either add the signals + connect:
```cpp
signals:
    void newSupplierRequested();
    void newProductRequested();

// In ctor
connect(newSupplier, &QAction::triggered, this, &GlobalToolBar::newSupplierRequested);
connect(newProduct,  &QAction::triggered, this, &GlobalToolBar::newProductRequested);
```
And wire them in `MainWindow::connectSignals` to navigate.

Or remove the menu entries.

---

### F-29. `PaginationFooter` — hide until pagination is implemented

**File:** `src/ui/components/tables/PaginationFooter.cpp`

Add `setVisible(false)` at the end of the constructor. Until a row-range proxy is added, the pagination is misleading.

Alternatively, disable prev/next:
```cpp
m_prevBtn->setEnabled(false);
m_nextBtn->setEnabled(false);
```

---

### F-30. `m_proxy` field type — make it the derived type

**Files:** all five pages.

Change the field declaration in the `.h`:
```cpp
// CustomersPage.h
private:
    class CustomerFilterProxy* m_proxy;        // forward declaration of inner class
```

And update the constructor to not need `static_cast` in `rebuildFilter`.

For inner-class proxies defined in the .cpp, use a small helper interface in the header:

```cpp
// CustomersPage.h
class CustomerFilterProxy;
…
private:
    CustomerFilterProxy* m_proxy;
```

The implementation file forward-declares + defines the inner class as it does today. The compile-time type now matches, and `static_cast` disappears.

---

### F-31. `ListPage` — protected widgets created in constructor are leaked if `setupListLayout` isn't called

**File:** `src/ui/pages/base/ListPage.{h,cpp}`

The current design lets derived classes "forget" to call `setupListLayout()`. Refactor to either:
1. Call `setupListLayout()` in the `ListPage` constructor (then derived classes that need extra widgets call an exposed `addAtTop(QWidget*)`).
2. Make `setupListLayout` a pure virtual that derived classes must implement.

Recommended: option 1 with an `addAtTop`/`addAtBottom` API:

```cpp
class ListPage : public Page {
public:
    explicit ListPage(QWidget* parent = nullptr);
protected:
    void addAtTop(QWidget* w);     // inserts above the filter strip
    void addAtBottom(QWidget* w);  // inserts below the pagination
    …
};
```

`InvoicesPage` then calls `addAtTop(m_statusTabs)` instead of `m_mainLayout->insertWidget(0, m_statusTabs)`.

---

### F-32. `SettingsPage` theme combo — apply on Save, not on combo change

**File:** `src/ui/pages/settings/SettingsPage.cpp:196-199`

Remove:
```cpp
connect(m_themeCombo, &QComboBox::currentTextChanged, this, [](const QString& t) {
    ThemeManager::instance().setTheme(
        t == "Dark" ? ThemeManager::Theme::Dark : ThemeManager::Theme::Light);
});
```

Move the theme application into `onSaveClicked`:
```cpp
ThemeManager::instance().setTheme(
    m_themeCombo->currentText() == "Dark"
        ? ThemeManager::Theme::Dark
        : ThemeManager::Theme::Light);
```

Now `Revert` correctly discards the theme change. (The user sees the theme dropdown change but the theme only applies on Save.)

---

### F-33. `Customer::display`, `Supplier::display`, etc. — use `qDebug()` instead of `std::cout`

**Files:** all five entities' `display()` methods.

Replace:
```cpp
std::cout << "[CUSTOMER] ID:" << id << ... ;
```
With:
```cpp
qDebug() << "[CUSTOMER] ID:" << id << ...;
```

And `#include <QDebug>` in each implementation.

This makes display calls visible during development (via the IDE debugger console) and silent in production.

---

### F-34. `BankAccount.cpp::withdraw` — `BankAccount` is abstract (no `canWithdraw` impl) so this code can never execute. Either remove or implement `canWithdraw` in `BankAccount`.

**File:** `BankAccount.cpp:16-20`

```cpp
void BankAccount::withdraw(double amount){
    if(canWithdraw(amount))
    balance -= amount;
    else throw std::out_of_range("The amount entered is beyond your actual balance.\n");
}
```

`BankAccount::canWithdraw` is *pure virtual* inherited from `Account::canWithdraw`. Calling `canWithdraw` from `BankAccount::withdraw` (when the dynamic type is `BankAccount`) is UB — but in practice it'll dispatch to the derived class. The intent is fine, but make the dispatch explicit by adding a check at construction that prevents instantiating `BankAccount` directly.

Add `#include <stdexcept>` and the proper logic.

---

### F-35. CMakeLists — add `Wall`, `Wextra`, `Wpedantic`, treat warnings as errors

**File:** `CMakeLists.txt`

Add after `find_package(Qt6 ...)`:
```cmake
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wnon-virtual-dtor)
endif()
```

This would have caught the `localtime` shadowing in `Account.cpp:12` (local variable named `localtime` shadowing the function), the unused `bytesWritten`, the missing `override` keyword, the missing virtual destructor on `Account`.

Optionally `-Werror` once the warnings are clean.

---

### F-36. Add an `AccountingSystem` test-target or remove the class

**File:** `core/AccountingSystem.{h,cpp}`

The class is well-built, well-tested-looking, but **unused by any caller**. Either:
1. Wire it into `MainWindow` as the source of Category/Budget data for a future `CategoriesPage`/`BudgetsPage`.
2. Delete it.

Don't leave dead code in `core/`.

---

### F-37. `ThemeManager` — emit `themeChanged` *before* setting the stylesheet

**File:** `src/ui/theme/ThemeManager.cpp:18-25`

Currently:
```cpp
void ThemeManager::setTheme(Theme theme){
    if (m_theme == theme) return;
    m_theme = theme;
    if (auto* app = qApp)
        app->setStyleSheet(buildStyleSheet(theme));
    emit themeChanged(theme);
}
```

`setStyleSheet` triggers a synchronous repaint cascade across all widgets. If any slot connected to `themeChanged` then queries `m_theme` (likely), the order is fine. But this should still emit before the repaint to give slots a chance to set state before paint. Cosmetic.

---

### F-38. `Settings` — switch to INI format under `%APPDATA%`

**File:** `main.cpp:9-16`

Add after `QApplication::setOrganizationName`:
```cpp
QSettings::setDefaultFormat(QSettings::IniFormat);
QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                   QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
```

Add `#include <QSettings>` and `#include <QStandardPaths>`.

This makes settings inspectable, backup-able, and version-controllable.

---

### F-39. Move `copyField` out of every `core/*.cpp` to one shared header

**File:** Create `core/StringField.h`:

```cpp
#pragma once
#include <cstring>
#include <cstddef>

inline void copyField(char* dest, std::size_t capacity, const char* src)
{
    std::memset(dest, 0, capacity);
    if (!src) return;
    std::strncpy(dest, src, capacity - 1);
    dest[capacity - 1] = '\0';
}
```

Update `core/Customer.cpp:6-13`, `Supplier.cpp:6-12`, `Product.cpp:6-12`, `Invoice.cpp:6-12`, `Payment.cpp:6-12` to remove their static `copyField` and `#include "core/StringField.h"`.

---

### F-40. Build tests

**Add:** `tests/` directory with one round-trip test per entity:

```cpp
// tests/test_customer.cpp
#include "Customer.h"
#include <cassert>
#include <cstring>

void test_customer_round_trip(){
    char buf[CUSTOMER_RECORD_SIZE];
    Customer c(CustomerData{42, "Acme Corp", "a@b.c", "555-1234", "TX-1", 100.0, false});
    c.serialize(buf);
    Customer c2;
    c2.deserialize(buf);
    assert(c.getId() == c2.getId());
    assert(std::strcmp(c.getName(), c2.getName()) == 0);
    // … and so on …
}
```

Add a CMake test target:
```cmake
enable_testing()
add_executable(test_round_trip tests/test_round_trip.cpp ${CORE_SOURCES})
target_include_directories(test_round_trip PRIVATE core)
add_test(NAME RoundTrip COMMAND test_round_trip)
```

This is the *minimum* automated check that protects against silent serialization regressions.

---

## Appendix: File Touch Map

If applying these fixes in one pass, here is the order that minimises rebuild thrash:

1. `constants.h` — F-6
2. `core/StringField.h` (new) — F-39
3. `core/Customer.cpp`, `core/Supplier.cpp`, `core/Product.cpp`, `core/Invoice.cpp`, `core/Payment.cpp` — F-23, F-24, F-39
4. `Account.h`, `Account.cpp`, `BankAccount.h`, `BankAccount.cpp`, `CashAccount.h`, `CashAccount.cpp`, `CheckingAccount.h`, `CheckingAccount.cpp`, `SavingsAccount.h`, `SavingAccount.cpp` → rename to `SavingsAccount.cpp` — F-1 through F-7, F-34, m-1, m-2, m-3, m-4
5. `Report.h`, `Report.cpp`, `MonthlyReport.h`, `MonthlyReport.cpp`, `CategoryReport.h`, `CategoryReport.cpp` — F-8, F-9, F-10, F-11
6. `transaction/RecurringExpense.cpp`, `transaction/RecurringIncome.cpp` — F-25
7. Delete `Budget.h`, `Budget.cpp`, `Category.h`, `Category.cpp` (root) — F-26
8. `src/ui/models/CustomerTableModel.{h,cpp}` and four siblings — F-14, F-21
9. `src/ui/dialogs/CustomerEditorDialog.{h,cpp}` and four siblings — F-12, F-13
10. `src/ui/pages/customers/CustomersPage.{h,cpp}`, `suppliers/SuppliersPage.{h,cpp}`, `products/ProductsPage.{h,cpp}`, `invoices/InvoicesPage.{h,cpp}`, `payments/PaymentsPage.{h,cpp}` — F-15, F-16, F-17, F-18, F-19, F-30
11. `src/ui/pages/base/ListPage.{h,cpp}` — F-31
12. `src/ui/pages/dashboard/DashboardPage.cpp` — F-22
13. `src/ui/pages/settings/SettingsPage.cpp` — F-32
14. `src/ui/components/tables/DataTableView.cpp`, `PaginationFooter.cpp` — F-27, F-29
15. `src/ui/app/GlobalToolBar.{h,cpp}` — F-28
16. `src/ui/theme/ThemeManager.cpp` — F-37
17. `main.cpp` — F-38
18. `CMakeLists.txt` — F-10 (add dead-code files), F-35 (warnings)
19. `tests/` (new) — F-40

After applying: clean rebuild, `ctest`, smoke-test every page including the legacy `CheckingAccount`/`SavingsAccount` chain. Once that is green, **re-review** for any new defects introduced by the fixes themselves.
