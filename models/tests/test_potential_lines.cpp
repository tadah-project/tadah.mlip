// test_potential_lines.cpp
//
// A potential is read as it was written: the RCTYPE*, RCUT* and list values
// of its own lines that its descriptors do not take are reported once per
// key and left out (D_mJoin::parse_context, test_potential_list_values.cpp).
// A configuration that includes the potential with CONFIG is the
// concatenation of its lines: a line outside the potential's own lines (a
// line of the including file, above or below the CONFIG line) adds its
// values to the key, and the key is read as in a training configuration:
// values past those the descriptors take are refused, naming the lines that
// set the key and their files, and the lines of the TYPE key. The refusal of
// a count names those lines for a training configuration read from a file as
// well.

#include "catch2/catch.hpp"

#include <tadah/core/context.h>
#include <tadah/core/core_user_config_parser.h>
#include <tadah/core/key_notice.h>
#include <tadah/core/periodic_table.h>
#include <tadah/models/dc_selector.h>
#include <tadah/models/descriptors/d_mjoin.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>
#include <unistd.h>

using tadah::core::Context;
using tadah::core::CoreUserConfigParser;
using tadah::core::IgnoredKeyNoticeCapture;
using tadah::models::D_mJoin;
using tadah::models::DC_Selector;

namespace {

/* A scratch directory unique to this process, removed with its files when
 * the object goes out of scope. */
class Scratch {
public:
  Scratch()
      : dir_(std::filesystem::temp_directory_path() /
             ("tadah_models_potential_lines_" + std::to_string(::getpid()))) {
    std::error_code ec;
    std::filesystem::remove_all(dir_, ec);
    std::filesystem::create_directories(dir_);
  }
  ~Scratch() {
    std::error_code ec;
    std::filesystem::remove_all(dir_, ec);
  }
  Scratch(const Scratch &) = delete;
  Scratch &operator=(const Scratch &) = delete;

  /* Writes @p body to <dir>/<name>; returns its path. */
  std::string write(const std::string &name, const std::string &body) const {
    const auto p = dir_ / name;
    std::ofstream os(p);
    os << body;
    os.close();
    REQUIRE_FALSE(os.fail());
    return p.string();
  }

private:
  std::filesystem::path dir_;
};

/* A single D2_Blip potential: RCUT2B 5 on line 6. */
const char *kSingle2B = "ATOM Ta\n"
                        "INIT2B true\n"
                        "INITMB false\n"
                        "TYPE2B D2_Blip 2 2 Ta Ta\n"
                        "RCTYPE2B Cut_Cos\n"
                        "RCUT2B 5\n"
                        "CGRID2B 2 3\n"
                        "SGRID2B 1 1\n"
                        "WEIGHTS 0.1 0.2 0.3\n";

/* A single DM_EAD potential: RCUTMB 5 on line 6. */
const char *kSingleMB = "ATOM Ta\n"
                        "INIT2B false\n"
                        "INITMB true\n"
                        "TYPEMB DM_EAD 0 2 2 Ta Ta\n"
                        "RCTYPEMB Cut_Cos\n"
                        "RCUTMB 5\n"
                        "CGRIDMB 2 3\n"
                        "SGRIDMB 1 1\n"
                        "WEIGHTS 0.1 0.2 0.3\n";

/* A D2_mJoin potential, D2_Blip with RCUT2B 5.3 and D2_BP with 4.5: RCUT2B
 * on line 8, CGRID2B on line 9. With @p old one more RCUT2B value, 6, than
 * the line takes, as tadah 1.5.0 wrote them. */
std::string joined_2b(bool old = false) {
  return std::string("ATOM Ta\n"
                     "INIT2B true\n"
                     "INITMB false\n"
                     "TYPE2B D2_mJoin\n"
                     "TYPE2B D2_Blip 2 2 Ta Ta\n"
                     "TYPE2B D2_BP 2 2 Ta Ta\n"
                     "RCTYPE2B Cut_Cos Cut_Cos\n") +
         (old ? "RCUT2B 5.3 4.5 6\n" : "RCUT2B 5.3 4.5\n") +
         "CGRID2B 2 3 2.5 3.5\n"
         "SGRID2B 1 1 0.5 0.5\n"
         "WEIGHTS 0.1 0.2 0.3 0.4 0.5\n";
}

/* A DM_mJoin potential of two DM_EAD descriptors: RCTYPEMB on line 7,
 * RCUTMB 4.5 5 on line 8. */
const char *kJoinedMB = "ATOM Ta\n"
                        "INIT2B false\n"
                        "INITMB true\n"
                        "TYPEMB DM_mJoin\n"
                        "TYPEMB DM_EAD 0 2 2 Ta Ta\n"
                        "TYPEMB DM_EAD 0 1 1 Ta Ta\n"
                        "RCTYPEMB Cut_Cos Cut_Cos\n"
                        "RCUTMB 4.5 5\n"
                        "CGRIDMB 2 3 2.5\n"
                        "SGRIDMB 1 1 1\n"
                        "WEIGHTS 0.1 0.2 0.3 0.4\n";

Context parsed(const std::string &file) {
  tadah::core::PeriodicTable::initialize();
  return CoreUserConfigParser().parse(file);
}

/* The message of D_mJoin::expand_grids (tadah train, tadah hpo and the
 * analysis commands) on @p c; empty when it throws none. */
std::string refusal(Context c) {
  try {
    D_mJoin::expand_grids(c);
  } catch (const std::exception &e) {
    return e.what();
  }
  return "";
}

/* The message of a model built from @p c (DC_Selector: tadah predict,
 * pair_style tadah); empty when it throws none. */
std::string load_refusal(const Context &c) {
  try {
    DC_Selector s(c);
  } catch (const std::exception &e) {
    return e.what();
  }
  return "";
}

std::string joined(const std::vector<std::string> &v) {
  std::string s;
  for (const auto &x : v) s += x + "\n";
  return s;
}

const char *kOneRcut2B =
    ": each descriptor takes one RCUT2B value, in the order of the TYPE2B "
    "lines";

} // namespace

/* A line of RCUT2B in a file that includes a single-descriptor potential
 * with CONFIG, above or below the CONFIG line, gives RCUT2B two values, and
 * the descriptor takes one: refused as in a training configuration, naming
 * both lines and their files, for training (expand_grids) and for a model
 * built from the configuration (DC_Selector). A smaller cutoff than the
 * potential's alike. A KDEG2B line, which the descriptor does not read, is
 * refused naming its line. Each refusal ends with the TYPE2B line of the
 * potential, " (TYPE2B on line 4 of single.tadah (CONFIG on line 2 of
 * above.train))".
 * Before the fix: accepted with 'Part of RCUT2B (5) is ignored: TYPE2B
 * D2_Blip takes one RCUT2B value, and its cutoff is the largest, 6; the
 * potential works unchanged with RCUT2B 6 alone': the largest cutoff was
 * the model's (4 below the CONFIG line was ignored).
 * Before the fix that names the TYPE line in a refusal of a count: each
 * refusal ended at "... in the order of the TYPE2B lines" or at the quoted
 * TYPE2B line. */
