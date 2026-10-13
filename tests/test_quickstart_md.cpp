// test_quickstart_md.cpp
//
// The LAMMPS example of the Quick Start page: docs/source/quickstart/in.ml
// with the potential shipped next to it, pot.tadah, run as a user runs it
// (`lmp -in in.ml` in that directory). The script is read from the docs
// directory; its pair_coeff line must be `pair_coeff * * pot.tadah Ta`, and
// only the file name is replaced by the quoted absolute path of the shipped
// pot.tadah. It runs in an in-process LAMMPS (LammpsRunner, serial) with a
// log file, and the thermo rows of its `run 1000` (thermo 100: steps 0 to
// 1000) are read from that log.
//
// A potential and a starting lattice that fit each other conserve the
// total energy of NVE MD at the script's timestep, start near zero
// pressure, lose about half of the initial temperature to the potential
// energy (equipartition, from a perfect lattice), and keep the bcc crystal.
// A crystal far from the potential's own lattice tears apart at fixed
// volume instead and turns the released potential energy into heat.
//
// It needs pair_style tadah, so a build without TADAH_ENABLE_NFP skips it.

#include "tadah_test_skip.hpp"  // before catch2/catch.hpp
#include "catch2/catch.hpp"

#ifdef TADAH_ENABLE_NFP
#include <tadah/core/periodic_table.h>
#include <tadah/hpo/runners.h>

#include <atom.h>
#endif

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <unistd.h>

