#include "catch2/catch.hpp"
#include <tadah/core/blueprint_parser.h>
#include <tadah/core/context.h>
#include <tadah/core/core_user_config_parser.h>
#include <tadah/core/key_notice.h>
#include <tadah/core/logger.h>
#include <dirent.h>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <istream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>
#include <unistd.h>

using namespace tadah::core;

namespace tadah {
namespace core {
/* A KeyLine as a failed check prints it: "{line 2 of a.cfg, potential,
 * file a.cfg, 1 value(s), line number 2, ordinal 2}". */
inline std::ostream &operator<<(std::ostream &os, const KeyLine &l) {
  return os << "{" << l.where << (l.potential ? ", potential" : "")
            << ", file " << l.source << ", " << l.values
            << " value(s), line number " << l.line << ", ordinal "
            << l.ordinal << "}";
}
}  // namespace core
}  // namespace tadah

TEST_CASE( "Testing Context read()", "[config_read]" ) {
  std::string path = "tests_data/valid_configs";
  DIR* dir = opendir(path.c_str());
  if (dir) {
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
      std::string filename(entry->d_name);
      if (filename != "." && filename != "..") {
        REQUIRE_NOTHROW(CoreUserConfigParser().parse(path+"/"+filename));
      }
    }
    closedir(dir);
  } else {
    std::cerr << "Failed to open directory." << std::endl;
  }

  path = "tests_data/invalid_configs";
  dir = opendir(path.c_str());
  if (dir) {
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
      std::string filename(entry->d_name);
      if (filename != "." && filename != "..") {
        std::cerr << "Testing: " << path+"/"+filename << std::endl;
        REQUIRE_THROWS(CoreUserConfigParser().parse(path+"/"+filename));
      }
    }
    closedir(dir);
  } else {
    std::cerr << "Failed to open directory." << std::endl;
  }

  Context c = CoreUserConfigParser().parse("tests_data/valid_configs/valid_config1");
  REQUIRE(c.size("DBFILE")==4);

  REQUIRE_THROWS(CoreUserConfigParser().parse("some_random_non_existing_config"));
}
TEST_CASE( "Testing Context add", "[config]" ) {
  std::string path = "tests_data/valid_configs";
  DIR* dir = opendir(path.c_str());
  if (dir) {
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
      std::string filename(entry->d_name);
      if (filename != "." && filename != "..") {
        REQUIRE_NOTHROW(CoreUserConfigParser().parse(path+"/"+filename));
        Context c1 = CoreUserConfigParser().parse(path+"/"+filename);
        Context c2 = CoreUserConfigParser().parse(path+"/"+filename);
        REQUIRE(c1==c2);
      }
    }
    closedir(dir);
  } else {
    std::cerr << "Failed to open directory." << std::endl;
  }
}
TEST_CASE("read() with empty lines and comments") {
    // A temporary file containing some blank lines and commented lines
    const char* filename = "test_empty_comment.cfg";
    {
        std::ofstream ofs(filename);
        ofs << "\n";               // empty line
        ofs << "# This is a comment line\n";
        ofs << "INIT2B true   # key plus inline comment\n";
        ofs << "\n";               // another blank line
    }

    // Now read it
    Context c = CoreUserConfigParser().parse(filename);
    // We expect "INIT2B true" was read
    REQUIRE(c.get<bool>("INIT2B") == true);

    // Cleanup
    std::remove(filename);
}

// ---------------------------------------------------------------------------
// CoreUserConfigParser::refuse_keys({KEY -> reason})
//
// A parser told to refuse a key (upper-case in the map) throws a
// std::runtime_error at the line that sets it, whatever its letter case.
// The message names the key in upper case, says it is "not accepted",
// names the line ("line N", counting every line of the file, comments and
// blank lines included) and the file, and gives the caller's reason. A
// CONFIG include inherits the refusal; the error then names the included
// file and its line. HPO refuses FORCE and STRESS in the --validation file
// this way: what validation predicts follows PC_FRMSE / PC_SRMSE, and what
// training fits follows the configuration file and -F/-S.
//
// These cases were committed before CoreUserConfigParser::refuse_keys
// existed. They reached it through a detection trait then, so the file
// compiled and every refuse_keys case failed at run time; now that the
// method exists they call it directly. Before it existed, a FORCE or STRESS
// line in HPO's --validation file was silently ignored.
// ---------------------------------------------------------------------------
namespace {

/* The reasons a caller gives. Neither contains a refused key in upper case,
 * "not accepted" or "line", so they cannot satisfy those checks for it. */
const std::string kForceReason =
    "what validation predicts follows PC_FRMSE, not this file";
const std::string kStressReason =
    "what validation predicts follows PC_SRMSE, not this file";

/* A scratch directory unique to this process (ctest runs every test case in
 * a process of its own, possibly concurrently), removed with its files when
 * the object goes out of scope. Its path has no upper-case FORCE or STRESS
 * in it, so a file name in a message cannot satisfy a check for the key. */
class RefuseScratch {
public:
  RefuseScratch()
      : dir_(std::filesystem::temp_directory_path() /
             ("tadah_core_refuse_keys_" + std::to_string(::getpid()))) {
    std::error_code ec;
    std::filesystem::remove_all(dir_, ec);
    std::filesystem::create_directories(dir_);
  }
  ~RefuseScratch() {
    std::error_code ec;
    std::filesystem::remove_all(dir_, ec);
  }
  RefuseScratch(const RefuseScratch &) = delete;
  RefuseScratch &operator=(const RefuseScratch &) = delete;

  /* Writes @p body to <dir>/<name>; returns the path as a string. */
  std::string write(const std::string &name, const std::string &body) const {
    const auto p = dir_ / name;
    std::ofstream os(p);
    os << body;
    os.close();
    REQUIRE_FALSE(os.fail());
    return p.string();
  }

  /* The path of <dir>/<name>, written or not. */
  std::string path(const std::string &name) const {
    return (dir_ / name).string();
  }

private:
  std::filesystem::path dir_;
};

/* The message of the std::runtime_error that @p parse_call throws; fails the
 * case when it throws none. Another exception type propagates (and fails the
 * case): the contract is std::runtime_error. */
template <class F>
std::string runtime_error_of(F &&parse_call, const std::string &what) {
  try {
    (void)parse_call();
  } catch (const std::runtime_error &e) {
    return e.what();
  }
  FAIL(what << " did not throw std::runtime_error");
  return {};
}

/* Whether @p msg names line @p n: "line <n>" not followed by another digit
 * (so line 1 is not found in "line 12"). */
bool names_line(const std::string &msg, std::size_t n) {
  const std::string needle = "line " + std::to_string(n);
  for (auto pos = msg.find(needle); pos != std::string::npos;
       pos = msg.find(needle, pos + 1)) {
    const auto end = pos + needle.size();
    if (end == msg.size() ||
        !std::isdigit(static_cast<unsigned char>(msg[end])))
      return true;
  }
  return false;
}

/* The checks every refusal shares: the key in upper case, "not accepted",
 * the line, the source (when not empty) and the reason. */
void check_refusal(const std::string &msg, const std::string &key,
                   std::size_t line, const std::string &source,
                   const std::string &reason) {
  INFO("message: " << msg);
  CHECK_THAT(msg, Catch::Matchers::Contains(key));
  CHECK_THAT(msg, Catch::Matchers::Contains("not accepted"));
  CHECK(names_line(msg, line));
  if (!source.empty()) CHECK_THAT(msg, Catch::Matchers::Contains(source));
  CHECK_THAT(msg, Catch::Matchers::Contains(reason));
}

} // namespace

// Pins: a refused key in a file is an error at its own line (comment and
// blank lines count), in any letter case, with an inline comment, for the
// value false as well as true, naming the upper-case key, "not accepted",
// the file and that key's reason (not another key's).
// Before refuse_keys existed this case failed before any parse. Matching
// the key case-sensitively, or counting only the non-blank, non-comment
// lines, makes it fail.
TEST_CASE("Config parser: refuse_keys refuses a key at its line, naming the "
          "key, the line, the file and the reason",
          "[config][refuse_keys]") {
  RefuseScratch dir;
  CoreUserConfigParser parser;
  parser.refuse_keys({{"FORCE", kForceReason}, {"STRESS", kStressReason}});

  SECTION("an upper-case key after a comment line and a blank line") {
    const std::string file = dir.write("val_upper.cfg",
                                       "# validation set\n"    // line 1
                                       "\n"                    // line 2
                                       "ATOM Ta\n"             // line 3
                                       "# the refused key\n"   // line 4
                                       "FORCE true\n"          // line 5
                                       "WATOM 73\n");          // line 6
    const std::string msg =
        runtime_error_of([&] { return parser.parse(file); }, "parse " + file);
    check_refusal(msg, "FORCE", 5, file, kForceReason);
  }
  SECTION("a lower-case key with an inline comment, value false") {
    const std::string file = dir.write("val_lower.cfg",
                                       "atom Ta\n"                        // 1
                                       "force false   # energies only\n"  // 2
                                       "watom 73\n");                     // 3
    const std::string msg =
        runtime_error_of([&] { return parser.parse(file); }, "parse " + file);
    check_refusal(msg, "FORCE", 2, file, kForceReason);
  }
  SECTION("a mixed-case STRESS gives the STRESS reason") {
    const std::string file = dir.write("val_mixed.cfg",
                                       "ATOM Ta\n"      // line 1
                                       "WATOM 73\n"     // line 2
                                       "\n"             // line 3
                                       "Stress true\n"  // line 4
                                       "INIT2B true\n");
    const std::string msg =
        runtime_error_of([&] { return parser.parse(file); }, "parse " + file);
    check_refusal(msg, "STRESS", 4, file, kStressReason);
    CHECK_THAT(msg, !Catch::Matchers::Contains(kForceReason));
  }
}

// Pins: CONFIG includes inherit the refused keys, at every depth; the error
// names the included file and the line of that file, and a refused key
// after an include still names the including file and its own line.
// Before refuse_keys existed this case failed before any parse. Dropping
// the refusal from the parser of a CONFIG include makes it fail.
TEST_CASE("Config parser: refuse_keys reaches into CONFIG includes",
          "[config][refuse_keys]") {
  RefuseScratch dir;
  CoreUserConfigParser parser;
  parser.refuse_keys({{"FORCE", kForceReason}});

  SECTION("a refused key inside an included file names that file and line") {
    const std::string inner = dir.write("inner.cfg",
                                        "# included settings\n"  // line 1
                                        "\n"                     // line 2
                                        "WATOM 73\n"             // line 3
                                        "FORCE true\n");         // line 4
    const std::string outer = dir.write("outer.cfg",
                                        "ATOM Ta\n"                // line 1
                                        "CONFIG " + inner + "\n"   // line 2
                                        "INIT2B true\n");          // line 3
    const std::string msg = runtime_error_of(
        [&] { return parser.parse(outer); }, "parse " + outer);
    check_refusal(msg, "FORCE", 4, inner, kForceReason);
  }
  SECTION("two levels of includes inherit the refusal") {
    const std::string deep = dir.write("deep.cfg",
                                       "INIT2B true\n"     // line 1
                                       "RCUT2B 5.0\n"      // line 2
                                       "force true\n");    // line 3
    const std::string mid = dir.write("mid.cfg",
                                      "WATOM 73\n"              // line 1
                                      "CONFIG " + deep + "\n");  // line 2
    const std::string top = dir.write("top.cfg",
                                      "ATOM Ta\n"               // line 1
                                      "CONFIG " + mid + "\n");   // line 2
    const std::string msg =
        runtime_error_of([&] { return parser.parse(top); }, "parse " + top);
    check_refusal(msg, "FORCE", 3, deep, kForceReason);
  }
  SECTION("a refused key after an include names the including file") {
    const std::string inner = dir.write("clean_inner.cfg", "WATOM 73\n");
    const std::string outer = dir.write("after_include.cfg",
                                        "ATOM Ta\n"               // line 1
                                        "CONFIG " + inner + "\n"  // line 2
                                        "INIT2B true\n"           // line 3
                                        "\n"                      // line 4
                                        "FORCE true\n");          // line 5
    const std::string msg = runtime_error_of(
        [&] { return parser.parse(outer); }, "parse " + outer);
    check_refusal(msg, "FORCE", 5, outer, kForceReason);
  }
}

// Pins: refusal is by exact key, never by text. A file whose keys are not
// refused -- with a comment and an inline comment that mention FORCE, a key
// whose name contains FORCE (ZERO_COM_FORCE), FSIGMA, and a DBFILE value
// FORCE -- parses to exactly the Context a plain parser returns, CONFIG
// include included.
// Before refuse_keys existed this case failed before any parse. Refusing
// by text (any line that mentions FORCE) instead of by key makes it fail.
TEST_CASE("Config parser: refuse_keys leaves a file without refused keys "
          "unchanged",
          "[config][refuse_keys]") {
  RefuseScratch dir;
  const std::string inner = dir.write(
      "plain_inner.cfg",
      "WATOM 73\n"
      "# FORCE true is decided by the configuration file\n"
      "ZERO_COM_FORCE true\n");
  const std::string file = dir.write("plain.cfg",
                                     "ATOM Ta\n"
                                     "INIT2B true   # not FORCE\n"
                                     "RCUT2B 5.0\n"
                                     "FSIGMA 0.2\n"
                                     "DBFILE FORCE\n"
                                     "CONFIG " + inner + "\n");

  CoreUserConfigParser refusing;
  refusing.refuse_keys({{"FORCE", kForceReason}, {"STRESS", kStressReason}});

  Context with_refusal;
  REQUIRE_NOTHROW(with_refusal = refusing.parse(file));
  const Context plain = CoreUserConfigParser().parse(file);
  CHECK(with_refusal == plain);
  CHECK_FALSE(with_refusal.has_stored("FORCE"));
  CHECK_FALSE(with_refusal.has_stored("STRESS"));
  CHECK(with_refusal.get<bool>("ZERO_COM_FORCE") == true);
  REQUIRE(with_refusal.size("DBFILE") == 1);
  CHECK(with_refusal("DBFILE")[0] == "FORCE");
}

// Pins: refused keys belong to the parser they were given to. The parser
// keeps refusing on every parse; after it refused (also from inside an
// include, which unwinds a nested parse) or parsed a file with an include
// successfully, a new parser -- plain, or refusing another key -- reads
// the same files and stores FORCE.
// Before refuse_keys existed this case failed before any parse. Keeping
// the refused keys in a static or thread_local, or in parse state that an
// exception from inside an include leaves set, makes it fail.
TEST_CASE("Config parser: refuse_keys does not leak into another parser",
          "[config][refuse_keys]") {
  RefuseScratch dir;
  const std::string with_force = dir.write("force.cfg", "ATOM Ta\nFORCE true\n");
  const std::string inc_force = dir.write("inc_force.cfg", "FORCE true\n");
  const std::string outer_force = dir.write(
      "outer_force.cfg", "ATOM Ta\nCONFIG " + inc_force + "\n");
  const std::string inc_clean = dir.write("inc_clean.cfg", "WATOM 73\n");
  const std::string outer_clean = dir.write(
      "outer_clean.cfg", "ATOM Ta\nCONFIG " + inc_clean + "\n");

  {
    CoreUserConfigParser refusing;
    refusing.refuse_keys({{"FORCE", kForceReason}});
    CHECK_THROWS_AS(refusing.parse(with_force), std::runtime_error);
    CHECK_THROWS_AS(refusing.parse(with_force), std::runtime_error);
    CHECK_NOTHROW(refusing.parse(outer_clean));
    CHECK_THROWS_AS(refusing.parse(outer_force), std::runtime_error);
    CHECK_NOTHROW(refusing.parse(outer_clean));
  }

  const CoreUserConfigParser plain{};
  Context direct, included;
  REQUIRE_NOTHROW(direct = plain.parse(with_force));
  CHECK(direct.get<bool>("FORCE") == true);
  REQUIRE_NOTHROW(included = plain.parse(outer_force));
  CHECK(included.get<bool>("FORCE") == true);

  CoreUserConfigParser stress_only;
  stress_only.refuse_keys({{"STRESS", kStressReason}});
  Context other;
  REQUIRE_NOTHROW(other = stress_only.parse(outer_force));
  CHECK(other.get<bool>("FORCE") == true);
}

