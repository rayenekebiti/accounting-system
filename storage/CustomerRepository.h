#ifndef CUSTOMER_REPOSITORY_H
#define CUSTOMER_REPOSITORY_H
#include <string>   // std::string
#include "BinaryRecordFile.h"
#include "../core/Customer.h"
#include <vector>
#include <cstring>

class CustomerRepository {
    BinaryRecordFile file_;

    static_assert(CUSTOMER_DELETED_OFFSET < CUSTOMER_RECORD_SIZE,
        "CUSTOMER_DELETED_OFFSET must be within the record");

public:
    explicit CustomerRepository(const std::string& path)
        : file_(path, CUSTOMER_RECORD_SIZE, CUSTOMER_DELETED_OFFSET) {}

    // True if a crash-leftover journal was replayed when the file opened.
    bool recovered() const { return file_.recoveredOnOpen(); }
    // True if a forward schema migration ran when the file opened.
    bool migrated()  const { return file_.migratedOnOpen(); }

    // Positional record count (live + soft-deleted) — the next id to assign.
    std::size_t count() { return file_.count(); }
    // Drop all records (projection rebuild from authoritative history).
    void clear() { file_.clear(); }
    // Deterministic content fingerprint of the projection (for drift detection).
    uint32_t contentHash() { return file_.contentHash(); }
    // Idempotent write at a known id: update if it exists, else append at the tail.
    // Used by the projector so re-applying an event is byte-stable.
    void upsertAt(const Customer& c)
    {
        if (c.getId() < static_cast<uint32_t>(file_.count())) {
            char buf[CUSTOMER_RECORD_SIZE]; c.serialize(buf);
            file_.update(c.getId(), buf);
        } else {
            Customer tmp = c;       // save() assigns id = count(); ids arrive in order
            save(tmp);
        }
    }

    uint32_t save(Customer& customer)
    {
        uint32_t id = static_cast<uint32_t>(file_.count());
        customer.setId(id);
        char buf[CUSTOMER_RECORD_SIZE];
        customer.serialize(buf);
        return file_.append(buf);
    }

    bool update(const Customer& customer)
    {
        char buf[CUSTOMER_RECORD_SIZE];
        customer.serialize(buf);
        return file_.update(customer.getId(), buf);
    }

    bool remove(uint32_t id)
    {
        char buf[CUSTOMER_RECORD_SIZE];
        if (!file_.read(id, buf)) return false;
        unsigned char flag = 1u;
        std::memcpy(buf + CUSTOMER_DELETED_OFFSET, &flag, sizeof(flag));
        return file_.update(id, buf);
    }

    Customer load(uint32_t id)
    {
        char buf[CUSTOMER_RECORD_SIZE];
        Customer c;
        if (!file_.read(id, buf)) return c;
        c.deserialize(buf);
        return c;
    }

    std::vector<Customer> loadAll()
    {
        std::vector<Customer> result;
        char buf[CUSTOMER_RECORD_SIZE];
        const std::size_t n = file_.count();
        for (std::size_t i = 0; i < n; ++i) {
            if (!file_.read(static_cast<uint32_t>(i), buf)) continue;
            unsigned char flag;
            std::memcpy(&flag, buf + CUSTOMER_DELETED_OFFSET, sizeof(flag));
            if (flag) continue;
            Customer c;
            c.deserialize(buf);
            if (c.isValid()) result.push_back(c);
        }
        return result;
    }
};

#endif