#ifdef TADAH_ENABLE_NFP
namespace {

const std::string kQuickstartDir = TADAH_QUICKSTART_DIR;

std::vector<std::string> read_lines(const std::string& path) {
  std::ifstream in(path);
  REQUIRE(in.is_open());
  std::vector<std::string> lines;
  for (std::string line; std::getline(in, line);) lines.push_back(line);
  return lines;
}

std::string read_text(const std::string& path) {
  std::ifstream in(path);
  std::stringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

std::vector<std::string> tokens_of(const std::string& line) {
  std::istringstream in(line);
  std::vector<std::string> out;
  for (std::string t; in >> t;) out.push_back(t);
  return out;
}

/* Whether @p t is written as a finite number: digits, sign, point and
 * exponent only, so "nan" and "inf" are not (the build uses -ffast-math, so
 * a parsed NaN cannot be tested reliably). */
bool finite_number_text(const std::string& t) {
  return !t.empty() &&
         t.find_first_not_of("0123456789+-.eE") == std::string::npos &&
         t.find_first_of("0123456789") != std::string::npos;
}

/* Removes a scratch file however the test case ends. */
struct RemoveOnExit {
  std::string path;
  ~RemoveOnExit() { std::remove(path.c_str()); }
};

/* One thermo row of `thermo_style custom step temp vol press pe etotal`. */
struct Row {
  double step, temp, vol, press, pe, etotal;
};

/* The thermo rows of the run in @p log: the rows of six numbers between
 * the header "Step Temp Volume Press PotEng TotEng" and "Loop time".
 * Another line there (a WARNING) is not a row. */
std::vector<Row> thermo_rows(const std::string& log) {
  const std::vector<std::string> header = {"Step",  "Temp",   "Volume",
                                           "Press", "PotEng", "TotEng"};
  std::istringstream in(log);
  std::vector<Row> rows;
  bool inside = false;
  for (std::string line; std::getline(in, line);) {
    const std::vector<std::string> t = tokens_of(line);
    if (t == header) {
      inside = true;
      continue;
    }
    if (!inside) continue;
    if (line.rfind("Loop time", 0) == 0) break;
    if (t.size() != 6 ||
        !std::all_of(t.begin(), t.end(), finite_number_text))
      continue;
    rows.push_back({std::stod(t[0]), std::stod(t[1]), std::stod(t[2]),
                    std::stod(t[3]), std::stod(t[4]), std::stod(t[5])});
  }
  return rows;
}

std::string num(double v) {
  std::ostringstream o;
  o << std::setprecision(17) << v;
  return o.str();
}

}  // namespace
#endif

/* Pins: docs/source/quickstart/in.ml (its pair_coeff line exactly
 * `pair_coeff * * pot.tadah Ta`) with the shipped pot.tadah runs its
 * `run 1000` to the end, 250 atoms, thermo rows at steps 0, 100, ..., 1000,
 * and
 *   - the total energy stays within 1 meV/atom of its step-0 value at every
 *     row (max |TotEng_k - TotEng_0| / N < 1e-3 eV);
 *   - the temperature at step 1000 is below the initial 1000 K;
 *   - the step-0 pressure is below 1e5 bar (10 GPa) in magnitude: the
 *     lattice of in.ml is close to the potential's own;
 *   - the potential energy rises, or falls by less than 0.05 eV/atom, from
 *     step 0 to step 1000: (PotEng_1000 - PotEng_0) / N > -0.05 eV;
 *   - after the run, common neighbour analysis (compute cna/atom, cutoff
 *     (1 + sqrt 2)/2 times the bcc lattice constant of the final volume)
 *     finds at least 90% of the atoms in a bcc environment.
 * Before the fix: the shipped potential was a D2_LJ pair potential with a
 * hard cutoff (Cut_Dummy at 6.5 A) fitted to energies at one volume, whose
 * own bcc lattice is near 2.9 A, and in.ml built bcc at 3.5 A. The run
 * completed and every check above failed: TotEng wandered by 3.46e-2
 * eV/atom, T went from 1000 K to 8542 K, the step-0 pressure was -7.40e5
 * bar, PotEng fell by 0.965 eV/atom as the crystal tore into dense
 * clusters, and no atom was left in a bcc environment. */
TEST_CASE("quickstart: in.ml with the shipped pot.tadah conserves energy and "
          "keeps the bcc crystal",
          "[quickstart][lammps]") {
#ifndef TADAH_ENABLE_NFP
  TADAH_SKIP("pair_style tadah is not built (TADAH_ENABLE_NFP OFF)");
#else
  tadah::core::PeriodicTable::initialize();

  /* in.ml, its pair_coeff line pointing at the shipped potential. */
  std::vector<std::string> script = read_lines(kQuickstartDir + "/in.ml");
  const std::string pot = kQuickstartDir + "/pot.tadah";
  REQUIRE(pot.find('"') == std::string::npos);
  int pair_coeff_lines = 0;
  for (std::string& line : script) {
    const std::vector<std::string> t = tokens_of(line);
    if (t.empty() || t[0] != "pair_coeff") continue;
    ++pair_coeff_lines;
    REQUIRE(t == std::vector<std::string>{"pair_coeff", "*", "*",
                                          "pot.tadah", "Ta"});
    line = "pair_coeff * * \"" + pot + "\" Ta";
  }
  REQUIRE(pair_coeff_lines == 1);

  const std::string log_path =
      "quickstart_md_" + std::to_string(::getpid()) + ".log";
  const RemoveOnExit log_guard{log_path};

  LammpsRunner runner;
  auto lmp = runner.create_instance(
      {"lmp", "-log", log_path, "-screen", "none"});
  REQUIRE(lmp != nullptr);
  if (!runner.run_script(lmp, script, tadah::core::Context{})) {
    lmp.reset();  // closes the log
    FAIL("in.ml stopped; LAMMPS log:\n" << read_text(log_path));
  }
  const double natoms = static_cast<double>(lmp->atom->natoms);
  REQUIRE(natoms == 250.0);

  /* Common neighbour analysis of the final configuration, at the bcc
   * lattice constant of the final volume (N/2 conventional cells). */
  REQUIRE(runner.run_script(lmp, {"variable qs_vol equal vol"},
                            tadah::core::Context{}));
  const std::string vol_text = runner.get_variable(lmp, "qs_vol");
  REQUIRE(finite_number_text(vol_text));
  const double a_final = std::cbrt(std::stod(vol_text) / (natoms / 2.0));
  const double cna_cut = 0.5 * (1.0 + std::sqrt(2.0)) * a_final;
  REQUIRE(runner.run_script(
      lmp,
      {"compute qs_cna all cna/atom " + num(cna_cut),
       "variable qs_is_bcc atom c_qs_cna==3",
       "compute qs_bcc all reduce sum v_qs_is_bcc",
       "variable qs_nbcc equal c_qs_bcc",
       "run 0 post no"},
      tadah::core::Context{}));
  const std::string nbcc_text = runner.get_variable(lmp, "qs_nbcc");
  lmp.reset();  // closes the log

  /* The rows of `run 1000` (the CNA `run 0` above prints its own block
   * after the first "Loop time"). */
  const std::string log = read_text(log_path);
  const std::vector<Row> rows = thermo_rows(log);
  if (rows.size() != 11)
    FAIL(rows.size() << " thermo rows, expected 11; LAMMPS log:\n" << log);
  for (std::size_t k = 0; k < rows.size(); ++k) {
    INFO("row " << k);
    CHECK(rows[k].step == 100.0 * static_cast<double>(k));
  }

  const Row& first = rows.front();
  const Row& last = rows.back();
  double max_dev = 0.0;
  for (const Row& r : rows)
    max_dev = std::max(max_dev, std::fabs(r.etotal - first.etotal) / natoms);
  const double dpe = (last.pe - first.pe) / natoms;
  REQUIRE(finite_number_text(nbcc_text));
  const double nbcc = std::stod(nbcc_text);

  INFO("max |TotEng - TotEng_0| / N = " << max_dev << " eV/atom");
  INFO("T: " << first.temp << " K -> " << last.temp << " K");
  INFO("P(step 0) = " << first.press << " bar");
  INFO("(PotEng_1000 - PotEng_0) / N = " << dpe << " eV/atom");
  INFO("bcc atoms after the run: " << nbcc << " of " << natoms
       << " (cna/atom cutoff " << cna_cut << " A)");
  CHECK(max_dev < 1e-3);
  CHECK(last.temp < first.temp);
  CHECK(std::fabs(first.press) < 1e5);
  CHECK(dpe > -0.05);
  CHECK(nbcc >= 0.9 * natoms);
#endif
}