// Pins: the refusal is an option of the parser, not of file input: the
// stream and line overloads refuse too, naming the line.
// Before refuse_keys existed this case failed before any parse. A stream
// or line overload of parse() that does not pass the refused keys on makes
// it fail.
TEST_CASE("Config parser: refuse_keys applies to stream and line input",
          "[config][refuse_keys]") {
  CoreUserConfigParser parser;
  parser.refuse_keys({{"FORCE", kForceReason}});

  SECTION("a stream") {
    std::istringstream in("ATOM Ta\n\nFORCE true\n");  // FORCE on line 3
    const std::string msg =
        runtime_error_of([&] { return parser.parse(in); }, "parse(stream)");
    check_refusal(msg, "FORCE", 3, "", kForceReason);
  }
  SECTION("lines") {
    const std::vector<std::string> lines = {"ATOM Ta", "force true"};
    const std::string msg =
        runtime_error_of([&] { return parser.parse(lines); }, "parse(lines)");
    check_refusal(msg, "FORCE", 2, "", kForceReason);
  }
}

// ---------------------------------------------------------------------------
// CONFIG with an inline comment
//
// Every key may carry an inline comment ("KEY value   # comment"); CONFIG,
// whose value is the path of the file to include, must too. The cases use
// the scratch directory of the refuse_keys cases above (RefuseScratch: unique
// to the process, removed with its files).
// ---------------------------------------------------------------------------
namespace {

/* The keys an included file sets, checked after the include. */
void check_included_keys(const Context &c) {
  CHECK(c.get<double>("WATOM") == 73.0);
  CHECK(c.get<double>("RCUT2B") == 5.0);
}

} // namespace

// Pins: a CONFIG line whose path is followed by an inline comment (blanks
// or a tab before the '#') includes the file it names, in a file and a
// stream: the included keys are set, and the result equals that of the same
// line without the comment.
// Before the fix: every section threw std::runtime_error "Failed to open
// config file: <path>   " -- the path was read up to the '#', with the blanks
// before it, so no such file existed (`tadah train -c` stopped with that
// error).
TEST_CASE("Config parser: a CONFIG line with an inline comment includes the "
          "file it names",
          "[config][config_include]") {
  RefuseScratch dir;
  const std::string inner =
      dir.write("inner_settings.cfg", "WATOM 73\nRCUT2B 5.0\n");
  const std::string plain = dir.write(
      "plain_include.cfg", "ATOM Ta\nCONFIG " + inner + "\nINIT2B true\n");
  const Context expected = CoreUserConfigParser().parse(plain);
  check_included_keys(expected);

  SECTION("a file, blanks before the comment") {
    const std::string outer =
        dir.write("commented_include.cfg",
                  "ATOM Ta\n"
                  "CONFIG " + inner + "   # nested config\n"
                  "INIT2B true\n");
    Context c;
    REQUIRE_NOTHROW(c = CoreUserConfigParser().parse(outer));
    check_included_keys(c);
    CHECK(c == expected);
  }
  SECTION("a file, a tab before the comment") {
    const std::string outer =
        dir.write("tab_include.cfg",
                  "ATOM Ta\n"
                  "CONFIG " + inner + "\t# nested config\n"
                  "INIT2B true\n");
    Context c;
    REQUIRE_NOTHROW(c = CoreUserConfigParser().parse(outer));
    CHECK(c == expected);
  }
  SECTION("a stream") {
    std::istringstream in("ATOM Ta\nCONFIG " + inner +
                          "   # nested config\nINIT2B true\n");
    Context c;
    REQUIRE_NOTHROW(c = CoreUserConfigParser().parse(in));
    CHECK(c == expected);
  }
}

namespace {

/* Captures what the Logger writes (to std::cout) with ERROR lines on;
 * restores the stream and the level afterwards. */
class CaptureLog {
public:
  CaptureLog()
      : level_(Logger::instance().getLogLevel()),
        old_(std::cout.rdbuf(out_.rdbuf())) {
    Logger::instance().setLogLevel(LogLevel::ERROR);
  }
  ~CaptureLog() {
    std::cout.rdbuf(old_);
    Logger::instance().setLogLevel(level_);
  }
  std::string text() const { return out_.str(); }

private:
  LogLevel level_;
  std::ostringstream out_;
  std::streambuf *old_;
};

/* The text of each line of `log`, after the Logger's "[time] ERROR: "
 * prefix (a line without one, such as the rest of a message that holds a
 * newline, as it is). */
std::vector<std::string> log_lines(const std::string &log) {
  std::vector<std::string> out;
  std::istringstream in(log);
  std::string line;
  const std::string tag = "ERROR: ";
  while (std::getline(in, line)) {
    const auto at = line.find(tag);
    out.push_back(at == std::string::npos ? line : line.substr(at + tag.size()));
  }
  return out;
}

std::string upper_case(std::string t) {
  for (char &ch : t)
    ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
  return t;
}

std::string trimmed(const std::string &t) {
  const auto b = t.find_first_not_of(" \t\r");
  if (b == std::string::npos) return "";
  return t.substr(b, t.find_last_not_of(" \t\r") - b + 1);
}

} // namespace

// Pins: with an inline comment on the CONFIG line, a missing file is reported
// by its path alone ("Failed to open config file: <path>", the path without
// the blanks before the '#'), and a circular include names the files of the
// chain, from the file of the parse, each by its path alone ("... CONFIG
// files: <a> -> <b> -> <a>"); each names the CONFIG line, its line and its
// file, as every error of a line does: "Failed to open config file: <path>
// (CONFIG on line 1 of <outer>)", "... -> <a> (CONFIG on line 2 of <b>)".
// Before the fix: the missing file was reported as "<path>   " (the blanks
// before the '#'), and the circular include was never detected: the first
// CONFIG line already failed to open "<b>  ", so the message was "Failed to
// open config file: <b>  " instead of the chain. Neither named the CONFIG
// line ("Failed to open config file: <path>", after the Logger line
// "CoreUserConfigParser::parse -> Failed to open file: <path>", and the
// chain alone). Until the file of the parse was among the files being read,
// the circle was found one include later, after the lines of <a> were read
// a second time: "... CONFIG files: <b> -> <a> -> <b> (CONFIG on line 2 of
// <a>)".
TEST_CASE("Config parser: a CONFIG line with an inline comment names the "
          "file alone in its errors",
          "[config][config_include][line_errors]") {
  RefuseScratch dir;

  SECTION("a missing file") {
    const std::string missing = dir.path("missing.cfg");
    const std::string outer = dir.write(
        "missing_include.cfg", "CONFIG " + missing + "   # not there\n");
    CaptureLog log;
    const std::string msg = runtime_error_of(
        [&] { return CoreUserConfigParser().parse(outer); }, "parse " + outer);
    CHECK(msg == "Failed to open config file: " + missing +
                     " (CONFIG on line 1 of " + outer + ")");
    CHECK(log.text().empty());
  }
  SECTION("a circular include") {
    const std::string b = dir.write(
        "loop_b.cfg",
        "WATOM 73\nCONFIG " + dir.path("loop_a.cfg") + "  # back to a\n");
    const std::string a =
        dir.write("loop_a.cfg", "ATOM Ta\nCONFIG " + b + "  # on to b\n");
    const std::string msg = runtime_error_of(
        [&] { return CoreUserConfigParser().parse(a); }, "parse " + a);
    CHECK(msg == "Circular dependency detected between CONFIG files: " + a +
                     " -> " + b + " -> " + a + " (CONFIG on line 2 of " + b +
                     ")");
  }
}

/* Pins: a line whose key is no configuration key is refused naming the
 * key (upper case), its line and its file, with the closest key, in one
 * message and without a Logger line:
 *   "Invalid key VERBOS on line 3 of <file> (closest key: VERBOSE)"
 * for a configuration file (comment and blank lines counted), for a file
 * it includes with CONFIG (that file and its line, and the CONFIG line that
 * includes it: "Invalid key VERBOS on line 2 of <inner> (CONFIG on line 2
 * of <outer>) (closest key: VERBOSE)"), for a potential and, without a
 * file, for a stream ("on line 2"). A TASK line is such a line: TASK is no
 * key ("Invalid key TASK on line 2 of <file>"), in a file and in a file it
 * includes.
 * Before the fix: the message was "Invalid key: VERBOS", without the line
 * and the file, after two Logger ERROR lines ("CoreUserConfigParser::parse
 * -> Invalid key: VERBOS", "  Did you mean: VERBOSE"); a TASK line was read
 * as the key TASK and stored (no exception). Until every error of a line of
 * an included file named the CONFIG lines that include it, the line of
 * <inner> was named without its CONFIG line ("Invalid key VERBOS on line 2
 * of <inner> (closest key: VERBOSE)"). */
TEST_CASE("Config parser: an unknown key is refused naming the key, the "
          "line and the file",
          "[config][unknown_key]") {
  RefuseScratch dir;
  const std::string hint = " (closest key: VERBOSE)";
  SECTION("a configuration file") {
    const std::string f =
        dir.write("typo.cfg", "# a comment\n\nVERBOS 1\nATOM Ta\n");
    CaptureLog log;
    const std::string msg = runtime_error_of(
        [&] { return CoreUserConfigParser().parse(f); }, "parse " + f);
    CHECK(msg == "Invalid key VERBOS on line 3 of " + f + hint);
    CHECK(log.text().empty());
  }
  SECTION("a file it includes with CONFIG") {
    const std::string inner = dir.write("inner_typo.cfg", "ATOM Ta\nverbos 1\n");
    const std::string outer =
        dir.write("outer_typo.cfg", "WATOM 73\nCONFIG " + inner + "\n");
    const std::string msg = runtime_error_of(
        [&] { return CoreUserConfigParser().parse(outer); }, "parse " + outer);
    CHECK(msg == "Invalid key VERBOS on line 2 of " + inner +
                     " (CONFIG on line 2 of " + outer + ")" + hint);
  }
  SECTION("a potential") {
    const std::string pot = dir.write(
        "typo_pot.tadah", "ATOM Ta\nWATOM 73\nVERBOS 0\nWEIGHTS 1.0 2.0\n");
    const std::string msg = runtime_error_of(
        [&] { return CoreUserConfigParser().parse(pot); }, "parse " + pot);
    CHECK(msg == "Invalid key VERBOS on line 3 of " + pot + hint);
  }
  SECTION("a stream") {
    std::istringstream in("ATOM Ta\nVERBOS 1\n");
    const std::string msg = runtime_error_of(
        [&] { return CoreUserConfigParser().parse(in); }, "parse(stream)");
    CHECK(msg == "Invalid key VERBOS on line 2" + hint);
  }
  SECTION("a TASK line, in a file and in a file it includes") {
    const std::string f = dir.write(
        "with_task.cfg", "ATOM Ta\nTASK predict\nDBFILE db.tadah\n");
    CHECK_THAT(runtime_error_of([&] { return CoreUserConfigParser().parse(f); },
                                "parse " + f),
               Catch::Matchers::StartsWith("Invalid key TASK on line 2 of " +
                                           f + " (closest key: "));
    const std::string outer =
        dir.write("includes_task.cfg", "ATOM Ta\n\nCONFIG " + f + "\n");
    CHECK_THAT(
        runtime_error_of([&] { return CoreUserConfigParser().parse(outer); },
                         "parse " + outer),
        Catch::Matchers::StartsWith("Invalid key TASK on line 2 of " + f));
  }
}

namespace {

/* What follows the first line of the refusal of a value of @p key that the
 * key's types do not take: "  Allowed schema:" and "    <KEY>   <type>"
 * for each of its value types, "  Format:" and "    <KEY>   <format>" for
 * each of its formats and, when it has examples, "  Examples:" and
 * "    <KEY>   <line>" for each non-blank line of each example, each on a
 * line of its own (the lines the Logger writes for such a value of the
 * command line). */
std::string value_help(const std::string &key) {
  const BlueprintKey &k = BlueprintParser::getKeysMapRef().at(key);
  std::string out = "\n  Allowed schema:";
  for (const std::string &type : k.value_type)
    out += "\n    " + key + "   " + type;
  out += "\n  Format:";
  for (const std::string &format : k.value_format)
    out += "\n    " + key + "   " + format;
  if (!k.examples.empty()) {
    out += "\n  Examples:";
    for (const std::string &example : k.examples) {
      std::istringstream lines(example);
      std::string line;
      while (std::getline(lines, line))
        if (!trimmed(line).empty()) out += "\n    " + key + "   " + trimmed(line);
    }
  }
  return out;
}

}  // namespace

/* Pins: an error of a line of a file names the key, the line and the file
 * (the line alone for lines of no file), in one message and without a
 * Logger line, as an unknown key does:
 *  - a value the key's types do not take, with the key's schema, format and
 *    examples after the first line: "Invalid value for key RCUT2B on line 3
 *    of <file>: 'abc'", then "  Allowed schema:", "    RCUT2B   {DOUBLE}",
 *    "  Format:", "    RCUT2B   <double> [<double> ...]", "  Examples:",
 *    "    RCUT2B   3.0" and "    RCUT2B   3.0 7.5"; a wrong number of
 *    values ('MODEL M_BLR', 'EMBF0 ZERO FREE') and a key without a value
 *    ("...: no value") alike; in a file a configuration includes with
 *    CONFIG (that file and its line, and the CONFIG line that includes it:
 *    "Invalid value for key EMBF0 on line 2 of <inner> (CONFIG on line 2 of
 *    <outer>): 'ZERO FREE'"), in a potential, and in a stream ("on line
 *    2");
 *  - more values than the key takes, over its lines, naming the lines
 *    before it: "Too many values for key MODEL on line 2 of <file>: MODEL
 *    takes at most 3, and its lines read so far give 4, with line 1 of
 *    <file> before it"; a second line of a key of one value: "Key ESIGMA
 *    is set on line 1 and on line 2: a key of one value is set once in a
 *    configuration file and the files it includes with CONFIG" (lines of no
 *    file), while two lines of ATOM give its two values;
 *  - a block that is not closed: "OPTIMIZER on line 2 of <file> is not
 *    closed; expected ENDOPTIMIZER".
 * Before the fix: "Invalid schema for key: RCUT2B" (and MODEL, EMBF0), with
 * neither line nor file, after the Logger ERROR lines
 * "CoreUserConfigParser::parse -> Invalid format for key: value -> RCUT2B:
 * abc" and the schema, format and examples; "Exceeded capacity for key:
 * MODEL" after the Logger line "Context::add -> capacity exceeded for:
 * MODEL"; "OPTIMIZER block is not closed with ENDOPTIMIZER". Until the
 * configuration was read as the concatenation of its lines, the second
 * ESIGMA line replaced the first (ESIGMA 2, no exception). Until the
 * refusals of a line named the CONFIG lines of an included line and the
 * lines of the key before it, the value of <inner> was named without its
 * CONFIG line, and "Too many values" ended "... read so far give 4". */
