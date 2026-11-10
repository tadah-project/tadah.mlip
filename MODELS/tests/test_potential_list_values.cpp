// test_potential_list_values.cpp
//
// The values of the RCTYPE*, RCUT* and list keys (CGRID2B, SGRID2B,
// KDEG2B; CGRIDMB, SGRIDMB, CEMBFUNC, SEMBFUNC, AMPGRIDMB, KDEGMB,
// KDEGEMB) of a TYPE2B or TYPEMB line belong to its descriptors, in the
// order of the TYPE lines (D_mJoin::parse_context).
//
// A training configuration with values no descriptor takes is refused:
// more RCTYPE/RCUT values than descriptors, more list values than the
// SIZE parameters ask for, and a list key of the line's type that no
// descriptor of the line reads (D2_LJ with CGRID2B).
//
// A potential (a context with WEIGHTS: tadah predict, pair_style tadah,
// training from a potential, tadah hpo with a potential as its training
// configuration) is read as it was written: tadah 1.5.0 kept such values
// of RCTYPE*, RCUT*, AMPGRIDMB and KDEG* in the potentials it wrote, and
// read only the values its descriptors take. Each such key is reported
// once (core::report_ignored_key_once, captured by pair_style tadah for
// the LAMMPS log), naming the values, and the model is the one without
// them: a joined line's descriptors take their values in order, a single
// descriptor takes the first RCTYPE value and its cutoff is the largest
// RCUT value.

#include "catch2/catch.hpp"

#include <tadah/core/context.h>
#include <tadah/core/key_notice.h>
#include <tadah/core/periodic_table.h>
#include <tadah/models/dc_selector.h>
#include <tadah/models/descriptors/d_mjoin.h>

#include <algorithm>
#include <string>
#include <vector>

using tadah::core::Context;
using tadah::core::IgnoredKeyNoticeCapture;
using tadah::models::D_mJoin;
using tadah::models::DC_Selector;
using Catch::Matchers::Contains;

