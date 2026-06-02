#ifndef CASH_ACCOUNT
#define CASH_ACCOUNT
#include <iostream>
#include "constants.h"
#include "Account.h"

class CashAccount: public Account{

    public:
    bool canWithdraw(double )override;
    unsigned short int getId();
    AccountType getAccountType() override;
};

#endif
