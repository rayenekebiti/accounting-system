#include "Supplier.h"
#include <cstring>
#include <stdexcept>
#include <iostream>

static void copyField(char* dest, std::size_t capacity, const char* src)
{
    std::memset(dest, 0, capacity);
    if (src == nullptr) return;
    std::strncpy(dest, src, capacity - 1);
    dest[capacity - 1] = '\0';
}

Supplier::Supplier()
    : id(0), balance(0.0), isDeleted(false)
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
    balance = info.balance;
    isDeleted = info.isDeleted;
}

bool Supplier::isValid() const
{
    return name[0] != '\0';
}

// Binary layout (SUPPLIER_RECORD_SIZE = 128 bytes):
//   0..1     id
//   2..33    name        (32)
//   34..81   email       (48)
//   82..97   phone       (16)
//   98..113  taxNumber   (16)
//   114..121 balance     (8)
//   122      isDeleted   (1)
//   123..127 padding     (5)
void Supplier::serialize(char* buffer) const
{
    if (!isValid())
        throw std::logic_error("Cannot serialize Supplier with empty name");
    std::memset(buffer, 0, SUPPLIER_RECORD_SIZE);
    std::memcpy(buffer + 0,   &id,       sizeof(id));
    std::memcpy(buffer + 2,   name,      SUPPLIER_NAME_LENGTH);
    std::memcpy(buffer + 34,  email,     SUPPLIER_EMAIL_LENGTH);
    std::memcpy(buffer + 82,  phone,     SUPPLIER_PHONE_LENGTH);
    std::memcpy(buffer + 98,  taxNumber, SUPPLIER_TAX_LENGTH);
    std::memcpy(buffer + 114, &balance,  sizeof(balance));
    unsigned char flag = isDeleted ? 1u : 0u;
    std::memcpy(buffer + 122, &flag,     sizeof(flag));
}

void Supplier::deserialize(const char* buffer)
{
    std::memcpy(&id,       buffer + 0,   sizeof(id));
    std::memcpy(name,      buffer + 2,   SUPPLIER_NAME_LENGTH);
    name[SUPPLIER_NAME_LENGTH - 1] = '\0';
    std::memcpy(email,     buffer + 34,  SUPPLIER_EMAIL_LENGTH);
    email[SUPPLIER_EMAIL_LENGTH - 1] = '\0';
    std::memcpy(phone,     buffer + 82,  SUPPLIER_PHONE_LENGTH);
    phone[SUPPLIER_PHONE_LENGTH - 1] = '\0';
    std::memcpy(taxNumber, buffer + 98,  SUPPLIER_TAX_LENGTH);
    taxNumber[SUPPLIER_TAX_LENGTH - 1] = '\0';
    std::memcpy(&balance,  buffer + 114, sizeof(balance));
    unsigned char flag;
    std::memcpy(&flag,     buffer + 122, sizeof(flag));
    isDeleted = (flag != 0);
}

void Supplier::display() const
{
    std::cout << "[SUPPLIER] ID:" << id
              << " Name:" << name
              << " Email:" << email
              << " Phone:" << phone
              << " Balance:" << balance
              << " Deleted:" << (isDeleted ? "yes" : "no") << "\n";
}

unsigned short int Supplier::getId() const                  { return id; }
void Supplier::setId(unsigned short int newId)              { id = newId; }

const char* Supplier::getName() const                       { return name; }
void Supplier::setName(const char* newName)                 { copyField(name, SUPPLIER_NAME_LENGTH, newName); }

const char* Supplier::getEmail() const                      { return email; }
void Supplier::setEmail(const char* newEmail)               { copyField(email, SUPPLIER_EMAIL_LENGTH, newEmail); }

const char* Supplier::getPhone() const                      { return phone; }
void Supplier::setPhone(const char* newPhone)               { copyField(phone, SUPPLIER_PHONE_LENGTH, newPhone); }

const char* Supplier::getTaxNumber() const                  { return taxNumber; }
void Supplier::setTaxNumber(const char* newTaxNumber)       { copyField(taxNumber, SUPPLIER_TAX_LENGTH, newTaxNumber); }

double Supplier::getBalance() const                         { return balance; }
void Supplier::setBalance(double newBalance)                { balance = newBalance; }

bool Supplier::getIsDeleted() const                         { return isDeleted; }
void Supplier::setIsDeleted(bool newIsDeleted)              { isDeleted = newIsDeleted; }
