#ifndef TRANSACTION_REPOSITORIES_H
#define TRANSACTION_REPOSITORIES_H
#include "BinaryRecordFile.h"
#include "../transaction/IncomeTransaction.h"
#include "../transaction/ExpenseTransaction.h"
#include "../transaction/RecurringIncome.h"
#include "../transaction/RecurringExpense.h"
#include <memory>
#include <vector>
#include <cstring>

// Layer 2 — TransactionRepository.
// Translates between typed Transaction objects and raw byte records.
// All polymorphism is resolved here via makeFromBuffer().
class TransactionRepository {
    BinaryRecordFile file_;

    // Offsets within a TRANSACTION_RECORD_SIZE buffer (see IncomeTransaction::serialize).
    static constexpr int TX_TYPE_OFFSET    = 88;  // int (TransactionType enum)
    static constexpr int TX_DELETED_OFFSET = 92;  // unsigned char flag

    std::unique_ptr<Transaction> makeFromBuffer(const char* buf)
    {
        int typeInt;
        std::memcpy(&typeInt, buf + TX_TYPE_OFFSET, sizeof(typeInt));

        Transaction* raw = nullptr;
        switch (static_cast<TransactionType>(typeInt)) {
            case INCOME:            raw = new IncomeTransaction();  break;
            case EXPENSE:           raw = new ExpenseTransaction(); break;
            case RECURRING_INCOME:  raw = new RecurringIncome();    break;
            case RECURRING_EXPENSE: raw = new RecurringExpense();   break;
            default: return nullptr;
        }
        std::unique_ptr<Transaction> tx(raw);
        tx->deserialize(buf);
        return tx;
    }

public:
    explicit TransactionRepository(const std::string& path)
        : file_(path, TRANSACTION_RECORD_SIZE) {}

    // Assigns an id to tx, serializes, appends. Returns the assigned id.
    uint16_t save(Transaction& tx)
    {
        uint16_t id = static_cast<uint16_t>(file_.count());
        tx.setId(id);
        char buf[TRANSACTION_RECORD_SIZE];
        tx.serialize(buf);
        return file_.append(buf);
    }

    // Overwrites the record for tx.getId() with the current state of tx.
    bool update(const Transaction& tx)
    {
        char buf[TRANSACTION_RECORD_SIZE];
        tx.serialize(buf);
        return file_.update(tx.getId(), buf);
    }

    // Soft-delete: sets the isDeleted flag byte in the on-disk record.
    bool remove(uint16_t id)
    {
        char buf[TRANSACTION_RECORD_SIZE];
        if (!file_.read(id, buf)) return false;
        unsigned char flag = 1u;
        std::memcpy(buf + TX_DELETED_OFFSET, &flag, sizeof(flag));
        return file_.update(id, buf);
    }

    // Returns nullptr if id is out of range or the record is soft-deleted.
    std::unique_ptr<Transaction> load(uint16_t id)
    {
        char buf[TRANSACTION_RECORD_SIZE];
        if (!file_.read(id, buf)) return nullptr;
        unsigned char flag;
        std::memcpy(&flag, buf + TX_DELETED_OFFSET, sizeof(flag));
        if (flag) return nullptr;
        return makeFromBuffer(buf);
    }

    // Returns all non-deleted records.
    std::vector<std::unique_ptr<Transaction>> loadAll()
    {
        std::vector<std::unique_ptr<Transaction>> result;
        char buf[TRANSACTION_RECORD_SIZE];
        const std::size_t n = file_.count();
        for (std::size_t i = 0; i < n; ++i) {
            if (!file_.read(static_cast<uint16_t>(i), buf)) continue;
            unsigned char flag;
            std::memcpy(&flag, buf + TX_DELETED_OFFSET, sizeof(flag));
            if (flag) continue;
            auto tx = makeFromBuffer(buf);
            if (tx) result.push_back(std::move(tx));
        }
        return result;
    }

    std::vector<std::unique_ptr<Transaction>> findByCategory(uint16_t catId)
    {
        auto all = loadAll();
        std::vector<std::unique_ptr<Transaction>> result;
        for (auto& tx : all)
            if (tx->getCategoryId() == catId)
                result.push_back(std::move(tx));
        return result;
    }

    // from/to are ISO date strings ("YYYY-MM-DD"); lexicographic compare works.
    std::vector<std::unique_ptr<Transaction>> findByDateRange(const char* from, const char* to)
    {
        auto all = loadAll();
        std::vector<std::unique_ptr<Transaction>> result;
        for (auto& tx : all) {
            const char* d = tx->getDate();
            if (std::strncmp(d, from, MAX_DATE_LENGTH) >= 0 &&
                std::strncmp(d, to,   MAX_DATE_LENGTH) <= 0)
                result.push_back(std::move(tx));
        }
        return result;
    }
};

#endif
