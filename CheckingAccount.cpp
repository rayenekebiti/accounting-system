#include "CheckingAccount.h"
#include <cstring>

CheckingAccount::CheckingAccount(unsigned short id,
                                 const std::string& accountName,
                                 double initialBalance,
                                 const std::string& bankName)
    : BankAccount(id, accountName, initialBalance, bankName) {}

bool CheckingAccount::canWithdraw(double amount) {
    return (balance - amount) >= overdraftLimit;
}

AccountType CheckingAccount::getAccountType() const { return CHECKING; }

void CheckingAccount::serialize(char* buf) const
{
    BankAccount::serialize(buf);
    std::memcpy(buf + 111, &overdraftLimit, sizeof(overdraftLimit));
}

void CheckingAccount::deserialize(const char* buf)
{
    BankAccount::deserialize(buf);
    std::memcpy(&overdraftLimit, buf + 111, sizeof(overdraftLimit));
}
