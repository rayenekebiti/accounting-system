#include <iostream>
#include "BankAccount.h"

class SavingsAccount: public BankAccount{

    private:
    float intrestRate;
    double withdrawalsThisMonth;
    double withdrawalLimit = MAX_WITHDRAWAL_LIMIT;
};