# Fix Plan — Accounting System

Branch: `fix/transaction-headers` → `main`
Goal: get the project to a clean, compiling, link-safe baseline before any new class (`ExpenseTransaction`, `RecurringIncome`, `Account`, …) is written.

---

## Current State

| File | Status |
|------|--------|
| `transaction.h` | Declares 12 getter/setter methods + 5 pure virtuals. **No `.cpp`.** |
| `IncomeTransaction.h` | Declares constructor + 5 overrides. **No `.cpp`.** |
| `constants.h` | Uses `const unsigned short int` constants and unscoped enums. `MAX_CATEGORY_TYPE_LENGTH = 8` will overflow on `"RECURRING_EXPENSE"`. |
| `filemanager.cpp` | Empty. Templates cannot live here. |
| `FileManager.h` | **Missing.** Needs to be written as a header-only template. |

Nothing compiles. Nothing links.

---

## Blockers (must complete before any new class)

### 1. Write `transaction.cpp`
Implement the 12 getter/setter methods declared in `transaction.h`.

- `#include <cstring>` for `memset` / `strncpy`
- In string setters (`setDescription`, `setDate`): `memset(field, 0, MAX_*)` **before** `strncpy(field, src, MAX_* - 1)`
- Getters are trivial returns

### 2. Write `IncomeTransaction.cpp`
Implement the constructor and the 5 overrides.

- Constructor must initialize **every** member: `id`, `description`, `amount`, `date`, `categoryId`, `type = INCOME`, `isDeleted = false`
- Zero-init `description`/`date` with `memset` before `strncpy`
- `getEffectiveAmount()` returns `+amount` (positive — income)
- `getType()` returns `INCOME`
- `serialize`/`deserialize` write the fixed-layout record into the 128-byte buffer

### 3. Rename `IncomeTransaction.h` → `income_transaction.h`
PascalCase filenames break `#include` on Linux (case-sensitive FS). Use `git mv` to preserve history. Update the `#include` in `IncomeTransaction.cpp` and anywhere else it appears.

### 4. Write `FileManager.h`, delete `filemanager.cpp`
Template definitions must live in the header; a standalone `.cpp` will not link. Move every `FileManager<T>` implementation into `FileManager.h` (inline in the class or after the class body). Delete `filemanager.cpp` with `git rm`.

### 5. Add `static_assert` for record size
In `income_transaction.h` and every future concrete transaction subclass:
```cpp
static_assert(sizeof(IncomeTransaction) <= TRANSACTION_RECORD_SIZE,
              "IncomeTransaction exceeds record size — update TRANSACTION_RECORD_SIZE");
```

### 6. Resolve `type` member vs `getType()` contradiction
`Transaction` currently has both `TransactionType type` (protected member) and `virtual TransactionType getType() const = 0` (pure virtual). Keep one.

**Recommended:** drop the `type` member. Override `getType()` in each subclass to return a hard-coded enum literal. In `serialize()`, write `getType()` into the buffer.

This removes a redundant field, eliminates the risk of `type` and the override disagreeing, and shrinks the record.

---

## Other Issues (fix in the same PR)

| Severity | Issue | Location | Fix |
|----------|-------|----------|-----|
| CRITICAL | `isDeleted` never initialized — garbage value silently corrupts every file read | `transaction.h` / `IncomeTransaction` ctor | Initialize to `false` in every subclass constructor |
| CRITICAL | `MAX_CATEGORY_TYPE_LENGTH = 8` truncates `"RECURRING_EXPENSE"` (17 chars) — buffer overflow | `constants.h:10` | Bump to `24` (or compute from longest enum name) |
| HIGH | `const unsigned short int` constants | `constants.h` | Switch to `constexpr std::size_t` (include `<cstddef>`) |
| HIGH | Unscoped enums leak `INCOME`, `EXPENSE`, `CASH`, … into global namespace | `constants.h:19–30` | Convert to `enum class TransactionType` / `enum class AccountType`; update call sites |
| MEDIUM | `MAX_END_DATE_LENGTH` declared but unused | `constants.h:6` | Add a comment naming the future class that consumes it (e.g. `// used by RecurringTransaction`) |

---

## Correct Order of Work

1. `transaction.cpp` — all getters/setters
2. `IncomeTransaction.cpp` — constructor + 5 overrides
3. `git mv IncomeTransaction.h income_transaction.h`
4. `FileManager.h` (full template header) + `git rm filemanager.cpp`
5. `static_assert` in `income_transaction.h`
6. Resolve `type` vs `getType()` contradiction
7. Initialize `isDeleted` in every constructor
8. Fix `MAX_CATEGORY_TYPE_LENGTH`
9. Switch constants to `constexpr`, enums to `enum class` (and update references)
10. Verify clean compile + link with a minimal `main.cpp` that constructs an `IncomeTransaction`, serializes, deserializes, and asserts equality
11. **Only then:** start `ExpenseTransaction`, `RecurringIncome`, `Account`, etc.

---

## Definition of Done

- `g++ -std=c++17 -Wall -Wextra -Wpedantic *.cpp -o accounting` succeeds with zero warnings
- A round-trip test (`serialize` → `deserialize`) produces a bitwise-equal object
- `static_assert` passes for `IncomeTransaction`
- No PascalCase filenames remain
- No `const unsigned short int` constants or unscoped enums remain
- `isDeleted` is provably initialized on every construction path
