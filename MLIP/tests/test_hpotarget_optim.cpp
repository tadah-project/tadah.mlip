// Catch2 v2 tests of the reader of the OPTIM lines of a target file against
// a training configuration (tadah/mlip/hpotarget_optim.h): the boxes of the
// values a line selects (explicit, or the AUTO box tadah hpo derives), the
// slot of each value, a DM_Embed component path, FIXWEIGHT coefficients,
// the order of the refusals and the cross-line rule. tadah hpo reads its
// OPTIM lines with it and tadah analysis check_bounds its target file; HPO's
// registry conformance cases compare it with tadah hpo on every registry
// case.
//
// Tag [hpotarget_optim]. The file compiles with or without the header
// (__has_include): without it it holds one case, which fails.

#include "catch2/catch.hpp"

#if __has_include(<tadah/mlip/hpotarget_optim.h>)

#include <tadah/core/context.h>
#include <tadah/core/core_user_config_parser.h>
#include <tadah/core/periodic_table.h>
#include <tadah/core/number_format.h>
#include <tadah/mlip/engines/hpotarget_writer.h>
#include <tadah/mlip/hpotarget_optim.h>

#include <sstream>
#include <string>
#include <utility>
#include <vector>

using tadah::core::Context;
using namespace tadah::mlip;

namespace {

/* D2_Blip 4 4 Ta Ta, CGRID2B 2 3 4 5, SGRID2B 1 1 1 1 (a radial bump: its
 * widths > 0), RCUT2B 5.5. */
Context blip_seed() {
  tadah::core::PeriodicTable::initialize();
  Context c;
  c.add("ATOM", "Ta");
  c.add("INIT2B", "true");
  c.add("INITMB", "false");
  c.add("TYPE2B", "D2_Blip 4 4 Ta Ta");
  c.add("CGRID2B", "2.0 3.0 4.0 5.0");
  c.add("SGRID2B", "1.0 1.0 1.0 1.0");
  c.add("RCTYPE2B", "Cut_Cos");
  c.add("RCUT2B", 5.5);
  c.add("VERBOSE", 0);
  return c;
}

/* D2_Blip5 + DM_REAM (Blip5 density) under RHOFORM NONNEG, AMPGRIDMB
 * 0.5 0 0.1. */
Context ream_nonneg_seed() {
  tadah::core::PeriodicTable::initialize();
  Context c;
  c.add("ATOM", "Ta");
  c.add("WATOM", 73);
  c.add("INIT2B", "true");
  c.add("INITMB", "true");
  c.add("TYPE2B", "D2_Blip5 4 4 Ta Ta");
  c.add("CGRID2B", "2.5 3.2 3.9 4.5");
  c.add("SGRID2B", "3.0 3.0 3.0 3.0");
  c.add("TYPEMB", "DM_REAM 3 3 3 3 5 5 0 0 Ta Ta");
  c.add("CGRIDMB", "2.8 3.6 4.4");
  c.add("SGRIDMB", "3.0 3.0 3.0");
  c.add("AMPGRIDMB", "0.5 0 0.1");
  c.add("RHOFORM", "NONNEG");
  c.add("CEMBFUNC", "-0.2 0.3 0.8 1.3 1.8");
  c.add("SEMBFUNC", "2.0 2.0 2.0 2.0 2.0");
  c.add("RCTYPE2B", "Cut_Cos");
  c.add("RCTYPEMB", "Cut_Cos");
  c.add("RCUT2B", 5.5);
  c.add("RCUTMB", 5.5);
  c.add("VERBOSE", 0);
  return c;
}

/* One DM_Embed component, ta (PAIRS Ta Ta), density centres 1 2.5 4. */
Context embed_seed() {
  tadah::core::PeriodicTable::initialize();
  std::istringstream text(
      "ATOM Ta\nWATOM 73\nINIT2B false\nINITMB true\nVERBOSE 0\n"
      "DESCRIPTOR ta DM_Embed\nPAIRS Ta Ta\nDENSITY\nBASIS Blip5\n"
      "FORM NONNEG\nCENTRES 1.0 2.5 4.0\nETA 1.5 1.5 1.5\n"
      "AMPLITUDES 0.4 0.2 0.1\nCUTOFF\nTYPE Cut_Poly\nRADIUS 5.0\n"
      "ENDCUTOFF\nENDDENSITY\nEMBEDDING\nBASIS Gaussian\n"
      "CENTRES 0.0 0.5 1.0 1.5\nWIDTHS 0.5 0.5 0.5 0.5\nREFERENCE ZERO\n"
      "ENDEMBEDDING\nENDDESCRIPTOR\n");
  return tadah::core::CoreUserConfigParser().parse(text);
}

/* The OPTIM lines of @p file read against @p c, prepared as tadah hpo
 * prepares it. */
std::vector<OptimTargetLine> read_file(Context &c, const std::string &file) {
  const OptimSeed seed = prepare_optim_seed(c, "tadah hpo");
  std::istringstream in(file);
  return read_optim_lines(in, c, seed);
}

/* The boxes of the one OPTIM line @p line, read against @p c. */
OptimLine read_line(Context c, const std::string &line) {
  const auto l = read_file(c, line + "\n");
  REQUIRE(l.size() == 1);
  return l[0].optim;
}

/* "<line>: <what>" of the refusal of @p file read against @p c, "" when
 * it is read. */
std::string refusal(Context c, const std::string &file) {
  try {
    (void)read_file(c, file);
  } catch (const TargetLineError &e) {
    return std::to_string(e.line) + ": " + e.what();
  } catch (const std::exception &e) {
    return std::string("not a TargetLineError: ") + e.what();
  }
  return "";
}

std::vector<std::pair<double, double>> boxes_of(const OptimLine &l) {
  std::vector<std::pair<double, double>> out;
  for (const auto &b : l.boxes) out.emplace_back(b.lo, b.hi);
  return out;
}

}  // namespace

