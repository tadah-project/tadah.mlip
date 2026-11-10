// Deprecated configuration keys (blueprint field `deprecated`).
//
// MPIWPCKG, MBLOCK and NBLOCK belonged to the legacy MPI training build,
// removed in 1.6.0. Files that contain them must still parse; the parser
// writes one notice per key and process to standard error
// (core::report_ignored_key_once, the rule of every ignored key), whatever
// the Logger level or VERBOSE, keeps the value check, and never stores the
// key, so it cannot reappear in a potential written from the parsed
// Context.
//
// ctest runs every test case in a process of its own, where no deprecated
// key has been reported yet. A case that expects a notice counts it only
// when core::ignored_key_reported() says the key was not reported before
// the case (a run of the whole binary in one process).

#include "catch2/catch.hpp"

#include <tadah/core/blueprint_parser.h>
#include <tadah/core/context.h>
#include <tadah/core/core_user_config_parser.h>
#include <tadah/core/key_notice.h>
#include <tadah/core/logger.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>
#include <unistd.h>

using namespace tadah::core;

namespace {

const std::vector<std::string> kMpiKeys = {"MPIWPCKG", "MBLOCK", "NBLOCK"};

/* Captures std::cerr (where the deprecation notice goes) and std::cout
 * (where the Logger prints) and sets the Logger level; restores the streams
 * and the level afterwards. A VERBOSE line in a parsed file changes the
 * Logger level too, so the restore covers it. */
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

  /* Lines of the captured standard error that contain `needle`. */
  std::vector<std::string> err_lines_with(const std::string &needle) const {
    return lines_with(err_.str(), needle);
  }
  /* Lines of the captured standard output that contain `needle`. */
  std::vector<std::string> out_lines_with(const std::string &needle) const {
    return lines_with(out_.str(), needle);
  }

private:
  static std::vector<std::string> lines_with(const std::string &text,
                                             const std::string &needle) {
    std::vector<std::string> out;
    std::istringstream in(text);
    for (std::string line; std::getline(in, line);)
      if (line.find(needle) != std::string::npos) out.push_back(line);
    return out;
  }

  std::ostringstream out_;
  std::ostringstream err_;
  LogLevel old_level_;
  std::streambuf *old_out_;
  std::streambuf *old_err_;
};

std::string deprecation_line(const std::string &key) {
  return "Deprecated key " + key + " is ignored";
}

/* The notices a case expects for @p key: 1 when the process has not
 * reported it yet (every ctest run), 0 when an earlier case of the same
 * process did. */
std::size_t expected_notices(const std::string &key) {
  return ignored_key_reported(key) ? 0 : 1;
}

/* A scratch directory unique to this process (ctest runs every test case
 * in its own process, possibly concurrently), removed with its files when
 * the object goes out of scope. */
class ScratchDir {
public:
  ScratchDir()
      : dir_(std::filesystem::temp_directory_path() /
             ("tadah_core_deprecated_keys_" + std::to_string(::getpid()))) {
    std::filesystem::create_directories(dir_);
  }
  ~ScratchDir() {
    std::error_code ec;
    std::filesystem::remove_all(dir_, ec);
  }
  ScratchDir(const ScratchDir &) = delete;
  ScratchDir &operator=(const ScratchDir &) = delete;

  const std::filesystem::path &path() const { return dir_; }

  std::filesystem::path write_file(const std::string &name,
                                   const std::string &body) const {
    const auto p = dir_ / name;
    std::ofstream(p) << body;
    return p;
  }

private:
  std::filesystem::path dir_;
};

std::string serialized(const Context &c) {
  std::ostringstream os;
  os << c;
  return os.str();
}

bool has_key_line(const std::string &text, const std::string &key) {
  std::istringstream in(text);
  for (std::string line; std::getline(in, line);) {
    std::istringstream ls(line);
    std::string first;
    if ((ls >> first) && first == key) return true;
  }
  return false;
}

/* The top file sets MBLOCK and includes a file that sets MBLOCK again,
 * NBLOCK and WATOM: one notice per key, and the ordinary keys of both
 * files are kept. */
