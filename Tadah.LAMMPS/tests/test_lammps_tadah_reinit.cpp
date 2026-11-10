// LammpsTadah::initialize maps the LAMMPS types to the potential's elements
// on every pair_coeff, also when the same potential object is initialised
// again: a driver's potential (pair_coeff * * EXTERN, staged or given with
// PairTadah::load) is adopted by every EXTERN line of a script, which may map
// the types differently each time.

#include "catch2/catch.hpp"

#include <tadah/core/context.h>
#include <tadah/core/core_user_config_parser.h>
#include <tadah/core/periodic_table.h>
#include <tadah/tadah.lammps/lammps_tadah.h>

#include <string>
#include <vector>

using namespace tadah::core;
using tadah::LammpsTadah;

namespace {

/* ATOM Nb Ta, BIAS true, D2_BP (3 functions) for every pair: DSIZE 5 for
 * pair_style tadah (an intercept column per species, then 3), as in
 * test_weights_layout.cpp. */
const std::vector<std::string> kNbTa = {
    "ATOM Nb Ta",           "BIAS true",           "NORM false",
    "DIMER false 0 false",  "EAMFAST false",       "INIT2B true",
    "TYPE2B D2_BP 3 3 * *", "CGRID2B 2.4 3.1 3.9", "SGRID2B 1.0 1.2 0.9",
    "RCTYPE2B Cut_Cos",     "RCUT2B 5.0",          "INITMB false",
    "MODEL M_BLR BF_Linear", "WEIGHTS -8.2 -7.1 0.1 -0.05 0.02"};

/* lt->initialize as pair_coeff * * pot.tadah <species...> calls it. */
void initialize(LammpsTadah &lt, std::vector<std::string> species) {
  std::vector<std::string> args = {"*", "*", "pot.tadah"};
  args.insert(args.end(), species.begin(), species.end());
  std::vector<char *> argv;
  for (auto &a : args) argv.push_back(&a[0]);
  lt.initialize(static_cast<int>(argv.size()), argv.data());
}

}  // namespace

// Pins: one LammpsTadah of a BIAS true Nb-Ta potential initialised for
// `* * pot.tadah Ta Nb` maps type 1 to Ta (Z 73) and type 2 to Nb (Z 41),
// each with its intercept column and energy (the weight of that column,
// -7.1 for Ta and -8.2 for Nb); initialised again for `* * pot.tadah Ta
// NULL` it maps type 2 to no element: Z 0, no intercept column (-1) and an
// intercept energy of 0, as a fresh object initialised for `Ta NULL` has.
// Before the fix: initialize() resized Z and skipped a NULL type, so type 2
// kept Z 41 from the first mapping, and with it Nb's intercept column (0)
// and energy (-8.2).
TEST_CASE("LammpsTadah::initialize maps a NULL type to no element when the "
          "object is initialised again",
          "[lammps_tadah][initialize]") {
  PeriodicTable::initialize();
  LammpsTadah lt(CoreUserConfigParser().parse(kNbTa));
  initialize(lt, {"Ta", "Nb"});
  REQUIRE(lt.Z == std::vector<int>{73, 41});
  REQUIRE(lt.species_intercept_idx_per_type[0] >= 0);
  REQUIRE(lt.species_intercept_idx_per_type[1] >= 0);
  CHECK(lt.bias_per_atom_type == std::vector<double>{-7.1, -8.2});

  initialize(lt, {"Ta", "NULL"});
  CHECK(lt.Z == std::vector<int>{73, 0});
  CHECK(lt.species_intercept_idx_per_type[1] == -1);
  CHECK(lt.bias_per_atom_type[1] == 0.0);

  LammpsTadah fresh(CoreUserConfigParser().parse(kNbTa));
  initialize(fresh, {"Ta", "NULL"});
  CHECK(fresh.Z == lt.Z);
  CHECK(fresh.species_intercept_idx_per_type ==
        lt.species_intercept_idx_per_type);
  CHECK(fresh.bias_per_atom_type == lt.bias_per_atom_type);
}