/* Pins: the AUTO boxes of a line, those tadah hpo derives: CGRID2B 2 3 4 5
 * (sign-free) (1-4) AUTO is the linear FRAC 0.5 box of each seed, [1, 3],
 * [1.5, 4.5], [2, 6], [2.5, 7.5], AutoLinear, slots 0 to 3; SGRID2B 1 1 1 1
 * (positive-only) the geometric RATIO 2 box [0.5, 2] each, AutoGeometric;
 * FRAC 0 FLOOR 0.1 the half-width 0.1; FRAC 0 GROUP_FLOOR 0.1 the
 * half-width 0.1 x 5, the largest seed of the line. Before the fix: the
 * header does not exist. */
TEST_CASE("OPTIM reader: the AUTO boxes are those tadah hpo derives",
          "[hpotarget_optim]") {
  const OptimLine c = read_line(blip_seed(), "OPTIM CGRID2B (1-4) AUTO");
  CHECK(c.key == "CGRID2B");
  CHECK(c.auto_box);
  CHECK(c.line == 1);
  CHECK(c.config == std::vector<std::string>{"2", "3", "4", "5"});
  CHECK(boxes_of(c) == std::vector<std::pair<double, double>>{
                           {1.0, 3.0}, {1.5, 4.5}, {2.0, 6.0}, {2.5, 7.5}});
  for (std::size_t i = 0; i < c.boxes.size(); ++i) {
    CHECK(c.boxes[i].slot == i);
    CHECK(c.boxes[i].index == i + 1);
    CHECK(c.boxes[i].origin == OptimBoxOrigin::AutoLinear);
  }
  const OptimLine s = read_line(blip_seed(), "OPTIM SGRID2B (1-4) AUTO");
  CHECK(boxes_of(s) == std::vector<std::pair<double, double>>(
                           4, std::pair<double, double>{0.5, 2.0}));
  for (const auto &b : s.boxes)
    CHECK(b.origin == OptimBoxOrigin::AutoGeometric);
  const OptimLine f =
      read_line(blip_seed(), "OPTIM CGRID2B (1,4) AUTO FRAC 0 FLOOR 0.1");
  CHECK(boxes_of(f) == std::vector<std::pair<double, double>>{
                           {2.0 - 0.1, 2.0 + 0.1}, {5.0 - 0.1, 5.0 + 0.1}});
  const OptimLine g =
      read_line(blip_seed(), "OPTIM CGRID2B (1,4) AUTO FRAC 0 GROUP_FLOOR 0.1");
  CHECK(boxes_of(g) == std::vector<std::pair<double, double>>{
                           {2.0 - 0.1 * 5.0, 2.0 + 0.1 * 5.0},
                           {5.0 - 0.1 * 5.0, 5.0 + 0.1 * 5.0}});
}

/* Pins: the AUTO boxes clamped at 0: AMPGRIDMB 0.5 0 0.1 under RHOFORM
 * NONNEG takes the group floor 1e-3 of the largest seed, so the 0 seed's
 * box is [0, 5e-4], the 0.5 seed's [0.25, 0.75], AutoLinearNonneg; FSIGMA
 * 0.1 0.05 (1-2) AUTO: the absolute part a (> 0) the geometric box [0.05,
 * 0.2], the relative part b (>= 0) the linear box [0.025, 0.075] clamped
 * at 0, AutoLinearNonneg. Before the fix: the header does not exist. */
