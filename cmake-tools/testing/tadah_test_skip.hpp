// TADAH_SKIP: report a Catch2 (v2) test case as Skipped to CTest.
// (cmake-tools, testing/tadah_test_skip.hpp; see testing/TadahTestSkip.cmake
// and the "Testing" section of the cmake-tools README.)
//
// Catch2 v2 has no skip: a test case that returns early -- because an
// optional data file or environment variable is absent -- passes, and CTest
// counts it as a pass that checked nothing. Such a guard calls
//
//     TADAH_SKIP("database not found at " << path);
//
// in the body of the TEST_CASE, before its first check, instead. The macro
//  * records the reason as a Catch warning ("skipped: <reason>"), so a run
//    of the whole binary shows it;
//  * records the skip request and returns from the enclosing function.
// When the test case ends, a Catch listener registered by this header
// writes the marker line "TADAH-SKIP: <reason>" to the process's standard
// error through C stdio, which the Catch reporters that capture a test
// case's std::cout/std::cerr (junit, xml) do not redirect -- but only if
// the test case has no failed assertion. Every test that
// catch_discover_tests() registers carries a SKIP_REGULAR_EXPRESSION
// matching that line at the start of a line, so CTest reports it as Skipped
// although it exits with 0; a test case that asked to be skipped and failed
// -- before the request, in another SECTION, or after TADAH_SKIP returned
// from a helper -- writes no marker and stays Failed.
//
// Rules:
//  * Include this header instead of (or before) "catch2/catch.hpp": the
//    listener needs Catch's external interfaces
//    (CATCH_CONFIG_EXTERNAL_INTERFACES), which this header switches on.
//  * Call TADAH_SKIP before the test case's first check, directly in the
//    TEST_CASE body (not in a helper function or lambda, from which it
//    returns only to the test case) and outside every SECTION. A skip
//    request never hides a failure, but checks that run after it are not
//    reported: the test case is Skipped if none of them failed.
//  * Only a test case registered as a CTest entry of its own is reported
//    as Skipped: a discovered entry, or an explicit add_test() entry that
//    runs exactly one test case and sets TADAH_TEST_SKIP_REGULAR_EXPRESSION.
//  * Nothing else may print a line that starts with "TADAH-SKIP: ": CTest
//    reports an entry whose output has such a line as Skipped, whatever its
//    exit status.
#ifndef TADAH_TEST_SKIP_HPP
#define TADAH_TEST_SKIP_HPP

#ifndef CATCH_CONFIG_EXTERNAL_INTERFACES
#define CATCH_CONFIG_EXTERNAL_INTERFACES
#endif
#include "catch2/catch.hpp"

// CATCH_REGISTER_LISTENER is defined with the listener interface.
#ifndef CATCH_REGISTER_LISTENER
#error "tadah_test_skip.hpp: include it before catch2/catch.hpp (or instead of it): its listener needs CATCH_CONFIG_EXTERNAL_INTERFACES, and catch2/catch.hpp was included without it"
#endif

#include <cstdio>
#include <sstream>
#include <string>

namespace tadah_test {

/** The marker that starts the skip line; TadahTestSkip.cmake matches it. */
inline constexpr const char *kSkipMarker = "TADAH-SKIP: ";

/** Write "\nTADAH-SKIP: <reason>\n" to stderr in one call. The leading
 *  newline puts the marker at the start of a line whatever was written
 *  before it. */
inline void emit_skip_marker(const std::string &reason) {
  const std::string line = std::string("\n") + kSkipMarker + reason + "\n";
  std::fputs(line.c_str(), stderr);
  std::fflush(stderr);
}

/** The skip request of the running test case (Catch runs test cases one
 *  at a time, on one thread). */
struct SkipRequest {
  bool requested = false;
  std::string reason;
};
inline SkipRequest skip_request;

/** Called by TADAH_SKIP: the first request of a test case wins. */
inline void request_skip(const std::string &reason) {
  if (skip_request.requested) return;
  skip_request.requested = true;
  skip_request.reason = reason;
}

/** Writes the marker when a test case that asked to be skipped ends
 *  without a failed assertion. Catch counts a failed CHECK or REQUIRE, an
 *  unexpected exception and a fatal signal as a failed assertion (and a
 *  [!shouldfail] test case that passed as one more). */
struct SkipListener : Catch::TestEventListenerBase {
  using TestEventListenerBase::TestEventListenerBase;

  void testCaseStarting(Catch::TestCaseInfo const &info) override {
    skip_request = SkipRequest{};
    TestEventListenerBase::testCaseStarting(info);
  }

  void testCaseEnded(Catch::TestCaseStats const &stats) override {
    if (skip_request.requested && stats.totals.assertions.failed == 0)
      emit_skip_marker(skip_request.reason);
    skip_request = SkipRequest{};
    TestEventListenerBase::testCaseEnded(stats);
  }
};

/** One registration per test binary, however many translation units
 *  include this header (an inline variable has a single instance;
 *  CATCH_REGISTER_LISTENER would register one listener per translation
 *  unit). */
inline const Catch::ListenerRegistrar<SkipListener> skip_listener_registrar;

} // namespace tadah_test

/** Ask for the current test case to be reported as skipped, and return from
 *  the enclosing function: see the file comment. `msg` is streamed like the
 *  message of Catch's WARN (`"n = " << n`). */
#define TADAH_SKIP(msg)                                                      \
  do {                                                                       \
    std::ostringstream tadah_skip_reason_;                                   \
    tadah_skip_reason_ << msg;                                               \
    WARN("skipped: " << tadah_skip_reason_.str());                           \
    ::tadah_test::request_skip(tadah_skip_reason_.str());                    \
    return;                                                                  \
  } while (false)

#endif // TADAH_TEST_SKIP_HPP
