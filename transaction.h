#ifndef TRANSACTION_H
#define TRANSACTION_H
#include "constants.h"
class Transaction
{
    protected:
    unsigned short int id;
    char description[MAX_DESCRIPTION_LENGTH];
    double amount;
    char date[MAX_DATE_LENGTH];
    unsigned short int category_id;
    TransactionType type;
    bool isDeleted;
    public:
    virtual void getEffectiveAmount()=0;
    virtual void getType()=0;
    virtual void display()=0;
    virtual void serialize()=0;
    virtual void deserialize()=0;
    unsigned short int getter(unsigned short int get_Transaction_id) const;
    void setter(const unsigned short int set_Transaction_id);
};

#endif