namespace {

/* A two-body joined line: D2_Blip (2 centres) and D2_Knot5 (2 knots),
 * with `extra` true one more RCTYPE2B, RCUT2B and KDEG2B value than the
 * line takes, as tadah 1.5.0 wrote them into a potential. */
Context joined_2b(bool potential, bool extra) {
  tadah::core::PeriodicTable::initialize();
  Context c;
  c.add("VERBOSE", 0);
  c.add("INIT2B", "true");
  c.add("INITMB", "false");
  c.add("TYPE2B", "D2_mJoin");
  c.add("TYPE2B", "D2_Blip 2 2 Ta Ta");
  c.add("TYPE2B", "D2_Knot5 2 2 Ta Ta");
  c.add("RCTYPE2B", extra ? "Cut_Cos Cut_Cos Cut_Tanh" : "Cut_Cos Cut_Cos");
  c.add("RCUT2B", extra ? "5 5 6" : "5 5");
  c.add("CGRID2B", "2 3 3 4.5");
  c.add("SGRID2B", "1 1");
  c.add("KDEG2B", extra ? "5 5 3" : "5 5");
  if (potential) c.add("WEIGHTS", "0.1 0.2 0.3 0.4 0.5");
  return c;
}

/* A many-body joined line of two DM_EAD descriptors, with `extra` true
 * one more RCTYPEMB and RCUTMB value than the line takes and an AMPGRIDMB
 * that DM_EAD does not read. */
Context joined_mb(bool potential, bool extra) {
  tadah::core::PeriodicTable::initialize();
  Context c;
  c.add("VERBOSE", 0);
  c.add("INIT2B", "false");
  c.add("INITMB", "true");
  c.add("TYPEMB", "DM_mJoin");
  c.add("TYPEMB", "DM_EAD 0 2 2 Ta Ta");
  c.add("TYPEMB", "DM_EAD 0 1 1 Ta Ta");
  c.add("RCTYPEMB", extra ? "Cut_Cos Cut_Cos Cut_Cos" : "Cut_Cos Cut_Cos");
  c.add("RCUTMB", extra ? "4.5 5 6" : "4.5 5");
  c.add("CGRIDMB", "2 3 2.5");
  c.add("SGRIDMB", "1 1 1");
  if (extra) c.add("AMPGRIDMB", "1 1");
  if (potential) c.add("WEIGHTS", "0.1 0.2 0.3 0.4");
  return c;
}

/* A single D2_Blip with two RCTYPE2B and RCUT2B values and a KDEG2B that
 * D2_Blip does not read; `clean` is the potential without them, the
 * largest RCUT2B value its cutoff. */
Context single_2b(bool potential, bool clean) {
  tadah::core::PeriodicTable::initialize();
  Context c;
  c.add("VERBOSE", 0);
  c.add("INIT2B", "true");
  c.add("INITMB", "false");
  c.add("TYPE2B", "D2_Blip 2 2 Ta Ta");
  c.add("RCTYPE2B", clean ? "Cut_Cos" : "Cut_Cos Cut_Tanh");
  c.add("RCUT2B", clean ? "6" : "5 6");
  c.add("CGRID2B", "2 3");
  c.add("SGRID2B", "1 1");
  if (!clean) c.add("KDEG2B", "5 5");
  if (potential) c.add("WEIGHTS", "0.1 0.2 0.3");
  return c;
}

/* A single DM_EAD with two RCTYPEMB and RCUTMB values (the larger one
 * first) and an AMPGRIDMB that DM_EAD does not read. */
Context single_mb(bool potential, bool clean) {
  tadah::core::PeriodicTable::initialize();
  Context c;
  c.add("VERBOSE", 0);
  c.add("INIT2B", "false");
  c.add("INITMB", "true");
  c.add("TYPEMB", "DM_EAD 0 2 2 Ta Ta");
  c.add("RCTYPEMB", clean ? "Cut_Cos" : "Cut_Cos Cut_Cos");
  c.add("RCUTMB", clean ? "5" : "5 4");
  c.add("CGRIDMB", "2 3");
  c.add("SGRIDMB", "1 1");
  if (!clean) c.add("AMPGRIDMB", "1 1");
  if (potential) c.add("WEIGHTS", "0.1 0.2 0.3");
  return c;
}

/* The two-body descriptor of `s` for a Ta-Ta pair at r. */
std::vector<double> aed_2b(DC_Selector &s, double r) {
  tadah::core::aed_type aed(s.descriptor_size());
  aed.set_zero();
  s.d2b->calc_aed(73, 73, r, r * r, aed, 1.0);
  return std::vector<double>(aed.data(), aed.data() + s.descriptor_size());
}

/* The many-body descriptor of `s` for one Ta neighbour at r. */
std::vector<double> aed_mb(DC_Selector &s, double r) {
  tadah::core::rho_type rho;
  s.dmb->init_rhoi(rho);
  s.dmb->calc_rho(73, 73, r, r * r, tadah::core::Vec3d(r, 0, 0), rho);
  tadah::core::aed_type aed(s.descriptor_size());
  aed.set_zero();
  s.dmb->calc_aed(rho, aed);
  return std::vector<double>(aed.data(), aed.data() + s.descriptor_size());
}

std::size_t count_of(const std::vector<std::string> &notices,
                     const std::string &notice) {
  return static_cast<std::size_t>(
      std::count(notices.begin(), notices.end(), notice));
}

std::string refusal(Context c) {
  try {
    D_mJoin::expand_grids(c);
  } catch (const std::exception &e) {
    return e.what();
  }
  return "";
}

const char *kJoined2B =
    "TYPE2B names 2 descriptors (D2_Blip, D2_Knot5), which take its first 2 "
    "values; the potential works unchanged without it";

} // namespace

/* A D2_mJoin potential with one more RCTYPE2B, RCUT2B and KDEG2B value than
 * its line takes loads (DC_Selector, as tadah predict and pair_style tadah
 * build it), each key reported once naming the value, and its model is the
 * potential's without them. Before the fix: refused ('TYPE2B names 2
 * descriptors (D2_Blip, D2_Knot5) but RCTYPE2B has 3 values: ...'). */
TEST_CASE("A joined two-body potential with values its descriptors do not "
          "take loads, the values reported and ignored",
          "[D_mJoin][list_keys][potential]") {
  Context pot = joined_2b(true, true);
  Context clean = joined_2b(true, false);
  std::vector<std::string> notices;
  {
    IgnoredKeyNoticeCapture cap;
    DC_Selector loaded(pot);
    notices = cap.notices();
    DC_Selector reference(clean);
    REQUIRE(loaded.descriptor_size() == reference.descriptor_size());
    for (const double r : {2.5, 4.0, 4.9, 5.5})
      CHECK_THAT(aed_2b(loaded, r),
                 Catch::Matchers::Approx(aed_2b(reference, r)).margin(1e-14));
  }
  INFO([&] {
    std::string s;
    for (const auto &n : notices) s += n + "\n";
    return s;
  }());
  CHECK(count_of(notices, std::string("Part of RCTYPE2B (Cut_Tanh) is "
                                      "ignored: ") + kJoined2B) == 1);
  CHECK(count_of(notices, std::string("Part of RCUT2B (6) is ignored: ") +
                              kJoined2B) == 1);
  CHECK(count_of(notices, std::string("Part of KDEG2B (3) is ignored: ") +
                              kJoined2B) == 1);
  CHECK(notices.size() == 3);
}

