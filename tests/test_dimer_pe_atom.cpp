// test_dimer_pe_atom.cpp
//
// The per-atom energies (compute pe/atom) of pair_style tadah with a DIMER
// potential, the published Tadah!N2 (docs/source/mlips/tadah.n2: DIMER true
// 1.1014 true, M_BLR BF_Polynomial2, cutoff 12 A). compute_dimers evaluates
// every pair of molecules whose centres of mass are within the cutoff minus
// the bond length (10.90 A) as one cluster of four atoms, and each of the
// four atoms gets an energy from that cluster: the energy of the system is
// the sum over the clusters, and the per-atom energy of an atom is the sum
// over the clusters of every molecule pair it belongs to.
//
// The molecules are created here (create_atoms single, create_bonds
// single/bond) at the centre of a periodic 40 A box, so that no periodic
// image is within the neighbour cutoff (12 + 2 A) of any atom; newton off
// (a DIMER potential requires it), serial, in an in-process LAMMPS
// (LammpsRunner, the instance on hpo_lammps_comm()).
//
// It needs pair_style tadah, so a build without TADAH_ENABLE_NFP skips it,
// and atom_style bond, so a LAMMPS without the MOLECULE package skips it.

#include "tadah_test_skip.hpp"  // before catch2/catch.hpp
#include "catch2/catch.hpp"

#ifdef TADAH_ENABLE_NFP
#include <tadah/core/periodic_table.h>
#include <tadah/hpo/runners.h>

#include <input.h>
#include <library.h>
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <exception>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef TADAH_ENABLE_NFP
namespace {

const std::string kN2 = std::string(TADAH_PUBLISHED_MLIPS_DIR) + "/tadah.n2";
const double kBond = 1.1014;            // DIMER bond length of tadah.n2
const double kComCut = 12.0 - kBond;    // compute_dimers' centre-of-mass cutoff

std::string num(double v) {
  std::ostringstream o;
  o << std::setprecision(17) << v;
  return o.str();
}

/* Whether @p t is written as a finite number: digits, sign, point and
 * exponent only, so "nan" and "inf" are not (the build uses -ffast-math, so
 * a parsed NaN cannot be tested reliably). */
bool finite_number_text(const std::string& t) {
  return !t.empty() &&
         t.find_first_not_of("0123456789+-.eE") == std::string::npos &&
         t.find_first_of("0123456789") != std::string::npos;
}

/* An N2 molecule: centre of mass @p c, bond along the unit vector @p u. */
struct Molecule {
  std::array<double, 3> c, u;
};

/* The unit vector of polar angle @p theta and azimuth @p phi. */
std::array<double, 3> unit(double theta, double phi) {
  return {std::sin(theta) * std::cos(phi), std::sin(theta) * std::sin(phi),
          std::cos(theta)};
}

double com_distance(const Molecule& a, const Molecule& b) {
  double s = 0.0;
  for (int k = 0; k < 3; ++k) s += (a.c[k] - b.c[k]) * (a.c[k] - b.c[k]);
  return std::sqrt(s);
}

/* The energy and the per-atom energies (by atom id) of one `run 0`. */
struct Result {
  double pe = 0.0;      // thermo pe
  double pe_sum = 0.0;  // compute reduce sum of pe/atom
  std::map<int, double> pe_atom;
};

/* Reads the equal-style variable @p name as a finite number. */
double read_number(const LammpsRunner& runner,
                   const LammpsRunner::LMPPtr& lmp,
                   const std::string& name) {
  const std::string t = runner.get_variable(lmp, name);
  INFO("variable " << name << " = '" << t << "'");
  REQUIRE(finite_number_text(t));
  return std::stod(t);
}

/* True when the LAMMPS library has atom_style bond, which is part of the
 * LAMMPS MOLECULE package: the molecules here are bonded atom pairs.
 * Throws when no instance can be created, so that the test case fails
 * instead of being skipped. */
bool lammps_has_atom_style_bond() {
  std::string error;
  const auto lmp = LammpsRunner().create_instance(
      {"lmp", "-log", "none", "-screen", "none"}, &error);
  if (!lmp)
    throw std::runtime_error("no LAMMPS instance to look up atom_style "
                             "bond in: " + error);
  return lammps_has_style(lmp.get(), "atom", "bond") != 0;
}

/* `run 0` of pair_style tadah with tadah.n2 on @p mols: molecule m (from 0)
 * is atoms 2m + 1 and 2m + 2, bonded. */
Result run_molecules(const std::vector<Molecule>& mols) {
  std::vector<std::string> s = {
      "units metal",
      "atom_style bond",
      "atom_modify map array sort 0 0.0",
      "boundary p p p",
      "newton off",
      "region box block 0 40 0 40 0 40 units box",
      "create_box 1 box bond/types 1 extra/bond/per/atom 1 "
      "extra/special/per/atom 1",
      "mass 1 14.007"};
  for (const Molecule& m : mols)
    for (const double sign : {0.5, -0.5})
      s.push_back("create_atoms 1 single " +
                  num(m.c[0] + sign * kBond * m.u[0]) + " " +
                  num(m.c[1] + sign * kBond * m.u[1]) + " " +
                  num(m.c[2] + sign * kBond * m.u[2]) + " units box");
  s.push_back("bond_style zero");
  s.push_back("bond_coeff 1 " + num(kBond));
  for (std::size_t m = 0; m < mols.size(); ++m)
    s.push_back("create_bonds single/bond 1 " + std::to_string(2 * m + 1) +
                " " + std::to_string(2 * m + 2));
  s.push_back("pair_style tadah");
  s.push_back("pair_coeff * * \"" + kN2 + "\" N");
  s.push_back("neighbor 2.0 bin");
  s.push_back("compute pea all pe/atom");
  s.push_back("compute pes all reduce sum c_pea");
  s.push_back("thermo_style custom step pe c_pes");
  s.push_back("variable e equal pe");
  s.push_back("variable esum equal c_pes");
  for (std::size_t i = 1; i <= 2 * mols.size(); ++i)
    s.push_back("variable ea" + std::to_string(i) + " equal C_pea[" +
                std::to_string(i) + "]");
  s.push_back("run 0 post no");

  LammpsRunner runner;
  auto lmp = runner.create_instance(
      {"lmp", "-log", "none", "-screen", "none"});
  REQUIRE(lmp != nullptr);
  for (const std::string& line : s) {
    std::string err;
    try {
      lmp->input->one(line);
    } catch (const std::exception& e) {
      err = e.what();
    }
    INFO("LAMMPS: '" << line << "'");
    REQUIRE(err == "");
  }
  Result r;
  r.pe = read_number(runner, lmp, "e");
  r.pe_sum = read_number(runner, lmp, "esum");
  for (std::size_t i = 1; i <= 2 * mols.size(); ++i)
    r.pe_atom[static_cast<int>(i)] =
        read_number(runner, lmp, "ea" + std::to_string(i));
  return r;
}

}  // namespace
#endif

