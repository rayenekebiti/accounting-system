#ifndef INCOME_TRANSACTION_H
#define INCOME_TRANSACTION_H
#include "transaction.h"

class IncomeTransaction : public Transaction
{
public:
    IncomeTransaction(unsigned short int id, const char* description,
                      double amount, const char* date,
                      unsigned short int categoryId);

    double getEffectiveAmount() const override;
    TransactionType getType() const override;
    void display() const override;
    void serialize(char* buffer) const override;
    void deserialize(const char* buffer) override;
};

#endif
