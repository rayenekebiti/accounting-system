#ifndef CORE_CUSTOMER_H
#define CORE_CUSTOMER_H
#include <cstddef>
#include <cstdint>
#include "Money.h"

inline constexpr std::size_t CUSTOMER_RECORD_SIZE  = 128;
inline constexpr std::size_t CUSTOMER_NAME_LENGTH  = 32;
inline constexpr std::size_t CUSTOMER_EMAIL_LENGTH = 48;
inline constexpr std::size_t CUSTOMER_PHONE_LENGTH = 16;
inline constexpr std::size_t CUSTOMER_TAX_LENGTH   = 16;

// Byte offset of the isDeleted flag within a serialized Customer record.
// Must match Customer::serialize(). Used by CustomerRepository and BinaryRecordFile.
inline constexpr std::size_t CUSTOMER_DELETED_OFFSET = 124;

struct CustomerData
{
    uint32_t    id;
    const char* name;
    const char* email;
    const char* phone;
    const char* taxNumber;
    Money       balance;       // outstanding amount owed by customer
    bool        isDeleted;
};

class Customer
{
    uint32_t id;
    char     name[CUSTOMER_NAME_LENGTH];
    char     email[CUSTOMER_EMAIL_LENGTH];
    char     phone[CUSTOMER_PHONE_LENGTH];
    char     taxNumber[CUSTOMER_TAX_LENGTH];
    Money    balance;
    bool     isDeleted;

public:
    Customer();
    explicit Customer(const CustomerData& info);

    bool isValid() const;                       // name is non-empty

    void serialize(char* buffer) const;         // throws std::logic_error if !isValid()
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
