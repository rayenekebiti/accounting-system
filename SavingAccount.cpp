#include <iostream>
#include "SavingsAccount.h"
#include <cmath>

SavingsAccount::SavingsAccount(unsigned short int id, const std::string& nname, double iBalance, const std::string& bankNname,
     float intRate):BankAccount(id, nname, iBalance, bankNname){
   intrestRate = intRate;
}
void SavingsAccount::withdraw(double money){
if(canWithdraw){ 
    balance -= money;
    withdrawalsThisMonth += money;
}
 else if(withdrawalsThisMonth > MAX_WITHDRAWAL_LIMIT){
    throw std::out_of_range("You have exceeded your monthly withdrawal limit.");
 }
 else throw std::out_of_range("The amount entered is beyond your actual balance.\n");
}
bool SavingsAccount::canWithdraw(double money){
 if ((money <= getBalance())&& withdrawalsThisMonth <= MAX_WITHDRAWAL_LIMIT ){ return true;}
 else return false;
}
AccountType SavingsAccount::getAccountType(){
    return SAVINGS;
}
double SavingsAccount::projectedInetrest(int months){
    return getBalance()*pow((1+intrestRate),months);
}

