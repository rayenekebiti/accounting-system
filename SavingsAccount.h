#include <iostream>
#include "BankAccount.h"

class SavingsAccount: public BankAccount{

    private:
    float intrestRate;
    double withdrawalsThisMonth;
    double withdrawalLimit = MAX_WITHDRAWAL_LIMIT;

    public:
    SavingsAccount(unsigned short int, const std::string&, double, const std::string&, float);
    void withdraw(double) override;
    bool canWithdraw(double) override;
    double projectedInetrest(int);
    AccountType getAccountType();
};