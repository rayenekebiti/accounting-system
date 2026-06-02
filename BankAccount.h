#ifndef BANK_ACCOUNT
#define BANK_ACCOUNT
#include <iostream>
#include "constants.h"
#include "Account.h"

class BankAccount : public Account{

    private:
    char bankName[MAX_BANK_NAME_LENGTH];
    unsigned short int accountNumber[MAX_ACCOUNT_NUM_LENGTH];

    public:
    AccountType getAccountType()override;
};
#endif