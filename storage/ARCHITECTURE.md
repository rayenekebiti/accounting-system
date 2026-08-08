# Storage Layer Architecture

## Overview — Four Layers

```
┌─────────────────────────────────────────────────────────────┐
│  UI Pages (InvoicesPage, CustomersPage, DashboardPage, ...) │
└──────────────────────────┬──────────────────────────────────┘
                           │ uses
┌──────────────────────────▼──────────────────────────────────┐
│  Layer 4: Qt Model Adapters (QAbstractTableModel)            │
│  TransactionTableModel, AccountTableModel, ...               │
└──────────────────────────┬──────────────────────────────────┘
                           │ uses
┌──────────────────────────▼──────────────────────────────────┐
│  Layer 3: StorageService  (facade / singleton)               │
│  Owns all repositories, single entry point for app code      │
└──────────────────────────┬──────────────────────────────────┘
                           │ owns
┌──────────────────────────▼──────────────────────────────────┐
│  Layer 2: Repositories  (one per entity type)                │
│  TransactionRepository, AccountRepository, CategoryRepo...   │
│  Handle polymorphism, return owning domain objects           │
└──────────────────────────┬──────────────────────────────────┘
                           │ uses
┌──────────────────────────▼──────────────────────────────────┐
│  Layer 1: BinaryRecordFile  (type-erased byte I/O)           │
│  Fixed-size record append/read/update/soft-delete            │
│  No knowledge of domain types — just bytes                   │
└─────────────────────────────────────────────────────────────┘
```

Why four layers: each one solves exactly one problem and depends only on the layer below. Tests and changes don't cascade.

---

## Layer 1 — `BinaryRecordFile`

**Job:** Read/write fixed-size byte blocks at deterministic offsets. Knows nothing about domain.

```cpp
// storage/BinaryRecordFile.h
class BinaryRecordFile {
    std::fstream file_;
    std::string  path_;
    size_t       recordSize_;
public:
    BinaryRecordFile(std::string path, size_t recordSize);
    bool open();                                    // creates if missing

    uint16_t append(const char* buffer);            // returns new id (= file pos / recordSize)
    bool     read  (uint16_t id, char* buffer);     // false if out of range
    bool     update(uint16_t id, const char* buffer);
    size_t   count() const;                         // file_size / recordSize
};
```

