#ifndef SUPPLIER_REPOSITORY_H
#define SUPPLIER_REPOSITORY_H
#include "BinaryRecordFile.h"
#include "../core/Supplier.h"
#include <vector>
#include <cstring>

class SupplierRepository {
    BinaryRecordFile file_;
    static constexpr int SUPP_DELETED_OFFSET = 122;

public:
    explicit SupplierRepository(const std::string& path)
        : file_(path, SUPPLIER_RECORD_SIZE) {}

    uint16_t save(Supplier& supplier)
    {
        uint16_t id = static_cast<uint16_t>(file_.count());
        supplier.setId(id);
        char buf[SUPPLIER_RECORD_SIZE];
        supplier.serialize(buf);
        return file_.append(buf);
    }

    bool update(const Supplier& supplier)
    {
        char buf[SUPPLIER_RECORD_SIZE];
        supplier.serialize(buf);
        return file_.update(supplier.getId(), buf);
    }

    bool remove(uint16_t id)
    {
        char buf[SUPPLIER_RECORD_SIZE];
        if (!file_.read(id, buf)) return false;
        unsigned char flag = 1u;
        std::memcpy(buf + SUPP_DELETED_OFFSET, &flag, sizeof(flag));
        return file_.update(id, buf);
    }

    Supplier load(uint16_t id)
    {
        char buf[SUPPLIER_RECORD_SIZE];
        Supplier s;
        if (!file_.read(id, buf)) return s;
        s.deserialize(buf);
        return s;
    }

    std::vector<Supplier> loadAll()
    {
        std::vector<Supplier> result;
        char buf[SUPPLIER_RECORD_SIZE];
        const std::size_t n = file_.count();
        for (std::size_t i = 0; i < n; ++i) {
            if (!file_.read(static_cast<uint16_t>(i), buf)) continue;
            unsigned char flag;
            std::memcpy(&flag, buf + SUPP_DELETED_OFFSET, sizeof(flag));
            if (flag) continue;
            Supplier s;
            s.deserialize(buf);
            if (s.isValid()) result.push_back(s);
        }
        return result;
    }
};

#endif
