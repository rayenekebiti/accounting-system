/*
MonthlyReport
Inherits: Report.
generate() computes: totalIncome, totalExpenses, netBalance from the transaction list.
display() prints a formatted summary to the console.
exportToFile() writes a plain text summary file.
*/


#ifndef MONTHLY_REPORT
#define MONTHLY_REPORT
#include <iostream>
#include  "Report.h"

class MonthlyReport :public Report{

    private:
    std::vector<std::shared_ptr<Transaction>> transactions;
    double totalIncome = 0.0;
    double totalExpenses = 0.0;


    public:
    MonthlyReport(unsigned short, unsigned short, std::vector<std::shared_ptr<Transaction>>&);
    void generate() override;
    void display() override;
    void exportToFile(const std::string&) override;

};



#endif
