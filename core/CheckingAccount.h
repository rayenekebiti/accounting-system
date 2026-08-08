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
    CheckingAccount(uint32_t, const std::string&, double, const std::string&);
    bool        canWithdraw(double) override;
    AccountType getAccountType() const override;
    void serialize(char* buffer) const override;
    void deserialize(const char* buffer) override;
};
