#include <iostream>
#include "constants.h"
#include "Account.h"

Account::Account(unsigned short int id, const std::string& nname, double iBalance){
    this->id = id;
    balance = iBalance;
    std::strncpy(this->name, nname.c_str(), MAX_NAME_LENGTH-1);
    this->name[sizeof(this->name)-1] = '\0';

    std::time_t now = std::time(nullptr);
    std::tm* localtime = std::localtime(&now);

    size_t bytesWritten = std::strftime(
            this->createdAt, 
            sizeof(this->createdAt), 
            "%Y-%m-%d %H:%M:%S", 
            localtime);
}
double Account::getBalance(){
return balance;
}
unsigned short int Account::getId(){
    return id;
}

