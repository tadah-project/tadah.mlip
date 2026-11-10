#include "catch2/catch.hpp"
#include <tadah/models/descriptors/dm/dm_func.h>

#include <cmath>
#include <cassert>

namespace tadah {
namespace models {

/**
 * @test Checks F_RLR behavior with a simple configuration.
 * Ensures no NaN is returned and verifies plausibility of results.
 */
TEST_CASE("F_RLR: Basic Function and Derivative", "[F_RLR]")
{
  // Reason: "INITMB=true" triggers parameter grid load in F_RLR constructor
  tadah::core::Context c;
  c.remove("INITMB");
  c.add("INITMB", "true");
  c.add("SEMBFUNC", 0.2);
  c.add("CEMBFUNC", 0.3);

  F_RLR rlr(c, 1, 1);

  double rho = 2.0;
  size_t p = 0; // Single parameter index
  double valF = rlr.calc_F(rho, p);
  double valdF = rlr.calc_dF(rho, p);

  // Reason: Checking for no NaN or inf
  REQUIRE_FALSE(std::isnan(valF));
  REQUIRE_FALSE(std::isinf(valF));
  REQUIRE_FALSE(std::isnan(valdF));
  REQUIRE_FALSE(std::isinf(valdF));

  // Basic range checks for sanity (not strict correctness)
  REQUIRE(valF != 0.0);
  REQUIRE(valdF != 0.0);
}

/**
 * @test Checks F_SQRT behavior with a simple configuration.
 * Ensures no NaN is returned and verifies plausibility of results.
 */
TEST_CASE("F_SQRT: Basic Function and Derivative", "[F_SQRT]")
{
  // Reason: "INITMB=true" triggers sgrid load in F_SQRT constructor
  tadah::core::Context c;
  c.remove("INITMB");
  c.add("INITMB", "true");
  c.add("SEMBFUNC", 0.2);

  F_SQRT sqrt_func(c, 0,1);

  double rho = 4.0;
  size_t p = 0; // Single parameter index
  double valF = sqrt_func.calc_F(rho, p);
  double valdF = sqrt_func.calc_dF(rho, p);

  // Reason: Checking for no NaN or inf
  REQUIRE_FALSE(std::isnan(valF));
  REQUIRE_FALSE(std::isinf(valF));
  REQUIRE_FALSE(std::isnan(valdF));
  REQUIRE_FALSE(std::isinf(valdF));

  // Basic numeric checks
  // s = 0.2, sqrt(4.0) = 2.0, so expected = 0.4 (ideal, ignoring round-off)
  REQUIRE(valF == Approx(0.4).margin(1e-12));

  // Derivative: 0.5*s / sqrt(rho) => 0.5*0.2/2.0 = 0.05
  REQUIRE(valdF == Approx(0.05).margin(1e-12));
}

/**
 * @test F_SQ: F(rho)=rho^2, F'(rho)=2*rho.  Exact for all rho.
 */
TEST_CASE("F_SQ: Function and Derivative", "[F_SQ]")
{
    tadah::core::Context c;
    c.remove("INITMB");
    c.add("INITMB", "true");

    F_SQ sq(c, 1, 1);
    const size_t p = 0;

    // Exact values
    REQUIRE(sq.calc_F (3.0, p) == Approx(9.0));
    REQUIRE(sq.calc_dF(3.0, p) == Approx(6.0));

    // General: F(rho)=rho^2, F'(rho)=2*rho
    for (double rho : {0.5, 1.0, 2.0, 5.0}) {
        REQUIRE_FALSE(std::isnan(sq.calc_F (rho, p)));
        REQUIRE(sq.calc_F (rho, p) == Approx(rho * rho));
        REQUIRE(sq.calc_dF(rho, p) == Approx(2.0 * rho));
    }
}

/**
 * @test F_Blip: no NaN/inf; derivative consistent with central differences.
 */
TEST_CASE("F_Blip: Function and Derivative", "[F_Blip]")
{
    tadah::core::Context c;
    c.remove("INITMB");
    c.add("INITMB", "true");
    // sgrid=0.5, cgrid=1.0 → blip support in rho: |0.5*(rho-1.0)| < 2  →  rho ∈ (-3, 5)
    c.add("CEMBFUNC", "1.0");
    c.add("SEMBFUNC", "0.5");

    F_Blip fb(c, 1, 1);
    const size_t p = 0;
    const double rho = 1.5;   // x = 0.5*(1.5-1.0) = 0.25 — well inside support

    const double F_val  = fb.calc_F (rho, p);
    const double dF_val = fb.calc_dF(rho, p);
    REQUIRE_FALSE(std::isnan (F_val));
    REQUIRE_FALSE(std::isinf (F_val));
    REQUIRE_FALSE(std::isnan (dF_val));
    REQUIRE_FALSE(std::isinf (dF_val));
    REQUIRE(std::fabs(F_val) > 0.0);

    // Numerical FD verification of derivative
    const double h    = 1e-7;
    const double dF_fd = (fb.calc_F(rho + h, p) - fb.calc_F(rho - h, p)) / (2.0 * h);
    REQUIRE(dF_val == Approx(dF_fd).margin(1e-5));
}

} // namespace models
} // namespace tadah