/* The same line in a training configuration is refused; training from the
 * potential (D_mJoin::expand_grids with WEIGHTS: tadah train and tadah hpo
 * with a potential as the training configuration) reports the values and
 * goes on. Before the fix: expand_grids refused the potential too. */
TEST_CASE("A joined two-body line with values its descriptors do not take: "
          "refused in a training configuration, reported in a potential",
          "[D_mJoin][list_keys][potential]") {
  CHECK(refusal(joined_2b(false, true)) ==
        "TYPE2B names 2 descriptors (D2_Blip, D2_Knot5) but RCTYPE2B has 3 "
        "values: each descriptor takes one RCTYPE2B value, in the order of "
        "the TYPE2B lines");
  {
    Context c = joined_2b(false, true);
    REQUIRE_THROWS_WITH(DC_Selector(c), Contains("RCTYPE2B has 3 values"));
  }
  Context pot = joined_2b(true, true);
  IgnoredKeyNoticeCapture cap;
  REQUIRE_NOTHROW(D_mJoin::expand_grids(pot));
  CHECK(count_of(cap.notices(), std::string("Part of RCUT2B (6) is "
                                            "ignored: ") + kJoined2B) == 1);
  CHECK(cap.notices().size() == 3);
}

/* A DM_mJoin potential: one more RCTYPEMB and RCUTMB value, and an AMPGRIDMB
 * that no descriptor of the line reads. Before the fix: refused ('TYPEMB
 * names 2 descriptors (DM_EAD, DM_EAD) but RCTYPEMB has 3 values: ...'). */
TEST_CASE("A joined many-body potential with values its descriptors do not "
          "take loads, the values reported and ignored",
          "[D_mJoin][list_keys][potential]") {
  const std::string who =
      "TYPEMB names 2 descriptors (DM_EAD, DM_EAD), which take ";
  std::vector<std::string> notices;
  {
    Context pot = joined_mb(true, true);
    Context clean = joined_mb(true, false);
    IgnoredKeyNoticeCapture cap;
    DC_Selector loaded(pot);
    notices = cap.notices();
    DC_Selector reference(clean);
    REQUIRE(loaded.descriptor_size() == reference.descriptor_size());
    for (const double r : {2.5, 4.0, 4.9, 5.5})
      CHECK_THAT(aed_mb(loaded, r),
                 Catch::Matchers::Approx(aed_mb(reference, r)).margin(1e-14));
  }
  CHECK(count_of(notices, "Part of RCTYPEMB (Cut_Cos) is ignored: " + who +
                              "its first 2 values; the potential works "
                              "unchanged without it") == 1);
  CHECK(count_of(notices, "Part of RCUTMB (6) is ignored: " + who +
                              "its first 2 values; the potential works "
                              "unchanged without it") == 1);
  CHECK(count_of(notices, "Key AMPGRIDMB (1 1) is ignored: " + who +
                              "none of its values; the potential works "
                              "unchanged without it") == 1);
  CHECK(notices.size() == 3);
  CHECK(refusal(joined_mb(false, true)) ==
        "TYPEMB names 2 descriptors (DM_EAD, DM_EAD) but RCTYPEMB has 3 "
        "values: each descriptor takes one RCTYPEMB value, in the order of "
        "the TYPEMB lines");
}

/* A single descriptor reads the first RCTYPE value, and its cutoff is the
 * largest RCUT value (RCUT2BMAX): a potential with two of each and a list
 * key the descriptor does not read loads as the potential with the first
 * RCTYPE value and the largest RCUT value alone, each key reported. A
 * training configuration with them is refused, as a joined line's is.
 * Before the fix: the potential loaded without a word. */
