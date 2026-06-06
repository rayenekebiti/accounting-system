
#include <iostream>
#include "CheckingAccount.h"

    ChekingAccount::ChekingAccount(unsigned short id, const std::string& nname, double iBalance, const std::string& bankNname)
    :BankAccount(id,nname,iBalance,bankName){};

bool ChekingAccount::canWithdraw(double money){
    if (money >= MAX_OVERDRAFT_LIMIT){ return true;}
 else return false;
}
AccountType ChekingAccount::getAccountType(){
    return CHECKING;
}