/* Pins: for 27 N2 molecules (centres on a 3 x 3 x 3 grid of spacing 4 A,
 * bonds in 27 directions), each of which has at least 19 others within the
 * centre-of-mass cutoff, the per-atom energies of pair_style tadah with
 * tadah.n2 sum to its energy: |sum(pe/atom) - pe| <= 1e-12 |pe|, and the
 * energy is not negligible (|pe| > 1e-2 eV).
 * Before the fix: compute_dimers set the per-atom energy of each atom of a
 * cluster to the cluster's energy for it (eatom[IJ[n]] = ei) instead of
 * adding it, so an atom kept only the energy of the last molecule pair it
 * was evaluated in: sum(pe/atom) -0.036475 eV against pe -0.61539 eV. */
TEST_CASE("pair_style tadah: the per-atom energies of a DIMER potential sum "
          "to its energy",
          "[dimer][lammps]") {
#ifndef TADAH_ENABLE_NFP
  TADAH_SKIP("pair_style tadah is not built (TADAH_ENABLE_NFP OFF)");
#else
  if (!lammps_has_atom_style_bond())
    TADAH_SKIP("this LAMMPS has no atom_style bond (the MOLECULE package)");
  tadah::core::PeriodicTable::initialize();
  std::vector<Molecule> mols;
  int k = 0;
  for (int i = -1; i <= 1; ++i)
    for (int j = -1; j <= 1; ++j)
      for (int l = -1; l <= 1; ++l, ++k)
        mols.push_back({{20.0 + 4.0 * i, 20.0 + 4.0 * j, 20.0 + 4.0 * l},
                        unit(0.37 + 1.13 * k, 0.71 + 2.39 * k)});
  std::size_t fewest = mols.size();
  for (const Molecule& a : mols) {
    std::size_t n = 0;
    for (const Molecule& b : mols)
      if (&a != &b && com_distance(a, b) <= kComCut) ++n;
    fewest = std::min(fewest, n);
  }
  REQUIRE(fewest >= 19);

  const Result r = run_molecules(mols);
  INFO("pe " << num(r.pe) << " eV, sum(pe/atom) " << num(r.pe_sum) << " eV");
  REQUIRE(std::fabs(r.pe) > 1e-2);
  CHECK(std::fabs(r.pe_sum - r.pe) <= 1e-12 * std::fabs(r.pe));
#endif
}

