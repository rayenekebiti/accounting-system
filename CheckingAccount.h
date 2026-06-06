#include <iostream>
#include "BankAccount.h"

class ChekingAccount :public BankAccount{

    private:
    double overdraftLimit = MAX_OVERDRAFT_LIMIT;
    
    public:
    ChekingAccount(unsigned short, const std::string&, double, const std::string&);
    bool canWithdraw(double) override;
    AccountType getAccountType() override;

};