TEST_CASE("A single two-body descriptor's potential with values it does not "
          "read loads, the values reported",
          "[D_mJoin][list_keys][potential]") {
  std::vector<std::string> notices;
  {
    Context pot = single_2b(true, false);
    Context clean = single_2b(true, true);
    IgnoredKeyNoticeCapture cap;
    DC_Selector loaded(pot);
    notices = cap.notices();
    DC_Selector reference(clean);
    CHECK(loaded.d2b->get_rcut() == Approx(6.0));
    REQUIRE(loaded.descriptor_size() == reference.descriptor_size());
    for (const double r : {2.5, 4.9, 5.5})
      CHECK_THAT(aed_2b(loaded, r),
                 Catch::Matchers::Approx(aed_2b(reference, r)).margin(1e-14));
  }
  INFO([&] {
    std::string s;
    for (const auto &n : notices) s += n + "\n";
    return s;
  }());
  CHECK(count_of(notices, "Part of RCTYPE2B (Cut_Tanh) is ignored: TYPE2B "
                          "D2_Blip takes its first value; the potential "
                          "works unchanged without it") == 1);
  CHECK(count_of(notices, "Part of RCUT2B (5) is ignored: TYPE2B D2_Blip "
                          "takes one RCUT2B value, and its cutoff is the "
                          "largest, 6; the potential works unchanged with "
                          "RCUT2B 6 alone") == 1);
  CHECK(count_of(notices, "Key KDEG2B (5 5) is ignored: TYPE2B D2_Blip "
                          "takes none of its values; the potential works "
                          "unchanged without it") == 1);
  CHECK(notices.size() == 3);
  CHECK(refusal(single_2b(false, false)) ==
        "TYPE2B names 1 descriptor (D2_Blip) but RCTYPE2B has 2 values: each "
        "descriptor takes one RCTYPE2B value, in the order of the TYPE2B "
        "lines");
  // training from the potential reports and goes on
  Context pot = single_2b(true, false);
  IgnoredKeyNoticeCapture cap;
  REQUIRE_NOTHROW(D_mJoin::expand_grids(pot));
  CHECK(cap.notices().size() == 3);
}

TEST_CASE("A single many-body descriptor's potential with values it does "
          "not read loads, the values reported",
          "[D_mJoin][list_keys][potential]") {
  std::vector<std::string> notices;
  {
    Context pot = single_mb(true, false);
    Context clean = single_mb(true, true);
    IgnoredKeyNoticeCapture cap;
    DC_Selector loaded(pot);
    notices = cap.notices();
    DC_Selector reference(clean);
    CHECK(loaded.dmb->get_rcut() == Approx(5.0));
    REQUIRE(loaded.descriptor_size() == reference.descriptor_size());
    for (const double r : {2.5, 4.5})
      CHECK_THAT(aed_mb(loaded, r),
                 Catch::Matchers::Approx(aed_mb(reference, r)).margin(1e-14));
  }
  CHECK(count_of(notices, "Part of RCTYPEMB (Cut_Cos) is ignored: TYPEMB "
                          "DM_EAD takes its first value; the potential works "
                          "unchanged without it") == 1);
  CHECK(count_of(notices, "Part of RCUTMB (4) is ignored: TYPEMB DM_EAD "
                          "takes one RCUTMB value, and its cutoff is the "
                          "largest, 5; the potential works unchanged with "
                          "RCUTMB 5 alone") == 1);
  CHECK(count_of(notices, "Key AMPGRIDMB (1 1) is ignored: TYPEMB DM_EAD "
                          "takes none of its values; the potential works "
                          "unchanged without it") == 1);
  CHECK(notices.size() == 3);
}

/* A list key of the line's type that no descriptor of the line reads is
 * refused in a training configuration as a SIZE of 0 with values is: the
 * values would be dropped from the model and the potential. Before the
 * fix: accepted, and CGRID2B and SGRID2B left out of the potential without
 * a word. */
