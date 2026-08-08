#include "Invoice.h"
#include <cstring>
#include <stdexcept>

static void copyField(char* dest, std::size_t capacity, const char* src)
{
    std::memset(dest, 0, capacity);
    if (src == nullptr) return;
    std::strncpy(dest, src, capacity - 1);
    dest[capacity - 1] = '\0';
}

static bool isKnownStatus(InvoiceStatus s)
{
    return s == INVOICE_DRAFT   || s == INVOICE_POSTED ||
           s == INVOICE_PAID    || s == INVOICE_OVERDUE ||
           s == INVOICE_VOID;
}

static void serializeDate(char* slot, IsoDate d)
{
    std::memset(slot, 0, INVOICE_DATE_LENGTH);
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

Invoice::Invoice()
    : id(0), customerId(0), subtotal(), taxAmount(), total(),
      status(INVOICE_STATUS_UNKNOWN), isDeleted(false)
{
    std::memset(invoiceNumber, 0, INVOICE_NUMBER_LENGTH);
}

Invoice::Invoice(const InvoiceData& info)
{
    if (info.invoiceNumber == nullptr || info.invoiceNumber[0] == '\0')
        throw std::invalid_argument("Invoice number cannot be empty");
    if (!isKnownStatus(info.status))
        throw std::invalid_argument("Invoice status must be a defined value");
    if (info.subtotal.isNegative() || info.taxAmount.isNegative() || info.total.isNegative())
        throw std::invalid_argument("Invoice amounts cannot be negative");

    id = info.id;
    copyField(invoiceNumber, INVOICE_NUMBER_LENGTH, info.invoiceNumber);
    customerId = info.customerId;
    issueDate  = info.issueDate;
    dueDate    = info.dueDate;
    subtotal   = info.subtotal;
    taxAmount  = info.taxAmount;
    total      = info.total;
    status     = info.status;
    isDeleted  = info.isDeleted;
}

bool Invoice::isValid() const
{
    return invoiceNumber[0] != '\0'
        && isKnownStatus(status)
        && !subtotal.isNegative() && !taxAmount.isNegative() && !total.isNegative();
}

// Binary layout (INVOICE_RECORD_SIZE = 96 bytes):
//   0..3     id             (uint32_t)       ← widened from 2 bytes in v1
//   4..19    invoiceNumber  (16)
//   20..23   customerId     (uint32_t)       ← widened from 2 bytes in v1
//   24..35   issueDate      (12) "YYYY-MM-DD\0\0"
//   36..47   dueDate        (12)
//   48..55   subtotal       (8) int64_t cents
//   56..63   taxAmount      (8)
//   64..71   total          (8)
//   72..75   status         (int32_t)
//   76       isDeleted      (1)              ← INVOICE_DELETED_OFFSET
//   77..95   padding        (19)
static_assert(INVOICE_DELETED_OFFSET == 76, "keep in sync with serialize layout");
static_assert(4 + 16 + 4 + 12 + 12 + 8 + 8 + 8 + 4 + 1 <= INVOICE_RECORD_SIZE,
              "Invoice fields exceed INVOICE_RECORD_SIZE");

void Invoice::serialize(char* buffer) const
{
    if (!isValid())
        throw std::logic_error("Cannot serialize Invoice with invalid state");
    std::memset(buffer, 0, INVOICE_RECORD_SIZE);
    std::memcpy(buffer + 0,  &id,           sizeof(id));
    std::memcpy(buffer + 4,  invoiceNumber, INVOICE_NUMBER_LENGTH);
    std::memcpy(buffer + 20, &customerId,   sizeof(customerId));
    serializeDate(buffer + 24, issueDate);
    serializeDate(buffer + 36, dueDate);
    std::int64_t sub = subtotal.cents();
    std::memcpy(buffer + 48, &sub,          sizeof(sub));
    std::int64_t tax = taxAmount.cents();
    std::memcpy(buffer + 56, &tax,          sizeof(tax));
    std::int64_t tot = total.cents();
    std::memcpy(buffer + 64, &tot,          sizeof(tot));
    int32_t s = static_cast<int32_t>(status);
    std::memcpy(buffer + 72, &s,            sizeof(s));
    unsigned char flag = isDeleted ? 1u : 0u;
    std::memcpy(buffer + 76, &flag,         sizeof(flag));
}

void Invoice::deserialize(const char* buffer)
{
    std::memcpy(&id,           buffer + 0,  sizeof(id));
    std::memcpy(invoiceNumber, buffer + 4,  INVOICE_NUMBER_LENGTH);
    invoiceNumber[INVOICE_NUMBER_LENGTH - 1] = '\0';
    std::memcpy(&customerId,   buffer + 20, sizeof(customerId));
    issueDate = deserializeDate(buffer + 24);
    dueDate   = deserializeDate(buffer + 36);
    std::int64_t sub;
    std::memcpy(&sub,          buffer + 48, sizeof(sub));
    subtotal = Money::fromCents(sub);
    std::int64_t tax;
    std::memcpy(&tax,          buffer + 56, sizeof(tax));
    taxAmount = Money::fromCents(tax);
    std::int64_t tot;
    std::memcpy(&tot,          buffer + 64, sizeof(tot));
    total = Money::fromCents(tot);
    int32_t sv;
    std::memcpy(&sv,           buffer + 72, sizeof(sv));
    InvoiceStatus decoded = static_cast<InvoiceStatus>(sv);
    status = isKnownStatus(decoded) ? decoded : INVOICE_STATUS_UNKNOWN;
    unsigned char flag;
    std::memcpy(&flag,         buffer + 76, sizeof(flag));
    isDeleted = (flag != 0);
}

uint32_t Invoice::getId() const                  { return id; }
void Invoice::setId(uint32_t newId)              { id = newId; }

const char* Invoice::getInvoiceNumber() const    { return invoiceNumber; }
void Invoice::setInvoiceNumber(const char* n)    { copyField(invoiceNumber, INVOICE_NUMBER_LENGTH, n); }

uint32_t Invoice::getCustomerId() const          { return customerId; }
void Invoice::setCustomerId(uint32_t cid)        { customerId = cid; }

IsoDate Invoice::getIssueDate() const            { return issueDate; }
void Invoice::setIssueDate(IsoDate d)            { issueDate = d; }

IsoDate Invoice::getDueDate() const              { return dueDate; }
void Invoice::setDueDate(IsoDate d)              { dueDate = d; }

Money Invoice::getSubtotal() const               { return subtotal; }
void Invoice::setSubtotal(Money m)               { if (!m.isNegative()) subtotal = m; }

Money Invoice::getTaxAmount() const              { return taxAmount; }
void Invoice::setTaxAmount(Money m)              { if (!m.isNegative()) taxAmount = m; }

Money Invoice::getTotal() const                  { return total; }
void Invoice::setTotal(Money m)                  { if (!m.isNegative()) total = m; }

InvoiceStatus Invoice::getStatus() const         { return status; }
void Invoice::setStatus(InvoiceStatus s)         { if (isKnownStatus(s)) status = s; }

bool Invoice::getIsDeleted() const               { return isDeleted; }
void Invoice::setIsDeleted(bool v)               { isDeleted = v; }
