#ifndef EXPENSE_TRANSACTION_H
#define EXPENSE_TRANSACTION_H
#include "transaction.h"

class TransactionRepository;

class ExpenseTransaction : public Transaction
{
protected:
    ExpenseTransaction() = default;
    friend class TransactionRepository;
public:
    ExpenseTransaction(const TransactionData& info);

    double getEffectiveAmount() const override;
    TransactionType getType() const override;
    void display() const override;
    void serialize(char* buffer) const override;
    void deserialize(const char* buffer) override;
};

#endif