**Key choices:**
- **Position-based IDs:** `id = byteOffset / recordSize`. No file header needed, no separate id counter to keep in sync.
- **Append-only writes** for new records → file position becomes the id automatically.
- **No domain knowledge** — it never asks "is this a Transaction?" Just bytes.
- **Soft delete is NOT here** — deletion is a domain concern (it's a flag in the record). Layer 1 only writes bytes.

---

## Layer 2 — Repositories

**Job:** Translate between domain objects and byte buffers. One repository per top-level entity. This is where polymorphism is solved.

```cpp
// storage/TransactionRepository.h
class TransactionRepository {
    BinaryRecordFile file_;
public:
    explicit TransactionRepository(const std::string& path);
    bool open();

    uint16_t save  (Transaction& tx);                       // appends, sets tx.id
    bool     update(const Transaction& tx);                 // by tx.getId()
    bool     remove(uint16_t id);                           // soft-delete (sets flag)

    std::unique_ptr<Transaction>              load   (uint16_t id);
    std::vector<std::unique_ptr<Transaction>> loadAll();   // skips soft-deleted

    // Query helpers (built on loadAll for now; index later)
    std::vector<std::unique_ptr<Transaction>> findByCategory(uint16_t catId);
    std::vector<std::unique_ptr<Transaction>> findByDateRange(const char* from, const char* to);

private:
    std::unique_ptr<Transaction> makeFromBuffer(const char* buf);  // reads type byte
};
```

**The polymorphism trick** (the heart of this layer):

```cpp
std::unique_ptr<Transaction> TransactionRepository::makeFromBuffer(const char* buf) {
    int type;
    std::memcpy(&type, buf + TX_TYPE_OFFSET, sizeof(int));    // peek at type byte

    std::unique_ptr<Transaction> tx;
    switch (static_cast<TransactionType>(type)) {
        case INCOME:            tx = std::make_unique<IncomeTransaction>();  break;
        case EXPENSE:           tx = std::make_unique<ExpenseTransaction>(); break;
        case RECURRING_INCOME:  tx = std::make_unique<RecurringIncome>();    break;
        case RECURRING_EXPENSE: tx = std::make_unique<RecurringExpense>();   break;
        default: return nullptr;
    }
    tx->deserialize(buf);
    return tx;
}
```

For this to work cleanly, each Transaction subclass needs a **protected default constructor** that the repository can call (since the current public constructor requires `TransactionData` and throws on bad input):

```cpp
class ExpenseTransaction : public Transaction {
protected:
    ExpenseTransaction() = default;          // for repository
    friend class TransactionRepository;
public:
    explicit ExpenseTransaction(const TransactionData& info);   // existing
    // ...
};
```

This is the cleanest way: validation happens at user-construction time, but the repository can build a blank instance and let `deserialize()` populate it.

**Same pattern for `AccountRepository`** — uses the account type byte to construct CashAccount/BankAccount/SavingsAccount/CheckingAccount.

**`CategoryRepository` and `BudgetRepository`** are simpler — non-polymorphic, just one type each. No type byte needed.

---

## Layer 3 — `StorageService` (Facade)

**Job:** Single entry point. The UI never touches a repository directly through a constructor — it asks the service.

```cpp
// storage/StorageService.h
class StorageService {
    TransactionRepository transactions_;
    AccountRepository     accounts_;
    CategoryRepository    categories_;
    BudgetRepository      budgets_;

    StorageService();   // private — singleton
public:
    static StorageService& instance();
    bool initialize(const QString& dataDir);   // opens all files

    TransactionRepository& transactions() { return transactions_; }
    AccountRepository&     accounts()     { return accounts_; }
    CategoryRepository&    categories()   { return categories_; }
    BudgetRepository&      budgets()      { return budgets_; }
};
```

**Why singleton:** in a Qt desktop app with a single MainWindow, there's exactly one storage instance. Passing it through 6 constructors to reach DashboardPage is noise. The singleton is the pragmatic choice here.

Called once in `main.cpp`:
```cpp
StorageService::instance().initialize(
    QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
);
```

---

## Layer 4 — Qt Model Adapters

**Job:** Wrap a repository in `QAbstractTableModel` so `DataTableView` can display it directly. No more `QStandardItemModel` with hardcoded rows.

```cpp
// src/ui/models/TransactionTableModel.h
class TransactionTableModel : public QAbstractTableModel {
    Q_OBJECT
    std::vector<std::unique_ptr<Transaction>> rows_;
    TransactionRepository* repo_;
public:
    explicit TransactionTableModel(TransactionRepository* repo, QObject* parent = nullptr);

    void reload();                                 // pulls fresh from repo, resets model
    void add   (std::unique_ptr<Transaction> tx);  // saves + appends row
    void edit  (int row);                          // pushes back to repo
    void remove(int row);                          // soft-delete in repo, removes row

    // QAbstractTableModel
    int      rowCount   (const QModelIndex& = {}) const override;
    int      columnCount(const QModelIndex& = {}) const override;
    QVariant data       (const QModelIndex&, int role) const override;
    QVariant headerData (int section, Qt::Orientation, int role) const override;
};
```

Then pages become trivial:

```cpp
void InvoicesPage::onActivated() {
    if (!m_model) {
        m_model = new TransactionTableModel(
            &StorageService::instance().transactions(), this);
        m_table->setModel(m_model);
    }
    m_model->reload();
}
```

---

## File System Layout

```
%APPDATA%/AccountingPro/data/      ← on Windows via QStandardPaths
├── transactions.dat
├── accounts.dat
├── categories.dat
├── budgets.dat
├── customers.dat        ← future
├── suppliers.dat        ← future
├── products.dat         ← future
├── invoices.dat         ← future
└── payments.dat         ← future
```

One file per top-level entity. No mixing.

---

## Cross-Cutting Rules

**ID strategy:** Position-based. `offset = id * RECORD_SIZE`. IDs are never reused, never reissued. Simple, fast, debuggable.

**Soft delete:** Each record has an `isDeleted` byte. `remove()` sets it to true. `loadAll()` skips them. Disk space is reclaimed by a future `compact()` operation (out of scope for v1).

**Errors:**
- Domain "not found" → `nullptr` (unique_ptr) or empty vector. Not an exception.
- I/O failure (corrupt file, disk full) → throw `std::runtime_error`. UI catches and shows `ConfirmDialog`.
- Bad input (negative income amount) → throw `std::invalid_argument` from the constructor, as it already does.

**Threading:** Single-threaded. The Qt UI thread does all I/O. Fine for a desktop app with sub-100k records. If you ever need async, wrap in `QtConcurrent::run` per call — repositories stay simple.

**Persistence format:** No magic bytes, no versioning, no schema migrations in v1. If you change a record layout later, write a one-shot converter script. Don't build a migration framework you don't need.

---

## How to Add a New Entity (e.g., Customer)

Five steps, mechanical:

1. Define `core/Customer.h/.cpp` with `serialize/deserialize/recordSize`.
2. Add `CUSTOMER_RECORD_SIZE` to `constants.h`.
3. Create `storage/CustomerRepository.h/.cpp` (copy CategoryRepository — non-polymorphic case).
4. Add it to `StorageService` (one member, one accessor, one `initialize` call).
5. Create `src/ui/models/CustomerTableModel.h/.cpp` and wire it into `CustomersPage::onActivated()`.

If the entity is polymorphic (like Transaction or Account), step 3 also includes a `makeFromBuffer()` switch on a type byte.

---

## Things to NOT Build (deliberately)

- **No ORM / no template repository hierarchy.** A single `RepositoryBase<T>` template sounds clever but breaks immediately on polymorphic types. Plain non-template classes per entity are clearer and easier to debug.
- **No database.** SQLite would be the right tool for a real product, but this is a fixed-size-record course project — keep it consistent with the assignment.
- **No caching layer.** `loadAll()` rereads the file every call. Files are small. If it ever matters, add an in-memory cache *inside* the repository — never above it.
- **No event bus / observer pattern.** The Qt model adapter emits signals; that's enough. Don't build a custom pub-sub on top.
- **No "transaction" (atomic multi-write).** You won't need it. Each call is one file write.

---

## Migration Path from Current Code

The existing `storage/filemanager.h` has these problems: VLA buffer, template can't handle polymorphic abstract types, uppercase/lowercase typo in destructor, undefined methods. **Don't fix it — replace it.**

Order of work:

1. **Day 1 AM** — Build `BinaryRecordFile`. Unit-test by writing/reading raw bytes.
2. **Day 1 PM** — Build `TransactionRepository` + `CategoryRepository`. Wire them through `StorageService`. Smoke-test by saving a transaction in `main.cpp` and reading it back.
3. **Day 2 AM** — Build `TransactionTableModel`. Replace hardcoded `QStandardItemModel` in `DashboardPage` and `InvoicesPage`.
4. **Day 2 PM** — Add `AccountRepository`, `BudgetRepository`, and remaining table models. Wire `SettingsPage` save buttons.

After this, the 35%-complete number becomes ~70%. Customers/Suppliers/Products/Invoices/Payments domain objects are the remaining gap, but each one is now a 30-minute task using the pattern above.
