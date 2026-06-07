#ifndef INCOME_TRANSACTION_H
#define INCOME_TRANSACTION_H
#include "transaction.h"

class TransactionRepository;

class IncomeTransaction : public Transaction
{
protected:
    IncomeTransaction() = default;
    friend class TransactionRepository;
public:
    IncomeTransaction(const TransactionData& info);

    double getEffectiveAmount() const override;
    TransactionType getType() const override;
    void display() const override;
    void serialize(char* buffer) const override;
    void deserialize(const char* buffer) override;
};

#endif
