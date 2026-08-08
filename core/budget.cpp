#include "Budget.h"
#include <cstring>
#include <stdexcept>

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
//   0..3    id            (uint32_t)     ← widened from 2 bytes in v1
//   4..7    categoryId    (uint32_t)     ← widened from 2 bytes
//   8..15   monthlyLimit  (double)
//   16..17  month         (uint16_t)
//   18..19  year          (uint16_t)
//   20      isDeleted     (1)            ← BUDGET_DELETED_OFFSET
//   21..31  padding       (11)
static_assert(BUDGET_DELETED_OFFSET == 20, "keep in sync with serialize layout");
static_assert(4 + 4 + 8 + 2 + 2 + 1 <= BUDGET_RECORD_SIZE,
              "Budget fields exceed BUDGET_RECORD_SIZE");

void Budget::serialize(char* buffer) const
{
    if (!isValid())
        throw std::logic_error("Cannot serialize Budget with invalid state");
    std::memset(buffer, 0, BUDGET_RECORD_SIZE);
    std::memcpy(buffer + 0,  &id,           sizeof(id));
    std::memcpy(buffer + 4,  &categoryId,   sizeof(categoryId));
    std::memcpy(buffer + 8,  &monthlyLimit, sizeof(monthlyLimit));
    std::memcpy(buffer + 16, &month,        sizeof(month));
    std::memcpy(buffer + 18, &year,         sizeof(year));
    unsigned char flag = isDeleted ? 1u : 0u;
    std::memcpy(buffer + 20, &flag,         sizeof(flag));
}

void Budget::deserialize(const char* buffer)
{
    std::memcpy(&id,           buffer + 0,  sizeof(id));
    std::memcpy(&categoryId,   buffer + 4,  sizeof(categoryId));
    std::memcpy(&monthlyLimit, buffer + 8,  sizeof(monthlyLimit));
    std::memcpy(&month,        buffer + 16, sizeof(month));
    std::memcpy(&year,         buffer + 18, sizeof(year));
    unsigned char flag;
    std::memcpy(&flag,         buffer + 20, sizeof(flag));
    isDeleted = (flag != 0);
}

bool Budget::isExceeded(double currentSpend) const { return currentSpend > monthlyLimit; }
double Budget::getRemainingBudget(double currentSpend) const { return monthlyLimit - currentSpend; }

uint32_t Budget::getId() const           { return id; }
void Budget::setId(uint32_t v)           { id = v; }

uint32_t Budget::getCategoryId() const   { return categoryId; }
void Budget::setCategoryId(uint32_t v)   { categoryId = v; }

double Budget::getMonthlyLimit() const   { return monthlyLimit; }
void Budget::setMonthlyLimit(double v)   { if (v >= 0) monthlyLimit = v; }

uint16_t Budget::getMonth() const        { return month; }
void Budget::setMonth(uint16_t v)        { if (v >= 1 && v <= 12) month = v; }

uint16_t Budget::getYear() const         { return year; }
void Budget::setYear(uint16_t v)         { year = v; }

bool Budget::getIsDeleted() const        { return isDeleted; }
void Budget::setIsDeleted(bool v)        { isDeleted = v; }
