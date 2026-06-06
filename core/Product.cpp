#include "Product.h"
#include <cstring>
#include <stdexcept>
#include <iostream>

static void copyField(char* dest, std::size_t capacity, const char* src)
{
    std::memset(dest, 0, capacity);
    if (src == nullptr) return;
    std::strncpy(dest, src, capacity - 1);
    dest[capacity - 1] = '\0';
}

Product::Product()
    : id(0), price(0.0), cost(0.0), stock(0), isDeleted(false)
{
    std::memset(code,        0, PRODUCT_CODE_LENGTH);
    std::memset(name,        0, PRODUCT_NAME_LENGTH);
    std::memset(description, 0, PRODUCT_DESC_LENGTH);
}

Product::Product(const ProductData& info)
{
    if (info.name == nullptr || info.name[0] == '\0')
        throw std::invalid_argument("Product name cannot be empty");
    if (info.price < 0)
        throw std::invalid_argument("Product price cannot be negative");
    if (info.cost < 0)
        throw std::invalid_argument("Product cost cannot be negative");

    id = info.id;
    copyField(code,        PRODUCT_CODE_LENGTH, info.code);
    copyField(name,        PRODUCT_NAME_LENGTH, info.name);
    copyField(description, PRODUCT_DESC_LENGTH, info.description);
    price = info.price;
    cost  = info.cost;
    stock = info.stock;
    isDeleted = info.isDeleted;
}

bool Product::isValid() const
{
    return name[0] != '\0' && price >= 0 && cost >= 0;
}

// Binary layout (PRODUCT_RECORD_SIZE = 192 bytes):
//   0..1     id
//   2..17    code         (16)
//   18..81   name         (64)
//   82..145  description  (64)
//   146..153 price        (8)
//   154..161 cost         (8)
//   162..165 stock        (4)
//   166      isDeleted    (1)
//   167..191 padding      (25)
void Product::serialize(char* buffer) const
{
    if (!isValid())
        throw std::logic_error("Cannot serialize Product with invalid state");
    std::memset(buffer, 0, PRODUCT_RECORD_SIZE);
    std::memcpy(buffer + 0,   &id,          sizeof(id));
    std::memcpy(buffer + 2,   code,         PRODUCT_CODE_LENGTH);
    std::memcpy(buffer + 18,  name,         PRODUCT_NAME_LENGTH);
    std::memcpy(buffer + 82,  description,  PRODUCT_DESC_LENGTH);
    std::memcpy(buffer + 146, &price,       sizeof(price));
    std::memcpy(buffer + 154, &cost,        sizeof(cost));
    std::memcpy(buffer + 162, &stock,       sizeof(stock));
    unsigned char flag = isDeleted ? 1u : 0u;
    std::memcpy(buffer + 166, &flag,        sizeof(flag));
}

void Product::deserialize(const char* buffer)
{
    std::memcpy(&id,          buffer + 0,   sizeof(id));
    std::memcpy(code,         buffer + 2,   PRODUCT_CODE_LENGTH);
    code[PRODUCT_CODE_LENGTH - 1] = '\0';
    std::memcpy(name,         buffer + 18,  PRODUCT_NAME_LENGTH);
    name[PRODUCT_NAME_LENGTH - 1] = '\0';
    std::memcpy(description,  buffer + 82,  PRODUCT_DESC_LENGTH);
    description[PRODUCT_DESC_LENGTH - 1] = '\0';
    std::memcpy(&price,       buffer + 146, sizeof(price));
    std::memcpy(&cost,        buffer + 154, sizeof(cost));
    std::memcpy(&stock,       buffer + 162, sizeof(stock));
    unsigned char flag;
    std::memcpy(&flag,        buffer + 166, sizeof(flag));
    isDeleted = (flag != 0);
}

void Product::display() const
{
    std::cout << "[PRODUCT] ID:" << id
              << " Code:" << code
              << " Name:" << name
              << " Price:" << price
              << " Cost:" << cost
              << " Stock:" << stock
              << " Deleted:" << (isDeleted ? "yes" : "no") << "\n";
}

unsigned short int Product::getId() const                   { return id; }
void Product::setId(unsigned short int newId)               { id = newId; }

const char* Product::getCode() const                        { return code; }
void Product::setCode(const char* newCode)                  { copyField(code, PRODUCT_CODE_LENGTH, newCode); }

const char* Product::getName() const                        { return name; }
void Product::setName(const char* newName)                  { copyField(name, PRODUCT_NAME_LENGTH, newName); }

const char* Product::getDescription() const                 { return description; }
void Product::setDescription(const char* newDescription)    { copyField(description, PRODUCT_DESC_LENGTH, newDescription); }

double Product::getPrice() const                            { return price; }
void Product::setPrice(double newPrice)
{
    if (newPrice < 0) return;
    price = newPrice;
}

double Product::getCost() const                             { return cost; }
void Product::setCost(double newCost)
{
    if (newCost < 0) return;
    cost = newCost;
}

int Product::getStock() const                               { return stock; }
void Product::setStock(int newStock)                        { stock = newStock; }

bool Product::getIsDeleted() const                          { return isDeleted; }
void Product::setIsDeleted(bool newIsDeleted)               { isDeleted = newIsDeleted; }
