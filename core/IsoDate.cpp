#include "IsoDate.h"

#include <cstdio>
#include <stdexcept>

bool IsoDate::isLeapYear(int year)
{
    if (year % 400 == 0) return true;
    if (year % 100 == 0) return false;
    return year % 4 == 0;
}

int IsoDate::daysInMonth(int year, int month)
{
    if (month < 1 || month > 12) return 0;
    static constexpr int days[12] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
    if (month == 2 && isLeapYear(year)) return 29;
    return days[month - 1];
}

std::optional<IsoDate> IsoDate::tryMake(int year, int month, int day)
{
    if (year < 1900 || year > 9999) return std::nullopt;
    if (month < 1 || month > 12)    return std::nullopt;
    if (day   < 1 || day > daysInMonth(year, month)) return std::nullopt;
    IsoDate d;
    d.year_  = static_cast<std::int16_t>(year);
    d.month_ = static_cast<std::uint8_t>(month);
    d.day_   = static_cast<std::uint8_t>(day);
    return d;
}

IsoDate::IsoDate(int year, int month, int day)
{
    auto made = tryMake(year, month, day);
    if (!made) throw std::invalid_argument("IsoDate: out-of-range Y/M/D");
    *this = *made;
}

namespace {
// Parse exactly N decimal digits from `s` starting at `pos`, advancing pos.
// Returns -1 on failure.
int readDigits(std::string_view s, std::size_t& pos, std::size_t n)
{
    if (pos + n > s.size()) return -1;
    int value = 0;
    for (std::size_t i = 0; i < n; ++i) {
        const char c = s[pos + i];
        if (c < '0' || c > '9') return -1;
        value = value * 10 + (c - '0');
    }
    pos += n;
    return value;
}
}

std::optional<IsoDate> IsoDate::fromString(std::string_view yyyymmdd)
{
    if (yyyymmdd.size() != 10) return std::nullopt;
    std::size_t pos = 0;
    const int y = readDigits(yyyymmdd, pos, 4);
    if (y < 0 || yyyymmdd[pos] != '-') return std::nullopt;
    ++pos;
    const int m = readDigits(yyyymmdd, pos, 2);
    if (m < 0 || yyyymmdd[pos] != '-') return std::nullopt;
    ++pos;
    const int d = readDigits(yyyymmdd, pos, 2);
    if (d < 0) return std::nullopt;
    return tryMake(y, m, d);
}

std::string IsoDate::toString() const
{
    if (!isValid()) return {};
    char buf[11];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", year_, month_, day_);
    return std::string{buf};
}
