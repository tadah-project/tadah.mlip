// Canary for the Catch2 test discovery in cmake-tools
// (testing/CatchAddTests.cmake); see check_discovery.cmake.
//
// Every test case name below contains something that CMake list handling,
// CMake bracket arguments, CTest or Catch's test-spec parser treats
// specially. The names are defined once, here. `--canary-names-hex` prints
// them hex-encoded, one per line, so the checker compares against an encoding
// that no escaping bug can distort. Any other command line runs Catch.
//
// When TADAH_CANARY_HIDDEN_CASE is set, a hidden test case ([.]) with that
// name is registered as well; check_discovery.cmake uses it to give a canary
// name a hidden twin. The canary registered with CTest never sets it.
#define CATCH_CONFIG_RUNNER
#include "catch2/catch.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

// Catch expands a tag alias wherever its text occurs in a test spec, even
// inside a quoted name (see "canary: tag alias ..." below).
CATCH_REGISTER_TAG_ALIAS("[@canary]", "[canary]")

namespace {

const char* const kCanaryNames[] = {
    "canary: plain name",
    // An unbalanced '[' made CMake list splitting swallow every later name.
    "canary: histogram counts samples in [low, high)",
    // A ';' split one name into two registrations.
    "canary: scalar -> (1) ; vector -> (1-N)",
    "canary: semicolon;without spaces",
    // Catch's escape character.
    "canary: backslash \\ in the middle",
    "canary: backslash before a semicolon \\;",
    "canary: ends with a backslash \\",
    // ',' separates specs; '[' and ']' delimit tags.
    "canary: commas, and more, commas",
    "canary: lone ] closing bracket",
    "canary: ['q'] and [\"q\"] brackets",
    "[canary] leading bracket, looks like a tag",
    // Catch prints names that start with '#' in double quotes.
    "#canary: leading hash",
    // Special at the start of a spec: '~' excludes, '"' quotes, ' ' is
    // skipped, '-' makes an option, "exclude:" excludes.
    "~canary: leading tilde",
    "~ canary: leading tilde and space",
    "-canary: leading dash",
    "\"canary: leading double quote",
    "exclude:canary leading exclude prefix",
    "canary: double quote \" inside",
    "canary: trailing space ",
    // Names made only of characters that are special in a spec.
    "-",
    "~",
    "\"",
    "\\",
    ",",
    "[",
    "]",
    "[]",
    "~ -\"\\,[]",
    // Catch trims a spec before parsing it, so whitespace other than ' '
    // must not uncover a '~', "exclude:" or '"' behind it. Without the
    // quoting, the tab-quote name would select its lookalike below.
    "\t~canary: tab then tilde",
    "\texclude:canary tab then exclude prefix",
    "\t\"canary: tab then double quote",
    "canary: tab then double quote",
    // A tag alias in a name must not be expanded.
    "canary: tag alias [@canary] in a name",
    // '*' is a wildcard only at either end.
    "canary: star * in the middle",
    // Closing delimiters of CMake bracket arguments.
    "canary: bracket-argument closers ]] ]=] ]==]",
    // CMake syntax that must stay literal.
    "canary: CMake syntax ${VAR} $ENV{HOME} @VAR@ \\n",
    "canary: tab\tinside",
};

void canary_case() { SUCCEED("canary test case ran"); }

}  // namespace

int main(int argc, char** argv) {
  if (argc == 2 && std::strcmp(argv[1], "--canary-names-hex") == 0) {
    for (const char* name : kCanaryNames) {
      for (const char* p = name; *p; ++p)
        std::printf("%02x", static_cast<unsigned char>(*p));
      std::printf("\n");
    }
    return 0;
  }
  for (const char* name : kCanaryNames)
    REGISTER_TEST_CASE(canary_case, name, "[canary]");
  if (const char* hidden = std::getenv("TADAH_CANARY_HIDDEN_CASE"))
    REGISTER_TEST_CASE(canary_case, hidden, "[.][canary_hidden]");
  return Catch::Session().run(argc, argv);
}
