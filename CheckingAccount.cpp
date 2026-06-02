#include <iostream>
#include "CheckingAccount.h"

bool ChekingAccount::canWithdraw(double money){
    if (money >= MAX_OVERDRAFT_LIMIT){ return true;}
 else return false;
}
AccountType ChekingAccount::getAccountType(){
    return CHECKING;
}