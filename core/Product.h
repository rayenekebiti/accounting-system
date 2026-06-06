#ifndef CORE_PRODUCT_H
#define CORE_PRODUCT_H
#include <cstddef>

inline constexpr std::size_t PRODUCT_RECORD_SIZE      = 192;
inline constexpr std::size_t PRODUCT_CODE_LENGTH      = 16;
inline constexpr std::size_t PRODUCT_NAME_LENGTH      = 64;
inline constexpr std::size_t PRODUCT_DESC_LENGTH      = 64;

struct ProductData
{
    unsigned short int id;
    const char*        code;          // SKU
    const char*        name;
    const char*        description;
    double             price;         // selling price (>=0)
    double             cost;          // purchase cost (>=0)
    int                stock;         // current quantity on hand
    bool               isDeleted;
};

class Product
{
    unsigned short int id;
    char               code[PRODUCT_CODE_LENGTH];
    char               name[PRODUCT_NAME_LENGTH];
    char               description[PRODUCT_DESC_LENGTH];
    double             price;
    double             cost;
    int                stock;
    bool               isDeleted;

public:
    Product();
    explicit Product(const ProductData& info);

    bool isValid() const;                       // name non-empty, price/cost >=0

    void serialize(char* buffer) const;
    void deserialize(const char* buffer);
    void display() const;

    unsigned short int getId() const;
    void setId(unsigned short int newId);

    const char* getCode() const;
    void setCode(const char* newCode);

    const char* getName() const;
    void setName(const char* newName);

    const char* getDescription() const;
    void setDescription(const char* newDescription);

    double getPrice() const;
    void setPrice(double newPrice);

    double getCost() const;
    void setCost(double newCost);

    int getStock() const;
    void setStock(int newStock);

    bool getIsDeleted() const;
    void setIsDeleted(bool newIsDeleted);
};

#endif
