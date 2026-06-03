#ifndef TRANSACTION_H
#define TRANSACTION_H
#include "../constants.h"

class Transaction
{
protected:
    unsigned short int id;
    char description[MAX_DESCRIPTION_LENGTH];
    double amount;
    char date[MAX_DATE_LENGTH];
    unsigned short int categoryId;
    TransactionType type;
    bool isDeleted;

public:
    virtual ~Transaction() = default;
    virtual double getEffectiveAmount() const = 0;
    virtual TransactionType getType() const = 0;
    virtual void display() const = 0;
    virtual void serialize(char* buffer) const = 0;
    virtual void deserialize(const char* buffer) = 0;

    unsigned short int getId() const;
    void setId(unsigned short int newId);

    const char* getDescription() const;
    void setDescription(const char* newDescription);

    double getAmount() const;
    void setAmount(double newAmount);

    const char* getDate() const;
    void setDate(const char* newDate);

    unsigned short int getCategoryId() const;
    void setCategoryId(unsigned short int newCategoryId);

    bool getIsDeleted() const;
    void setIsDeleted(bool newIsDeleted);
};

#endif
