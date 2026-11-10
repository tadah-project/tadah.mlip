#include "catch2/catch.hpp"
#include <tadah/models/eam/eam_lattice.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

using namespace tadah::models;

TEST_CASE("eam_lattice nearest-neighbour distances", "[eam_lattice]") {
  const double a = 3.30;
  REQUIRE(eam_lattice::nn_distance("bcc", a) ==
          Approx(a * std::sqrt(3.0) / 2.0));
  REQUIRE(eam_lattice::nn_distance("fcc", a) == Approx(a / std::sqrt(2.0)));
  REQUIRE(eam_lattice::nn_distance("hcp", a) == Approx(a));
  // unknown falls back to bcc
  REQUIRE(eam_lattice::nn_distance("dia", a) ==
          Approx(a * std::sqrt(3.0) / 2.0));
}

TEST_CASE("eam_lattice neighbour multiplicities and counts", "[eam_lattice]") {
  const double a = 3.0;
  auto count_at = [](const std::vector<double> &d, double r) {
    return std::count_if(d.begin(), d.end(), [&](double x) {
      return std::fabs(x - r) < 1e-6;
    });
  };

  SECTION("bcc: 8 first + 6 second neighbours") {
    auto d = eam_lattice::neighbour_distances("bcc", a, 1.05 * a);
    REQUIRE(count_at(d, a * std::sqrt(3.0) / 2.0) == 8);
    REQUIRE(count_at(d, a) == 6);
    REQUIRE(d.size() == 14);
  }
  SECTION("fcc: 12 first neighbours") {
    auto d = eam_lattice::neighbour_distances("fcc", a, 0.99 * a);
    REQUIRE(count_at(d, a / std::sqrt(2.0)) == 12);
    REQUIRE(d.size() == 12);
  }
  SECTION("hcp (ideal c/a): 12 first neighbours") {
    auto d = eam_lattice::neighbour_distances("hcp", a, 1.01 * a);
    REQUIRE(count_at(d, a) == 12);
    REQUIRE(d.size() == 12);
  }
  SECTION("all distances within cutoff and positive") {
    for (const char *lat : {"bcc", "fcc", "hcp"}) {
      auto d = eam_lattice::neighbour_distances(lat, a, 2.5 * a);
      REQUIRE(!d.empty());
      for (double x : d) {
        REQUIRE(x > 0.0);
        REQUIRE(x <= 2.5 * a);
      }
    }
  }
}

TEST_CASE("eam_lattice supported()", "[eam_lattice]") {
  REQUIRE(eam_lattice::supported("bcc"));
  REQUIRE(eam_lattice::supported("fcc"));
  REQUIRE(eam_lattice::supported("hcp"));
  REQUIRE(!eam_lattice::supported("dia"));
  REQUIRE(!eam_lattice::supported(""));
  // supported() expects the canonical lower-case form (the setfl reader
  // normalises the header label); raw upper case is intentionally not matched.
  REQUIRE(!eam_lattice::supported("FCC"));
}

TEST_CASE("eam_lattice crystal_epa", "[eam_lattice]") {
  const double a = 3.0;
  auto one = [](double) { return 1.0; };   // phi = rho = 1
  auto idF = [](double x) { return x; };    // F(x) = x
  // With phi=rho=1 and F=id, E/atom = 0.5*sum(phi) + F(sum(rho)) = 1.5 * Nnbr,
  // so crystal_epa reduces to a clean neighbour-count check.
  SECTION("fcc: 12 first neighbours -> 1.5*12") {
    REQUIRE(eam_lattice::crystal_epa("fcc", a, 0.99 * a, one, one, idF) ==
            Approx(1.5 * 12));
  }
  SECTION("bcc: 8+6 neighbours -> 1.5*14") {
    REQUIRE(eam_lattice::crystal_epa("bcc", a, 1.05 * a, one, one, idF) ==
            Approx(1.5 * 14));
  }
  SECTION("hcp (ideal c/a): 12 first neighbours -> 1.5*12") {
    REQUIRE(eam_lattice::crystal_epa("hcp", a, 1.01 * a, one, one, idF) ==
            Approx(1.5 * 12));
  }
  SECTION("unsupported lattice -> NaN") {
    REQUIRE(std::isnan(eam_lattice::crystal_epa("dia", a, 2.0 * a, one, one,
                                                idF)));
  }
}

