#ifndef BANK_ACCOUNT
#define BANK_ACCOUNT
#include <iostream>
#include "constants.h"
#include "Account.h"

class BankAccount : public Account{

    protected:
    char bankName[MAX_BANK_NAME_LENGTH];
    unsigned short int accountNumber[MAX_ACCOUNT_NUM_LENGTH];

    public:
    BankAccount(unsigned short int, const std::string&,double, const std::string&);
    AccountType getAccountType()override;
    void deposit(double )override;
    void withdraw(double )override;
};
#endif