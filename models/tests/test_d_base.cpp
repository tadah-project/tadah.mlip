#include "catch2/catch.hpp"
#include <tadah/models/descriptors/d_base.h>
#include <tadah/models/descriptors/d2/d2_blip.h>
#include <tadah/models/descriptors/d2/d2_blip5.h>
#include <tadah/models/descriptors/d2/d2_bp.h>
#include <tadah/models/descriptors/d_mjoin.h>
#include <tadah/core/context.h>
#include <tadah/core/core_types.h>
#include <tadah/core/periodic_table.h>
#include <tadah/core/blueprint_parser.h>
#include <tadah/core/core_user_config_parser.h>
#include <tadah/core/logger.h>
#include <tadah/core/number_format.h>

#include <cmath>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

using namespace tadah::core;
using namespace tadah::models;

namespace {
struct WatomTestFixture {
  WatomTestFixture() { PeriodicTable::initialize(); }
};

Context make_minimal_blip_context_watom_tests() {
  Context c;
  c.add("INIT2B", "true");
  c.add("VERBOSE", 0);
  c.add("SGRID2B", "2.0 1.0");
  c.add("CGRID2B", "0.5 1.4");
  c.add("RCUT2B", 3.5);
  c.add("TYPE2B", "D2_Blip");
  c.add("TYPE2B", 2);
  c.add("TYPE2B", 2);
  c.add("TYPE2B", "H"); c.add("TYPE2B", "H");
  return c;
}
}  // namespace
TEST_CASE( "Testing D_Base", "[D_Base]" ) {

  v_type sgrid(5);
  v_type cgrid(6);
  Context c;
  c.add("SGRID2B", "LIN 5 0.1 12");
  c.add("SGRIDMB", "LIN 5 0.1 12");
  c.add("CGRID2B", "LIN 5 0.1 12");
  c.add("CGRIDMB", "LIN 5 0.1 12");
  D_Base::get_grid(c, "SGRID2B", sgrid );
  std::vector<double> sg1 = {0.1,3.075,6.05,9.025,12};
  REQUIRE_THAT(sgrid, Catch::Matchers::Equals(sg1));
  D_Base::get_grid(c, "SGRIDMB", sgrid );


  D_Base::get_grid(c, "CGRID2B", cgrid );
  D_Base::get_grid(c, "CGRIDMB", cgrid );
  std::vector<double> cg1 = {0.1,3.075,6.05,9.025,12};
  REQUIRE_THAT(cgrid, Catch::Matchers::Equals(cg1));

  double margin = 1e-10;
  Context c2;
  c2.add("SGRID2B", "GEOM 5 0.1 12");
  c2.add("SGRIDMB", "GEOM 5 0.1 12");
  c2.add("CGRID2B", "GEOM 5 0.1 12");
  c2.add("CGRIDMB", "GEOM 5 0.1 12");
  D_Base::get_grid(c2, "SGRID2B", sgrid );
  D_Base::get_grid(c2, "SGRIDMB", sgrid );
  std::vector<double> sg2 = {0.1, 0.330975092, 1.095445115, 3.6256504768, 12.0 };
  REQUIRE_THAT(sgrid, Catch::Matchers::Approx(sg2).margin(margin));

  D_Base::get_grid(c2, "CGRID2B", cgrid );
  D_Base::get_grid(c2, "CGRIDMB", cgrid );
  std::vector<double> cg2 = {0.1, 0.330975092, 1.095445115, 3.6256504768, 12.0 };
  REQUIRE_THAT(cgrid, Catch::Matchers::Approx(cg2).margin(margin));

  Context c3;
  c3.add("SGRID2B", "-2.1 5 0.1 12");
  D_Base::get_grid(c3, "SGRID2B", sgrid );

  //throws
  Context c_err;
  c_err.add("SGRID2B", "LOG 5 0.1 12");
  REQUIRE_THROWS(D_Base::get_grid(c_err,"SGRID2B",sgrid));
}

// =====================================================================
// Dual-format WATOM tests. Exercise D_Base::update_weights /
// get_atom_Z_for_arg_pos / has_watom_arg_pos / dweight_dw_n through a
// concrete D2_Blip descriptor.
// =====================================================================

