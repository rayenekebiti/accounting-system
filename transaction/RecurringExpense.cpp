#include "RecurringExpense.h"
#include <cstring>
#include <ctime>
#include <cstdio>
#include <iostream>

RecurringExpense::RecurringExpense(const RecurringExpense_info& info)
    : ExpenseTransaction({info.id, info.description, info.amount, info.date, info.categoryId, info.isDeleted})
{
    frequencyDays = info.frequencyDays;
    std::strncpy(endDate, info.endDate, MAX_END_DATE_LENGTH - 1);
    endDate[MAX_END_DATE_LENGTH - 1] = '\0';
    type = RECURRING_EXPENSE;
}

bool RecurringExpense::isDueToday() const
{
    time_t now = time(nullptr);
    struct tm* todayTm = localtime(&now);
    todayTm->tm_hour = 0; todayTm->tm_min = 0; todayTm->tm_sec = 0;
    time_t todayMidnight = mktime(todayTm);

    int sy, sm, sd;
    std::sscanf(date, "%d-%d-%d", &sy, &sm, &sd);
    struct tm startTm = {};
    startTm.tm_year = sy - 1900;
    startTm.tm_mon  = sm - 1;
    startTm.tm_mday = sd;
    time_t startTime = mktime(&startTm);

    int diffDays = static_cast<int>(difftime(todayMidnight, startTime) / 86400);
    return diffDays >= 0 && (diffDays % frequencyDays) == 0;
}

TransactionType RecurringExpense::getType() const
{
    return RECURRING_EXPENSE;
}

void RecurringExpense::display() const
{
    std::cout << "[RECURRING_EXPENSE] ID:" << id
              << " Amount:" << amount
              << " Date:" << date
              << " Desc:" << description
              << " Cat:" << categoryId
              << " Every:" << frequencyDays << " days"
              << " Until:" << endDate << "\n";
}

void RecurringExpense::serialize(char* buffer) const
{
    ExpenseTransaction::serialize(buffer);
    std::memcpy(buffer + 93, &frequencyDays, sizeof(frequencyDays));
    std::memcpy(buffer + 97, endDate,        MAX_END_DATE_LENGTH);
}

void RecurringExpense::deserialize(const char* buffer)
{
    ExpenseTransaction::deserialize(buffer);
    std::memcpy(&frequencyDays, buffer + 93, sizeof(frequencyDays));
    std::memcpy(endDate,        buffer + 97, MAX_END_DATE_LENGTH);
    endDate[MAX_END_DATE_LENGTH - 1] = '\0';
}
