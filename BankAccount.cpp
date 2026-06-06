#include <iostream>
#include "BankAccount.h"

BankAccount::BankAccount(unsigned short int id, const std::string& nname, double iBalance, const std::string& bankNname):Account(id, nname, iBalance){
    std::strncpy(this->bankName, bankNname.c_str(), MAX_BANK_NAME_LENGTH - 1);
    this->bankName[sizeof(this->bankName) - 1] = '\0';
};

AccountType BankAccount::getAccountType(){
    return BANK;
}

void BankAccount::deposit(double amount){
    balance += amount;
}
void BankAccount::withdraw(double amount){
    if(canWithdraw(amount)) 
    balance -= amount;
    else throw std::out_of_range("The amount entered is beyond your actual balance.\n");
}
