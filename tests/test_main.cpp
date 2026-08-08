// Single-TU main entry point. All TEST_CASE registrations from sibling
// translation units register into a shared static registry at startup.
#define TINY_TEST_MAIN
#include "tiny_test.h"  // NOLINT(misc-include-cleaner) — provides main() under TINY_TEST_MAIN
