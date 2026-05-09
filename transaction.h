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
    virtual void getEffectiveAmount=0;
    
};

#endif