TEST_CASE("A line of RCUT2B in a file that includes a single-descriptor "
          "potential is refused naming both lines",
          "[D_mJoin][potential][config_include][potential_lines]") {
  Scratch dir;
  const std::string pot = dir.write("single.tadah", kSingle2B);
  SECTION("above the CONFIG line") {
    const std::string above =
        dir.write("above.train", "RCUT2B 6\nCONFIG " + pot + "\n");
    const std::string expected =
        "TYPE2B names 1 descriptor (D2_Blip) but RCUT2B has 2 values, set "
        "on line 1 of " + above + " and on line 6 of " + pot +
        " (CONFIG on line 2 of " + above + ")" + kOneRcut2B +
        " (TYPE2B on line 4 of " + pot + " (CONFIG on line 2 of " + above +
        "))";
    IgnoredKeyNoticeCapture cap;
    CHECK(refusal(parsed(above)) == expected);
    CHECK(load_refusal(parsed(above)) == expected);
    INFO(joined(cap.notices()));
    CHECK(cap.notices().empty());
  }
  SECTION("below the CONFIG line, a smaller cutoff") {
    const std::string below =
        dir.write("below.train", "CONFIG " + pot + "\nRCUT2B 4\n");
    IgnoredKeyNoticeCapture cap;
    CHECK(refusal(parsed(below)) ==
          "TYPE2B names 1 descriptor (D2_Blip) but RCUT2B has 2 values, set "
          "on line 6 of " + pot + " (CONFIG on line 1 of " + below +
              ") and on line 2 of " + below + kOneRcut2B +
              " (TYPE2B on line 4 of " + pot + " (CONFIG on line 1 of " +
              below + "))");
    INFO(joined(cap.notices()));
    CHECK(cap.notices().empty());
  }
  SECTION("a list key the descriptor does not read") {
    const std::string kdeg =
        dir.write("kdeg.train", "KDEG2B 5 5\nCONFIG " + pot + "\n");
    IgnoredKeyNoticeCapture cap;
    CHECK(refusal(parsed(kdeg)) ==
          "TYPE2B D2_Blip takes 0 KDEG2B values, but KDEG2B has 2 values, set "
          "on line 1 of " + kdeg + ": TYPE2B D2_Blip 2 2 Ta Ta (TYPE2B on "
          "line 4 of " + pot + " (CONFIG on line 2 of " + kdeg + "))");
    INFO(joined(cap.notices()));
    CHECK(cap.notices().empty());
  }
}

/* The same for a single many-body descriptor: RCUTMB below the CONFIG line,
 * the refusal ending with the TYPEMB line of the potential.
 * Before the fix: 'Part of RCUTMB (4.5) is ignored: TYPEMB DM_EAD takes one
 * RCUTMB value, and its cutoff is the largest, 5; ...', accepted.
 * Before the fix that names the TYPE line in a refusal of a count: the
 * refusal ended at "... in the order of the TYPEMB lines". */
TEST_CASE("A line of RCUTMB in a file that includes a single many-body "
          "potential is refused naming both lines",
          "[D_mJoin][potential][config_include][potential_lines]") {
  Scratch dir;
  const std::string pot = dir.write("single_mb.tadah", kSingleMB);
  const std::string below =
      dir.write("below_mb.train", "CONFIG " + pot + "\nRCUTMB 4.5\n");
  IgnoredKeyNoticeCapture cap;
  CHECK(refusal(parsed(below)) ==
        "TYPEMB names 1 descriptor (DM_EAD) but RCUTMB has 2 values, set on "
        "line 6 of " + pot + " (CONFIG on line 1 of " + below +
            ") and on line 2 of " + below +
            ": each descriptor takes one RCUTMB value, in the order of the "
            "TYPEMB lines (TYPEMB on line 4 of " + pot +
            " (CONFIG on line 1 of " + below + "))");
  INFO(joined(cap.notices()));
  CHECK(cap.notices().empty());
}

/* A D2_mJoin potential (D2_Blip 5.3, D2_BP 4.5) included with CONFIG:
 * 'RCUT2B 6' above the CONFIG line would give the descriptors 6 and 5.3,
 * below it 6 would be read by none; a CGRID2B line below it gives the line
 * two values past those its descriptors take. Each is refused, naming the
 * lines that set the key and the three TYPE2B lines of the potential.
 * Before the fix: accepted, 'Part of RCUT2B (4.5) is ignored: TYPE2B names
 * 2 descriptors (D2_Blip, D2_BP), which take its first 2 values; the
 * potential works unchanged without it' (D2_BP's cutoff 5.3), 'Part of
 * RCUT2B (6) ...' and 'Part of CGRID2B (4 5) ...'.
 * Before the fix that names the TYPE line in a refusal of a count: each
 * refusal ended at "... in the order of the TYPE2B lines". */
TEST_CASE("A line of a descriptor key in a file that includes a D2_mJoin "
          "potential is refused naming the lines",
          "[D_mJoin][potential][config_include][potential_lines]") {
  Scratch dir;
  const std::string pot = dir.write("joined.tadah", joined_2b());
  const std::string named =
      "TYPE2B names 2 descriptors (D2_Blip, D2_BP)";
  SECTION("RCUT2B above the CONFIG line") {
    const std::string above =
        dir.write("above.train", "RCUT2B 6.0\nCONFIG " + pot + "\n");
    const std::string by = " (CONFIG on line 2 of " + above + ")";
    const std::string expected =
        named + " but RCUT2B has 3 values, set on line 1 of " + above +
        " and on line 8 of " + pot + by + kOneRcut2B + " (TYPE2B on line 4 of " +
        pot + by + ", on line 5 of " + pot + by + " and on line 6 of " + pot +
        by + ")";
    IgnoredKeyNoticeCapture cap;
    CHECK(refusal(parsed(above)) == expected);
    CHECK(load_refusal(parsed(above)) == expected);
    INFO(joined(cap.notices()));
    CHECK(cap.notices().empty());
  }
  SECTION("RCUT2B below the CONFIG line") {
    const std::string below =
        dir.write("below.train", "CONFIG " + pot + "\nRCUT2B 6.0\n");
    const std::string by = " (CONFIG on line 1 of " + below + ")";
    IgnoredKeyNoticeCapture cap;
    CHECK(refusal(parsed(below)) ==
          named + " but RCUT2B has 3 values, set on line 8 of " + pot + by +
              " and on line 2 of " + below + kOneRcut2B +
              " (TYPE2B on line 4 of " + pot + by + ", on line 5 of " + pot +
              by + " and on line 6 of " + pot + by + ")");
    INFO(joined(cap.notices()));
    CHECK(cap.notices().empty());
  }
  SECTION("CGRID2B below the CONFIG line") {
    const std::string grid =
        dir.write("grid.train", "CONFIG " + pot + "\nCGRID2B 4 5\n");
    const std::string by = " (CONFIG on line 1 of " + grid + ")";
    IgnoredKeyNoticeCapture cap;
    CHECK(refusal(parsed(grid)) ==
          named + ", which take 4 CGRID2B values, but CGRID2B has 6 values, "
                  "set on line 9 of " + pot + by + " and on line 2 of " +
              grid +
              ": each descriptor takes its CGRID2B values, in the order of "
              "the TYPE2B lines (TYPE2B on line 4 of " + pot + by +
              ", on line 5 of " + pot + by + " and on line 6 of " + pot + by +
              ")");
    INFO(joined(cap.notices()));
    CHECK(cap.notices().empty());
  }
}

