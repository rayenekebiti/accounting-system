# transaction.cpp — Design Document

## What goes here

Only the concrete (non-virtual) method implementations declared in `Transaction`.  
The five pure virtuals (`getEffectiveAmount`, `getType`, `display`, `serialize`, `deserialize`) are **not** implemented here — they belong in each subclass.

---

## Includes

```cpp
#include "transaction.h"
#include <cstring>   // strncpy
#include <cstdio>    // printf (used in display() of subclasses, not here)
```

---

## Getters

All read-only — return the member directly.

| Method | Return |
|---|---|
| `getId()` | `return id;` |
| `getDescription()` | `return description;` |
| `getAmount()` | `return amount;` |
| `getDate()` | `return date;` |
| `getCategoryId()` | `return categoryId;` |
| `getIsDeleted()` | `return isDeleted;` |

---

## Setters

Primitive types (`id`, `amount`, `categoryId`, `isDeleted`) — direct assignment.

String types (`description`, `date`) — use `strncpy` + force null-terminator at last index.  
Why: members are fixed-size `char[]` arrays, not `std::string`. `strncpy` won't terminate if source is exactly at the limit, so the null must be forced manually.

```cpp
void Transaction::setDescription(const char* newDescription) {
    strncpy(description, newDescription, MAX_DESCRIPTION_LENGTH - 1);
    description[MAX_DESCRIPTION_LENGTH - 1] = '\0';
}

void Transaction::setDate(const char* newDate) {
    strncpy(date, newDate, MAX_DATE_LENGTH - 1);
    date[MAX_DATE_LENGTH - 1] = '\0';
}
```

---

## What is NOT in transaction.cpp

- No constructor — `Transaction` is abstract; subclasses construct it via their own constructors setting the protected members directly.
- No `serialize` / `deserialize` — pure virtual, implemented per subclass.
- No `display` — pure virtual, implemented per subclass.
- No `getEffectiveAmount` / `getType` — pure virtual, implemented per subclass.

---

## File structure

```
transaction.cpp
├── #include "transaction.h"
├── #include <cstring>
├── getId()
├── setId()
├── getDescription()
├── setDescription()       ← strncpy + force null
├── getAmount()
├── setAmount()
├── getDate()
├── setDate()              ← strncpy + force null
├── getCategoryId()
├── setCategoryId()
├── getIsDeleted()
└── setIsDeleted()
```

Total: 12 methods, all simple.
