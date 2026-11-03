// test_failscore_grading_bound.cpp
//
// The bound that the refusal of a FAILSCORE too small for the grading bands
// of FAILSCORE_GRADING ON gives ("a FAILSCORE of 1e-290 or more has them"),
// checked in the flags of this test (test_tadah: the build's fast-math
// flags, which flush subnormal numbers to zero, the only arithmetic in
// which a normal FAILSCORE can lack its bands). HPO's
// test_failgrade_parse.cpp checks it in HPO's own flags.

#include "catch2/catch.hpp"

#ifdef TADAH_ENABLE_NFP

#include <tadah/hpo/pc_fail_grading.h>

#include <string>

/* Pins: graded_failscore_is_ordered (the four grading bands of
 * FAILSCORE_GRADING ON, each 0.00025 x FAILSCORE wide, strictly ordered
 * below FAILSCORE) is true for every power of ten from
 * kGradedFailscoreSufficient, 1e-290, to 1e308, compiled as test_tadah is.
 * Before the fix: does not compile (no kGradedFailscoreSufficient: the
 * refusal gave no bound). */
TEST_CASE("FAILSCORE_GRADING: every FAILSCORE from the stated bound has its "
          "grading bands, in this build's arithmetic",
          "[failgrade][failscore_bound]") {
  CHECK(tadah::hpo::kGradedFailscoreSufficient == 1e-290);
  for (int e = -290; e <= 308; ++e) {
    const double fs = std::stod("1e" + std::to_string(e));
    INFO("FAILSCORE 1e" << e);
    CHECK(tadah::hpo::graded_failscore_is_ordered(fs));
  }
}

#endif  // TADAH_ENABLE_NFP