/* A DM_mJoin potential (two DM_EAD) included with CONFIG: an RCUTMB line
 * below the CONFIG line and an RCTYPEMB line above it are refused naming
 * the lines and the three TYPEMB lines of the potential.
 * Before the fix: accepted, 'Part of RCUTMB (6) is ignored: ...' and 'Part
 * of RCTYPEMB (Cut_Cos) is ignored: ...' (the descriptors took Cut_Tanh and
 * the potential's first Cut_Cos).
 * Before the fix that names the TYPE line in a refusal of a count: each
 * refusal ended at "... in the order of the TYPEMB lines". */
TEST_CASE("A line of a descriptor key in a file that includes a DM_mJoin "
          "potential is refused naming the lines",
          "[D_mJoin][potential][config_include][potential_lines]") {
  Scratch dir;
  const std::string pot = dir.write("joined_mb.tadah", kJoinedMB);
  const std::string named = "TYPEMB names 2 descriptors (DM_EAD, DM_EAD)";
  SECTION("RCUTMB below the CONFIG line") {
    const std::string below =
        dir.write("below_mb.train", "CONFIG " + pot + "\nRCUTMB 6\n");
    const std::string by = " (CONFIG on line 1 of " + below + ")";
    IgnoredKeyNoticeCapture cap;
    CHECK(refusal(parsed(below)) ==
          named + " but RCUTMB has 3 values, set on line 8 of " + pot + by +
              " and on line 2 of " + below +
              ": each descriptor takes one RCUTMB value, in the order of the "
              "TYPEMB lines (TYPEMB on line 4 of " + pot + by +
              ", on line 5 of " + pot + by + " and on line 6 of " + pot + by +
              ")");
    INFO(joined(cap.notices()));
    CHECK(cap.notices().empty());
  }
  SECTION("RCTYPEMB above the CONFIG line") {
    const std::string above =
        dir.write("above_mb.train", "RCTYPEMB Cut_Tanh\nCONFIG " + pot + "\n");
    const std::string by = " (CONFIG on line 2 of " + above + ")";
    IgnoredKeyNoticeCapture cap;
    CHECK(refusal(parsed(above)) ==
          named + " but RCTYPEMB has 3 values, set on line 1 of " + above +
              " and on line 7 of " + pot + by +
              ": each descriptor takes one RCTYPEMB value, in the order of "
              "the TYPEMB lines (TYPEMB on line 4 of " + pot + by +
              ", on line 5 of " + pot + by + " and on line 6 of " + pot + by +
              ")");
    INFO(joined(cap.notices()));
    CHECK(cap.notices().empty());
  }
}

/* A TYPE2B line below the CONFIG line of a D2_mJoin potential of tadah
 * 1.5.0 (RCUT2B 5.3 4.5 6 7, two values past those its two descriptors
 * take) adds a third descriptor, with its RCTYPE2B, CGRID2B and SGRID2B
 * lines: the potential's RCUT2B values are then read by descriptors that
 * are not the potential's own, as in a configuration, and four values for
 * three descriptors are refused naming the potential's line and the
 * TYPE2B lines, the potential's three and the line of ext.train.
 * Before the fix: accepted with 'Part of RCUT2B (7) is ignored: TYPE2B
 * names 3 descriptors (D2_Blip, D2_BP, D2_BP), which take its first 3
 * values; the potential works unchanged without it', the third descriptor
 * taking the potential's unread 6 as its cutoff.
 * Before the fix that names the TYPE line in a refusal of a count: the
 * refusal ended at "... in the order of the TYPE2B lines". */
TEST_CASE("A descriptor that a file including a potential adds to its line "
          "reads the potential's values as a configuration does",
          "[D_mJoin][potential][config_include][potential_lines]") {
  Scratch dir;
  std::string body = joined_2b(true);
  body.replace(body.find("RCUT2B 5.3 4.5 6\n"), 17, "RCUT2B 5.3 4.5 6 7\n");
  const std::string pot = dir.write("old2.tadah", body);
  const std::string ext = dir.write(
      "ext.train", "CONFIG " + pot +
                       "\nTYPE2B D2_BP 2 2 Ta Ta\nRCTYPE2B Cut_Cos\n"
                       "CGRID2B 2 3\nSGRID2B 1 1\n");
  IgnoredKeyNoticeCapture cap;
  const std::string by = " (CONFIG on line 1 of " + ext + ")";
  CHECK(refusal(parsed(ext)) ==
        "TYPE2B names 3 descriptors (D2_Blip, D2_BP, D2_BP) but RCUT2B has 4 "
        "values, set on line 8 of " + pot + by + kOneRcut2B +
            " (TYPE2B on line 4 of " + pot + by + ", on line 5 of " + pot +
            by + ", on line 6 of " + pot + by + " and on line 2 of " + ext +
            ")");
  INFO(joined(cap.notices()));
  CHECK(cap.notices().empty());
}

/* Pins: the values of a potential's own lines that its descriptors do not
 * take are reported and ignored, read alone (tadah predict -p, pair_style
 * tadah) or included with CONFIG by a file whose lines set other keys
 * (training from the potential): a D2_mJoin potential of tadah 1.5.0 with
 * RCUT2B 5.3 4.5 6 loads with 'Part of RCUT2B (6) is ignored: TYPE2B names
 * 2 descriptors (D2_Blip, D2_BP), which take its first 2 values; the
 * potential works unchanged without it', and training from it keeps
 * RCUT2B 5.3 4.5; a single D2_Blip potential with RCUT2B 5 6 loads with its
 * largest cutoff, 6.
 * Before the fix: passes as well. */
