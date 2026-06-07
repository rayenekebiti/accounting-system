#ifndef RECURRING_EXPENSE_H
#define RECURRING_EXPENSE_H
#include "ExpenseTransaction.h"

class TransactionRepository;

class RecurringExpense : public ExpenseTransaction
{
private:
    int frequencyDays;
    char endDate[MAX_END_DATE_LENGTH];

protected:
    RecurringExpense() = default;
    friend class TransactionRepository;
public:
    RecurringExpense(const RecurringTransactionData& info);

    bool isDueToday() const;

    TransactionType getType() const override;
    void display() const override;
    void serialize(char* buffer) const override;
    void deserialize(const char* buffer) override;
};

#endif
