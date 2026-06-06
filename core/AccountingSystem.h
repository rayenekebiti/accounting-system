#ifndef CORE_ACCOUNTING_SYSTEM_H
#define CORE_ACCOUNTING_SYSTEM_H
#include "Category.h"
#include "Budget.h"
#include <vector>
#include <unordered_map>
#include <optional>
#include <cstddef>

// In-memory coordinator. Holds collections of domain records and exposes
// query/mutation operations. Persistence is delegated to the storage layer
// (FileManager) — this class is intentionally not serializable itself.
//
// Lookups, inserts, and removals are O(1) average via an id→index map.
// Iteration order over getCategories()/getBudgets() reflects insertion order.
// IDs are permanent: once an id is added, it cannot be reused — even after
// soft delete — to keep the index single-valued.
class AccountingSystem
{
    std::vector<Category> categories;
    std::unordered_map<unsigned short int, std::size_t> categoryIndex;

    std::vector<Budget> budgets;
    std::unordered_map<unsigned short int, std::size_t> budgetIndex;

public:
    // Categories — returns false on invalid record or duplicate id
    bool addCategory(const Category& c);
    bool removeCategory(unsigned short int id);                   // soft delete
    std::optional<Category> findCategory(unsigned short int id) const;
    const std::vector<Category>& getCategories() const;

    // Budgets
    bool addBudget(const Budget& b);
    bool removeBudget(unsigned short int id);                     // soft delete
    std::optional<Budget> findBudget(unsigned short int id) const;
    const std::vector<Budget>& getBudgets() const;

    // Bulk load from FileManager output. Silently drops invalid records
    // (failed isValid()) and duplicate ids. Returns count accepted.
    std::size_t loadCategories(std::vector<Category> records);
    std::size_t loadBudgets(std::vector<Budget> records);

    void clear();
};

#endif
