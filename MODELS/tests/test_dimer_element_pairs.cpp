// DIMER potentials and element pairs.
//
// The DIMER evaluators -- DescriptorsCalc::calc_dimer (Tadah.MLIP, `tadah
// train` and `tadah predict`) and PairTadah::compute_dimers (pair_style
// tadah) -- evaluate every atom as Z = 1, whatever its element. A two-body
// or many-body descriptor declared for specific element pairs
// (`TYPE2B D2_Blip 4 4 N N`) then applies to no pair the evaluation sees,
// and the model predicted from zero descriptors without a word. Both
// evaluators refuse such a potential when it is loaded, through
// require_dimer_element_pairs(): each active descriptor must apply to
// every element pair, i.e. be declared with `* *`.

#include "catch2/catch.hpp"

#include <tadah/core/context.h>
#include <tadah/core/periodic_table.h>
#include <tadah/models/descriptors/d_all.h>
#include <tadah/models/descriptors/dimer_element_pairs.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace tadah::core;
using namespace tadah::models;

namespace {

const char *kDimerError =
    "DIMER evaluates every atom as Z=1; declare TYPE2B/TYPEMB with * * "
    "element pairs";

/* A D2_Blip (two functions) and a DM_Blip (Lmax 1, two functions) for the
 * given element pairs; `dimer` sets DIMER, `init2b`/`initmb` switch the
 * descriptors on. */
Context blip_context(const std::string &pairs2b, const std::string &pairsmb,
                     bool dimer, bool init2b = true, bool initmb = true) {
  PeriodicTable::initialize();
  Context c;
  c.remove("VERBOSE");
  c.add("VERBOSE", 0);
  c.remove("INIT2B");
  c.add("INIT2B", init2b ? "true" : "false");
  c.remove("INITMB");
  c.add("INITMB", initmb ? "true" : "false");
  c.add("TYPE2B", "D2_Blip 2 2 " + pairs2b);
  c.add("CGRID2B", "1.0 2.0");
  c.add("SGRID2B", "0.8 0.5");
  c.add("RCUT2B", 5.0);
  c.add("TYPEMB", "DM_Blip 1 2 2 " + pairsmb);
  c.add("CGRIDMB", "1.0 2.0");
  c.add("SGRIDMB", "0.8 0.5");
  c.add("RCUTMB", 5.0);
  c.remove("DIMER");
  c.add("DIMER", dimer ? "true 1.1 true" : "false 0 false");
  return c;
}

/* A DM_Blip that reports a non-zero size although INITMB switches it off
 * (every descriptor in MODELS then has size 0): a descriptor that ignored
 * INITMB. calc_dimer does not evaluate a switched-off descriptor, so the
 * element-pair rule must not check it, whatever its size. */
struct DM_BlipSizedWhenOff : DM_Blip {
  explicit DM_BlipSizedWhenOff(const Context &c) : DM_Blip(c) { s = 4; }
};

/* A D2_mJoin of two D2_Blip children with the given element pairs. */
Context d2_mjoin_context(const std::string &pairs1,
                         const std::string &pairs2) {
  PeriodicTable::initialize();
  Context c;
  c.remove("VERBOSE");
  c.add("VERBOSE", 0);
  c.remove("INIT2B");
  c.add("INIT2B", "true");
  c.add("RCTYPE2B", "Cut_Cos");
  c.add("RCTYPE2B", "Cut_Cos");
  c.add("RCUT2B", 5.0);
  c.add("RCUT2B", 5.0);
  c.add("TYPE2B", "D2_mJoin D2_Blip 1 1 " + pairs1 +
                              " D2_Blip 1 1 " + pairs2);
  c.add("CGRID2B", "1.0 2.0");
  c.add("SGRID2B", "0.8 0.5");
  return c;
}

/* A DM_mJoin of two DM_Blip children with the given element pairs. */
Context dm_mjoin_context(const std::string &pairs1,
                         const std::string &pairs2) {
  PeriodicTable::initialize();
  Context c;
  c.remove("VERBOSE");
  c.add("VERBOSE", 0);
  c.remove("INITMB");
  c.add("INITMB", "true");
  c.add("RCTYPEMB", "Cut_Cos");
  c.add("RCTYPEMB", "Cut_Cos");
  c.add("RCUTMB", 5.0);
  c.add("RCUTMB", 5.0);
  c.add("TYPEMB", "DM_mJoin DM_Blip 0 1 1 " + pairs1 +
                              " DM_Blip 1 1 1 " + pairs2);
  c.add("CGRIDMB", "1.0 2.0");
  c.add("SGRIDMB", "0.8 0.5");
  return c;
}

} // namespace

