#include "../tiny_test.h"
#include "../../core/Money.h"

TEST_CASE("Money default is zero")
{
    Money m;
    CHECK(m.cents() == 0);
    CHECK(m.isZero());
    CHECK(!m.isNegative());
}

TEST_CASE("Money fromMajor and fromCents agree")
{
    CHECK(Money::fromMajor(12).cents()   == 1200);
    CHECK(Money::fromCents(1200).cents() == 1200);
    CHECK(Money::fromMajor(12) == Money::fromCents(1200));
}

TEST_CASE("Money::fromDouble rounds half-away-from-zero")
{
    CHECK(Money::fromDouble(0.005).cents()  ==  1);
    CHECK(Money::fromDouble(-0.005).cents() == -1);
    CHECK(Money::fromDouble(1.234).cents()  == 123);
    CHECK(Money::fromDouble(-1.234).cents() == -123);
    CHECK(Money::fromDouble(0.0).cents()    ==  0);
}

TEST_CASE("Money arithmetic stays exact in cents")
{
    Money a = Money::fromCents(1099);
    Money b = Money::fromCents( 250);

    CHECK((a + b).cents() == 1349);
    CHECK((a - b).cents() ==  849);
    CHECK((-a).cents()    == -1099);
    CHECK((a * 3).cents() == 3297);
    CHECK((3 * a).cents() == 3297);

    Money c = a;
    c += b;
    CHECK(c.cents() == 1349);
    c -= b;
    CHECK(c.cents() == 1099);
}

TEST_CASE("Money::scaledBy rounds half-away-from-zero")
{
    // 100 cents * 0.19 = 19 cents exactly.
    CHECK(Money::fromCents(100).scaledBy(0.19).cents() == 19);
    // 123 cents * 0.075 = 9.225 → 9 cents (banker's-half-away rounds .225 down toward zero? No: 9.225 rounds to 9 because 0.225 < 0.5).
    CHECK(Money::fromCents(123).scaledBy(0.075).cents() == 9);
    // Negative path.
    CHECK(Money::fromCents(-100).scaledBy(0.19).cents() == -19);
}

TEST_CASE("Money comparisons order by cents")
{
    Money a = Money::fromCents(100);
    Money b = Money::fromCents(200);
    CHECK(a < b);
    CHECK(b > a);
    CHECK(a <= a);
    CHECK(a >= a);
    CHECK(a != b);
    CHECK(a == Money::fromCents(100));
}

TEST_CASE("Money::formatPlain produces fixed two-decimal output")
{
    CHECK(Money::fromCents(0).formatPlain()      == "0.00");
    CHECK(Money::fromCents(7).formatPlain()      == "0.07");
    CHECK(Money::fromCents(125).formatPlain()    == "1.25");
    CHECK(Money::fromCents(123456).formatPlain() == "1234.56");
    CHECK(Money::fromCents(-7).formatPlain()     == "-0.07");
    CHECK(Money::fromCents(-1099).formatPlain()  == "-10.99");
}

TEST_CASE("Money::toDouble round-trips small magnitudes exactly enough")
{
    for (std::int64_t c : { -1234567, -1, 0, 1, 50, 99, 100, 12345 }) {
        Money m = Money::fromCents(c);
        Money back = Money::fromDouble(m.toDouble());
        CHECK(back.cents() == c);
    }
}
