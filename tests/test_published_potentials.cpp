// The potentials published with the documentation (docs/source/mlips).
//
// Tadah!Kr1 and Tadah!Kr2 are two-body potentials that also ship as LAMMPS
// tables (Tadah.Kr1.tab, Tadah.Kr2.tab, written by pair_write from the same
// models): the pair energy and force predicted for a Kr dimer must
// reproduce every row of the table. Tadah!N2 is a DIMER model of two N2
// molecules; it has no reference table, so its energies for two molecule
// pairs are checked against pair_style tadah, which computes the DIMER
// descriptors with its own code.
//
// The files are loaded the way `tadah predict` loads a potential (with
// `-F` for the Kr tables).

#include "catch2/catch.hpp"

#include <tadah/core/context.h>
#include <tadah/core/core_user_config_parser.h>
#include <tadah/core/periodic_table.h>
#include <tadah/core/registry.h>
#include <tadah/mlip/atom.h>
#include <tadah/mlip/descriptors_calc.h>
#include <tadah/mlip/design_matrix/functions/dm_function_base.h>
#include <tadah/mlip/memory/mlip_workspace_manager.h>
#include <tadah/mlip/models/m_tadah_base.h>
#include <tadah/mlip/nn_finder.h>
#include <tadah/mlip/structure.h>
#include <tadah/mlip/trainer.h>
#include <tadah/models/dc_selector.h>
#ifdef TADAH_ENABLE_NFP
#include <tadah/tadah.lammps/lammps_tadah.h>
#endif

#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using namespace tadah::core;
using namespace tadah::mlip;
using tadah::models::DC_Selector;

namespace {

const std::string kMlipsDir = TADAH_PUBLISHED_MLIPS_DIR;

enum class Forces { no, yes };

/* A potential file parsed as PredictEngine parses it; with Forces::yes,
 * FORCE is set as `tadah predict -F` sets it (the descriptors calculator
 * and the model read the flag from this Context). */
Context load(const std::string &file, Forces forces) {
  Context ctx = CoreUserConfigParser().parse(file);
  if (forces == Forces::yes) {
    ctx.remove("FORCE");
    ctx.add("FORCE", "true");
  }
  return ctx;
}

/* A potential file loaded as PredictEngine loads it. */
class Potential {
public:
  Potential(const std::string &file, Forces forces)
      : Potential(load(file, forces)) {}

  /* A potential already parsed into a Context. */
  explicit Potential(Context ctx)
      : ctx_(std::move(ctx)), dcs_(ctx_),
        dc_(ctx_, *dcs_.d2b, *dcs_.dmb, *dcs_.c2b, *dcs_.cmb),
        fb_(factory<DM_Function_Base, const Context &>(
            ctx_.get<std::string>("MODEL", 1), ctx_)),
        model_(factory<M_Tadah_Base, DM_Function_Base &, Context &>(
            ctx_.get<std::string>("MODEL", 0), *fb_, ctx_)) {}

  const Context &context() const { return ctx_; }

