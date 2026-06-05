#ifndef RECURRING_EXPENSE_H
#define RECURRING_EXPENSE_H
#include "ExpenseTransaction.h"

class RecurringExpense : public ExpenseTransaction
{
private:
    int frequencyDays;
    char endDate[MAX_END_DATE_LENGTH];

public:
    RecurringExpense(const RecurringTransactionData& info);

    bool isDueToday() const;

    TransactionType getType() const override;
    void display() const override;
    void serialize(char* buffer) const override;
    void deserialize(const char* buffer) override;
};

#endif
