#ifndef CORE_PAYMENT_H
#define CORE_PAYMENT_H
#include <cstddef>

inline constexpr std::size_t PAYMENT_RECORD_SIZE   = 64;
inline constexpr std::size_t PAYMENT_NUMBER_LENGTH = 16;
inline constexpr std::size_t PAYMENT_DATE_LENGTH   = 12;

enum PartyType
{
    PARTY_CUSTOMER,
    PARTY_SUPPLIER,
    PARTY_UNKNOWN
};

enum PaymentMethod
{
    PAYMENT_CASH,
    PAYMENT_BANK,
    PAYMENT_CHECK,
    PAYMENT_CARD,
    PAYMENT_METHOD_UNKNOWN
};

struct PaymentData
{
    unsigned short int id;
    const char*        paymentNumber;     // e.g. "PMT-0042"
    unsigned short int invoiceId;         // 0 if not tied to an invoice
    unsigned short int partyId;           // customer or supplier id
    PartyType          partyType;
    const char*        date;
    double             amount;
    PaymentMethod      method;
    bool               isDeleted;
};

class Payment
{
    unsigned short int id;
    char               paymentNumber[PAYMENT_NUMBER_LENGTH];
    unsigned short int invoiceId;
    unsigned short int partyId;
    PartyType          partyType;
    char               date[PAYMENT_DATE_LENGTH];
    double             amount;
    PaymentMethod      method;
    bool               isDeleted;

public:
    Payment();
    explicit Payment(const PaymentData& info);

    bool isValid() const;                       // number non-empty, types known, amount>0

    void serialize(char* buffer) const;
    void deserialize(const char* buffer);
    void display() const;

    unsigned short int getId() const;
    void setId(unsigned short int newId);

    const char* getPaymentNumber() const;
    void setPaymentNumber(const char* newNumber);

    unsigned short int getInvoiceId() const;
    void setInvoiceId(unsigned short int newInvoiceId);

    unsigned short int getPartyId() const;
    void setPartyId(unsigned short int newPartyId);

    PartyType getPartyType() const;
    void setPartyType(PartyType newType);

    const char* getDate() const;
    void setDate(const char* newDate);

    double getAmount() const;
    void setAmount(double newAmount);

    PaymentMethod getMethod() const;
    void setMethod(PaymentMethod newMethod);

    bool getIsDeleted() const;
    void setIsDeleted(bool newIsDeleted);
};

#endif
