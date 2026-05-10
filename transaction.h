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
    virtual ~Transaction()=default;
    virtual double getEffectiveAmount()=0;
    virtual TransactionType getType()=0;
    virtual void display()=0;
    virtual void serialize()=0;
    virtual void deserialize()=0;
    unsigned short int id_getter(unsigned short int& id_get) const;
    void id_setter(const unsigned short int& set_id);
    const char* description_getter() const;
    void description_setter(const char* set_description);
    double amount_getter(double amount_get) const;
    void amount_setter(const double set_amount);
    const char* date_getter() const;
    void date_setter(const char* set_date);
    unsigned short int category_id_getter(unsigned short int category_id_get) const;
    void category_id_setter(const unsigned short int set_category_id);
    TransactionType type_getter(TransactionType type_get) const;
    void type_setter(const TransactionType set_type);
    bool isDeleted_getter(bool isDeleted_get) const;
    void isDeleted_setter(const bool set_isDeleted);
};

#endif
