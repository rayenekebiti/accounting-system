#include <iostream>
#include "report.h"

class CategoryReport : public Report {
    public:
    CategoryReport(unsigned short, unsigned short, std::vector<std::shared_ptr<Transaction>>&);
    void generate() override;
    void display() override;
};