  /* Predicted total energy of `st`, and its forces with Forces::yes. */
  Structure predict(Structure st) {
    NNFinder().calc(st, ctx_);
    return model_->predict(ctx_, st, dc_);
  }

private:
  Context ctx_;
  DC_Selector dcs_;
  DescriptorsCalc<> dc_;
  std::unique_ptr<DM_Function_Base> fb_;
  std::unique_ptr<M_Tadah_Base> model_;
};

/* A 40 A cubic cell: a periodic image of a partner is never within the
 * 12 A cutoffs of these potentials. */
Structure empty_cell() {
  Structure st;
  st.cell.load(40.0, 0.0, 0.0, 0.0, 40.0, 0.0, 0.0, 0.0, 40.0);
  return st;
}

Structure kr_dimer(double r) {
  const Element Kr = PeriodicTable::find_by_symbol("Kr");
  Structure st = empty_cell();
  st.add_atom(Atom(Kr, 10.0, 10.0, 10.0, 0, 0, 0));
  st.add_atom(Atom(Kr, 10.0 + r, 10.0, 10.0, 0, 0, 0));
  return st;
}

/* Two N2 molecules (bond length b) whose centres of mass are d apart along
 * x; the second molecule is tilted. */
Structure n2_pair(double b, double d) {
  const Element N = PeriodicTable::find_by_symbol("N");
  const double h = 0.5 * b;
  const double ux = 0.3, uy = 0.1, uz = std::sqrt(1.0 - ux * ux - uy * uy);
  Structure st = empty_cell();
  st.add_atom(Atom(N, 10.0, 10.0, 10.0 - h, 0, 0, 0));
  st.add_atom(Atom(N, 10.0, 10.0, 10.0 + h, 0, 0, 0));
  st.add_atom(Atom(N, 10.0 + d - h * ux, 10.0 - h * uy, 10.0 - h * uz, 0, 0, 0));
  st.add_atom(Atom(N, 10.0 + d + h * ux, 10.0 + h * uy, 10.0 + h * uz, 0, 0, 0));
  return st;
}

/* Finite by its exponent bits: the build uses -ffast-math, under which the
 * compiler may fold std::isfinite() to true. */
bool is_finite_bits(double x) {
  std::uint64_t bits;
  std::memcpy(&bits, &x, sizeof bits);
  return ((bits >> 52) & 0x7ffu) != 0x7ffu;
}

struct TableRow {
  double r;
  double energy;
  double force;  // -dE/dr: the force on the second atom, along r
};

/* The rows of a LAMMPS pair_write table (i, r, energy, force), checked
 * against the N given in its header. */
std::vector<TableRow> read_pair_table(const std::string &file) {
  std::ifstream in(file);
  REQUIRE(in.is_open());
  std::vector<TableRow> rows;
  size_t n_header = 0;
  for (std::string line; std::getline(in, line);) {
    std::istringstream ls(line);
    std::string first;
    if (!(ls >> first) || first[0] == '#') continue;
    if (first == "N") {
      ls >> n_header;
      continue;
    }
    size_t i;
    TableRow row;
    std::istringstream rs(line);
    if (rs >> i >> row.r >> row.energy >> row.force) rows.push_back(row);
  }
  REQUIRE(n_header > 0);
  REQUIRE(rows.size() == n_header);
  return rows;
}

/* Every row of the published table: the predicted energy of a Kr dimer at
 * separation r is the pair energy of the table, and the force on the
 * second atom (placed at +r along x) is the table force along x, equal and
 * opposite to the force on the first atom. The tables print about 15
 * significant digits; the predictions agree to a few parts in 1e12 (the
 * largest relative differences are in the last rows before the cutoff,
 * where the energy is ~1e-9 eV), and beyond the cutoff both are zero. */
void check_kr_against_table(const std::string &name) {
  PeriodicTable::initialize();
  std::unique_ptr<Potential> pot;
  REQUIRE_NOTHROW(
      pot.reset(new Potential(kMlipsDir + "/" + name, Forces::yes)));
  const auto rows = read_pair_table(kMlipsDir + "/" + name + ".tab");
  for (const TableRow &row : rows) {
    INFO(name << " at r = " << row.r << " A");
    const Structure pred = pot->predict(kr_dimer(row.r));
    REQUIRE(pred.atoms.size() == 2);
    const auto &f1 = pred.atoms[0].force;
    const auto &f2 = pred.atoms[1].force;
    CHECK(pred.energy == Approx(row.energy).epsilon(1e-10).margin(1e-15));
    CHECK(f2(0) == Approx(row.force).epsilon(1e-10).margin(1e-15));
    CHECK(f1(0) == Approx(-row.force).epsilon(1e-10).margin(1e-15));
    for (int k : {1, 2}) {
      CHECK(f1(k) == Approx(0.0).margin(1e-15));
      CHECK(f2(k) == Approx(0.0).margin(1e-15));
    }
  }
}

} // namespace

TEST_CASE("Published potentials: Tadah!Kr1 reproduces Tadah.Kr1.tab",
          "[published_potentials]") {
  check_kr_against_table("Tadah.Kr1");
}

TEST_CASE("Published potentials: Tadah!Kr2 reproduces Tadah.Kr2.tab",
          "[published_potentials]") {
  check_kr_against_table("Tadah.Kr2");
}

