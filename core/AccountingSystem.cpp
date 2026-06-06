#include "AccountingSystem.h"
#include <utility>

// ── Categories ───────────────────────────────────────────────────────────────

bool AccountingSystem::addCategory(const Category& c)
{
    if (!c.isValid()) return false;
    if (categoryIndex.find(c.getId()) != categoryIndex.end()) return false;

    categories.push_back(c);
    categoryIndex[c.getId()] = categories.size() - 1;
    return true;
}

bool AccountingSystem::removeCategory(unsigned short int id)
{
    auto it = categoryIndex.find(id);
    if (it == categoryIndex.end()) return false;

    Category& c = categories[it->second];
    if (c.getIsDeleted()) return false;
    c.setIsDeleted(true);
    return true;
}

std::optional<Category> AccountingSystem::findCategory(unsigned short int id) const
{
    auto it = categoryIndex.find(id);
    if (it == categoryIndex.end()) return std::nullopt;

    const Category& c = categories[it->second];
    if (c.getIsDeleted()) return std::nullopt;
    return c;
}

const std::vector<Category>& AccountingSystem::getCategories() const
{
    return categories;
}

// ── Budgets ──────────────────────────────────────────────────────────────────

bool AccountingSystem::addBudget(const Budget& b)
{
    if (!b.isValid()) return false;
    if (budgetIndex.find(b.getId()) != budgetIndex.end()) return false;

    budgets.push_back(b);
    budgetIndex[b.getId()] = budgets.size() - 1;
    return true;
}

bool AccountingSystem::removeBudget(unsigned short int id)
{
    auto it = budgetIndex.find(id);
    if (it == budgetIndex.end()) return false;

    Budget& b = budgets[it->second];
    if (b.getIsDeleted()) return false;
    b.setIsDeleted(true);
    return true;
}

std::optional<Budget> AccountingSystem::findBudget(unsigned short int id) const
{
    auto it = budgetIndex.find(id);
    if (it == budgetIndex.end()) return std::nullopt;

    const Budget& b = budgets[it->second];
    if (b.getIsDeleted()) return std::nullopt;
    return b;
}

const std::vector<Budget>& AccountingSystem::getBudgets() const
{
    return budgets;
}

// ── Bulk load ────────────────────────────────────────────────────────────────

std::size_t AccountingSystem::loadCategories(std::vector<Category> records)
{
    categories.clear();
    categoryIndex.clear();
    categories.reserve(records.size());
    categoryIndex.reserve(records.size());

    std::size_t accepted = 0;
    for (auto& r : records) {
        if (!r.isValid()) continue;
        const unsigned short int id = r.getId();
        if (categoryIndex.find(id) != categoryIndex.end()) continue;

        categories.push_back(std::move(r));
        categoryIndex[id] = categories.size() - 1;
        ++accepted;
    }
    return accepted;
}

std::size_t AccountingSystem::loadBudgets(std::vector<Budget> records)
{
    budgets.clear();
    budgetIndex.clear();
    budgets.reserve(records.size());
    budgetIndex.reserve(records.size());

    std::size_t accepted = 0;
    for (auto& r : records) {
        if (!r.isValid()) continue;
        const unsigned short int id = r.getId();
        if (budgetIndex.find(id) != budgetIndex.end()) continue;

        budgets.push_back(std::move(r));
        budgetIndex[id] = budgets.size() - 1;
        ++accepted;
    }
    return accepted;
}

void AccountingSystem::clear()
{
    categories.clear();
    categoryIndex.clear();
    budgets.clear();
    budgetIndex.clear();
}
