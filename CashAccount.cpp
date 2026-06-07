#include <iostream>
#include <cstring>
#include <ctime>
#include "CashAccount.h"

CashAccount::CashAccount(unsigned short int id, const std::string& nname, double iBalance)
    : Account(id, nname, iBalance) {}

bool CashAccount::canWithdraw(double amount) {
    return amount <= balance;
}
void CashAccount::deposit(double amount) {
    balance += amount;
}
void CashAccount::withdraw(double amount) {
    if (canWithdraw(amount)) {
        balance -= amount;
    } else {
        throw std::out_of_range("The amount entered is beyond your actual balance.\n");
    }
}
unsigned short int CashAccount::getId() const { return id; }
AccountType CashAccount::getAccountType() const { return CASH; }

void CashAccount::serialize(char* buf) const   { Account::serialize(buf); }
void CashAccount::deserialize(const char* buf) { Account::deserialize(buf); }