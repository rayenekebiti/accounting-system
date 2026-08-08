#ifndef CORE_INVOICE_H
#define CORE_INVOICE_H
#include <cstddef>
#include <cstdint>
#include "Money.h"
#include "IsoDate.h"

inline constexpr std::size_t INVOICE_RECORD_SIZE   = 96;
inline constexpr std::size_t INVOICE_NUMBER_LENGTH = 16;
inline constexpr std::size_t INVOICE_DATE_LENGTH   = 12;

// Byte offset of the isDeleted flag within a serialized Invoice record.
inline constexpr std::size_t INVOICE_DELETED_OFFSET = 76;

enum InvoiceStatus
{
    INVOICE_DRAFT,
    INVOICE_POSTED,
    INVOICE_PAID,
    INVOICE_OVERDUE,
    INVOICE_VOID,
    INVOICE_STATUS_UNKNOWN
};

struct InvoiceData
{
    uint32_t      id;
    const char*   invoiceNumber;   // e.g. "INV-1024"
    uint32_t      customerId;
    IsoDate       issueDate;
    IsoDate       dueDate;
    Money         subtotal;
    Money         taxAmount;
    Money         total;
    InvoiceStatus status;
    bool          isDeleted;
};

class Invoice
{
    uint32_t      id;
    char          invoiceNumber[INVOICE_NUMBER_LENGTH];
    uint32_t      customerId;
    IsoDate       issueDate;
    IsoDate       dueDate;
    Money         subtotal;
    Money         taxAmount;
    Money         total;
    InvoiceStatus status;
    bool          isDeleted;

public:
    Invoice();
    explicit Invoice(const InvoiceData& info);

    bool isValid() const;                       // number non-empty, status known, totals >=0

    void serialize(char* buffer) const;
    void deserialize(const char* buffer);

    uint32_t getId() const;
    void setId(uint32_t newId);

    const char* getInvoiceNumber() const;
    void setInvoiceNumber(const char* newNumber);

    uint32_t getCustomerId() const;
    void setCustomerId(uint32_t newCustomerId);

    IsoDate getIssueDate() const;
    void setIssueDate(IsoDate newDate);

    IsoDate getDueDate() const;
    void setDueDate(IsoDate newDate);

    Money getSubtotal() const;
    void setSubtotal(Money newSubtotal);

    Money getTaxAmount() const;
    void setTaxAmount(Money newTax);

    Money getTotal() const;
    void setTotal(Money newTotal);

    InvoiceStatus getStatus() const;
    void setStatus(InvoiceStatus newStatus);

    bool getIsDeleted() const;
    void setIsDeleted(bool newIsDeleted);
};

#endif