TEST_CASE("Config parser: an error of a line names the key, the line and "
          "the file",
          "[config][line_errors]") {
  RefuseScratch dir;
  SECTION("a value the key's types do not take") {
    const std::string f =
        dir.write("value.cfg", "# a comment\n\nRCUT2B abc\nATOM Ta\n");
    CaptureLog log;
    const std::string msg = runtime_error_of(
        [&] { return CoreUserConfigParser().parse(f); }, "parse " + f);
    CHECK(msg == "Invalid value for key RCUT2B on line 3 of " + f +
                     ": 'abc'\n"
                     "  Allowed schema:\n"
                     "    RCUT2B   {DOUBLE}\n"
                     "  Format:\n"
                     "    RCUT2B   <double> [<double> ...]\n"
                     "  Examples:\n"
                     "    RCUT2B   3.0\n"
                     "    RCUT2B   3.0 7.5");
    CHECK(log.text().empty());
  }
  SECTION("a wrong number of values, and no value") {
    struct Case {
      std::string name, body, message;
    };
    const std::vector<Case> cases = {
        {"few.cfg", "ATOM Ta\nMODEL M_BLR\n",
         "Invalid value for key MODEL on line 2 of " + dir.path("few.cfg") +
             ": 'M_BLR'" + value_help("MODEL")},
        {"two.cfg", "ATOM Ta\n# one value\nEMBF0 ZERO FREE\n",
         "Invalid value for key EMBF0 on line 3 of " + dir.path("two.cfg") +
             ": 'ZERO FREE'" + value_help("EMBF0")},
        {"none.cfg", "RCUT2B\n",
         "Invalid value for key RCUT2B on line 1 of " + dir.path("none.cfg") +
             ": no value" + value_help("RCUT2B")},
    };
    for (const Case &c : cases) {
      const std::string f = dir.write(c.name, c.body);
      CaptureLog log;
      const std::string msg = runtime_error_of(
          [&] { return CoreUserConfigParser().parse(f); }, "parse " + f);
      INFO(c.name);
      CHECK(msg == c.message);
      CHECK(log.text().empty());
    }
  }
  SECTION("a file it includes with CONFIG, a potential, a stream") {
    const std::string inner =
        dir.write("inner_value.cfg", "ATOM Ta\nEMBF0 ZERO FREE\n");
    const std::string outer =
        dir.write("outer_value.cfg", "WATOM 73\nCONFIG " + inner + "\n");
    CHECK(runtime_error_of(
              [&] { return CoreUserConfigParser().parse(outer); },
              "parse " + outer) ==
          "Invalid value for key EMBF0 on line 2 of " + inner +
              " (CONFIG on line 2 of " + outer + "): 'ZERO FREE'" +
              value_help("EMBF0"));
    const std::string pot = dir.write(
        "value_pot.tadah", "ATOM Ta\nWATOM 73\nRCUT2B abc\nWEIGHTS 1.0 2.0\n");
    CHECK(runtime_error_of([&] { return CoreUserConfigParser().parse(pot); },
                           "parse " + pot) ==
          "Invalid value for key RCUT2B on line 3 of " + pot + ": 'abc'" +
              value_help("RCUT2B"));
    std::istringstream in("ATOM Ta\nMODEL M_BLR\n");
    CHECK(runtime_error_of([&] { return CoreUserConfigParser().parse(in); },
                           "parse(stream)") ==
          "Invalid value for key MODEL on line 2: 'M_BLR'" +
              value_help("MODEL"));
  }
  SECTION("more values than the key takes, over its lines") {
    const std::string f = dir.write(
        "twice.cfg", "MODEL M_BLR BF_Linear\nMODEL M_KRR Kern_Linear\n");
    CaptureLog log;
    CHECK(runtime_error_of([&] { return CoreUserConfigParser().parse(f); },
                           "parse " + f) ==
          "Too many values for key MODEL on line 2 of " + f +
              ": MODEL takes at most 3, and its lines read so far give 4, "
              "with line 1 of " + f + " before it");
    CHECK(log.text().empty());
    // A second line of a key of one value is refused, naming both lines
    // (it replaced the value of the line before: ESIGMA 2); a key of
    // several values adds the values of its lines, up to the number it
    // takes.
    const auto &keys = BlueprintParser::getKeysMapRef();
    REQUIRE(keys.at("ESIGMA").value_N.at(0) == 1);
    REQUIRE(std::abs(keys.at("ATOM").value_N.at(0)) > 2);
    CHECK(runtime_error_of(
              [] {
                return CoreUserConfigParser().parse(std::vector<std::string>{
                    "ESIGMA 1", "ESIGMA 2", "ATOM Ta", "ATOM Nb"});
              },
              "parse(lines)") ==
          "Key ESIGMA is set on line 1 and on line 2: a key of one value is "
          "set once in a configuration file and the files it includes with "
          "CONFIG");
    Context c;
    REQUIRE_NOTHROW(c = CoreUserConfigParser().parse(std::vector<std::string>{
                        "ESIGMA 1", "ATOM Ta", "ATOM Nb"}));
    CHECK(c.get<double>("ESIGMA") == 1);
    CHECK(c.size("ATOM") == 2);
  }
  SECTION("a block that is not closed") {
    const std::string f =
        dir.write("open_block.cfg", "ATOM Ta\nOPTIMIZER\nALGO x\n");
    CHECK(runtime_error_of([&] { return CoreUserConfigParser().parse(f); },
                           "parse " + f) ==
          "OPTIMIZER on line 2 of " + f + " is not closed; expected "
                                          "ENDOPTIMIZER");
  }
}

/* Pins: when CoreUserConfigParser refuses a value of a key (the value does
 * not have the key's format), the lines it writes under "  Examples:" are,
 * for every key whose value can be refused (one of "@", "", "1.5", "@ @ @"
 * is), exactly one line "    <KEY>   <line>" for each non-blank line of each
 * of the key's examples, in order, and the word after the key is never the
 * key again.
 * Before the fix: an example that began with its key was printed with the
 * key twice ("    BETA   BETA 0.0001"), and an example of several lines was
 * printed as one message, whose later lines were the example's own text
 * without the "    <KEY>   " in front (or the log prefix of a line):
 * "    CGRID2B   CGRID2B   1.0 2.0" then "CGRID2B   1.5 2.5", and
 * "  TYPE2B    D2_MIE 11 6 Ti Ti" after "    TYPE2B   TYPE2B    D2_mJoin". */
TEST_CASE("Config parser: a refused value lists each example once, after "
          "its key", "[config][examples]") {
  std::size_t keys = 0;
  for (const auto &kv : BlueprintParser::getKeysMapRef()) {
    const std::string &key = kv.first;
    const BlueprintKey &k = kv.second;
    if (k.examples.empty()) continue;
    std::string refused;
    bool found = false;
    for (const std::string candidate : {"@", "", "1.5", "@ @ @"}) {
      if (!CoreUserConfigParser::validateSchemaOnly(candidate, k)) {
        refused = candidate;
        found = true;
        break;
      }
    }
    if (!found) continue;   /* every value has its format: never refused */
    ++keys;
    INFO("key " << key << ", refused value '" << refused << "'");

    std::vector<std::string> expected;
    for (const std::string &example : k.examples) {
      std::istringstream lines(example);
      std::string line;
      while (std::getline(lines, line))
        if (!trimmed(line).empty())
          expected.push_back("    " + key + "   " + trimmed(line));
    }

    std::string log;
    {
      CaptureLog capture;
      CHECK_FALSE(CoreUserConfigParser::validateWithBlueprint(key, refused, k));
      log = capture.text();
    }
    const std::vector<std::string> all = log_lines(log);
    const auto head = std::find(all.begin(), all.end(), "  Examples:");
    REQUIRE(head != all.end());
    const std::vector<std::string> printed(head + 1, all.end());
    CHECK(printed == expected);
    for (const std::string &line : printed) {
      INFO("printed '" << line << "'");
      std::istringstream words(line);
      std::string first, second;
      words >> first >> second;
      CHECK(upper_case(first) == key);
      CHECK(upper_case(second) != key);
    }
  }
  CHECK(keys > 100);
}

/* Pins: a key that CoreUserConfigParser reads as a block -- the key alone
 * on its line opens it and the lines up to END<KEY> are its value: the
 * `tadah refit` stages INIT, EXPLORE, REFINE and FUSE and their nested
 * OPTIMIZER -- has no examples in the blueprint. "<KEY>   <value>", as the
 * parser's refusal and the configuration-keys page show an example, is not
 * how such a key is written (the parser does not read the rest of the
 * opening line). The block keys are found from the parser itself: the key
 * alone on a line is refused as a block that is not closed ("<KEY> on line
 * 1 is not closed; expected END<KEY>"; "<KEY> block is not closed with
 * END<KEY>" before the line was named).
 * Before the fix: all five had an example, "INIT / PHI XMIN PIN / ENDINIT"
 * and the like, and OPTIMIZER's was "see HPO_REFIT_2B_EAM/REDESIGN/configs
 * (OPTIMIZER nested in EXPLORE/REFINE/FUSE)", a directory that is in no
 * Tadah repository. */
TEST_CASE("Config parser: a key written as a block has no examples",
          "[config][examples]") {
  std::set<std::string> blocks;
  for (const auto &kv : BlueprintParser::getKeysMapRef()) {
    const std::string &key = kv.first;
    std::string what;
    {
      CaptureLog quiet;
      try {
        CoreUserConfigParser().parse(std::vector<std::string>{key});
      } catch (const std::exception &e) {
        what = e.what();
      }
    }
    if (what != key + " on line 1 is not closed; expected END" + key)
      continue;
    blocks.insert(key);
    INFO("block key " << key);
    CHECK(kv.second.examples.empty());
  }
  CHECK(blocks == std::set<std::string>{"EXPLORE", "FUSE", "INIT",
                                        "OPTIMIZER", "REFINE"});
}

namespace {
/* The message parse() throws for `lines`, or "no exception". */
std::string parse_refusal(const std::vector<std::string> &lines) {
  try {
    (void)CoreUserConfigParser().parse(lines);
  } catch (const std::exception &e) {
    return e.what();
  }
  return "no exception";
}

/* The message parse() throws for the file @p path, or "no exception". */
std::string parse_file_refusal(const std::string &path) {
  try {
    (void)CoreUserConfigParser().parse(path);
  } catch (const std::exception &e) {
    return e.what();
  }
  return "no exception";
}
}  // namespace

/* Pins: a configuration line whose list values are joined by a comma or a
 * '+' (which the parser does not split at: it splits a line at blanks) is
 * refused when it is parsed, naming the line, the key, the value and the
 * separator, and the line to write instead:
 *   "Invalid value for key FIXWEIGHT on line 1: '0.1,0.2' separates values
 *    with ',', which a configuration line is not split at (values are
 *    separated by blanks: FIXWEIGHT 0.1 0.2)"
 * also with the file and line of a configuration file (line 3), for
 * '+' ('MPARAMS 1.0+2.0'), for both ('CGRID2B 1.0,2.0+3.0': "',' and
 * '+'"), with blanks around the comma ('SGRID2B 1.0, 2.0': SGRID2B 1.0
 * 2.0), for a key of several single values ('MODEL M_BLR,BF_Linear'), for an
 * index list joined by '+' ('FIXINDEX 13+14'), and for a line of 2048
 * characters or more. A value that blanks would not make valid either is
 * refused as any value the key's types do not take ('SBASIS 1,2': "Invalid
 * value for key SBASIS on line 1: '1,2'" and the key's schema, format and
 * examples; "Invalid schema for key: SBASIS" before the line was named).
 * What the parser keeps as one value is accepted: an index list with commas
 * ('FIXINDEX 13,14', its reader splits it), a path with a comma or a '+'
 * ('DBFILE a,b.tadah', 'SETFL Ta+v2.eam.alloy'), an exponent's sign
 * ('FIXWEIGHT 1e+5 2'). (Until the names keys this test pinned 'ATOM
 * Ti+Nb' as the one value 'Ti+Nb'; ATOM's values are names, and the next
 * test pins its refusal.)
 * Before the fix: 'FIXWEIGHT 0.1,0.2', 'MPARAMS 1.0+2.0', 'CGRID2B
 * 1.0,2.0+3.0', 'SGRID2B 1.0, 2.0', 'FIXINDEX 13+14' and the long line were
 * accepted (one value per word, '0.1,0.2' refused only when read, and
 * '13+14' by FIXINDEX's reader), and 'MODEL M_BLR,BF_Linear' was refused as
 * "Invalid schema for key: MODEL". */
TEST_CASE("Config parser: a comma or '+' between values is refused, naming "
          "the line, the key and the separator",
          "[config][separators]") {
  const std::string tail = ", which a configuration line is not split at "
                           "(values are separated by blanks: ";
  struct Case {
    std::string line, message;
  };
  const std::vector<Case> cases = {
      {"FIXWEIGHT 0.1,0.2",
       "Invalid value for key FIXWEIGHT on line 1: '0.1,0.2' separates "
       "values with ','" + tail + "FIXWEIGHT 0.1 0.2)"},
      {"MPARAMS 1.0+2.0",
       "Invalid value for key MPARAMS on line 1: '1.0+2.0' separates values "
       "with '+'" + tail + "MPARAMS 1.0 2.0)"},
      {"CGRID2B 1.0,2.0+3.0",
       "Invalid value for key CGRID2B on line 1: '1.0,2.0+3.0' separates "
       "values with ',' and '+'" + tail + "CGRID2B 1.0 2.0 3.0)"},
      {"SGRID2B 1.0, 2.0",
       "Invalid value for key SGRID2B on line 1: '1.0, 2.0' separates "
       "values with ','" + tail + "SGRID2B 1.0 2.0)"},
      {"model M_BLR,BF_Linear",
       "Invalid value for key MODEL on line 1: 'M_BLR,BF_Linear' separates "
       "values with ','" + tail + "MODEL M_BLR BF_Linear)"},
      {"FIXINDEX 13+14",
       "Invalid value for key FIXINDEX on line 1: '13+14' separates values "
       "with '+'" + tail + "FIXINDEX 13 14)"},
      {"SBASIS 1,2", "Invalid value for key SBASIS on line 1: '1,2'" +
                         value_help("SBASIS")},
  };
  for (const Case &c : cases) {
    INFO(c.line);
    CHECK(parse_refusal({c.line}) == c.message);
  }
  // The file and line of a configuration file.
  RefuseScratch dir;
  const std::string file =
      dir.write("separators.cfg", "# comment\nVERBOSE 1\nFIXWEIGHT 0.1,0.2\n");
  CHECK(parse_file_refusal(file) ==
        "Invalid value for key FIXWEIGHT on line 3 of " + file +
            ": '0.1,0.2' separates values with ','" + tail +
            "FIXWEIGHT 0.1 0.2)");
  // A line of 2048 characters or more.
  std::string long_line = "FIXWEIGHT 1.0";
  while (long_line.size() < 2100) long_line += ",2.0";
  const std::string long_msg = parse_refusal({long_line});
  INFO(long_msg.substr(0, 80));
  CHECK(long_msg.rfind("Invalid value for key FIXWEIGHT on line 1: "
                       "'1.0,2.0,2.0,",
                       0) == 0);
  CHECK(long_msg.find("' separates values with ','" + tail +
                      "FIXWEIGHT 1.0 2.0 2.0 ") != std::string::npos);

  // One value per word, as the parser keeps it.
  Context c;
  REQUIRE_NOTHROW(c = CoreUserConfigParser().parse(std::vector<std::string>{
                      "FIXINDEX 13,14", "DBFILE a,b.tadah",
                      "SETFL Ta+v2.eam.alloy", "FIXWEIGHT 1e+5 2"}));
  CHECK(c.size("FIXINDEX") == 1);
  CHECK(c.get<std::string>("FIXINDEX") == "13,14");
  CHECK(c.get<std::string>("DBFILE") == "a,b.tadah");
  CHECK(c.get<std::string>("SETFL") == "Ta+v2.eam.alloy");
  CHECK(c.size("FIXWEIGHT") == 2);
  CHECK(c.get<double>("FIXWEIGHT") == 1e5);
}