TEST_CASE("OPTIM reader: the AUTO boxes of non-negative values",
          "[hpotarget_optim]") {
  const OptimLine a = read_line(ream_nonneg_seed(), "OPTIM AMPGRIDMB (1-2) AUTO");
  REQUIRE(a.boxes.size() == 2);
  CHECK(a.boxes[0].lo == 0.5 - 0.25);
  CHECK(a.boxes[0].hi == 0.5 + 0.25);
  CHECK(a.boxes[1].lo == 0.0);
  CHECK(a.boxes[1].hi == 1e-3 * 0.5);
  for (const auto &b : a.boxes)
    CHECK(b.origin == OptimBoxOrigin::AutoLinearNonneg);
  Context sig = blip_seed();
  sig.add("FSIGMA", "0.1 0.05");
  const OptimLine s = read_line(sig, "OPTIM FSIGMA (1-2) AUTO");
  REQUIRE(s.boxes.size() == 2);
  CHECK(s.boxes[0].lo == 0.1 / 2.0);
  CHECK(s.boxes[0].hi == 0.1 * 2.0);
  CHECK(s.boxes[0].origin == OptimBoxOrigin::AutoGeometric);
  CHECK(s.boxes[1].lo == 0.05 - 0.5 * 0.05);
  CHECK(s.boxes[1].hi == 0.05 + 0.5 * 0.05);
  CHECK(s.boxes[1].origin == OptimBoxOrigin::AutoLinearNonneg);
}

/* Pins: an explicit box applies to each value its index list selects:
 * (1,3) gives slots 0 and 2, the box and its tag (LOG) on each, Explicit;
 * a FIXWEIGHT index names a coefficient, the slot of its FIXINDEX entry
 * (FIXINDEX 3 7: OPTIM FIXWEIGHT (7) is slot 1, index 7); a DM_Embed
 * component path, in any case, is upper-cased and reads the component's
 * values. Before the fix: the header does not exist. */
TEST_CASE("OPTIM reader: index lists, FIXWEIGHT coefficients and component "
          "paths",
          "[hpotarget_optim]") {
  const OptimLine l = read_line(blip_seed(), "OPTIM cgrid2b (1,3) 1.5 4.5 LOG");
  CHECK(l.key == "CGRID2B");
  REQUIRE(l.boxes.size() == 2);
  CHECK(l.boxes[0].slot == 0);
  CHECK(l.boxes[1].slot == 2);
  CHECK(l.boxes[1].index == 3);
  CHECK(l.boxes[1].lo == 1.5);
  CHECK(l.boxes[1].hi == 4.5);
  CHECK(l.boxes[1].origin == OptimBoxOrigin::Explicit);
  CHECK(l.shape == OptimShape::Log);
  CHECK(l.tagged);

  Context fw = blip_seed();
  fw.add("FIXINDEX", "3 7");
  fw.add("FIXWEIGHT", "0.3 0.4");
  const OptimLine w = read_line(fw, "OPTIM FIXWEIGHT (7) 0 1");
  REQUIRE(w.boxes.size() == 1);
  CHECK(w.boxes[0].index == 7);
  CHECK(w.boxes[0].slot == 1);
  CHECK(refusal(fw, "OPTIM FIXWEIGHT (5) 0 1") ==
        "1: OPTIM FIXWEIGHT (5): coefficient 5 is not in FIXINDEX (3 7); "
        "the indices of OPTIM FIXWEIGHT name coefficients, those FIXINDEX "
        "lists, not positions among the FIXWEIGHT values");

  const OptimLine e = read_line(embed_seed(), "OPTIM ta.Density.Centres (2) 2.4 2.6");
  CHECK(e.key == "TA.DENSITY.CENTRES");
  CHECK(e.named);
  CHECK(e.config == std::vector<std::string>{"1", "2.5", "4"});
  REQUIRE(e.boxes.size() == 1);
  CHECK(e.boxes[0].slot == 1);
}

/* Pins: the refusals come in tadah hpo's order: a key that is no key of
 * the training configuration before its index list (OPTIM FOO 2 1 2), the
 * discrete index OALGO (1) before its bounds, RATIO <= 1 before the refusal
 * of AUTO on a cutoff key; a value two lines select is refused at the
 * second, naming the first ("on line 1"), the keys matched in any case; a
 * refusal of an entry names the key and the entry once; an index past the
 * key's values names their count; an unfinished last line is refused at
 * its line. Each is a TargetLineError at the line's last physical line.
 * Before the fix: the header does not exist. */
