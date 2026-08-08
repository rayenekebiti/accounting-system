#ifndef EXPENSE_REPOSITORY_H
#define EXPENSE_REPOSITORY_H
#include "BinaryRecordFile.h"
#include "../core/Expense.h"
#include <vector>
#include <cstring>

// Disposable projection of the authoritative Expense events (mirrors SupplierRepository).
// Never a write authority — writes go through AuditJournal::recordExpense*.
class ExpenseRepository {
    BinaryRecordFile file_;

    static_assert(EXPENSE_DELETED_OFFSET < EXPENSE_RECORD_SIZE,
        "EXPENSE_DELETED_OFFSET must be within the record");

public:
    explicit ExpenseRepository(const std::string& path)
        : file_(path, EXPENSE_RECORD_SIZE, EXPENSE_DELETED_OFFSET) {}

    bool recovered() const { return file_.recoveredOnOpen(); }
    bool migrated()  const { return file_.migratedOnOpen(); }

    // Event-authored projection support: positional count, content fingerprint, disposable
    // clear, idempotent write at a stable id.
    std::size_t count() { return file_.count(); }
    uint32_t contentHash() { return file_.contentHash(); }
    void clear() { file_.clear(); }
    void upsertAt(const Expense& e)
    {
        if (e.getId() < static_cast<uint32_t>(file_.count())) {
            char buf[EXPENSE_RECORD_SIZE]; e.serialize(buf);
            file_.update(e.getId(), buf);
        } else {
            Expense tmp = e; save(tmp);   // append at the tail (id == count)
        }
    }

    uint32_t save(Expense& expense)
    {
        uint32_t id = static_cast<uint32_t>(file_.count());
        expense.setId(id);
        char buf[EXPENSE_RECORD_SIZE];
        expense.serialize(buf);
        return file_.append(buf);
    }

    bool update(const Expense& expense)
    {
        char buf[EXPENSE_RECORD_SIZE];
        expense.serialize(buf);
        return file_.update(expense.getId(), buf);
    }

    Expense load(uint32_t id)
    {
        char buf[EXPENSE_RECORD_SIZE];
        Expense e;
        if (!file_.read(id, buf)) return e;
        e.deserialize(buf);
        return e;
    }

    std::vector<Expense> loadAll()
    {
        std::vector<Expense> result;
        char buf[EXPENSE_RECORD_SIZE];
        const std::size_t n = file_.count();
        for (std::size_t i = 0; i < n; ++i) {
            if (!file_.read(static_cast<uint32_t>(i), buf)) continue;
            unsigned char flag;
            std::memcpy(&flag, buf + EXPENSE_DELETED_OFFSET, sizeof(flag));
            if (flag) continue;
            Expense e;
            e.deserialize(buf);
            if (e.isValid()) result.push_back(e);
        }
        return result;
    }
};

#endif
