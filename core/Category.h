#ifndef CORE_CATEGORY_H
#define CORE_CATEGORY_H
#include <cstddef>   // std::size_t
#include <cstdint>
#include "../constants.h"

// Byte offset of the isDeleted flag within a serialized Category record.
inline constexpr std::size_t CATEGORY_DELETED_OFFSET = 40;

struct CategoryData
{
    uint32_t        id;
    const char*     name;
    TransactionType type;        // INCOME or EXPENSE only
    bool            isDeleted;
};

class Category
{
    uint32_t        id;
    char            name[MAX_CATEGORY_NAME_LENGTH];
    TransactionType type;
    bool            isDeleted;

public:
    Category();                                   // for FileManager deserialize
    explicit Category(const CategoryData& info);  // validates type, name

    bool isValid() const;                         // type is INCOME or EXPENSE

    void serialize(char* buffer) const;           // throws std::logic_error if !isValid()
    void deserialize(const char* buffer);

    uint32_t getId() const;
    void setId(uint32_t newId);

    const char* getName() const;
    void setName(const char* newName);

    TransactionType getType() const;
    void setType(TransactionType newType);

    bool getIsDeleted() const;
    void setIsDeleted(bool newIsDeleted);
};

#endif
