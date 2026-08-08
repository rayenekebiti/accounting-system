#ifndef CASH_ACCOUNT
#define CASH_ACCOUNT
#include <string>   // std::string
#include <iostream>
#include "../constants.h"
#include "Account.h"

class AccountRepository;

class CashAccount: public Account{

protected:
    CashAccount() = default;
    friend class AccountRepository;

public:
    CashAccount(uint32_t, const std::string&, double);
    bool canWithdraw(double) override;
    uint32_t getId() const override;
    AccountType getAccountType() const override;
    void deposit(double) override;
    void withdraw(double) override;
    void serialize(char* buffer) const override;
    void deserialize(const char* buffer) override;
};

#endif