TEST_CASE("OPTIM reader: the refusals and their order", "[hpotarget_optim]") {
  CHECK(refusal(blip_seed(), "OPTIM FOO 2 1 2").rfind(
            "1: OPTIM FOO: FOO is not a key of the training configuration",
            0) == 0);
  Context oalgo = blip_seed();
  oalgo.add("OALGO", "1 0.5");
  CHECK(refusal(oalgo, "OPTIM OALGO (1) x 2").rfind(
            "1: OPTIM OALGO (1) is not a valid search target: the first "
            "OALGO value selects the training algorithm",
            0) == 0);
  CHECK(refusal(blip_seed(), "OPTIM RCUT2B (1) AUTO RATIO 0.5") ==
        "1: OPTIM RCUT2B: AUTO RATIO must be > 1 (got 0.5)");
  CHECK(refusal(blip_seed(), "OPTIM RCUT2B (1) AUTO") ==
        "1: OPTIM RCUT2B: a cutoff key takes explicit bounds, OPTIM RCUT2B "
        "(indices) <low> <high>, and no AUTO box: the neighbour list is "
        "sized from the explicit high bound");
  CHECK(refusal(blip_seed(),
                "OPTIM CGRID2B (1-2) 1 6\n# a comment\nOPTIM cgrid2b (2) \\\n"
                "  2 4\n") ==
        "4: OPTIM CGRID2B (2): the OPTIM CGRID2B line on line 1 selects "
        "this index as well; one OPTIM line selects each index (OPTIM keys "
        "match in any case)");
  const std::string w = refusal(blip_seed(), "OPTIM SGRID2B (2) 0 3");
  CHECK(w.rfind("1: OPTIM SGRID2B (2): the low bound 0 is not > 0, but the "
                "D2_Blip pair basis requires every SGRID2B width > 0",
                0) == 0);
  CHECK(w.find("OPTIM SGRID2B (2)", w.find("OPTIM SGRID2B (2)") + 1) ==
        std::string::npos);
  CHECK(refusal(blip_seed(), "OPTIM CGRID2B (1-5) AUTO") ==
        "1: OPTIM CGRID2B (5): CGRID2B has 4 values in the training "
        "configuration, so the indices of OPTIM CGRID2B run from 1 to 4");
  CHECK(refusal(blip_seed(), "OPTIM CGRID2B (1) 1 2\nOPTIM CGRID2B (2) 1 \\") ==
        "2: the line ends with \\, which continues it on the next line, and "
        "the file has no next line");
}

/* Pins: read_optim_lines reads only the OPTIM lines, in order, those inside
 * a block too (OPTIMIZER ... ENDOPTIMIZER: tadah hpo --check refuses such a
 * file, and tadah analysis check_bounds audits the boxes), and keeps each
 * line's source (first and last physical line, text). Before the fix: the
 * header does not exist. */
TEST_CASE("OPTIM reader: the OPTIM lines of a file", "[hpotarget_optim]") {
  Context c = blip_seed();
  const auto l = read_file(
      c, "MAXEVAL 10\nOPTIMIZER\nLIB NLOPT\nOPTIM CGRID2B (1) 1.5 2.5\n"
         "ENDOPTIMIZER\nOPTIM SGRID2B (1,2) \\\n 0.5 2  # widths\n");
  REQUIRE(l.size() == 2);
  CHECK(l[0].source.first == 4);
  CHECK(l[0].optim.key == "CGRID2B");
  CHECK(l[1].source.first == 6);
  CHECK(l[1].source.last == 7);
  CHECK(l[1].source.text == "OPTIM SGRID2B (1,2)  0.5 2");
  CHECK(l[1].optim.line == 7);
  CHECK(l[1].optim.boxes.size() == 2);
}

/* D2_Blip 4 4 Ta Ta and DM_mBlipEnv 0 1 1 2 2 Ta Ta: CGRIDMB 0, SGRIDMB
 * 0.2, CEMBFUNC 1 2, SEMBFUNC 1 1, RCUTENV 1.5, RCUTMB 5. */
static Context blipenv_seed() {
  tadah::core::PeriodicTable::initialize();
  Context env;
  env.add("ATOM", "Ta");
  env.add("INIT2B", "true");
  env.add("INITMB", "true");
  env.add("TYPE2B", "D2_Blip 4 4 Ta Ta");
  env.add("CGRID2B", "2.0 3.0 4.0 5.0");
  env.add("SGRID2B", "1.0 1.0 1.0 1.0");
  env.add("RCTYPE2B", "Cut_Cos");
  env.add("RCUT2B", 5.5);
  env.add("TYPEMB", "DM_mBlipEnv 0 1 1 2 2 Ta Ta");
  env.add("CGRIDMB", 0.0);
  env.add("SGRIDMB", 0.2);
  env.add("CEMBFUNC", "1.0 2.0");
  env.add("SEMBFUNC", "1.0 1.0");
  env.add("RCUTENV", 1.5);
  env.add("RCTYPEMB", "Cut_Cos");
  env.add("RCUTMB", 5.0);
  env.add("VERBOSE", 0);
  return env;
}

/* D2_BP 2 2 Ta Ta, CGRID2B 0 2, SGRID2B @p widths, RCUT2B 5 (a Gaussian
 * pair term). */
static Context bp_seed(const char *widths = "0.5 1.0") {
  tadah::core::PeriodicTable::initialize();
  Context c;
  c.add("ATOM", "Ta");
  c.add("INIT2B", "true");
  c.add("INITMB", "false");
  c.add("TYPE2B", "D2_BP 2 2 Ta Ta");
  c.add("CGRID2B", "0.0 2.0");
  c.add("SGRID2B", widths);
  c.add("RCTYPE2B", "Cut_Cos");
  c.add("RCUT2B", 5.0);
  c.add("VERBOSE", 0);
  return c;
}

