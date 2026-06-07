#ifndef PRODUCT_REPOSITORY_H
#define PRODUCT_REPOSITORY_H
#include "BinaryRecordFile.h"
#include "../core/Product.h"
#include <vector>
#include <cstring>

class ProductRepository {
    BinaryRecordFile file_;
    static constexpr int PROD_DELETED_OFFSET = 166;

public:
    explicit ProductRepository(const std::string& path)
        : file_(path, PRODUCT_RECORD_SIZE) {}

    uint16_t save(Product& product)
    {
        uint16_t id = static_cast<uint16_t>(file_.count());
        product.setId(id);
        char buf[PRODUCT_RECORD_SIZE];
        product.serialize(buf);
        return file_.append(buf);
    }

    bool update(const Product& product)
    {
        char buf[PRODUCT_RECORD_SIZE];
        product.serialize(buf);
        return file_.update(product.getId(), buf);
    }

    bool remove(uint16_t id)
    {
        char buf[PRODUCT_RECORD_SIZE];
        if (!file_.read(id, buf)) return false;
        unsigned char flag = 1u;
        std::memcpy(buf + PROD_DELETED_OFFSET, &flag, sizeof(flag));
        return file_.update(id, buf);
    }

    Product load(uint16_t id)
    {
        char buf[PRODUCT_RECORD_SIZE];
        Product p;
        if (!file_.read(id, buf)) return p;
        p.deserialize(buf);
        return p;
    }

    std::vector<Product> loadAll()
    {
        std::vector<Product> result;
        char buf[PRODUCT_RECORD_SIZE];
        const std::size_t n = file_.count();
        for (std::size_t i = 0; i < n; ++i) {
            if (!file_.read(static_cast<uint16_t>(i), buf)) continue;
            unsigned char flag;
            std::memcpy(&flag, buf + PROD_DELETED_OFFSET, sizeof(flag));
            if (flag) continue;
            Product p;
            p.deserialize(buf);
            if (p.isValid()) result.push_back(p);
        }
        return result;
    }
};

#endif
