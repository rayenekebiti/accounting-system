#include "Expense.h"
#include <cstring>
#include <stdexcept>

static void copyField(char* dest, std::size_t capacity, const char* src)
{
    std::memset(dest, 0, capacity);
    if (src == nullptr) return;
    std::strncpy(dest, src, capacity - 1);
    dest[capacity - 1] = '\0';
}

Expense::Expense()
    : id(0), supplierId(EXPENSE_NO_SUPPLIER), amount(), year(0), month(0), day(0),
      category(EXPENSE_CAT_OTHER), paymentMethod(EXPENSE_PAY_CASH), status(EXPENSE_ACTIVE),
      taxRatePermille(0), isDeleted(false)
{
    std::memset(memo, 0, EXPENSE_MEMO_LENGTH);
}

Expense::Expense(const ExpenseData& info)
    : id(info.id), supplierId(info.supplierId), amount(info.amount),
      year(static_cast<int16_t>(info.date.year())),
      month(static_cast<uint8_t>(info.date.month())),
      day(static_cast<uint8_t>(info.date.day())),
      category(info.category), paymentMethod(info.paymentMethod), status(info.status),
      taxRatePermille(info.taxRatePermille), isDeleted(info.isDeleted)
{
    copyField(memo, EXPENSE_MEMO_LENGTH, info.memo);
}

bool Expense::isValid() const
{
    return amount.cents() != 0;   // a real expense (or reversal) carries a non-zero amount
}

// Binary layout (EXPENSE_RECORD_SIZE = 128 bytes):
//   0..3     id            (uint32_t)
//   4..7     supplierId    (uint32_t)   0xFFFFFFFF = none
//   8..15    amountCents   (int64_t)    signed (negative = reversal)
//   16..17   year          (int16_t)
//   18       month         (uint8_t)
//   19       day           (uint8_t)
//   20       category      (uint8_t)
//   21       paymentMethod (uint8_t)
//   22       status        (uint8_t)
//   23       padding       (1)
//   24..87   memo          (64)
//   88..89   taxRatePermille (int16_t)  ← added in the reserved region (record size UNCHANGED;
//                                          old records read 0 here → no tax, backward-compatible)
//   90..123  reserved      (34)
//   124      isDeleted     (1)          ← EXPENSE_DELETED_OFFSET
//   125..127 padding       (3)
static_assert(EXPENSE_DELETED_OFFSET == 124, "keep in sync with serialize layout");

void Expense::serialize(char* buffer) const
{
    if (!isValid())
        throw std::logic_error("Cannot serialize Expense with a zero amount");
    std::memset(buffer, 0, EXPENSE_RECORD_SIZE);
    std::memcpy(buffer + 0,  &id,         sizeof(id));
    std::memcpy(buffer + 4,  &supplierId, sizeof(supplierId));
    std::int64_t cents = amount.cents();
    std::memcpy(buffer + 8,  &cents,      sizeof(cents));
    std::memcpy(buffer + 16, &year,       sizeof(year));
    buffer[18] = static_cast<char>(month);
    buffer[19] = static_cast<char>(day);
    buffer[20] = static_cast<char>(category);
    buffer[21] = static_cast<char>(paymentMethod);
    buffer[22] = static_cast<char>(status);
    std::memcpy(buffer + 24, memo, EXPENSE_MEMO_LENGTH);
    std::memcpy(buffer + 88, &taxRatePermille, sizeof(taxRatePermille));
    unsigned char flag = isDeleted ? 1u : 0u;
    std::memcpy(buffer + EXPENSE_DELETED_OFFSET, &flag, sizeof(flag));
}

void Expense::deserialize(const char* buffer)
{
    std::memcpy(&id,         buffer + 0, sizeof(id));
    std::memcpy(&supplierId, buffer + 4, sizeof(supplierId));
    std::int64_t cents;
    std::memcpy(&cents,      buffer + 8, sizeof(cents));
    amount = Money::fromCents(cents);
    std::memcpy(&year,       buffer + 16, sizeof(year));
    month         = static_cast<uint8_t>(buffer[18]);
    day           = static_cast<uint8_t>(buffer[19]);
    category      = static_cast<uint8_t>(buffer[20]);
    paymentMethod = static_cast<uint8_t>(buffer[21]);
    status        = static_cast<uint8_t>(buffer[22]);
    std::memcpy(memo, buffer + 24, EXPENSE_MEMO_LENGTH);
    memo[EXPENSE_MEMO_LENGTH - 1] = '\0';
    std::memcpy(&taxRatePermille, buffer + 88, sizeof(taxRatePermille));
    unsigned char flag;
    std::memcpy(&flag, buffer + EXPENSE_DELETED_OFFSET, sizeof(flag));
    isDeleted = (flag != 0);
}

uint32_t Expense::getId() const           { return id; }
void Expense::setId(uint32_t v)           { id = v; }

uint32_t Expense::getSupplierId() const   { return supplierId; }
void Expense::setSupplierId(uint32_t v)   { supplierId = v; }

Money Expense::getAmount() const          { return amount; }
void Expense::setAmount(Money m)          { amount = m; }

IsoDate Expense::getDate() const
{
    auto d = IsoDate::tryMake(year, month, day);
    return d ? *d : IsoDate{};
}
void Expense::setDate(IsoDate d)
{
    year  = static_cast<int16_t>(d.year());
    month = static_cast<uint8_t>(d.month());
    day   = static_cast<uint8_t>(d.day());
}

uint8_t Expense::getCategory() const      { return category; }
void Expense::setCategory(uint8_t c)      { category = c; }

uint8_t Expense::getPaymentMethod() const { return paymentMethod; }
void Expense::setPaymentMethod(uint8_t m) { paymentMethod = m; }

uint8_t Expense::getStatus() const        { return status; }
void Expense::setStatus(uint8_t s)        { status = s; }

int16_t Expense::getTaxRatePermille() const { return taxRatePermille; }
void Expense::setTaxRatePermille(int16_t v) { taxRatePermille = v; }

const char* Expense::getMemo() const      { return memo; }
void Expense::setMemo(const char* m)      { copyField(memo, EXPENSE_MEMO_LENGTH, m); }

bool Expense::getIsDeleted() const        { return isDeleted; }
void Expense::setIsDeleted(bool v)        { isDeleted = v; }
