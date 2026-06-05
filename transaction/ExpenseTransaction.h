#ifndef EXPENSE_TRANSACTION_H
#define EXPENSE_TRANSACTION_H
#include "transaction.h"

class ExpenseTransaction : public Transaction
{
public:
    ExpenseTransaction(const TransactionData& info);

    double getEffectiveAmount() const override;
    TransactionType getType() const override;
    void display() const override;
    void serialize(char* buffer) const override;
    void deserialize(const char* buffer) override;
};

#endif
