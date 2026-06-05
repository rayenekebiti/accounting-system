#ifndef RECURRING_INCOME_H
#define RECURRING_INCOME_H
#include "IncomeTransaction.h"

class RecurringIncome : public IncomeTransaction
{
private:
    int frequencyDays;
    char endDate[MAX_END_DATE_LENGTH];

public:
    RecurringIncome(const RecurringTransactionData& info);

    bool isDueToday() const;

    TransactionType getType() const override;
    void display() const override;
    void serialize(char* buffer) const override;
    void deserialize(const char* buffer) override;
};

#endif
