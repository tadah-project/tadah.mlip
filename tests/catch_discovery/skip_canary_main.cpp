// Catch's main for the skip canary (skip_canary.cpp, skip_canary_more.cpp).
// It does not include tadah_test_skip.hpp: the TADAH-SKIP listener must
// reach the test run from the translation units that use TADAH_SKIP.
#define CATCH_CONFIG_MAIN
#include "catch2/catch.hpp"
