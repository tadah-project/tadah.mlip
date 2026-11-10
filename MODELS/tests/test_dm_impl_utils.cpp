#include "catch2/catch.hpp"
#include <tadah/models/descriptors/dm/dm_impl_utils.h>

#include <cmath>

using namespace tadah::models::detail;

// -----------------------------------------------------------------------
//  StackOrHeapBuffer
// -----------------------------------------------------------------------

TEST_CASE("StackOrHeapBuffer: stack allocation (n <= N)", "[dm_impl_utils]")
{
    StackOrHeapBuffer<8> buf(4);
    REQUIRE(buf.size() == 4);
    double *g  = buf.g();
    double *dg = buf.dg();
    REQUIRE(g  != nullptr);
    REQUIRE(dg != nullptr);
    REQUIRE(g  != dg);   // independent arrays

    for (std::size_t i = 0; i < 4; ++i) g [i] = static_cast<double>(i);
    for (std::size_t i = 0; i < 4; ++i) dg[i] = static_cast<double>(i) * 10.0;

    for (std::size_t i = 0; i < 4; ++i) REQUIRE(g [i] == Approx(i));
    for (std::size_t i = 0; i < 4; ++i) REQUIRE(dg[i] == Approx(i * 10.0));
}

TEST_CASE("StackOrHeapBuffer: heap allocation (n > N)", "[dm_impl_utils]")
{
    StackOrHeapBuffer<4> buf(8);   // 8 > N=4 → heap
    REQUIRE(buf.size() == 8);
    double *g  = buf.g();
    double *dg = buf.dg();
    REQUIRE(g  != nullptr);
    REQUIRE(dg != nullptr);
    REQUIRE(g  != dg);

    for (std::size_t i = 0; i < 8; ++i) g [i] = static_cast<double>(i);
    for (std::size_t i = 0; i < 8; ++i) dg[i] = static_cast<double>(i) * 10.0;

    for (std::size_t i = 0; i < 8; ++i) REQUIRE(g [i] == Approx(i));
    for (std::size_t i = 0; i < 8; ++i) REQUIRE(dg[i] == Approx(i * 10.0));
}

TEST_CASE("StackOrHeapBuffer: edge cases", "[dm_impl_utils]")
{
    REQUIRE_NOTHROW(StackOrHeapBuffer<4>(0));
    REQUIRE_NOTHROW(StackOrHeapBuffer<1>(1));

    StackOrHeapBuffer<4> buf1(1);
    REQUIRE(buf1.size() == 1);
    buf1.g ()[0] = 3.14;
    buf1.dg()[0] = 2.71;
    REQUIRE(buf1.g ()[0] == Approx(3.14));
    REQUIRE(buf1.dg()[0] == Approx(2.71));

    // Exactly at boundary: n==N stays on stack
    StackOrHeapBuffer<4> buf2(4);
    REQUIRE(buf2.size() == 4);
    REQUIRE(buf2.g () != nullptr);
    REQUIRE(buf2.dg() != nullptr);
    REQUIRE(buf2.g () != buf2.dg());
}

// -----------------------------------------------------------------------
//  FACT_TBL / fact_cached   (table covers 0! to 6!)
// -----------------------------------------------------------------------

TEST_CASE("FACT_TBL and fact_cached", "[dm_impl_utils]")
{
    REQUIRE(fact_cached(0) == Approx(1.0));
    REQUIRE(fact_cached(1) == Approx(1.0));
    REQUIRE(fact_cached(2) == Approx(2.0));
    REQUIRE(fact_cached(3) == Approx(6.0));
    REQUIRE(fact_cached(4) == Approx(24.0));
    REQUIRE(fact_cached(5) == Approx(120.0));
    REQUIRE(fact_cached(6) == Approx(720.0));

    // Cross-check against tgamma
    for (int k = 0; k <= 6; ++k)
        REQUIRE(fact_cached(k) == Approx(std::tgamma(static_cast<double>(k + 1))));
}

// -----------------------------------------------------------------------
//  pow_i06
// -----------------------------------------------------------------------

TEST_CASE("pow_i06: branchless integer powers 0..6", "[dm_impl_utils]")
{
    const double x = 2.5;
    for (int n = 0; n <= 6; ++n)
        REQUIRE(pow_i06(x, n) == Approx(std::pow(x, n)));

    REQUIRE(pow_i06(x,   0) == Approx(1.0));
    REQUIRE(pow_i06(2.0, 3) == Approx(8.0));
    REQUIRE(pow_i06(1.0, 6) == Approx(1.0));
    REQUIRE(pow_i06(0.0, 0) == Approx(1.0));
    REQUIRE(pow_i06(3.0, 4) == Approx(81.0));

    // Negative base
    for (int n = 0; n <= 6; ++n)
        REQUIRE(pow_i06(-1.5, n) == Approx(std::pow(-1.5, n)));
}

// -----------------------------------------------------------------------
//  precompute_powers (values only)
// -----------------------------------------------------------------------

TEST_CASE("precompute_powers: values only", "[dm_impl_utils]")
{
    double p[7] = {};
    const double x = 2.5;
    precompute_powers(x, 4, p);
    for (int n = 0; n <= 4; ++n)
        REQUIRE(p[n] == Approx(std::pow(x, n)));

    // x == 1: all powers are 1
    precompute_powers(1.0, 6, p);
    for (int n = 0; n <= 6; ++n)
        REQUIRE(p[n] == Approx(1.0));

    // Lmax == 0: only p[0] = 1
    precompute_powers(3.7, 0, p);
    REQUIRE(p[0] == Approx(1.0));

    // x == 0: p[0]=1, rest 0
    precompute_powers(0.0, 4, p);
    REQUIRE(p[0] == Approx(1.0));
    for (int n = 1; n <= 4; ++n)
        REQUIRE(p[n] == Approx(0.0));
}

// -----------------------------------------------------------------------
//  precompute_powers (values and derivatives)
// -----------------------------------------------------------------------

TEST_CASE("precompute_powers: values and derivatives", "[dm_impl_utils]")
{
    double p[7] = {}, dp[7] = {};
    const double x = 2.5;
    precompute_powers(x, 4, p, dp);

    REQUIRE(dp[0] == Approx(0.0));
    for (int n = 0; n <= 4; ++n)
        REQUIRE(p[n] == Approx(std::pow(x, n)));
    for (int n = 1; n <= 4; ++n)
        REQUIRE(dp[n] == Approx(static_cast<double>(n) * std::pow(x, n - 1)));

    // Cross-check derivatives via central differences
    const double h = 1e-7;
    double pp[7] = {}, pm[7] = {};
    precompute_powers(x + h, 4, pp);
    precompute_powers(x - h, 4, pm);
    for (int n = 1; n <= 4; ++n)
        REQUIRE(dp[n] == Approx((pp[n] - pm[n]) / (2.0 * h)).margin(1e-6));

    // Lmax == 0: dp[0] = 0
    precompute_powers(3.7, 0, p, dp);
    REQUIRE(p [0] == Approx(1.0));
    REQUIRE(dp[0] == Approx(0.0));
}