TEST_CASE("A list key that no descriptor of the line reads is refused in a "
          "training configuration",
          "[D_mJoin][list_keys]") {
  tadah::core::PeriodicTable::initialize();
  {
    Context c;
    c.add("VERBOSE", 0);
    c.add("INIT2B", "true");
    c.add("TYPE2B", "D2_LJ Ta Ta");
    c.add("RCTYPE2B", "Cut_Cos");
    c.add("RCUT2B", 5.0);
    c.add("CGRID2B", "1 2 3");
    CHECK(refusal(c) == "TYPE2B D2_LJ takes 0 CGRID2B values, but CGRID2B "
                        "has 3 values: TYPE2B D2_LJ Ta Ta");
  }
  {
    Context c;
    c.add("VERBOSE", 0);
    c.add("INIT2B", "true");
    c.add("TYPE2B", "D2_mJoin");
    c.add("TYPE2B", "D2_LJ Ta Ta");
    c.add("TYPE2B", "D2_MIE 12 6 Ta Ta");
    c.add("RCTYPE2B", "Cut_Cos Cut_Cos");
    c.add("RCUT2B", "5 5");
    c.add("CGRID2B", "1 2");
    c.add("SGRID2B", "1 1");
    CHECK(refusal(c) ==
          "TYPE2B names 2 descriptors (D2_LJ, D2_MIE), which take 0 CGRID2B "
          "values, but CGRID2B has 2 values: each descriptor takes its "
          "CGRID2B values, in the order of the TYPE2B lines");
  }
  {
    Context c;
    c.add("VERBOSE", 0);
    c.add("INITMB", "true");
    c.add("TYPEMB", "DM_EAD 0 2 2 Ta Ta");
    c.add("RCTYPEMB", "Cut_Cos");
    c.add("RCUTMB", 5.0);
    c.add("CGRIDMB", "2 3");
    c.add("SGRIDMB", "1 1");
    c.add("AMPGRIDMB", "1 1");
    CHECK(refusal(c) == "TYPEMB DM_EAD takes 0 AMPGRIDMB values, but "
                        "AMPGRIDMB has 2 values: TYPEMB DM_EAD 0 2 2 Ta Ta");
  }
  {
    // a key of the other line's type is not the line's
    Context c;
    c.add("VERBOSE", 0);
    c.add("INIT2B", "true");
    c.add("TYPE2B", "D2_Blip 2 2 Ta Ta");
    c.add("RCTYPE2B", "Cut_Cos");
    c.add("RCUT2B", 5.0);
    c.add("CGRID2B", "2 3");
    c.add("SGRID2B", "1 1");
    c.add("KDEGMB", "3");
    CHECK(refusal(c).empty());
  }
}

/* A value a single descriptor reads is not ignored: the descriptor reads
 * every value of its list keys, so a potential whose list key has more
 * values than its TYPE line's SIZE parameter asks for is refused, as the
 * configuration is. */
TEST_CASE("A single descriptor's potential whose list key disagrees with "
          "its TYPE line is refused",
          "[D_mJoin][list_keys][potential]") {
  Context pot = single_2b(true, true);
  pot.remove("CGRID2B");
  pot.remove("SGRID2B");
  pot.add("CGRID2B", "2 3 4");
  pot.add("SGRID2B", "1 1 1");
  REQUIRE_THROWS_WITH(DC_Selector(pot),
                      "TYPE2B D2_Blip takes 2 CGRID2B values, but CGRID2B "
                      "has 3 values: TYPE2B D2_Blip 2 2 Ta Ta");
}

/* A DM_Poly descriptor reads the CEMBFUNC and SEMBFUNC of its channel
 * function (F_RLR, F_Blip; F_SQRT reads SEMBFUNC when set) whole from the
 * context: its TYPE line has no SIZE parameter for them. A single DM_Poly
 * line keeps them whole through the grid split. Before the fix: tadah train
 * (D_mJoin::expand_grids) removed them from the configuration, so DM_PS_RLR
 * failed with 'Key not found in the Context object: SEMBFUNC' and
 * DM_PS_SQRT trained with every SEMBFUNC value 1. */
