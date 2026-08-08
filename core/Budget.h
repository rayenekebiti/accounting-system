#ifndef CORE_BUDGET_H
#define CORE_BUDGET_H
#include <cstddef>   // std::size_t
#include <cstdint>
#include "../constants.h"

// Byte offset of the isDeleted flag within a serialized Budget record.
inline constexpr std::size_t BUDGET_DELETED_OFFSET = 20;

struct BudgetData
{
    uint32_t id;
    uint32_t categoryId;
    double   monthlyLimit;
    uint16_t month;       // 1..12
    uint16_t year;
    bool     isDeleted;
};

class Budget
{
    uint32_t id;
    uint32_t categoryId;
    double   monthlyLimit;
    uint16_t month;
    uint16_t year;
    bool     isDeleted;

public:
    Budget();                                  // for FileManager deserialize
    explicit Budget(const BudgetData& info);   // validates month, limit

    bool isValid() const;                      // monthlyLimit>=0, month in 1..12

    void serialize(char* buffer) const;        // throws std::logic_error if !isValid()
    void deserialize(const char* buffer);

    bool   isExceeded(double currentSpend) const;
    double getRemainingBudget(double currentSpend) const;

    uint32_t getId() const;
    void setId(uint32_t newId);

    uint32_t getCategoryId() const;
    void setCategoryId(uint32_t newCategoryId);

    double getMonthlyLimit() const;
    void setMonthlyLimit(double newLimit);

    uint16_t getMonth() const;
    void setMonth(uint16_t newMonth);

    uint16_t getYear() const;
    void setYear(uint16_t newYear);

    bool getIsDeleted() const;
    void setIsDeleted(bool newIsDeleted);
};

#endif
