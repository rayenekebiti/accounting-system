# Handoff: Accounting System — Code Audit Results

## Context

Console C++ accounting system (SCUT course project). Class hierarchy for transactions with
fixed-size binary record serialization via a templated `FileManager<T>`. Current branch:
`fix/transaction-headers`. A PR is open to merge into `main`.

## Current State

Only `.h` files exist for the two main classes. The one `.cpp` file (`filemanager.cpp`) is
empty. Nothing compiles or links yet.

---

## Must Fix Before Writing Any New Class

These are blockers. Do not add `ExpenseTransaction`, `RecurringIncome`, `Account`, or any
other class until all six are resolved.

### 1. Write `transaction.cpp`
`transaction.h` declares 12 getter/setter methods. None have implementations. Link will fail.
- Use `memset(field, 0, MAX_SIZE)` before every `strncpy` in string setters (`description`, `date`)
- Include `<cstring>`

### 2. Write `IncomeTransaction.cpp`
Constructor and all 5 overrides (`getEffectiveAmount`, `getType`, `display`, `serialize`,
`deserialize`) are declared but have no bodies anywhere. Link will fail.
- Constructor MUST initialize every member: `type = INCOME`, `isDeleted = false`
- Zero-init char arrays with `memset` before `strncpy`

### 3. Rename `IncomeTransaction.h` → `income_transaction.h`
PascalCase filenames break `#include` on Linux (case-sensitive filesystem).

### 4. Write `FileManager.h` as a proper template header — delete `filemanager.cpp`
Template method definitions cannot live in a standalone `.cpp` and be linked conventionally.
`filemanager.cpp` compiles to an empty object file. All `FileManager<T>` implementations must
go inside the header.

### 5. Add `static_assert` for record size
`TRANSACTION_RECORD_SIZE = 128` is a magic number with no compile-time verification.
Add to `income_transaction.h` (and every future concrete subclass):
```cpp
static_assert(sizeof(IncomeTransaction) <= TRANSACTION_RECORD_SIZE,
              "IncomeTransaction exceeds record size — update TRANSACTION_RECORD_SIZE");
```

### 6. Resolve `type` member vs `getType()` contradiction
`Transaction` has both a `TransactionType type` protected member AND a
`virtual TransactionType getType() const = 0` pure virtual. This is contradictory:
- If `getType()` is pure virtual and overridden by hard-coding a return value, `type` is dead weight
- If `type` is the authoritative source, `getType()` doesn't need to be virtual

**Pick one.** Recommended: remove the `type` member, override `getType()` in each subclass
to return a hard-coded enum value, serialize it via `getType()` in `serialize()`.

---

## Other Issues to Fix Soon

| Severity | Issue | Location |
|----------|-------|----------|
| CRITICAL | `isDeleted` is never initialized — garbage value causes silent data corruption on every file read | `transaction.h`, `IncomeTransaction.h` |
| CRITICAL | `MAX_CATEGORY_TYPE_LENGTH = 8` is too small for `"RECURRING_EXPENSE"` (17 chars) — buffer overflow | `constants.h` line 10 |
| HIGH | `const unsigned short int` constants should be `constexpr std::size_t` | `constants.h` |
| HIGH | Unscoped enums (`INCOME`, `EXPENSE`, `CASH`) pollute the global namespace — use `enum class` | `constants.h` lines 19–30 |
| MEDIUM | `MAX_END_DATE_LENGTH` is declared but unused — add a comment noting which future class uses it | `constants.h` line 6 |

---

## Correct Order of Work

1. `transaction.cpp` — all getters/setters
2. `IncomeTransaction.cpp` — constructor + 5 overrides
3. Rename `IncomeTransaction.h` → `income_transaction.h`
4. `FileManager.h` — full template header, delete `filemanager.cpp`
5. Add `static_assert` in `income_transaction.h`
6. Resolve `type` vs `getType()` contradiction
7. Fix `isDeleted` initialization
8. Fix `MAX_CATEGORY_TYPE_LENGTH`
9. Switch constants to `constexpr`, switch enums to `enum class`
10. Only then: start `ExpenseTransaction`, `RecurringIncome`, etc.
