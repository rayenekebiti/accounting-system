#ifndef TRANSACTION_H
#define TRANSACTION_H
#include <cstdint>
#include "../constants.h"

inline constexpr std::size_t TRANSACTION_DELETED_OFFSET = 96;

struct TransactionData
{
    uint32_t    id;
    const char* description;
    double      amount;
    const char* date;
    uint32_t    categoryId;
    bool        isDeleted;
};

struct RecurringTransactionData : TransactionData
{
    int         frequencyDays;
    const char* endDate;
};

class Transaction
{
protected:
    uint32_t id;
    char description[MAX_DESCRIPTION_LENGTH];
    double amount;
    char date[MAX_DATE_LENGTH];
    uint32_t categoryId;
    TransactionType type;
    bool isDeleted;

public:
    virtual ~Transaction() = default;
    virtual double getEffectiveAmount() const = 0;
    virtual TransactionType getType() const = 0;
    virtual void display() const = 0;
    virtual void serialize(char* buffer) const = 0;
    virtual void deserialize(const char* buffer) = 0;

    uint32_t getId() const;
    void setId(uint32_t newId);

    const char* getDescription() const;
    void setDescription(const char* newDescription);

    double getAmount() const;
    void setAmount(double newAmount);

    const char* getDate() const;
    void setDate(const char* newDate);

    uint32_t getCategoryId() const;
    void setCategoryId(uint32_t newCategoryId);

    bool getIsDeleted() const;
    void setIsDeleted(bool newIsDeleted);
};

#endif
