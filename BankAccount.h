#ifndef BANK_ACCOUNT
#define BANK_ACCOUNT
#include <iostream>
#include <string>
#include "constants.h"
#include "Account.h"

class BankAccount : public Account {

protected:
    char bankName[MAX_BANK_NAME_LENGTH];
    char accountNumber[MAX_ACCOUNT_NUM_LENGTH];

    BankAccount() = default;

public:
    BankAccount(unsigned short int, const std::string&, double, const std::string&);
    AccountType getAccountType() const override;
    void deposit(double) override;
    void withdraw(double) override;
    void serialize(char* buffer) const override;
    void deserialize(const char* buffer) override;
};

#endif
