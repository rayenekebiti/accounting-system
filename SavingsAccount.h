#pragma once
#include <string>
#include "BankAccount.h"

class AccountRepository;

class SavingsAccount : public BankAccount {

private:
    float  intrestRate;
    double withdrawalsThisMonth;
    double withdrawalLimit = MAX_WITHDRAWAL_LIMIT;

protected:
    SavingsAccount() = default;
    friend class AccountRepository;

public:
    SavingsAccount(unsigned short int, const std::string&, double,
                   const std::string&, float);
    void   withdraw(double) override;
    bool   canWithdraw(double) override;
    double projectedInetrest(int);
    AccountType getAccountType() const override;
    void serialize(char* buffer) const override;
    void deserialize(const char* buffer) override;
};
