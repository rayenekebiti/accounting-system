#ifndef INCOME_TRANSACTION_H
#define INCOME_TRANSACTION_H
#include "transaction.h"
class IncomeTransaction:public Transaction
{
 double getEffectiveAmount() override;
 TransactionType getType() override;
 void display() override;
 void serialize() override;
 void deserialize() override;

};

#endif