void check_include_chain(const std::filesystem::path &top) {
  const std::size_t mblock = expected_notices("MBLOCK");
  const std::size_t nblock = expected_notices("NBLOCK");
  StreamCapture cap(LogLevel::WARNING);
  Context c;
  REQUIRE_NOTHROW(c = CoreUserConfigParser().parse(top.string()));
  CHECK(cap.err_lines_with(deprecation_line("MBLOCK")).size() == mblock);
  CHECK(cap.err_lines_with(deprecation_line("NBLOCK")).size() == nblock);
  CHECK_FALSE(c.contains("MBLOCK"));
  CHECK_FALSE(c.contains("NBLOCK"));
  CHECK(c.get<std::string>("ATOM") == "Ta");
  CHECK(c.get<double>("WATOM") == Approx(73.0));
}

} // namespace

TEST_CASE("Config parser: the legacy MPI keys parse, warn once each on "
          "stderr and are not stored",
          "[config][deprecated]") {
  /* The Logger is at WARNING, so a notice sent through it would reach
   * standard output: it must appear on standard error only. */
  std::vector<std::size_t> expected;
  for (const auto &key : kMpiKeys) expected.push_back(expected_notices(key));
  StreamCapture cap(LogLevel::WARNING);
  Context c;
  REQUIRE_NOTHROW(c = CoreUserConfigParser().parse(std::vector<std::string>{
                      "ATOM Ta", "MPIWPCKG 50", "MBLOCK 64", "NBLOCK 64",
                      "MBLOCK 32   # a second line for the same key"}));

  CHECK(c.get<std::string>("ATOM") == "Ta");
  for (std::size_t k = 0; k < kMpiKeys.size(); ++k) {
    const std::string &key = kMpiKeys[k];
    INFO(key);
    const auto warned = cap.err_lines_with(deprecation_line(key));
    REQUIRE(warned.size() == expected[k]);
    CHECK(ignored_key_reported(key));
    if (!warned.empty())
      CHECK_THAT(warned[0], Catch::Matchers::StartsWith("WARNING: ") &&
                                Catch::Matchers::Contains("1.6.0") &&
                                Catch::Matchers::Contains("TADAH_BUILD_MPI"));
    CHECK(cap.out_lines_with(deprecation_line(key)).empty());
    CHECK_FALSE(c.contains(key));
  }
}

TEST_CASE("Config parser: the deprecation notice ignores the Logger level and "
          "VERBOSE",
          "[config][deprecated]") {
  /* pair_style tadah leaves the Logger at ERROR, and a VERBOSE 0 line
   * lowers it while the file is parsed: neither may hide the notice. */
  /* Catch runs the case once per section, in one process: the sections
   * read different keys, so under ctest each is reported there for the
   * first time. */
  SECTION("Logger at ERROR, as in LAMMPS") {
    const std::size_t n = expected_notices("MBLOCK");
    StreamCapture cap(LogLevel::ERROR);
    REQUIRE_NOTHROW(CoreUserConfigParser().parse(
        std::vector<std::string>{"ATOM Ta", "MBLOCK 64"}));
    CHECK(cap.err_lines_with(deprecation_line("MBLOCK")).size() == n);
  }
  SECTION("VERBOSE 0 on an earlier line") {
    const std::size_t mpiwpckg = expected_notices("MPIWPCKG");
    const std::size_t nblock = expected_notices("NBLOCK");
    StreamCapture cap(LogLevel::WARNING);
    REQUIRE_NOTHROW(CoreUserConfigParser().parse(std::vector<std::string>{
        "VERBOSE 0", "ATOM Ta", "MPIWPCKG 50", "NBLOCK 64"}));
    CHECK(cap.err_lines_with(deprecation_line("MPIWPCKG")).size() == mpiwpckg);
    CHECK(cap.err_lines_with(deprecation_line("NBLOCK")).size() == nblock);
  }
}

