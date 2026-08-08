#include "ExpenseTransaction.h"
#include <cstring>
#include <stdexcept>
#include <iostream>

ExpenseTransaction::ExpenseTransaction(const TransactionData& info)
{
    if (info.amount < 0)
        throw std::out_of_range("amount can't be negative");
    id = info.id;
    std::strncpy(description, info.description, MAX_DESCRIPTION_LENGTH - 1);
    description[MAX_DESCRIPTION_LENGTH - 1] = '\0';
    amount = -info.amount;
    std::strncpy(date, info.date, MAX_DATE_LENGTH - 1);
    date[MAX_DATE_LENGTH - 1] = '\0';
    categoryId = info.categoryId;
    type = EXPENSE;
    isDeleted = info.isDeleted;
}

double ExpenseTransaction::getEffectiveAmount() const
{
    return amount;
}

TransactionType ExpenseTransaction::getType() const
{
    return EXPENSE;
}

void ExpenseTransaction::display() const
{
    std::cout << "[EXPENSE] ID:" << id
              << " Amount:" << amount
              << " Date:" << date
              << " Desc:" << description
              << " Cat:" << categoryId << "\n";
}

void ExpenseTransaction::serialize(char* buffer) const
{
    std::memset(buffer, 0, TRANSACTION_RECORD_SIZE);
    std::memcpy(buffer + 0,  &id,         sizeof(id));
    std::memcpy(buffer + 4,  description, MAX_DESCRIPTION_LENGTH);
    std::memcpy(buffer + 68, &amount,     sizeof(amount));
    std::memcpy(buffer + 76, date,        MAX_DATE_LENGTH);
    std::memcpy(buffer + 88, &categoryId, sizeof(categoryId));
    int t = static_cast<int>(type);
    std::memcpy(buffer + 92, &t,          sizeof(t));
    std::memcpy(buffer + 96, &isDeleted,  sizeof(isDeleted));
}

void ExpenseTransaction::deserialize(const char* buffer)
{
    std::memcpy(&id,         buffer + 0,  sizeof(id));
    std::memcpy(description, buffer + 4,  MAX_DESCRIPTION_LENGTH);
    description[MAX_DESCRIPTION_LENGTH - 1] = '\0';
    std::memcpy(&amount,     buffer + 68, sizeof(amount));
    std::memcpy(date,        buffer + 76, MAX_DATE_LENGTH);
    date[MAX_DATE_LENGTH - 1] = '\0';
    std::memcpy(&categoryId, buffer + 88, sizeof(categoryId));
    int t;
    std::memcpy(&t,          buffer + 92, sizeof(t));
    type = static_cast<TransactionType>(t);
    std::memcpy(&isDeleted,  buffer + 96, sizeof(isDeleted));
}
