#ifndef CORE_ISO_DATE_H
#define CORE_ISO_DATE_H

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

// IsoDate — locale-free YYYY-MM-DD calendar date.
//
// Why: the legacy storage layer wrote dates as "5 Jun 2026" — that string
// only round-trips on machines whose Qt locale matches the writer's, so a
// French ↔ English mix silently dropped records out of every report.
// Storing year/month/day as small integers lets us serialize as a stable
// 10-byte ASCII string and compare lexicographically.
//
// A default-constructed IsoDate is *invalid* (isValid() == false). Use it
// as a sentinel for "no date set"; do not arithmetic with it.
class IsoDate {
    std::int16_t year_  = 0;     // 1900..9999 once valid
    std::uint8_t month_ = 0;     // 1..12 once valid
    std::uint8_t day_   = 0;     // 1..daysInMonth(year, month) once valid

public:
    constexpr IsoDate() = default;
    IsoDate(int year, int month, int day);   // throws std::invalid_argument

    static std::optional<IsoDate> tryMake (int year, int month, int day);
    static std::optional<IsoDate> fromString(std::string_view yyyymmdd);

    constexpr int year()  const { return year_;  }
    constexpr int month() const { return month_; }
    constexpr int day()   const { return day_;   }

    constexpr bool isValid() const {
        return year_ >= 1900 && year_ <= 9999
            && month_ >= 1 && month_ <= 12
            && day_   >= 1 && day_   <= 31;
    }

    // "YYYY-MM-DD" — always 10 chars when valid, empty string when not.
    std::string toString() const;

    static bool isLeapYear  (int year);
    static int  daysInMonth (int year, int month);

    constexpr bool operator==(IsoDate rhs) const {
        return year_ == rhs.year_ && month_ == rhs.month_ && day_ == rhs.day_;
    }
    constexpr bool operator!=(IsoDate rhs) const { return !(*this == rhs); }
    constexpr bool operator< (IsoDate rhs) const {
        if (year_  != rhs.year_)  return year_  < rhs.year_;
        if (month_ != rhs.month_) return month_ < rhs.month_;
        return day_ < rhs.day_;
    }
    constexpr bool operator<=(IsoDate rhs) const { return *this < rhs || *this == rhs; }
    constexpr bool operator> (IsoDate rhs) const { return rhs < *this; }
    constexpr bool operator>=(IsoDate rhs) const { return !(*this < rhs); }
};

#endif
