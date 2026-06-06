#include "Invoice.h"
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

static bool isKnownStatus(InvoiceStatus s)
{
    return s == INVOICE_DRAFT   || s == INVOICE_POSTED ||
           s == INVOICE_PAID    || s == INVOICE_OVERDUE ||
           s == INVOICE_VOID;
}

Invoice::Invoice()
    : id(0), customerId(0), subtotal(0.0), taxAmount(0.0), total(0.0),
      status(INVOICE_STATUS_UNKNOWN), isDeleted(false)
{
    std::memset(invoiceNumber, 0, INVOICE_NUMBER_LENGTH);
    std::memset(issueDate,     0, INVOICE_DATE_LENGTH);
    std::memset(dueDate,       0, INVOICE_DATE_LENGTH);
}

Invoice::Invoice(const InvoiceData& info)
{
    if (info.invoiceNumber == nullptr || info.invoiceNumber[0] == '\0')
        throw std::invalid_argument("Invoice number cannot be empty");
    if (!isKnownStatus(info.status))
        throw std::invalid_argument("Invoice status must be a defined value");
    if (info.subtotal < 0 || info.taxAmount < 0 || info.total < 0)
        throw std::invalid_argument("Invoice amounts cannot be negative");

    id = info.id;
    copyField(invoiceNumber, INVOICE_NUMBER_LENGTH, info.invoiceNumber);
    customerId = info.customerId;
    copyField(issueDate, INVOICE_DATE_LENGTH, info.issueDate);
    copyField(dueDate,   INVOICE_DATE_LENGTH, info.dueDate);
    subtotal  = info.subtotal;
    taxAmount = info.taxAmount;
    total     = info.total;
    status    = info.status;
    isDeleted = info.isDeleted;
}

bool Invoice::isValid() const
{
    return invoiceNumber[0] != '\0'
        && isKnownStatus(status)
        && subtotal >= 0 && taxAmount >= 0 && total >= 0;
}

// Binary layout (INVOICE_RECORD_SIZE = 96 bytes):
//   0..1     id
//   2..17    invoiceNumber  (16)
//   18..19   customerId
//   20..31   issueDate      (12)
//   32..43   dueDate        (12)
//   44..51   subtotal       (8)
//   52..59   taxAmount      (8)
//   60..67   total          (8)
//   68..71   status         (int)
//   72       isDeleted      (1)
//   73..95   padding        (23)
void Invoice::serialize(char* buffer) const
{
    if (!isValid())
        throw std::logic_error("Cannot serialize Invoice with invalid state");
    std::memset(buffer, 0, INVOICE_RECORD_SIZE);
    std::memcpy(buffer + 0,  &id,            sizeof(id));
    std::memcpy(buffer + 2,  invoiceNumber,  INVOICE_NUMBER_LENGTH);
    std::memcpy(buffer + 18, &customerId,    sizeof(customerId));
    std::memcpy(buffer + 20, issueDate,      INVOICE_DATE_LENGTH);
    std::memcpy(buffer + 32, dueDate,        INVOICE_DATE_LENGTH);
    std::memcpy(buffer + 44, &subtotal,      sizeof(subtotal));
    std::memcpy(buffer + 52, &taxAmount,     sizeof(taxAmount));
    std::memcpy(buffer + 60, &total,         sizeof(total));
    int s = static_cast<int>(status);
    std::memcpy(buffer + 68, &s,             sizeof(s));
    unsigned char flag = isDeleted ? 1u : 0u;
    std::memcpy(buffer + 72, &flag,          sizeof(flag));
}

void Invoice::deserialize(const char* buffer)
{
    std::memcpy(&id,            buffer + 0,  sizeof(id));
    std::memcpy(invoiceNumber,  buffer + 2,  INVOICE_NUMBER_LENGTH);
    invoiceNumber[INVOICE_NUMBER_LENGTH - 1] = '\0';
    std::memcpy(&customerId,    buffer + 18, sizeof(customerId));
    std::memcpy(issueDate,      buffer + 20, INVOICE_DATE_LENGTH);
    issueDate[INVOICE_DATE_LENGTH - 1] = '\0';
    std::memcpy(dueDate,        buffer + 32, INVOICE_DATE_LENGTH);
    dueDate[INVOICE_DATE_LENGTH - 1] = '\0';
    std::memcpy(&subtotal,      buffer + 44, sizeof(subtotal));
    std::memcpy(&taxAmount,     buffer + 52, sizeof(taxAmount));
    std::memcpy(&total,         buffer + 60, sizeof(total));
    int s;
    std::memcpy(&s,             buffer + 68, sizeof(s));
    InvoiceStatus decoded = static_cast<InvoiceStatus>(s);
    status = isKnownStatus(decoded) ? decoded : INVOICE_STATUS_UNKNOWN;
    unsigned char flag;
    std::memcpy(&flag,          buffer + 72, sizeof(flag));
    isDeleted = (flag != 0);
}

void Invoice::display() const
{
    const char* statusStr =
        (status == INVOICE_DRAFT)   ? "Draft"   :
        (status == INVOICE_POSTED)  ? "Posted"  :
        (status == INVOICE_PAID)    ? "Paid"    :
        (status == INVOICE_OVERDUE) ? "Overdue" :
        (status == INVOICE_VOID)    ? "Void"    : "Unknown";

    std::cout << "[INVOICE] ID:" << id
              << " #" << invoiceNumber
              << " Customer:" << customerId
              << " Issue:" << issueDate
              << " Due:" << dueDate
              << " Total:" << total
              << " Status:" << statusStr
              << " Deleted:" << (isDeleted ? "yes" : "no") << "\n";
}

unsigned short int Invoice::getId() const                   { return id; }
void Invoice::setId(unsigned short int newId)               { id = newId; }

const char* Invoice::getInvoiceNumber() const               { return invoiceNumber; }
void Invoice::setInvoiceNumber(const char* newNumber)       { copyField(invoiceNumber, INVOICE_NUMBER_LENGTH, newNumber); }

unsigned short int Invoice::getCustomerId() const           { return customerId; }
void Invoice::setCustomerId(unsigned short int newCustomerId) { customerId = newCustomerId; }

const char* Invoice::getIssueDate() const                   { return issueDate; }
void Invoice::setIssueDate(const char* newDate)             { copyField(issueDate, INVOICE_DATE_LENGTH, newDate); }

const char* Invoice::getDueDate() const                     { return dueDate; }
void Invoice::setDueDate(const char* newDate)               { copyField(dueDate, INVOICE_DATE_LENGTH, newDate); }

double Invoice::getSubtotal() const                         { return subtotal; }
void Invoice::setSubtotal(double newSubtotal)
{
    if (newSubtotal < 0) return;
    subtotal = newSubtotal;
}

double Invoice::getTaxAmount() const                        { return taxAmount; }
void Invoice::setTaxAmount(double newTax)
{
    if (newTax < 0) return;
    taxAmount = newTax;
}

double Invoice::getTotal() const                            { return total; }
void Invoice::setTotal(double newTotal)
{
    if (newTotal < 0) return;
    total = newTotal;
}

InvoiceStatus Invoice::getStatus() const                    { return status; }
void Invoice::setStatus(InvoiceStatus newStatus)
{
    if (!isKnownStatus(newStatus)) return;
    status = newStatus;
}

bool Invoice::getIsDeleted() const                          { return isDeleted; }
void Invoice::setIsDeleted(bool newIsDeleted)               { isDeleted = newIsDeleted; }
