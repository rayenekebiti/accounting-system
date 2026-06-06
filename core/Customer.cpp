#include "Customer.h"
#include <cstring>
#include <stdexcept>
#include <iostream>

// Local helper: bounded copy with explicit zero-fill and null termination.
static void copyField(char* dest, std::size_t capacity, const char* src)
{
    std::memset(dest, 0, capacity);
    if (src == nullptr) return;
    std::strncpy(dest, src, capacity - 1);
    dest[capacity - 1] = '\0';
}

Customer::Customer()
    : id(0), balance(0.0), isDeleted(false)
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
    balance = info.balance;
    isDeleted = info.isDeleted;
}

bool Customer::isValid() const
{
    return name[0] != '\0';
}

// Binary layout (CUSTOMER_RECORD_SIZE = 128 bytes):
//   0..1     id
//   2..33    name        (32)
//   34..81   email       (48)
//   82..97   phone       (16)
//   98..113  taxNumber   (16)
//   114..121 balance     (8)
//   122      isDeleted   (1)
//   123..127 padding     (5)
void Customer::serialize(char* buffer) const
{
    if (!isValid())
        throw std::logic_error("Cannot serialize Customer with empty name");
    std::memset(buffer, 0, CUSTOMER_RECORD_SIZE);
    std::memcpy(buffer + 0,   &id,       sizeof(id));
    std::memcpy(buffer + 2,   name,      CUSTOMER_NAME_LENGTH);
    std::memcpy(buffer + 34,  email,     CUSTOMER_EMAIL_LENGTH);
    std::memcpy(buffer + 82,  phone,     CUSTOMER_PHONE_LENGTH);
    std::memcpy(buffer + 98,  taxNumber, CUSTOMER_TAX_LENGTH);
    std::memcpy(buffer + 114, &balance,  sizeof(balance));
    unsigned char flag = isDeleted ? 1u : 0u;
    std::memcpy(buffer + 122, &flag,     sizeof(flag));
}

void Customer::deserialize(const char* buffer)
{
    std::memcpy(&id,      buffer + 0,   sizeof(id));
    std::memcpy(name,     buffer + 2,   CUSTOMER_NAME_LENGTH);
    name[CUSTOMER_NAME_LENGTH - 1] = '\0';
    std::memcpy(email,    buffer + 34,  CUSTOMER_EMAIL_LENGTH);
    email[CUSTOMER_EMAIL_LENGTH - 1] = '\0';
    std::memcpy(phone,    buffer + 82,  CUSTOMER_PHONE_LENGTH);
    phone[CUSTOMER_PHONE_LENGTH - 1] = '\0';
    std::memcpy(taxNumber, buffer + 98, CUSTOMER_TAX_LENGTH);
    taxNumber[CUSTOMER_TAX_LENGTH - 1] = '\0';
    std::memcpy(&balance,  buffer + 114, sizeof(balance));
    unsigned char flag;
    std::memcpy(&flag,     buffer + 122, sizeof(flag));
    isDeleted = (flag != 0);
}

void Customer::display() const
{
    std::cout << "[CUSTOMER] ID:" << id
              << " Name:" << name
              << " Email:" << email
              << " Phone:" << phone
              << " Balance:" << balance
              << " Deleted:" << (isDeleted ? "yes" : "no") << "\n";
}

unsigned short int Customer::getId() const                  { return id; }
void Customer::setId(unsigned short int newId)              { id = newId; }

const char* Customer::getName() const                       { return name; }
void Customer::setName(const char* newName)                 { copyField(name, CUSTOMER_NAME_LENGTH, newName); }

const char* Customer::getEmail() const                      { return email; }
void Customer::setEmail(const char* newEmail)               { copyField(email, CUSTOMER_EMAIL_LENGTH, newEmail); }

const char* Customer::getPhone() const                      { return phone; }
void Customer::setPhone(const char* newPhone)               { copyField(phone, CUSTOMER_PHONE_LENGTH, newPhone); }

const char* Customer::getTaxNumber() const                  { return taxNumber; }
void Customer::setTaxNumber(const char* newTaxNumber)       { copyField(taxNumber, CUSTOMER_TAX_LENGTH, newTaxNumber); }

double Customer::getBalance() const                         { return balance; }
void Customer::setBalance(double newBalance)                { balance = newBalance; }

bool Customer::getIsDeleted() const                         { return isDeleted; }
void Customer::setIsDeleted(bool newIsDeleted)              { isDeleted = newIsDeleted; }