TEST_CASE("A potential's own values that its descriptors do not take are "
          "reported, read alone or included with CONFIG",
          "[D_mJoin][potential][config_include][potential_lines]") {
  Scratch dir;
  const std::string pot = dir.write("old.tadah", joined_2b(true));
  const std::string notice =
      "Part of RCUT2B (6) is ignored: TYPE2B names 2 descriptors (D2_Blip, "
      "D2_BP), which take its first 2 values; the potential works unchanged "
      "without it";
  SECTION("read alone") {
    IgnoredKeyNoticeCapture cap;
    CHECK(load_refusal(parsed(pot)).empty());
    CHECK(cap.notices() == std::vector<std::string>{notice});
  }
  SECTION("included with CONFIG") {
    const std::string run = dir.write(
        "run.train", "DBFILE db.tadah\nCONFIG " + pot + "\nLAMBDA 0\n");
    IgnoredKeyNoticeCapture cap;
    Context c = parsed(run);
    REQUIRE_NOTHROW(D_mJoin::expand_grids(c));
    CHECK(c("RCUT2B") == std::vector<std::string>{"5.3", "4.5"});
    CHECK(cap.notices() == std::vector<std::string>{notice});
  }
  SECTION("a single descriptor read alone") {
    std::string body = kSingle2B;
    body.replace(body.find("RCUT2B 5\n"), 9, "RCUT2B 5 6\n");
    const std::string single = dir.write("old_single.tadah", body);
    IgnoredKeyNoticeCapture cap;
    tadah::core::PeriodicTable::initialize();
    const Context c = CoreUserConfigParser().parse(single);
    DC_Selector s(c);
    CHECK(s.d2b->get_rcut() == Approx(6.0));
    CHECK(cap.notices() ==
          std::vector<std::string>{
              "Part of RCUT2B (5) is ignored: TYPE2B D2_Blip takes one RCUT2B "
              "value, and its cutoff is the largest, 6; the potential works "
              "unchanged with RCUT2B 6 alone"});
  }
}

/* A training configuration read from a file, without a potential: the
 * refusal of a count names the line that sets the key and its file (a line
 * of no file, its number), and ends with the TYPE2B line, " (TYPE2B on line
 * 4 of two.train)" (" (TYPE2B on line 4)").
 * Before the fix: 'TYPE2B names 1 descriptor (D2_Blip) but RCUT2B has 2
 * values: each descriptor ...' and 'TYPE2B D2_Blip takes 2 CGRID2B values,
 * but CGRID2B has 3 values: TYPE2B D2_Blip 2 2 Ta Ta', without the line.
 * Before the fix that names the TYPE line in a refusal of a count: the
 * refusals ended at "... in the order of the TYPE2B lines" and at the
 * quoted TYPE2B line. */
TEST_CASE("The refusal of a count of a training configuration names the "
          "lines of the key",
          "[D_mJoin][list_keys][potential_lines]") {
  Scratch dir;
  std::string body = kSingle2B;
  body.erase(body.find("WEIGHTS"));
  SECTION("RCUT2B") {
    std::string two = body;
    two.replace(two.find("RCUT2B 5\n"), 9, "RCUT2B 5 6\n");
    const std::string cfg = dir.write("two.train", two);
    CHECK(refusal(parsed(cfg)) ==
          "TYPE2B names 1 descriptor (D2_Blip) but RCUT2B has 2 values, set "
          "on line 6 of " + cfg + kOneRcut2B + " (TYPE2B on line 4 of " +
              cfg + ")");
  }
  SECTION("CGRID2B, by lines of no file") {
    std::vector<std::string> lines;
    for (std::size_t at = 0, end; (end = body.find('\n', at)) !=
                                  std::string::npos;
         at = end + 1)
      lines.push_back(body.substr(at, end - at));
    lines.push_back("CGRID2B 4");
    tadah::core::PeriodicTable::initialize();
    CHECK(refusal(CoreUserConfigParser().parse(lines)) ==
          "TYPE2B D2_Blip takes 2 CGRID2B values, but CGRID2B has 3 values, "
          "set on line 7 and on line 9: TYPE2B D2_Blip 2 2 Ta Ta (TYPE2B "
          "on line 4)");
  }
}

/* Pins: a refusal of a descriptor of a TYPE line read from a file ends with
 * the lines of the TYPE key, as an error of a line names it: " (TYPE2B on
 * line 4 of <file>)", every line of a D2_mJoin, " (TYPE2B on line 1 of
 * <inc> (CONFIG on line 1 of <run>), on line 2 of <inc> (CONFIG on line 1
 * of <run>) and on line 3 of <inc> (CONFIG on line 1 of <run>))":
 *  - a list key the descriptor's SIZE asks for that no line sets ("TYPE2B
 *    D2_Blip takes 2 CGRID2B values and 2 SGRID2B values, but CGRID2B and
 *    SGRID2B are not set: TYPE2B D2_Blip 2 2 Ta Ta"), alone on its line and
 *    in a D2_mJoin of an included file;
 *  - a SIZE too large for the program ("...: parameter 1, the number of
 *    CGRID2B values, is too large: 99999999999999999999");
 *  - a SIZE that is not a whole number ("TYPE2B D2_Blip takes 2 parameters
 *    before its element pairs; parameter 2, the number of SGRID2B values,
 *    must be a non-negative integer, not \"Ta\": TYPE2B D2_Blip 2 Ta Ta").
 * Before the fix: each refusal ended at the quoted TYPE line, naming
 * neither its line nor its file. */
TEST_CASE("A refusal of a descriptor of a TYPE line names the lines of the "
          "TYPE key",
          "[D_mJoin][config_include][type_lines]") {
  Scratch dir;
  const std::string head = "ATOM Ta\nINIT2B true\nINITMB false\n";
  const std::string cut = "RCTYPE2B Cut_Cos\nRCUT2B 5\n";
  SECTION("a list key not set") {
    const std::string one =
        dir.write("one.train", head + "TYPE2B D2_Blip 2 2 Ta Ta\n" + cut);
    CHECK(refusal(parsed(one)) ==
          "TYPE2B D2_Blip takes 2 CGRID2B values and 2 SGRID2B values, but "
          "CGRID2B and SGRID2B are not set: TYPE2B D2_Blip 2 2 Ta Ta "
          "(TYPE2B on line 4 of " + one + ")");
  }
  SECTION("a list key not set, in a D2_mJoin of an included file") {
    const std::string inc = dir.write(
        "inc.train", "TYPE2B D2_mJoin\nTYPE2B D2_Blip 2 2 Ta Ta\n"
                     "TYPE2B D2_BP 2 2 Ta Ta\n"
                     "RCTYPE2B Cut_Cos Cut_Cos\nRCUT2B 5 5\n");
    const std::string run =
        dir.write("run.train", "CONFIG " + inc + "\n" + head);
    const std::string by = " (CONFIG on line 1 of " + run + ")";
    CHECK(refusal(parsed(run)) ==
          "TYPE2B D2_Blip takes 2 CGRID2B values and 2 SGRID2B values, but "
          "CGRID2B and SGRID2B are not set: TYPE2B D2_Blip 2 2 Ta Ta "
          "(TYPE2B on line 1 of " + inc + by + ", on line 2 of " + inc +
              by + " and on line 3 of " + inc + by + ")");
  }
  SECTION("a SIZE too large") {
    const std::string big = dir.write(
        "big.train",
        head + "TYPE2B D2_Blip 99999999999999999999 2 Ta Ta\n" + cut);
    CHECK(refusal(parsed(big)) ==
          "TYPE2B D2_Blip 99999999999999999999 2 Ta Ta: parameter 1, the "
          "number of CGRID2B values, is too large: 99999999999999999999 "
          "(TYPE2B on line 4 of " + big + ")");
  }
  SECTION("a SIZE that is not a whole number") {
    const std::string word = dir.write(
        "word.train", head + "TYPE2B D2_Blip 2 Ta Ta\n" + cut +
                          "CGRID2B 1 2\nSGRID2B 1 1\n");
    CHECK(refusal(parsed(word)) ==
          "TYPE2B D2_Blip takes 2 parameters before its element pairs; "
          "parameter 2, the number of SGRID2B values, must be a non-negative "
          "integer, not \"Ta\": TYPE2B D2_Blip 2 Ta Ta (TYPE2B on line 4 of " +
              word + ")");
  }
}