TEST_CASE_METHOD(WatomTestFixture, "WATOM per-species single element: diagonal write",
          "[D_Base][watom][dual_format]") {
  Context c = make_minimal_blip_context_watom_tests();
  c.add("ATOM", "H");
  c.add("WATOM", 0.5);
  D2_Blip d(c);

  CHECK(d.weights[1][1] == Approx(0.5));
  CHECK(d.weights[8][8] == Approx(1.0));  // unrelated element stays at default
  CHECK(d.has_watom_arg_pos(1));
  CHECK_FALSE(d.has_watom_arg_pos(2));
  CHECK(d.get_atom_Z_for_arg_pos(1) == 1);
  CHECK(d.dweight_dw_n(1, 1, 1) == Approx(1.0));
}

TEST_CASE_METHOD(WatomTestFixture, "WATOM per-species two elements: geometric-mean off-diagonal",
          "[D_Base][watom][dual_format]") {
  Context c = make_minimal_blip_context_watom_tests();
  c.add("ATOM", "H");
  c.add("ATOM", "O");
  c.add("WATOM", 0.5);
  c.add("WATOM", 2.0);
  D2_Blip d(c);

  CHECK(d.weights[1][1] == Approx(0.5));
  CHECK(d.weights[8][8] == Approx(2.0));
  const double cross = std::sqrt(0.5 * 2.0);
  CHECK(d.weights[1][8] == Approx(cross));
  CHECK(d.weights[8][1] == Approx(cross));
  CHECK(d.has_watom_arg_pos(1));
  CHECK(d.has_watom_arg_pos(2));
  CHECK_FALSE(d.has_watom_arg_pos(3));
  CHECK(d.get_atom_Z_for_arg_pos(1) == 1);
  CHECK(d.get_atom_Z_for_arg_pos(2) == 8);

  // Chain factors at arg_pos=1 (Z_target=H, w_H=0.5, w_O=2.0):
  //   d w[H][H] / d w_H = 1
  //   d w[O][O] / d w_H = 0
  //   d w[H][O] / d w_H = 0.5 * sqrt(w_O / w_H) = 0.5 * sqrt(4) = 1.0
  CHECK(d.dweight_dw_n(1, 1, 1) == Approx(1.0));
  CHECK(d.dweight_dw_n(8, 8, 1) == Approx(0.0));
  CHECK(d.dweight_dw_n(1, 8, 1) == Approx(0.5 * std::sqrt(2.0 / 0.5)));
  CHECK(d.dweight_dw_n(8, 1, 1) == Approx(0.5 * std::sqrt(2.0 / 0.5)));
  // arg_pos=2 (Z_target=O):
  CHECK(d.dweight_dw_n(8, 8, 2) == Approx(1.0));
  CHECK(d.dweight_dw_n(1, 1, 2) == Approx(0.0));
  CHECK(d.dweight_dw_n(1, 8, 2) == Approx(0.5 * std::sqrt(0.5 / 2.0)));
}

TEST_CASE_METHOD(WatomTestFixture, "WATOM pair-triple: direct symmetric writes, no geometric mean",
          "[D_Base][watom][dual_format]") {
  Context c = make_minimal_blip_context_watom_tests();
  c.add("ATOM", "H");
  c.add("ATOM", "O");
  // Pair triples in arbitrary order.
  c.add("WATOM", "H"); c.add("WATOM", "H"); c.add("WATOM", 0.7);
  c.add("WATOM", "O"); c.add("WATOM", "O"); c.add("WATOM", 1.3);
  c.add("WATOM", "H"); c.add("WATOM", "O"); c.add("WATOM", 0.9);
  D2_Blip d(c);

  CHECK(d.weights[1][1] == Approx(0.7));
  CHECK(d.weights[8][8] == Approx(1.3));
  CHECK(d.weights[1][8] == Approx(0.9));  // direct, NOT sqrt(0.7*1.3)
  CHECK(d.weights[8][1] == Approx(0.9));
  CHECK(d.weights[6][6] == Approx(1.0));  // unrelated element default

  CHECK(d.has_watom_arg_pos(1));
  CHECK(d.has_watom_arg_pos(2));
  CHECK(d.has_watom_arg_pos(3));
  CHECK_FALSE(d.has_watom_arg_pos(4));

  // PairTriple mode: arg_pos→Z mapping is empty.
  CHECK(d.get_atom_Z_for_arg_pos(1) == 0);

  // Chain factors in pair-triple mode:
  //   arg_pos=1 → (H,H); 1.0 only on the (H,H) pair.
  CHECK(d.dweight_dw_n(1, 1, 1) == Approx(1.0));
  CHECK(d.dweight_dw_n(8, 8, 1) == Approx(0.0));
  CHECK(d.dweight_dw_n(1, 8, 1) == Approx(0.0));
  //   arg_pos=3 → (H,O); 1.0 on (H,O) and (O,H) only.
  CHECK(d.dweight_dw_n(1, 8, 3) == Approx(1.0));
  CHECK(d.dweight_dw_n(8, 1, 3) == Approx(1.0));
  CHECK(d.dweight_dw_n(1, 1, 3) == Approx(0.0));
}

