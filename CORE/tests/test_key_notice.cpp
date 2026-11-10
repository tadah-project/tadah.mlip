// The notice for a key that is accepted but has no effect (key_notice.h):
// one line on standard error, whatever the Logger level, and at most once
// per process for report_ignored_key_once(); while an
// IgnoredKeyNoticeCapture lives it receives the notice instead, once per
// capture.

#include "catch2/catch.hpp"

#include <tadah/core/key_notice.h>
#include <tadah/core/logger.h>

#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using namespace tadah::core;

namespace {

/* Captures std::cerr and std::cout and sets the Logger level; restores the
 * streams and the level afterwards. */
class StreamCapture {
public:
  explicit StreamCapture(LogLevel level)
      : old_level_(Logger::instance().getLogLevel()),
        old_out_(std::cout.rdbuf(out_.rdbuf())),
        old_err_(std::cerr.rdbuf(err_.rdbuf())) {
    Logger::instance().setLogLevel(level);
  }
  ~StreamCapture() {
    std::cout.rdbuf(old_out_);
    std::cerr.rdbuf(old_err_);
    Logger::instance().setLogLevel(old_level_);
  }
  StreamCapture(const StreamCapture &) = delete;
  StreamCapture &operator=(const StreamCapture &) = delete;

  std::string err() const { return err_.str(); }
  std::string out() const { return out_.str(); }

private:
  std::ostringstream out_;
  std::ostringstream err_;
  LogLevel old_level_;
  std::streambuf *old_out_;
  std::streambuf *old_err_;
};

std::size_t count_of(const std::string &text, const std::string &needle) {
  std::size_t n = 0;
  for (std::size_t p = text.find(needle); p != std::string::npos;
       p = text.find(needle, p + needle.size()))
    ++n;
  return n;
}

} // namespace

TEST_CASE("report_ignored_key: one WARNING line on stderr, whatever the "
          "Logger level",
          "[key_notice]") {
  for (const LogLevel level : {LogLevel::ERROR, LogLevel::WARNING}) {
    StreamCapture cap(level);
    report_ignored_key("Key TEST_NOTICE_A", "the reason.");
    CHECK(cap.err() == "WARNING: Key TEST_NOTICE_A is ignored: the reason.\n");
    CHECK(cap.out().empty());
  }
}

TEST_CASE("report_ignored_key_once: once per key and process, also across "
          "threads",
          "[key_notice]") {
  const std::string key = "TEST_NOTICE_ONCE_KEY";
  const std::string line = "WARNING: Key " + key + " is ignored: why.\n";
  REQUIRE_FALSE(ignored_key_reported(key));
  StreamCapture cap(LogLevel::ERROR);
  CHECK(report_ignored_key_once(key, "Key " + key, "why."));
  CHECK(ignored_key_reported(key));
  CHECK_FALSE(report_ignored_key_once(key, "Key " + key, "why."));
  // another subject or reason for the same key: still nothing
  CHECK_FALSE(report_ignored_key_once(key, "Key " + key + " 2", "other."));
  std::vector<std::thread> threads;
  for (int t = 0; t < 8; ++t)
    threads.emplace_back(
        [&key] { report_ignored_key_once(key, "Key " + key, "why."); });
  for (auto &t : threads) t.join();
  CHECK(cap.err() == line);

  // another key has its own notice
  const std::string other = "TEST_NOTICE_ONCE_OTHER";
  REQUIRE_FALSE(ignored_key_reported(other));
  CHECK(report_ignored_key_once(other, "Key " + other, "why."));
  CHECK(count_of(cap.err(), "is ignored") == 2);
}

/* Pins: while an IgnoredKeyNoticeCapture lives, report_ignored_key() and
 * report_ignored_key_once() give it the notice ("<subject> is ignored:
 * <reason>", without "WARNING: ") and write nothing to standard error; a
 * key reported once is not reported again under a capture; captures nest
 * (the inner one receives, the outer one again after it); after the
 * capture the notice goes to standard error again. pair_style tadah loads
 * a potential under one and writes its notices as LAMMPS warnings.
 * A new class: there is no before (the notices went to standard error
 * only, which LAMMPS' log never saw). */
TEST_CASE("IgnoredKeyNoticeCapture collects the notices instead of "
          "standard error",
          "[key_notice]") {
  StreamCapture cap(LogLevel::WARNING);
  {
    IgnoredKeyNoticeCapture outer;
    report_ignored_key("Key A", "reason a");
    {
      IgnoredKeyNoticeCapture inner;
      CHECK(report_ignored_key_once("capture_test_key_B", "Key B", "reason b"));
      CHECK_FALSE(report_ignored_key_once("capture_test_key_B", "Key B", "reason b"));
      REQUIRE(inner.notices().size() == 1);
      CHECK(inner.notices()[0] == "Key B is ignored: reason b");
    }
    report_ignored_key("Key C", "reason c");
    REQUIRE(outer.notices().size() == 2);
    CHECK(outer.notices()[0] == "Key A is ignored: reason a");
    CHECK(outer.notices()[1] == "Key C is ignored: reason c");
  }
  CHECK(cap.err().empty());
  report_ignored_key("Key D", "reason d");
  CHECK(cap.err() == "WARNING: Key D is ignored: reason d\n");
}

/* Pins: a capture receives the notice of every key reported while it
 * lives, once per capture, also a key the process reported before it (to
 * standard error, or to an earlier capture): report_ignored_key_once()
 * for a key reported outside any capture gives the next capture the
 * notice once (a second call under it gives nothing more), and a second
 * capture gets it once again; nothing more goes to standard error, and
 * ignored_key_reported() stays true. pair_style tadah loads a potential
 * under a capture of its own, so every LAMMPS instance writes the notice
 * to its log.lammps (two runs in one process: the Python module, tadah
 * hpo's PC_LAMMPS evaluations).
 * Before the fix: report_ignored_key_once() gave a key once per process,
 * so both captures received nothing (0 notices), and only the first LAMMPS
 * instance of a process had the warning in its log. */
TEST_CASE("IgnoredKeyNoticeCapture receives a key the process reported "
          "before it, once per capture",
          "[key_notice]") {
  const std::string key = "capture_test_key_reported_before";
  StreamCapture cap(LogLevel::WARNING);
  REQUIRE_FALSE(ignored_key_reported(key));
  CHECK(report_ignored_key_once(key, "Key E", "reason e"));
  CHECK(cap.err() == "WARNING: Key E is ignored: reason e\n");
  for (int run = 0; run < 2; ++run) {
    INFO("capture " << run + 1);
    IgnoredKeyNoticeCapture notices;
    report_ignored_key_once(key, "Key E", "reason e");
    report_ignored_key_once(key, "Key E", "reason e");
    REQUIRE(notices.notices().size() == 1);
    CHECK(notices.notices()[0] == "Key E is ignored: reason e");
  }
  CHECK(ignored_key_reported(key));
  CHECK(cap.err() == "WARNING: Key E is ignored: reason e\n");
  // outside a capture: still once per process
  CHECK_FALSE(report_ignored_key_once(key, "Key E", "reason e"));
  CHECK(cap.err() == "WARNING: Key E is ignored: reason e\n");
}