/* Pins: a refusal of a count of a list key, RCTYPE or RCUT ends with the
 * lines of the TYPE key, as the other refusals of a descriptor do, so that
 * a TYPE line of an included file is located: with "TYPE2B D2_Blip 2 2 Ta
 * Ta" on line 1 of inc.train, which line 4 of run.train includes with
 * CONFIG,
 *  - "RCUT2B 5 6": "TYPE2B names 1 descriptor (D2_Blip) but RCUT2B has 2
 *    values, set on line 5 of run.train: each descriptor takes one RCUT2B
 *    value, in the order of the TYPE2B lines (TYPE2B on line 1 of inc.train
 *    (CONFIG on line 4 of run.train))";
 *  - "CGRID2B 2", fewer values than the line takes: "TYPE2B D2_Blip takes 2
 *    CGRID2B values, but CGRID2B has 1 value, set on line 6 of run.train:
 *    TYPE2B D2_Blip 2 2 Ta Ta (TYPE2B on line 1 of inc.train (CONFIG on line
 *    4 of run.train))";
 *  - "CGRID2B 2 3 4", more: "... but CGRID2B has 3 values, set on line 6 of
 *    run.train: TYPE2B D2_Blip 2 2 Ta Ta (TYPE2B on line 1 of ...)";
 * and with a D2_mJoin of D2_Blip 2 2 and D2_BP 1 1 on lines 1 to 3 of
 * incj.train, "CGRID2B 2 3 4 5": "TYPE2B names 2 descriptors (D2_Blip,
 * D2_BP), which take 3 CGRID2B values, but CGRID2B has 4 values, set on
 * line 6 of runj.train: each descriptor takes its CGRID2B values, in the
 * order of the TYPE2B lines (TYPE2B on line 1 of incj.train (CONFIG on line
 * 4 of runj.train), on line 2 ... and on line 3 ...)", and "CGRID2B 2 3":
 * "TYPE2B D2_BP (descriptor 2 of 2) takes 1 CGRID2B value, but CGRID2B has
 * 0 values left of 2 (the descriptors before it take 2), set on line 6 of
 * runj.train: TYPE2B D2_BP 1 1 Ta Ta (TYPE2B on line 1 of ...)".
 * Before the fix: each refusal ended at the quoted TYPE2B line or at "...
 * in the order of the TYPE2B lines", naming neither the line nor the file
 * of the TYPE2B line. */
TEST_CASE("A refusal of a count of a descriptor key names the TYPE line of "
          "an included file",
          "[D_mJoin][config_include][type_lines][potential_lines]") {
  Scratch dir;
  const std::string head = "ATOM Ta\nINIT2B true\nINITMB false\n";
  const std::string inc =
      dir.write("inc.train", "TYPE2B D2_Blip 2 2 Ta Ta\nRCTYPE2B Cut_Cos\n");
  auto run = [&](const std::string &lines) {
    return dir.write("run.train", head + "CONFIG " + inc + "\n" + lines);
  };
  const std::string single =
      " (TYPE2B on line 1 of " + inc + " (CONFIG on line 4 of " + run("") +
      "))";
  SECTION("RCUT2B") {
    const std::string r = run("RCUT2B 5 6\nCGRID2B 2 3\nSGRID2B 1 1\n");
    CHECK(refusal(parsed(r)) ==
          "TYPE2B names 1 descriptor (D2_Blip) but RCUT2B has 2 values, set "
          "on line 5 of " + r + kOneRcut2B + single);
  }
  SECTION("fewer CGRID2B values than the line takes") {
    const std::string r = run("RCUT2B 5\nCGRID2B 2\nSGRID2B 1 1\n");
    CHECK(refusal(parsed(r)) ==
          "TYPE2B D2_Blip takes 2 CGRID2B values, but CGRID2B has 1 value, "
          "set on line 6 of " + r + ": TYPE2B D2_Blip 2 2 Ta Ta" + single);
  }
  SECTION("more CGRID2B values than the line takes") {
    const std::string r = run("RCUT2B 5\nCGRID2B 2 3 4\nSGRID2B 1 1\n");
    CHECK(refusal(parsed(r)) ==
          "TYPE2B D2_Blip takes 2 CGRID2B values, but CGRID2B has 3 values, "
          "set on line 6 of " + r + ": TYPE2B D2_Blip 2 2 Ta Ta" + single);
  }
  const std::string incj = dir.write(
      "incj.train", "TYPE2B D2_mJoin\nTYPE2B D2_Blip 2 2 Ta Ta\n"
                    "TYPE2B D2_BP 1 1 Ta Ta\nRCTYPE2B Cut_Cos Cut_Cos\n");
  auto runj = [&](const std::string &lines) {
    return dir.write("runj.train", head + "CONFIG " + incj + "\n" + lines);
  };
  const std::string runj_path = runj("");
  const std::string by = " (CONFIG on line 4 of " + runj_path + ")";
  const std::string joined_on = " (TYPE2B on line 1 of " + incj + by +
                                ", on line 2 of " + incj + by +
                                " and on line 3 of " + incj + by + ")";
  SECTION("more CGRID2B values than a D2_mJoin line takes") {
    const std::string r =
        runj("RCUT2B 5 5\nCGRID2B 2 3 4 5\nSGRID2B 1 1 1\n");
    CHECK(refusal(parsed(r)) ==
          "TYPE2B names 2 descriptors (D2_Blip, D2_BP), which take 3 CGRID2B "
          "values, but CGRID2B has 4 values, set on line 6 of " + r +
              ": each descriptor takes its CGRID2B values, in the order of "
              "the TYPE2B lines" + joined_on);
  }
  SECTION("fewer CGRID2B values than a D2_mJoin line takes") {
    const std::string r = runj("RCUT2B 5 5\nCGRID2B 2 3\nSGRID2B 1 1 1\n");
    CHECK(refusal(parsed(r)) ==
          "TYPE2B D2_BP (descriptor 2 of 2) takes 1 CGRID2B value, but "
          "CGRID2B has 0 values left of 2 (the descriptors before it take 2), "
          "set on line 6 of " + r + ": TYPE2B D2_BP 1 1 Ta Ta" + joined_on);
  }
}

