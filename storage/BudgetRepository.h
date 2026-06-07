#ifndef BUDGET_REPOSITORY_H
#define BUDGET_REPOSITORY_H
#include "BinaryRecordFile.h"
#include "../core/Budget.h"
#include <vector>
#include <cstring>

// Layer 2 — BudgetRepository.
// Non-polymorphic: one Budget type, already has serialize/deserialize.
class BudgetRepository {
    BinaryRecordFile file_;

    static constexpr int BDG_DELETED_OFFSET = 16;  // unsigned char flag

public:
    explicit BudgetRepository(const std::string& path)
        : file_(path, BUDGET_RECORD_SIZE) {}

    uint16_t save(Budget& budget)
    {
        uint16_t id = static_cast<uint16_t>(file_.count());
        budget.setId(id);
        char buf[BUDGET_RECORD_SIZE];
        budget.serialize(buf);
        return file_.append(buf);
    }

    bool update(const Budget& budget)
    {
        char buf[BUDGET_RECORD_SIZE];
        budget.serialize(buf);
        return file_.update(budget.getId(), buf);
    }

    bool remove(uint16_t id)
    {
        char buf[BUDGET_RECORD_SIZE];
        if (!file_.read(id, buf)) return false;
        unsigned char flag = 1u;
        std::memcpy(buf + BDG_DELETED_OFFSET, &flag, sizeof(flag));
        return file_.update(id, buf);
    }

    Budget load(uint16_t id)
    {
        char buf[BUDGET_RECORD_SIZE];
        Budget b;
        if (!file_.read(id, buf)) return b;
        b.deserialize(buf);
        return b;
    }

    std::vector<Budget> loadAll()
    {
        std::vector<Budget> result;
        char buf[BUDGET_RECORD_SIZE];
        const std::size_t n = file_.count();
        for (std::size_t i = 0; i < n; ++i) {
            if (!file_.read(static_cast<uint16_t>(i), buf)) continue;
            unsigned char flag;
            std::memcpy(&flag, buf + BDG_DELETED_OFFSET, sizeof(flag));
            if (flag) continue;
            Budget b;
            b.deserialize(buf);
            if (b.isValid()) result.push_back(b);
        }
        return result;
    }

    // Returns all budgets for a given category.
    std::vector<Budget> findByCategory(uint16_t catId)
    {
        auto all = loadAll();
        std::vector<Budget> result;
        for (auto& b : all)
            if (b.getCategoryId() == catId)
                result.push_back(b);
        return result;
    }
};

#endif
