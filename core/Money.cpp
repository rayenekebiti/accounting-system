#include "Money.h"

#include <cmath>
#include <cstdint>
#include <cstdio>

namespace {
// Convert a rounded double to int64 WITHOUT undefined behaviour. Casting an out-of-range
// or non-finite double to an integer is UB in C++ (and yields garbage in practice), so a
// value that can't fit an int64 (a non-finite or absurd amount that slipped past input
// validation) is clamped to the representable range — a defined result, never UB. Callers
// still validate sane bounds at the UI boundary; this is the last-line safety net.
std::int64_t saturateToInt64(double rounded)
{
    if (std::isnan(rounded)) return 0;
    // 2^63 is not exactly representable but is a safe upper fence; anything ≥ it overflows.
    constexpr double kMax = 9223372036854775808.0;   // 2^63
    if (rounded >=  kMax) return INT64_MAX;
    if (rounded <  -kMax) return INT64_MIN;
    return static_cast<std::int64_t>(rounded);
}
} // namespace

Money Money::fromDouble(double major)
{
    const double cents = major * 100.0;
    const double rounded = cents >= 0.0 ? std::floor(cents + 0.5)
                                        : std::ceil (cents - 0.5);
    return Money{saturateToInt64(rounded)};
}

Money Money::scaledBy(double rate) const
{
    const double scaled = static_cast<double>(cents_) * rate;
    const double rounded = scaled >= 0.0 ? std::floor(scaled + 0.5)
                                         : std::ceil (scaled - 0.5);
    return Money{saturateToInt64(rounded)};
}

std::string Money::formatPlain() const
{
    // Largest int64 is 19 digits; sign + decimal point + null = 23 chars max.
    char buf[32];
    const std::int64_t abs = cents_ < 0 ? -cents_ : cents_;
    const std::int64_t whole = abs / 100;
    const int          frac  = static_cast<int>(abs % 100);
    std::snprintf(buf, sizeof(buf), "%s%lld.%02d",
                  cents_ < 0 ? "-" : "",
                  static_cast<long long>(whole),
                  frac);
    return std::string{buf};
}