TEST_CASE_METHOD(WatomTestFixture, "WATOM pair-triple: bad token count throws",
          "[D_Base][watom][dual_format]") {
  Context c = make_minimal_blip_context_watom_tests();
  c.add("ATOM", "H");
  c.add("ATOM", "O");
  // Two tokens — not a multiple of three.
  c.add("WATOM", "H"); c.add("WATOM", "H");
  REQUIRE_THROWS_AS(D2_Blip(c), std::runtime_error);
}

TEST_CASE_METHOD(WatomTestFixture, "WATOM default (no WATOM key) leaves pair weights at 1.0",
          "[D_Base][watom][dual_format]") {
  Context c = make_minimal_blip_context_watom_tests();
  // No ATOM, no WATOM: update_weights returns early, default-filled.
  D2_Blip d(c);
  for (int i = 1; i < 119; ++i) {
    CHECK(d.weights[i][i] == Approx(1.0));
  }
  CHECK_FALSE(d.has_watom_arg_pos(1));
  CHECK(d.dweight_dw_n(1, 1, 1) == Approx(0.0));  // Empty mode
}

TEST_CASE_METHOD(WatomTestFixture, "WATOM single-species H reduces to old per-Zj semantics",
          "[D_Base][watom][dual_format][regression]") {
  // WATOM regression: single H species → diagonal chain == 1.0,
  // recovers the old δ(Zj, Z_target) gate.
  Context c = make_minimal_blip_context_watom_tests();
  c.add("ATOM", "H");
  c.add("WATOM", 1.0);
  D2_Blip d(c);
  CHECK(d.dweight_dw_n(1, 1, 1) == Approx(1.0));
  CHECK(d.weights[1][1] == Approx(1.0));
}

TEST_CASE("D_Base::get_grid reads the subnormal values the lossless writers "
          "produce", "[D_Base][lossless]") {
  // Context::add(double) and write_potential() now keep a subnormal (the
  // old fixed-decimal rendering flushed it to 0); std::stod threw
  // out_of_range on it, so get_grid rejected the grid.
  const double sub = 1e-310;
  const double dmin = std::numeric_limits<double>::denorm_min();
  SECTION("plain tokens") {
    const v_type g = D_Base::get_grid(std::vector<std::string>{
        "1.0", format_roundtrip(sub), "2.0", format_roundtrip(-dmin)});
    REQUIRE(g.size() == 4);
    CHECK(g[1] == sub);
    CHECK(g[3] == -dmin);
  }
  SECTION("LIN/GEOM endpoints") {
    const v_type g = D_Base::get_grid(
        std::vector<std::string>{"LIN", "3", "0", "1e-310"});
    REQUIRE(g.size() == 3);
    CHECK(g.back() == sub);
  }
  SECTION("Context add -> write_potential -> reparse -> get_grid") {
    Context c;
    c.add("CGRID2B", std::vector<double>{1.0, sub, 2.0});
    std::ostringstream os;
    c.write_potential(os, 18);
    std::istringstream in(os.str());
    const Context back = CoreUserConfigParser().parse(in);
    v_type g;
    D_Base::get_grid(back, "CGRID2B", g);
    REQUIRE(g.size() == 3);
    CHECK(g[0] == 1.0);
    CHECK(g[1] == sub);
    CHECK(g[2] == 2.0);
  }
  SECTION("non-numbers and overflow are still rejected") {
    CHECK_THROWS_AS(D_Base::get_grid(std::vector<std::string>{"abc"}),
                    std::invalid_argument);
    CHECK_THROWS_AS(D_Base::get_grid(std::vector<std::string>{"1e400"}),
                    std::invalid_argument);
    CHECK_THROWS_AS(D_Base::get_grid(std::vector<std::string>{"1.5x"}),
                    std::invalid_argument);
  }
}

