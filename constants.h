#ifndef CONSTANTS_H
#define CONSTANTS_H

const unsigned short int MAX_DESCRIPTION_LENGTH= 64;
const unsigned short int MAX_DATE_LENGTH= 12;
const unsigned short int MAX_END_DATE_LENGTH= 12;
const unsigned short int MAX_NAME_LENGTH= 32;
const unsigned short int MAX_BANK_NAME_LENGTH= 32;
const unsigned short int MAX_CATEGORY_NAME_LENGTH = 32;
const unsigned short int MAX_CATEGORY_TYPE_LENGTH =8; 
const unsigned short int MAX_ACCOUNT_CREATION_DATE_LENGTH= 12;
const unsigned short int MAX_ACCOUNT_NUM_LENGTH= 20;

enum TransactionType
{
    INCOME, EXPENSE,
    RECURRING_INCOME,
    RECURRING_EXPENSE
};
enum AccountType
{
    CASH,BANK,
    SAVINGS,
    CHECKING
};

#endif