/* Pins: AUTO is refused for the distance cutoffs alone, RCUT2B and RCUTMB
 * (with or without AUTO options), with the cutoff refusal; RCUTENV, the
 * density at which the envelope of DM_mBlipEnv reaches 1, takes it, and
 * its values stay > 0 (optim_key_positive_only), as the widths' do:
 * RCUTENV 1.5 (1) AUTO is the geometric RATIO 2 box [0.75, 3],
 * AutoGeometric.
 * Before the fix: every key that starts with RCUT took no AUTO box, so
 * OPTIM RCUTENV (1) AUTO was refused as a cutoff key ("OPTIM RCUTENV: a
 * cutoff key takes explicit bounds, ..."). Before the fix that puts
 * RCUTENV among the keys whose values stay positive: the box was the
 * linear FRAC 0.5 box [0.75, 2.25], AutoLinear, and
 * optim_key_positive_only("RCUTENV") was false. */
TEST_CASE("OPTIM reader: AUTO is refused for the distance cutoffs only",
          "[hpotarget_optim]") {
  const Context env = blipenv_seed();
  INFO(refusal(env, "OPTIM RCUTENV (1) AUTO"));
  REQUIRE(refusal(env, "OPTIM RCUTENV (1) AUTO") == "");
  const OptimLine e = read_line(env, "OPTIM RCUTENV (1) AUTO");
  CHECK(e.auto_box);
  CHECK(boxes_of(e) ==
        std::vector<std::pair<double, double>>{{0.75, 3.0}});
  REQUIRE(e.boxes.size() == 1);
  CHECK(e.boxes[0].origin == OptimBoxOrigin::AutoGeometric);
  CHECK(optim_key_positive_only("RCUTENV"));
  for (const char *key : {"SGRID2B", "SGRIDMB", "SEMBFUNC"})
    CHECK(optim_key_positive_only(key));
  for (const char *key : {"CGRID2B", "CGRIDMB", "CEMBFUNC", "AMPGRIDMB",
                          "WATOM", "FIXWEIGHT", "RCUT2B"})
    CHECK_FALSE(optim_key_positive_only(key));
  for (const char *key : {"RCUT2B", "RCUTMB"})
    CHECK(refusal(env, "OPTIM " + std::string(key) + " (1) AUTO") ==
          "1: OPTIM " + std::string(key) + ": a cutoff key takes explicit "
          "bounds, OPTIM " + std::string(key) + " (indices) <low> <high>, "
          "and no AUTO box: the neighbour list is sized from the explicit "
          "high bound");
  CHECK(refusal(env, "OPTIM RCUTMB (1) AUTO FRAC 0.1") ==
        "1: OPTIM RCUTMB: a cutoff key takes explicit bounds, OPTIM RCUTMB "
        "(indices) <low> <high>, and no AUTO box: the neighbour list is "
        "sized from the explicit high bound");
}


/* Pins: an explicit box whose low bound is below the lower limit that the
 * descriptor reading the key declares is refused at its OPTIM line,
 * naming the key and index, the bound, the descriptor and the limit, once
 * per descriptor family:
 *   - DM_mBlipEnv: RCUTENV -1 (the box OPTIM RCUTENV (1) -1 7), SGRIDMB 0
 *     and SEMBFUNC -0.5 are not > 0, "... so the low bound of its box is
 *     > 0 (an AUTO FRAC box is clamped at half its seed)"; CGRIDMB -0.5 is
 *     negative, "... is >= 0 (its AUTO FRAC box, the default, is clamped
 *     at 0)"; RCUTMB -1, a cutoff, which takes no AUTO box, "... is >= 0";
 *   - D2_BP: SGRID2B -0.5 (a Gaussian width) is not > 0, CGRID2B -1 is
 *     negative;
 *   - DM_REAM (RHOFORM NONNEG, a Blip5 density): CGRIDMB -0.5 is negative,
 *     by "the DM_REAM descriptor";
 * and a box at or above the limit is read unchanged: RCUTENV [0.5, 3],
 * CGRIDMB [0, 0.5], SGRID2B [1e-3, 1], CGRID2B [0, 2.5].
 * Before the fix: every such box was read, and an evaluation whose value
 * the descriptor refuses (RCUTENV < 0: "Cutoff distance cannot be
 * negative") scored FAILSCORE. Before the fix of the clamp: the > 0 tail
 * said "(an AUTO FRAC box is clamped at 1e-06)". */