/* Pins: a key whose STRING values are names (blueprint field names: ATOM,
 * atompair, RCTYPE2B, RCTYPEMB, TYPE2B, TYPEMB, MODEL, WATOM, WNOISE)
 * refuses a value with a comma, or a '+' joined to the character before
 * it, which no name holds: the refusal names the line, the key, the value
 * and the separator, and the line to write, as for a list of numbers:
 *   "Invalid value for key ATOM on line 1: 'Ta,Nb' separates values with
 *    ',', which a configuration line is not split at (values are separated
 *    by blanks: ATOM Ta Nb)"
 * likewise 'ATOM Ti+Nb', 'atompair Ta+Nb', 'RCTYPE2B Cut_Cos,Cut_Tanh',
 * 'TYPE2B D2_Blip 4 4 Ta,Nb Nb,Ta' and 'WATOM Ta,Ta 0.5'. A names value
 * that blanks would not make valid either is refused as any value the
 * key's types do not take ('WNOISE E,F 1e-3', 'MODEL M_KRR Kern,Linear 2':
 * "Invalid value for key WNOISE on line 1: 'E,F 1e-3'" and the key's
 * schema, format and examples; "Invalid schema for key: ..." before the
 * line was named). A '+' after a name or a word that ends in e is a
 * separator too, not an exponent's sign: 'TYPE2B D2_Blip 4 4 Fe+Fe' (TYPE2B
 * D2_Blip 4 4 Fe Fe), 'DIMER true+1.0 false' (DIMER true 1.0 false); the
 * sign of an exponent stays in its number ('FIXWEIGHT 1e+5+2': FIXWEIGHT
 * 1e+5 2).
 * Before the fix: the names values were accepted, one value per word
 * ('Ta,Nb', 'Ti+Nb', 'Cut_Cos,Cut_Tanh', ...; tadah failed later with a
 * message that did not name the separator), and so were 'WNOISE E,F 1e-3'
 * and 'MODEL M_KRR Kern,Linear 2' ("no exception"); 'TYPE2B D2_Blip 4 4
 * Fe+Fe' and 'DIMER true+1.0 false' were refused as "Invalid schema for
 * key: TYPE2B" and "... DIMER" (any '+' after an e or an E was taken for an
 * exponent's sign); 'WATOM Ta,Ta 0.5' (its two names one word, short of a
 * name pair and a weight) and 'FIXWEIGHT 1e+5+2' pass as well (pins). */
TEST_CASE("Config parser: a comma or '+' in a name is refused as a "
          "separator",
          "[config][separators]") {
  const std::string tail = ", which a configuration line is not split at "
                           "(values are separated by blanks: ";
  auto msg = [&](const std::string &key, const std::string &value,
                 const std::string &seps, const std::string &line) {
    return "Invalid value for key " + key + " on line 1: '" + value +
           "' separates values with " + seps + tail + line + ")";
  };
  struct Case {
    std::string line, message;
  };
  const std::vector<Case> cases = {
      {"ATOM Ta,Nb", msg("ATOM", "Ta,Nb", "','", "ATOM Ta Nb")},
      {"ATOM Ti+Nb", msg("ATOM", "Ti+Nb", "'+'", "ATOM Ti Nb")},
      {"atompair Ta+Nb", msg("ATOMPAIR", "Ta+Nb", "'+'", "ATOMPAIR Ta Nb")},
      {"RCTYPE2B Cut_Cos,Cut_Tanh",
       msg("RCTYPE2B", "Cut_Cos,Cut_Tanh", "','",
           "RCTYPE2B Cut_Cos Cut_Tanh")},
      {"TYPE2B D2_Blip 4 4 Ta,Nb Nb,Ta",
       msg("TYPE2B", "D2_Blip 4 4 Ta,Nb Nb,Ta", "','",
           "TYPE2B D2_Blip 4 4 Ta Nb Nb Ta")},
      {"WATOM Ta,Ta 0.5", msg("WATOM", "Ta,Ta 0.5", "','", "WATOM Ta Ta 0.5")},
      {"WNOISE E,F 1e-3", "Invalid value for key WNOISE on line 1: "
                          "'E,F 1e-3'" + value_help("WNOISE")},
      {"MODEL M_KRR Kern,Linear 2", "Invalid value for key MODEL on line 1: "
                                    "'M_KRR Kern,Linear 2'" +
                                        value_help("MODEL")},
      {"TYPE2B D2_Blip 4 4 Fe+Fe",
       msg("TYPE2B", "D2_Blip 4 4 Fe+Fe", "'+'", "TYPE2B D2_Blip 4 4 Fe Fe")},
      {"DIMER true+1.0 false",
       msg("DIMER", "true+1.0 false", "'+'", "DIMER true 1.0 false")},
      {"FIXWEIGHT 1e+5+2",
       msg("FIXWEIGHT", "1e+5+2", "'+'", "FIXWEIGHT 1e+5 2")},
  };
  for (const Case &c : cases) {
    INFO(c.line);
    CHECK(parse_refusal({c.line}) == c.message);
  }
}

namespace {

/* The rule of the [skip_key_lines] cases: in a file whose key lines give
 * WEIGHTS, the lines of EFILTER and WDBFILE are skipped, and the line of
 * ESHIFT_ATOM when the file's lines give ESHIFT too. */
bool skip_in_weights_files(const std::set<std::string> &file_keys,
                           const std::string &key) {
  if (!file_keys.count("WEIGHTS")) return false;
  if (key == "EFILTER" || key == "WDBFILE") return true;
  return key == "ESHIFT_ATOM" && file_keys.count("ESHIFT") > 0;
}

/* "source:line KEY v1 v2" for each skipped line, to compare as a whole. */
std::vector<std::string> described(const std::vector<SkippedKeyLine> &lines) {
  std::vector<std::string> out;
  for (const SkippedKeyLine &l : lines) {
    std::string s = l.source + ":" + std::to_string(l.line) + " " + l.key;
    for (const std::string &v : l.values) s += " " + v;
    out.push_back(s);
  }
  return out;
}

} // namespace

// Pins: parse(file, rule, skipped) reads every line as parse(file) does and
// does not store a plain key line that the rule selects by the keys of the
// lines of the file the line is in, listing it instead:
//  - pot.cfg (ATOM, WEIGHTS, EFILTER 5 6, a blank line, "wdbfile 1 1" with
//    an in-line comment, ESHIFT_ATOM true; no ESHIFT): EFILTER and WDBFILE
//    are not stored, ESHIFT_ATOM true is; skipped is "pot.cfg:4 EFILTER 5
//    6", "pot.cfg:6 WDBFILE 1 1" (the line of the file, comment and blank
//    lines counted; the key upper case; the values as written); a parse
//    without a rule stores all of them;
//  - outer.cfg, which has no WEIGHTS line and includes pot.cfg with CONFIG
//    between its EFILTER -9 -7 and WDBFILE 0.5 1 2 lines: its own EFILTER
//    and WDBFILE are stored as it gives them (the include's, skipped,
//    neither replaces the one before the CONFIG line nor comes before the
//    one after it), WEIGHTS comes from pot.cfg, and skipped lists pot.cfg's
//    two lines under the path the CONFIG line gives;
//  - pot2.cfg with ESHIFT beside WEIGHTS: its ESHIFT_ATOM line is skipped;
//  - a skipped line is checked as any line: "EFILTER five 6" in a file with
//    WEIGHTS is refused, naming EFILTER;
//  - the body of a block (INIT ... ENDINIT) is not a key line of the file: a
//    WEIGHTS token inside it does not make the file's EFILTER skipped.
// Before the fix: does not compile (no parse overload with a rule; no
// SkippedKeyLine).
TEST_CASE("Config parser: a key line rule skips the lines it selects by the "
          "keys of the file they are in",
          "[config][skip_key_lines]") {
  RefuseScratch dir;
  const std::string pot = dir.write("pot.cfg",
                                    "# a potential\n"              // 1
                                    "ATOM Ta\n"                    // 2
                                    "WEIGHTS 0.25 -0.5\n"          // 3
                                    "EFILTER 5 6\n"                // 4
                                    "\n"                           // 5
                                    "wdbfile 1 1   # two sets\n"   // 6
                                    "ESHIFT_ATOM true\n");         // 7
  const CoreUserConfigParser parser;
  std::vector<SkippedKeyLine> skipped;

  SECTION("a file with WEIGHTS") {
    const Context c = parser.parse(pot, skip_in_weights_files, skipped);
    CHECK_FALSE(c.has_stored("EFILTER"));
    CHECK_FALSE(c.has_stored("WDBFILE"));
    CHECK(c.has_stored("WEIGHTS"));
    CHECK(c.get<bool>("ESHIFT_ATOM"));
    CHECK(described(skipped) ==
          std::vector<std::string>{pot + ":4 EFILTER 5 6",
                                   pot + ":6 WDBFILE 1 1"});
    const Context all = parser.parse(pot);
    CHECK(all.has_stored("EFILTER"));
    CHECK(all.has_stored("WDBFILE"));
  }
  SECTION("a file that includes it with CONFIG") {
    const std::string outer = dir.write("outer.cfg",
                                        "EFILTER -9 -7\n"           // 1
                                        "CONFIG " + pot + "\n"      // 2
                                        "WDBFILE 0.5 1 2\n");       // 3
    const Context c = parser.parse(outer, skip_in_weights_files, skipped);
    CHECK(c("EFILTER") == std::vector<std::string>{"-9", "-7"});
    CHECK(c("WDBFILE") == std::vector<std::string>{"0.5", "1", "2"});
    CHECK(c("WEIGHTS") == std::vector<std::string>{"0.25", "-0.5"});
    CHECK(described(skipped) ==
          std::vector<std::string>{pot + ":4 EFILTER 5 6",
                                   pot + ":6 WDBFILE 1 1"});
  }
  SECTION("ESHIFT beside WEIGHTS") {
    const std::string pot2 = dir.write("pot2.cfg",
                                       "WEIGHTS 1\n"           // 1
                                       "ESHIFT 0.5\n"          // 2
                                       "ESHIFT_ATOM true\n");  // 3
    const Context c = parser.parse(pot2, skip_in_weights_files, skipped);
    CHECK_FALSE(c.has_stored("ESHIFT_ATOM"));
    CHECK(described(skipped) ==
          std::vector<std::string>{pot2 + ":3 ESHIFT_ATOM true"});
  }
  SECTION("a skipped line is checked") {
    const std::string bad = dir.write("bad.cfg", "WEIGHTS 1\nEFILTER five 6\n");
    const std::string msg = runtime_error_of(
        [&] { return parser.parse(bad, skip_in_weights_files, skipped); },
        "parse " + bad);
    CHECK_THAT(msg, Catch::Matchers::Contains("EFILTER"));
  }
  SECTION("a block's body is not a key line of the file") {
    const std::string blk = dir.write("blk.cfg",
                                      "ATOM Ta\n"
                                      "INIT\n"
                                      "  WEIGHTS 1\n"
                                      "ENDINIT\n"
                                      "EFILTER 5 6\n");
    const Context c = parser.parse(blk, skip_in_weights_files, skipped);
    CHECK(c("EFILTER") == std::vector<std::string>{"5", "6"});
    CHECK(skipped.empty());
  }
}

namespace {

/* The keys that set up a run (its input files, verbosity, output,
 * pre-flight audit and health monitor), never a model, with a value each:
 * the key lines of a run setting a potential of an earlier version can
 * have (a 1.5.x best_pot.tadah has HPOTARGET, VERBOSE and NUMERIC, and
 * VALIDATION when its run had a validation file). */
const char *const kRunSettingLines =
    "VALIDATION config.val\n"
    "HPOTARGET config.hpo\n"
    "VERBOSE 2\n"
    "NUMERIC 12\n"
    "OUTFILE old.tadah\n"
    "AUDIT warn\n"
    "AUDIT_SAMPLE 2\n"
    "HEALTH_LOG full\n";
const char *const kRunSettingKeys[] = {"VALIDATION", "HPOTARGET", "VERBOSE",
                                       "NUMERIC",    "OUTFILE",   "AUDIT",
                                       "AUDIT_SAMPLE", "HEALTH_LOG"};

/* The Logger level at @p level while it lives, restored at its end. */
struct LoggerAt {
  LogLevel old_level;
  explicit LoggerAt(LogLevel level)
      : old_level(Logger::instance().getLogLevel()) {
    Logger::instance().setLogLevel(level);
  }
  ~LoggerAt() { Logger::instance().setLogLevel(old_level); }
  LoggerAt(const LoggerAt &) = delete;
  LoggerAt &operator=(const LoggerAt &) = delete;
};

/* A rule that selects no line: parse(file, rule, skipped) then lists only
 * the lines the parser itself does not read. */
bool select_none(const std::set<std::string> &, const std::string &) {
  return false;
}

} // namespace

