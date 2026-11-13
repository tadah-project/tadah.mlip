// The paths of INHERIT in a blueprint. A path names a table by the names of
// the tables that lead to it from the top of the blueprint, with a dot
// between two names. Its parts are the runs of characters between its dots,
// the empty ones included: "A..b" has the parts "A", "" and "b", ".b" has ""
// and "b", "A." has "A" and "", and the empty path has one part, which is
// empty. A path of n dots has n + 1 parts; no other character separates.
//
// Two places read a path:
//  - the INHERIT of a table takes the fields of the table each path names,
//    found from the top of the blueprint, one table per part;
//  - the INHERIT of the keys table of a command gives the command one option
//    per path, named by the last part of the path, which inherits the path.

#include "catch2/catch.hpp"

#include <tadah/core/blueprint_parser.h>

#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

using namespace tadah::core;
using Catch::Matchers::Contains;

namespace {

const char *const kNamed = "The table the path names.";
const char *const kOther = "Another table.";

/* A path, the table it names (as the header of a TOML table, in which ""
 * is an empty name), tables that a path split in another way names, and
 * the last part of the path. */
struct PathCase {
  std::string path;
  std::string named;
  std::vector<std::string> others;
  std::string last;
};

const std::vector<PathCase> &path_cases() {
  static const std::vector<PathCase> cases = {
      // No dot: one part.
      {"b", "b", {}, "b"},
      // One dot: two parts; the path is not the name of one table.
      {"A.b", "A.b", {"\"A.b\""}, "b"},
      // Dots in a row: an empty part between each two of them.
      {"A..b", "A.\"\".b", {"A.b"}, "b"},
      {"A...b", "A.\"\".\"\".b", {"A.b", "A.\"\".b"}, "b"},
      // A dot at the start: an empty first part.
      {".b", "\"\".b", {"b"}, "b"},
      {"..b", "\"\".\"\".b", {"b", "\"\".b"}, "b"},
      // A dot at the end: an empty last part.
      {"A.", "A.\"\"", {}, ""},
      {"A..", "A.\"\".\"\"", {"A.b"}, ""},
      // The empty path: one empty part. A dot alone: two.
      {"", "\"\"", {"b"}, ""},
      {".", "\"\".\"\"", {"b"}, ""},
      // A blank and a comma are characters of a part.
      {"A b.c,d", "\"A b\".\"c,d\"", {"A.b.c.d", "A.b.\"c,d\""}, "c,d"},
  };
  return cases;
}

/* A table [header] with the mandatory fields of an option. */
std::string option_table(const std::string &header,
                         const std::string &description) {
  return "[" + header + "]\nvalue_type = [\"STRING\"]\n"
         "value_format = [\"<string>\"]\nvalue_N = [1]\n"
         "description = \"" + description + "\"\n";
}

/* The command demo of a blueprint (TOML) that holds @p tables and, in the
 * table @p inheriting of the command, INHERIT = [@p path]. */
BlueprintTask demo_command(const std::string &tables,
                           const std::string &inheriting,
                           const std::string &path) {
  std::istringstream ss(
      tables +
      "[TASKS.demo]\ndescription = \"A command.\"\n"
      "long_description = \"\"\nexamples = [\"tadah demo\"]\n"
      "[" + inheriting + "]\nINHERIT = ['" + path + "']\n");
  const toml::value blueprint = toml::parse(ss);
  return BlueprintParser::buildTasksMap(blueprint).at("DEMO");
}

/* The tables of @p c: the one its path names and the others. */
std::string tables_of(const PathCase &c) {
  std::string tables = option_table(c.named, kNamed);
  for (const std::string &other : c.others)
    tables += option_table(other, kOther);
  return tables;
}

}  // namespace

/* Pins: the INHERIT of a table takes the fields of the table that the
 * parts of each path name, the empty parts included, and of no other.
 * Before the change that added this test: passes (the split of a path into
 * parts was the one the test states). */
TEST_CASE("Blueprint: a table inherits the table that the parts of the path "
          "name, empty parts included",
          "[BlueprintParser][inherit]") {
  for (const PathCase &c : path_cases()) {
    INFO("the path '" << c.path << "', which names [" << c.named << "]");
    BlueprintTask demo;
    REQUIRE_NOTHROW(
        demo = demo_command(tables_of(c), "TASKS.demo.keys.opt", c.path));
    REQUIRE(demo.blueprintKeys.size() == 1);
    CHECK(demo.blueprintKeys[0].keyName == "DEMO.opt");
    CHECK(demo.blueprintKeys[0].description == kNamed);
  }
}

/* Pins: the INHERIT of the keys table of a command gives one option per
 * path, named by the last part of the path (empty when the path ends with
 * a dot or is empty), with the fields of the table the path names.
 * Before the change that added this test: passes. */
TEST_CASE("Blueprint: the keys of a command inherit one option per path, "
          "named by the last part of the path",
          "[BlueprintParser][inherit]") {
  for (const PathCase &c : path_cases()) {
    INFO("the path '" << c.path << "', which names [" << c.named << "]");
    BlueprintTask demo;
    REQUIRE_NOTHROW(
        demo = demo_command(tables_of(c), "TASKS.demo.keys", c.path));
    REQUIRE(demo.blueprintKeys.size() == 1);
    CHECK(demo.blueprintKeys[0].keyName == "DEMO." + c.last);
    CHECK(demo.blueprintKeys[0].description == kNamed);
  }
}

/* Pins: a path with an empty part names no table of a blueprint that holds
 * only [A.b] and [b], and is refused in both places: an empty part is not
 * skipped, and the empty path is one empty part, not the blueprint itself.
 * Before the change that added this test: passes. */
TEST_CASE("Blueprint: a path whose empty part names no table is refused",
          "[BlueprintParser][inherit]") {
  const std::string tables =
      option_table("A.b", kOther) + option_table("b", kOther);
  for (const char *path : {"A..b", ".b", "b.", ".A.b", "A.b.", "", ".", ".."}) {
    for (const char *inheriting : {"TASKS.demo.keys.opt", "TASKS.demo.keys"}) {
      INFO("the path '" << path << "' in [" << inheriting << "]");
      CHECK_THROWS_WITH(demo_command(tables, inheriting, path),
                        Contains("Error during inheritance expansion"));
    }
  }
}
