#ifndef PAYMENT_REPOSITORY_H
#define PAYMENT_REPOSITORY_H
#include "BinaryRecordFile.h"
#include "../core/Payment.h"
#include <vector>
#include <cstring>

class PaymentRepository {
    BinaryRecordFile file_;
    static constexpr int PAY_DELETED_OFFSET = 50;

public:
    explicit PaymentRepository(const std::string& path)
        : file_(path, PAYMENT_RECORD_SIZE) {}

    uint16_t save(Payment& payment)
    {
        uint16_t id = static_cast<uint16_t>(file_.count());
        payment.setId(id);
        char buf[PAYMENT_RECORD_SIZE];
        payment.serialize(buf);
        return file_.append(buf);
    }

    bool update(const Payment& payment)
    {
        char buf[PAYMENT_RECORD_SIZE];
        payment.serialize(buf);
        return file_.update(payment.getId(), buf);
    }

    bool remove(uint16_t id)
    {
        char buf[PAYMENT_RECORD_SIZE];
        if (!file_.read(id, buf)) return false;
        unsigned char flag = 1u;
        std::memcpy(buf + PAY_DELETED_OFFSET, &flag, sizeof(flag));
        return file_.update(id, buf);
    }

    Payment load(uint16_t id)
    {
        char buf[PAYMENT_RECORD_SIZE];
        Payment p;
        if (!file_.read(id, buf)) return p;
        p.deserialize(buf);
        return p;
    }

    std::vector<Payment> loadAll()
    {
        std::vector<Payment> result;
        char buf[PAYMENT_RECORD_SIZE];
        const std::size_t n = file_.count();
        for (std::size_t i = 0; i < n; ++i) {
            if (!file_.read(static_cast<uint16_t>(i), buf)) continue;
            unsigned char flag;
            std::memcpy(&flag, buf + PAY_DELETED_OFFSET, sizeof(flag));
            if (flag) continue;
            Payment p;
            p.deserialize(buf);
            if (p.isValid()) result.push_back(p);
        }
        return result;
    }

    std::vector<Payment> findByInvoice(uint16_t invoiceId)
    {
        auto all = loadAll();
        std::vector<Payment> result;
        for (auto& p : all)
            if (p.getInvoiceId() == invoiceId)
                result.push_back(p);
        return result;
    }

    std::vector<Payment> findByParty(uint16_t partyId, PartyType partyType)
    {
        auto all = loadAll();
        std::vector<Payment> result;
        for (auto& p : all)
            if (p.getPartyId() == partyId && p.getPartyType() == partyType)
                result.push_back(p);
        return result;
    }
};

#endif
