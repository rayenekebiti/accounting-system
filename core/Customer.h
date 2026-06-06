#ifndef CORE_CUSTOMER_H
#define CORE_CUSTOMER_H
#include <cstddef>

inline constexpr std::size_t CUSTOMER_RECORD_SIZE  = 128;
inline constexpr std::size_t CUSTOMER_NAME_LENGTH  = 32;
inline constexpr std::size_t CUSTOMER_EMAIL_LENGTH = 48;
inline constexpr std::size_t CUSTOMER_PHONE_LENGTH = 16;
inline constexpr std::size_t CUSTOMER_TAX_LENGTH   = 16;

struct CustomerData
{
    unsigned short int id;
    const char*        name;
    const char*        email;
    const char*        phone;
    const char*        taxNumber;
    double             balance;       // outstanding amount owed by customer
    bool               isDeleted;
};

class Customer
{
    unsigned short int id;
    char               name[CUSTOMER_NAME_LENGTH];
    char               email[CUSTOMER_EMAIL_LENGTH];
    char               phone[CUSTOMER_PHONE_LENGTH];
    char               taxNumber[CUSTOMER_TAX_LENGTH];
    double             balance;
    bool               isDeleted;

public:
    Customer();
    explicit Customer(const CustomerData& info);

    bool isValid() const;                       // name is non-empty

    void serialize(char* buffer) const;         // throws std::logic_error if !isValid()
    void deserialize(const char* buffer);
    void display() const;

    unsigned short int getId() const;
    void setId(unsigned short int newId);

    const char* getName() const;
    void setName(const char* newName);

    const char* getEmail() const;
    void setEmail(const char* newEmail);

    const char* getPhone() const;
    void setPhone(const char* newPhone);

    const char* getTaxNumber() const;
    void setTaxNumber(const char* newTaxNumber);

    double getBalance() const;
    void setBalance(double newBalance);

    bool getIsDeleted() const;
    void setIsDeleted(bool newIsDeleted);
};

#endif
