#include "MonthlyReport.h"
#include <fstream>
#include <stdexcept>

MonthlyReport::MonthlyReport(unsigned short month, unsigned short year,
                             std::vector<std::shared_ptr<Transaction>>& source)
    : Report(month, year)
{
    for (const auto& sp : source) addTransaction(sp.get());
}

void MonthlyReport::generate() {
    totalIncome   = 0.0;
    totalExpenses = 0.0;
    for (auto* t : transactions) {
        const double amount = t->getAmount();
        if (amount >= 0) totalIncome   += amount;
        else             totalExpenses += -amount;
    }
}

void MonthlyReport::display() {
    std::cout << "Monthly Report for " << month << "/" << year << "\n"
              << "Total Income:   " << totalIncome   << "\n"
              << "Total Expenses: " << totalExpenses << "\n"
              << "Net Balance:    " << (totalIncome - totalExpenses) << "\n";
}

void MonthlyReport::exportToFile(const std::string& fileName) {
    std::ofstream out(fileName);
    if (!out) throw std::runtime_error("Cannot open " + fileName);
    out << "Monthly Report for " << month << "/" << year << "\n"
        << "Total Income:   " << totalIncome   << "\n"
        << "Total Expenses: " << totalExpenses << "\n"
        << "Net Balance:    " << (totalIncome - totalExpenses) << "\n";
}
