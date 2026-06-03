#ifndef RECURRING_INCOME_H
#define RECURRING_INCOME_H
#include "IncomeTransaction.h"
class RecurringIncome:public IncomeTransaction
{
    int frequencyDays;
    char* EndDate;
    public:
    void isDueToday() const;
    void display() const override;
    void serialize(char* buffer) const override;
    void deserialize(const char* buffer) override;
};

#endif