/* Pins: three N2 molecules A, B and C (atoms 1-2, 3-4, 5-6), each pair
 * within the centre-of-mass cutoff, are evaluated as the clusters AB, AC
 * and BC: the energy of the three is the sum of the energies of the three
 * pairs, each run on its own (|dE| <= 1e-12 of the largest), and the
 * per-atom energy of every atom of the three is the sum of its per-atom
 * energies in the two pairs it belongs to (an atom of A: AB and AC), to
 * 1e-12 of the largest per-atom energy. Every atom gets more than 1e-6 eV
 * in magnitude from each of its two pairs, so a per-atom energy that misses
 * either pair fails.
 * Before the fix: the energies summed (-0.014518 eV), but every per-atom
 * energy of the three was that of the pair evaluated last for that atom
 * (atom 1: -0.021000 eV, that of AC, where AB and AC give -0.034421 eV;
 * atom 3: -0.024453 eV, that of BC, where AB and BC give -0.037989 eV). */
TEST_CASE("pair_style tadah: a DIMER atom's per-atom energy is the sum over "
          "the molecule pairs it belongs to",
          "[dimer][lammps]") {
#ifndef TADAH_ENABLE_NFP
  TADAH_SKIP("pair_style tadah is not built (TADAH_ENABLE_NFP OFF)");
#else
  if (!lammps_has_atom_style_bond())
    TADAH_SKIP("this LAMMPS has no atom_style bond (the MOLECULE package)");
  tadah::core::PeriodicTable::initialize();
  const Molecule A{{20.0, 20.0, 20.0}, unit(0.4, 0.3)};
  const Molecule B{{23.6, 20.3, 19.8}, unit(1.3, 2.0)};
  const Molecule C{{21.4, 23.3, 20.6}, unit(2.2, 4.1)};
  REQUIRE(com_distance(A, B) <= kComCut);
  REQUIRE(com_distance(A, C) <= kComCut);
  REQUIRE(com_distance(B, C) <= kComCut);

  const Result abc = run_molecules({A, B, C});
  const Result ab = run_molecules({A, B});
  const Result ac = run_molecules({A, C});
  const Result bc = run_molecules({B, C});

  const double e_pairs = ab.pe + ac.pe + bc.pe;
  const double e_scale =
      std::max({std::fabs(ab.pe), std::fabs(ac.pe), std::fabs(bc.pe)});
  INFO("E(ABC) " << num(abc.pe) << " eV, E(AB) + E(AC) + E(BC) "
                 << num(e_pairs) << " eV");
  REQUIRE(e_scale > 1e-3);
  REQUIRE(std::fabs(abc.pe - e_pairs) <= 1e-12 * e_scale);

  /* Atom ids: in ABC, A = 1 2, B = 3 4, C = 5 6; in a pair run the first
   * molecule is 1 2 and the second 3 4. For each atom of ABC, its ids in
   * the two pair runs it belongs to. */
  struct AtomPairs {
    int id;
    const Result* p;
    int id_p;
    const Result* q;
    int id_q;
  };
  const std::vector<AtomPairs> atoms = {
      {1, &ab, 1, &ac, 1}, {2, &ab, 2, &ac, 2}, {3, &ab, 3, &bc, 1},
      {4, &ab, 4, &bc, 2}, {5, &ac, 3, &bc, 3}, {6, &ac, 4, &bc, 4}};
  double ea_scale = 0.0, epair_min = 1e300;
  for (const AtomPairs& a : atoms) {
    ea_scale = std::max(ea_scale, std::fabs(abc.pe_atom.at(a.id)));
    epair_min = std::min({epair_min, std::fabs(a.p->pe_atom.at(a.id_p)),
                          std::fabs(a.q->pe_atom.at(a.id_q))});
  }
  INFO("largest |pe/atom| in ABC " << ea_scale << " eV; smallest |pe/atom| "
       "of an atom in one of its pairs " << epair_min << " eV");
  REQUIRE(epair_min > 1e-6);
  for (const AtomPairs& a : atoms) {
    const double e = abc.pe_atom.at(a.id);
    const double e_p = a.p->pe_atom.at(a.id_p);
    const double e_q = a.q->pe_atom.at(a.id_q);
    INFO("atom " << a.id << ": pe/atom " << num(e) << " eV in ABC, "
         << num(e_p) << " + " << num(e_q) << " = " << num(e_p + e_q)
         << " eV in its two pairs");
    CHECK(std::fabs(e - (e_p + e_q)) <= 1e-12 * ea_scale);
  }
#endif
}