// Pins: no parse stores a run setting of a potential: the lines of
// VALIDATION, HPOTARGET, VERBOSE, NUMERIC, OUTFILE, AUDIT, AUDIT_SAMPLE and
// HEALTH_LOG in a file whose own key lines have WEIGHTS (a potential of an
// earlier version: a 1.5.x best_pot.tadah has HPOTARGET, VERBOSE and
// NUMERIC, and VALIDATION when its run had a validation file) are checked
// and not stored, so its VERBOSE 2 leaves the Logger level where it was
// (WARNING) -- the level of the program that loads the potential (tadah
// predict, pair_style tadah, refit --edit, a retrain, tadah hpo -c) is its
// own:
//  - pot.cfg (ATOM, WEIGHTS and the eight lines): parse(file) stores ATOM
//    and WEIGHTS and none of the eight, and the level stays WARNING;
//  - the same lines without WEIGHTS (a training configuration): all eight
//    stored as written, and VERBOSE 2 sets the level to INFO;
//  - outer.cfg, no WEIGHTS, "VERBOSE 0", "CONFIG pot.cfg", "NUMERIC 9": its
//    own VERBOSE 0 and NUMERIC 9 are stored and set the level (ERROR), none
//    of pot.cfg's eight is (its VERBOSE 2 leaves the level at ERROR);
//  - parse(file, rule, skipped) with a rule that selects nothing lists the
//    eight lines of pot.cfg (line, key upper case, values as written: the
//    lines 3 to 10), and ATOM and WEIGHTS are stored;
//  - parse(lines) and parse(istream) of the potential's lines store none of
//    the eight either, and leave the level at WARNING.
// Before the fix: every parse stored the eight (a run setting of a
// potential was read as the setting of the program that loaded it) and
// the potential's VERBOSE 2 set the level to INFO: the has_stored, level
// and skipped checks fail.
TEST_CASE("Config parser: a potential's run setting lines are not stored, "
          "and its VERBOSE sets no log level",
          "[config][skip_key_lines][potential_run_settings]") {
  RefuseScratch dir;
  const std::string pot = dir.write("pot.cfg", std::string("ATOM Ta\n"
                                                           "WEIGHTS 0.25 -0.5\n") +
                                                   kRunSettingLines);
  const CoreUserConfigParser parser;

  SECTION("a potential: none of the eight stored, no level set") {
    LoggerAt at(LogLevel::WARNING);
    const Context c = parser.parse(pot);
    CHECK(Logger::instance().getLogLevel() == LogLevel::WARNING);
    CHECK(c.has_stored("ATOM"));
    CHECK(c.has_stored("WEIGHTS"));
    for (const char *key : kRunSettingKeys) {
      INFO("key " << key);
      CHECK_FALSE(c.has_stored(key));
    }
  }
  SECTION("a training configuration: all eight stored, VERBOSE sets INFO") {
    LoggerAt at(LogLevel::WARNING);
    const std::string cfg =
        dir.write("train.cfg", std::string("ATOM Ta\n") + kRunSettingLines);
    const Context c = parser.parse(cfg);
    CHECK(Logger::instance().getLogLevel() == LogLevel::INFO);
    for (const char *key : kRunSettingKeys) {
      INFO("key " << key);
      CHECK(c.has_stored(key));
    }
    CHECK(c("NUMERIC") == std::vector<std::string>{"12"});
    CHECK(c("OUTFILE") == std::vector<std::string>{"old.tadah"});
  }
  SECTION("a file that includes the potential with CONFIG") {
    LoggerAt at(LogLevel::WARNING);
    const std::string outer = dir.write("outer.cfg", "VERBOSE 0\n"
                                                     "CONFIG " + pot + "\n"
                                                     "NUMERIC 9\n");
    const Context c = parser.parse(outer);
    CHECK(Logger::instance().getLogLevel() == LogLevel::ERROR);
    CHECK(c("VERBOSE") == std::vector<std::string>{"0"});
    CHECK(c("NUMERIC") == std::vector<std::string>{"9"});
    CHECK(c.has_stored("WEIGHTS"));
    for (const char *key : {"VALIDATION", "HPOTARGET", "OUTFILE", "AUDIT",
                            "AUDIT_SAMPLE", "HEALTH_LOG"}) {
      INFO("key " << key);
      CHECK_FALSE(c.has_stored(key));
    }
  }
  SECTION("listed as lines not read") {
    LoggerAt at(LogLevel::WARNING);
    std::vector<SkippedKeyLine> skipped;
    const Context c = parser.parse(pot, select_none, skipped);
    CHECK(c.has_stored("ATOM"));
    CHECK(c.has_stored("WEIGHTS"));
    CHECK(described(skipped) ==
          std::vector<std::string>{
              pot + ":3 VALIDATION config.val", pot + ":4 HPOTARGET config.hpo",
              pot + ":5 VERBOSE 2", pot + ":6 NUMERIC 12",
              pot + ":7 OUTFILE old.tadah", pot + ":8 AUDIT warn",
              pot + ":9 AUDIT_SAMPLE 2", pot + ":10 HEALTH_LOG full"});
    CHECK(Logger::instance().getLogLevel() == LogLevel::WARNING);
  }
  SECTION("the lines and stream overloads") {
    LoggerAt at(LogLevel::WARNING);
    std::vector<std::string> lines = {"ATOM Ta", "WEIGHTS 0.25 -0.5"};
    {
      std::istringstream in(kRunSettingLines);
      for (std::string l; std::getline(in, l);) lines.push_back(l);
    }
    const Context from_lines = parser.parse(lines);
    std::ostringstream text;
    for (const std::string &l : lines) text << l << "\n";
    std::istringstream stream(text.str());
    const Context from_stream = parser.parse(stream);
    CHECK(Logger::instance().getLogLevel() == LogLevel::WARNING);
    for (const char *key : kRunSettingKeys) {
      INFO("key " << key);
      CHECK_FALSE(from_lines.has_stored(key));
      CHECK_FALSE(from_stream.has_stored(key));
    }
    CHECK(from_lines.has_stored("WEIGHTS"));
    CHECK(from_stream.has_stored("WEIGHTS"));
  }
}

// Pins: CHECK, the check instruction of tadah hpo (`tadah hpo --check`), is a
// run setting as HPOTARGET is: run_setting_keys() lists it, so no parse
// stores the CHECK line of a potential -- `tadah hpo -c` of a potential
// with a CHECK true line is a run, not a check -- and the line is still
// checked as any line (CHECK maybe is refused):
//  - pot.cfg (ATOM, WEIGHTS, CHECK true): parse(file) stores ATOM and
//    WEIGHTS and no CHECK; parse(file, rule, skipped) with a rule that
//    selects nothing lists "pot.cfg:3 CHECK true";
//  - the same lines without WEIGHTS (a training configuration) store CHECK
//    true;
//  - a potential with CHECK maybe is refused, naming the key.
// Before the fix: run_setting_keys() has no CHECK, and the potential's
// CHECK true is stored and not listed: the list, has_stored and skipped
// checks fail.
TEST_CASE("Config parser: a potential's CHECK line is not stored, a training "
          "configuration's is",
          "[config][skip_key_lines][potential_run_settings][check]") {
  const std::vector<std::string> &settings = run_setting_keys();
  CHECK(std::find(settings.begin(), settings.end(), "CHECK") !=
        settings.end());

  RefuseScratch dir;
  const CoreUserConfigParser parser;
  const std::string pot =
      dir.write("pot.cfg", "ATOM Ta\nWEIGHTS 0.25 -0.5\nCHECK true\n");
  {
    const Context c = parser.parse(pot);
    CHECK(c.has_stored("ATOM"));
    CHECK(c.has_stored("WEIGHTS"));
    CHECK_FALSE(c.has_stored("CHECK"));
  }
  {
    std::vector<SkippedKeyLine> skipped;
    const Context c = parser.parse(pot, select_none, skipped);
    CHECK_FALSE(c.has_stored("CHECK"));
    CHECK(described(skipped) == std::vector<std::string>{pot + ":3 CHECK true"});
  }
  {
    const std::string cfg = dir.write("train.cfg", "ATOM Ta\nCHECK true\n");
    const Context c = parser.parse(cfg);
    REQUIRE(c.has_stored("CHECK"));
    CHECK(c("CHECK") == std::vector<std::string>{"true"});
  }
  {
    const std::string bad =
        dir.write("bad.cfg", "ATOM Ta\nWEIGHTS 0.25 -0.5\nCHECK maybe\n");
    std::string what;
    try {
      (void)parser.parse(bad);
    } catch (const std::exception &e) {
      what = e.what();
    }
    INFO("thrown: " << what);
    CHECK(what.find("CHECK") != std::string::npos);
  }
}

// Pins: a key that only some commands read -- an option of their command
// line (ANALYTICS of tadah predict, INDEX of tadah data print and others) or
// a key of their configuration file (REFIT_MODE and the OPTIMIZER block of
// tadah refit) -- is no key of a potential. A potential (a file whose own
// lines give WEIGHTS) of an earlier version may hold such a line, as tadah
// train wrote what its configuration held: every parse checks the line as
// any line and does not store it, and gives one notice per key naming its
// line as written, the line number, the file and the commands that read the
// key ("Key ANALYTICS is ignored: ANALYTICS true on line 3 of pot.cfg ...
// tadah predict ..."); the potential's own keys are stored. A file without
// WEIGHTS stores the same lines (the command that reads a configuration
// file refuses the keys of other commands, TaskFactory), and a potential's
// line with a value its key does not take is refused as any line.
// Before the fix: every parse stored the four keys of the potential and
// gave no notice.
TEST_CASE("Config parser: a potential's line of a key of another command is "
          "not stored, with a notice",
          "[config][potential_run_settings][command_keys]") {
  RefuseScratch dir;
  const CoreUserConfigParser parser;
  const std::string pot = dir.write(
      "pot.cfg", "ATOM Ta\nWEIGHTS 0.25 -0.5\nANALYTICS true\n"
                 "REFIT_MODE dense\nINDEX 1,3\n"
                 "OPTIMIZER\nALGO LN_SBPLX\nENDOPTIMIZER\n");
  {
    IgnoredKeyNoticeCapture cap;
    const Context c = parser.parse(pot);
    CHECK(c.has_stored("ATOM"));
    CHECK(c.has_stored("WEIGHTS"));
    for (const char *key : {"ANALYTICS", "REFIT_MODE", "INDEX", "OPTIMIZER"}) {
      INFO("key " << key);
      CHECK_FALSE(c.has_stored(key));
    }
    std::string notices;
    for (const std::string &n : cap.notices()) notices += n + "\n";
    INFO(notices);
    CHECK(cap.notices().size() == 4);
    for (const auto &[line, owner] :
         {std::pair<std::string, std::string>{"ANALYTICS true on line 3",
                                              "tadah predict"},
          {"REFIT_MODE dense on line 4", "tadah refit"},
          {"INDEX 1,3 on line 5", "tadah data print"},
          {"OPTIMIZER on line 6", "tadah refit"}}) {
      INFO(line);
      bool named = false;
      for (const std::string &n : cap.notices())
        if (n.find(line + " of " + pot) != std::string::npos &&
            n.find(owner) != std::string::npos)
          named = true;
      CHECK(named);
    }
  }
  {
    const std::string cfg = dir.write(
        "train.cfg", "ATOM Ta\nANALYTICS true\nREFIT_MODE dense\nINDEX 1,3\n");
    const Context c = parser.parse(cfg);
    for (const char *key : {"ANALYTICS", "REFIT_MODE", "INDEX"}) {
      INFO("key " << key);
      CHECK(c.has_stored(key));
    }
  }
  {
    const std::string bad =
        dir.write("bad.cfg", "ATOM Ta\nWEIGHTS 0.25 -0.5\nANALYTICS maybe\n");
    std::string what;
    try {
      (void)parser.parse(bad);
    } catch (const std::exception &e) {
      what = e.what();
    }
    INFO("thrown: " << what);
    CHECK(what.find("ANALYTICS") != std::string::npos);
    CHECK(names_line(what, 3));
  }
}

// ---------------------------------------------------------------------------
// A configuration is the concatenation of its lines: a CONFIG line reads the
// lines of the file it names at its place, and a nested CONFIG line the same
// way. A key of one value (|value_N[0]| 1 in the blueprint) is set by one
// line of the configuration: a second line, in the same file or in another
// file of the configuration, in either order, is refused naming both lines,
// each with its file and, for a line of an included file, the CONFIG lines
// that include it. A key of several values takes the values of all its
// lines in line order, up to the number it takes ("Too many values" at the
// line that exceeds it, in either order). A line a parse does not read (a
// potential's run setting, a line the caller's rule skips) sets nothing.
// ---------------------------------------------------------------------------
namespace {

/* The rule the refusal of a second line of a key of one value states. */
const std::string kSetOnce =
    "a key of one value is set once in a configuration file and the files "
    "it includes with CONFIG";

/* What a refusal of a line of a key adds when the key's lines are a
 * potential's and another file's: how a potential's value is changed. */
const std::string kCopyAdvice =
    "; a potential's value is changed by editing its line in a copy of the "
    "potential (the Training page of the documentation, Retraining from a "
    "potential)";

} // namespace

// Pins: a list key (DBFILE) and a key of several values (ATOM) set above a
// CONFIG line, in the included file and below the CONFIG line take the
// values of every line, in line order: outer.cfg "DBFILE a.tadah", "ATOM
// Ta", "CONFIG inc.cfg", "DBFILE c.tadah", inc.cfg "DBFILE b.tadah", "ATOM
// Nb", give DBFILE a.tadah b.tadah c.tadah and ATOM Ta Nb; the stream and
// lines overloads read the CONFIG line the same way.
// Before the fix: the include's lines replaced every line of their keys
// above the CONFIG line (Context::merge with overwrite): DBFILE b.tadah
// c.tadah and ATOM Nb, a.tadah and Ta dropped without a message.
TEST_CASE("Config parser: a list key above and below a CONFIG include adds "
          "up in line order",
          "[config][config_include][concatenation]") {
  RefuseScratch dir;
  const std::string inc =
      dir.write("list_inc.cfg", "DBFILE b.tadah\nATOM Nb\n");
  const std::string body = "DBFILE a.tadah\nATOM Ta\nCONFIG " + inc +
                           "\nDBFILE c.tadah\n";
  const std::string outer = dir.write("list_outer.cfg", body);
  const std::vector<std::string> dbfiles{"a.tadah", "b.tadah", "c.tadah"};
  const std::vector<std::string> atoms{"Ta", "Nb"};
  {
    const Context c = CoreUserConfigParser().parse(outer);
    CHECK(c("DBFILE") == dbfiles);
    CHECK(c("ATOM") == atoms);
  }
  {
    std::istringstream in(body);
    const Context c = CoreUserConfigParser().parse(in);
    CHECK(c("DBFILE") == dbfiles);
    CHECK(c("ATOM") == atoms);
  }
  {
    const Context c = CoreUserConfigParser().parse(std::vector<std::string>{
        "DBFILE a.tadah", "ATOM Ta", "CONFIG " + inc, "DBFILE c.tadah"});
    CHECK(c("DBFILE") == dbfiles);
    CHECK(c("ATOM") == atoms);
  }
}

