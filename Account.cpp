#include "Account.h"
#include "constants.h"
#include <cstring>
#include <ctime>

Account::Account(unsigned short int idIn, const std::string& nameIn, double initialBalance)
    : id(idIn), balance(initialBalance), isDeleted(false)
{
    std::memset(name,      0, sizeof(name));
    std::memset(createdAt, 0, sizeof(createdAt));

    std::strncpy(name, nameIn.c_str(), sizeof(name) - 1);

    std::time_t now = std::time(nullptr);
    std::tm local{};
#if defined(_WIN32)
    if (localtime_s(&local, &now) != 0) return;
#else
    if (!localtime_r(&now, &local)) return;
#endif
    // "%Y-%m-%d" = 10 chars + NUL = 11 — fits in createdAt[12].
    std::strftime(createdAt, sizeof(createdAt), "%Y-%m-%d", &local);
}

double             Account::getBalance() const { return balance; }
unsigned short int Account::getId()      const { return id; }

void Account::setId(unsigned short int newId) { id = newId; }
bool Account::getIsDeleted() const            { return isDeleted; }
void Account::setIsDeleted(bool v)            { isDeleted = v; }

void Account::serialize(char* buf) const
{
    std::memset(buf, 0, ACCOUNT_RECORD_SIZE);
    std::memcpy(buf + 0,  &id,        sizeof(id));
    std::memcpy(buf + 2,  name,       MAX_NAME_LENGTH);
    std::memcpy(buf + 34, &balance,   sizeof(balance));
    std::memcpy(buf + 42, createdAt,  MAX_ACCOUNT_CREATION_DATE_LENGTH);
    int t = static_cast<int>(getAccountType());
    std::memcpy(buf + 54, &t,         sizeof(t));
    unsigned char flag = isDeleted ? 1u : 0u;
    std::memcpy(buf + 58, &flag,      sizeof(flag));
}

void Account::deserialize(const char* buf)
{
    std::memcpy(&id,        buf + 0,  sizeof(id));
    std::memcpy(name,       buf + 2,  MAX_NAME_LENGTH);
    name[MAX_NAME_LENGTH - 1] = '\0';
    std::memcpy(&balance,   buf + 34, sizeof(balance));
    std::memcpy(createdAt,  buf + 42, MAX_ACCOUNT_CREATION_DATE_LENGTH);
    createdAt[MAX_ACCOUNT_CREATION_DATE_LENGTH - 1] = '\0';
    // type byte at offset 54 is consumed by AccountRepository::makeFromBuffer
    unsigned char flag;
    std::memcpy(&flag,      buf + 58, sizeof(flag));
    isDeleted = (flag != 0);
}
