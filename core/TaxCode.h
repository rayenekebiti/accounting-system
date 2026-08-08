#ifndef CORE_TAX_CODE_H
#define CORE_TAX_CODE_H
#include <cstddef>
#include <cstdint>
#include "IsoDate.h"

// A tax code is an authoritative, append-only fact (TaxCodeCreated event). It is NEVER mutated:
// a rate change is a NEW version of the same `family`, effective from a new date. An invoice/
// expense captures the resolved RATE at authoring (baked into the ledger posting), so changing
// today's rate can never reinterpret a historically-authored transaction.
inline constexpr std::size_t TAX_CODE_NAME_LENGTH   = 32;
inline constexpr std::size_t TAX_CODE_PAYLOAD_SIZE  = 56;   // fixed event payload

enum TaxType : uint8_t {
    TAX_TYPE_VAT        = 0,
    TAX_TYPE_GST        = 1,
    TAX_TYPE_SALES      = 2,   // Sales Tax
    TAX_TYPE_ZERO_RATED = 3,   // 0% but reportable (distinct from exempt)
    TAX_TYPE_EXEMPT     = 4,   // no tax line
};

struct TaxCode {
    uint32_t id             = 0;
    uint16_t family         = 0;    // groups versions of the same logical code
    uint16_t version        = 1;    // 1-based within a family
    uint8_t  type           = TAX_TYPE_VAT;
    int32_t  ratePermille   = 0;    // e.g. 150 = 15.0% (÷1000); 0 for zero-rated/exempt
    IsoDate  effectiveDate;
    char     name[TAX_CODE_NAME_LENGTH] = {};

    void setName(const char* n);

    // Serialize / deserialize the fixed event payload (TAX_CODE_PAYLOAD_SIZE bytes). Layout:
    //   0..3  id | 4..5 family | 6..7 version | 8 type | 9 pad | 10..13 ratePermille (i32)
    //   14..23 effectiveDate (10 ASCII) | 24..55 name (32)
    void serialize(char* buffer) const;
    void deserialize(const char* buffer);
};

// Display helper — the human name of a tax type.
const char* taxTypeName(uint8_t type);

// The tax on a net amount at a per-mille rate — the SINGLE deterministic formula used both when
// authoring a posting and when re-deriving it for a void/reversal, so the compensating entry
// cancels EXACTLY. Truncating integer arithmetic (no float, no locale). netCents ≤ ~1e14 and
// rate ≤ ~1000 → the product stays well inside int64.
inline int64_t taxOnNet(int64_t netCents, int32_t ratePermille)
{
    return (netCents * static_cast<int64_t>(ratePermille)) / 1000;
}

#endif // CORE_TAX_CODE_H