// Pins: a key of one value that a file and a file it includes with CONFIG
// both set is refused, in either order, naming the key, both lines with
// their files and the CONFIG line of the included one:
//  - above: outer.cfg "BIAS true", "CONFIG inc.cfg"; inc.cfg "# shared",
//    "BIAS false": "Key BIAS is set on line 1 of <outer> and on line 2 of
//    <inc> (CONFIG on line 2 of <outer>): a key of one value is set once in
//    a configuration file and the files it includes with CONFIG";
//  - below: "CONFIG inc.cfg", "bias true": "Key BIAS is set on line 2 of
//    <inc> (CONFIG on line 1 of <outer>) and on line 2 of <outer>: ...";
//  - a stream: "BIAS true", "CONFIG inc.cfg": "Key BIAS is set on line 1
//    and on line 2 of <inc> (CONFIG on line 2): ...";
//  - a potential the file includes (ATOM, WEIGHTS, RCUT2B 5.0, BIAS
//    false): "RCUT2B 4.0" above its CONFIG line gives RCUT2B 4.0 5.0 and
//    below it 5.0 4.0, while "BIAS true" above it is refused naming the
//    potential's line 4, with the way to change a potential's value ("...
//    with CONFIG; a potential's value is changed by editing its line in a
//    copy of the potential (the Training page of the documentation,
//    Retraining from a potential)");
//  - a file included twice: "CONFIG inc.cfg" on lines 1 and 2: "Key BIAS is
//    set on line 2 of <inc> (CONFIG on line 1 of <outer>) and on line 2 of
//    <inc> (CONFIG on line 2 of <outer>): ...".
// Before the fix: no section threw. The include's BIAS replaced the line
// above it (BIAS false; the potential's BIAS false likewise, and its
// RCUT2B the RCUT2B 4.0 above it: RCUT2B 5.0), a line below the include
// replaced the include's (BIAS true), the second include replaced the
// first, and a stream behaved as a file. Until the refusal gave the way to
// change a potential's value, the potential's case ended at the rule.
TEST_CASE("Config parser: a key of one value set in a file and in a file it "
          "includes is refused naming both lines",
          "[config][config_include][concatenation]") {
  RefuseScratch dir;
  const std::string inc = dir.write("once_inc.cfg", "# shared\nBIAS false\n");
  SECTION("above the CONFIG line") {
    const std::string outer =
        dir.write("once_above.cfg", "BIAS true\nCONFIG " + inc + "\n");
    CaptureLog log;
    CHECK(runtime_error_of([&] { return CoreUserConfigParser().parse(outer); },
                           "parse " + outer) ==
          "Key BIAS is set on line 1 of " + outer + " and on line 2 of " +
              inc + " (CONFIG on line 2 of " + outer + "): " + kSetOnce);
    CHECK(log.text().empty());
  }
  SECTION("below the CONFIG line") {
    const std::string outer =
        dir.write("once_below.cfg", "CONFIG " + inc + "\nbias true\n");
    CHECK(runtime_error_of([&] { return CoreUserConfigParser().parse(outer); },
                           "parse " + outer) ==
          "Key BIAS is set on line 2 of " + inc + " (CONFIG on line 1 of " +
              outer + ") and on line 2 of " + outer + ": " + kSetOnce);
  }
  SECTION("a stream") {
    std::istringstream in("BIAS true\nCONFIG " + inc + "\n");
    CHECK(runtime_error_of([&] { return CoreUserConfigParser().parse(in); },
                           "parse(stream)") ==
          "Key BIAS is set on line 1 and on line 2 of " + inc +
              " (CONFIG on line 2): " + kSetOnce);
  }
  SECTION("a potential the file includes") {
    const std::string pot =
        dir.write("once_pot.tadah",
                  "ATOM Ta\nWEIGHTS 0.25 -0.5\nRCUT2B 5.0\nBIAS false\n");
    const std::string above = dir.write(
        "pot_above.cfg", "RCUT2B 4.0\nCONFIG " + pot + "\n");
    const std::string below = dir.write(
        "pot_below.cfg", "CONFIG " + pot + "\nRCUT2B 4.0\n");
    CHECK(CoreUserConfigParser().parse(above)("RCUT2B") ==
          std::vector<std::string>{"4.0", "5.0"});
    CHECK(CoreUserConfigParser().parse(below)("RCUT2B") ==
          std::vector<std::string>{"5.0", "4.0"});
    const std::string bias =
        dir.write("pot_bias.cfg", "BIAS true\nCONFIG " + pot + "\n");
    CHECK(runtime_error_of([&] { return CoreUserConfigParser().parse(bias); },
                           "parse " + bias) ==
          "Key BIAS is set on line 1 of " + bias + " and on line 4 of " +
              pot + " (CONFIG on line 2 of " + bias + "): " + kSetOnce +
              kCopyAdvice);
  }
  SECTION("a file included twice") {
    const std::string outer = dir.write(
        "twice_inc.cfg", "CONFIG " + inc + "\nCONFIG " + inc + "\n");
    CHECK(runtime_error_of([&] { return CoreUserConfigParser().parse(outer); },
                           "parse " + outer) ==
          "Key BIAS is set on line 2 of " + inc + " (CONFIG on line 1 of " +
              outer + ") and on line 2 of " + inc + " (CONFIG on line 2 of " +
              outer + "): " + kSetOnce);
  }
}

// Pins: a key of one value set by two lines of one file is refused naming
// both lines, whatever the values and the letter case: "ESIGMA 1", "#
// again", "ESIGMA 2" gives "Key ESIGMA is set on line 1 of <f> and on line
// 3 of <f>: a key of one value is set once in a configuration file and the
// files it includes with CONFIG"; "bias true", "BIAS true" (the same
// value) "Key BIAS is set on line 1 of <f> and on line 2 of <f>: ..."; the
// lines overload "ESIGMA 1", "ESIGMA 2" "Key ESIGMA is set on line 1 and on
// line 2: ...". A key of several values set by two lines is read (ATOM Ta,
// ATOM Nb: ATOM Ta Nb).
// Before the fix: the second line replaced the first without a message
// (ESIGMA 2, BIAS true; Context::add overwrites a key of one value).
TEST_CASE("Config parser: a key of one value set twice in one file is "
          "refused naming both lines",
          "[config][concatenation]") {
  RefuseScratch dir;
  const std::string f = dir.write("esigma_twice.cfg",
                                  "ESIGMA 1\n# again\nESIGMA 2\n");
  CaptureLog log;
  CHECK(runtime_error_of([&] { return CoreUserConfigParser().parse(f); },
                         "parse " + f) ==
        "Key ESIGMA is set on line 1 of " + f + " and on line 3 of " + f +
            ": " + kSetOnce);
  const std::string same = dir.write("bias_twice.cfg", "bias true\nBIAS true\n");
  CHECK(runtime_error_of([&] { return CoreUserConfigParser().parse(same); },
                         "parse " + same) ==
        "Key BIAS is set on line 1 of " + same + " and on line 2 of " + same +
            ": " + kSetOnce);
  CHECK(runtime_error_of(
            [&] {
              return CoreUserConfigParser().parse(
                  std::vector<std::string>{"ESIGMA 1", "ESIGMA 2"});
            },
            "parse(lines)") ==
        "Key ESIGMA is set on line 1 and on line 2: " + kSetOnce);
  CHECK(log.text().empty());
  const Context c = CoreUserConfigParser().parse(
      std::vector<std::string>{"ATOM Ta", "ATOM Nb"});
  CHECK(c("ATOM") == std::vector<std::string>{"Ta", "Nb"});
}

// Pins: a key of several values set above a CONFIG line and in the file
// it includes adds up as below it, so more values than the key takes are
// refused in either order at the line that exceeds the number, with the
// CONFIG line of an included line and the lines of the key before it:
// outer.cfg "OALGO 1 1e-12", "CONFIG inc.cfg", inc.cfg "OALGO 1 1e-13",
// gives "Too many values for key OALGO on line 1 of <inc> (CONFIG on line 2
// of <outer>): OALGO takes at most 2, and its lines read so far give 4,
// with line 1 of <outer> before it"; "CONFIG inc.cfg", "OALGO 1 1e-12"
// gives "Too many values for key OALGO on line 2 of <outer>: OALGO takes at
// most 2, and its lines read so far give 4, with line 1 of <inc> (CONFIG on
// line 1 of <outer>) before it".
// Before the fix: above the CONFIG line the include's OALGO replaced the
// line (OALGO 1 1e-13, no exception); below it the refusal was the same.
// Until the refusal named the lines of the key before it, both ended "...
// read so far give 4", without the line of the other file.
TEST_CASE("Config parser: a key of several values adds up across a CONFIG "
          "include, up to the number it takes, in either order",
          "[config][config_include][concatenation]") {
  RefuseScratch dir;
  const std::string inc = dir.write("oalgo_inc.cfg", "OALGO 1 1e-13\n");
  const std::string above =
      dir.write("oalgo_above.cfg", "OALGO 1 1e-12\nCONFIG " + inc + "\n");
  CHECK(runtime_error_of([&] { return CoreUserConfigParser().parse(above); },
                         "parse " + above) ==
        "Too many values for key OALGO on line 1 of " + inc +
            " (CONFIG on line 2 of " + above +
            "): OALGO takes at most 2, and its lines read so far give 4, "
            "with line 1 of " + above + " before it");
  const std::string below =
      dir.write("oalgo_below.cfg", "CONFIG " + inc + "\nOALGO 1 1e-12\n");
  CHECK(runtime_error_of([&] { return CoreUserConfigParser().parse(below); },
                         "parse " + below) ==
        "Too many values for key OALGO on line 2 of " + below +
            ": OALGO takes at most 2, and its lines read so far give 4, "
            "with line 1 of " + inc + " (CONFIG on line 1 of " + below +
            ") before it");
}

// Pins: a file and a file it includes that set different keys give every
// key as its line sets it, with no exception, no Logger line and no
// notice: outer.cfg "BIAS true", "CONFIG inc.cfg", "OALGO 3"; inc.cfg
// "LAMBDA 0", "RCUT2B 5.0".
// Before the fix: passes as well (a pin: the rule of a key set twice
// leaves the keys set once as they were).
TEST_CASE("Config parser: a file and a file it includes that set different "
          "keys give both, with no message",
          "[config][config_include][concatenation]") {
  RefuseScratch dir;
  const std::string inc = dir.write("apart_inc.cfg", "LAMBDA 0\nRCUT2B 5.0\n");
  const std::string outer = dir.write(
      "apart_outer.cfg", "BIAS true\nCONFIG " + inc + "\nOALGO 3\n");
  CaptureLog log;
  IgnoredKeyNoticeCapture notices;
  Context c;
  REQUIRE_NOTHROW(c = CoreUserConfigParser().parse(outer));
  CHECK(c("BIAS") == std::vector<std::string>{"true"});
  CHECK(c("LAMBDA") == std::vector<std::string>{"0"});
  CHECK(c("RCUT2B") == std::vector<std::string>{"5.0"});
  CHECK(c("OALGO") == std::vector<std::string>{"3"});
  CHECK(log.text().empty());
  CHECK(notices.notices().empty());
}

// Pins: the lines of a potential that a parse does not read set nothing, so
// a file that includes the potential with CONFIG gives its own lines of
// those keys above or below the CONFIG line: pot.cfg (ATOM, WEIGHTS,
// EFILTER 5 6, ESHIFT 0.5, ESHIFT_ATOM true, VERBOSE 2), read with a rule
// that skips EFILTER, and ESHIFT_ATOM beside ESHIFT, in a file with WEIGHTS:
//  - outer.cfg "EFILTER -9 -2", "VERBOSE 0", "CONFIG pot.cfg", "ESHIFT_ATOM
//    false" gives EFILTER -9 -2, VERBOSE 0 and ESHIFT_ATOM false, and the
//    skipped lines pot.cfg:3 EFILTER 5 6, pot.cfg:5 ESHIFT_ATOM true and
//    pot.cfg:6 VERBOSE 2 (a run setting of a potential, never stored);
//  - "CONFIG pot.cfg", "EFILTER -9 -2" gives EFILTER -9 -2.
// Before the fix: passes as well (a pin: a skipped line was never stored,
// so the merge of the include had nothing of these keys to replace).
TEST_CASE("Config parser: a potential's lines a parse does not read do not "
          "clash with the lines of their keys around its CONFIG line",
          "[config][config_include][concatenation][skip_key_lines]") {
  RefuseScratch dir;
  const std::string pot = dir.write("skip_pot.cfg",
                                    "ATOM Ta\n"            // 1
                                    "WEIGHTS 0.25 -0.5\n"  // 2
                                    "EFILTER 5 6\n"        // 3
                                    "ESHIFT 0.5\n"         // 4
                                    "ESHIFT_ATOM true\n"   // 5
                                    "VERBOSE 2\n");        // 6
  const CoreUserConfigParser parser;
  LoggerAt at(LogLevel::WARNING);
  {
    const std::string outer = dir.write("skip_outer.cfg",
                                        "EFILTER -9 -2\nVERBOSE 0\nCONFIG " +
                                            pot + "\nESHIFT_ATOM false\n");
    std::vector<SkippedKeyLine> skipped;
    Context c;
    REQUIRE_NOTHROW(c = parser.parse(outer, skip_in_weights_files, skipped));
    CHECK(c("EFILTER") == std::vector<std::string>{"-9", "-2"});
    CHECK(c("VERBOSE") == std::vector<std::string>{"0"});
    CHECK(c("ESHIFT_ATOM") == std::vector<std::string>{"false"});
    CHECK(described(skipped) ==
          std::vector<std::string>{pot + ":3 EFILTER 5 6",
                                   pot + ":5 ESHIFT_ATOM true",
                                   pot + ":6 VERBOSE 2"});
  }
  {
    const std::string outer =
        dir.write("skip_below.cfg", "CONFIG " + pot + "\nEFILTER -9 -2\n");
    std::vector<SkippedKeyLine> skipped;
    Context c;
    REQUIRE_NOTHROW(c = parser.parse(outer, skip_in_weights_files, skipped));
    CHECK(c("EFILTER") == std::vector<std::string>{"-9", "-2"});
  }
}

// Pins: in a nested include the refusal of a key of one value names the
// file of each line and the CONFIG lines that include it, innermost first:
// top.cfg "BIAS true", "CONFIG mid.cfg"; mid.cfg "ATOM Ta", "CONFIG
// deep.cfg"; deep.cfg "WATOM 73", "BIAS false": "Key BIAS is set on line 1
// of <top> and on line 2 of <deep> (CONFIG on line 2 of <mid>, which CONFIG
// on line 2 of <top> includes): ..."; with top.cfg "CONFIG mid.cfg", "BIAS
// true": "Key BIAS is set on line 2 of <deep> (CONFIG on line 2 of <mid>,
// which CONFIG on line 1 of <top> includes) and on line 2 of <top>: ...".
// Before the fix: neither threw (BIAS false above the CONFIG line, BIAS
// true below it).
TEST_CASE("Config parser: a key of one value set in a nested include is "
          "refused naming its file and the CONFIG lines that include it",
          "[config][config_include][concatenation]") {
  RefuseScratch dir;
  const std::string deep = dir.write("deep.cfg", "WATOM 73\nBIAS false\n");
  const std::string mid = dir.write("mid.cfg", "ATOM Ta\nCONFIG " + deep + "\n");
  const std::string top =
      dir.write("top.cfg", "BIAS true\nCONFIG " + mid + "\n");
  CHECK(runtime_error_of([&] { return CoreUserConfigParser().parse(top); },
                         "parse " + top) ==
        "Key BIAS is set on line 1 of " + top + " and on line 2 of " + deep +
            " (CONFIG on line 2 of " + mid + ", which CONFIG on line 2 of " +
            top + " includes): " + kSetOnce);
  const std::string after =
      dir.write("top_after.cfg", "CONFIG " + mid + "\nBIAS true\n");
  CHECK(runtime_error_of([&] { return CoreUserConfigParser().parse(after); },
                         "parse " + after) ==
        "Key BIAS is set on line 2 of " + deep + " (CONFIG on line 2 of " +
            mid + ", which CONFIG on line 1 of " + after +
            " includes) and on line 2 of " + after + ": " + kSetOnce);
}

