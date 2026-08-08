#ifndef CORE_INVOICE_LINE_H
#define CORE_INVOICE_LINE_H
#include <cstddef>
#include <cstdint>
#include "Money.h"

inline constexpr std::size_t INVOICE_LINE_RECORD_SIZE    = 128;
inline constexpr std::size_t INVOICE_LINE_DESC_LENGTH    = 64;
inline constexpr std::size_t INVOICE_LINE_DELETED_OFFSET = 98;

struct InvoiceLineData {
    uint32_t    id                 = 0;
    uint32_t    invoiceId          = 0;
    uint32_t    productId          = 0;      // 0 = ad-hoc (no linked product)
    const char* description        = nullptr;
    int32_t     quantityMilliunits = 1000;   // quantity × 1000 (1000 = qty 1.000)
    Money       unitPrice;
    int16_t     taxRatePermille    = 0;      // e.g. 190 = 19.0 %
    Money       lineTotal;                   // cached: unitPrice * qty * (1 + tax/1000)
    bool        isDeleted          = false;
};

class InvoiceLine {
    uint32_t id                 = 0;
    uint32_t invoiceId          = 0;
    uint32_t productId          = 0;
    char     description[INVOICE_LINE_DESC_LENGTH] = {};
    int32_t  quantityMilliunits = 1000;
    Money    unitPrice;
    int16_t  taxRatePermille    = 0;
    Money    lineTotal;
    bool     isDeleted          = false;

public:
    InvoiceLine() = default;
    explicit InvoiceLine(const InvoiceLineData& info);

    void serialize(char* buffer) const;
    void deserialize(const char* buffer);

    // Recompute lineTotal from the other fields (unit price * qty * (1 + tax)).
    void recompute();

    uint32_t    getId()                  const { return id; }
    void        setId(uint32_t v)              { id = v; }
    uint32_t    getInvoiceId()           const { return invoiceId; }
    void        setInvoiceId(uint32_t v)       { invoiceId = v; }
    uint32_t    getProductId()           const { return productId; }
    void        setProductId(uint32_t v)       { productId = v; }
    const char* getDescription()         const { return description; }
    void        setDescription(const char* s);
    int32_t     getQuantityMilliunits()  const { return quantityMilliunits; }
    void        setQuantityMilliunits(int32_t v) { quantityMilliunits = v; }
    double      getQuantity()            const { return quantityMilliunits / 1000.0; }
    Money       getUnitPrice()           const { return unitPrice; }
    void        setUnitPrice(Money v)          { unitPrice = v; }
    int16_t     getTaxRatePermille()     const { return taxRatePermille; }
    void        setTaxRatePermille(int16_t v)  { taxRatePermille = v; }
    Money       getLineTotal()           const { return lineTotal; }
    bool        getIsDeleted()           const { return isDeleted; }
    void        setIsDeleted(bool v)           { isDeleted = v; }
};

#endif