TEST_CASE("eam_lattice require_crystal", "[eam_lattice]") {
  // accepts fcc/bcc/hcp with a0>0, case-insensitively
  REQUIRE_NOTHROW(eam_lattice::require_crystal("fcc", 3.6));
  REQUIRE_NOTHROW(eam_lattice::require_crystal("FCC", 3.6));
  REQUIRE_NOTHROW(eam_lattice::require_crystal("BCC", 2.8));
  REQUIRE_NOTHROW(eam_lattice::require_crystal("HCP", 2.9));
  // rejects unspecified / unsupported lattice, and non-positive a0
  REQUIRE_THROWS(eam_lattice::require_crystal("NONE", 0.0));
  REQUIRE_THROWS(eam_lattice::require_crystal("", 3.6));
  REQUIRE_THROWS(eam_lattice::require_crystal("dia", 5.0));
  REQUIRE_THROWS(eam_lattice::require_crystal("fcc", 0.0));
}

/* Pins: reach_distance(lattice, a0, s) is s x nn_distance(lattice, a0)
 * (the nearest-neighbour distance of the crystal compressed to a/a0 = s,
 * EAMRefit's r_reach), bit for bit. require_crystal_on_grid(lattice, a0,
 * reach_smin, dr) applies require_crystal's rules (its message for NONE,
 * an unknown lattice, a0 <= 0) and then refuses a crystal whose nearest
 * neighbours, at a/a0 = reach_smin, are less than the table's step dr
 * apart: hcp with reach_smin 1 (the distance is a0) passes at a0 = dr
 * exactly and is refused one ulp below it; the fcc lattice of a0 = 1e-323
 * (a subnormal) with reach_smin 0.1, whose distance underflows to 0, is
 * refused with "tadah refit: the lattice constant of the EAM header,
 * a0 = 1e-323 A, puts the nearest neighbours of the fcc crystal 0 A apart
 * at the strongest compression the refit tests (a/a0 = REFIT_REACH_SMIN
 * 0.1), less than the step of the table's radial grid, dr = 0.0125313 A:
 * the table describes no pair distance of that crystal. The a0 of the element
 * line of the setfl file (`<Z> <mass> <a0> <lattice>`) is the equilibrium
 * lattice constant in Angstrom."; real crystals (Cu, Fe, Ti
 * at REFIT_REACH_SMIN 0.1 to 1) pass. The gate also takes the table's
 * cutoff, rcut, whose rule the next test pins: every crystal here has its
 * nearest neighbours below the rcut it is given (5 A, or 1 A for the
 * boundary at a0 = dr), so these expectations are those of the gate
 * without it.
 * Before the fix: neither function exists (the crystal gate was
 * require_crystal alone, a known lattice and a0 > 0): the test does not
 * compile. The message then ended "Edit the element line of the setfl file
 * (...): set the equilibrium lattice constant, in Angstrom.", an
 * instruction. */
