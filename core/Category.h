#ifndef CORE_CATEGORY_H
#define CORE_CATEGORY_H
#include "../constants.h"

struct CategoryData
{
    unsigned short int id;
    const char*        name;
    TransactionType    type;        // INCOME or EXPENSE only
    bool               isDeleted;
};

class Category
{
    unsigned short int id;
    char               name[MAX_CATEGORY_NAME_LENGTH];
    TransactionType    type;
    bool               isDeleted;

public:
    Category();                                   // for FileManager deserialize
    explicit Category(const CategoryData& info);  // validates type, name

    bool isValid() const;                         // type is INCOME or EXPENSE

    void serialize(char* buffer) const;           // throws std::logic_error if !isValid()
    void deserialize(const char* buffer);
    void display() const;

    unsigned short int getId() const;
    void setId(unsigned short int newId);

    const char* getName() const;
    void setName(const char* newName);

    TransactionType getType() const;
    void setType(TransactionType newType);

    bool getIsDeleted() const;
    void setIsDeleted(bool newIsDeleted);
};

#endif
