#ifndef RECURRING_EXPENSE_H
#define RECURRING_EXPENSE_H
#include "ExpenseTransaction.h"

struct RecurringExpense_info
{
    unsigned short int id;
    const char* description;
    double amount;
    const char* date;
    unsigned short int categoryId;
    bool isDeleted;
    int frequencyDays;
    const char* endDate;
};

class RecurringExpense : public ExpenseTransaction
{
private:
    int frequencyDays;
    char endDate[MAX_END_DATE_LENGTH];

public:
    RecurringExpense(const RecurringExpense_info& info);

    bool isDueToday() const;

    TransactionType getType() const override;
    void display() const override;
    void serialize(char* buffer) const override;
    void deserialize(const char* buffer) override;
};

#endif
