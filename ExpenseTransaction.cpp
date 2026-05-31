#include "ExpenseTransaction.h"
#include <cstring>
#include <stdexcept>

ExpenseTransaction::ExpenseTransaction(ExpenseTransaction_info& info)
{
    this->id = info.id;
    std::strncpy(this->description, info.description, MAX_DESCRIPTION_LENGTH - 1);
    this->description[MAX_DESCRIPTION_LENGTH - 1] = '\0';
    if (info.amount < 0)
        throw std::out_of_range("amount can't be negative");
    this->amount = -info.amount;
    std::strncpy(this->date, info.date, MAX_DATE_LENGTH - 1);
    this->date[MAX_DATE_LENGTH - 1] = '\0';
    this->categoryId = info.categoryId;
    this->type = info.type;
    this->isDeleted = info.isDeleted;
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

}

void ExpenseTransaction::serialize(char* buffer) const
{

}

void ExpenseTransaction::deserialize(const char* buffer)
{

}
