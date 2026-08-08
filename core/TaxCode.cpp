#include "TaxCode.h"
#include <cstring>

void TaxCode::setName(const char* n)
{
    std::memset(name, 0, TAX_CODE_NAME_LENGTH);
    if (!n) return;
    std::strncpy(name, n, TAX_CODE_NAME_LENGTH - 1);
    name[TAX_CODE_NAME_LENGTH - 1] = '\0';
}

void TaxCode::serialize(char* buffer) const
{
    std::memset(buffer, 0, TAX_CODE_PAYLOAD_SIZE);
    std::memcpy(buffer + 0, &id,      4);
    std::memcpy(buffer + 4, &family,  2);
    std::memcpy(buffer + 6, &version, 2);
    buffer[8] = static_cast<char>(type);
    std::memcpy(buffer + 10, &ratePermille, 4);
    const std::string d = effectiveDate.toString();          // "" or 10 chars
    std::memcpy(buffer + 14, d.data(), d.size() > 10 ? 10 : d.size());
    std::memcpy(buffer + 24, name, TAX_CODE_NAME_LENGTH);
}

void TaxCode::deserialize(const char* buffer)
{
    std::memcpy(&id,      buffer + 0, 4);
    std::memcpy(&family,  buffer + 4, 2);
    std::memcpy(&version, buffer + 6, 2);
    type = static_cast<uint8_t>(buffer[8]);
    std::memcpy(&ratePermille, buffer + 10, 4);
    char ds[11] = {};
    std::memcpy(ds, buffer + 14, 10);
    auto d = IsoDate::fromString(std::string(ds));
    effectiveDate = d ? *d : IsoDate{};
    std::memcpy(name, buffer + 24, TAX_CODE_NAME_LENGTH);
    name[TAX_CODE_NAME_LENGTH - 1] = '\0';
}

const char* taxTypeName(uint8_t type)
{
    switch (type) {
    case TAX_TYPE_VAT:        return "VAT";
    case TAX_TYPE_GST:        return "GST";
    case TAX_TYPE_SALES:      return "Sales Tax";
    case TAX_TYPE_ZERO_RATED: return "Zero-rated";
    case TAX_TYPE_EXEMPT:     return "Exempt";
    default:                  return "—";
    }
}
