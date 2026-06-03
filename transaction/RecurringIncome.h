#ifndef RECURRING_INCOME_H
#define RECURRING_INCOME_H
#include "IncomeTransaction.h"

struct RecurringIncome_info
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

class RecurringIncome : public IncomeTransaction
{
private:
    int frequencyDays;
    char endDate[MAX_END_DATE_LENGTH];

public:
    RecurringIncome(const RecurringIncome_info& info);

    bool isDueToday() const;

    TransactionType getType() const override;
    void display() const override;
    void serialize(char* buffer) const override;
    void deserialize(const char* buffer) override;
};

#endif