TEST_CASE("Config parser: a deprecated key never reaches a written potential",
          "[config][deprecated]") {
  /* A potential file is the Context streamed with operator<< (see
   * TrainEngine::dumpPotFile). contains() inserts blueprint defaults, so a
   * deprecated key must have none, or it would be written back. */
  SECTION("parsed from a file") {
    StreamCapture cap(LogLevel::WARNING);
    Context c = CoreUserConfigParser().parse(std::vector<std::string>{
        "ATOM Ta", "WATOM 73", "MPIWPCKG 50", "MBLOCK 64", "NBLOCK 64"});
    for (const auto &key : kMpiKeys) CHECK_FALSE(c.contains(key));
    const std::string pot = serialized(c);
    CHECK(has_key_line(pot, "ATOM"));
    for (const auto &key : kMpiKeys) {
      INFO(key << " in:\n" << pot);
      CHECK_FALSE(has_key_line(pot, key));
    }
  }
  SECTION("queried on a Context that never had it") {
    Context c;
    c.add("ATOM", "Ta");
    for (const auto &key : kMpiKeys) CHECK_FALSE(c.contains(key));
    const std::string pot = serialized(c);
    for (const auto &key : kMpiKeys) {
      INFO(key << " in:\n" << pot);
      CHECK_FALSE(has_key_line(pot, key));
    }
  }
  SECTION("the blueprint gives them no default") {
    const auto &keys = BlueprintParser::getKeysMapRef();
    for (const auto &key : kMpiKeys) {
      INFO(key);
      CHECK(keys.at(key).default_value.empty());
    }
  }
}

TEST_CASE("Config parser: a CONFIG include chain reports a deprecated key once",
          "[config][deprecated]") {
  std::filesystem::path scratch;
  {
    const ScratchDir dir;
    scratch = dir.path();
    const auto sub =
        dir.write_file("sub.cfg", "MBLOCK 32\nNBLOCK 8\nWATOM 73\n");
    const auto top = dir.write_file(
        "top.cfg", "ATOM Ta\nMBLOCK 64\nCONFIG " + sub.string() + "\n");
    check_include_chain(top);
  }
  // The files of the test do not outlive it.
  CHECK_FALSE(std::filesystem::exists(scratch));
}

TEST_CASE("Config parser: a deprecated key is reported once per process, "
          "however many parses read it, a failed one included",
          "[config][deprecated]") {
  /* One rule for every ignored key, deprecated or without effect: one
   * stderr notice per key and process. */
  const std::vector<std::string> ok = {"ATOM Ta", "MBLOCK 64"};
  const std::size_t first = expected_notices("MBLOCK");
  StreamCapture cap(LogLevel::WARNING);

  REQUIRE_NOTHROW(CoreUserConfigParser().parse(ok));
  CHECK(cap.err_lines_with(deprecation_line("MBLOCK")).size() == first);
  CHECK(ignored_key_reported("MBLOCK"));

  // A failing parse (unknown key after a deprecated one) reads the key
  // again: no second notice, and the parser is left at the top level.
  REQUIRE_THROWS_WITH(CoreUserConfigParser().parse(std::vector<std::string>{
                          "MBLOCK 64", "NOT_A_TADAH_KEY 1"}),
                      Catch::Matchers::Contains("NOT_A_TADAH_KEY"));
  CHECK(cap.err_lines_with(deprecation_line("MBLOCK")).size() == first);

  Context c;
  REQUIRE_NOTHROW(c = CoreUserConfigParser().parse(ok));
  CHECK(cap.err_lines_with(deprecation_line("MBLOCK")).size() == first);
  CHECK(c.get<std::string>("ATOM") == "Ta");
  CHECK_FALSE(c.contains("MBLOCK"));
}

/* Pins: the value of a deprecated key is checked as any value, and refused
 * naming the key and its line ("Invalid value for key MBLOCK on line 1:
 * 'sixty-four'", the key's schema, format and examples after it).
 * Before the fix: refused as "Invalid schema for key: MBLOCK", without the
 * line. */
