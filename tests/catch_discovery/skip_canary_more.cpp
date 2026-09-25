// A second translation unit of the skip canary that includes
// tadah_test_skip.hpp (see skip_canary.cpp and check_skip.cmake): the skip
// works from any translation unit, and the marker line is written once.
#include "tadah_test_skip.hpp"

TEST_CASE("skip canary: optional data absent in another translation unit",
          "[skip_canary]") {
  TADAH_SKIP("optional data absent in another translation unit");
  FAIL("TADAH_SKIP returned to the test case");
}