TEST_CASE("A single DM_Poly line keeps the CEMBFUNC and SEMBFUNC its "
          "channel function reads",
          "[D_mJoin][list_keys][dm_poly]") {
  tadah::core::PeriodicTable::initialize();
  const auto config = [](const std::string &line, bool cemb) {
    // F_Blip expands one density into its CEMBFUNC/SEMBFUNC channels
    const bool one = line.find("Blip") != std::string::npos;
    Context c;
    c.add("VERBOSE", 0);
    c.add("INIT2B", "false");
    c.add("INITMB", "true");
    c.add("TYPEMB", line);
    c.add("RCTYPEMB", "Cut_Cos");
    c.add("RCUTMB", 5.0);
    c.add("CGRIDMB", one ? "2" : "2 3");
    c.add("SGRIDMB", one ? "1" : "1 1");
    if (cemb) c.add("CEMBFUNC", "0.5 0.5");
    c.add("SEMBFUNC", "2 3");
    return c;
  };
  for (const std::string line :
       {"DM_PS_RLR 1 2 2 Ta Ta", "DM_GOW_Blip 1 1 1 Ta Ta",
        "DM_PS_SQRT 1 2 2 Ta Ta", "DM_OW_SQRT 1 2 2 Ta Ta"}) {
    INFO(line);
    const bool cemb = line.find("SQRT") == std::string::npos;
    Context c = config(line, cemb);
    REQUIRE_NOTHROW(D_mJoin::expand_grids(c));
    REQUIRE(c.contains("SEMBFUNC"));
    CHECK(c.size("SEMBFUNC") == 2);
    CHECK(c.get<double>("SEMBFUNC", 1) == 3.0);
    CHECK(c.contains("CEMBFUNC") == cemb);
    REQUIRE_NOTHROW(DC_Selector(c));
  }
  // DM_PS_SQRT reads SEMBFUNC: its descriptor differs from SEMBFUNC 1 1
  Context sqrt23 = config("DM_PS_SQRT 1 2 2 Ta Ta", false);
  D_mJoin::expand_grids(sqrt23);
  Context sqrt11 = sqrt23;
  sqrt11.remove("SEMBFUNC");
  sqrt11.add("SEMBFUNC", "1 1");
  DC_Selector s23(sqrt23), s11(sqrt11);
  const auto a = aed_mb(s23, 2.5), b = aed_mb(s11, 2.5);
  CHECK(a != b);
}

namespace {

/* A single DM_Poly line of `label` (L 1, 2 centres, 2 widths) with the
 * CEMBFUNC and SEMBFUNC its channel function reads whole. */
Context dm_poly(const std::string &label, bool potential) {
  tadah::core::PeriodicTable::initialize();
  Context c;
  c.add("VERBOSE", 0);
  c.add("INIT2B", "false");
  c.add("INITMB", "true");
  c.add("TYPEMB", label + " 1 2 2 Ta Ta");
  c.add("RCTYPEMB", "Cut_Cos");
  c.add("RCUTMB", 5.0);
  c.add("CGRIDMB", "2 3");
  c.add("SGRIDMB", "1 1");
  if (label.find("SQRT") == std::string::npos) c.add("CEMBFUNC", "0.5 0.5");
  c.add("SEMBFUNC", "2 3");
  if (potential) c.add("WEIGHTS", "0.1 0.2 0.3");
  return c;
}

void replace(Context &c, const std::string &key, const std::string &value) {
  c.remove(key);
  c.add(key, value);
}

} // namespace

/* A single DM_Poly line whose channel function reads CEMBFUNC/SEMBFUNC
 * whole is checked as any single line: in a training configuration more
 * RCTYPE/RCUT values than descriptors, more or fewer CGRIDMB values than its
 * SIZE parameter, and a list key it does not read (AMPGRIDMB) are refused;
 * in a potential the RCUT values below the largest and the unread key are
 * reported. The keys it reads whole are neither split nor reported.
 * Before the fix: the line was left to the descriptor, unchecked: each of
 * these trained and loaded without a word, DM_Poly reading every CGRIDMB
 * value whatever its SIZE parameter. */
TEST_CASE("A single DM_Poly line whose channel function reads CEMBFUNC or "
          "SEMBFUNC is checked as any single line",
          "[D_mJoin][list_keys][dm_poly]") {
  for (const std::string label : {"DM_PS_RLR", "DM_PS_SQRT", "DM_GOW_RLR"}) {
    INFO(label);
    const std::string line = "TYPEMB " + label + " 1 2 2 Ta Ta";
    CHECK(refusal(dm_poly(label, false)) == "");
    {
      Context c = dm_poly(label, false);
      replace(c, "RCUTMB", "5 6");
      CHECK(refusal(c) == "TYPEMB names 1 descriptor (" + label +
                              ") but RCUTMB has 2 values: each descriptor "
                              "takes one RCUTMB value, in the order of the "
                              "TYPEMB lines");
    }
    {
      Context c = dm_poly(label, false);
      replace(c, "CGRIDMB", "2 3 4");
      CHECK(refusal(c) == "TYPEMB " + label + " takes 2 CGRIDMB values, "
                          "but CGRIDMB has 3 values: " + line);
      replace(c, "CGRIDMB", "2");
      CHECK(refusal(c) == "TYPEMB " + label + " takes 2 CGRIDMB values, "
                          "but CGRIDMB has 1 value: " + line);
    }
    {
      Context c = dm_poly(label, false);
      c.add("AMPGRIDMB", "1 1");
      CHECK(refusal(c) == "TYPEMB " + label + " takes 0 AMPGRIDMB values, "
                          "but AMPGRIDMB has 2 values: " + line);
    }
    /* a potential: reported, and the model builds */
    Context pot = dm_poly(label, true);
    replace(pot, "RCUTMB", "5 6");
    pot.add("AMPGRIDMB", "1 1");
    std::vector<std::string> notices;
    {
      IgnoredKeyNoticeCapture cap;
      REQUIRE_NOTHROW(DC_Selector(pot));
      notices = cap.notices();
    }
    CHECK(count_of(notices, "Part of RCUTMB (5) is ignored: TYPEMB " + label +
                                " takes one RCUTMB value, and its cutoff is "
                                "the largest, 6; the potential works "
                                "unchanged with RCUTMB 6 alone") == 1);
    CHECK(count_of(notices, "Key AMPGRIDMB (1 1) is ignored: TYPEMB " +
                                label + " takes none of its values; the "
                                "potential works unchanged without it") == 1);
    CHECK(notices.size() == 2);
  }
}

