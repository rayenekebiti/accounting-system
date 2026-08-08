#ifndef ACCOUNT_H
#define ACCOUNT_H
#include <iostream>
#include <string>
#include <cstdint>
#include "../constants.h"

class AccountRepository;

// Binary layout (ACCOUNT_RECORD_SIZE = 160 bytes):
//   0..3    id          (uint32_t)
//   4..35   name        (char[32])
//   36..43  balance     (double)
//   44..55  createdAt   (char[12])
//   56..59  type        (int from AccountType enum)
//   60      isDeleted   (unsigned char flag)
//   61..92  bankName    (char[32])  — BankAccount+ only
//   93..112 accountNum  (char[20])  — BankAccount+ only
//   113..   subclass-specific fields
inline constexpr std::size_t ACCOUNT_DELETED_OFFSET = 60;

class Account {

protected:
    uint32_t id;
    char   name[MAX_NAME_LENGTH];
    double balance;
    char   createdAt[MAX_ACCOUNT_CREATION_DATE_LENGTH];
    bool   isDeleted;

    Account() = default;
    friend class AccountRepository;

public:
    Account(uint32_t, const std::string&, double);
    virtual ~Account() = default;

    virtual AccountType getAccountType() const = 0;
    virtual bool        canWithdraw(double) = 0;
    virtual void        deposit(double) = 0;
    virtual void        withdraw(double) = 0;
    virtual void        serialize(char* buffer) const;
    virtual void        deserialize(const char* buffer);

    virtual uint32_t    getId() const;
    virtual double      getBalance() const;
    const char*         getName() const { return name; }
    void setId(uint32_t newId);
    bool getIsDeleted() const;
    void setIsDeleted(bool v);
};

#endif