// Pins: a CONFIG line that names a file whose lines are being read, the file
// of the parse among them, is refused as a circular include before a line
// of that file is read again, so a key of one value, or of a bounded number
// of values, above the CONFIG line is not read twice. A file is the same
// file whatever its path ("<dir>/./same.cfg" names <dir>/same.cfg):
//  - self.cfg "BIAS true", "CONFIG self.cfg": "Circular dependency detected
//    between CONFIG files: <self> -> <self> (CONFIG on line 2 of <self>)",
//    through parse(file) and parse(file, rule, skipped);
//  - oalgo_self.cfg "OALGO 1 1e-12", "CONFIG oalgo_self.cfg": "... <o> ->
//    <o> (CONFIG on line 2 of <o>)";
//  - cycle_a.cfg "BIAS true", "CONFIG cycle_b.cfg"; cycle_b.cfg "LAMBDA 0",
//    "CONFIG cycle_a.cfg": "... <a> -> <b> -> <a> (CONFIG on line 2 of
//    <b>)";
//  - same.cfg "BIAS true", "CONFIG <dir>/./same.cfg": "... <same> ->
//    <dir>/./same.cfg (CONFIG on line 2 of <same>)"; the parse of
//    "<dir>/./same_top.cfg" ("BIAS true", "CONFIG <dir>/same_top.cfg"):
//    "... <dir>/./same_top.cfg -> <same_top> (CONFIG on line 2 of
//    <dir>/./same_top.cfg)".
// Before the fix: the file of the parse was not among the files being read
// and a path was compared by its text, so the lines of the file were read a
// second time first, and the second line of a key was refused instead of
// the circle: "Key BIAS is set on line 1 of <self> and on line 1 of <self>
// (CONFIG on line 2 of <self>): a key of one value is set once in a
// configuration file and the files it includes with CONFIG"; "Too many
// values for key OALGO on line 1 of <o> (CONFIG on line 2 of <o>): OALGO
// takes at most 2, and its lines read so far give 4"; "Key BIAS is set on
// line 1 of <a> and on line 1 of <a> (CONFIG on line 2 of <b>, which CONFIG
// on line 2 of <a> includes): ..."; "Key BIAS is set on line 1 of <same>
// and on line 1 of <dir>/./same.cfg (CONFIG on line 2 of <same>): ...".
TEST_CASE("Config parser: a file that includes itself is refused as a "
          "circular include before its lines are read again",
          "[config][config_include][concatenation]") {
  RefuseScratch dir;
  const auto circle = [](const std::vector<std::string> &chain,
                         const std::string &config_line) {
    std::string out = "Circular dependency detected between CONFIG files: ";
    for (std::size_t k = 0; k < chain.size(); ++k)
      out += (k ? " -> " : "") + chain[k];
    return out + " (" + config_line + ")";
  };
  SECTION("a key of one value above the CONFIG line") {
    const std::string self = dir.write(
        "self.cfg", "BIAS true\nCONFIG " + dir.path("self.cfg") + "\n");
    const std::string expected =
        circle({self, self}, "CONFIG on line 2 of " + self);
    CHECK(runtime_error_of([&] { return CoreUserConfigParser().parse(self); },
                           "parse " + self) == expected);
    std::vector<SkippedKeyLine> skipped;
    CHECK(runtime_error_of(
              [&] {
                return CoreUserConfigParser().parse(self, skip_in_weights_files,
                                                    skipped);
              },
              "parse " + self + " with a rule") == expected);
  }
  SECTION("a key of a bounded number of values above the CONFIG line") {
    const std::string self =
        dir.write("oalgo_self.cfg",
                  "OALGO 1 1e-12\nCONFIG " + dir.path("oalgo_self.cfg") + "\n");
    CHECK(runtime_error_of([&] { return CoreUserConfigParser().parse(self); },
                           "parse " + self) ==
          circle({self, self}, "CONFIG on line 2 of " + self));
  }
  SECTION("a circle of two files") {
    const std::string b = dir.write(
        "cycle_b.cfg", "LAMBDA 0\nCONFIG " + dir.path("cycle_a.cfg") + "\n");
    const std::string a =
        dir.write("cycle_a.cfg", "BIAS true\nCONFIG " + b + "\n");
    CHECK(runtime_error_of([&] { return CoreUserConfigParser().parse(a); },
                           "parse " + a) ==
          circle({a, b, a}, "CONFIG on line 2 of " + b));
  }
  SECTION("the same file by another path") {
    const std::string other = dir.path("./same.cfg");
    REQUIRE(other != dir.path("same.cfg"));
    const std::string same =
        dir.write("same.cfg", "BIAS true\nCONFIG " + other + "\n");
    CHECK(runtime_error_of([&] { return CoreUserConfigParser().parse(same); },
                           "parse " + same) ==
          circle({same, other}, "CONFIG on line 2 of " + same));
    const std::string top = dir.write(
        "same_top.cfg", "BIAS true\nCONFIG " + dir.path("same_top.cfg") + "\n");
    const std::string top_other = dir.path("./same_top.cfg");
    CHECK(runtime_error_of(
              [&] { return CoreUserConfigParser().parse(top_other); },
              "parse " + top_other) ==
          circle({top_other, top}, "CONFIG on line 2 of " + top_other));
  }
}

/* The lines that gave each key its values: the parse records them on the
 * Context it returns (Context::key_lines()), in line order, each with where
 * it is (as the errors of a line name it), whether it is a line of a
 * potential, a file whose own key lines have WEIGHTS, the file it is in (as
 * the parse names it: the file given to the parse, or the path of the
 * CONFIG line that includes it; empty for lines of no file), the number of
 * values it gave and the number of the line in its file (KeyLine::line, the
 * number "where" names first). A configuration that includes a potential
 * with CONFIG
 * (pot.tadah: ATOM, WEIGHTS 0.25 -0.5, RCUT2B 5.0 on line 3) and gives
 * RCUT2B 4.0 above or below the CONFIG line:
 *  - above.cfg "RCUT2B 4.0", "CONFIG pot.tadah", "DBFILE a.tadah":
 *    RCUT2B {line 1 of above.cfg, not a potential's; line 3 of pot.tadah
 *    (CONFIG on line 2 of above.cfg), a potential's}; WEIGHTS {line 2 of
 *    pot.tadah (CONFIG on line 2 of above.cfg), a potential's}; DBFILE
 *    {line 3 of above.cfg};
 *  - below.cfg "CONFIG pot.tadah", "RCUT2B 4.0": the potential's line first;
 *  - the potential read alone: RCUT2B {line 3 of pot.tadah, a potential's};
 *  - a nested include, top.cfg "CONFIG mid.cfg", mid.cfg "LAMBDA 0",
 *    "CONFIG pot.tadah": LAMBDA {line 1 of mid.cfg (CONFIG on line 1 of
 *    top.cfg)}, RCUT2B {line 3 of pot.tadah (CONFIG on line 2 of mid.cfg,
 *    which CONFIG on line 1 of top.cfg includes), a potential's};
 *  - lines of no file "ATOM Ta", "RCUT2B 5.0", "WEIGHTS 1": RCUT2B {line 2,
 *    a potential's};
 *  - a line the parse does not store (the potential's EFILTER under the
 *    rule of its potential lines) sets no line;
 *  - a key of two lines of one file, "RCUT2B 5.0" and "RCUT2B 6.0 7.0" of
 *    a potential: {line 3, 1 value} and {line 5, 2 values}, both of that
 *    file;
 *  - the line of an included file has its number in that file (RCUT2B,
 *    line 3 of pot.tadah), not the number of the CONFIG line;
 *  - the number of a line among the lines of its file that hold more than
 *    blanks and a comment (KeyLine::ordinal): in the files above, without
 *    such lines, the line's number; in a potential of "ATOM Ta", "# a
 *    comment", "WEIGHTS 1", a blank line, "RCUT2B 5.0", "  # an indented
 *    comment", "RCUT2B 6.0 # an inline comment", "LAMBDA 0", "RCUT2B 7.0":
 *    RCUT2B {line 5, ordinal 3}, {line 7, ordinal 4}, {line 9, ordinal 6},
 *    so that the first two follow one another and the third does not; and
 *    the same for lines of no file.
 * Before the fix: does not compile (KeyLine has no line: a line was
 * recorded by where it is, whether it is a potential's, its file and the
 * number of its values, so that two lines of one file could not be told to
 * be one after the other).
 * Before the fix of the ordinal: does not compile (KeyLine has no ordinal:
 * a comment or a blank line between two lines of a key set them apart as
 * a line of another key does). */
TEST_CASE("Config parser: the Context records the lines that gave each key "
          "its values and whether each is a potential's line",
          "[config][config_include][key_lines]") {
  RefuseScratch dir;
  const std::string pot =
      dir.write("lines_pot.tadah", "ATOM Ta\nWEIGHTS 0.25 -0.5\nRCUT2B 5.0\n"
                                   "EFILTER -9 -2\n");
  SECTION("a line above the CONFIG line") {
    const std::string above = dir.write(
        "lines_above.cfg", "RCUT2B 4.0\nCONFIG " + pot + "\nDBFILE a.tadah\n");
    const Context c = CoreUserConfigParser().parse(above);
    const std::string by = " (CONFIG on line 2 of " + above + ")";
    CHECK(c("RCUT2B") == std::vector<std::string>{"4.0", "5.0"});
    CHECK(c.key_lines("RCUT2B") ==
          std::vector<KeyLine>{{"line 1 of " + above, false, above, 1, 1, 1},
                               {"line 3 of " + pot + by, true, pot, 1, 3, 3}});
    CHECK(c.key_lines("WEIGHTS") ==
          std::vector<KeyLine>{{"line 2 of " + pot + by, true, pot, 2, 2, 2}});
    CHECK(c.key_lines("ATOM") ==
          std::vector<KeyLine>{{"line 1 of " + pot + by, true, pot, 1, 1, 1}});
    CHECK(c.key_lines("DBFILE") ==
          std::vector<KeyLine>{{"line 3 of " + above, false, above, 1, 3, 3}});
    CHECK(c.key_lines("LAMBDA").empty());
  }
  SECTION("a line below the CONFIG line") {
    const std::string below =
        dir.write("lines_below.cfg", "CONFIG " + pot + "\nRCUT2B 4.0\n");
    const Context c = CoreUserConfigParser().parse(below);
    CHECK(c.key_lines("RCUT2B") ==
          std::vector<KeyLine>{
              {"line 3 of " + pot + " (CONFIG on line 1 of " + below + ")",
               true, pot, 1, 3, 3},
              {"line 2 of " + below, false, below, 1, 2, 2}});
  }
  SECTION("the potential read alone") {
    const Context c = CoreUserConfigParser().parse(pot);
    CHECK(c.key_lines("RCUT2B") ==
          std::vector<KeyLine>{{"line 3 of " + pot, true, pot, 1, 3, 3}});
    CHECK(c.key_lines("EFILTER") ==
          std::vector<KeyLine>{{"line 4 of " + pot, true, pot, 2, 4, 4}});
  }
  SECTION("a nested include") {
    const std::string mid =
        dir.write("lines_mid.cfg", "LAMBDA 0\nCONFIG " + pot + "\n");
    const std::string top = dir.write("lines_top.cfg", "CONFIG " + mid + "\n");
    const Context c = CoreUserConfigParser().parse(top);
    CHECK(c.key_lines("LAMBDA") ==
          std::vector<KeyLine>{
              {"line 1 of " + mid + " (CONFIG on line 1 of " + top + ")",
               false, mid, 1, 1, 1}});
    CHECK(c.key_lines("RCUT2B") ==
          std::vector<KeyLine>{{"line 3 of " + pot + " (CONFIG on line 2 of " +
                                    mid + ", which CONFIG on line 1 of " +
                                    top + " includes)",
                                true, pot, 1, 3, 3}});
  }
  SECTION("lines of no file") {
    const Context c = CoreUserConfigParser().parse(
        std::vector<std::string>{"ATOM Ta", "RCUT2B 5.0", "WEIGHTS 1"});
    CHECK(c.key_lines("RCUT2B") ==
          std::vector<KeyLine>{{"line 2", true, "", 1, 2, 2}});
  }
  SECTION("a line the parse does not store") {
    const std::string run =
        dir.write("lines_run.cfg", "CONFIG " + pot + "\nDBFILE a.tadah\n");
    std::vector<SkippedKeyLine> skipped;
    const Context c =
        CoreUserConfigParser().parse(run, skip_in_weights_files, skipped);
    REQUIRE_FALSE(c.has_stored("EFILTER"));
    CHECK(c.key_lines("EFILTER").empty());
    CHECK(c.key_lines("DBFILE") ==
          std::vector<KeyLine>{{"line 2 of " + run, false, run, 1, 2, 2}});
  }
  SECTION("a key of two lines of one file") {
    const std::string two = dir.write(
        "lines_two.tadah",
        "ATOM Ta\nWEIGHTS 0.25 -0.5\nRCUT2B 5.0\nDBFILE a.tadah\n"
        "RCUT2B 6.0 7.0\n");
    const Context c = CoreUserConfigParser().parse(two);
    CHECK(c("RCUT2B") == std::vector<std::string>{"5.0", "6.0", "7.0"});
    CHECK(c.key_lines("RCUT2B") ==
          std::vector<KeyLine>{{"line 3 of " + two, true, two, 1, 3, 3},
                               {"line 5 of " + two, true, two, 2, 5, 5}});
  }
  SECTION("comments and blank lines between the lines of a key") {
    const std::vector<std::string> text{
        "ATOM Ta",          "# a comment",
        "WEIGHTS 1",        "",
        "RCUT2B 5.0",       "  # an indented comment",
        "RCUT2B 6.0 # an inline comment",
        "LAMBDA 0",         "RCUT2B 7.0"};
    std::string body;
    for (const auto &l : text) body += l + "\n";
    const std::string commented = dir.write("lines_commented.tadah", body);
    const Context c = CoreUserConfigParser().parse(commented);
    CHECK(c.key_lines("RCUT2B") ==
          std::vector<KeyLine>{
              {"line 5 of " + commented, true, commented, 1, 5, 3},
              {"line 7 of " + commented, true, commented, 1, 7, 4},
              {"line 9 of " + commented, true, commented, 1, 9, 6}});
    const Context nofile = CoreUserConfigParser().parse(text);
    CHECK(nofile.key_lines("RCUT2B") ==
          std::vector<KeyLine>{{"line 5", true, "", 1, 5, 3},
                               {"line 7", true, "", 1, 7, 4},
                               {"line 9", true, "", 1, 9, 6}});
  }
}

