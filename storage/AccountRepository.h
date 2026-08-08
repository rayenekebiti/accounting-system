#ifndef ACCOUNT_REPOSITORY_H
#define ACCOUNT_REPOSITORY_H
#include <string>   // std::string
#include <utility>   // std::move
#include "BinaryRecordFile.h"
#include "../core/CashAccount.h"
#include "../core/SavingsAccount.h"
#include "../core/CheckingAccount.h"
#include <memory>
#include <vector>
#include <cstring>

static_assert(ACCOUNT_DELETED_OFFSET < ACCOUNT_RECORD_SIZE,
    "ACCOUNT_DELETED_OFFSET must be within the record");

// Polymorphic: reads the AccountType byte to construct the right concrete subclass.
class AccountRepository {
    BinaryRecordFile file_;

    static constexpr std::size_t ACCT_TYPE_OFFSET = 56;  // int (AccountType enum)

    std::unique_ptr<Account> makeFromBuffer(const char* buf)
    {
        int typeInt;
        std::memcpy(&typeInt, buf + ACCT_TYPE_OFFSET, sizeof(typeInt));

        Account* raw = nullptr;
        switch (static_cast<AccountType>(typeInt)) {
            case CASH:     raw = new CashAccount();     break;
            case SAVINGS:  raw = new SavingsAccount();  break;
            case CHECKING: raw = new CheckingAccount(); break;
            default: return nullptr;   // BANK is abstract; should not appear
        }
        std::unique_ptr<Account> acc(raw);
        acc->deserialize(buf);
        return acc;
    }

public:
    explicit AccountRepository(const std::string& path)
        : file_(path, ACCOUNT_RECORD_SIZE, ACCOUNT_DELETED_OFFSET) {}

    uint32_t save(Account& acc)
    {
        uint32_t id = static_cast<uint32_t>(file_.count());
        acc.setId(id);
        char buf[ACCOUNT_RECORD_SIZE];
        acc.serialize(buf);
        return file_.append(buf);
    }

    bool update(const Account& acc)
    {
        char buf[ACCOUNT_RECORD_SIZE];
        acc.serialize(buf);
        return file_.update(acc.getId(), buf);
    }

    bool remove(uint32_t id)
    {
        char buf[ACCOUNT_RECORD_SIZE];
        if (!file_.read(id, buf)) return false;
        unsigned char flag = 1u;
        std::memcpy(buf + ACCOUNT_DELETED_OFFSET, &flag, sizeof(flag));
        return file_.update(id, buf);
    }

    std::unique_ptr<Account> load(uint32_t id)
    {
        char buf[ACCOUNT_RECORD_SIZE];
        if (!file_.read(id, buf)) return nullptr;
        unsigned char flag;
        std::memcpy(&flag, buf + ACCOUNT_DELETED_OFFSET, sizeof(flag));
        if (flag) return nullptr;
        return makeFromBuffer(buf);
    }

    std::vector<std::unique_ptr<Account>> loadAll()
    {
        std::vector<std::unique_ptr<Account>> result;
        char buf[ACCOUNT_RECORD_SIZE];
        const std::size_t n = file_.count();
        for (std::size_t i = 0; i < n; ++i) {
            if (!file_.read(static_cast<uint32_t>(i), buf)) continue;
            unsigned char flag;
            std::memcpy(&flag, buf + ACCOUNT_DELETED_OFFSET, sizeof(flag));
            if (flag) continue;
            auto acc = makeFromBuffer(buf);
            if (acc) result.push_back(std::move(acc));
        }
        return result;
    }
};

#endif
