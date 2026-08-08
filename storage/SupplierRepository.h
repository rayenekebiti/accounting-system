#ifndef SUPPLIER_REPOSITORY_H
#define SUPPLIER_REPOSITORY_H
#include <string>   // std::string
#include "BinaryRecordFile.h"
#include "../core/Supplier.h"
#include <vector>
#include <cstring>

class SupplierRepository {
    BinaryRecordFile file_;

    static_assert(SUPPLIER_DELETED_OFFSET < SUPPLIER_RECORD_SIZE,
        "SUPPLIER_DELETED_OFFSET must be within the record");

public:
    explicit SupplierRepository(const std::string& path)
        : file_(path, SUPPLIER_RECORD_SIZE, SUPPLIER_DELETED_OFFSET) {}

    // True if a crash-leftover journal was replayed when the file opened.
    bool recovered() const { return file_.recoveredOnOpen(); }
    // True if a forward schema migration ran when the file opened.
    bool migrated()  const { return file_.migratedOnOpen(); }

    // Event-authored projection support (mirrors CustomerRepository): positional count,
    // content fingerprint, disposable clear, and idempotent write at a stable id.
    std::size_t count() { return file_.count(); }
    uint32_t contentHash() { return file_.contentHash(); }
    void clear() { file_.clear(); }
    void upsertAt(const Supplier& s)
    {
        if (s.getId() < static_cast<uint32_t>(file_.count())) {
            char buf[SUPPLIER_RECORD_SIZE]; s.serialize(buf);
            file_.update(s.getId(), buf);
        } else {
            Supplier tmp = s; save(tmp);   // append at the tail (id == count)
        }
    }

    uint32_t save(Supplier& supplier)
    {
        uint32_t id = static_cast<uint32_t>(file_.count());
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

    bool remove(uint32_t id)
    {
        char buf[SUPPLIER_RECORD_SIZE];
        if (!file_.read(id, buf)) return false;
        unsigned char flag = 1u;
        std::memcpy(buf + SUPPLIER_DELETED_OFFSET, &flag, sizeof(flag));
        return file_.update(id, buf);
    }

    Supplier load(uint32_t id)
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
            if (!file_.read(static_cast<uint32_t>(i), buf)) continue;
            unsigned char flag;
            std::memcpy(&flag, buf + SUPPLIER_DELETED_OFFSET, sizeof(flag));
            if (flag) continue;
            Supplier s;
            s.deserialize(buf);
            if (s.isValid()) result.push_back(s);
        }
        return result;
    }
};

#endif