/* A grid generator (LIN or GEOM) takes three values, <n> <start> <stop>:
 * get_grid refuses one that the grid ends before, a count that is not a
 * whole number from 1 to 1000000, an endpoint that is not a finite decimal
 * number and a GEOM endpoint at or below 0, naming the generator as written;
 * the Context overload and the grids of D2_mJoin name the key. Before the
 * fix: a truncated 'LIN 9' was read past the end of the list (undefined
 * behaviour), 'LIN -3 2 4' failed in std::vector ('cannot create
 * std::vector larger than max_size()'), 'LIN 9.5 2 4' made 9 points,
 * 'LIN 3 0 1x' ended at 1, 'GEOM 3 2 -4' gave NaN values and 'LIN 0 2 4'
 * an empty grid. */
TEST_CASE("D_Base::get_grid refuses an incomplete or invalid LIN/GEOM "
          "generator", "[D_Base][grid]") {
  using Catch::Matchers::Contains;
  using strings = std::vector<std::string>;
  SECTION("the grid ends before the three values") {
    for (const strings &g :
         {strings{"LIN"}, strings{"LIN", "9"}, strings{"LIN", "9", "2.0"},
          strings{"1.0", "GEOM", "3", "0.1"}, strings{"lin", "9"}}) {
      INFO(g.size());
      REQUIRE_THROWS_AS(D_Base::get_grid(g), std::invalid_argument);
      REQUIRE_THROWS_WITH(D_Base::get_grid(g),
                          Contains("takes three values, <n> <start> <stop>"));
    }
    REQUIRE_THROWS_WITH(D_Base::get_grid(strings{"LIN", "9"}),
                        Contains("Invalid grid specification: LIN takes "
                                 "three values, <n> <start> <stop>, but the "
                                 "grid ends after 'LIN 9'"));
  }
  SECTION("a count that is not a whole number from 1 to 1000000") {
    for (const char *n : {"-3", "0", "9.5", "abc", "1000001", "1e9"}) {
      INFO(n);
      const strings g{"LIN", n, "2", "4"};
      REQUIRE_THROWS_AS(D_Base::get_grid(g), std::invalid_argument);
      REQUIRE_THROWS_WITH(
          D_Base::get_grid(g),
          Contains("the number of points of 'LIN " + std::string(n) +
                   " 2 4' must be a whole number from 1 to 1000000"));
    }
  }
  SECTION("an endpoint that is not a finite decimal number") {
    for (const strings &g :
         {strings{"LIN", "3", "0", "1x"}, strings{"LIN", "3", "abc", "1"},
          strings{"GEOM", "3", "0.1", "inf"}, strings{"LIN", "3", "0", "nan"},
          strings{"LIN", "3", "0", "1e400"}}) {
      INFO(g[2] + " " + g[3]);
      REQUIRE_THROWS_AS(D_Base::get_grid(g), std::invalid_argument);
      REQUIRE_THROWS_WITH(D_Base::get_grid(g),
                          Contains("must be finite decimal numbers"));
    }
  }
  SECTION("a GEOM endpoint at or below 0") {
    for (const strings &g : {strings{"GEOM", "3", "2", "-4"},
                             strings{"GEOM", "3", "0", "4"},
                             strings{"GEOM", "3", "-2", "-4"}}) {
      INFO(g[2] + " " + g[3]);
      REQUIRE_THROWS_AS(D_Base::get_grid(g), std::invalid_argument);
      REQUIRE_THROWS_WITH(D_Base::get_grid(g),
                          Contains("must be above 0"));
    }
  }
  SECTION("a generator whose points are not finite numbers") {
    // finite endpoints whose difference is beyond the largest double: the
    // step of LIN is infinite (before the fix: the points NaN, inf and
    // 1.7e308, refused later as '-nan' for the key, without the generator)
    for (const strings &g : {strings{"LIN", "3", "-1.7e308", "1.7e308"},
                             strings{"1.0", "LIN", "2", "1.7e308",
                                     "-1.7e308"}}) {
      INFO(g.size());
      REQUIRE_THROWS_AS(D_Base::get_grid(g), std::invalid_argument);
    }
    REQUIRE_THROWS_WITH(
        D_Base::get_grid(strings{"LIN", "3", "-1.7e308", "1.7e308"}),
        "Invalid grid specification: the points of 'LIN 3 -1.7e308 1.7e308' "
        "are not all finite numbers (its stop minus its start is beyond the "
        "largest number)");
    // the largest finite range is a grid
    const v_type ok = D_Base::get_grid(strings{"LIN", "3", "-8e307", "8e307"});
    REQUIRE(ok.size() == 3);
    CHECK(ok[1] == 0.0);
  }
  SECTION("the Context overload names the key") {
    Context c;
    c.add("CGRID2B", "LIN");
    c.add("CGRID2B", "-3");
    c.add("CGRID2B", "2");
    c.add("CGRID2B", "4");
    v_type g;
    REQUIRE_THROWS_AS(D_Base::get_grid(c, "CGRID2B", g),
                      std::invalid_argument);
    REQUIRE_THROWS_WITH(D_Base::get_grid(c, "CGRID2B", g),
                        Contains("CGRID2B: Invalid grid specification"));
  }
  SECTION("a D2_mJoin grid names the key") {
    Context c;
    c.add("TYPE2B", "D2_mJoin");
    for (const char *t : {"D2_Blip", "3", "3", "H", "H"}) c.add("TYPE2B", t);
    for (const char *t : {"D2_Blip", "2", "2", "H", "H"}) c.add("TYPE2B", t);
    c.add("RCTYPE2B", "Cut_Cos");
    c.add("RCTYPE2B", "Cut_Cos");
    c.add("RCUT2B", 3.0);
    c.add("RCUT2B", 3.0);
    for (const char *t : {"LIN", "-3", "2", "4", "1.0", "2.0"})
      c.add("CGRID2B", t);
    for (const char *t : {"1", "1", "1", "1", "1"}) c.add("SGRID2B", t);
    REQUIRE_THROWS_AS(D_mJoin::expand_grids(c), std::invalid_argument);
    REQUIRE_THROWS_WITH(D_mJoin::expand_grids(c),
                        Contains("CGRID2B: Invalid grid specification: the "
                                 "number of points of 'LIN -3 2 4'"));
  }
  SECTION("valid generators are unchanged") {
    CHECK(D_Base::get_grid(strings{"LIN", "1", "2", "4"}) == v_type{2.0});
    CHECK(D_Base::get_grid(strings{"LIN", "9.0", "2", "4"}).size() == 9);
    CHECK(D_Base::get_grid(strings{"lin", "3", "-1", "1"}) ==
          v_type{-1.0, 0.0, 1.0});
    const v_type g = D_Base::get_grid(
        strings{"0.5", "GEOM", "3", "1", "4", "LIN", "2", "5", "6"});
    REQUIRE(g.size() == 6);
    CHECK(g[0] == 0.5);
    CHECK(g[1] == Approx(1.0));
    CHECK(g[2] == Approx(2.0));
    CHECK(g[3] == Approx(4.0));
    CHECK(g[4] == 5.0);
    CHECK(g[5] == 6.0);
  }
}

