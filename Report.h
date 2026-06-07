#ifndef REPORT_H
#define REPORT_H
#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include "transaction/transaction.h"
#include "constants.h"

class Report {

    protected:
    unsigned short month;
    unsigned short year;
    std::vector<Transaction*> transactions;

    public:
    Report(unsigned short, unsigned short);
    virtual ~Report();
    void addTransaction(Transaction*);
    void clearTransactions();
    virtual void display()                          = 0;
    virtual void generate()                         = 0;
    virtual void exportToFile(const std::string&)   = 0;
};

#endif