TEST_CASE("eam_lattice require_crystal_on_grid: the crystal's nearest "
          "neighbours at REFIT_REACH_SMIN at least the table's dr apart",
          "[eam_lattice]") {
  CHECK(eam_lattice::reach_distance("fcc", 3.6, 0.75) ==
        0.75 * (3.6 / std::sqrt(2.0)));
  CHECK(eam_lattice::reach_distance("bcc", 2.87, 0.1) ==
        0.1 * (2.87 * std::sqrt(3.0) / 2.0));
  CHECK(eam_lattice::reach_distance("hcp", 2.95, 1.0) == 2.95);

  // real crystals pass, at the ends of REFIT_REACH_SMIN's range too
  for (const double s : {0.1, 0.75, 1.0}) {
    INFO("reach_smin " << s);
    CHECK_NOTHROW(eam_lattice::require_crystal_on_grid("fcc", 3.615, s,
                                                       0.0125313, 5.0));
    CHECK_NOTHROW(eam_lattice::require_crystal_on_grid("BCC", 2.8665, s,
                                                       0.005, 5.0));
    CHECK_NOTHROW(eam_lattice::require_crystal_on_grid("hcp", 2.95, s,
                                                       0.006, 5.0));
  }

  // require_crystal's rules first, with its message
  for (const auto &bad : {std::make_pair(std::string("NONE"), 0.0),
                          std::make_pair(std::string("dia"), 5.0),
                          std::make_pair(std::string("fcc"), 0.0)}) {
    INFO("lattice " << bad.first << ", a0 " << bad.second);
    CHECK_THROWS_WITH(eam_lattice::require_crystal_on_grid(
                          bad.first, bad.second, 0.75, 0.01, 5.0),
                      Catch::Contains("crystal usable for the faithfulness "
                                      "self-test"));
  }

  // the boundary: hcp at reach_smin 1, whose distance is a0 itself
  const double dr = 0.0125;
  CHECK_NOTHROW(eam_lattice::require_crystal_on_grid("hcp", dr, 1.0, dr, 1.0));
  const double below = std::nextafter(dr, 0.0);
  CHECK_THROWS_WITH(
      eam_lattice::require_crystal_on_grid("HCP", below, 1.0, dr, 1.0),
      "tadah refit: the lattice constant of the EAM header, a0 = "
      "0.012499999999999999 A, puts the nearest neighbours of the hcp "
      "crystal 0.012499999999999999 A apart at the strongest compression "
      "the refit tests (a/a0 = REFIT_REACH_SMIN 1), less than the step of "
      "the table's radial grid, dr = 0.0125 A: the table describes no pair "
      "distance of that crystal. The a0 of the element line of the setfl file "
      "(`<Z> <mass> <a0> <lattice>`) is the equilibrium lattice constant "
      "in Angstrom.");

  // a subnormal a0: the distance underflows to 0
  const double tiny = 1e-323;
  REQUIRE(tiny > 0.0);
  REQUIRE(eam_lattice::reach_distance("fcc", tiny, 0.1) == 0.0);
  CHECK_THROWS_WITH(
      eam_lattice::require_crystal_on_grid("fcc", tiny, 0.1, 0.0125313, 5.0),
      "tadah refit: the lattice constant of the EAM header, a0 = 1e-323 A, "
      "puts the nearest neighbours of the fcc crystal 0 A apart at the "
      "strongest compression the refit tests (a/a0 = REFIT_REACH_SMIN "
      "0.1), less than the step of the table's radial grid, dr = "
      "0.0125313 A: the table describes no pair distance of that crystal. "
      "The a0 of the element line of the setfl file (`<Z> <mass> <a0> "
      "<lattice>`) is the equilibrium lattice constant in Angstrom.");
}

/* Pins: require_crystal_on_grid(lattice, a0, reach_smin, dr, rcut), the
 * crystal gate of `tadah refit`, refuses a crystal whose nearest
 * neighbours at a0 are at or beyond the table's cutoff rcut, where LAMMPS
 * counts no pair (pair_style eam/alloy counts a pair below rcut only): the
 * crystal of the header has no interaction in LAMMPS, the refit's
 * self-test would compare crystals without neighbours (at most a shell at
 * exactly rcut, which tadah's enumeration keeps: neighbour_distances("hcp",
 * 5, 5) holds the 12 nearest neighbours at 5) and the compact fit's
 * window would start at or above rcut. The fcc lattice of a0 = 361 (a
 * lattice constant of copper in picometres) at rcut 5 is refused with
 * exactly "tadah refit: the lattice constant of the EAM header, a0 = 361
 * A, puts the nearest neighbours of the fcc crystal 255.26554800834364 A
 * apart, at or beyond the table's cutoff, rcut = 5 A, and LAMMPS
 * (pair_style eam/alloy) counts a pair only below rcut: the crystal has no
 * interaction in LAMMPS. The a0 of the element line of the setfl file (`<Z>
 * <mass> <a0> <lattice>`) is the equilibrium lattice constant in Angstrom
 * (a value in picometres is 100 times the value in Angstrom)." --
 * before the dr rule, which it passes (its distance at REFIT_REACH_SMIN is
 * 191 A); the boundary is exact: hcp (whose distance is a0) is refused at
 * a0 = rcut, with the same sentence, and passes one ulp below it, and bcc
 * likewise at a0 = rcut / (sqrt(3)/2). A crystal whose neighbours lie
 * below rcut at a0 but beyond it at the scan's a/a0 = 1.10 passes (a real
 * expanded crystal), and so do the crystals of real tables.
 * Before the fix: the refusal said "...: the crystal has no pair within
 * the cutoff." (false for tadah, which counts the hcp shell at exactly
 * rcut), so the two exact-text checks fail; the neighbour_distances pin
 * passes as well (a pin of why the text names LAMMPS). Its last sentence
 * then read "Edit the element line of the setfl file (...): set the
 * equilibrium lattice constant, ...", an instruction. */
