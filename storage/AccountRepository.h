#ifndef ACCOUNT_REPOSITORY_H
#define ACCOUNT_REPOSITORY_H
#include "BinaryRecordFile.h"
#include "../CashAccount.h"
#include "../SavingsAccount.h"
#include "../CheckingAccount.h"
#include <memory>
#include <vector>
#include <cstring>

// Layer 2 — AccountRepository.
// Polymorphic: reads the AccountType byte to construct the right concrete subclass.
class AccountRepository {
    BinaryRecordFile file_;

    // Offsets within an ACCOUNT_RECORD_SIZE buffer (see Account::serialize).
    static constexpr int ACCT_TYPE_OFFSET    = 54;  // int (AccountType enum)
    static constexpr int ACCT_DELETED_OFFSET = 58;  // unsigned char flag

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
        : file_(path, ACCOUNT_RECORD_SIZE) {}

    // Assigns an id to acc, serializes, appends. Returns the assigned id.
    uint16_t save(Account& acc)
    {
        uint16_t id = static_cast<uint16_t>(file_.count());
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

    bool remove(uint16_t id)
    {
        char buf[ACCOUNT_RECORD_SIZE];
        if (!file_.read(id, buf)) return false;
        unsigned char flag = 1u;
        std::memcpy(buf + ACCT_DELETED_OFFSET, &flag, sizeof(flag));
        return file_.update(id, buf);
    }

    std::unique_ptr<Account> load(uint16_t id)
    {
        char buf[ACCOUNT_RECORD_SIZE];
        if (!file_.read(id, buf)) return nullptr;
        unsigned char flag;
        std::memcpy(&flag, buf + ACCT_DELETED_OFFSET, sizeof(flag));
        if (flag) return nullptr;
        return makeFromBuffer(buf);
    }

    std::vector<std::unique_ptr<Account>> loadAll()
    {
        std::vector<std::unique_ptr<Account>> result;
        char buf[ACCOUNT_RECORD_SIZE];
        const std::size_t n = file_.count();
        for (std::size_t i = 0; i < n; ++i) {
            if (!file_.read(static_cast<uint16_t>(i), buf)) continue;
            unsigned char flag;
            std::memcpy(&flag, buf + ACCT_DELETED_OFFSET, sizeof(flag));
            if (flag) continue;
            auto acc = makeFromBuffer(buf);
            if (acc) result.push_back(std::move(acc));
        }
        return result;
    }
};

#endif
