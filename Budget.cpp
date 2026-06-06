#include <iostream>
#include "Budget.h"
Budget::Budget(unsigned short categoryId, unsigned short monthlyLimit, unsigned short month, unsigned short year)
: categoriId(categoryId), monthlyLimit(monthlyLimit), month(month), year(year){};
bool Budget::isExceeded(double currentSpend){
    return currentSpend > monthlyLimit;
}
double Budget::getRemainingBudget(double currentSpend){
    return monthlyLimit - currentSpend;
}
void Budget::display(){
    std::cout << "Category ID: " << categoriId << std::endl;
    std::cout << "Monthly Limit: " << monthlyLimit << std::endl;
    std::cout << "Month: " << month << std::endl;
    std::cout << "Year: " << year << std::endl;
}