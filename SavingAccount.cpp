#include "SavingsAccount.h"
#include <cmath>
#include <cstring>
#include <stdexcept>

SavingsAccount::SavingsAccount(unsigned short int id, const std::string& name,
                               double initialBalance, const std::string& bankName,
                               float intRate)
    : BankAccount(id, name, initialBalance, bankName),
      intrestRate(intRate),
      withdrawalsThisMonth(0.0) {}

void SavingsAccount::withdraw(double money) {
    if (canWithdraw(money)) {
        balance -= money;
        withdrawalsThisMonth += money;
    } else if (withdrawalsThisMonth > MAX_WITHDRAWAL_LIMIT) {
        throw std::out_of_range("Monthly withdrawal limit exceeded.");
    } else {
        throw std::out_of_range("Withdrawal exceeds current balance.");
    }
}

bool SavingsAccount::canWithdraw(double money) {
    return (money <= getBalance()) && (withdrawalsThisMonth <= MAX_WITHDRAWAL_LIMIT);
}

AccountType SavingsAccount::getAccountType() const { return SAVINGS; }

double SavingsAccount::projectedInetrest(int months) {
    return getBalance() * std::pow(1.0 + intrestRate, months);
}

void SavingsAccount::serialize(char* buf) const
{
    BankAccount::serialize(buf);
    std::memcpy(buf + 111, &intrestRate,          sizeof(intrestRate));
    std::memcpy(buf + 115, &withdrawalsThisMonth, sizeof(withdrawalsThisMonth));
}

void SavingsAccount::deserialize(const char* buf)
{
    BankAccount::deserialize(buf);
    std::memcpy(&intrestRate,          buf + 111, sizeof(intrestRate));
    std::memcpy(&withdrawalsThisMonth, buf + 115, sizeof(withdrawalsThisMonth));
}
