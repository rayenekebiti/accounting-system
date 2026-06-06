#include "Category.h"
#include <cstring>
#include <stdexcept>
#include <iostream>

Category::Category()
    : id(0), type(UNKNOWN), isDeleted(false)
{
    std::memset(name, 0, MAX_CATEGORY_NAME_LENGTH);
}

Category::Category(const CategoryData& info)
{
    if (info.name == nullptr)
        throw std::invalid_argument("Category name cannot be null");
    if (info.type != INCOME && info.type != EXPENSE)
        throw std::invalid_argument("Category type must be INCOME or EXPENSE");

    id = info.id;
    std::memset(name, 0, MAX_CATEGORY_NAME_LENGTH);   // explicit zero before copy
    std::strncpy(name, info.name, MAX_CATEGORY_NAME_LENGTH - 1);
    name[MAX_CATEGORY_NAME_LENGTH - 1] = '\0';
    type = info.type;
    isDeleted = info.isDeleted;
}

bool Category::isValid() const
{
    return type == INCOME || type == EXPENSE;
}

// Binary layout (CATEGORY_RECORD_SIZE = 48 bytes):
//   0..1    id           (unsigned short int)
//   2..33   name         (char[32])
//   34..37  type         (int from enum)
//   38      isDeleted    (bool)
//   39..47  padding      (zero)
void Category::serialize(char* buffer) const
{
    if (!isValid())
        throw std::logic_error("Cannot serialize Category with type UNKNOWN");
    std::memset(buffer, 0, CATEGORY_RECORD_SIZE);
    std::memcpy(buffer + 0,  &id,        sizeof(id));
    std::memcpy(buffer + 2,  name,       MAX_CATEGORY_NAME_LENGTH);
    int t = static_cast<int>(type);
    std::memcpy(buffer + 34, &t,         sizeof(t));
    unsigned char flag = isDeleted ? 1u : 0u;     // avoid bool trap representation
    std::memcpy(buffer + 38, &flag,      sizeof(flag));
}

void Category::deserialize(const char* buffer)
{
    std::memcpy(&id,   buffer + 0,  sizeof(id));
    std::memcpy(name,  buffer + 2,  MAX_CATEGORY_NAME_LENGTH);
    name[MAX_CATEGORY_NAME_LENGTH - 1] = '\0';
    int t;
    std::memcpy(&t,    buffer + 34, sizeof(t));
    // Defensive: clamp unknown values to UNKNOWN rather than a random enum
    TransactionType decoded = static_cast<TransactionType>(t);
    type = (decoded == INCOME || decoded == EXPENSE) ? decoded : UNKNOWN;
    unsigned char flag;
    std::memcpy(&flag, buffer + 38, sizeof(flag));
    isDeleted = (flag != 0);
}

void Category::display() const
{
    const char* typeStr =
        (type == INCOME)  ? "INCOME"  :
        (type == EXPENSE) ? "EXPENSE" : "UNKNOWN";

    std::cout << "[CATEGORY] ID:" << id
              << " Name:" << name
              << " Type:" << typeStr
              << " Deleted:" << (isDeleted ? "yes" : "no") << "\n";
}

unsigned short int Category::getId() const            { return id; }
void Category::setId(unsigned short int newId)        { id = newId; }

const char* Category::getName() const                 { return name; }
void Category::setName(const char* newName)
{
    if (newName == nullptr) return;
    std::strncpy(name, newName, MAX_CATEGORY_NAME_LENGTH - 1);
    name[MAX_CATEGORY_NAME_LENGTH - 1] = '\0';
}

TransactionType Category::getType() const             { return type; }
void Category::setType(TransactionType newType)
{
    if (newType != INCOME && newType != EXPENSE) return;
    type = newType;
}

bool Category::getIsDeleted() const                   { return isDeleted; }
void Category::setIsDeleted(bool newIsDeleted)        { isDeleted = newIsDeleted; }
