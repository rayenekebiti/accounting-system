#ifndef ACCOUNT_H
#define ACCOUNT_H
#include <iostream>
#include "constants.h"
class Account {

    private:
    unsigned short int id;
    char name[MAX_NAME_LENGTH];
    double balance;
    char createdAt[MAX_ACCOUNT_CREATION_DATE_LENGTH];

    public:
    virtual AccountType getAccountType() = 0;
    virtual bool canWithdraw(double) = 0;

    // serialize and deserialize
    
    void deposit(double);
    double withdraw(double);
    unsigned short int getId();
    double getBalance();


};

#endif