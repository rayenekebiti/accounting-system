#include "CategoryReport.h"
#include <fstream>
#include <stdexcept>

CategoryReport::CategoryReport(unsigned short month, unsigned short year,
                               std::vector<std::shared_ptr<Transaction>>& source)
    : Report(month, year)
{
    for (const auto& sp : source) addTransaction(sp.get());
}

void CategoryReport::generate() {
    categoryTotals.clear();
    for (auto* t : transactions)
        categoryTotals[t->getCategoryId()] += t->getAmount();
}

void CategoryReport::display() {
    std::cout << "Category Report for " << month << "/" << year << "\n"
              << "Category ID | Total Amount\n";
    for (const auto& [id, total] : categoryTotals)
        if (total != 0.0)
            std::cout << id << " | " << total << "\n";
}

void CategoryReport::exportToFile(const std::string& fileName) {
    std::ofstream out(fileName);
    if (!out) throw std::runtime_error("Cannot open " + fileName);
    out << "Category Report for " << month << "/" << year << "\n"
        << "Category ID | Total Amount\n";
    for (const auto& [id, total] : categoryTotals)
        if (total != 0.0)
            out << id << " | " << total << "\n";
}
