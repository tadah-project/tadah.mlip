// test_eam_setfl_cache.cpp
//
// shared_setfl(path): the setfl file read and splined once for every
// descriptor of the process that reads the same file with the same
// contents (D2_EAM, DM_EAM). tadah hpo builds the descriptors of every
// evaluation anew; each used to parse and spline the file (about 12 ms for
// a 10001-point table, as long as a fit).

#include "catch2/catch.hpp"
#include <tadah/models/eam/eam_setfl_io.h>

#include <cstdio>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>

using namespace tadah::models;

namespace {

const char *const kSetfl = "tests_data/eam.alloy";

std::string file_text(const std::string &path) {
  std::ifstream in(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(in),
                     std::istreambuf_iterator<char>());
}

void write_text(const std::string &path, const std::string &text) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out << text;
}

}  // namespace

/* Pins: shared_setfl returns the same parsed and splined table for two
 * calls with one path while the file is unchanged, with the values a fresh
 * EAMSetflIO reads (rcut, nr, the z2r spline at r = 2.86 bit for bit);
 * after the file is rewritten with other contents (the fixture with its
 * first comment line changed, then the fixture of another element mass)
 * it returns another table with the new values; a missing file is
 * refused naming the path.
 * Before the fix: does not compile (shared_setfl is new); D2_EAM and
 * DM_EAM read and splined their setfl file in every constructor. */
TEST_CASE("shared_setfl parses a setfl file once while its contents are "
          "unchanged",
          "[eam][setfl_cache]") {
  const std::string path = "tests_data_eam_shared.alloy";
  const std::string text = file_text(kSetfl);
  REQUIRE(!text.empty());
  write_text(path, text);

  EAMSetflIO fresh;
  fresh.read_setfl(path);
  fresh.build_splines();

  const std::shared_ptr<const EAMSetflIO> a = shared_setfl(path);
  const std::shared_ptr<const EAMSetflIO> b = shared_setfl(path);
  REQUIRE(a);
  CHECK(a.get() == b.get());
  CHECK(a->path() == path);
  CHECK(a->rcut() == fresh.rcut());
  CHECK(a->nr() == fresh.nr());
  CHECK(a->z2_tab(2.86) == fresh.z2_tab(2.86));
  CHECK(a->F_tab(0.5 * a->rhomax()) == fresh.F_tab(0.5 * fresh.rhomax()));

  // Same values, other bytes (the first comment line): parsed again.
  std::string retitled = text;
  retitled[1] = retitled[1] == 'X' ? 'Y' : 'X';
  write_text(path, retitled);
  const std::shared_ptr<const EAMSetflIO> c = shared_setfl(path);
  CHECK(c.get() != a.get());
  CHECK(c->z2_tab(2.86) == fresh.z2_tab(2.86));

  // Other values: the new table has them.
  const std::string mass = "180.95";
  const std::size_t at = text.find(mass);
  REQUIRE(at != std::string::npos);
  std::string heavier = text;
  heavier.replace(at, mass.size(), "181.95");
  write_text(path, heavier);
  const std::shared_ptr<const EAMSetflIO> d = shared_setfl(path);
  CHECK(d.get() != c.get());
  CHECK(d->atomic_mass() == 181.95);
  CHECK(a->atomic_mass() == 180.95);   // a table in use is not changed

  std::remove(path.c_str());
  CHECK_THROWS_WITH(shared_setfl(path), Catch::Contains(path));
}