/* Pins: a TYPE line a program builds without element pairs, or with an odd
 * number of element types, is refused naming the descriptor, impersonally:
 * "TYPE2B D2_Blip: the line names no element types; a TYPE2B line ends
 * with its element pairs, such as Ta Ta" and "TYPE2B D2_Blip: the element
 * types come in pairs, and the line has 3 of them, Ta Ta W" (a context of
 * no file: no line is named). The parser of a configuration file refuses
 * such a line first (the TYPE2B schema).
 * Before the fix: "Error: No element types specified. Please provide
 * element types for this calculator. For example: ..." and "Error: Element
 * types must be provided in pairs. Ensure each element is paired
 * correctly. ...". */
TEST_CASE("A TYPE line without its element pairs is refused naming the "
          "descriptor",
          "[D_mJoin][type_lines]") {
  auto built = [](const std::vector<std::string> &type) {
    Context c;
    c.add("ATOM", "Ta");
    c.add("INIT2B", "true");
    c.add("INITMB", "false");
    for (const auto &t : type) c.add("TYPE2B", t);
    c.add("RCTYPE2B", "Cut_Cos");
    c.add("RCUT2B", "5");
    c.add("CGRID2B", "1");
    c.add("CGRID2B", "2");
    c.add("SGRID2B", "1");
    c.add("SGRID2B", "1");
    return c;
  };
  CHECK(refusal(built({"D2_Blip", "2", "2"})) ==
        "TYPE2B D2_Blip: the line names no element types; a TYPE2B line "
        "ends with its element pairs, such as Ta Ta");
  CHECK(refusal(built({"D2_Blip", "2", "2", "Ta", "Ta", "W"})) ==
        "TYPE2B D2_Blip: the element types come in pairs, and the line has "
        "3 of them, Ta Ta W");
}

/* ------------------------------------------------------------------------
 * A key of a potential's descriptors set on two lines of the potential
 * file, or TYPE lines other than those tadah writes: a command that reads
 * the potential with -c (tadah train, tadah hpo, tadah refit and the
 * analysis commands; the blueprint name of the command given) refuses it,
 * naming the lines; a potential read for prediction is read with the
 * values of all its lines, with the WARNING of values its descriptors do
 * not take.
 * ---------------------------------------------------------------------- */
namespace {

/* What D_mJoin::check_potential_lines(@p c, @p command) throws; empty when
 * it throws nothing. */
std::string lines_refusal(const Context &c,
                          const std::string &command = "TRAIN") {
  try {
    D_mJoin::check_potential_lines(c, command);
  } catch (const std::exception &e) {
    return e.what();
  }
  return "";
}

/* The refusal of @p key set on @p lines (each "line 6 of copy.tadah") of
 * one potential, read by @p words ("tadah train"). */
std::string two_lines(const std::string &key,
                      const std::vector<std::string> &lines,
                      const std::string &words = "tadah train") {
  std::string on;
  for (std::size_t i = 0; i < lines.size(); ++i)
    on += (i == 0 ? "" : i + 1 == lines.size() ? " and on " : ", on ") +
          lines[i];
  return "Key " + key + " is set on " + on +
         ", lines of one potential (a file with WEIGHTS): " + words +
         " reads a key of a potential's descriptors from the line the "
         "potential has for it, and a potential's value is changed by "
         "editing its line in a copy of the potential (the Training page of "
         "the documentation, Retraining from a potential)";
}

/* How the refusal of a TYPE key @p key, read by @p words, says that a
 * potential has its lines: those tadah writes, which follow one another, a
 * new line at each descriptor's name and, when the key names a DESCRIPTOR
 * block (@p block, "@ta"), at each block's ID. */
std::string type_rule(const std::string &key, const std::string &words,
                      const std::string &block) {
  return words + " reads the " + key +
         " key of a potential from the lines tadah writes for it, which "
         "follow one another, a new line at each descriptor's name" +
         (block.empty() ? std::string()
                        : " and at each DESCRIPTOR block's ID (" + block +
                              ")") +
         ", and a potential's value is changed by editing its line in a copy "
         "of the potential (the Training page of the documentation, "
         "Retraining from a potential)";
}

/* The refusal of the TYPE key @p key of a potential whose first lines,
 * @p written ("lines 4 to 6"), are as tadah writes them, and whose lines
 * @p outside (each "line 12 of jtype.tadah") are outside them, read by
 * @p words. */
std::string outside_lines(const std::string &key,
                          const std::vector<std::string> &outside,
                          const std::string &written,
                          const std::string &words = "tadah train",
                          const std::string &block = "") {
  std::string on;
  for (std::size_t i = 0; i < outside.size(); ++i)
    on += (i == 0 ? "" : i + 1 == outside.size() ? " and " : ", ") +
          outside[i];
  on[0] = 'L';
  const bool one = written.rfind("line ", 0) == 0;
  return on +
         (outside.size() == 1 ? " is a " + key + " line"
                              : " are " + key + " lines") +
         " of a potential (a file with WEIGHTS) outside its " + key +
         (one ? " line (" : " lines (") + written +
         "): " + type_rule(key, words, block);
}

/* The refusal of the TYPE key @p key set on @p lines of one potential whose
 * first lines are not those tadah writes, read by @p words. */
std::string type_lines(const std::string &key,
                       const std::vector<std::string> &lines,
                       const std::string &words = "tadah train",
                       const std::string &block = "") {
  std::string on;
  for (std::size_t i = 0; i < lines.size(); ++i)
    on += (i == 0 ? "" : i + 1 == lines.size() ? " and on " : ", on ") +
          lines[i];
  return "Key " + key + " is set on " + on +
         ", lines of one potential (a file with WEIGHTS): " +
         type_rule(key, words, block);
}

/* A DM_Embed block of the ID @p id, of PAIRS Ta Ta. */
std::string embed_block(const std::string &id) {
  return "DESCRIPTOR " + id + " DM_Embed\n"
         "  PAIRS Ta Ta\n"
         "  DENSITY\n"
         "    BASIS Blip5\n"
         "    FORM NONNEG\n"
         "    CENTRES 1.0 2.5 4.0\n"
         "    ETA 1.5 1.5 1.5\n"
         "    AMPLITUDES 0.4 0.2 0.1\n"
         "    CUTOFF\n"
         "      TYPE Cut_Poly\n"
         "      RADIUS 5.0\n"
         "    ENDCUTOFF\n"
         "  ENDDENSITY\n"
         "  EMBEDDING\n"
         "    BASIS Gaussian\n"
         "    CENTRES 0.0 0.5 1.0 1.5\n"
         "    WIDTHS 0.5 0.5 0.5 0.5\n"
         "    REFERENCE ZERO\n"
         "  ENDEMBEDDING\n"
         "ENDDESCRIPTOR\n";
}

/* A potential of the TYPEMB lines @p typemb, which start on line 4, and
 * the DM_Embed blocks of the IDs @p ids. By default a DM_Embed block
 * placed before a DM_Blip descriptor, its TYPEMB lines 4 and 5 as tadah
 * writes them: "TYPEMB @ta" then "TYPEMB DM_Blip 0 1 1 Ta Ta". The keys of
 * the descriptor (RCTYPEMB, RCUTMB 4.5, CGRIDMB 2.8, SGRIDMB 1) follow
 * when @p descriptor. */
std::string block_potential(
    const std::string &typemb = "TYPEMB @ta\nTYPEMB DM_Blip 0 1 1 Ta Ta\n",
    const std::vector<std::string> &ids = {"ta"}, bool descriptor = true) {
  std::string pot = "ATOM Ta\n"
                    "INIT2B false\n"
                    "INITMB true\n" +
                    typemb;
  if (descriptor)
    pot += "RCTYPEMB Cut_Cos\n"
           "RCUTMB 4.5\n"
           "CGRIDMB 2.8\n"
           "SGRIDMB 1\n";
  for (const auto &id : ids) pot += embed_block(id);
  return pot + "WEIGHTS 0.1 0.2 0.3 0.4 0.5\n";
}

/* The number of lines of @p text. */
std::size_t line_count(const std::string &text) {
  return static_cast<std::size_t>(
      std::count(text.begin(), text.end(), '\n'));
}

}  // namespace