/* The grids of such a line are expanded as any line's: OPTIM CGRIDMB (i)
 * addresses the i-th centre, and a written potential holds the values. The
 * keys it reads whole are expanded too.
 * Before the fix: the line was left to the descriptor with 'LIN 3 2 4.5' as
 * written, so CGRIDMB (2) was the generator's start, 2, not the second
 * centre, 3.25. */
TEST_CASE("The grids of a single DM_Poly line whose channel function reads "
          "SEMBFUNC are expanded",
          "[D_mJoin][list_keys][dm_poly]") {
  Context c = dm_poly("DM_PS_SQRT", false);
  replace(c, "TYPEMB", "DM_PS_SQRT 1 3 3 Ta Ta");
  replace(c, "CGRIDMB", "LIN 3 2 4.5");
  replace(c, "SGRIDMB", "GEOM 3 1 4");
  replace(c, "SEMBFUNC", "LIN 3 1 3");
  REQUIRE_NOTHROW(D_mJoin::expand_grids(c));
  REQUIRE(c.size("CGRIDMB") == 3);
  CHECK(c.get<double>("CGRIDMB", 1) == 3.25);
  REQUIRE(c.size("SGRIDMB") == 3);
  CHECK(c.get<double>("SGRIDMB", 2) == Approx(4.0));
  REQUIRE(c.size("SEMBFUNC") == 3);
  CHECK(c.get<double>("SEMBFUNC", 1) == 2.0);
  REQUIRE_NOTHROW(DC_Selector(c));
}

/* In a DM_mJoin line a descriptor takes its share of each list key: one
 * whose channel function needs CEMBFUNC and SEMBFUNC of its own (F_RLR,
 * F_Blip) cannot be joined, and is refused naming it. One whose channel
 * function reads SEMBFUNC only when set (F_SQRT) is joined with none, every
 * SEMBFUNC value 1, as a single line without SEMBFUNC.
 * Before the fix: the joined DM_PS_RLR was built without its keys ('Key not
 * found in the Context object: SEMBFUNC'). */
TEST_CASE("A DM_mJoin line refuses a DM_Poly descriptor that needs CEMBFUNC "
          "and SEMBFUNC of its own",
          "[D_mJoin][list_keys][dm_poly]") {
  const auto joined = [](const std::string &second) {
    tadah::core::PeriodicTable::initialize();
    Context c;
    c.add("VERBOSE", 0);
    c.add("INIT2B", "false");
    c.add("INITMB", "true");
    c.add("TYPEMB", "DM_mJoin");
    c.add("TYPEMB", "DM_EAD 0 1 1 Ta Ta");
    c.add("TYPEMB", second);
    c.add("RCTYPEMB", "Cut_Cos Cut_Cos");
    c.add("RCUTMB", "5 5");
    c.add("CGRIDMB", "2 2 3");
    c.add("SGRIDMB", "1 1 1");
    return c;
  };
  {
    Context c = joined("DM_PS_RLR 1 2 2 Ta Ta");
    c.add("CEMBFUNC", "0.5 0.5");
    c.add("SEMBFUNC", "2 3");
    const std::string what =
        "TYPEMB DM_PS_RLR (descriptor 2 of 2) reads CEMBFUNC and SEMBFUNC "
        "whole and cannot be joined: a DM_mJoin line gives each descriptor "
        "only its share of a list key; DM_PS_RLR can be the only TYPEMB "
        "descriptor";
    CHECK(refusal(c) == what);
    REQUIRE_THROWS_WITH(DC_Selector(c), what);
  }
  {
    Context c = joined("DM_PS_SQRT 1 2 2 Ta Ta");
    REQUIRE_NOTHROW(D_mJoin::expand_grids(c));
    REQUIRE_NOTHROW(DC_Selector(c));
  }
}

