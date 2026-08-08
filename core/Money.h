#ifndef CORE_MONEY_H
#define CORE_MONEY_H

#include <cstdint>
#include <string>

// Money — int64 cents value type. Replaces `double` in every financial field
// so totals, balances, and rounding behave deterministically.
//
// Representation: cents_ is the signed minor unit (e.g. cents for USD/EUR).
// One unit of the major currency = 100 cents. Range ±~92 quadrillion cents,
// which is ~$92 quadrillion — plenty for any books we will ever write.
//
// All arithmetic is exact: + and - act on cents directly, scaling by an int
// is exact, scaling by a rate is rounded half-away-from-zero. Conversions to
// and from double exist for the UI boundary only — never round-trip money
// through double inside the domain.
class Money {
    std::int64_t cents_ = 0;

public:
    constexpr Money() = default;
    constexpr explicit Money(std::int64_t cents) : cents_(cents) {}

    static constexpr Money fromCents(std::int64_t cents) { return Money{cents}; }
    static constexpr Money fromMajor(std::int64_t major) { return Money{major * 100}; }

    // Half-away-from-zero rounding so 0.005 → 1 cent, -0.005 → -1 cent.
    static Money fromDouble(double major);

    constexpr std::int64_t cents() const { return cents_; }
    constexpr double       toDouble() const { return static_cast<double>(cents_) / 100.0; }
    constexpr bool         isZero() const { return cents_ == 0; }
    constexpr bool         isNegative() const { return cents_ < 0; }

    // "1234.56" / "-1234.56" / "0.00". Always two decimals, no thousands
    // separator, locale-free. Use a Qt formatter for user-facing display.
    std::string formatPlain() const;

    constexpr Money  operator+(Money rhs) const { return Money{cents_ + rhs.cents_}; }
    constexpr Money  operator-(Money rhs) const { return Money{cents_ - rhs.cents_}; }
    constexpr Money  operator-() const          { return Money{-cents_}; }
    constexpr Money& operator+=(Money rhs)      { cents_ += rhs.cents_; return *this; }
    constexpr Money& operator-=(Money rhs)      { cents_ -= rhs.cents_; return *this; }

    constexpr Money operator*(std::int64_t n) const { return Money{cents_ * n}; }

    // Scale by a rational rate (e.g. tax 0.19). Rounds half-away-from-zero.
    Money scaledBy(double rate) const;

    constexpr bool operator==(Money rhs) const { return cents_ == rhs.cents_; }
    constexpr bool operator!=(Money rhs) const { return cents_ != rhs.cents_; }
    constexpr bool operator< (Money rhs) const { return cents_ <  rhs.cents_; }
    constexpr bool operator<=(Money rhs) const { return cents_ <= rhs.cents_; }
    constexpr bool operator> (Money rhs) const { return cents_ >  rhs.cents_; }
    constexpr bool operator>=(Money rhs) const { return cents_ >= rhs.cents_; }
};

constexpr Money operator*(std::int64_t n, Money m) { return m * n; }

#endif
