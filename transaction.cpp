#include <iostream>
#include "transaction.h"
#include <cstring>

    unsigned short int Transaction::getId() const
    { 
     return id;
    };
    void Transaction::setId(const unsigned short int newId)
    {
     id=newId;
    };
    const char* Transaction::getDescription() const
    {
        return description;
    };
    void Transaction::setDescription(const char* newDescription)
    {
        strncpy(description,newDescription,MAX_DESCRIPTION_LENGTH-1);
        description[MAX_DESCRIPTION_LENGTH-1]='\0';
    };

    double Transaction::getAmount() const
    {
        return amount;
    };
    void Transaction::setAmount(const double newAmount)
    {
     amount=newAmount;
    };

    const char* Transaction::getDate() const
    {
        return date;
    };
    void Transaction::setDate(const char* newDate)
    {
        strncpy(date,newDate,MAX_DATE_LENGTH-1);
        date[MAX_DATE_LENGTH-1]='\0';
    };

    unsigned short int Transaction::getCategoryId() const
    {
        return categoryId;
    };
    void Transaction::setCategoryId(const unsigned short int newCategoryId)
    {
        categoryId=newCategoryId;
    };

    bool Transaction::getIsDeleted() const
    {
        return isDeleted;
    };
    void Transaction::setIsDeleted(const bool newIsDeleted)
    {
     isDeleted=newIsDeleted;
    };

