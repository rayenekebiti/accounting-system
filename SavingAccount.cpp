#include <iostream>
#include "SavingsAccount.h"
#include <cmath>
bool SavingsAccount::canWithdraw(double money){
 if (money <= getBalance()){ return true;}
 else return false;
}
AccountType SavingsAccount::getAccountType(){
    return SAVINGS;
}
double SavingsAccount::projectedInetrest(int months){
    double intrest = getBalance()*pow((1+intrestRate),months);
}
