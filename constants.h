#ifndef CONSTANTS_H
#define CONSTANTS_H

const unsigned short int MAX_DESCRIPTION_LENGTH       = 64;
const unsigned short int MAX_DATE_LENGTH               = 12;
const unsigned short int MAX_END_DATE_LENGTH           = 12;
const unsigned short int MAX_NAME_LENGTH               = 32;
const unsigned short int MAX_BANK_NAME_LENGTH          = 32;
const unsigned short int MAX_CATEGORY_NAME_LENGTH      = 32;
const unsigned short int MAX_CATEGORY_TYPE_LENGTH      = 8;
const unsigned short int MAX_ACCOUNT_CREATION_DATE_LENGTH = 12;
const unsigned short int MAX_ACCOUNT_NUM_LENGTH        = 20;

const unsigned short int TRANSACTION_RECORD_SIZE = 128;
const unsigned short int ACCOUNT_RECORD_SIZE     = 160;
const unsigned short int CATEGORY_RECORD_SIZE    = 48;
const unsigned short int BUDGET_RECORD_SIZE      = 32;

// Monetary limits — signed double to avoid unsigned wrap-around.
constexpr double MAX_WITHDRAWAL_LIMIT = 50000.0;
constexpr double MAX_OVERDRAFT_LIMIT  = -20000.0;

enum TransactionType
{
    INCOME, EXPENSE, UNKNOWN,
    RECURRING_INCOME,
    RECURRING_EXPENSE
};
enum AccountType
{
    CASH, BANK,
    SAVINGS,
    CHECKING
};

#endif
