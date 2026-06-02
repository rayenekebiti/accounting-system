#include <iostream>
#include "BankAccount.h"

class ChekingAccount :public BankAccount{

    private:
    double overdraftLimit = MAX_OVERDRAFT_LIMIT;
    
    public:
    bool canWithdraw(double) override;
    AccountType getAccountType() override;

};