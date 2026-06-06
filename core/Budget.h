#ifndef CORE_BUDGET_H
#define CORE_BUDGET_H
#include "../constants.h"

struct BudgetData
{
    unsigned short int id;
    unsigned short int categoryId;
    double             monthlyLimit;
    unsigned short int month;       // 1..12
    unsigned short int year;
    bool               isDeleted;
};

class Budget
{
    unsigned short int id;
    unsigned short int categoryId;
    double             monthlyLimit;
    unsigned short int month;
    unsigned short int year;
    bool               isDeleted;

public:
    Budget();                                  // for FileManager deserialize
    explicit Budget(const BudgetData& info);   // validates month, limit

    bool isValid() const;                      // monthlyLimit>=0, month in 1..12

    void serialize(char* buffer) const;        // throws std::logic_error if !isValid()
    void deserialize(const char* buffer);
    void display() const;

    bool   isExceeded(double currentSpend) const;
    double getRemainingBudget(double currentSpend) const;

    unsigned short int getId() const;
    void setId(unsigned short int newId);

    unsigned short int getCategoryId() const;
    void setCategoryId(unsigned short int newCategoryId);

    double getMonthlyLimit() const;
    void setMonthlyLimit(double newLimit);

    unsigned short int getMonth() const;
    void setMonth(unsigned short int newMonth);

    unsigned short int getYear() const;
    void setYear(unsigned short int newYear);

    bool getIsDeleted() const;
    void setIsDeleted(bool newIsDeleted);
};

#endif
