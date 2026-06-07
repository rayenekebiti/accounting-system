#ifndef INVOICE_REPOSITORY_H
#define INVOICE_REPOSITORY_H
#include "BinaryRecordFile.h"
#include "../core/Invoice.h"
#include <vector>
#include <cstring>

class InvoiceRepository {
    BinaryRecordFile file_;
    static constexpr int INV_DELETED_OFFSET = 72;

public:
    explicit InvoiceRepository(const std::string& path)
        : file_(path, INVOICE_RECORD_SIZE) {}

    uint16_t save(Invoice& invoice)
    {
        uint16_t id = static_cast<uint16_t>(file_.count());
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

    bool remove(uint16_t id)
    {
        char buf[INVOICE_RECORD_SIZE];
        if (!file_.read(id, buf)) return false;
        unsigned char flag = 1u;
        std::memcpy(buf + INV_DELETED_OFFSET, &flag, sizeof(flag));
        return file_.update(id, buf);
    }

    Invoice load(uint16_t id)
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
            if (!file_.read(static_cast<uint16_t>(i), buf)) continue;
            unsigned char flag;
            std::memcpy(&flag, buf + INV_DELETED_OFFSET, sizeof(flag));
            if (flag) continue;
            Invoice inv;
            inv.deserialize(buf);
            if (inv.isValid()) result.push_back(inv);
        }
        return result;
    }

    std::vector<Invoice> findByCustomer(uint16_t customerId)
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
