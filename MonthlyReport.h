#ifndef MONTHLY_REPORT
#define MONTHLY_REPORT
#include <iostream>
#include <memory>
#include <vector>
#include "Report.h"

class MonthlyReport : public Report {

    private:
    double totalIncome   = 0.0;
    double totalExpenses = 0.0;

    public:
    MonthlyReport(unsigned short, unsigned short,
                  std::vector<std::shared_ptr<Transaction>>&);
    void generate()                         override;
    void display()                          override;
    void exportToFile(const std::string&)   override;
};

#endif