TEST_CASE("Published potentials: Tadah!N2 reproduces the pair_style tadah "
          "energies of two N2 molecules",
          "[published_potentials]") {
  PeriodicTable::initialize();
  std::unique_ptr<Potential> pot;
  REQUIRE_NOTHROW(
      pot.reset(new Potential(kMlipsDir + "/tadah.n2", Forces::no)));
  REQUIRE(pot->context().get<bool>("DIMER", 0));
  const double bond = pot->context().get<double>("DIMER", 1);

  const double e_near = pot->predict(n2_pair(bond, 3.5)).energy;
  const double e_far = pot->predict(n2_pair(bond, 4.5)).energy;
  INFO("E(3.5 A) = " << e_near << " eV, E(4.5 A) = " << e_far << " eV");
  CHECK(is_finite_bits(e_near));
  CHECK(is_finite_bits(e_far));
  // Reference: LAMMPS pair_style tadah with this file on the same two
  // geometries (atom_style bond, the molecules as bonded pairs, newton off,
  // `run 0`; thermo pe printed with %.15g). pair_style tadah computes the
  // DIMER descriptors with its own code, so the two agree only if the
  // file's descriptor lines, grids and weights are read as the model was
  // fitted. A two-body or many-body block that applies to no element pair
  // still gives finite, separation-dependent energies (of the wrong sign
  // and four orders of magnitude too large), so finiteness alone proves
  // little.
  CHECK(e_near == Approx(-0.00737599445968229).epsilon(1e-9));
  CHECK(e_far == Approx(-0.00502446493587006).epsilon(1e-9));
}

TEST_CASE("Published potentials: Tadah!N2 with element pairs other than "
          "* * is refused by tadah train, tadah predict and pair_style "
          "tadah", "[published_potentials][dimer]") {
  /* A DIMER potential is evaluated with every atom as Z = 1 (by
   * DescriptorsCalc::calc_dimer and by pair_style tadah's compute_dimers),
   * so its descriptors must be declared for `* *`. Declared for `N N` they
   * applied to no pair the evaluation sees, and the model predicted from
   * zero descriptors without a word. The published file (`* *`) loads; the
   * same file with `N N` pairs is refused wherever it is loaded. */
  const std::string msg =
      "DIMER evaluates every atom as Z=1; declare TYPE2B/TYPEMB with * * "
      "element pairs";
  PeriodicTable::initialize();
  std::vector<std::string> lines;
  {
    std::ifstream in(kMlipsDir + "/tadah.n2");
    REQUIRE(in.is_open());
    for (std::string line; std::getline(in, line);) {
      if (line.rfind("TYPE2B", 0) == 0) {
        REQUIRE(line.find("*     *") != std::string::npos);
        line = "TYPE2B D2_Blip 4 4 N N";
      } else if (line.rfind("TYPEMB", 0) == 0) {
        REQUIRE(line.find("*     *") != std::string::npos);
        line = "TYPEMB DM_Blip 1 4 4 N N";
      }
      lines.push_back(line);
    }
  }
  const Context n_n = CoreUserConfigParser().parse(lines);
  REQUIRE(n_n.get<bool>("DIMER", 0));

  /* tadah predict */
  CHECK_THROWS_WITH(Potential(n_n), Catch::Contains(msg));
  CHECK_NOTHROW(Potential(kMlipsDir + "/tadah.n2", Forces::no));

  /* tadah train */
  {
    tadah::mlip::memory::MLIPWorkspaceManager ws;
    Context ctx = n_n;
    CHECK_THROWS_WITH(Trainer(ctx, ws), Catch::Contains(msg));
    Context published = load(kMlipsDir + "/tadah.n2", Forces::no);
    CHECK_NOTHROW(Trainer(published, ws));
  }

#ifdef TADAH_ENABLE_NFP
  /* pair_style tadah (pair_coeff * * <file> N builds this object) */
  CHECK_THROWS_WITH(tadah::LammpsTadah(n_n), Catch::Contains(msg));
  CHECK_NOTHROW(
      tadah::LammpsTadah(load(kMlipsDir + "/tadah.n2", Forces::no)));
#endif
}
