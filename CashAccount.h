#ifndef CASH_ACCOUNT
#define CASH_ACCOUNT
#include <iostream>
#include "constants.h"
#include "Account.h"

class CashAccount: public Account{

    public:
    CashAccount(unsigned short int , const std::string&, double);
    bool canWithdraw(double )override;
    unsigned short int getId();
    AccountType getAccountType() override;
    void deposit(double)override;
    void withdraw(double)override;
};

#endif