TEST_CASE("OPTIM reader: an explicit box below the lower limit of the key "
          "is refused at its line",
          "[hpotarget_optim][low_bound]") {
  const std::string positive_tail =
      ", so the low bound of its box is > 0 (an AUTO FRAC box is clamped at "
      "half its seed)";
  const std::string nonneg_tail =
      ", so the low bound of its box is >= 0 (its AUTO FRAC box, the "
      "default, is clamped at 0)";
  const Context env = blipenv_seed();
  CHECK(refusal(env, "OPTIM RCUTENV (1) -1 7") ==
        "1: OPTIM RCUTENV (1): the low bound -1 is not > 0, but the "
        "DM_mBlipEnv descriptor requires every RCUTENV value > 0" +
            positive_tail);
  CHECK(refusal(env, "OPTIM SGRIDMB (1) 0 0.5") ==
        "1: OPTIM SGRIDMB (1): the low bound 0 is not > 0, but the "
        "DM_mBlipEnv descriptor requires every SGRIDMB value > 0" +
            positive_tail);
  CHECK(refusal(env, "OPTIM SEMBFUNC (2) -0.5 2") ==
        "1: OPTIM SEMBFUNC (2): the low bound -0.5 is not > 0, but the "
        "DM_mBlipEnv descriptor requires every SEMBFUNC value > 0" +
            positive_tail);
  CHECK(refusal(env, "OPTIM CGRIDMB (1) -0.5 0.5") ==
        "1: OPTIM CGRIDMB (1): the low bound -0.5 is negative, but the "
        "DM_mBlipEnv descriptor requires every CGRIDMB value >= 0" +
            nonneg_tail);
  CHECK(refusal(env, "OPTIM RCUTMB (1) -1 6") ==
        "1: OPTIM RCUTMB (1): the low bound -1 is negative, but the "
        "DM_mBlipEnv descriptor requires every RCUTMB value >= 0, so the low "
        "bound of its box is >= 0");
  CHECK(refusal(bp_seed(), "OPTIM SGRID2B (1-2) -0.5 1.5") ==
        "1: OPTIM SGRID2B (1): the low bound -0.5 is not > 0, but the D2_BP "
        "descriptor requires every SGRID2B value > 0" + positive_tail);
  CHECK(refusal(bp_seed(), "OPTIM CGRID2B (2) -1 2.5") ==
        "1: OPTIM CGRID2B (2): the low bound -1 is negative, but the D2_BP "
        "descriptor requires every CGRID2B value >= 0" + nonneg_tail);
  CHECK(refusal(ream_nonneg_seed(), "OPTIM CGRIDMB (1) -0.5 3") ==
        "1: OPTIM CGRIDMB (1): the low bound -0.5 is negative, but the "
        "DM_REAM descriptor requires every CGRIDMB value >= 0" + nonneg_tail);

  CHECK(boxes_of(read_line(env, "OPTIM RCUTENV (1) 0.5 3")) ==
        std::vector<std::pair<double, double>>{{0.5, 3.0}});
  CHECK(boxes_of(read_line(env, "OPTIM CGRIDMB (1) 0 0.5")) ==
        std::vector<std::pair<double, double>>{{0.0, 0.5}});
  CHECK(boxes_of(read_line(bp_seed(), "OPTIM SGRID2B (1) 1e-3 1")) ==
        std::vector<std::pair<double, double>>{{1e-3, 1.0}});
  CHECK(boxes_of(read_line(bp_seed(), "OPTIM CGRID2B (2) 0 2.5")) ==
        std::vector<std::pair<double, double>>{{0.0, 2.5}});
}

/* Pins: an AUTO box that reaches below the lower limit of the key is
 * clamped at it, and the clamped box is the box of the value: the limit of
 * a key whose values are >= 0 (AutoLinearNonneg), half the seed for a key
 * whose values are > 0 (AutoLinearPositive), as the refit writer clamps
 * the box of a width:
 *   - DM_mBlipEnv: RCUTENV 1.5 AUTO FRAC 1.5 (x0 - d = -0.75) is
 *     [0.75, 3.75]; SGRIDMB 0.2 AUTO FRAC 1.5 (-0.1) is [0.1, 0.5];
 *     CGRIDMB 0 AUTO FRAC 0 FLOOR 0.5 (-0.5) is [0, 0.5]; RCUTENV AUTO
 *     FRAC 1.5 LOG is read with the clamped box, which lies above 0;
 *   - D2_Blip: SGRID2B 1 AUTO FRAC 2 (the bump rule, -1) is [0.5, 3];
 *   - DM_REAM under NONNEG: WATOM 73 AUTO FRAC 1.5 (the form's rule,
 *     -36.5) is [36.5, 182.5];
 * an AUTO box above the limit is unchanged (RCUTENV AUTO FRAC 0.2 is
 * [1.2, 1.8], AutoLinear); a seed the rule does not take is refused,
 * naming it (D2_BP SGRID2B -0.5: "the seed -0.5 is not > 0, but the D2_BP
 * descriptor requires every SGRID2B value > 0").
 * Before the fix: RCUTENV, SGRIDMB of DM_mBlipEnv and CGRIDMB took the
 * boxes [-0.75, 3.75], [-0.1, 0.5] and [-0.5, 0.5] without a word; the
 * SGRID2B of D2_Blip and the WATOM boxes were refused ("the AUTO box
 * [-1, 3] has the low bound -1, which is not > 0, but the D2_Blip pair
 * basis requires every SGRID2B width > 0, ..."); a negative D2_BP width
 * took the mirrored box [-1, -0.25]. Before the fix of the clamp: a box of
 * a value > 0 was clamped at 1e-6 ([1e-6, 3.75], [1e-6, 0.5], [1e-6, 3],
 * [1e-6, 182.5]), the refit writer's at half the seed. */
