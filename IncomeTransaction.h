#ifndef INCOME_TRANSACTION_H
#define INCOME_TRANSACTION_H
#include "transaction.h"
struct IncomeTransaction_info
{
unsigned short int id;
const char* description;
double amount;
const char* date;
unsigned short int categoryId;
TransactionType type;
bool isDeleted;
};
class IncomeTransaction : public Transaction
{
public:
    IncomeTransaction(IncomeTransaction_info& info);

    double getEffectiveAmount() const override;
    TransactionType getType() const override;
    void display() const override;
    void serialize(char* buffer) const override;
    void deserialize(const char* buffer) override;
};

#endif
