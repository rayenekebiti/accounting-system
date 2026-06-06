#include <iostream>
#include <iomanip>
#include "Category.h"

unsigned short int Category::getId(){
    return id;
}
std::string Category::getName(){
  return std::string(this->name);
}
TransactionType Category::getType(){
std::string stype(this->type);
if(stype == "INCOME"){
    return INCOME;
}
else if(stype == "EXPENSE"){
    return EXPENSE;
} 
else return UNKNOWN; 
}
void const Category::display(){

    std::cout << std::left;
    std::cout << std::setw(8) << this->id;
    std::cout << std::setw(32) << this->name;
    std::cout << std::setw(10) << this->type << std::endl;
}
