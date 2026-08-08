#include "transaction.h"
#include <cstring>

uint32_t Transaction::getId() const { return id; }

void Transaction::setId(uint32_t newId) { id = newId; }

const char* Transaction::getDescription() const { return description; }

void Transaction::setDescription(const char* newDescription)
{
    std::strncpy(description, newDescription, MAX_DESCRIPTION_LENGTH - 1);
    description[MAX_DESCRIPTION_LENGTH - 1] = '\0';
}

double Transaction::getAmount() const { return amount; }

void Transaction::setAmount(double newAmount) { amount = newAmount; }

const char* Transaction::getDate() const { return date; }

void Transaction::setDate(const char* newDate)
{
    std::strncpy(date, newDate, MAX_DATE_LENGTH - 1);
    date[MAX_DATE_LENGTH - 1] = '\0';
}

uint32_t Transaction::getCategoryId() const { return categoryId; }

void Transaction::setCategoryId(uint32_t newCategoryId) { categoryId = newCategoryId; }

bool Transaction::getIsDeleted() const { return isDeleted; }

void Transaction::setIsDeleted(bool newIsDeleted) { isDeleted = newIsDeleted; }
