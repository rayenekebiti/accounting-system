#ifndef EXPENSE_TRANSACTION_H
#define EXPENSE_TRANSACTION_H
#include "transaction.h"

struct ExpenseTransaction_info
{
    unsigned short int id;
    const char* description;
    double amount;
    const char* date;
    unsigned short int categoryId;
    bool isDeleted;
};

class ExpenseTransaction : public Transaction
{
public:
    ExpenseTransaction(const ExpenseTransaction_info& info);

    double getEffectiveAmount() const override;
    TransactionType getType() const override;
    void display() const override;
    void serialize(char* buffer) const override;
    void deserialize(const char* buffer) override;
};

#endif