TEST_CASE("Config parser: a deprecated key's value is still checked",
          "[config][deprecated]") {
  StreamCapture cap(LogLevel::WARNING);
  REQUIRE_THROWS_WITH(
      CoreUserConfigParser().parse(
          std::vector<std::string>{"MBLOCK sixty-four"}),
      Catch::Matchers::StartsWith(
          "Invalid value for key MBLOCK on line 1: 'sixty-four'\n"));
  REQUIRE_THROWS_WITH(
      CoreUserConfigParser().parse(std::vector<std::string>{"NBLOCK -1"}),
      Catch::Matchers::StartsWith(
          "Invalid value for key NBLOCK on line 1: '-1'\n"));
}

TEST_CASE("BlueprintParser::findClosestKey never suggests a deprecated key",
          "[findClosestKey][deprecated]") {
  const auto &keys = BlueprintParser::getKeysMapRef();
  for (const std::string typo : {"MBLOK", "NBLOK", "MPIWPCK"}) {
    INFO(typo);
    const std::string suggested = BlueprintParser::findClosestKey(typo, keys);
    CHECK_FALSE(suggested.empty());
    for (const auto &key : kMpiKeys) CHECK(suggested != key);
  }
}

/* Pins: each line of a deprecated key that a parse reads is recorded on the
 * Context it returns (Context::deprecated_keys(), in line order, the lines
 * of a file included with CONFIG where its CONFIG line is): the key, the
 * file, the line number, the values and the line; retired_keys() does not
 * list them. A copy and merge() carry them (each once), and neither
 * operator<< nor a written potential has them. record_deprecated_key()
 * refuses a key that is not deprecated (std::invalid_argument). A fit
 * refuses a Context that records one (MODELS require_no_inert_keys), naming
 * its line and file.
 * Before the fix: does not compile (Context had no deprecated_keys()): a
 * deprecated line was not recorded, so nothing could name it. */
TEST_CASE("Config parser: the lines of a deprecated key are recorded on the "
          "Context, with their file and line",
          "[config][deprecated]") {
  const ScratchDir dir;
  const auto sub = dir.write_file("sub.cfg", "NBLOCK 8\nWATOM 73\n");
  const auto top = dir.write_file(
      "top.cfg", "ATOM Ta\nMBLOCK 64\nCONFIG " + sub.string() + "\n");
  StreamCapture cap(LogLevel::WARNING);
  const Context c = CoreUserConfigParser().parse(top.string());
  const std::vector<RetiredKeyLine> lines = c.deprecated_keys();
  REQUIRE(lines.size() == 2u);
  CHECK(lines[0].key == "MBLOCK");
  CHECK(lines[0].source == top.string());
  CHECK(lines[0].line == 2u);
  CHECK(lines[0].values == std::vector<std::string>{"64"});
  CHECK(lines[0].text == "MBLOCK 64");
  CHECK(lines[1].key == "NBLOCK");
  CHECK(lines[1].source == sub.string());
  CHECK(lines[1].line == 1u);
  CHECK(lines[1].values == std::vector<std::string>{"8"});
  CHECK(c.retired_keys().empty());

  const Context copy = c;
  CHECK((copy.deprecated_keys() == lines));
  Context other;
  other.add("ATOM", "Ta");
  const Context merged = Context::merge(other, c);
  CHECK((merged.deprecated_keys() == lines));
  const Context twice = Context::merge(c, c);
  CHECK((twice.deprecated_keys() == lines));

  const std::string pot = serialized(c);
  CHECK_FALSE(has_key_line(pot, "MBLOCK"));
  CHECK_FALSE(has_key_line(pot, "NBLOCK"));

  Context bare;
  CHECK_THROWS_AS(bare.record_deprecated_key(RetiredKeyLine{
                      "ATOM", "x", 1, {"Ta"}, "ATOM Ta"}),
                  std::invalid_argument);
  CHECK_THROWS_AS(bare.record_deprecated_key(RetiredKeyLine{
                      "EWEIGHT", "x", 1, {"1"}, "EWEIGHT 1"}),
                  std::invalid_argument);
}
