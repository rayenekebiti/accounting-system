#include "Customer.h"
#include <cstring>
#include <stdexcept>

static void copyField(char* dest, std::size_t capacity, const char* src)
{
    std::memset(dest, 0, capacity);
    if (src == nullptr) return;
    std::strncpy(dest, src, capacity - 1);
    dest[capacity - 1] = '\0';
}

Customer::Customer()
    : id(0), balance(), isDeleted(false)
{
    std::memset(name,      0, CUSTOMER_NAME_LENGTH);
    std::memset(email,     0, CUSTOMER_EMAIL_LENGTH);
    std::memset(phone,     0, CUSTOMER_PHONE_LENGTH);
    std::memset(taxNumber, 0, CUSTOMER_TAX_LENGTH);
}

Customer::Customer(const CustomerData& info)
{
    if (info.name == nullptr || info.name[0] == '\0')
        throw std::invalid_argument("Customer name cannot be empty");

    id = info.id;
    copyField(name,      CUSTOMER_NAME_LENGTH,  info.name);
    copyField(email,     CUSTOMER_EMAIL_LENGTH, info.email);
    copyField(phone,     CUSTOMER_PHONE_LENGTH, info.phone);
    copyField(taxNumber, CUSTOMER_TAX_LENGTH,   info.taxNumber);
    balance   = info.balance;
    isDeleted = info.isDeleted;
}

bool Customer::isValid() const
{
    return name[0] != '\0';
}

// Binary layout (CUSTOMER_RECORD_SIZE = 128 bytes):
//   0..3     id          (uint32_t)        ← widened from 2 bytes in v1
//   4..35    name        (32)
//   36..83   email       (48)
//   84..99   phone       (16)
//   100..115 taxNumber   (16)
//   116..123 balance     (8) — int64_t cents
//   124      isDeleted   (1)              ← CUSTOMER_DELETED_OFFSET
//   125..127 padding     (3)
static_assert(CUSTOMER_DELETED_OFFSET == 124, "keep in sync with serialize layout");
static_assert(4 + 32 + 48 + 16 + 16 + 8 + 1 <= CUSTOMER_RECORD_SIZE,
              "Customer fields exceed CUSTOMER_RECORD_SIZE");

void Customer::serialize(char* buffer) const
{
    if (!isValid())
        throw std::logic_error("Cannot serialize Customer with empty name");
    std::memset(buffer, 0, CUSTOMER_RECORD_SIZE);
    std::memcpy(buffer + 0,   &id,       sizeof(id));
    std::memcpy(buffer + 4,   name,      CUSTOMER_NAME_LENGTH);
    std::memcpy(buffer + 36,  email,     CUSTOMER_EMAIL_LENGTH);
    std::memcpy(buffer + 84,  phone,     CUSTOMER_PHONE_LENGTH);
    std::memcpy(buffer + 100, taxNumber, CUSTOMER_TAX_LENGTH);
    std::int64_t cents = balance.cents();
    std::memcpy(buffer + 116, &cents,    sizeof(cents));
    unsigned char flag = isDeleted ? 1u : 0u;
    std::memcpy(buffer + 124, &flag,     sizeof(flag));
}

void Customer::deserialize(const char* buffer)
{
    std::memcpy(&id,      buffer + 0,   sizeof(id));
    std::memcpy(name,     buffer + 4,   CUSTOMER_NAME_LENGTH);
    name[CUSTOMER_NAME_LENGTH - 1] = '\0';
    std::memcpy(email,    buffer + 36,  CUSTOMER_EMAIL_LENGTH);
    email[CUSTOMER_EMAIL_LENGTH - 1] = '\0';
    std::memcpy(phone,    buffer + 84,  CUSTOMER_PHONE_LENGTH);
    phone[CUSTOMER_PHONE_LENGTH - 1] = '\0';
    std::memcpy(taxNumber, buffer + 100, CUSTOMER_TAX_LENGTH);
    taxNumber[CUSTOMER_TAX_LENGTH - 1] = '\0';
    std::int64_t cents;
    std::memcpy(&cents,   buffer + 116, sizeof(cents));
    balance = Money::fromCents(cents);
    unsigned char flag;
    std::memcpy(&flag,    buffer + 124, sizeof(flag));
    isDeleted = (flag != 0);
}

uint32_t Customer::getId() const                 { return id; }
void Customer::setId(uint32_t newId)             { id = newId; }

const char* Customer::getName() const            { return name; }
void Customer::setName(const char* n)            { copyField(name, CUSTOMER_NAME_LENGTH, n); }

const char* Customer::getEmail() const           { return email; }
void Customer::setEmail(const char* e)           { copyField(email, CUSTOMER_EMAIL_LENGTH, e); }

const char* Customer::getPhone() const           { return phone; }
void Customer::setPhone(const char* p)           { copyField(phone, CUSTOMER_PHONE_LENGTH, p); }

const char* Customer::getTaxNumber() const       { return taxNumber; }
void Customer::setTaxNumber(const char* t)       { copyField(taxNumber, CUSTOMER_TAX_LENGTH, t); }

Money Customer::getBalance() const               { return balance; }
void Customer::setBalance(Money m)               { balance = m; }

bool Customer::getIsDeleted() const              { return isDeleted; }
void Customer::setIsDeleted(bool v)              { isDeleted = v; }
