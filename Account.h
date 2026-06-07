#ifndef ACCOUNT_H
#define ACCOUNT_H
#include <iostream>
#include <string>
#include "constants.h"

class AccountRepository;

// Binary layout (ACCOUNT_RECORD_SIZE = 160 bytes):
//   0..1    id          (unsigned short int)
//   2..33   name        (char[32])
//   34..41  balance     (double)
//   42..53  createdAt   (char[12])
//   54..57  type        (int from AccountType enum)
//   58      isDeleted   (unsigned char flag)
//   59..90  bankName    (char[32])  — BankAccount+ only
//   91..110 accountNum  (char[20])  — BankAccount+ only
//   111..   subclass-specific fields
class Account {

protected:
    unsigned short int id;
    char   name[MAX_NAME_LENGTH];
    double balance;
    char   createdAt[MAX_ACCOUNT_CREATION_DATE_LENGTH];
    bool   isDeleted;

    Account() = default;
    friend class AccountRepository;

public:
    Account(unsigned short int, const std::string&, double);
    virtual ~Account() = default;

    virtual AccountType getAccountType() const = 0;
    virtual bool        canWithdraw(double) = 0;
    virtual void        deposit(double) = 0;
    virtual void        withdraw(double) = 0;
    virtual void        serialize(char* buffer) const;
    virtual void        deserialize(const char* buffer);

    virtual unsigned short int getId() const;
    virtual double             getBalance() const;
    void setId(unsigned short int newId);
    bool getIsDeleted() const;
    void setIsDeleted(bool v);
};

#endif
