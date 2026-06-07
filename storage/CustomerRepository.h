#ifndef CUSTOMER_REPOSITORY_H
#define CUSTOMER_REPOSITORY_H
#include "BinaryRecordFile.h"
#include "../core/Customer.h"
#include <vector>
#include <cstring>

class CustomerRepository {
    BinaryRecordFile file_;
    static constexpr int CUST_DELETED_OFFSET = 122;

public:
    explicit CustomerRepository(const std::string& path)
        : file_(path, CUSTOMER_RECORD_SIZE) {}

    uint16_t save(Customer& customer)
    {
        uint16_t id = static_cast<uint16_t>(file_.count());
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

    bool remove(uint16_t id)
    {
        char buf[CUSTOMER_RECORD_SIZE];
        if (!file_.read(id, buf)) return false;
        unsigned char flag = 1u;
        std::memcpy(buf + CUST_DELETED_OFFSET, &flag, sizeof(flag));
        return file_.update(id, buf);
    }

    Customer load(uint16_t id)
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
            if (!file_.read(static_cast<uint16_t>(i), buf)) continue;
            unsigned char flag;
            std::memcpy(&flag, buf + CUST_DELETED_OFFSET, sizeof(flag));
            if (flag) continue;
            Customer c;
            c.deserialize(buf);
            if (c.isValid()) result.push_back(c);
        }
        return result;
    }
};

#endif
