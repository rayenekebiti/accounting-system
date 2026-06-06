#include "CategoryReport.h"
#include <vector>
CategoryReport::CategoryReport(unsigned short month, unsigned short year, std::vector<std::shared_ptr<Transaction>>& transactions)
  : Report(month, year) {
    for (const auto& transaction : transactions) {
        addTransaction(transaction.get());
    }
}
    std::vector<double> categoryTotals(100, 0.0); // Assuming a maximum of 100 categories

void CategoryReport::generate(){
    for (const auto& transaction : transactions) {
        unsigned short categoryId = transaction->getCategoryId();
        if (categoryId < categoryTotals.size()) {
            categoryTotals[categoryId] += transaction->getAmount();
        }
    }
}
void CategoryReport::display() {
    std::cout << "Category Report for " << month << "/" << year << std::endl;
    std::cout << "Category ID | Total Amount" << std::endl;
    for (size_t i = 0; i < categoryTotals.size(); ++i) {
        if (categoryTotals[i] != 0) {
            std::cout << i << " | " << categoryTotals[i] << std::endl;
        }
    }
}
