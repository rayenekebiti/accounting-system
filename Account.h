#ifndef ACCOUNT_H
#define ACCOUNT_H
#include <iostream>
#include "constants.h"
class Account {

    protected:
    unsigned short int id;
    char name[MAX_NAME_LENGTH];
    double balance;
    char createdAt[MAX_ACCOUNT_CREATION_DATE_LENGTH];

    public:
    Account(unsigned short int, const std::string&, double);
    virtual AccountType getAccountType() = 0;
    virtual bool canWithdraw(double) = 0;
    virtual void serialize(std::ostream&);
    virtual void deserialize(std::istream&);
    virtual void deposit(double) = 0;
    virtual void withdraw(double) = 0;
    virtual unsigned short int getId();
    virtual double getBalance();

};

#endif