TEST_CASE("eam_lattice require_crystal_on_grid: the crystal's nearest "
          "neighbours at a0 below the table's cutoff",
          "[eam_lattice]") {
  CHECK_THROWS_WITH(
      eam_lattice::require_crystal_on_grid("fcc", 361.0, 0.75, 0.0125313,
                                           5.0),
      "tadah refit: the lattice constant of the EAM header, a0 = 361 A, "
      "puts the nearest neighbours of the fcc crystal 255.26554800834364 A "
      "apart, at or beyond the table's cutoff, rcut = 5 A, and LAMMPS "
      "(pair_style eam/alloy) counts a pair only below rcut: the crystal has "
      "no interaction in LAMMPS. The a0 of the element line of the setfl file "
      "(`<Z> <mass> <a0> <lattice>`) is the equilibrium lattice constant in "
      "Angstrom (a value in picometres is 100 times the value in "
      "Angstrom).");

  // the boundary: hcp, whose nearest-neighbour distance is a0 itself;
  // tadah's enumeration counts its shell at exactly rcut, LAMMPS does not
  const double rcut = 5.0;
  CHECK_THROWS_WITH(
      eam_lattice::require_crystal_on_grid("HCP", rcut, 0.75, 0.01, rcut),
      Catch::Contains("puts the nearest neighbours of the hcp crystal 5 A "
                      "apart, at or beyond the table's cutoff, rcut = 5 A, "
                      "and LAMMPS (pair_style eam/alloy) counts a pair only "
                      "below rcut: the crystal has no interaction in "
                      "LAMMPS."));
  {
    const std::vector<double> shell =
        eam_lattice::neighbour_distances("hcp", rcut, rcut);
    CHECK(shell.size() == 12);
    for (const double d : shell) CHECK(d == Approx(rcut));
  }
  CHECK_NOTHROW(eam_lattice::require_crystal_on_grid(
      "hcp", std::nextafter(rcut, 0.0), 0.75, 0.01, rcut));
  // bcc at the a0 whose distance is rcut, and one ulp of a0 below it
  double a_bcc = rcut / (std::sqrt(3.0) / 2.0);
  while (eam_lattice::nn_distance("bcc", a_bcc) < rcut)
    a_bcc = std::nextafter(a_bcc, 2.0 * rcut);
  CHECK_THROWS_WITH(
      eam_lattice::require_crystal_on_grid("bcc", a_bcc, 0.75, 0.01, rcut),
      Catch::Contains("at or beyond the table's cutoff, rcut = 5 A"));
  const double a_below = std::nextafter(a_bcc, 0.0);
  REQUIRE(eam_lattice::nn_distance("bcc", a_below) < rcut);
  CHECK_NOTHROW(
      eam_lattice::require_crystal_on_grid("bcc", a_below, 0.75, 0.01, rcut));

  // nearest neighbours below rcut at a0, beyond it at a/a0 = 1.10: passes
  const double a_fcc = 0.95 * rcut * std::sqrt(2.0);
  REQUIRE(1.10 * eam_lattice::nn_distance("fcc", a_fcc) > rcut);
  CHECK_NOTHROW(
      eam_lattice::require_crystal_on_grid("fcc", a_fcc, 0.75, 0.01, rcut));

  // real tables: Cu (Mishin), Fe (Mendelev), Ta, Ti
  CHECK_NOTHROW(
      eam_lattice::require_crystal_on_grid("fcc", 3.615, 0.75, 9e-4, 5.50679));
  CHECK_NOTHROW(
      eam_lattice::require_crystal_on_grid("bcc", 2.855, 0.1, 5.3e-4, 5.3));
  CHECK_NOTHROW(eam_lattice::require_crystal_on_grid("bcc", 3.304, 0.75,
                                                     1.1166e-3, 5.58191));
  CHECK_NOTHROW(
      eam_lattice::require_crystal_on_grid("hcp", 2.95, 1.0, 6e-4, 6.0));

  // require_crystal's refusals still come first
  CHECK_THROWS_WITH(
      eam_lattice::require_crystal_on_grid("NONE", 0.0, 0.75, 0.01, 5.0),
      Catch::Contains("crystal usable for the faithfulness self-test"));
}

