#include "../tiny_test.h"
#include "../../core/IsoDate.h"

#include <stdexcept>

TEST_CASE("IsoDate default is invalid")
{
    IsoDate d;
    CHECK(!d.isValid());
    CHECK(d.toString().empty());
}

TEST_CASE("IsoDate valid construction")
{
    IsoDate d(2026, 6, 10);
    CHECK(d.isValid());
    CHECK(d.year()  == 2026);
    CHECK(d.month() == 6);
    CHECK(d.day()   == 10);
    CHECK(d.toString() == "2026-06-10");
}

TEST_CASE("IsoDate::tryMake rejects bad inputs")
{
    CHECK(!IsoDate::tryMake(1899, 1, 1).has_value());      // year too early
    CHECK(!IsoDate::tryMake(2026, 0, 10).has_value());     // month underflow
    CHECK(!IsoDate::tryMake(2026, 13, 10).has_value());    // month overflow
    CHECK(!IsoDate::tryMake(2026, 2, 30).has_value());     // Feb 30
    CHECK(!IsoDate::tryMake(2025, 2, 29).has_value());     // non-leap Feb 29
    CHECK( IsoDate::tryMake(2024, 2, 29).has_value());     // leap Feb 29
    CHECK( IsoDate::tryMake(2000, 2, 29).has_value());     // div-400 leap
    CHECK(!IsoDate::tryMake(1900, 2, 29).has_value());     // div-100 non-leap
}

TEST_CASE("IsoDate constructor throws on invalid Y/M/D")
{
    bool threw = false;
    try { IsoDate d(2025, 2, 29); }
    catch (const std::invalid_argument&) { threw = true; }
    CHECK(threw);
}

TEST_CASE("IsoDate::fromString parses canonical form")
{
    auto d = IsoDate::fromString("2026-06-10");
    REQUIRE(d.has_value());
    CHECK(d->year() == 2026);
    CHECK(d->month() == 6);
    CHECK(d->day() == 10);
}

TEST_CASE("IsoDate::fromString rejects malformed strings")
{
    CHECK(!IsoDate::fromString("").has_value());
    CHECK(!IsoDate::fromString("2026-6-10").has_value());      // missing zero-pad
    CHECK(!IsoDate::fromString("2026/06/10").has_value());     // wrong separator
    CHECK(!IsoDate::fromString("06-10-2026").has_value());     // wrong order
    CHECK(!IsoDate::fromString("2026-06-10 ").has_value());    // trailing space
    CHECK(!IsoDate::fromString("2026-13-01").has_value());     // bad month
    CHECK(!IsoDate::fromString("abcd-ef-gh").has_value());     // non-digit
}

TEST_CASE("IsoDate compares lexicographically")
{
    IsoDate a(2026, 1, 31);
    IsoDate b(2026, 2,  1);
    IsoDate c(2026, 2,  1);
    CHECK(a < b);
    CHECK(b > a);
    CHECK(b == c);
    CHECK(a != b);
    CHECK(b <= c);
    CHECK(b >= c);
}

TEST_CASE("IsoDate string round-trips for representative dates")
{
    for (const char* s : { "1900-01-01", "2000-02-29", "2026-06-10", "9999-12-31" }) {
        auto d = IsoDate::fromString(s);
        REQUIRE(d.has_value());
        CHECK(d->toString() == s);
    }
}

TEST_CASE("IsoDate::daysInMonth covers the calendar")
{
    CHECK(IsoDate::daysInMonth(2026,  1) == 31);
    CHECK(IsoDate::daysInMonth(2026,  2) == 28);
    CHECK(IsoDate::daysInMonth(2024,  2) == 29);
    CHECK(IsoDate::daysInMonth(2026,  4) == 30);
    CHECK(IsoDate::daysInMonth(2026, 12) == 31);
    CHECK(IsoDate::daysInMonth(2026,  0) ==  0);
    CHECK(IsoDate::daysInMonth(2026, 13) ==  0);
}
