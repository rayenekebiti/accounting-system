#include "MonthlyReport.h"

MonthlyReport::MonthlyReport(unsigned short month, unsigned short year, std::vector<std::shared_ptr<Transaction>>& transactions)
  : Report(month, year), transactions(transactions) {

}
void MonthlyReport::generate(){

    for (const auto& transaction : transactions) {
        if (transaction->getAmount() > 0) {
            totalIncome += transaction->getAmount();
        } else {
            totalExpenses += transaction->getAmount();
        }
    }
}
void MonthlyReport::display(){
    std::cout << "Monthly Report for " << month << "/" << year << std::endl;
    std::cout << "Total Income: " << totalIncome << std::endl;
    std::cout << "Total Expenses: " << totalExpenses << std::endl;
    std::cout << "Net Balance: " << (totalIncome + totalExpenses) << std::endl;
}

//exportToFile()
