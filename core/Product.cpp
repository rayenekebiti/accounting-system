#include "Product.h"
#include <cstring>
#include <stdexcept>

static void copyField(char* dest, std::size_t capacity, const char* src)
{
    std::memset(dest, 0, capacity);
    if (src == nullptr) return;
    std::strncpy(dest, src, capacity - 1);
    dest[capacity - 1] = '\0';
}

Product::Product()
    : id(0), price(), cost(), stock(0), isDeleted(false)
{
    std::memset(code,        0, PRODUCT_CODE_LENGTH);
    std::memset(name,        0, PRODUCT_NAME_LENGTH);
    std::memset(description, 0, PRODUCT_DESC_LENGTH);
}

Product::Product(const ProductData& info)
{
    if (info.name == nullptr || info.name[0] == '\0')
        throw std::invalid_argument("Product name cannot be empty");
    if (info.price.isNegative())
        throw std::invalid_argument("Product price cannot be negative");
    if (info.cost.isNegative())
        throw std::invalid_argument("Product cost cannot be negative");

    id = info.id;
    copyField(code,        PRODUCT_CODE_LENGTH, info.code);
    copyField(name,        PRODUCT_NAME_LENGTH, info.name);
    copyField(description, PRODUCT_DESC_LENGTH, info.description);
    price     = info.price;
    cost      = info.cost;
    stock     = info.stock;
    isDeleted = info.isDeleted;
}

bool Product::isValid() const
{
    return name[0] != '\0' && !price.isNegative() && !cost.isNegative();
}

// Binary layout (PRODUCT_RECORD_SIZE = 192 bytes):
//   0..3     id           (uint32_t)        ← widened from 2 bytes in v1
//   4..19    code         (16)
//   20..83   name         (64)
//   84..147  description  (64)
//   148..155 price        (8) int64_t cents
//   156..163 cost         (8) int64_t cents
//   164..167 stock        (int32_t)
//   168      isDeleted    (1)              ← PRODUCT_DELETED_OFFSET
//   169..191 padding      (23)
static_assert(PRODUCT_DELETED_OFFSET == 168, "keep in sync with serialize layout");
static_assert(4 + 16 + 64 + 64 + 8 + 8 + 4 + 1 <= PRODUCT_RECORD_SIZE,
              "Product fields exceed PRODUCT_RECORD_SIZE");

void Product::serialize(char* buffer) const
{
    if (!isValid())
        throw std::logic_error("Cannot serialize Product with invalid state");
    std::memset(buffer, 0, PRODUCT_RECORD_SIZE);
    std::memcpy(buffer + 0,   &id,         sizeof(id));
    std::memcpy(buffer + 4,   code,        PRODUCT_CODE_LENGTH);
    std::memcpy(buffer + 20,  name,        PRODUCT_NAME_LENGTH);
    std::memcpy(buffer + 84,  description, PRODUCT_DESC_LENGTH);
    std::int64_t priceCents = price.cents();
    std::memcpy(buffer + 148, &priceCents, sizeof(priceCents));
    std::int64_t costCents  = cost.cents();
    std::memcpy(buffer + 156, &costCents,  sizeof(costCents));
    std::memcpy(buffer + 164, &stock,      sizeof(stock));
    unsigned char flag = isDeleted ? 1u : 0u;
    std::memcpy(buffer + 168, &flag,       sizeof(flag));
}

void Product::deserialize(const char* buffer)
{
    std::memcpy(&id,         buffer + 0,   sizeof(id));
    std::memcpy(code,        buffer + 4,   PRODUCT_CODE_LENGTH);
    code[PRODUCT_CODE_LENGTH - 1] = '\0';
    std::memcpy(name,        buffer + 20,  PRODUCT_NAME_LENGTH);
    name[PRODUCT_NAME_LENGTH - 1] = '\0';
    std::memcpy(description, buffer + 84,  PRODUCT_DESC_LENGTH);
    description[PRODUCT_DESC_LENGTH - 1] = '\0';
    std::int64_t priceCents;
    std::memcpy(&priceCents, buffer + 148, sizeof(priceCents));
    price = Money::fromCents(priceCents);
    std::int64_t costCents;
    std::memcpy(&costCents,  buffer + 156, sizeof(costCents));
    cost = Money::fromCents(costCents);
    std::memcpy(&stock,      buffer + 164, sizeof(stock));
    unsigned char flag;
    std::memcpy(&flag,       buffer + 168, sizeof(flag));
    isDeleted = (flag != 0);
}

uint32_t Product::getId() const            { return id; }
void Product::setId(uint32_t v)            { id = v; }

const char* Product::getCode() const       { return code; }
void Product::setCode(const char* c)       { copyField(code, PRODUCT_CODE_LENGTH, c); }

const char* Product::getName() const       { return name; }
void Product::setName(const char* n)       { copyField(name, PRODUCT_NAME_LENGTH, n); }

const char* Product::getDescription() const { return description; }
void Product::setDescription(const char* d) { copyField(description, PRODUCT_DESC_LENGTH, d); }

Money Product::getPrice() const            { return price; }
void Product::setPrice(Money m)            { if (!m.isNegative()) price = m; }

Money Product::getCost() const             { return cost; }
void Product::setCost(Money m)             { if (!m.isNegative()) cost = m; }

int32_t Product::getStock() const          { return stock; }
void Product::setStock(int32_t v)          { stock = v; }

bool Product::getIsDeleted() const         { return isDeleted; }
void Product::setIsDeleted(bool v)         { isDeleted = v; }