/* Pins: the refusal of a setfl header without a crystal the self-test can
 * use gives a0 as its shortest round-trip text and says where the crystal
 * comes from: "tadah refit: the EAM header does not specify a crystal
 * usable for the faithfulness self-test (lattice='NONE', a0=0). The
 * self-test needs one of fcc, bcc and hcp and a positive lattice constant,
 * which the element line of the setfl file (`<Z> <mass> <a0> <lattice>`)
 * gives: the ground-state lattice, fcc, bcc or hcp in place of a
 * placeholder such as NONE, and the equilibrium lattice constant." (a0
 * 2.5e-7 is "2.5e-07").
 * Before the fix: a0 printed by std::to_string ("a0=0.000000"), and the
 * message ended "Edit the element line of the setfl file (...): set the
 * ground-state lattice (e.g. change 'NONE' to 'fcc', 'bcc' or 'hcp') and
 * the equilibrium lattice constant.". */
TEST_CASE("require_crystal says where the crystal of the self-test comes "
          "from",
          "[eam_lattice]") {
  CHECK_THROWS_WITH(
      eam_lattice::require_crystal("NONE", 0.0),
      "tadah refit: the EAM header does not specify a crystal usable for "
      "the faithfulness self-test (lattice='NONE', a0=0). The self-test "
      "needs one of fcc, bcc and hcp and a positive lattice constant, which "
      "the element line of the setfl file (`<Z> <mass> <a0> <lattice>`) "
      "gives: the ground-state lattice, fcc, bcc or hcp in place of a "
      "placeholder such as NONE, and the equilibrium lattice constant.");
  CHECK_THROWS_WITH(eam_lattice::require_crystal("", 2.5e-7),
                    Catch::Contains("(lattice='<missing>', a0=2.5e-07)"));
}

/* Pins: neighbour_distances(lattice, a, rcut) enumerates a cube of
 * ceil(rcut / a) + 1 cells each way (a the cell's shortest edge for bcc
 * and fcc) with an int index, and refuses a cube that index cannot reach
 * -- rcut / a not finite or above 2147483645 -- with std::invalid_argument
 * naming the lattice, a, rcut and rcut / a: "eam_lattice::neighbour_
 * distances: the neighbours of the fcc lattice of lattice constant a =
 * 1e-300 within r = 5 lie up to 4.9999999999999997e+300 lattice vectors
 * away, more than the enumeration's int index reaches (2147483645)"; a = 0
 * (rcut / a infinite) likewise; crystal_epa, which enumerates through it,
 * too (hcp). A cube
 * it reaches is enumerated as before (fcc a = 3: 12 first neighbours).
 * Before the fix: ceil(rcut / a) + 1 was converted to int unchecked, which
 * is undefined above INT_MAX; on x86-64 it gave INT_MIN + 1, the loops ran
 * no iteration and the function returned no neighbour (so the refit's
 * self-test compared crystals without neighbours): no exception is thrown
 * and the checks fail. */
TEST_CASE("eam_lattice neighbour_distances refuses a cube its int index "
          "cannot enumerate",
          "[eam_lattice]") {
  const auto refusal = [](const std::string &lattice, double a, double rcut) {
    try {
      (void)eam_lattice::neighbour_distances(lattice, a, rcut);
    } catch (const std::invalid_argument &e) {
      return std::string(e.what());
    }
    return std::string("no exception");
  };
  CHECK(refusal("fcc", 1e-300, 5.0) ==
        "eam_lattice::neighbour_distances: the neighbours of the fcc "
        "lattice of lattice constant a = 1e-300 within r = 5 lie up to "
        "4.9999999999999997e+300 lattice vectors away, more than the "
        "enumeration's int index reaches (2147483645)");
  CHECK(refusal("bcc", 0.0, 5.0) ==
        "eam_lattice::neighbour_distances: the neighbours of the bcc "
        "lattice of lattice constant a = 0 within r = 5 lie up to inf "
        "lattice vectors away, more than the enumeration's int index "
        "reaches (2147483645)");
  CHECK_THROWS_AS(eam_lattice::crystal_epa(
                      "hcp", 1e-300, 5.0, [](double) { return 1.0; },
                      [](double) { return 1.0; }, [](double x) { return x; }),
                  std::invalid_argument);
  CHECK(eam_lattice::neighbour_distances("fcc", 3.0, 0.99 * 3.0).size() == 12);
}
