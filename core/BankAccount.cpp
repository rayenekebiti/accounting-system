#include <iostream>
#include <cstring>
#include "BankAccount.h"

BankAccount::BankAccount(uint32_t id, const std::string& nname,
                         double iBalance, const std::string& bankNname)
    : Account(id, nname, iBalance)
{
    std::memset(bankName,      0, sizeof(bankName));
    std::memset(accountNumber, 0, sizeof(accountNumber));
    std::strncpy(bankName, bankNname.c_str(), MAX_BANK_NAME_LENGTH - 1);
}

AccountType BankAccount::getAccountType() const { return BANK; }

void BankAccount::deposit(double amount) { balance += amount; }
void BankAccount::withdraw(double amount) {
    if (canWithdraw(amount))
        balance -= amount;
    else
        throw std::out_of_range("The amount entered is beyond your actual balance.\n");
}

void BankAccount::serialize(char* buf) const
{
    Account::serialize(buf);
    std::memcpy(buf + 61, bankName,      MAX_BANK_NAME_LENGTH);
    std::memcpy(buf + 93, accountNumber, MAX_ACCOUNT_NUM_LENGTH);
}

void BankAccount::deserialize(const char* buf)
{
    Account::deserialize(buf);
    std::memcpy(bankName,      buf + 61, MAX_BANK_NAME_LENGTH);
    bankName[MAX_BANK_NAME_LENGTH - 1] = '\0';
    std::memcpy(accountNumber, buf + 93, MAX_ACCOUNT_NUM_LENGTH);
    accountNumber[MAX_ACCOUNT_NUM_LENGTH - 1] = '\0';
}