/* The help of the radial grid keys names the generators get_grid takes,
 * LIN and GEOM, and every example of those keys is a grid get_grid reads.
 * Before the fix: CGRID2B, SGRID2B and SGRIDMB named 'LOG or LIN' and
 * CGRIDMB 'L' (logarithmic) or 'U' (uniform spacing); get_grid refuses
 * each of LOG, L and U. The help states the rule of a joined line (the
 * values of all lines form one list) and the format line names the
 * generator '<generator>' (before the fix: 'one list ... for each joined
 * descriptor, one per line', and '<algorithm>'). */
TEST_CASE("The radial grid keys' help names the LIN and GEOM generators",
          "[D_Base][grid][blueprint]") {
  using Catch::Matchers::Contains;
  const auto &keys = BlueprintParser::getKeysMapRef();
  for (const char *k : {"CGRID2B", "SGRID2B", "CGRIDMB", "SGRIDMB"}) {
    INFO(k);
    const auto it = keys.find(k);
    REQUIRE(it != keys.end());
    const std::string &text = it->second.longDescription;
    CHECK_THAT(text, Contains("LIN <n> <start> <stop>"));
    CHECK_THAT(text, Contains("GEOM <n> <start> <stop>"));
    CHECK_THAT(text, !Contains("LOG"));
    CHECK_THAT(text, !Contains("'L'"));
    CHECK_THAT(text, !Contains("'U'"));
    // the rule D_mJoin::parse_context applies: the values of all lines are
    // one list, taken by the joined descriptors in the order of their TYPE
    // lines; no per-line list
    CHECK_THAT(text, Contains("form one list"));
    CHECK_THAT(text, Contains("in the order of the"));
    CHECK_THAT(text, !Contains("one per line"));
    // a line holds values or generators, as the key's format allows
    CHECK_THAT(text, Contains("A line holds values or generators"));
    CHECK_THAT(text, !Contains("or both"));
    // the format line names the generator as the text does
    REQUIRE(it->second.value_format.size() == 2);
    CHECK(it->second.value_format[1] ==
          "(<generator> <n> <start> <stop>) [...]");
    for (const auto &f : it->second.value_format)
      CHECK_THAT(f, !Contains("<algorithm>"));
    for (const std::string &ex : it->second.examples) {
      INFO(ex);
      std::istringstream in(ex);
      std::vector<std::string> tokens;
      for (std::string t; in >> t;) tokens.push_back(t);
      CHECK_NOTHROW(D_Base::get_grid(tokens));
    }
  }
  for (const char *gen : {"LOG", "L", "U"})
    CHECK_THROWS_AS(D_Base::get_grid(std::vector<std::string>{
                        gen, "3", "1", "4"}),
                    std::invalid_argument);
}

