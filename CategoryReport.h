#pragma once
#include <unordered_map>
#include <memory>
#include <vector>
#include "Report.h"

class CategoryReport : public Report {
    private:
    std::unordered_map<unsigned short, double> categoryTotals;

    public:
    CategoryReport(unsigned short, unsigned short,
                   std::vector<std::shared_ptr<Transaction>>&);
    void generate() override;
    void display()  override;
    void exportToFile(const std::string& fileName) override;
};
