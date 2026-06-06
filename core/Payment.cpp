#include "Payment.h"
#include <cstring>
#include <stdexcept>
#include <iostream>

static void copyField(char* dest, std::size_t capacity, const char* src)
{
    std::memset(dest, 0, capacity);
    if (src == nullptr) return;
    std::strncpy(dest, src, capacity - 1);
    dest[capacity - 1] = '\0';
}

static bool isKnownPartyType(PartyType t)
{
    return t == PARTY_CUSTOMER || t == PARTY_SUPPLIER;
}

static bool isKnownMethod(PaymentMethod m)
{
    return m == PAYMENT_CASH || m == PAYMENT_BANK ||
           m == PAYMENT_CHECK || m == PAYMENT_CARD;
}

Payment::Payment()
    : id(0), invoiceId(0), partyId(0), partyType(PARTY_UNKNOWN),
      amount(0.0), method(PAYMENT_METHOD_UNKNOWN), isDeleted(false)
{
    std::memset(paymentNumber, 0, PAYMENT_NUMBER_LENGTH);
    std::memset(date,          0, PAYMENT_DATE_LENGTH);
}

Payment::Payment(const PaymentData& info)
{
    if (info.paymentNumber == nullptr || info.paymentNumber[0] == '\0')
        throw std::invalid_argument("Payment number cannot be empty");
    if (!isKnownPartyType(info.partyType))
        throw std::invalid_argument("Payment partyType must be CUSTOMER or SUPPLIER");
    if (!isKnownMethod(info.method))
        throw std::invalid_argument("Payment method must be a defined value");
    if (info.amount <= 0)
        throw std::invalid_argument("Payment amount must be positive");

    id = info.id;
    copyField(paymentNumber, PAYMENT_NUMBER_LENGTH, info.paymentNumber);
    invoiceId = info.invoiceId;
    partyId   = info.partyId;
    partyType = info.partyType;
    copyField(date, PAYMENT_DATE_LENGTH, info.date);
    amount    = info.amount;
    method    = info.method;
    isDeleted = info.isDeleted;
}

bool Payment::isValid() const
{
    return paymentNumber[0] != '\0'
        && isKnownPartyType(partyType)
        && isKnownMethod(method)
        && amount > 0;
}

// Binary layout (PAYMENT_RECORD_SIZE = 64 bytes):
//   0..1     id
//   2..17    paymentNumber  (16)
//   18..19   invoiceId
//   20..21   partyId
//   22..25   partyType      (int)
//   26..37   date           (12)
//   38..45   amount         (8)
//   46..49   method         (int)
//   50       isDeleted      (1)
//   51..63   padding        (13)
void Payment::serialize(char* buffer) const
{
    if (!isValid())
        throw std::logic_error("Cannot serialize Payment with invalid state");
    std::memset(buffer, 0, PAYMENT_RECORD_SIZE);
    std::memcpy(buffer + 0,  &id,            sizeof(id));
    std::memcpy(buffer + 2,  paymentNumber,  PAYMENT_NUMBER_LENGTH);
    std::memcpy(buffer + 18, &invoiceId,     sizeof(invoiceId));
    std::memcpy(buffer + 20, &partyId,       sizeof(partyId));
    int pt = static_cast<int>(partyType);
    std::memcpy(buffer + 22, &pt,            sizeof(pt));
    std::memcpy(buffer + 26, date,           PAYMENT_DATE_LENGTH);
    std::memcpy(buffer + 38, &amount,        sizeof(amount));
    int m = static_cast<int>(method);
    std::memcpy(buffer + 46, &m,             sizeof(m));
    unsigned char flag = isDeleted ? 1u : 0u;
    std::memcpy(buffer + 50, &flag,          sizeof(flag));
}

void Payment::deserialize(const char* buffer)
{
    std::memcpy(&id,            buffer + 0,  sizeof(id));
    std::memcpy(paymentNumber,  buffer + 2,  PAYMENT_NUMBER_LENGTH);
    paymentNumber[PAYMENT_NUMBER_LENGTH - 1] = '\0';
    std::memcpy(&invoiceId,     buffer + 18, sizeof(invoiceId));
    std::memcpy(&partyId,       buffer + 20, sizeof(partyId));
    int pt;
    std::memcpy(&pt,            buffer + 22, sizeof(pt));
    PartyType decodedPt = static_cast<PartyType>(pt);
    partyType = isKnownPartyType(decodedPt) ? decodedPt : PARTY_UNKNOWN;
    std::memcpy(date,           buffer + 26, PAYMENT_DATE_LENGTH);
    date[PAYMENT_DATE_LENGTH - 1] = '\0';
    std::memcpy(&amount,        buffer + 38, sizeof(amount));
    int m;
    std::memcpy(&m,             buffer + 46, sizeof(m));
    PaymentMethod decodedM = static_cast<PaymentMethod>(m);
    method = isKnownMethod(decodedM) ? decodedM : PAYMENT_METHOD_UNKNOWN;
    unsigned char flag;
    std::memcpy(&flag,          buffer + 50, sizeof(flag));
    isDeleted = (flag != 0);
}

void Payment::display() const
{
    const char* partyStr = (partyType == PARTY_CUSTOMER) ? "Customer" :
                           (partyType == PARTY_SUPPLIER) ? "Supplier" : "Unknown";
    const char* methodStr =
        (method == PAYMENT_CASH)  ? "Cash"  :
        (method == PAYMENT_BANK)  ? "Bank"  :
        (method == PAYMENT_CHECK) ? "Check" :
        (method == PAYMENT_CARD)  ? "Card"  : "Unknown";

    std::cout << "[PAYMENT] ID:" << id
              << " #" << paymentNumber
              << " Invoice:" << invoiceId
              << " Party:" << partyStr << "/" << partyId
              << " Date:" << date
              << " Amount:" << amount
              << " Method:" << methodStr
              << " Deleted:" << (isDeleted ? "yes" : "no") << "\n";
}

unsigned short int Payment::getId() const                   { return id; }
void Payment::setId(unsigned short int newId)               { id = newId; }

const char* Payment::getPaymentNumber() const               { return paymentNumber; }
void Payment::setPaymentNumber(const char* newNumber)       { copyField(paymentNumber, PAYMENT_NUMBER_LENGTH, newNumber); }

unsigned short int Payment::getInvoiceId() const            { return invoiceId; }
void Payment::setInvoiceId(unsigned short int newInvoiceId) { invoiceId = newInvoiceId; }

unsigned short int Payment::getPartyId() const              { return partyId; }
void Payment::setPartyId(unsigned short int newPartyId)     { partyId = newPartyId; }

PartyType Payment::getPartyType() const                     { return partyType; }
void Payment::setPartyType(PartyType newType)
{
    if (!isKnownPartyType(newType)) return;
    partyType = newType;
}

const char* Payment::getDate() const                        { return date; }
void Payment::setDate(const char* newDate)                  { copyField(date, PAYMENT_DATE_LENGTH, newDate); }

double Payment::getAmount() const                           { return amount; }
void Payment::setAmount(double newAmount)
{
    if (newAmount <= 0) return;
    amount = newAmount;
}

PaymentMethod Payment::getMethod() const                    { return method; }
void Payment::setMethod(PaymentMethod newMethod)
{
    if (!isKnownMethod(newMethod)) return;
    method = newMethod;
}

bool Payment::getIsDeleted() const                          { return isDeleted; }
void Payment::setIsDeleted(bool newIsDeleted)               { isDeleted = newIsDeleted; }
