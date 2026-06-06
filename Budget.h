/*
Budget
Fields: categoryId, monthlyLimit, month (1-12), year.
Methods: isExceeded(currentSpend), getRemainingBudget(currentSpend), display(), serialize(), deserialize().
isExceeded() simply compares currentSpend to monthlyLimit. No side effects.
*/
#ifndef BUDGET_H
#define BUDGET_H
#include <iostream>
#include "constants.h"

class Budget{

    protected:
    unsigned short categoriId;
    const unsigned short monthlyLimit;
    unsigned short month;
    unsigned short year;

    public:
    Budget(unsigned short, unsigned short, unsigned short, unsigned short);
    bool isExceeded(double currentSpend);
    double getRemainingBudget(double currentSpend);
    void display();
    

};
#endif