/* A grid key's line holds values or generators (one or more), and lines of
 * each kind may follow one another, as the help says: such a configuration
 * is read and its grid is the lines' values in order. A line with values
 * and a generator together is refused by the key's format.
 * Before the fix: passes as well (the help said a line holds values,
 * generators or both). */
TEST_CASE("A grid key takes lines of values and lines of generators",
          "[D_Base][grid][blueprint]") {
  using tadah::core::CoreUserConfigParser;
  Context c;
  REQUIRE_NOTHROW(c = CoreUserConfigParser().parse(std::vector<std::string>{
                      "ATOM Ta", "CGRID2B 1.0 1.5", "CGRID2B LIN 2 2 3",
                      "CGRID2B LIN 2 4 5 GEOM 2 6 12"}));
  v_type g;
  D_Base::get_grid(c, "CGRID2B", g);
  const std::vector<double> want = {1.0, 1.5, 2.0, 3.0, 4.0, 5.0, 6.0, 12.0};
  REQUIRE(g.size() == want.size());
  for (std::size_t i = 0; i < want.size(); ++i) CHECK(g[i] == Approx(want[i]));
  std::ostringstream quiet;
  auto *old = std::cerr.rdbuf(quiet.rdbuf());
  auto *old_out = std::cout.rdbuf(quiet.rdbuf());
  for (const std::string line : {"CGRID2B 1.0 LIN 3 2 5.3",
                                 "SGRID2B GEOM 3 0.2 1.0 0.1"}) {
    INFO(line);
    CHECK_THROWS(CoreUserConfigParser().parse(
        std::vector<std::string>{"ATOM Ta", line}));
  }
  std::cerr.rdbuf(old);
  std::cout.rdbuf(old_out);
}

/* With the INFO log level (tadah train -v 2) a two-body descriptor lists its
 * grids under their keys, CGRID2B and SGRID2B: "D2_Blip CGRID2B:".
 * Before the fix: D2_Blip, D2_BP and D2_Blip5 labelled them "CGRIDMB:" and
 * "SGRIDMB:", the keys of the many-body grids. */
TEST_CASE("A two-body descriptor names its grids CGRID2B and SGRID2B in the "
          "log", "[D_Base][grid][log]") {
  PeriodicTable::initialize();
  for (const char *label : {"D2_Blip", "D2_BP", "D2_Blip5"}) {
    INFO(label);
    Context c = make_minimal_blip_context_watom_tests();
    c.remove("TYPE2B");
    for (const std::string t : {std::string(label), std::string("2"),
                                std::string("2"), std::string("H"),
                                std::string("H")})
      c.add("TYPE2B", t);
    std::ostringstream captured;
    auto &logger = Logger::instance();
    const LogLevel level = logger.getLogLevel();
    logger.setLogLevel(LogLevel::INFO);
    auto *old = std::cout.rdbuf(captured.rdbuf());
    try {
      if (std::string(label) == "D2_Blip") D2_Blip d(c);
      else if (std::string(label) == "D2_BP") D2_BP d(c);
      else D2_Blip5 d(c);
    } catch (...) {
      std::cout.rdbuf(old);
      logger.setLogLevel(level);
      throw;
    }
    std::cout.rdbuf(old);
    logger.setLogLevel(level);
    const std::string text = captured.str();
    INFO(text);
    CHECK_THAT(text, Catch::Matchers::Contains(std::string(label) +
                                               " CGRID2B:"));
    CHECK_THAT(text, Catch::Matchers::Contains(std::string(label) +
                                               " SGRID2B:"));
    CHECK(text.find("GRIDMB") == std::string::npos);
  }
}
