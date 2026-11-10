// TYPE2B / TYPEMB lines: a descriptor name, then the parameters that
// descriptor takes (D2_Blip: the sizes of CGRID2B and SGRID2B; DM_Blip:
// Lmax and the sizes of CGRIDMB and SGRIDMB), then the element pairs it
// applies to. The key's format does not know how many parameters a
// descriptor takes (TYPE2B accepts a bare name, and both keys accept
// element pairs after fewer numbers than a descriptor takes), so the
// descriptor must reject a line that lacks its parameters, naming the key,
// the descriptor and what is missing:
//  - a line with fewer tokens than parameters crashed (a vector erase past
//    its end), for example `TYPE2B D2_Blip`, the form potential files had
//    before descriptors named their parameters;
//  - a line whose element pairs stand where the size parameters belong,
//    for example `TYPE2B D2_Blip * *` or `TYPEMB DM_Blip 1 N N`, was read
//    as parameters followed by no element pairs, so the descriptor applied
//    to no pair of elements and contributed nothing, without an error.

#include "catch2/catch.hpp"

#include <tadah/core/context.h>
#include <tadah/core/periodic_table.h>
#include <tadah/models/descriptors/d2/d2_blip.h>
#include <tadah/models/descriptors/dm/dm_blip.h>

#include <memory>
#include <string>
#include <vector>

using namespace tadah::core;
using namespace tadah::models;

namespace {

Context blip2b_context(const std::vector<std::string> &type2b) {
  PeriodicTable::initialize();
  Context c;
  c.add("INIT2B", "true");
  c.add("VERBOSE", 0);
  c.add("CGRID2B", "1.0 2.0");
  c.add("SGRID2B", "0.8 0.5");
  c.add("RCUT2B", 5.0);
  for (const auto &t : type2b) c.add("TYPE2B", t);
  return c;
}

Context blipmb_context(const std::vector<std::string> &typemb) {
  PeriodicTable::initialize();
  Context c;
  c.add("INITMB", "true");
  c.add("VERBOSE", 0);
  c.add("CGRIDMB", "1.0 2.0");
  c.add("SGRIDMB", "0.8 0.5");
  c.add("RCUTMB", 5.0);
  for (const auto &t : typemb) c.add("TYPEMB", t);
  return c;
}

} // namespace

TEST_CASE("TYPE2B lines must carry every parameter of their descriptor",
          "[D_Base][type_line]") {
  using Catch::Matchers::Contains;
  SECTION("no parameters") {
    const Context c = blip2b_context({"D2_Blip"});
    REQUIRE_THROWS_WITH(D2_Blip(c), Contains("TYPE2B D2_Blip") &&
                                        Contains("takes 2 parameters") &&
                                        Contains("has 0"));
  }
  SECTION("one of two parameters") {
    const Context c = blip2b_context({"D2_Blip", "2"});
    REQUIRE_THROWS_WITH(D2_Blip(c), Contains("TYPE2B D2_Blip") &&
                                        Contains("takes 2 parameters") &&
                                        Contains("has 1"));
  }
  SECTION("element pairs in place of both parameters") {
    const Context c = blip2b_context({"D2_Blip", "*", "*"});
    REQUIRE_THROWS_WITH(D2_Blip(c),
                        Contains("TYPE2B D2_Blip") &&
                            Contains("takes 2 parameters") &&
                            Contains("parameter 1, the number of CGRID2B "
                                     "values, must be a non-negative "
                                     "integer, not \"*\"") &&
                            Contains(": TYPE2B D2_Blip * *"));
  }
  SECTION("an element pair in place of the second parameter") {
    const Context c = blip2b_context({"D2_Blip", "2", "H", "H"});
    REQUIRE_THROWS_WITH(D2_Blip(c),
                        Contains("parameter 2, the number of SGRID2B "
                                 "values, must be a non-negative integer, "
                                 "not \"H\""));
  }
  SECTION("a complete line applies to its element pairs") {
    const Context c = blip2b_context({"D2_Blip", "2", "2", "H", "He"});
    std::unique_ptr<D2_Blip> d;
    REQUIRE_NOTHROW(d.reset(new D2_Blip(c)));
    CHECK(d->is_init_for_atoms(1, 2));
    CHECK_FALSE(d->is_init_for_atoms(1, 1));
  }
  SECTION("a complete line with wildcards applies to every pair") {
    const Context c = blip2b_context({"D2_Blip", "2", "2", "*", "*"});
    std::unique_ptr<D2_Blip> d;
    REQUIRE_NOTHROW(d.reset(new D2_Blip(c)));
    CHECK(d->is_init_for_atoms(7, 7));
    CHECK(d->is_init_for_atoms(1, 36));
  }
}

TEST_CASE("TYPEMB lines must carry every parameter of their descriptor",
          "[D_Base][type_line]") {
  using Catch::Matchers::Contains;
  SECTION("Lmax only") {
    // A file cannot hold this line (the key's format wants an element pair
    // after Lmax), but a Context built in code can.
    const Context c = blipmb_context({"DM_Blip", "1"});
    REQUIRE_THROWS_WITH(DM_Blip(c), Contains("TYPEMB DM_Blip") &&
                                        Contains("takes 3 parameters") &&
                                        Contains("has 1"));
  }
  SECTION("Lmax, then element pairs in place of the grid sizes") {
    // The key's format accepts this line (STRING UINT {STRING STRING}).
    const Context c = blipmb_context({"DM_Blip", "1", "N", "N"});
    REQUIRE_THROWS_WITH(DM_Blip(c),
                        Contains("TYPEMB DM_Blip") &&
                            Contains("takes 3 parameters") &&
                            Contains("parameter 2, the number of CGRIDMB "
                                     "values, must be a non-negative "
                                     "integer, not \"N\"") &&
                            Contains(": TYPEMB DM_Blip 1 N N"));
  }
  SECTION("a complete line applies to its element pairs") {
    const Context c = blipmb_context({"DM_Blip", "1", "2", "2", "H", "H"});
    std::unique_ptr<DM_Blip> d;
    REQUIRE_NOTHROW(d.reset(new DM_Blip(c)));
    CHECK(d->is_init_for_atoms(1, 1));
    CHECK_FALSE(d->is_init_for_atoms(1, 2));
  }
}