TEST_CASE("A descriptor applies to every element pair only when its "
          "element pairs include * * (a wildcard pair)",
          "[D_Base][dimer]") {
  for (const auto &p : {std::make_pair("* *", true),
                        std::make_pair("N N", false),
                        std::make_pair("* N", false),
                        std::make_pair("N *", false),
                        std::make_pair("H H", false),
                        std::make_pair("N N * *", true)}) {
    INFO("element pairs: " << p.first);
    const Context c = blip_context(p.first, p.first, false);
    CHECK(D2_Blip(c).applies_to_every_element_pair() == p.second);
    CHECK(DM_Blip(c).applies_to_every_element_pair() == p.second);
  }
}

TEST_CASE("An mJoin descriptor applies to every element pair only when "
          "each child does", "[D_Base][mjoin][dimer]") {
  for (const auto &p : {std::make_pair(std::make_pair("* *", "* *"), true),
                        std::make_pair(std::make_pair("* *", "N N"), false),
                        std::make_pair(std::make_pair("N N", "* *"), false),
                        std::make_pair(std::make_pair("N N", "N N"), false)}) {
    INFO("children: " << p.first.first << " / " << p.first.second);
    Context c2 = d2_mjoin_context(p.first.first, p.first.second);
    CHECK(D2_mJoin(c2).applies_to_every_element_pair() == p.second);
    Context cm = dm_mjoin_context(p.first.first, p.first.second);
    CHECK(DM_mJoin(cm).applies_to_every_element_pair() == p.second);
  }
}

TEST_CASE("require_dimer_element_pairs refuses a DIMER potential whose "
          "descriptors name specific element pairs",
          "[dimer][element_pairs]") {
  using Catch::Matchers::Contains;
  SECTION("two-body descriptor for N N") {
    const Context c = blip_context("N N", "* *", true);
    const D2_Blip d2(c);
    const DM_Blip dm(c);
    REQUIRE_THROWS_WITH(require_dimer_element_pairs(c, d2, dm),
                        Contains(kDimerError) &&
                            Contains("TYPE2B D2_Blip 2 2 N N"));
  }
  SECTION("many-body descriptor for * N") {
    const Context c = blip_context("* *", "* N", true);
    const D2_Blip d2(c);
    const DM_Blip dm(c);
    REQUIRE_THROWS_WITH(require_dimer_element_pairs(c, d2, dm),
                        Contains(kDimerError) &&
                            Contains("TYPEMB DM_Blip 1 2 2 * N"));
  }
  SECTION("every descriptor for * *") {
    const Context c = blip_context("* *", "* *", true);
    const D2_Blip d2(c);
    const DM_Blip dm(c);
    REQUIRE_NOTHROW(require_dimer_element_pairs(c, d2, dm));
  }
  SECTION("specific element pairs without DIMER") {
    const Context c = blip_context("N N", "N N", false);
    const D2_Blip d2(c);
    const DM_Blip dm(c);
    REQUIRE_NOTHROW(require_dimer_element_pairs(c, d2, dm));
  }
  SECTION("a descriptor that is switched off is not checked") {
    const Context c = blip_context("* *", "N N", true, true, false);
    const D2_Blip d2(c);
    const DM_Dummy dm(c);
    REQUIRE_NOTHROW(require_dimer_element_pairs(c, d2, dm));
  }
  // The two sections below each isolate one of the two reasons a
  // descriptor is not checked: switched off (INITMB false), and empty.
  SECTION("a switched-off descriptor with specific element pairs is not "
          "checked, whatever its size") {
    const Context c = blip_context("* *", "N N", true, true, false);
    const D2_Blip d2(c);
    const DM_BlipSizedWhenOff dm(c);
    REQUIRE(dm.size() > 0);
    REQUIRE_FALSE(dm.applies_to_every_element_pair());
    REQUIRE_NOTHROW(require_dimer_element_pairs(c, d2, dm));
  }
  SECTION("a switched-on descriptor of size 0 is not checked") {
    const Context c = blip_context("* *", "N N", true, true, true);
    const D2_Blip d2(c);
    const DM_Dummy dm(c);
    REQUIRE(dm.size() == 0);
    REQUIRE_FALSE(dm.applies_to_every_element_pair());
    REQUIRE_NOTHROW(require_dimer_element_pairs(c, d2, dm));
  }
}
