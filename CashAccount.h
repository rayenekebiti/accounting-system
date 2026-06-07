#ifndef CASH_ACCOUNT
#define CASH_ACCOUNT
#include <iostream>
#include "constants.h"
#include "Account.h"

class AccountRepository;

class CashAccount: public Account{

protected:
    CashAccount() = default;
    friend class AccountRepository;

public:
    CashAccount(unsigned short int , const std::string&, double);
    bool canWithdraw(double) override;
    unsigned short int getId() const override;
    AccountType getAccountType() const override;
    void deposit(double) override;
    void withdraw(double) override;
    void serialize(char* buffer) const override;
    void deserialize(const char* buffer) override;
};

#endif
