#pragma once
#include <string>
#include "BankAccount.h"

class AccountRepository;

class CheckingAccount : public BankAccount {

private:
    double overdraftLimit = MAX_OVERDRAFT_LIMIT;

protected:
    CheckingAccount() = default;
    friend class AccountRepository;

public:
    CheckingAccount(unsigned short, const std::string&, double, const std::string&);
    bool        canWithdraw(double) override;
    AccountType getAccountType() const override;
    void serialize(char* buffer) const override;
    void deserialize(const char* buffer) override;
};
