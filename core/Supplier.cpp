#include "Supplier.h"
#include <cstring>
#include <stdexcept>

static void copyField(char* dest, std::size_t capacity, const char* src)
{
    std::memset(dest, 0, capacity);
    if (src == nullptr) return;
    std::strncpy(dest, src, capacity - 1);
    dest[capacity - 1] = '\0';
}

Supplier::Supplier()
    : id(0), balance(), isDeleted(false)
{
    std::memset(name,      0, SUPPLIER_NAME_LENGTH);
    std::memset(email,     0, SUPPLIER_EMAIL_LENGTH);
    std::memset(phone,     0, SUPPLIER_PHONE_LENGTH);
    std::memset(taxNumber, 0, SUPPLIER_TAX_LENGTH);
}

Supplier::Supplier(const SupplierData& info)
{
    if (info.name == nullptr || info.name[0] == '\0')
        throw std::invalid_argument("Supplier name cannot be empty");

    id = info.id;
    copyField(name,      SUPPLIER_NAME_LENGTH,  info.name);
    copyField(email,     SUPPLIER_EMAIL_LENGTH, info.email);
    copyField(phone,     SUPPLIER_PHONE_LENGTH, info.phone);
    copyField(taxNumber, SUPPLIER_TAX_LENGTH,   info.taxNumber);
    balance   = info.balance;
    isDeleted = info.isDeleted;
}

bool Supplier::isValid() const
{
    return name[0] != '\0';
}

// Binary layout (SUPPLIER_RECORD_SIZE = 128 bytes):
//   0..3     id          (uint32_t)        ← widened from 2 bytes in v1
//   4..35    name        (32)
//   36..83   email       (48)
//   84..99   phone       (16)
//   100..115 taxNumber   (16)
//   116..123 balance     (8) int64_t cents
//   124      isDeleted   (1)              ← SUPPLIER_DELETED_OFFSET
//   125..127 padding     (3)
static_assert(SUPPLIER_DELETED_OFFSET == 124, "keep in sync with serialize layout");

void Supplier::serialize(char* buffer) const
{
    if (!isValid())
        throw std::logic_error("Cannot serialize Supplier with empty name");
    std::memset(buffer, 0, SUPPLIER_RECORD_SIZE);
    std::memcpy(buffer + 0,   &id,       sizeof(id));
    std::memcpy(buffer + 4,   name,      SUPPLIER_NAME_LENGTH);
    std::memcpy(buffer + 36,  email,     SUPPLIER_EMAIL_LENGTH);
    std::memcpy(buffer + 84,  phone,     SUPPLIER_PHONE_LENGTH);
    std::memcpy(buffer + 100, taxNumber, SUPPLIER_TAX_LENGTH);
    std::int64_t cents = balance.cents();
    std::memcpy(buffer + 116, &cents,    sizeof(cents));
    unsigned char flag = isDeleted ? 1u : 0u;
    std::memcpy(buffer + 124, &flag,     sizeof(flag));
}

void Supplier::deserialize(const char* buffer)
{
    std::memcpy(&id,       buffer + 0,   sizeof(id));
    std::memcpy(name,      buffer + 4,   SUPPLIER_NAME_LENGTH);
    name[SUPPLIER_NAME_LENGTH - 1] = '\0';
    std::memcpy(email,     buffer + 36,  SUPPLIER_EMAIL_LENGTH);
    email[SUPPLIER_EMAIL_LENGTH - 1] = '\0';
    std::memcpy(phone,     buffer + 84,  SUPPLIER_PHONE_LENGTH);
    phone[SUPPLIER_PHONE_LENGTH - 1] = '\0';
    std::memcpy(taxNumber, buffer + 100, SUPPLIER_TAX_LENGTH);
    taxNumber[SUPPLIER_TAX_LENGTH - 1] = '\0';
    std::int64_t cents;
    std::memcpy(&cents,    buffer + 116, sizeof(cents));
    balance = Money::fromCents(cents);
    unsigned char flag;
    std::memcpy(&flag,     buffer + 124, sizeof(flag));
    isDeleted = (flag != 0);
}

uint32_t Supplier::getId() const            { return id; }
void Supplier::setId(uint32_t v)            { id = v; }

const char* Supplier::getName() const       { return name; }
void Supplier::setName(const char* n)       { copyField(name, SUPPLIER_NAME_LENGTH, n); }

const char* Supplier::getEmail() const      { return email; }
void Supplier::setEmail(const char* e)      { copyField(email, SUPPLIER_EMAIL_LENGTH, e); }

const char* Supplier::getPhone() const      { return phone; }
void Supplier::setPhone(const char* p)      { copyField(phone, SUPPLIER_PHONE_LENGTH, p); }

const char* Supplier::getTaxNumber() const  { return taxNumber; }
void Supplier::setTaxNumber(const char* t)  { copyField(taxNumber, SUPPLIER_TAX_LENGTH, t); }

Money Supplier::getBalance() const          { return balance; }
void Supplier::setBalance(Money m)          { balance = m; }

bool Supplier::getIsDeleted() const         { return isDeleted; }
void Supplier::setIsDeleted(bool v)         { isDeleted = v; }
