#ifndef TRANSACTION_H
#define TRANSACTION_H
#include "constants.h"
class Transaction
{
    protected:
    struct Transaction_info{
    unsigned short int id;
    char description[MAX_DESCRIPTION_LENGTH];
    double amount;
    char date[MAX_DATE_LENGTH];
    unsigned short int category_id;
    TransactionType type;
    bool isDeleted;
    };
    Transaction_info default_Transaction; 
    public:
    virtual ~Transaction()=default;
    virtual double getEffectiveAmount()=0;
    virtual TransactionType getType()=0;
    virtual void display()=0;
    virtual void serialize()=0;
    virtual void deserialize()=0;
     Transaction_info Transaction_info_getter() const;
     //{return default_Transaction}
    void Transaction_info_setter(const Transaction_info& set_Transaction_info);
    //{deault_Transaction=set_Transaction}
};

#endif