TEST_CASE("OPTIM reader: an AUTO box below the lower limit of the key is "
          "clamped at it",
          "[hpotarget_optim][low_bound]") {
  const Context env = blipenv_seed();
  const OptimLine r = read_line(env, "OPTIM RCUTENV (1) AUTO FRAC 1.5");
  CHECK(boxes_of(r) == std::vector<std::pair<double, double>>{{0.75, 3.75}});
  REQUIRE(r.boxes.size() == 1);
  CHECK(r.boxes[0].origin == OptimBoxOrigin::AutoLinearPositive);
  const OptimLine s = read_line(env, "OPTIM SGRIDMB (1) AUTO FRAC 1.5");
  CHECK(boxes_of(s) == std::vector<std::pair<double, double>>{{0.1, 0.5}});
  REQUIRE(s.boxes.size() == 1);
  CHECK(s.boxes[0].origin == OptimBoxOrigin::AutoLinearPositive);
  const OptimLine c = read_line(env, "OPTIM CGRIDMB (1) AUTO FRAC 0 FLOOR 0.5");
  CHECK(boxes_of(c) == std::vector<std::pair<double, double>>{{0.0, 0.5}});
  REQUIRE(c.boxes.size() == 1);
  CHECK(c.boxes[0].origin == OptimBoxOrigin::AutoLinearNonneg);
  const OptimLine l = read_line(env, "OPTIM RCUTENV (1) AUTO FRAC 1.5 LOG");
  CHECK(boxes_of(l) == std::vector<std::pair<double, double>>{{0.75, 3.75}});
  const OptimLine u = read_line(env, "OPTIM RCUTENV (1) AUTO FRAC 0.2");
  REQUIRE(u.boxes.size() == 1);
  CHECK(u.boxes[0].lo == 1.5 - 0.2 * 1.5);
  CHECK(u.boxes[0].hi == 1.5 + 0.2 * 1.5);
  CHECK(u.boxes[0].origin == OptimBoxOrigin::AutoLinear);

  const OptimLine b = read_line(blip_seed(), "OPTIM SGRID2B (1) AUTO FRAC 2");
  CHECK(boxes_of(b) == std::vector<std::pair<double, double>>{{0.5, 3.0}});
  REQUIRE(b.boxes.size() == 1);
  CHECK(b.boxes[0].origin == OptimBoxOrigin::AutoLinearPositive);
  const OptimLine w = read_line(ream_nonneg_seed(), "OPTIM WATOM (1) AUTO FRAC 1.5");
  CHECK(boxes_of(w) == std::vector<std::pair<double, double>>{{36.5, 182.5}});

  CHECK(refusal(bp_seed("-0.5 1.0"), "OPTIM SGRID2B (1) AUTO") ==
        "1: OPTIM SGRID2B (1): the seed -0.5 is not > 0, but the D2_BP "
        "descriptor requires every SGRID2B value > 0");
}

/* Pins: the low end of a clamped AUTO FRAC box of a value > 0 is the one
 * the refit writer gives the box of a width (engines::optim_group_bound
 * with positive_only, the starter target file of tadah refit) at the same
 * percentage: SGRID2B 1 and 1e-6 of a D2_BP at FRAC 1.5 and at 150% (a
 * group floor below the seed), half the seed, so the two programs give one
 * box.
 * Before the fix of the clamp: the reader clamped at 1e-6 (seed 1) and the
 * writer at max(1e-6, |v|/2) (seed 1e-6: 1e-6, the seed itself). */
TEST_CASE("OPTIM reader: a clamped AUTO FRAC box of a width starts where "
          "the refit writer's box starts",
          "[hpotarget_optim][low_bound]") {
  for (const double seed : {1.0, 1e-6}) {
    INFO("SGRID2B " << seed);
    const std::string widths = tadah::core::format_roundtrip(seed) + " 1.0";
    const OptimLine r =
        read_line(bp_seed(widths.c_str()), "OPTIM SGRID2B (1) AUTO FRAC 1.5");
    REQUIRE(r.boxes.size() == 1);
    const engines::OptimBound w =
        engines::optim_group_bound(seed, 150.0, 1e-3 * seed, true);
    CHECK(r.boxes[0].lo == w.lo);
    CHECK(r.boxes[0].lo == 0.5 * seed);
  }
}

