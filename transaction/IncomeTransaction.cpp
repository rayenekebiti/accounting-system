#include "IncomeTransaction.h"
#include <cstring>
#include <stdexcept>
#include <iostream>

IncomeTransaction::IncomeTransaction(const TransactionData& info)
{
    if (info.amount < 0)
        throw std::out_of_range("amount can't be negative");
    id = info.id;
    std::strncpy(description, info.description, MAX_DESCRIPTION_LENGTH - 1);
    description[MAX_DESCRIPTION_LENGTH - 1] = '\0';
    amount = info.amount;
    std::strncpy(date, info.date, MAX_DATE_LENGTH - 1);
    date[MAX_DATE_LENGTH - 1] = '\0';
    categoryId = info.categoryId;
    type = INCOME;
    isDeleted = info.isDeleted;
}

double IncomeTransaction::getEffectiveAmount() const
{
    return amount;
}

TransactionType IncomeTransaction::getType() const
{
    return INCOME;
}

void IncomeTransaction::display() const
{
    std::cout << "[INCOME] ID:" << id
              << " Amount:" << amount
              << " Date:" << date
              << " Desc:" << description
              << " Cat:" << categoryId << "\n";
}

void IncomeTransaction::serialize(char* buffer) const
{
    std::memset(buffer, 0, TRANSACTION_RECORD_SIZE);
    std::memcpy(buffer + 0,  &id,         sizeof(id));
    std::memcpy(buffer + 2,  description, MAX_DESCRIPTION_LENGTH);
    std::memcpy(buffer + 66, &amount,     sizeof(amount));
    std::memcpy(buffer + 74, date,        MAX_DATE_LENGTH);
    std::memcpy(buffer + 86, &categoryId, sizeof(categoryId));
    int t = static_cast<int>(type);
    std::memcpy(buffer + 88, &t,          sizeof(t));
    std::memcpy(buffer + 92, &isDeleted,  sizeof(isDeleted));
}

void IncomeTransaction::deserialize(const char* buffer)
{
    std::memcpy(&id,         buffer + 0,  sizeof(id));
    std::memcpy(description, buffer + 2,  MAX_DESCRIPTION_LENGTH);
    description[MAX_DESCRIPTION_LENGTH - 1] = '\0';
    std::memcpy(&amount,     buffer + 66, sizeof(amount));
    std::memcpy(date,        buffer + 74, MAX_DATE_LENGTH);
    date[MAX_DATE_LENGTH - 1] = '\0';
    std::memcpy(&categoryId, buffer + 86, sizeof(categoryId));
    int t;
    std::memcpy(&t,          buffer + 88, sizeof(t));
    type = static_cast<TransactionType>(t);
    std::memcpy(&isDeleted,  buffer + 92, sizeof(isDeleted));
}
