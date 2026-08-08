#ifndef CORE_SUPPLIER_H
#define CORE_SUPPLIER_H
#include <cstddef>
#include <cstdint>
#include "Money.h"

inline constexpr std::size_t SUPPLIER_RECORD_SIZE  = 128;
inline constexpr std::size_t SUPPLIER_NAME_LENGTH  = 32;
inline constexpr std::size_t SUPPLIER_EMAIL_LENGTH = 48;
inline constexpr std::size_t SUPPLIER_PHONE_LENGTH = 16;
inline constexpr std::size_t SUPPLIER_TAX_LENGTH   = 16;

// Byte offset of the isDeleted flag within a serialized Supplier record.
inline constexpr std::size_t SUPPLIER_DELETED_OFFSET = 124;

struct SupplierData
{
    uint32_t    id;
    const char* name;
    const char* email;
    const char* phone;
    const char* taxNumber;
    Money       balance;       // outstanding amount owed to supplier
    bool        isDeleted;
};

class Supplier
{
    uint32_t id;
    char     name[SUPPLIER_NAME_LENGTH];
    char     email[SUPPLIER_EMAIL_LENGTH];
    char     phone[SUPPLIER_PHONE_LENGTH];
    char     taxNumber[SUPPLIER_TAX_LENGTH];
    Money    balance;
    bool     isDeleted;

public:
    Supplier();
    explicit Supplier(const SupplierData& info);

    bool isValid() const;

    void serialize(char* buffer) const;
    void deserialize(const char* buffer);

    uint32_t getId() const;
    void setId(uint32_t newId);

    const char* getName() const;
    void setName(const char* newName);

    const char* getEmail() const;
    void setEmail(const char* newEmail);

    const char* getPhone() const;
    void setPhone(const char* newPhone);

    const char* getTaxNumber() const;
    void setTaxNumber(const char* newTaxNumber);

    Money getBalance() const;
    void setBalance(Money newBalance);

    bool getIsDeleted() const;
    void setIsDeleted(bool newIsDeleted);
};

#endif