/* Pins: the seed of an AUTO box that the low-bound rule of a >= 0 key
 * does not take is refused, naming the seed, the descriptor and the rule,
 * for AUTO FRAC and AUTO RATIO alike: a D2_BP centre CGRID2B -0.5 with
 * OPTIM CGRID2B (1) AUTO FRAC 0.5 or AUTO RATIO 2 gives "the seed -0.5 is
 * negative, but the D2_BP descriptor requires every CGRID2B value >= 0";
 * the centre 0 is taken ([0, 0.5] under AUTO FRAC 0 FLOOR 0.5).
 * Before the fix: AUTO FRAC 0.5 searched the box [-0.75, -0.25] and AUTO
 * RATIO 2 the box [-1, -0.25] of the centre, every value of which
 * tadah hpo refuses in an explicit box. */
TEST_CASE("OPTIM reader: a negative seed of a centre is refused for an AUTO "
          "box",
          "[hpotarget_optim][low_bound]") {
  Context c = bp_seed();
  c.remove("CGRID2B");
  c.add("CGRID2B", "-0.5 2.0");
  for (const char *line : {"OPTIM CGRID2B (1) AUTO FRAC 0.5",
                           "OPTIM CGRID2B (1) AUTO RATIO 2"}) {
    INFO(line);
    CHECK(refusal(c, line) ==
          "1: OPTIM CGRID2B (1): the seed -0.5 is negative, but the D2_BP "
          "descriptor requires every CGRID2B value >= 0");
  }
  CHECK(boxes_of(read_line(bp_seed(), "OPTIM CGRID2B (1) AUTO FRAC 0 FLOOR "
                                      "0.5")) ==
        std::vector<std::pair<double, double>>{{0.0, 0.5}});
}

/* Pins: on a DM_mJoin line, RCUTENV, which the line gives every
 * descriptor whole, keeps the rule of DM_mBlipEnv whatever the order of
 * the descriptors: with DM_mBlip first and with DM_mBlipEnv first,
 * OPTIM RCUTENV (1) -1 7 is refused ("the low bound -1 is not > 0, but the
 * DM_mBlipEnv descriptor requires every RCUTENV value > 0, ...") and
 * RCUTENV 1.5 AUTO FRAC 1.5 is clamped to [0.75, 3.75]
 * (AutoLinearPositive).
 * Before the fix: with DM_mBlip first the box [-1, 7] was taken and the
 * AUTO box was [-0.75, 3.75] (AutoLinear): the rule went by the first
 * descriptor of the line, which declares no limit for RCUTENV. */
TEST_CASE("OPTIM reader: RCUTENV on a DM_mJoin line keeps the rule of "
          "DM_mBlipEnv in any order",
          "[hpotarget_optim][low_bound]") {
  for (const char *line :
       {"DM_mJoin DM_mBlip 0 1 1 2 2 Ta Ta DM_mBlipEnv 0 1 1 2 2 Ta Ta",
        "DM_mJoin DM_mBlipEnv 0 1 1 2 2 Ta Ta DM_mBlip 0 1 1 2 2 Ta Ta"}) {
    INFO(line);
    Context c = blipenv_seed();
    for (const char *k : {"TYPEMB", "CGRIDMB", "SGRIDMB", "CEMBFUNC",
                          "SEMBFUNC", "RCTYPEMB", "RCUTMB"})
      c.remove(k);
    c.add("TYPEMB", line);
    c.add("CGRIDMB", "0.1 0.1");
    c.add("SGRIDMB", "0.2 0.2");
    c.add("CEMBFUNC", "1.0 2.0 1.0 2.0");
    c.add("SEMBFUNC", "1.0 1.0 1.0 1.0");
    c.add("RCTYPEMB", "Cut_Cos Cut_Cos");
    c.add("RCUTMB", "5.0 5.0");
    CHECK(refusal(c, "OPTIM RCUTENV (1) -1 7") ==
          "1: OPTIM RCUTENV (1): the low bound -1 is not > 0, but the "
          "DM_mBlipEnv descriptor requires every RCUTENV value > 0, so the "
          "low bound of its box is > 0 (an AUTO FRAC box is clamped at half "
          "its seed)");
    const OptimLine a = read_line(c, "OPTIM RCUTENV (1) AUTO FRAC 1.5");
    CHECK(boxes_of(a) == std::vector<std::pair<double, double>>{{0.75, 3.75}});
    REQUIRE(a.boxes.size() == 1);
    CHECK(a.boxes[0].origin == OptimBoxOrigin::AutoLinearPositive);
  }
}

#else

TEST_CASE("OPTIM reader: the header exists", "[hpotarget_optim]") {
  FAIL("tadah/mlip/hpotarget_optim.h does not exist: MLIP has no reader of "
       "the OPTIM lines");
}

#endif
