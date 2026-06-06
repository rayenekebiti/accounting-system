#include "Report.h"
Report::Report(unsigned short Month, unsigned short Year):month(Month), year(Year){}
Report::~Report(){
    transactions.clear();
}
void Report::addTransaction(Transaction* transPtr){
    transactions.push_back(transPtr);
}
void Report::clearTransactions(){
    transactions.clear();
}


