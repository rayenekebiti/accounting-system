// tiny_test.h — minimal placeholder for doctest.
//
// The improvement plan calls for vendoring doctest (single-header) so unit
// tests get its rich diagnostics. To keep the first PR self-contained and
// network-free, we ship this ~80-line shim that exposes the same surface
// (TEST_CASE / SUBCASE / CHECK / REQUIRE) plus a built-in main. Drop the
// real doctest.h in this directory and delete this file once available —
// no call-site changes needed.
//
// Failure prints "FAIL file:line  expr" and increments a counter. Process
// exits 0 if all checks passed, 1 otherwise.
#ifndef TINY_TEST_H
#define TINY_TEST_H

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace tiny_test {

struct TestCase {
    std::string name;
    std::function<void()> fn;
};

inline std::vector<TestCase>& registry()
{
    static std::vector<TestCase> r;
    return r;
}

inline int& failureCount()
{
    static int n = 0;
    return n;
}

inline int& checkCount()
{
    static int n = 0;
    return n;
}

struct Registrar {
    Registrar(const std::string& name, std::function<void()> fn) {
        registry().push_back({name, std::move(fn)});
    }
};

inline void reportFailure(const char* file, int line, const char* expr)
{
    ++failureCount();
    std::fprintf(stderr, "  FAIL %s:%d  %s\n", file, line, expr);
}

inline int runAll()
{
    int caseCount = 0;
    for (const auto& tc : registry()) {
        const int failuresBefore = failureCount();
        std::fprintf(stdout, "[RUN ] %s\n", tc.name.c_str());
        tc.fn();
        ++caseCount;
        if (failureCount() == failuresBefore)
            std::fprintf(stdout, "[ OK ] %s\n", tc.name.c_str());
        else
            std::fprintf(stdout, "[FAIL] %s\n", tc.name.c_str());
    }
    std::fprintf(stdout, "\n%d test case(s), %d check(s), %d failure(s)\n",
                 caseCount, checkCount(), failureCount());
    return failureCount() == 0 ? 0 : 1;
}

} // namespace tiny_test

#define TT_CONCAT_INNER(a, b) a##b
#define TT_CONCAT(a, b)       TT_CONCAT_INNER(a, b)

#define TEST_CASE(name)                                                       \
    static void TT_CONCAT(tt_fn_, __LINE__)();                                \
    static ::tiny_test::Registrar TT_CONCAT(tt_reg_, __LINE__){               \
        name, &TT_CONCAT(tt_fn_, __LINE__)};                                  \
    static void TT_CONCAT(tt_fn_, __LINE__)()

// Lightweight grouping — runs the block inline, just for readability parity
// with doctest. No isolated state per subcase.
#define SUBCASE(name) if (true)

#define CHECK(expr)                                                            \
    do {                                                                       \
        ++::tiny_test::checkCount();                                           \
        if (!(expr)) ::tiny_test::reportFailure(__FILE__, __LINE__, #expr);    \
    } while (false)

// REQUIRE behaves like CHECK plus an early return from the test case on
// failure, matching doctest's stop-the-case semantics.
#define REQUIRE(expr)                                                          \
    do {                                                                       \
        ++::tiny_test::checkCount();                                           \
        if (!(expr)) { ::tiny_test::reportFailure(__FILE__, __LINE__, #expr);  \
                       return; }                                               \
    } while (false)

#ifdef TINY_TEST_MAIN
int main() { return ::tiny_test::runAll(); }
#endif

#endif
