#include <iostream>
#include "CashAccount.h"

bool CashAccount::canWithdraw(double amount){
 if (amount <= getBalance()){ return true;}
 else return false;
}
unsigned short int CashAccount::getId(){
return Account::getId();
}
AccountType CashAccount::getAccountType(){
    return CASH;
}