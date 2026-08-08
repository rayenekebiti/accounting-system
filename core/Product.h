#ifndef CORE_PRODUCT_H
#define CORE_PRODUCT_H
#include <cstddef>
#include <cstdint>
#include "Money.h"

inline constexpr std::size_t PRODUCT_RECORD_SIZE      = 192;
inline constexpr std::size_t PRODUCT_CODE_LENGTH      = 16;
inline constexpr std::size_t PRODUCT_NAME_LENGTH      = 64;
inline constexpr std::size_t PRODUCT_DESC_LENGTH      = 64;

// Byte offset of the isDeleted flag within a serialized Product record.
inline constexpr std::size_t PRODUCT_DELETED_OFFSET = 168;

struct ProductData
{
    uint32_t    id;
    const char* code;          // SKU
    const char* name;
    const char* description;
    Money       price;         // selling price (>=0)
    Money       cost;          // purchase cost (>=0)
    int32_t     stock;         // current quantity on hand
    bool        isDeleted;
};

class Product
{
    uint32_t id;
    char     code[PRODUCT_CODE_LENGTH];
    char     name[PRODUCT_NAME_LENGTH];
    char     description[PRODUCT_DESC_LENGTH];
    Money    price;
    Money    cost;
    int32_t  stock;
    bool     isDeleted;

public:
    Product();
    explicit Product(const ProductData& info);

    bool isValid() const;                       // name non-empty, price/cost >=0

    void serialize(char* buffer) const;
    void deserialize(const char* buffer);

    uint32_t getId() const;
    void setId(uint32_t newId);

    const char* getCode() const;
    void setCode(const char* newCode);

    const char* getName() const;
    void setName(const char* newName);

    const char* getDescription() const;
    void setDescription(const char* newDescription);

    Money getPrice() const;
    void setPrice(Money newPrice);

    Money getCost() const;
    void setCost(Money newCost);

    int32_t getStock() const;
    void setStock(int32_t newStock);

    bool getIsDeleted() const;
    void setIsDeleted(bool newIsDeleted);
};

#endif
