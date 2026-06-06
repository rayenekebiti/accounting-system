#include "Budget.h"
#include <cstring>
#include <stdexcept>
#include <iostream>

Budget::Budget()
    : id(0), categoryId(0), monthlyLimit(0.0), month(1), year(1970), isDeleted(false)
{}

Budget::Budget(const BudgetData& info)
{
    if (info.monthlyLimit < 0)
        throw std::invalid_argument("Budget monthly limit cannot be negative");
    if (info.month < 1 || info.month > 12)
        throw std::invalid_argument("Budget month must be in 1..12");

    id           = info.id;
    categoryId   = info.categoryId;
    monthlyLimit = info.monthlyLimit;
    month        = info.month;
    year         = info.year;
    isDeleted    = info.isDeleted;
}

bool Budget::isValid() const
{
    return monthlyLimit >= 0 && month >= 1 && month <= 12;
}

// Binary layout (BUDGET_RECORD_SIZE = 32 bytes):
//   0..1    id            (unsigned short int)
//   2..3    categoryId    (unsigned short int)
//   4..11   monthlyLimit  (double)
//   12..13  month         (unsigned short int)
//   14..15  year          (unsigned short int)
//   16      isDeleted     (bool)
//   17..31  padding       (zero)
void Budget::serialize(char* buffer) const
{
    if (!isValid())
        throw std::logic_error("Cannot serialize Budget with invalid state");
    std::memset(buffer, 0, BUDGET_RECORD_SIZE);
    std::memcpy(buffer + 0,  &id,           sizeof(id));
    std::memcpy(buffer + 2,  &categoryId,   sizeof(categoryId));
    std::memcpy(buffer + 4,  &monthlyLimit, sizeof(monthlyLimit));
    std::memcpy(buffer + 12, &month,        sizeof(month));
    std::memcpy(buffer + 14, &year,         sizeof(year));
    unsigned char flag = isDeleted ? 1u : 0u;        // avoid bool trap representation
    std::memcpy(buffer + 16, &flag,         sizeof(flag));
}

void Budget::deserialize(const char* buffer)
{
    std::memcpy(&id,           buffer + 0,  sizeof(id));
    std::memcpy(&categoryId,   buffer + 2,  sizeof(categoryId));
    std::memcpy(&monthlyLimit, buffer + 4,  sizeof(monthlyLimit));
    std::memcpy(&month,        buffer + 12, sizeof(month));
    std::memcpy(&year,         buffer + 14, sizeof(year));
    unsigned char flag;
    std::memcpy(&flag,         buffer + 16, sizeof(flag));
    isDeleted = (flag != 0);
}

void Budget::display() const
{
    std::cout << "[BUDGET] ID:" << id
              << " CategoryID:" << categoryId
              << " Limit:" << monthlyLimit
              << " " << month << "/" << year
              << " Deleted:" << (isDeleted ? "yes" : "no") << "\n";
}

bool Budget::isExceeded(double currentSpend) const
{
    return currentSpend > monthlyLimit;
}

double Budget::getRemainingBudget(double currentSpend) const
{
    return monthlyLimit - currentSpend;
}

unsigned short int Budget::getId() const                   { return id; }
void Budget::setId(unsigned short int newId)               { id = newId; }

unsigned short int Budget::getCategoryId() const           { return categoryId; }
void Budget::setCategoryId(unsigned short int newCategoryId) { categoryId = newCategoryId; }

double Budget::getMonthlyLimit() const                     { return monthlyLimit; }
void Budget::setMonthlyLimit(double newLimit)
{
    if (newLimit < 0) return;
    monthlyLimit = newLimit;
}

unsigned short int Budget::getMonth() const                { return month; }
void Budget::setMonth(unsigned short int newMonth)
{
    if (newMonth < 1 || newMonth > 12) return;
    month = newMonth;
}

unsigned short int Budget::getYear() const                 { return year; }
void Budget::setYear(unsigned short int newYear)           { year = newYear; }

bool Budget::getIsDeleted() const                          { return isDeleted; }
void Budget::setIsDeleted(bool newIsDeleted)               { isDeleted = newIsDeleted; }
