#ifndef CORE_PAYMENT_H
#define CORE_PAYMENT_H
#include <cstddef>
#include <cstdint>
#include "Money.h"
#include "IsoDate.h"

inline constexpr std::size_t PAYMENT_RECORD_SIZE   = 64;
inline constexpr std::size_t PAYMENT_NUMBER_LENGTH = 16;
inline constexpr std::size_t PAYMENT_DATE_LENGTH   = 12;

// Byte offset of the isDeleted flag within a serialized Payment record.
inline constexpr std::size_t PAYMENT_DELETED_OFFSET = 56;

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
    uint32_t      id;
    const char*   paymentNumber;     // e.g. "PMT-0042"
    uint32_t      invoiceId;         // 0 if not tied to an invoice
    uint32_t      partyId;           // customer or supplier id
    PartyType     partyType;
    IsoDate       date;
    Money         amount;
    PaymentMethod method;
    bool          isDeleted;
};

class Payment
{
    uint32_t      id;
    char          paymentNumber[PAYMENT_NUMBER_LENGTH];
    uint32_t      invoiceId;
    uint32_t      partyId;
    PartyType     partyType;
    IsoDate       date;
    Money         amount;
    PaymentMethod method;
    bool          isDeleted;

public:
    Payment();
    explicit Payment(const PaymentData& info);

    bool isValid() const;                       // number non-empty, types known, amount>0

    void serialize(char* buffer) const;
    void deserialize(const char* buffer);

    uint32_t getId() const;
    void setId(uint32_t newId);

    const char* getPaymentNumber() const;
    void setPaymentNumber(const char* newNumber);

    uint32_t getInvoiceId() const;
    void setInvoiceId(uint32_t newInvoiceId);

    uint32_t getPartyId() const;
    void setPartyId(uint32_t newPartyId);

    PartyType getPartyType() const;
    void setPartyType(PartyType newType);

    IsoDate getDate() const;
    void setDate(IsoDate newDate);

    Money getAmount() const;
    void setAmount(Money newAmount);

    PaymentMethod getMethod() const;
    void setMethod(PaymentMethod newMethod);

    bool getIsDeleted() const;
    void setIsDeleted(bool newIsDeleted);
};

#endif
