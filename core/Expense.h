#ifndef CORE_EXPENSE_H
#define CORE_EXPENSE_H
#include <cstddef>
#include <cstdint>
#include "Money.h"
#include "IsoDate.h"

inline constexpr std::size_t EXPENSE_RECORD_SIZE = 128;
inline constexpr std::size_t EXPENSE_MEMO_LENGTH = 64;

// Byte offset of the isDeleted flag within a serialized Expense record.
inline constexpr std::size_t EXPENSE_DELETED_OFFSET = 124;

// Sentinel supplierId for "no supplier" (a cash expense with no vendor record).
inline constexpr uint32_t EXPENSE_NO_SUPPLIER = 0xFFFFFFFFu;

// Fixed category set (metadata for filtering/reporting; NOT a per-category ledger account
// in v1 — every expense debits the single "Expenses" account). Values are persisted; never
// renumber. New categories may be appended.
enum ExpenseCategory : uint8_t {
    EXPENSE_CAT_OFFICE    = 0,
    EXPENSE_CAT_RENT      = 1,
    EXPENSE_CAT_UTILITIES = 2,
    EXPENSE_CAT_TRAVEL    = 3,
    EXPENSE_CAT_OTHER     = 4,
};

// Payment method decides the credit side of the posting: Cash → Cr Cash (immediate),
// Credit → Cr Accounts Payable (a payable owed to the supplier).
enum ExpensePaymentMethod : uint8_t {
    EXPENSE_PAY_CASH   = 0,
    EXPENSE_PAY_CREDIT = 1,
};

enum ExpenseStatus : uint8_t {
    EXPENSE_ACTIVE = 0,
    EXPENSE_VOID   = 1,
};

struct ExpenseData
{
    uint32_t    id            = 0;
    uint32_t    supplierId    = EXPENSE_NO_SUPPLIER;
    Money       amount;
    IsoDate     date;
    uint8_t     category      = EXPENSE_CAT_OTHER;
    uint8_t     paymentMethod = EXPENSE_PAY_CASH;
    uint8_t     status        = EXPENSE_ACTIVE;
    int16_t     taxRatePermille = 0;
    const char* memo          = nullptr;
    bool        isDeleted     = false;
};

// Expense — a fixed-size operational record, event-authored + projected like Supplier/Invoice.
// A negative amount is a legitimate reversal (a sign-flipped compensating expense).
class Expense
{
    uint32_t id;
    uint32_t supplierId;
    Money    amount;
    int16_t  year;
    uint8_t  month;
    uint8_t  day;
    uint8_t  category;
    uint8_t  paymentMethod;
    uint8_t  status;
    int16_t  taxRatePermille;   // per-mille input-tax rate on `amount` (net); 0 = no tax
    char     memo[EXPENSE_MEMO_LENGTH];
    bool     isDeleted;

public:
    Expense();
    explicit Expense(const ExpenseData& info);

    bool isValid() const;   // a real expense carries a non-zero amount

    void serialize(char* buffer) const;
    void deserialize(const char* buffer);

    uint32_t getId() const;
    void setId(uint32_t newId);

    uint32_t getSupplierId() const;
    void setSupplierId(uint32_t v);

    Money getAmount() const;
    void setAmount(Money m);

    IsoDate getDate() const;
    void setDate(IsoDate d);

    uint8_t getCategory() const;
    void setCategory(uint8_t c);

    uint8_t getPaymentMethod() const;
    void setPaymentMethod(uint8_t m);

    uint8_t getStatus() const;
    void setStatus(uint8_t s);

    int16_t getTaxRatePermille() const;
    void setTaxRatePermille(int16_t v);

    const char* getMemo() const;
    void setMemo(const char* m);

    bool getIsDeleted() const;
    void setIsDeleted(bool v);
};

#endif