/* Pins: a copy of the single D2_Blip potential (RCUT2B 5 on line 6) with
 * "RCUT2B 6" appended (line 10):
 *  - D_mJoin::check_potential_lines refuses it for tadah train ("Key RCUT2B
 *    is set on line 6 of <copy> and on line 10 of <copy>, lines of one
 *    potential (a file with WEIGHTS): tadah train reads a key of a
 *    potential's descriptors from the line the potential has for it, and a
 *    potential's value is changed by editing its line in a copy of the
 *    potential (the Training page of the documentation, Retraining from a
 *    potential)"), and for tadah hpo naming tadah hpo;
 *  - included with CONFIG by run.train, the lines name the CONFIG line;
 *  - a model built from it for prediction (DC_Selector) loads, with the
 *    WARNING "Part of RCUT2B (5) is ignored: TYPE2B D2_Blip takes one RCUT2B
 *    value, and its cutoff is the largest, 6; ...";
 *  - "RCUT2B 5 6" on its one line (a potential of tadah 1.5.0), the D2_mJoin
 *    potential with its three TYPE2B lines (4 to 6), with RCUT2B 5.3 4.5 6
 *    on one line, a training configuration (no WEIGHTS) with two RCUT2B
 *    lines (refused by its count, test above) and a potential with two
 *    DBFILE lines (a key of no descriptor) pass the check;
 *  - the TYPE lines tadah writes, a new line at each descriptor's name and
 *    at each DESCRIPTOR block's ID, pass the check: "TYPEMB @ta" then
 *    "TYPEMB DM_Blip 0 1 1 Ta Ta" (a block placed before a descriptor),
 *    "TYPEMB DM_EAD 0 1 1 Ta Ta" then "TYPEMB @ta" (after it), and "TYPEMB
 *    @ta" then "TYPEMB @tb" (two blocks);
 *  - a comment line and a blank line between the TYPE2B lines of the
 *    D2_mJoin potential, and a comment between its TYPEMB lines "@ta" and
 *    "DM_Blip 0 1 1 Ta Ta", pass the check;
 *  - a TYPE line outside the first lines of the key, which are as tadah
 *    writes them, is refused, naming it and those lines and saying how a
 *    potential has its TYPE lines ("Line 12 of <file> is a TYPE2B line of a
 *    potential (a file with WEIGHTS) outside its TYPE2B lines (lines 4 to
 *    6): tadah train reads the TYPE2B key of a potential from the lines
 *    tadah writes for it, which follow one another, a new line at each
 *    descriptor's name, and a potential's value is changed by editing its
 *    line in a copy of the potential (...)"): "TYPE2B D2_Blip 2 2 Ta W"
 *    appended to the single potential (line 10, outside line 4), "W Ta W"
 *    (a line that starts with no descriptor) and "D2_LJ Ta Ta" (a
 *    descriptor) appended to the D2_mJoin potential (line 12), its D2_BP
 *    line moved below the RCTYPE2B line (line 7, outside lines 4 and 5),
 *    and "TYPEMB DM_EAD 0 1 1 Ta Ta" appended to the potential of a block,
 *    whose refusal adds "and at each DESCRIPTOR block's ID (@ta)";
 *  - TYPE lines one after another that are not those tadah writes, "TYPE2B
 *    W Ta W" inserted below the TYPE2B line of the single potential (lines
 *    4 and 5), are refused naming both ("Key TYPE2B is set on line 4 of
 *    <file> and on line 5 of <file>, lines of one potential (a file with
 *    WEIGHTS): tadah train reads the TYPE2B key of a potential from the
 *    lines tadah writes for it, ...");
 *  - a CGRIDMB line appended to the single DM_EAD potential (CGRIDMB on
 *    line 7) is refused, naming both lines.
 * Before the fix: does not compile (no D_mJoin::check_potential_lines): a
 * training command read the copy as the 1.5.0 potentials whose one line
 * has values its descriptors do not take, with the WARNING, and trained a
 * cutoff of 6.
 * Before the fix of the TYPE lines (the check accepted lines of a TYPE key
 * whose first value named D2_mJoin or DM_mJoin and whose later lines each
 * started with a descriptor's name, wherever they were): "D2_LJ Ta Ta"
 * appended at line 12 and the D2_BP line moved below RCTYPE2B passed the
 * check, and "TYPEMB @ta"
 * then "TYPEMB DM_Blip 0 1 1 Ta Ta", which tadah writes, was refused as two
 * lines of TYPEMB ("Key TYPEMB is set on line 4 of <file> and on line 5 of
 * <file>, ... (a DM_mJoin line has one line per descriptor, each starting
 * with the descriptor's name), ..."); every refusal of a TYPE key said "(a
 * D2_mJoin line has one line per descriptor, each starting with the
 * descriptor's name)".
 * Before the fix of the block IDs, the key lines and the refusal of a TYPE
 * line: "TYPEMB DM_EAD 0 1 1 Ta Ta" then "TYPEMB @ta", "TYPEMB @ta" then
 * "TYPEMB @tb", and the TYPE lines with a comment or a blank line between
 * them were refused as two lines of the key; a refusal of a TYPE key named
 * every line of the key ("Key TYPE2B is set on line 4 of <file>, on line 5
 * ..., on line 6 ... and on line 12 of <file>, ...: tadah train reads a key
 * of a potential's descriptors from the line the potential has for it (the
 * TYPE2B lines of a potential follow one another, a new line at each
 * descriptor's name, as tadah writes them), ..."). */
