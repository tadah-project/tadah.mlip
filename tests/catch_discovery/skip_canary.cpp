// Canary for the TADAH-SKIP marker of cmake-tools (testing/tadah_test_skip.hpp,
// testing/TadahTestSkip.cmake); see check_skip.cmake.
//
// TADAH_SKIP(<reason>) asks for the test case to be reported as skipped and
// returns from it. When the test case ends without a failed assertion, the
// line "TADAH-SKIP: <reason>" is written, and every discovered CTest entry
// carries a SKIP_REGULAR_EXPRESSION that matches that line at the start of a
// line, so CTest reports the case as Skipped although it exits with 0. The
// other cases pin what must NOT be skipped: an ordinary pass, failures whose
// output contains the marker text anywhere but at the start of a line, and
// failures in a test case that also asked to be skipped.
//
// Like a real suite, the test cases live outside the CATCH_CONFIG_MAIN
// translation unit (skip_canary_main.cpp), and a second translation unit
// (skip_canary_more.cpp) includes tadah_test_skip.hpp too.
//
// Five of these cases fail on purpose: the binary is not registered with
// catch_discover_tests(); check_skip.cmake discovers and runs it in a
// scratch CTest directory of its own.
#include "tadah_test_skip.hpp"

#include <cstdio>
#include <string>

TEST_CASE("skip canary: optional data absent", "[skip_canary]") {
  TADAH_SKIP("optional data absent (" << 42 << " files missing)");
  FAIL("TADAH_SKIP returned to the test case");
}

TEST_CASE("skip canary: passes", "[skip_canary]") {
  SUCCEED("an ordinary pass stays Passed");
}

TEST_CASE("skip canary: fails quoting the marker", "[skip_canary]") {
  // The reporter echoes the operands indented and quoted: not a marker line.
  CHECK(std::string(tadah_test::kSkipMarker) + "quoted" == "");
}

TEST_CASE("skip canary: fails after printing the marker mid-line",
          "[skip_canary]") {
  std::printf("note: %sprinted inside a line\n", tadah_test::kSkipMarker);
  std::fflush(stdout);
  FAIL("the marker text inside a line does not make a skip");
}

// A skip request does not hide a failure of the same test case, wherever
// the failure happens.
TEST_CASE("skip canary: fails a check and then skips", "[skip_canary]") {
  CHECK(1 == 2);
  TADAH_SKIP("optional data absent after a failed check");
}

TEST_CASE("skip canary: one section fails, another skips", "[skip_canary]") {
  SECTION("fails") { CHECK(1 == 2); }
  SECTION("skips") { TADAH_SKIP("optional data absent in a section"); }
}

TEST_CASE("skip canary: skips in a helper and then fails", "[skip_canary]") {
  // TADAH_SKIP returns from the lambda only; the test case goes on.
  auto guard = [] { TADAH_SKIP("optional data absent in a helper"); };
  guard();
  CHECK(1 == 2);
}
