// test_pair_coeff_reload.cpp
//
// pair_style tadah when a LAMMPS script gives it another potential: a second
// `pair_coeff * * <file>` (with or without `pair_style tadah` again, which
// keeps the pair style), a `pair_coeff * * EXTERN` (the potential a driver
// such as tadah hpo gives it: staged with PairTadah::stage_extern, or passed
// to the pair style with PairTadah::load) after a file and a file after it.
// Every case runs `run 0` in an in-process LAMMPS (LammpsRunner, the
// instance on hpo_lammps_comm()) and compares the energy and the force norm
// with those of a fresh LAMMPS instance that was given only the potential
// expected.
//
// The potentials (each written to a file by the case, except the shipped
// Quick Start potential):
//   A   docs/source/quickstart/pot.tadah: D2_Blip + DM_mBlipEnv, the generic
//       many-body kernel (compute_2b_mb_half);
//   B   a D2_LJ pair potential of Ta (the weights of Tadah.LAMMPS's
//       tests_data/pots/ta_lj_cutcos.tadah, an earlier Quick Start
//       potential, Cut_Cos at 6.5 A): the pair fast path (compute_2b_fast);
//   C   the weights of A with ATOM Ta Nb, both descriptors for every element
//       pair and WATOM 0.309 (Ta), 0.25 (Nb): two species in the generic
//       kernel;
//   D   ATOM Ta Nb with one D2_LJ block for Ta-Ta pairs only (the weights of
//       B): a pure two-body potential of two elements, collapsed to a pair
//       table for the element of the one LAMMPS type;
//   E   the descriptors of A with MODEL M_BLR BF_Polynomial2 (Tadah.LAMMPS's
//       tests_data/pots/ta_poly2_blip_env.tadah): a non-linear model, the
//       full-list kernel (compute_2b_mb_full).
// The cell: 54 atoms of bcc Ta at 3.30 A (3x3x3 cells), every atom moved by
// up to 0.05 A (displace_atoms, a fixed seed); with two LAMMPS types, the
// atoms of even ID are type 2.
//
// It needs pair_style tadah, so a build without TADAH_ENABLE_NFP skips it.

#include "tadah_test_skip.hpp"  // before catch2/catch.hpp
#include "catch2/catch.hpp"

#ifdef TADAH_ENABLE_NFP
#include <tadah/core/context.h>
#include <tadah/core/core_user_config_parser.h>
#include <tadah/core/periodic_table.h>
#include <tadah/hpo/hpo_mpi.h>  // hpo_lammps_comm()
#include <tadah/hpo/runners.h>
#include <tadah/tadah.lammps/lammps_tadah.h>

#include <force.h>
#include <input.h>
#include <pair_tadah.h>
#endif

#include <cmath>
#include <cstdio>
#include <exception>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include <unistd.h>