TEST_CASE("A key of a descriptor set on two lines of one potential file is "
          "refused by a command that trains from it",
          "[D_mJoin][potential][potential_lines][retrain]") {
  Scratch dir;
  const std::string copy =
      dir.write("copy.tadah", std::string(kSingle2B) + "RCUT2B 6\n");
  SECTION("an RCUT2B line appended to a copy") {
    CHECK(lines_refusal(parsed(copy)) ==
          two_lines("RCUT2B", {"line 6 of " + copy, "line 10 of " + copy}));
    CHECK(lines_refusal(parsed(copy), "HPO") ==
          two_lines("RCUT2B", {"line 6 of " + copy, "line 10 of " + copy},
                    "tadah hpo"));
    IgnoredKeyNoticeCapture cap;
    CHECK(load_refusal(parsed(copy)).empty());
    CHECK(cap.notices() ==
          std::vector<std::string>{
              "Part of RCUT2B (5) is ignored: TYPE2B D2_Blip takes one RCUT2B "
              "value, and its cutoff is the largest, 6; the potential works "
              "unchanged with RCUT2B 6 alone"});
  }
  SECTION("the copy included with CONFIG") {
    const std::string run =
        dir.write("run.train", "DBFILE db.tadah\nCONFIG " + copy + "\n");
    const std::string by = " (CONFIG on line 2 of " + run + ")";
    CHECK(lines_refusal(parsed(run)) ==
          two_lines("RCUT2B",
                    {"line 6 of " + copy + by, "line 10 of " + copy + by}));
  }
  SECTION("what a potential writes, a configuration and other keys") {
    std::string old = kSingle2B;
    old.replace(old.find("RCUT2B 5\n"), 9, "RCUT2B 5 6\n");
    CHECK(lines_refusal(parsed(dir.write("old.tadah", old))).empty());
    CHECK(lines_refusal(parsed(dir.write("j.tadah", joined_2b()))).empty());
    CHECK(lines_refusal(parsed(dir.write("jo.tadah", joined_2b(true))))
              .empty());
    std::string config = kSingle2B;
    config.erase(config.find("WEIGHTS"));
    CHECK(lines_refusal(parsed(dir.write("two.train",
                                         config + "RCUT2B 6\n")))
              .empty());
    CHECK(lines_refusal(parsed(dir.write(
              "db.tadah", std::string(kSingle2B) +
                              "DBFILE a.tadah\nDBFILE b.tadah\n")))
              .empty());
  }
  SECTION("the TYPE lines tadah writes for blocks and descriptors") {
    CHECK(lines_refusal(parsed(dir.write("block.tadah", block_potential())))
              .empty());
    CHECK(lines_refusal(parsed(dir.write(
              "after.tadah",
              block_potential("TYPEMB DM_EAD 0 1 1 Ta Ta\nTYPEMB @ta\n"))))
              .empty());
    CHECK(lines_refusal(parsed(dir.write(
              "two.tadah", block_potential("TYPEMB @ta\nTYPEMB @tb\n",
                                           {"ta", "tb"}, false))))
              .empty());
  }
  SECTION("comments and blank lines between the TYPE lines") {
    std::string commented = joined_2b();
    commented.insert(commented.find("TYPE2B D2_Blip"), "# the Blip part\n");
    commented.insert(commented.find("TYPE2B D2_BP"), "\n");
    CHECK(lines_refusal(parsed(dir.write("comment.tadah", commented)))
              .empty());
    CHECK(lines_refusal(parsed(dir.write(
              "mbcomment.tadah",
              block_potential("TYPEMB @ta\n  # the descriptor\n"
                              "TYPEMB DM_Blip 0 1 1 Ta Ta\n"))))
              .empty());
  }
  SECTION("a TYPE line appended") {
    const std::string single =
        dir.write("type.tadah",
                  std::string(kSingle2B) + "TYPE2B D2_Blip 2 2 Ta W\n");
    CHECK(lines_refusal(parsed(single)) ==
          outside_lines("TYPE2B", {"line 10 of " + single}, "line 4"));
    for (const char *line : {"TYPE2B W Ta W\n", "TYPE2B D2_LJ Ta Ta\n"}) {
      const std::string joined =
          dir.write("jtype.tadah", joined_2b() + line);
      CAPTURE(line);
      CHECK(lines_refusal(parsed(joined)) ==
            outside_lines("TYPE2B", {"line 12 of " + joined}, "lines 4 to 6"));
    }
    const std::string block = block_potential();
    const std::string mb = dir.write(
        "mbtype.tadah", block + "TYPEMB DM_EAD 0 1 1 Ta Ta\n");
    CHECK(lines_refusal(parsed(mb)) ==
          outside_lines("TYPEMB",
                        {"line " + std::to_string(line_count(block) + 1) +
                         " of " + mb},
                        "lines 4 and 5", "tadah train", "@ta"));
  }
  SECTION("TYPE2B lines tadah does not write") {
    std::string moved = joined_2b();
    moved.erase(moved.find("TYPE2B D2_BP 2 2 Ta Ta\n"), 23);
    moved.insert(moved.find("RCUT2B"), "TYPE2B D2_BP 2 2 Ta Ta\n");
    const std::string apart = dir.write("apart.tadah", moved);
    CHECK(lines_refusal(parsed(apart)) ==
          outside_lines("TYPE2B", {"line 7 of " + apart}, "lines 4 and 5"));
    std::string split = kSingle2B;
    split.insert(split.find("RCTYPE2B"), "TYPE2B W Ta W\n");
    const std::string next = dir.write("next.tadah", split);
    CHECK(lines_refusal(parsed(next)) ==
          type_lines("TYPE2B", {"line 4 of " + next, "line 5 of " + next}));
  }
  SECTION("a CGRIDMB line appended") {
    const std::string mb =
        dir.write("mb.tadah", std::string(kSingleMB) + "CGRIDMB 4\n");
    CHECK(lines_refusal(parsed(mb)) ==
          two_lines("CGRIDMB", {"line 7 of " + mb, "line 10 of " + mb}));
  }
}