/* Pins: every error of a line of a file that a configuration includes with
 * CONFIG names that file, its line and the CONFIG lines that include it,
 * innermost first, as the refusal of a second line of a key of one value
 * does; outer.cfg is "WATOM 73", "CONFIG <inc>":
 *  - inc.cfg "ATOM Ta", "OPTIMIZER", "ALGO x": "OPTIMIZER on line 2 of
 *    <inc> (CONFIG on line 2 of <outer>) is not closed; expected
 *    ENDOPTIMIZER";
 *  - desc.cfg "DESCRIPTOR", "ENDFOO": "Mismatched ENDFOO on line 2 of
 *    <desc> (CONFIG on line 2 of <outer>); expected ENDDESCRIPTOR";
 *    open.cfg "DESCRIPTOR", "TYPE x": "DESCRIPTOR on line 1 of <open>
 *    (CONFIG on line 2 of <outer>) is not closed; expected ENDDESCRIPTOR";
 *  - a key the parser refuses (refuse_keys), force.cfg "ATOM Ta", "FORCE
 *    true": "Key FORCE on line 2 of <force> (CONFIG on line 2 of <outer>)
 *    is not accepted here: <reason>";
 *  - a key of another command in a configuration file of tadah train,
 *    cmd.cfg "ANALYTICS true": "Key ANALYTICS on line 1 of <cmd> (CONFIG on
 *    line 2 of <outer>) is not accepted here: ANALYTICS is the option
 *    --analytics (-A) of tadah predict, ...";
 *  - a CONFIG line of a nested include whose file does not open: top.cfg
 *    "CONFIG mid.cfg", mid.cfg "ATOM Ta", "CONFIG missing.cfg": "Failed to
 *    open config file: <missing> (CONFIG on line 2 of <mid>, which CONFIG
 *    on line 1 of <top> includes)".
 * Before the fix: each error named the line and its file alone ("OPTIMIZER
 * on line 2 of <inc> is not closed; ...", "Key FORCE on line 2 of <force>
 * is not accepted here: ..."), and the file that does not open the
 * innermost CONFIG line alone ("(CONFIG on line 2 of <mid>)"). */
TEST_CASE("Config parser: an error of a line of an included file names the "
          "CONFIG lines that include it",
          "[config][config_include][line_errors]") {
  RefuseScratch dir;
  const auto outer_of = [&dir](const std::string &name,
                               const std::string &inc) {
    return dir.write(name, "WATOM 73\nCONFIG " + inc + "\n");
  };
  const auto by = [](const std::string &outer) {
    return " (CONFIG on line 2 of " + outer + ")";
  };
  SECTION("a block that is not closed") {
    const std::string inc =
        dir.write("open_inc.cfg", "ATOM Ta\nOPTIMIZER\nALGO x\n");
    const std::string outer = outer_of("open_outer.cfg", inc);
    CHECK(runtime_error_of([&] { return CoreUserConfigParser().parse(outer); },
                           "parse " + outer) ==
          "OPTIMIZER on line 2 of " + inc + by(outer) +
              " is not closed; expected ENDOPTIMIZER");
  }
  SECTION("a DESCRIPTOR block") {
    const std::string desc = dir.write("desc_inc.cfg", "DESCRIPTOR\nENDFOO\n");
    const std::string outer = outer_of("desc_outer.cfg", desc);
    CHECK(runtime_error_of([&] { return CoreUserConfigParser().parse(outer); },
                           "parse " + outer) ==
          "Mismatched ENDFOO on line 2 of " + desc + by(outer) +
              "; expected ENDDESCRIPTOR");
    const std::string open =
        dir.write("desc_open.cfg", "DESCRIPTOR\nTYPE x\n");
    const std::string around = outer_of("desc_around.cfg", open);
    CHECK(runtime_error_of(
              [&] { return CoreUserConfigParser().parse(around); },
              "parse " + around) ==
          "DESCRIPTOR on line 1 of " + open + by(around) +
              " is not closed; expected ENDDESCRIPTOR");
  }
  SECTION("a key the parser refuses") {
    const std::string inc =
        dir.write("refused_inc.cfg", "ATOM Ta\nFORCE true\n");
    const std::string outer = outer_of("refused_outer.cfg", inc);
    CoreUserConfigParser parser;
    parser.refuse_keys({{"FORCE", kForceReason}});
    CHECK(runtime_error_of([&] { return parser.parse(outer); },
                           "parse " + outer) ==
          "Key FORCE on line 2 of " + inc + by(outer) +
              " is not accepted here: " + kForceReason);
  }
  SECTION("a key of another command") {
    const std::string inc = dir.write("command_inc.cfg", "ANALYTICS true\n");
    const std::string outer = outer_of("command_outer.cfg", inc);
    CoreUserConfigParser parser;
    parser.set_command("TRAIN");
    CHECK_THAT(runtime_error_of([&] { return parser.parse(outer); },
                                "parse " + outer),
               Catch::Matchers::StartsWith(
                   "Key ANALYTICS on line 1 of " + inc + by(outer) +
                   " is not accepted here: ANALYTICS is the option "
                   "--analytics (-A) of tadah predict"));
  }
  SECTION("a nested include whose file does not open") {
    const std::string missing = dir.path("missing.cfg");
    const std::string mid =
        dir.write("missing_mid.cfg", "ATOM Ta\nCONFIG " + missing + "\n");
    const std::string top =
        dir.write("missing_top.cfg", "CONFIG " + mid + "\n");
    CHECK(runtime_error_of([&] { return CoreUserConfigParser().parse(top); },
                           "parse " + top) ==
          "Failed to open config file: " + missing + " (CONFIG on line 2 of " +
              mid + ", which CONFIG on line 1 of " + top + " includes)");
  }
}

/* Pins: a second line of a key that a potential's own lines set, in a file
 * that includes the potential with CONFIG, is refused with the way to
 * change a potential's value; pot.tadah is "ATOM Ta", "WEIGHTS 0.25 -0.5",
 * "MODEL M_BLR BF_Linear", "BIAS false":
 *  - model.cfg "MODEL M_BLR BF_Linear", "CONFIG pot.tadah": "Too many values
 *    for key MODEL on line 3 of <pot> (CONFIG on line 2 of <model>): MODEL
 *    takes at most 3, and its lines read so far give 4, with line 1 of
 *    <model> before it; a potential's value is changed by editing its line
 *    in a copy of the potential (the Training page of the documentation,
 *    Retraining from a potential)";
 *  - below.cfg "CONFIG pot.tadah", "BIAS true": "Key BIAS is set on line 4
 *    of <pot> (CONFIG on line 1 of <below>) and on line 2 of <below>: a key
 *    of one value is set once in a configuration file and the files it
 *    includes with CONFIG; a potential's value is changed ...";
 *  - a copy of the potential with a second BIAS line: both lines are the
 *    potential's, and the refusal states the rule alone ("Key BIAS is set
 *    on line 4 of <copy> and on line 5 of <copy>: a key of one value is set
 *    once in a configuration file and the files it includes with CONFIG").
 * Before the fix: the first two ended at the rule ("... read so far give
 * 4", "... the files it includes with CONFIG"), the first without the line
 * of <model>. */
TEST_CASE("Config parser: a second line of a potential's key in a file that "
          "includes it gives the way to change the potential's value",
          "[config][config_include][concatenation]") {
  RefuseScratch dir;
  const std::string body =
      "ATOM Ta\nWEIGHTS 0.25 -0.5\nMODEL M_BLR BF_Linear\nBIAS false\n";
  const std::string pot = dir.write("advice_pot.tadah", body);
  const std::string model = dir.write(
      "advice_model.cfg", "MODEL M_BLR BF_Linear\nCONFIG " + pot + "\n");
  CHECK(runtime_error_of([&] { return CoreUserConfigParser().parse(model); },
                         "parse " + model) ==
        "Too many values for key MODEL on line 3 of " + pot +
            " (CONFIG on line 2 of " + model +
            "): MODEL takes at most 3, and its lines read so far give 4, "
            "with line 1 of " + model + " before it" + kCopyAdvice);
  const std::string below =
      dir.write("advice_below.cfg", "CONFIG " + pot + "\nBIAS true\n");
  CHECK(runtime_error_of([&] { return CoreUserConfigParser().parse(below); },
                         "parse " + below) ==
        "Key BIAS is set on line 4 of " + pot + " (CONFIG on line 1 of " +
            below + ") and on line 2 of " + below + ": " + kSetOnce +
            kCopyAdvice);
  const std::string copy =
      dir.write("advice_copy.tadah", body + "BIAS true\n");
  CHECK(runtime_error_of([&] { return CoreUserConfigParser().parse(copy); },
                         "parse " + copy) ==
        "Key BIAS is set on line 4 of " + copy + " and on line 5 of " + copy +
            ": " + kSetOnce);
}

/* Pins: a key that a command takes fewer values of than the key itself (the
 * command's table in the blueprint gives the key a number of its own:
 * OUTFILE of tadah train, tadah refit and tadah analysis bfunc, each of
 * which writes one file) is read with the
 * command's number in a configuration file of that command
 * (set_command()):
 *  - outer.cfg "OUTFILE a.tadah", "CONFIG inc.cfg", inc.cfg "OUTFILE
 *    b.tadah": "Key OUTFILE is set on line 1 of <outer> and on line 1 of
 *    <inc> (CONFIG on line 2 of <outer>): a key of one value is set once in
 *    a configuration file and the files it includes with CONFIG";
 *  - one.cfg "OUTFILE a.tadah b.tadah": "Too many values for key OUTFILE on
 *    line 1 of <one>: OUTFILE of tadah train takes at most 1, and its lines
 *    read so far give 2" (of tadah refit, ... for the others);
 *  - a parse of no command reads both lines, and both values of one line,
 *    as OUTFILE takes any number of values.
 * A configuration file of tadah analysis dataset_stats or tadah analysis
 * suggest_bounds, whose -c needs -o (the cli_needs of its CONFIG key),
 * refuses the first OUTFILE line of either file at its line: "Key OUTFILE
 * on line 1 of <outer> is not accepted here: OUTFILE is the option
 * --outfile (-o) that --config (-c) of tadah analysis dataset_stats needs
 * on the command line, not a key of a configuration file of tadah analysis
 * dataset_stats".
 * Before the fix: every parse read both lines and both values (OUTFILE
 * a.tadah b.tadah), and the command wrote the first file, a.tadah, the
 * other ignored without a message. Before the fix of a key that -c needs
 * on the command line, tadah analysis dataset_stats was one of the
 * commands above, with their two messages; before the fix that makes the
 * -c of tadah analysis suggest_bounds need -o, so was suggest_bounds. */
TEST_CASE("Config parser: a key a command takes one value of is set once in "
          "a configuration file of that command",
          "[config][config_include][concatenation][command_counts]") {
  RefuseScratch dir;
  const std::string inc = dir.write("outfile_inc.cfg", "OUTFILE b.tadah\n");
  const std::string outer = dir.write(
      "outfile_outer.cfg", "OUTFILE a.tadah\nCONFIG " + inc + "\n");
  const std::string one =
      dir.write("outfile_one.cfg", "OUTFILE a.tadah b.tadah\n");
  for (const std::string task : {"TRAIN", "REFIT", "ANALYSIS.bfunc"}) {
    INFO("command " << task);
    CoreUserConfigParser parser;
    parser.set_command(task);
    CHECK(runtime_error_of([&] { return parser.parse(outer); },
                           "parse " + outer) ==
          "Key OUTFILE is set on line 1 of " + outer + " and on line 1 of " +
              inc + " (CONFIG on line 2 of " + outer + "): " + kSetOnce);
    CHECK(runtime_error_of([&] { return parser.parse(one); },
                           "parse " + one) ==
          "Too many values for key OUTFILE on line 1 of " + one +
              ": OUTFILE of " + task_command_words(task) +
              " takes at most 1, and its lines read so far give 2");
  }
  for (const std::string task :
       {"ANALYSIS.dataset_stats", "ANALYSIS.suggest_bounds"}) {
    INFO("command " << task);
    CoreUserConfigParser parser;
    parser.set_command(task);
    const std::string words = task_command_words(task);
    for (const std::string &file : {outer, one})
      CHECK(runtime_error_of([&] { return parser.parse(file); },
                             "parse " + file) ==
            "Key OUTFILE on line 1 of " + file +
                " is not accepted here: OUTFILE is the option --outfile (-o) "
                "that --config (-c) of " + words + " needs on the command "
                "line, not a key of a configuration file of " + words);
  }
  const std::vector<std::string> both{"a.tadah", "b.tadah"};
  CHECK(CoreUserConfigParser().parse(outer)("OUTFILE") == both);
  CHECK(CoreUserConfigParser().parse(one)("OUTFILE") == both);
}

/* Pins: a configuration file of a command whose -c needs the option of a
 * key on the command line refuses a line of that key at its line, also
 * for a key that other commands read from a configuration file
 * (HPOTARGET, which tadah analysis check_bounds reads from one): a
 * configuration file of tadah hpo with "HPOTARGET config.hpo" on line 2,
 * "Key HPOTARGET on line 2 of <file> is not accepted here: HPOTARGET is
 * the option --hpotarget that --config (-c) of tadah hpo needs on the
 * command line, not a key of a configuration file of tadah hpo"; a CONFIG
 * include of tadah hpo names the line of the including file as well
 * ("... on line 1 of <inc> (CONFIG on line 1 of <outer>) ..."); the same
 * file is read by tadah analysis check_bounds (HPOTARGET config.hpo) and
 * by a parse of no command; a potential (a file with WEIGHTS) with an
 * HPOTARGET line loads as a configuration file of tadah hpo without it,
 * the line of a run setting.
 * Before the fix: tadah hpo read the line, and its --hpotarget, which its
 * -c needs, replaced it without a message. */
TEST_CASE("Config parser: a configuration file of tadah hpo refuses an "
          "HPOTARGET line, which --hpotarget replaces",
          "[config][command_keys][hpotarget]") {
  RefuseScratch dir;
  const std::string file =
      dir.write("hpo_target.cfg", "DBFILE db.tadah\nHPOTARGET config.hpo\n");
  const std::string inc = dir.write("hpo_inc.cfg", "HPOTARGET config.hpo\n");
  const std::string outer =
      dir.write("hpo_outer.cfg", "CONFIG " + inc + "\nDBFILE db.tadah\n");
  CoreUserConfigParser hpo;
  hpo.set_command("HPO");
  CHECK(runtime_error_of([&] { return hpo.parse(file); }, "parse " + file) ==
        "Key HPOTARGET on line 2 of " + file +
            " is not accepted here: HPOTARGET is the option --hpotarget that "
            "--config (-c) of tadah hpo needs on the command line, not a key "
            "of a configuration file of tadah hpo");
  CHECK(runtime_error_of([&] { return hpo.parse(outer); }, "parse " + outer) ==
        "Key HPOTARGET on line 1 of " + inc + " (CONFIG on line 1 of " +
            outer +
            ") is not accepted here: HPOTARGET is the option --hpotarget "
            "that --config (-c) of tadah hpo needs on the command line, not "
            "a key of a configuration file of tadah hpo");
  CoreUserConfigParser check;
  check.set_command("ANALYSIS.check_bounds");
  CHECK(check.parse(file).get<std::string>("HPOTARGET") == "config.hpo");
  CHECK(CoreUserConfigParser().parse(file).get<std::string>("HPOTARGET") ==
        "config.hpo");
  const std::string pot = dir.write(
      "hpo_pot.tadah", "WEIGHTS 1.0 2.0\nHPOTARGET config.hpo\n");
  const tadah::core::Context p = hpo.parse(pot);
  CHECK(p.has_stored("WEIGHTS"));
  CHECK_FALSE(p.has_stored("HPOTARGET"));
}
