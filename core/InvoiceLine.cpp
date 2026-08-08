#include "InvoiceLine.h"
#include <cstring>
#include <cmath>

InvoiceLine::InvoiceLine(const InvoiceLineData& info)
{
    id                  = info.id;
    invoiceId           = info.invoiceId;
    productId           = info.productId;
    quantityMilliunits  = info.quantityMilliunits;
    unitPrice           = info.unitPrice;
    taxRatePermille     = info.taxRatePermille;
    lineTotal           = info.lineTotal;
    isDeleted           = info.isDeleted;
    std::memset(description, 0, INVOICE_LINE_DESC_LENGTH);
    if (info.description)
        std::strncpy(description, info.description, INVOICE_LINE_DESC_LENGTH - 1);
}

void InvoiceLine::setDescription(const char* s)
{
    std::memset(description, 0, INVOICE_LINE_DESC_LENGTH);
    if (s) std::strncpy(description, s, INVOICE_LINE_DESC_LENGTH - 1);
}

void InvoiceLine::recompute()
{
    const double qty    = quantityMilliunits / 1000.0;
    const double lineSub = unitPrice.toDouble() * qty;
    const double tax    = lineSub * (taxRatePermille / 1000.0);
    lineTotal = Money::fromDouble(lineSub + tax);
}

// Binary layout (INVOICE_LINE_RECORD_SIZE = 128 bytes):
//   0..3    id                (uint32_t)
//   4..7    invoiceId         (uint32_t)
//   8..11   productId         (uint32_t)
//  12..75   description       (char[64])
//  76..79   quantityMilliunits(int32_t)
//  80..87   unitPrice         (int64_t cents)
//  88..89   taxRatePermille   (int16_t)
//  90..97   lineTotal         (int64_t cents)
//  98       isDeleted         (uint8_t)  ← INVOICE_LINE_DELETED_OFFSET
//  99..127  padding           (29 bytes)
static_assert(INVOICE_LINE_DELETED_OFFSET == 98,
    "keep in sync with serialize layout");
static_assert(4+4+4+64+4+8+2+8+1 <= INVOICE_LINE_RECORD_SIZE,
    "InvoiceLine fields exceed INVOICE_LINE_RECORD_SIZE");

void InvoiceLine::serialize(char* buf) const
{
    std::memset(buf, 0, INVOICE_LINE_RECORD_SIZE);
    std::memcpy(buf +  0, &id,                 sizeof(id));
    std::memcpy(buf +  4, &invoiceId,           sizeof(invoiceId));
    std::memcpy(buf +  8, &productId,           sizeof(productId));
    std::memcpy(buf + 12, description,          INVOICE_LINE_DESC_LENGTH);
    std::memcpy(buf + 76, &quantityMilliunits,  sizeof(quantityMilliunits));
    int64_t up = unitPrice.cents();
    std::memcpy(buf + 80, &up,                  sizeof(up));
    std::memcpy(buf + 88, &taxRatePermille,     sizeof(taxRatePermille));
    int64_t lt = lineTotal.cents();
    std::memcpy(buf + 90, &lt,                  sizeof(lt));
    unsigned char flag = isDeleted ? 1u : 0u;
    std::memcpy(buf + 98, &flag,                sizeof(flag));
}

void InvoiceLine::deserialize(const char* buf)
{
    std::memcpy(&id,                buf +  0, sizeof(id));
    std::memcpy(&invoiceId,         buf +  4, sizeof(invoiceId));
    std::memcpy(&productId,         buf +  8, sizeof(productId));
    std::memcpy(description,        buf + 12, INVOICE_LINE_DESC_LENGTH);
    description[INVOICE_LINE_DESC_LENGTH - 1] = '\0';
    std::memcpy(&quantityMilliunits, buf + 76, sizeof(quantityMilliunits));
    int64_t up;
    std::memcpy(&up,                buf + 80, sizeof(up));
    unitPrice = Money::fromCents(up);
    std::memcpy(&taxRatePermille,   buf + 88, sizeof(taxRatePermille));
    int64_t lt;
    std::memcpy(&lt,                buf + 90, sizeof(lt));
    lineTotal = Money::fromCents(lt);
    unsigned char flag;
    std::memcpy(&flag,              buf + 98, sizeof(flag));
    isDeleted = (flag != 0);
}