/* Training from a potential (tadah train -c pot.tadah, tadah hpo -c
 * pot.tadah) reports the values no descriptor takes and goes on with the
 * potential's model: D_mJoin::expand_grids leaves them out of the context,
 * so the training context (the potential without its WEIGHTS, as
 * Trainer::training_context makes it) builds that model and a retrained
 * potential is written without them. A single descriptor keeps its largest
 * RCUT value, its cutoff.
 * Before the fix: they stayed in the context, and the model of the training
 * context was refused ('TYPE2B names 2 descriptors (D2_Blip, D2_Knot5) but
 * RCTYPE2B has 3 values: ...'). */
TEST_CASE("Training from a potential with values its descriptors do not take "
          "leaves them out",
          "[D_mJoin][list_keys][potential]") {
  const auto retrain = [](Context pot) {
    REQUIRE_NOTHROW(D_mJoin::expand_grids(pot));
    pot.remove("WEIGHTS");
    return pot;
  };
  {
    Context c = retrain(joined_2b(true, true));
    CHECK(c.size("RCUT2B") == 2);
    CHECK(c.size("RCTYPE2B") == 2);
    CHECK(c.size("KDEG2B") == 2);
    std::unique_ptr<DC_Selector> s;
    REQUIRE_NOTHROW(s.reset(new DC_Selector(c)));
    Context clean = joined_2b(false, false);
    DC_Selector reference(clean);
    for (const double r : {2.5, 4.9})
      CHECK_THAT(aed_2b(*s, r),
                 Catch::Matchers::Approx(aed_2b(reference, r)).margin(1e-14));
  }
  {
    Context c = retrain(joined_mb(true, true));
    CHECK(c.size("RCUTMB") == 2);
    CHECK(c.size("RCTYPEMB") == 2);
    CHECK_FALSE(c.contains("AMPGRIDMB"));
    REQUIRE_NOTHROW(DC_Selector(c));
  }
  {
    Context c = retrain(single_2b(true, false));
    REQUIRE(c.size("RCUT2B") == 1);
    CHECK(c.get<double>("RCUT2B") == 6.0);
    REQUIRE(c.size("RCTYPE2B") == 1);
    CHECK(c.get<std::string>("RCTYPE2B") == "Cut_Cos");
    CHECK_FALSE(c.contains("KDEG2B"));
    std::unique_ptr<DC_Selector> s;
    REQUIRE_NOTHROW(s.reset(new DC_Selector(c)));
    CHECK(s->d2b->get_rcut() == Approx(6.0));
  }
  {
    /* what was left out, for the messages of tadah hpo */
    Context pot = single_2b(true, false);
    const auto left = D_mJoin::expand_grids(pot);
    REQUIRE(left.size() == 3);
    CHECK(left[0].key == "RCTYPE2B");
    CHECK(left[0].given == std::vector<std::string>{"Cut_Cos", "Cut_Tanh"});
    CHECK(left[0].kept == std::vector<std::string>{"Cut_Cos"});
    CHECK(left[1].key == "RCUT2B");
    CHECK(left[1].given == std::vector<std::string>{"5", "6"});
    CHECK(left[1].kept == std::vector<std::string>{"6"});
    CHECK(left[2].key == "KDEG2B");
    CHECK(left[2].given == std::vector<std::string>{"5", "5"});
    CHECK(left[2].kept.empty());
    /* a configuration, or a potential without such values: nothing */
    Context clean = single_2b(true, true);
    CHECK(D_mJoin::expand_grids(clean).empty());
  }
  {
    Context c = retrain(single_mb(true, false));
    REQUIRE(c.size("RCUTMB") == 1);
    CHECK(c.get<double>("RCUTMB") == 5.0);
    CHECK_FALSE(c.contains("AMPGRIDMB"));
    REQUIRE_NOTHROW(DC_Selector(c));
  }
}
