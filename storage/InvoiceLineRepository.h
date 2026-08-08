#ifndef INVOICE_LINE_REPOSITORY_H
#define INVOICE_LINE_REPOSITORY_H
#include "BinaryRecordFile.h"
#include "../core/InvoiceLine.h"
#include <vector>
#include <cstring>

static_assert(INVOICE_LINE_DELETED_OFFSET < INVOICE_LINE_RECORD_SIZE,
    "INVOICE_LINE_DELETED_OFFSET must be within the record");

class InvoiceLineRepository {
    BinaryRecordFile file_;

public:
    explicit InvoiceLineRepository(const std::string& path)
        : file_(path, INVOICE_LINE_RECORD_SIZE, INVOICE_LINE_DELETED_OFFSET) {}

    uint32_t save(InvoiceLine& line)
    {
        const uint32_t id = static_cast<uint32_t>(file_.count());
        line.setId(id);
        char buf[INVOICE_LINE_RECORD_SIZE];
        line.serialize(buf);
        return file_.append(buf);
    }

    bool update(const InvoiceLine& line)
    {
        char buf[INVOICE_LINE_RECORD_SIZE];
        line.serialize(buf);
        return file_.update(line.getId(), buf);
    }

    bool remove(uint32_t id)
    {
        char buf[INVOICE_LINE_RECORD_SIZE];
        if (!file_.read(id, buf)) return false;
        unsigned char flag = 1u;
        std::memcpy(buf + INVOICE_LINE_DELETED_OFFSET, &flag, sizeof(flag));
        return file_.update(id, buf);
    }

    InvoiceLine load(uint32_t id)
    {
        char buf[INVOICE_LINE_RECORD_SIZE];
        InvoiceLine line;
        if (!file_.read(id, buf)) return line;
        line.deserialize(buf);
        return line;
    }

    std::vector<InvoiceLine> loadAll()
    {
        std::vector<InvoiceLine> result;
        char buf[INVOICE_LINE_RECORD_SIZE];
        const std::size_t n = file_.count();
        for (std::size_t i = 0; i < n; ++i) {
            if (!file_.read(static_cast<uint32_t>(i), buf)) continue;
            unsigned char flag;
            std::memcpy(&flag, buf + INVOICE_LINE_DELETED_OFFSET, sizeof(flag));
            if (flag) continue;
            InvoiceLine line;
            line.deserialize(buf);
            result.push_back(line);
        }
        return result;
    }

    // Event-authored projection support. Stable line id = positional slot, assigned
    // at authoring and embedded in the event — NEVER derived from index/order at apply.
    std::size_t count() { return file_.count(); }
    uint32_t contentHash() { return file_.contentHash(); }
    void clear() { file_.clear(); }
    void upsertAt(const InvoiceLine& line)
    {
        if (line.getId() < static_cast<uint32_t>(file_.count())) {
            char buf[INVOICE_LINE_RECORD_SIZE]; line.serialize(buf);
            file_.update(line.getId(), buf);
        } else {
            InvoiceLine tmp = line; save(tmp);   // append at the tail (id == count)
        }
    }

    std::vector<InvoiceLine> findByInvoice(uint32_t invoiceId)
    {
        std::vector<InvoiceLine> result;
        char buf[INVOICE_LINE_RECORD_SIZE];
        const std::size_t n = file_.count();
        for (std::size_t i = 0; i < n; ++i) {
            if (!file_.read(static_cast<uint32_t>(i), buf)) continue;
            unsigned char flag;
            std::memcpy(&flag, buf + INVOICE_LINE_DELETED_OFFSET, sizeof(flag));
            if (flag) continue;
            uint32_t inv;
            std::memcpy(&inv, buf + 4, sizeof(inv));
            if (inv != invoiceId) continue;
            InvoiceLine line;
            line.deserialize(buf);
            result.push_back(line);
        }
        return result;
    }
};

#endif