#ifdef TADAH_ENABLE_NFP
namespace {

using LMPPtr = LammpsRunner::LMPPtr;
using tadah::core::Context;

const std::string kQuickstartPot =
    std::string(TADAH_QUICKSTART_DIR) + "/pot.tadah";

/* The LAMMPS error of a run whose pair style has no potential. */
const std::string kNoPotential =
    "pair_style tadah has no potential: the last pair_coeff, or a driver's "
    "PairTadah::load(), failed (see its error)";

/* B: a D2_LJ pair potential of Ta. */
const char* const kLjTa =
    "ATOM     Ta\n"
    "BIAS     false\n"
    "DIMER     false     0     false\n"
    "INIT2B     true\n"
    "INITMB     false\n"
    "MODEL     M_KRR     Kern_Linear     1\n"
    "NORM     false\n"
    "RCTYPE2B     Cut_Cos\n"
    "RCUT2B     6.5\n"
    "TYPE2B     D2_LJ     Ta     Ta\n"
    "WATOM     73\n"
    "WEIGHTS     14.204213349318765225     2604.396965191186154698\n";

/* C: two species, the weights of the Quick Start potential. */
const char* const kTaNb =
    "ATOM              Ta   Nb\n"
    "BIAS              false\n"
    "CEMBFUNC          0.5   0.75   1   1.25   1.5\n"
    "CGRID2B           1   2.43333333333333357   3.8666666666666667   "
    "5.29999999999999982\n"
    "CGRIDMB           2.87999999999999989\n"
    "DIMER             false   0   false\n"
    "INIT2B            true\n"
    "INITMB            true\n"
    "MODEL             M_KRR   Kern_Linear   1\n"
    "NORM              false\n"
    "RCTYPE2B          Cut_Cos\n"
    "RCTYPEMB          Cut_Cos\n"
    "RCUT2B            5.29999999999999982\n"
    "RCUTENV           0.599999999999999978\n"
    "RCUTMB            5\n"
    "SEMBFUNC          2   2   2   2   2\n"
    "SGRID2B           0.100000000000000033   0.21544346900318842   "
    "0.464158883361277919   0.999999999999999889\n"
    "SGRIDMB           8\n"
    "TYPE2B            D2_Blip   4   4   *   *\n"
    "TYPEMB            DM_mBlipEnv   0   1   1   5   5   *   *\n"
    "WATOM             0.309   0.25\n"
    "WEIGHTS           133.357774127816555   -123.755171524655708   "
    "-10.4804763244958963   -18.0240635733812411   -6.3685917658913116   "
    "4.13446130558423164   -8.25236089850869803   3.95181987589451467   "
    "-6.04877887227442557\n";

/* D: two elements, a D2_LJ block for Ta-Ta pairs only. */
const char* const kLjTaNb =
    "ATOM     Ta     Nb\n"
    "BIAS     false\n"
    "DIMER     false     0     false\n"
    "INIT2B     true\n"
    "INITMB     false\n"
    "MODEL     M_KRR     Kern_Linear     1\n"
    "NORM     false\n"
    "RCTYPE2B     Cut_Cos\n"
    "RCUT2B     6.5\n"
    "TYPE2B     D2_LJ     Ta     Ta\n"
    "WATOM     73     41\n"
    "WEIGHTS     14.204213349318765225     2604.396965191186154698\n";

/* E: the descriptors of A, M_BLR BF_Polynomial2. */
const char* const kPoly2Ta =
    "ATOM           Ta\n"
    "BIAS           false\n"
    "CEMBFUNC       0.5   0.75   1   1.25   1.5\n"
    "CGRID2B        1   2.43333333333333357   3.8666666666666667   "
    "5.29999999999999982\n"
    "CGRIDMB        2.87999999999999989\n"
    "DIMER          false   0   false\n"
    "ESHIFT_ATOM    false\n"
    "ESHIFT_DBATOM  false\n"
    "EWEIGHT        1\n"
    "FORCE          false\n"
    "FWEIGHT        1\n"
    "INIT2B         true\n"
    "INITMB         true\n"
    "LAMBDA         9.99999999999999955e-07\n"
    "LSCALE         1\n"
    "MODEL          M_BLR   BF_Polynomial2\n"
    "NORM           false\n"
    "OALGO          1\n"
    "RCTYPE2B       Cut_Cos\n"
    "RCTYPEMB       Cut_Cos\n"
    "RCUT2B         5.29999999999999982\n"
    "RCUTENV        0.599999999999999978\n"
    "RCUTMB         5\n"
    "SEMBFUNC       2   2   2   2   2\n"
    "SGRID2B        0.100000000000000033   0.21544346900318842   "
    "0.464158883361277919   0.999999999999999889\n"
    "SGRIDMB        8\n"
    "STRESS         true\n"
    "SWEIGHT        1\n"
    "TYPE2B         D2_Blip   4   4   Ta   Ta\n"
    "TYPEMB         DM_mBlipEnv   0   1   1   5   5   Ta   Ta\n"
    "WATOM          0.308999999999999997\n"
    "WDBFILE_AUTO   0\n"
    "WEIGHTS        35.6589071552540062   3.66711246876482999   "
    "14.9821352083998462   -12.4461670658942953   14.060495641135903   "
    "27.9792881344421716   30.3471487066385563   15.9190005053368999   "
    "6.12829099592373083   -29.3880382421684416   -20.744416849726278   "
    "-16.4494268799491081   -15.175935537523813   -28.2283761585886346   "
    "-28.4440519312849673   -19.3570852772099649   -5.57760681970864169   "
    "9.21625647879266552   9.04338740218760329   -10.3591861183464697   "
    "-6.53965411235208727   -4.34017711925866401   -6.94451471019293542   "
    "-6.43237890112177002   10.906007073899481   1.86438238966215297   "
    "-0.100395027583860208   -2.76981440226984965   1.01810964978230811   "
    "4.78557005401223812   2.65269873759534658   -3.399949756931961   "
    "-3.84867454231402339   3.76349850002657149   1.45800417715656239   "
    "2.67600202037784918   2.04035500797920477   1.19021697509395263   "
    "-0.826996247584006028   -1.78279673009389872   "
    "-2.02101303656082765   5.40584654332275161   -0.845723481246445252   "
    "3.79298054776447202   -6.06431610500038332\n"
    "ZERO_COM_FORCE false\n";

/* A potential file written for the test case, removed however it ends. */
struct PotFile {
  std::string path;
  PotFile(const std::string& tag, const char* text)
      : path("pair_coeff_reload_" + std::to_string(::getpid()) + "_" + tag +
             ".tadah") {
    std::ofstream(path) << text;
  }
  ~PotFile() { std::remove(path.c_str()); }
  PotFile(const PotFile&) = delete;
  PotFile& operator=(const PotFile&) = delete;
};

/* Removes a scratch file however the test case ends. */
struct RemoveOnExit {
  std::string path;
  ~RemoveOnExit() { std::remove(path.c_str()); }
};

/* Stages @p pot for `pair_coeff * * EXTERN` (PairTadah::stage_extern, as
 * LammpsRunner::run_script does); the slot is cleared however the test case
 * ends. */
struct StagedSlot {
  explicit StagedSlot(std::shared_ptr<tadah::LammpsTadah> pot) {
    LAMMPS_NS::PairTadah::stage_extern(std::move(pot));
  }
  ~StagedSlot() { LAMMPS_NS::PairTadah::clear_staged_extern(); }
  StagedSlot(const StagedSlot&) = delete;
  StagedSlot& operator=(const StagedSlot&) = delete;
};

Context parse(const std::string& path) {
  return tadah::core::CoreUserConfigParser().parse(path);
}

LMPPtr make_instance(const std::string& log = "none") {
  LMPPtr lmp = LammpsRunner().create_instance(
      tadah::hpo::hpo_lammps_comm(), {"lmp", "-log", log, "-screen", "none"});
  REQUIRE(lmp != nullptr);
  return lmp;
}

/* Executes one LAMMPS command: "" when it ran, else its exception's
 * message. */
std::string exec(const LMPPtr& lmp, const std::string& line) {
  try {
    lmp->input->one(line);
  } catch (const std::exception& e) {
    return e.what();
  }
  return "";
}

/* Executes @p line, which must run. */
void ok(const LMPPtr& lmp, const std::string& line) {
  INFO("LAMMPS: '" << line << "'");
  REQUIRE(exec(lmp, line) == "");
}

/* The cell of 54 atoms, @p ntypes LAMMPS types, `pair_style tadah`. */
void make_cell(const LMPPtr& lmp, int ntypes) {
  for (const std::string& line :
       {std::string("units metal"), std::string("atom_style atomic"),
        std::string("atom_modify map array sort 0 0.0"),
        std::string("boundary p p p"), std::string("lattice bcc 3.30"),
        std::string("region box block 0 3 0 3 0 3"),
        "create_box " + std::to_string(ntypes) + " box",
        std::string("create_atoms 1 box"),
        std::string("displace_atoms all random 0.05 0.05 0.05 4711 units box"),
        std::string("mass * 180.94788")})
    ok(lmp, line);
  if (ntypes == 2) {
    ok(lmp, "group even id 2:54:2");
    ok(lmp, "set group even type 2");
    ok(lmp, "mass 2 92.90637");
  }
  ok(lmp, "neighbor 1.0 bin");
  ok(lmp, "variable e equal pe");
  ok(lmp, "variable fn equal fnorm");
  ok(lmp, "pair_style tadah");
}

/* Whether @p t is written as a finite number: digits, sign, point and
 * exponent only, so "nan" and "inf" are not (the build uses -ffast-math, so
 * a parsed NaN cannot be tested reliably). */
bool finite_number_text(const std::string& t) {
  return !t.empty() &&
         t.find_first_not_of("0123456789+-.eE") == std::string::npos &&
         t.find_first_of("0123456789") != std::string::npos;
}

/* The energy and the force norm of one `run 0`. */
struct Energy {
  double pe = 0.0;
  double fnorm = 0.0;
};

std::string num(double v) {
  std::ostringstream o;
  o << std::setprecision(17) << v;
  return o.str();
}

std::ostream& operator<<(std::ostream& os, const Energy& e) {
  return os << "pe " << num(e.pe) << " eV, fnorm " << num(e.fnorm) << " eV/A";
}

double read_number(const LMPPtr& lmp, const std::string& name) {
  const std::string t = LammpsRunner().get_variable(lmp, name);
  INFO("variable " << name << " = '" << t << "'");
  REQUIRE(finite_number_text(t));
  return std::stod(t);
}

/* `run 0 post no`, which must run. */
Energy run0(const LMPPtr& lmp) {
  ok(lmp, "run 0 post no");
  return {read_number(lmp, "e"), read_number(lmp, "fn")};
}

/* The energy of a fresh instance given only `pair_coeff * * <path>
 * <species>`. */
Energy fresh_file(const std::string& path, const std::string& species,
                  int ntypes) {
  LMPPtr lmp = make_instance();
  make_cell(lmp, ntypes);
  ok(lmp, "pair_coeff * * " + path + " " + species);
  return run0(lmp);
}

/* The energy of a fresh instance given only the potential of @p path,
 * staged, at `pair_coeff * * EXTERN <species>`. */
Energy fresh_extern(const std::string& path, const std::string& species) {
  StagedSlot slot(std::make_shared<tadah::LammpsTadah>(parse(path)));
  LMPPtr lmp = make_instance();
  make_cell(lmp, 1);
  ok(lmp, "pair_coeff * * EXTERN " + species);
  return run0(lmp);
}

/* @p got is the energy of @p want: both values to 1e-12 of their size. */
bool same(const Energy& got, const Energy& want) {
  return std::fabs(got.pe - want.pe) <= 1e-12 * std::fabs(want.pe) &&
         std::fabs(got.fnorm - want.fnorm) <= 1e-12 * std::fabs(want.fnorm);
}

/* The two potentials give clearly different energies, so a case that gets
 * the wrong one fails. */
void require_distinct(const Energy& a, const Energy& b) {
  INFO("first: " << a << "; second: " << b);
  REQUIRE(std::fabs(a.pe - b.pe) > 1e-3 * std::fabs(a.pe));
}

LAMMPS_NS::PairTadah* pair_of(const LMPPtr& lmp) {
  auto* pt = dynamic_cast<LAMMPS_NS::PairTadah*>(lmp->force->pair);
  REQUIRE(pt != nullptr);
  return pt;
}

std::string read_text(const std::string& path) {
  std::ifstream in(path);
  std::stringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

}  // namespace
#endif

/* Pins: in one LAMMPS instance, pair_coeff * * A Ta, run 0, then
 * pair_coeff * * B Ta, run 0 gives B's energy and force norm (those of a
 * fresh instance given only B), to 1e-12; pair_coeff * * A Ta again gives
 * A's. A and B differ by more than 1e-3 of A's energy.
 * Before the fix: the second pair_coeff did not read B (coeff() loaded a
 * file only when the pair style held no potential): the second run gave A's
 * energy and force norm, -437.004 eV against B's -223.940 eV. */
TEST_CASE("pair_style tadah: a second pair_coeff loads its potential file",
          "[pair_coeff][lammps]") {
#ifndef TADAH_ENABLE_NFP
  TADAH_SKIP("pair_style tadah is not built (TADAH_ENABLE_NFP OFF)");
#else
  tadah::core::PeriodicTable::initialize();
  const PotFile b("lj", kLjTa);
  const Energy ea = fresh_file(kQuickstartPot, "Ta", 1);
  const Energy eb = fresh_file(b.path, "Ta", 1);
  require_distinct(ea, eb);

  LMPPtr lmp = make_instance();
  make_cell(lmp, 1);
  ok(lmp, "pair_coeff * * " + kQuickstartPot + " Ta");
  const Energy first = run0(lmp);
  INFO("A: " << ea << "; B: " << eb << "; first run: " << first);
  CHECK(same(first, ea));
  ok(lmp, "pair_coeff * * " + b.path + " Ta");
  const Energy second = run0(lmp);
  INFO("second run (B): " << second);
  CHECK(same(second, eb));
  ok(lmp, "pair_coeff * * " + kQuickstartPot + " Ta");
  const Energy third = run0(lmp);
  INFO("third run (A again): " << third);
  CHECK(same(third, ea));
#endif
}

/* Pins: pair_style tadah issued again (LAMMPS keeps the pair style, and its
 * potential, when the style is the same) and pair_coeff * * B Ta after a
 * run with A give B's energy, to 1e-12.
 * Before the fix: A's energy (-437.004 eV against B's -223.940 eV). */
TEST_CASE("pair_style tadah: pair_style tadah and pair_coeff again load the "
          "new potential file",
          "[pair_coeff][lammps]") {
#ifndef TADAH_ENABLE_NFP
  TADAH_SKIP("pair_style tadah is not built (TADAH_ENABLE_NFP OFF)");
#else
  tadah::core::PeriodicTable::initialize();
  const PotFile b("lj", kLjTa);
  const Energy ea = fresh_file(kQuickstartPot, "Ta", 1);
  const Energy eb = fresh_file(b.path, "Ta", 1);
  require_distinct(ea, eb);

  LMPPtr lmp = make_instance();
  make_cell(lmp, 1);
  ok(lmp, "pair_coeff * * " + kQuickstartPot + " Ta");
  CHECK(same(run0(lmp), ea));
  ok(lmp, "pair_style tadah");
  ok(lmp, "pair_coeff * * " + b.path + " Ta");
  const Energy e = run0(lmp);
  INFO("A: " << ea << "; B: " << eb << "; after pair_style tadah and "
       "pair_coeff B: " << e);
  CHECK(same(e, eb));
#endif
}

/* Pins: with two LAMMPS types, pair_coeff * * A Ta Ta (one species), run 0,
 * then pair_coeff * * C Ta Nb (two species), run 0 gives C's energy (that
 * of a fresh instance given only C Ta Nb), to 1e-12, and the potential the
 * pair style holds maps type 1 to Ta (Z 73) and type 2 to Nb (Z 41).
 * Before the fix: the second pair_coeff was a LAMMPS error, "pair_coeff:
 * failed to initialise Tadah potential '<C>': Symbol not in potential file:
 * Nb": C was not read, and Nb was looked up in A, which has Ta only. */
TEST_CASE("pair_style tadah: a two-species potential after a one-species one",
          "[pair_coeff][lammps]") {
#ifndef TADAH_ENABLE_NFP
  TADAH_SKIP("pair_style tadah is not built (TADAH_ENABLE_NFP OFF)");
#else
  tadah::core::PeriodicTable::initialize();
  const PotFile c("tanb", kTaNb);
  const Energy ea = fresh_file(kQuickstartPot, "Ta Ta", 2);
  const Energy ec = fresh_file(c.path, "Ta Nb", 2);
  require_distinct(ea, ec);

  LMPPtr lmp = make_instance();
  make_cell(lmp, 2);
  ok(lmp, "pair_coeff * * " + kQuickstartPot + " Ta Ta");
  CHECK(same(run0(lmp), ea));
  ok(lmp, "pair_coeff * * " + c.path + " Ta Nb");
  const Energy e = run0(lmp);
  INFO("A Ta Ta: " << ea << "; C Ta Nb: " << ec << "; after A, C: " << e);
  CHECK(same(e, ec));
  const tadah::LammpsTadah* lt = pair_of(lmp)->lammps_tadah();
  REQUIRE(lt != nullptr);
  CHECK(lt->Z == std::vector<int>{73, 41});
  CHECK(lt->c.size("ATOM") == 2);
#endif
}

/* Pins: a pair_coeff * * EXTERN after a pair_coeff with a file adopts the
 * potential staged for it (B after file A gives B's energy, to 1e-12: a pin,
 * it passed before the fix as well); with nothing staged and no load() of
 * the driver, pair_coeff * * EXTERN after file A is a LAMMPS error that
 * names A's file:
 *   "pair_coeff * * EXTERN: no staged or pre-loaded Tadah potential
 *    (pair_style tadah holds the potential of '<A>', loaded by an earlier
 *    pair_coeff; EXTERN adopts a potential the driver staged or gave this
 *    pair style with load())",
 * after which the pair style has no potential: run 0 is a LAMMPS error
 * ("pair_style tadah has no potential: ...").
 * Before the fix: the second leg's EXTERN ran without an error and run 0
 * gave A's energy (coeff() kept any potential it held). */
TEST_CASE("pair_style tadah: pair_coeff * * EXTERN after a pair_coeff with a "
          "file",
          "[pair_coeff][lammps][extern]") {
#ifndef TADAH_ENABLE_NFP
  TADAH_SKIP("pair_style tadah is not built (TADAH_ENABLE_NFP OFF)");
#else
  tadah::core::PeriodicTable::initialize();
  LAMMPS_NS::PairTadah::clear_staged_extern();
  const PotFile b("lj", kLjTa);
  const Energy ea = fresh_file(kQuickstartPot, "Ta", 1);
  const Energy eb = fresh_extern(b.path, "Ta");
  require_distinct(ea, eb);

  SECTION("the staged potential") {
    LMPPtr lmp = make_instance();
    make_cell(lmp, 1);
    ok(lmp, "pair_coeff * * " + kQuickstartPot + " Ta");
    CHECK(same(run0(lmp), ea));
    StagedSlot slot(std::make_shared<tadah::LammpsTadah>(parse(b.path)));
    ok(lmp, "pair_coeff * * EXTERN Ta");
    const Energy e = run0(lmp);
    INFO("A: " << ea << "; B staged: " << eb << "; EXTERN after A: " << e);
    CHECK(same(e, eb));
  }
  SECTION("nothing staged: an error naming the file") {
    LMPPtr lmp = make_instance();
    make_cell(lmp, 1);
    ok(lmp, "pair_coeff * * " + kQuickstartPot + " Ta");
    CHECK(same(run0(lmp), ea));
    const std::string what = exec(lmp, "pair_coeff * * EXTERN Ta");
    INFO("pair_coeff * * EXTERN Ta: '" << what << "'");
    CHECK(what.find("pair_coeff * * EXTERN: no staged or pre-loaded Tadah "
                    "potential (pair_style tadah holds the potential of '" +
                    kQuickstartPot + "', loaded by an earlier pair_coeff; "
                    "EXTERN adopts a potential the driver staged or gave "
                    "this pair style with load())") != std::string::npos);
    const std::string run = exec(lmp, "run 0 post no");
    INFO("run 0: '" << run << "'");
    CHECK(run.find(kNoPotential) != std::string::npos);
  }
#endif
}

/* Pins: a pair_coeff with a file after a pair_coeff * * EXTERN that adopted
 * a staged potential loads the file: staged B, EXTERN, run 0 gives B's
 * energy, then pair_coeff * * A Ta, run 0 gives A's (to 1e-12). The pair
 * style then holds another object than the staged one, which the slot keeps
 * alive (use_count 2: the slot and the test; 1 once the slot is cleared).
 * A pair_coeff * * EXTERN after the slot is cleared is an error naming the
 * cleared slot.
 * Before the fix: the file line kept the staged potential (B's energy for
 * the second run, the pair style holding the staged object, use_count 3),
 * and the last EXTERN kept it too, without an error. */
TEST_CASE("pair_style tadah: a pair_coeff with a file after pair_coeff * * "
          "EXTERN",
          "[pair_coeff][lammps][extern]") {
#ifndef TADAH_ENABLE_NFP
  TADAH_SKIP("pair_style tadah is not built (TADAH_ENABLE_NFP OFF)");
#else
  tadah::core::PeriodicTable::initialize();
  LAMMPS_NS::PairTadah::clear_staged_extern();
  const PotFile b("lj", kLjTa);
  const Energy ea = fresh_file(kQuickstartPot, "Ta", 1);
  const Energy eb = fresh_extern(b.path, "Ta");
  require_distinct(ea, eb);

  auto staged = std::make_shared<tadah::LammpsTadah>(parse(b.path));
  LMPPtr lmp = make_instance();
  make_cell(lmp, 1);
  {
    StagedSlot slot(staged);
    ok(lmp, "pair_coeff * * EXTERN Ta");
    CHECK(same(run0(lmp), eb));
    CHECK(pair_of(lmp)->lammps_tadah() == staged.get());
    ok(lmp, "pair_coeff * * " + kQuickstartPot + " Ta");
    const Energy e = run0(lmp);
    INFO("B staged: " << eb << "; A: " << ea << "; file A after EXTERN: "
         << e);
    CHECK(same(e, ea));
    CHECK(pair_of(lmp)->lammps_tadah() != staged.get());
    CHECK(staged.use_count() == 2);
  }
  CHECK(staged.use_count() == 1);
  CHECK(staged->cutoff_max == 6.5);  // alive, and the potential it was

  /* The staged potential an EXTERN adopted, once the slot is cleared. */
  {
    StagedSlot slot(staged);
    ok(lmp, "pair_coeff * * EXTERN Ta");
  }
  const std::string what = exec(lmp, "pair_coeff * * EXTERN Ta");
  INFO("pair_coeff * * EXTERN Ta after the slot was cleared: '" << what
       << "'");
  CHECK(what.find("pair_coeff * * EXTERN: no staged or pre-loaded Tadah "
                  "potential (pair_style tadah holds the potential an "
                  "earlier pair_coeff * * EXTERN adopted from the staged "
                  "slot, which the driver has cleared since") !=
        std::string::npos);
#endif
}

/* Pins: a potential a driver gives the pair style with PairTadah::load
 * (the legacy per-line pattern) is adopted by pair_coeff * * EXTERN, and by
 * a second EXTERN without another load() (B's energy both times, to 1e-12:
 * a pin, it passed before the fix as well); a pair_coeff with a file after
 * it loads the file (A's energy).
 * Before the fix: the file line kept the driver's potential (B's energy). */
TEST_CASE("pair_style tadah: a driver's load() is adopted by EXTERN only",
          "[pair_coeff][lammps][extern]") {
#ifndef TADAH_ENABLE_NFP
  TADAH_SKIP("pair_style tadah is not built (TADAH_ENABLE_NFP OFF)");
#else
  tadah::core::PeriodicTable::initialize();
  LAMMPS_NS::PairTadah::clear_staged_extern();
  const PotFile b("lj", kLjTa);
  const Energy ea = fresh_file(kQuickstartPot, "Ta", 1);
  const Energy eb = fresh_extern(b.path, "Ta");
  require_distinct(ea, eb);

  SECTION("load(), then EXTERN twice") {
    LMPPtr lmp = make_instance();
    make_cell(lmp, 1);
    REQUIRE(pair_of(lmp)->load(parse(b.path)));
    ok(lmp, "pair_coeff * * EXTERN Ta");
    CHECK(same(run0(lmp), eb));
    ok(lmp, "pair_coeff * * EXTERN Ta");
    CHECK(same(run0(lmp), eb));
  }
  SECTION("load(), then a file") {
    LMPPtr lmp = make_instance();
    make_cell(lmp, 1);
    REQUIRE(pair_of(lmp)->load(parse(b.path)));
    ok(lmp, "pair_coeff * * " + kQuickstartPot + " Ta");
    const Energy e = run0(lmp);
    INFO("B loaded by the driver: " << eb << "; A: " << ea
         << "; file A after load(B): " << e);
    CHECK(same(e, ea));
  }
#endif
}

/* Pins: two potential objects staged in turn for the same pair style (no
 * clear between them, as in a cached LAMMPS instance whose script does not
 * start with clear): each pair_coeff * * EXTERN Ta, run 0 gives the energy
 * of a fresh instance given that potential (to 1e-12), for A (linear: the
 * generic many-body kernel, compute_2b_mb_half) and for E (non-linear: the
 * full-list kernel, compute_2b_mb_full); the files E, A, E in turn in one
 * pair style give E's, A's and E's energies (to 1e-12; E and A differ by
 * 0.125 eV). Each kernel sizes the descriptor arrays of the potential it
 * evaluates.
 * Before the fix: both kernels grew them only when atom->nmax exceeded a
 * count of the pair style, so the second object's arrays stayed empty and
 * its run 0 wrote through them: SIGSEGV in compute_2b_mb_half (A) and in
 * compute_2b_mb_full (E). With that rule put back in compute_2b_mb_full
 * alone, every other case of this file passes, and the second E object, and
 * the second E file, die with SIGSEGV. (1.6.0 did not read the second
 * file: the files E, A, E gave E's energy three times.) */
TEST_CASE("pair_style tadah: a second potential object in the same pair "
          "style gets its own descriptor arrays",
          "[pair_coeff][lammps][extern]") {
#ifndef TADAH_ENABLE_NFP
  TADAH_SKIP("pair_style tadah is not built (TADAH_ENABLE_NFP OFF)");
#else
  tadah::core::PeriodicTable::initialize();
  LAMMPS_NS::PairTadah::clear_staged_extern();
  const PotFile e_file("poly2", kPoly2Ta);

  SECTION("two staged objects of A: the generic many-body kernel") {
    const Energy ea = fresh_extern(kQuickstartPot, "Ta");
    LMPPtr lmp = make_instance();
    make_cell(lmp, 1);
    for (int k = 0; k < 2; ++k) {
      StagedSlot slot(
          std::make_shared<tadah::LammpsTadah>(parse(kQuickstartPot)));
      ok(lmp, "pair_coeff * * EXTERN Ta");
      const Energy e = run0(lmp);
      INFO("object " << k << ": " << e << "; A: " << ea);
      CHECK(same(e, ea));
    }
  }
  SECTION("two staged objects of E: the full-list kernel") {
    const Energy ee = fresh_extern(e_file.path, "Ta");
    LMPPtr lmp = make_instance();
    make_cell(lmp, 1);
    for (int k = 0; k < 2; ++k) {
      StagedSlot slot(std::make_shared<tadah::LammpsTadah>(parse(e_file.path)));
      ok(lmp, "pair_coeff * * EXTERN Ta");
      REQUIRE_FALSE(pair_of(lmp)->lammps_tadah()->linear);
      const Energy e = run0(lmp);
      INFO("object " << k << ": " << e << "; E: " << ee);
      CHECK(same(e, ee));
    }
  }
  SECTION("the files E, A, E: both kernels") {
    const Energy ee = fresh_file(e_file.path, "Ta", 1);
    const Energy ea = fresh_file(kQuickstartPot, "Ta", 1);
    INFO("E: " << ee << "; A: " << ea);
    REQUIRE_FALSE(same(ee, ea));
    LMPPtr lmp = make_instance();
    make_cell(lmp, 1);
    ok(lmp, "pair_coeff * * " + e_file.path + " Ta");
    const Energy first = run0(lmp);
    INFO("E: " << first);
    CHECK(same(first, ee));
    ok(lmp, "pair_coeff * * " + kQuickstartPot + " Ta");
    const Energy second = run0(lmp);
    INFO("then A: " << second);
    CHECK(same(second, ea));
    ok(lmp, "pair_coeff * * " + e_file.path + " Ta");
    const Energy third = run0(lmp);
    INFO("then E: " << third);
    CHECK(same(third, ee));
  }
#endif
}

/* Pins: one staged object of D (ATOM Ta Nb, D2_LJ for Ta-Ta pairs only),
 * adopted by pair_coeff * * EXTERN Ta, run 0, then by pair_coeff * * EXTERN
 * Nb, run 0 (one LAMMPS type; the pair fast path tables the element of the
 * type): the first run gives the energy of D at EXTERN Ta (to 1e-12), the
 * second that of a fresh instance given D at EXTERN Nb (0: D has no Nb-Nb
 * term; energy and force norm within 1e-12 of the Ta run's), and the
 * object's tables are those of Nb (Z 41).
 * Before the fix: setup_eam_fastpath reused the object's tables, built for
 * Ta, so the Nb run gave the Ta energy and force norm (-223.940 eV against
 * 0) and the tables kept Z 73. */
TEST_CASE("pair_style tadah: the collapse tables of a potential object "
          "follow the element of the LAMMPS type",
          "[pair_coeff][lammps][extern][eamfast]") {
#ifndef TADAH_ENABLE_NFP
  TADAH_SKIP("pair_style tadah is not built (TADAH_ENABLE_NFP OFF)");
#else
  tadah::core::PeriodicTable::initialize();
  LAMMPS_NS::PairTadah::clear_staged_extern();
  const PotFile d("ljtanb", kLjTaNb);
  const Energy e_ta = fresh_extern(d.path, "Ta");
  const Energy e_nb = fresh_extern(d.path, "Nb");
  INFO("D at Ta: " << e_ta << "; D at Nb: " << e_nb);
  REQUIRE(std::fabs(e_ta.pe) > 1.0);
  REQUIRE(std::fabs(e_nb.pe) < 1e-12);

  StagedSlot slot(std::make_shared<tadah::LammpsTadah>(parse(d.path)));
  LMPPtr lmp = make_instance();
  make_cell(lmp, 1);
  ok(lmp, "pair_coeff * * EXTERN Ta");
  const Energy first = run0(lmp);
  REQUIRE(pair_of(lmp)->lammps_tadah()->eam_fast);
  ok(lmp, "pair_coeff * * EXTERN Nb");
  const Energy second = run0(lmp);
  INFO("EXTERN Ta: " << first << "; then EXTERN Nb: " << second);
  CHECK(same(first, e_ta));
  CHECK(std::fabs(second.pe) <= 1e-12 * std::fabs(e_ta.pe));
  CHECK(std::fabs(second.fnorm) <= 1e-12 * std::fabs(e_ta.fnorm));
  CHECK(pair_of(lmp)->lammps_tadah()->eam_tables.Z == 41);
#endif
}

/* Pins: a pair_coeff whose file does not load is a LAMMPS error also after
 * the pair style was given a potential ("pair_coeff: failed to load Tadah
 * potential from '<file>': ..."), and the pair style then has none: run 0
 * is a LAMMPS error, "pair_style tadah has no potential: the last
 * pair_coeff, or a driver's PairTadah::load(), failed (see its error)", not
 * a crash; a pair_coeff that loads a file makes it run again (B's energy).
 * Likewise a file that loads but has no element of the line
 * ("pair_coeff: failed to initialise Tadah potential '<B>': Symbol not in
 * potential file: Nb"), and a failed pair_coeff in a fresh pair style.
 * Before the fix: after A, the line with the missing file ran without an
 * error (the file was not read) and so did run 0, on A, and the pair_coeff
 * of B after them kept A; the line of B with Nb failed naming B, but on A
 * (B was not read), and run 0 then ran on A; in a fresh pair style, run 0
 * after the failed pair_coeff died with SIGSEGV in init_style() (a null
 * potential). */
TEST_CASE("pair_style tadah: a pair_coeff whose file does not load leaves "
          "no potential",
          "[pair_coeff][lammps]") {
#ifndef TADAH_ENABLE_NFP
  TADAH_SKIP("pair_style tadah is not built (TADAH_ENABLE_NFP OFF)");
#else
  tadah::core::PeriodicTable::initialize();
  const PotFile b("lj", kLjTa);
  const Energy eb = fresh_file(b.path, "Ta", 1);
  const std::string missing = "/nonexistent_dir/missing.tadah";

  SECTION("a missing file after a potential") {
    LMPPtr lmp = make_instance();
    make_cell(lmp, 1);
    ok(lmp, "pair_coeff * * " + kQuickstartPot + " Ta");
    (void)run0(lmp);
    const std::string what = exec(lmp, "pair_coeff * * " + missing + " Ta");
    INFO("pair_coeff * * " << missing << " Ta: '" << what << "'");
    CHECK(what.find("pair_coeff: failed to load Tadah potential from '" +
                    missing + "'") != std::string::npos);
    const std::string run = exec(lmp, "run 0 post no");
    INFO("run 0: '" << run << "'");
    CHECK(run.find(kNoPotential) != std::string::npos);
    ok(lmp, "pair_coeff * * " + b.path + " Ta");
    CHECK(same(run0(lmp), eb));
  }
  SECTION("a file without the element after a potential") {
    LMPPtr lmp = make_instance();
    make_cell(lmp, 1);
    ok(lmp, "pair_coeff * * " + kQuickstartPot + " Ta");
    (void)run0(lmp);
    const std::string what = exec(lmp, "pair_coeff * * " + b.path + " Nb");
    INFO("pair_coeff * * " << b.path << " Nb: '" << what << "'");
    CHECK(what.find("pair_coeff: failed to initialise Tadah potential '" +
                    b.path + "': Symbol not in potential file: Nb") !=
          std::string::npos);
    const std::string run = exec(lmp, "run 0 post no");
    INFO("run 0: '" << run << "'");
    CHECK(run.find(kNoPotential) != std::string::npos);
    ok(lmp, "pair_coeff * * " + b.path + " Ta");
    CHECK(same(run0(lmp), eb));
  }
  SECTION("a missing file in a fresh pair style") {
    LMPPtr lmp = make_instance();
    make_cell(lmp, 1);
    CHECK(exec(lmp, "pair_coeff * * " + missing + " Ta") != "");
    const std::string run = exec(lmp, "run 0 post no");
    INFO("run 0: '" << run << "'");
    CHECK(run.find(kNoPotential) != std::string::npos);
  }
#endif
}

/* Pins: a pair_coeff that pair_style tadah refuses before it reads a file
 * leaves the pair style without a potential, as one whose file does not
 * load (the case above). After pair_coeff * * A Ta and run 0 (one LAMMPS
 * type), each line below is a LAMMPS error, the pair style then holds no
 * potential (lammps_tadah() is null) and run 0 is a LAMMPS error:
 *   pair_coeff * * B Ta Ta (two elements) and pair_coeff * * B (none):
 *     "Number of element to type mappings does not match number of atom
 *      types", then run 0: "pair_style tadah has no potential: ...";
 *   pair_coeff * * (no file): "Pair style tadah requires a potential file:
 *     pair_coeff * * <file> <elements...>", then the same run 0 error;
 *   pair_coeff * * B NULL (no type mapped): "Incorrect args for pair
 *     coefficients", then run 0: "All pair coeffs are not set" (LAMMPS
 *     checks the types before the pair style);
 * and a pair_coeff * * B Ta after each runs B (B's energy, to 1e-12).
 * A driver's potential (PairTadah::load(B)) and pair_coeff * * EXTERN Ta Ta:
 * the count error, then run 0: "pair_style tadah has no potential: ...",
 * and pair_coeff * * EXTERN Ta: "pair_coeff * * EXTERN: no staged or
 * pre-loaded Tadah potential" (the driver must give it again with load()).
 * A line LAMMPS refuses before it calls the pair style, pair_coeff 1 1 B Ta
 * ("Pair_coeff must start with * * for pair style tadah"), leaves A: run 0
 * gives A's energy (a pin: it passed before the fix as well).
 * Before the fix: each refused line left the potential the pair style held
 * (the error came from coeff()'s argument check or from
 * Pair::map_element2type, before the file was read): lammps_tadah() was A,
 * and run 0 ran A without an error (-437.004 eV) after the lines with two
 * elements, no element and no file (after the NULL line LAMMPS refused
 * run 0, as it does now); after the EXTERN line the pair style
 * kept the driver's B, run 0 gave B's energy (-223.940 eV) and the next
 * pair_coeff * * EXTERN Ta adopted B again. (1.6.0 also kept A at the
 * pair_coeff * * B Ta after each line: the first case of this file.) */
TEST_CASE("pair_style tadah: a pair_coeff refused before its file is read "
          "leaves no potential",
          "[pair_coeff][lammps]") {
#ifndef TADAH_ENABLE_NFP
  TADAH_SKIP("pair_style tadah is not built (TADAH_ENABLE_NFP OFF)");
#else
  tadah::core::PeriodicTable::initialize();
  LAMMPS_NS::PairTadah::clear_staged_extern();
  const PotFile b("lj", kLjTa);
  const Energy ea = fresh_file(kQuickstartPot, "Ta", 1);
  const Energy eb = fresh_file(b.path, "Ta", 1);
  require_distinct(ea, eb);
  const std::string count_error =
      "Number of element to type mappings does not match number of atom "
      "types";

  struct Refused {
    std::string what;       // the section
    std::string line;       // after pair_coeff * * A Ta and run 0
    std::string error;      // of the line
    std::string run_error;  // of the run 0 after it
  };
  const std::vector<Refused> refused = {
      {"two elements", "pair_coeff * * " + b.path + " Ta Ta", count_error,
       kNoPotential},
      {"no element", "pair_coeff * * " + b.path, count_error, kNoPotential},
      {"no file", "pair_coeff * *",
       "Pair style tadah requires a potential file: pair_coeff * * <file> "
       "<elements...>",
       kNoPotential},
      {"no type mapped", "pair_coeff * * " + b.path + " NULL",
       "Incorrect args for pair coefficients", "All pair coeffs are not set"},
  };
  for (const Refused& r : refused) {
    SECTION(r.what) {
      LMPPtr lmp = make_instance();
      make_cell(lmp, 1);
      ok(lmp, "pair_coeff * * " + kQuickstartPot + " Ta");
      CHECK(same(run0(lmp), ea));
      const std::string what = exec(lmp, r.line);
      INFO(r.line << ": '" << what << "'");
      CHECK(what.find(r.error) != std::string::npos);
      CHECK(pair_of(lmp)->lammps_tadah() == nullptr);
      const std::string run = exec(lmp, "run 0 post no");
      INFO("run 0: '" << run << "'");
      CHECK(run.find(r.run_error) != std::string::npos);
      ok(lmp, "pair_coeff * * " + b.path + " Ta");
      CHECK(same(run0(lmp), eb));
    }
  }
  SECTION("a driver's potential and an EXTERN line with two elements") {
    LMPPtr lmp = make_instance();
    make_cell(lmp, 1);
    REQUIRE(pair_of(lmp)->load(parse(b.path)));
    ok(lmp, "pair_coeff * * EXTERN Ta");
    CHECK(same(run0(lmp), eb));
    const std::string what = exec(lmp, "pair_coeff * * EXTERN Ta Ta");
    INFO("pair_coeff * * EXTERN Ta Ta: '" << what << "'");
    CHECK(what.find(count_error) != std::string::npos);
    CHECK(pair_of(lmp)->lammps_tadah() == nullptr);
    const std::string run = exec(lmp, "run 0 post no");
    INFO("run 0: '" << run << "'");
    CHECK(run.find(kNoPotential) != std::string::npos);
    const std::string again = exec(lmp, "pair_coeff * * EXTERN Ta");
    INFO("pair_coeff * * EXTERN Ta: '" << again << "'");
    CHECK(again.find("pair_coeff * * EXTERN: no staged or pre-loaded Tadah "
                     "potential") != std::string::npos);
  }
  SECTION("a line LAMMPS refuses before the pair style (a pin)") {
    LMPPtr lmp = make_instance();
    make_cell(lmp, 1);
    ok(lmp, "pair_coeff * * " + kQuickstartPot + " Ta");
    CHECK(same(run0(lmp), ea));
    const std::string what = exec(lmp, "pair_coeff 1 1 " + b.path + " Ta");
    INFO("pair_coeff 1 1 " << b.path << " Ta: '" << what << "'");
    CHECK(what.find("Pair_coeff must start with * * for pair style tadah") !=
          std::string::npos);
    REQUIRE(pair_of(lmp)->lammps_tadah() != nullptr);
    CHECK(same(run0(lmp), ea));
  }
#endif
}

/* Pins: the lines pair_coeff writes to log.lammps are those of the potential
 * it loads: after pair_coeff * * A Ta (the generic kernel: no fast-path
 * line), pair_coeff * * B Ta logs B's "Tadah! pair-collapse fast path ON"
 * line, once in the whole log.
 * Before the fix: the log had no fast-path line (B was not loaded; the
 * second pair_coeff logged what it logs for A). */
TEST_CASE("pair_style tadah: a second pair_coeff logs the lines of the "
          "potential it loads",
          "[pair_coeff][lammps]") {
#ifndef TADAH_ENABLE_NFP
  TADAH_SKIP("pair_style tadah is not built (TADAH_ENABLE_NFP OFF)");
#else
  tadah::core::PeriodicTable::initialize();
  const PotFile b("lj", kLjTa);
  const std::string log_path =
      "pair_coeff_reload_" + std::to_string(::getpid()) + ".log";
  const RemoveOnExit log_guard{log_path};
  {
    LMPPtr lmp = make_instance(log_path);
    make_cell(lmp, 1);
    ok(lmp, "pair_coeff * * " + kQuickstartPot + " Ta");
    (void)run0(lmp);
    ok(lmp, "pair_coeff * * " + b.path + " Ta");
    (void)run0(lmp);
  }  // LAMMPS destroyed: log.lammps flushed and closed
  const std::string log = read_text(log_path);
  const std::string line = "Tadah! pair-collapse fast path ON";
  const std::size_t at = log.find(line);
  const std::size_t b_line = log.find("pair_coeff * * " + b.path + " Ta");
  INFO(log);
  REQUIRE(b_line != std::string::npos);
  CHECK(at != std::string::npos);
  CHECK(at > b_line);
  CHECK(log.find(line, at + 1) == std::string::npos);
#endif
}
