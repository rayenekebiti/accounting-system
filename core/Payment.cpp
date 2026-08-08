#include "Payment.h"
#include <cstring>
#include <stdexcept>

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

static void serializeDate(char* slot, IsoDate d)
{
    std::memset(slot, 0, PAYMENT_DATE_LENGTH);
    if (!d.isValid()) return;
    const std::string s = d.toString();
    std::memcpy(slot, s.c_str(), s.size());
}

static IsoDate deserializeDate(const char* slot)
{
    char tmp[11] = {};
    std::memcpy(tmp, slot, 10);
    auto opt = IsoDate::fromString(tmp);
    return opt.value_or(IsoDate{});
}

Payment::Payment()
    : id(0), invoiceId(0), partyId(0), partyType(PARTY_UNKNOWN),
      date(), amount(), method(PAYMENT_METHOD_UNKNOWN), isDeleted(false)
{
    std::memset(paymentNumber, 0, PAYMENT_NUMBER_LENGTH);
}

Payment::Payment(const PaymentData& info)
{
    if (info.paymentNumber == nullptr || info.paymentNumber[0] == '\0')
        throw std::invalid_argument("Payment number cannot be empty");
    if (!isKnownPartyType(info.partyType))
        throw std::invalid_argument("Payment partyType must be CUSTOMER or SUPPLIER");
    if (!isKnownMethod(info.method))
        throw std::invalid_argument("Payment method must be a defined value");
    if (info.amount.cents() <= 0)
        throw std::invalid_argument("Payment amount must be positive");

    id = info.id;
    copyField(paymentNumber, PAYMENT_NUMBER_LENGTH, info.paymentNumber);
    invoiceId = info.invoiceId;
    partyId   = info.partyId;
    partyType = info.partyType;
    date      = info.date;
    amount    = info.amount;
    method    = info.method;
    isDeleted = info.isDeleted;
}

bool Payment::isValid() const
{
    return paymentNumber[0] != '\0'
        && isKnownPartyType(partyType)
        && isKnownMethod(method)
        && amount.cents() > 0;
}

// Binary layout (PAYMENT_RECORD_SIZE = 64 bytes):
//   0..3     id             (uint32_t)       ← widened from 2 bytes in v1
//   4..19    paymentNumber  (16)
//   20..23   invoiceId      (uint32_t)       ← widened from 2 bytes
//   24..27   partyId        (uint32_t)       ← widened from 2 bytes
//   28..31   partyType      (int32_t)
//   32..43   date           (12) "YYYY-MM-DD\0\0"
//   44..51   amount         (8) int64_t cents
//   52..55   method         (int32_t)
//   56       isDeleted      (1)              ← PAYMENT_DELETED_OFFSET
//   57..63   padding        (7)
static_assert(PAYMENT_DELETED_OFFSET == 56, "keep in sync with serialize layout");
static_assert(4 + 16 + 4 + 4 + 4 + 12 + 8 + 4 + 1 <= PAYMENT_RECORD_SIZE,
              "Payment fields exceed PAYMENT_RECORD_SIZE");

void Payment::serialize(char* buffer) const
{
    if (!isValid())
        throw std::logic_error("Cannot serialize Payment with invalid state");
    std::memset(buffer, 0, PAYMENT_RECORD_SIZE);
    std::memcpy(buffer + 0,  &id,           sizeof(id));
    std::memcpy(buffer + 4,  paymentNumber, PAYMENT_NUMBER_LENGTH);
    std::memcpy(buffer + 20, &invoiceId,    sizeof(invoiceId));
    std::memcpy(buffer + 24, &partyId,      sizeof(partyId));
    int32_t pt = static_cast<int32_t>(partyType);
    std::memcpy(buffer + 28, &pt,           sizeof(pt));
    serializeDate(buffer + 32, date);
    std::int64_t cents = amount.cents();
    std::memcpy(buffer + 44, &cents,        sizeof(cents));
    int32_t m = static_cast<int32_t>(method);
    std::memcpy(buffer + 52, &m,            sizeof(m));
    unsigned char flag = isDeleted ? 1u : 0u;
    std::memcpy(buffer + 56, &flag,         sizeof(flag));
}

void Payment::deserialize(const char* buffer)
{
    std::memcpy(&id,           buffer + 0,  sizeof(id));
    std::memcpy(paymentNumber, buffer + 4,  PAYMENT_NUMBER_LENGTH);
    paymentNumber[PAYMENT_NUMBER_LENGTH - 1] = '\0';
    std::memcpy(&invoiceId,    buffer + 20, sizeof(invoiceId));
    std::memcpy(&partyId,      buffer + 24, sizeof(partyId));
    int32_t pt;
    std::memcpy(&pt,           buffer + 28, sizeof(pt));
    PartyType decodedPt = static_cast<PartyType>(pt);
    partyType = isKnownPartyType(decodedPt) ? decodedPt : PARTY_UNKNOWN;
    date = deserializeDate(buffer + 32);
    std::int64_t cents;
    std::memcpy(&cents,        buffer + 44, sizeof(cents));
    amount = Money::fromCents(cents);
    int32_t mv;
    std::memcpy(&mv,           buffer + 52, sizeof(mv));
    PaymentMethod decodedM = static_cast<PaymentMethod>(mv);
    method = isKnownMethod(decodedM) ? decodedM : PAYMENT_METHOD_UNKNOWN;
    unsigned char flag;
    std::memcpy(&flag,         buffer + 56, sizeof(flag));
    isDeleted = (flag != 0);
}

uint32_t Payment::getId() const              { return id; }
void Payment::setId(uint32_t v)              { id = v; }

const char* Payment::getPaymentNumber() const      { return paymentNumber; }
void Payment::setPaymentNumber(const char* n)      { copyField(paymentNumber, PAYMENT_NUMBER_LENGTH, n); }

uint32_t Payment::getInvoiceId() const       { return invoiceId; }
void Payment::setInvoiceId(uint32_t v)       { invoiceId = v; }

uint32_t Payment::getPartyId() const         { return partyId; }
void Payment::setPartyId(uint32_t v)         { partyId = v; }

PartyType Payment::getPartyType() const      { return partyType; }
void Payment::setPartyType(PartyType t)      { if (isKnownPartyType(t)) partyType = t; }

IsoDate Payment::getDate() const             { return date; }
void Payment::setDate(IsoDate d)             { date = d; }

Money Payment::getAmount() const             { return amount; }
void Payment::setAmount(Money m)             { if (m.cents() > 0) amount = m; }

PaymentMethod Payment::getMethod() const     { return method; }
void Payment::setMethod(PaymentMethod m)     { if (isKnownMethod(m)) method = m; }

bool Payment::getIsDeleted() const           { return isDeleted; }
void Payment::setIsDeleted(bool v)           { isDeleted = v; }
