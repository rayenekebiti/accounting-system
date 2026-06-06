#ifndef CORE_INVOICE_H
#define CORE_INVOICE_H
#include <cstddef>

inline constexpr std::size_t INVOICE_RECORD_SIZE   = 96;
inline constexpr std::size_t INVOICE_NUMBER_LENGTH = 16;
inline constexpr std::size_t INVOICE_DATE_LENGTH   = 12;

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
    unsigned short int id;
    const char*        invoiceNumber;   // e.g. "INV-1024"
    unsigned short int customerId;
    const char*        issueDate;       // "DD MMM YYYY"
    const char*        dueDate;
    double             subtotal;
    double             taxAmount;
    double             total;
    InvoiceStatus      status;
    bool               isDeleted;
};

class Invoice
{
    unsigned short int id;
    char               invoiceNumber[INVOICE_NUMBER_LENGTH];
    unsigned short int customerId;
    char               issueDate[INVOICE_DATE_LENGTH];
    char               dueDate[INVOICE_DATE_LENGTH];
    double             subtotal;
    double             taxAmount;
    double             total;
    InvoiceStatus      status;
    bool               isDeleted;

public:
    Invoice();
    explicit Invoice(const InvoiceData& info);

    bool isValid() const;                       // number non-empty, status known, totals >=0

    void serialize(char* buffer) const;
    void deserialize(const char* buffer);
    void display() const;

    unsigned short int getId() const;
    void setId(unsigned short int newId);

    const char* getInvoiceNumber() const;
    void setInvoiceNumber(const char* newNumber);

    unsigned short int getCustomerId() const;
    void setCustomerId(unsigned short int newCustomerId);

    const char* getIssueDate() const;
    void setIssueDate(const char* newDate);

    const char* getDueDate() const;
    void setDueDate(const char* newDate);

    double getSubtotal() const;
    void setSubtotal(double newSubtotal);

    double getTaxAmount() const;
    void setTaxAmount(double newTax);

    double getTotal() const;
    void setTotal(double newTotal);

    InvoiceStatus getStatus() const;
    void setStatus(InvoiceStatus newStatus);

    bool getIsDeleted() const;
    void setIsDeleted(bool newIsDeleted);
};

#endif
