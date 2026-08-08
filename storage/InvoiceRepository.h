#ifndef INVOICE_REPOSITORY_H
#define INVOICE_REPOSITORY_H
#include "BinaryRecordFile.h"
#include "../core/Invoice.h"
#include <vector>
#include <cstring>

class InvoiceRepository {
    BinaryRecordFile file_;

    static_assert(INVOICE_DELETED_OFFSET < INVOICE_RECORD_SIZE,
        "INVOICE_DELETED_OFFSET must be within the record");

public:
    explicit InvoiceRepository(const std::string& path)
        : file_(path, INVOICE_RECORD_SIZE, INVOICE_DELETED_OFFSET) {}

    // True if a crash-leftover journal was replayed when the file opened.
    bool recovered() const { return file_.recoveredOnOpen(); }
    // True if a forward schema migration ran when the file opened.
    bool migrated()  const { return file_.migratedOnOpen(); }

    // Event-authored projection support: positional count, content fingerprint,
    // disposable clear, and idempotent write at a stable id.
    std::size_t count() { return file_.count(); }
    uint32_t contentHash() { return file_.contentHash(); }
    void clear() { file_.clear(); }
    void upsertAt(const Invoice& inv)
    {
        if (inv.getId() < static_cast<uint32_t>(file_.count())) {
            char buf[INVOICE_RECORD_SIZE]; inv.serialize(buf);
            file_.update(inv.getId(), buf);
        } else {
            Invoice tmp = inv; save(tmp);   // append at the tail (id == count)
        }
    }

    uint32_t save(Invoice& invoice)
    {
        uint32_t id = static_cast<uint32_t>(file_.count());
        invoice.setId(id);
        char buf[INVOICE_RECORD_SIZE];
        invoice.serialize(buf);
        return file_.append(buf);
    }

    bool update(const Invoice& invoice)
    {
        char buf[INVOICE_RECORD_SIZE];
        invoice.serialize(buf);
        return file_.update(invoice.getId(), buf);
    }

    bool remove(uint32_t id)
    {
        char buf[INVOICE_RECORD_SIZE];
        if (!file_.read(id, buf)) return false;
        unsigned char flag = 1u;
        std::memcpy(buf + INVOICE_DELETED_OFFSET, &flag, sizeof(flag));
        return file_.update(id, buf);
    }

    Invoice load(uint32_t id)
    {
        char buf[INVOICE_RECORD_SIZE];
        Invoice inv;
        if (!file_.read(id, buf)) return inv;
        inv.deserialize(buf);
        return inv;
    }

    std::vector<Invoice> loadAll()
    {
        std::vector<Invoice> result;
        char buf[INVOICE_RECORD_SIZE];
        const std::size_t n = file_.count();
        for (std::size_t i = 0; i < n; ++i) {
            if (!file_.read(static_cast<uint32_t>(i), buf)) continue;
            unsigned char flag;
            std::memcpy(&flag, buf + INVOICE_DELETED_OFFSET, sizeof(flag));
            if (flag) continue;
            Invoice inv;
            inv.deserialize(buf);
            if (inv.isValid()) result.push_back(inv);
        }
        return result;
    }

    // Returns the id of a live (non-deleted) invoice whose number matches, or -1.
    // Used to enforce invoice-number uniqueness before a save. O(n) scan; callers
    // invoke it only on commit, which is rare relative to reads.
    int findIdByNumber(const char* number)
    {
        if (!number || number[0] == '\0') return -1;
        char buf[INVOICE_RECORD_SIZE];
        const std::size_t n = file_.count();
        for (std::size_t i = 0; i < n; ++i) {
            if (!file_.read(static_cast<uint32_t>(i), buf)) continue;
            unsigned char flag;
            std::memcpy(&flag, buf + INVOICE_DELETED_OFFSET, sizeof(flag));
            if (flag) continue;
            Invoice inv;
            inv.deserialize(buf);
            if (std::strcmp(inv.getInvoiceNumber(), number) == 0)
                return static_cast<int>(inv.getId());
        }
        return -1;
    }

    std::vector<Invoice> findByCustomer(uint32_t customerId)
    {
        auto all = loadAll();
        std::vector<Invoice> result;
        for (auto& inv : all)
            if (inv.getCustomerId() == customerId)
                result.push_back(inv);
        return result;
    }
};

#endif
