/*
Category
Fields: id, name (max 32 chars), type — either INCOME or EXPENSE (max 8 chars).
Methods: getId(), getName(), getType(), display(), serialize(), deserialize().
No inheritance. Flat class.
*/
#ifndef CATEGORY_H
#define CATEGORY_H
#include <iostream>
#include "constants.h"
class Category{

    private:
    unsigned short int id;
    char name[MAX_NAME_LENGTH];
    char type[8];
    
    public:
    unsigned short int getId();
    std::string getName();
    TransactionType getType();
    void const display();
    void serialize(std::ostream&);
    void deserialize(std::istream&);
    
};
#endif