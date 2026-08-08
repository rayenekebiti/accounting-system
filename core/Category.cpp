#include "Category.h"
#include <cstring>
#include <stdexcept>

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
    std::memset(name, 0, MAX_CATEGORY_NAME_LENGTH);
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
//   0..3    id           (uint32_t)     ← widened from 2 bytes in v1
//   4..35   name         (char[32])
//   36..39  type         (int32_t from enum)
//   40      isDeleted    (1)            ← CATEGORY_DELETED_OFFSET
//   41..47  padding      (7)
static_assert(CATEGORY_DELETED_OFFSET == 40, "keep in sync with serialize layout");
static_assert(4 + 32 + 4 + 1 <= CATEGORY_RECORD_SIZE,
              "Category fields exceed CATEGORY_RECORD_SIZE");

void Category::serialize(char* buffer) const
{
    if (!isValid())
        throw std::logic_error("Cannot serialize Category with type UNKNOWN");
    std::memset(buffer, 0, CATEGORY_RECORD_SIZE);
    std::memcpy(buffer + 0,  &id,   sizeof(id));
    std::memcpy(buffer + 4,  name,  MAX_CATEGORY_NAME_LENGTH);
    int32_t t = static_cast<int32_t>(type);
    std::memcpy(buffer + 36, &t,    sizeof(t));
    unsigned char flag = isDeleted ? 1u : 0u;
    std::memcpy(buffer + 40, &flag, sizeof(flag));
}

void Category::deserialize(const char* buffer)
{
    std::memcpy(&id,  buffer + 0,  sizeof(id));
    std::memcpy(name, buffer + 4,  MAX_CATEGORY_NAME_LENGTH);
    name[MAX_CATEGORY_NAME_LENGTH - 1] = '\0';
    int32_t t;
    std::memcpy(&t,   buffer + 36, sizeof(t));
    TransactionType decoded = static_cast<TransactionType>(t);
    type = (decoded == INCOME || decoded == EXPENSE) ? decoded : UNKNOWN;
    unsigned char flag;
    std::memcpy(&flag, buffer + 40, sizeof(flag));
    isDeleted = (flag != 0);
}

uint32_t Category::getId() const         { return id; }
void Category::setId(uint32_t v)         { id = v; }

const char* Category::getName() const    { return name; }
void Category::setName(const char* n)
{
    if (n == nullptr) return;
    std::strncpy(name, n, MAX_CATEGORY_NAME_LENGTH - 1);
    name[MAX_CATEGORY_NAME_LENGTH - 1] = '\0';
}

TransactionType Category::getType() const { return type; }
void Category::setType(TransactionType t)
{
    if (t != INCOME && t != EXPENSE) return;
    type = t;
}

bool Category::getIsDeleted() const      { return isDeleted; }
void Category::setIsDeleted(bool v)      { isDeleted = v; }
