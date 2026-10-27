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
//
// A crystal predicts the same per-atom energy and forces whichever
// periodic cell describes it: a cell thinner than the cutoff, a triclinic
// or sheared one, or one whose atoms lie outside it, must give what its
// supercell (every height at least the cutoff) gives.

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

#include <algorithm>
#include <array>
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

using Rows = std::array<std::array<double, 3>, 3>;  // rows[k]: lattice vector a_k
using Frac = std::array<double, 3>;
using Offset = std::array<int, 3>;

/* A Kr crystal whose cell rows are the lattice vectors a_k: atom i sits at
 * r_i = f_i0 a0 + f_i1 a1 + f_i2 a2, with its fractional coordinates f_i in
 * [0, 1), moved by the lattice vector m_i0 a0 + m_i1 a1 + m_i2 a2 when
 * offsets m are given (an unwrapped copy of the same crystal). */
Structure kr_crystal(const Rows &rows, const std::vector<Frac> &frac,
                     const std::vector<Offset> &offset = {}) {
  const Element Kr = PeriodicTable::find_by_symbol("Kr");
  Structure st;
  st.cell.load(rows[0][0], rows[0][1], rows[0][2],
               rows[1][0], rows[1][1], rows[1][2],
               rows[2][0], rows[2][1], rows[2][2]);
  for (size_t i = 0; i < frac.size(); ++i) {
    double r[3];
    for (int c = 0; c < 3; ++c) {
      r[c] = frac[i][0] * rows[0][c] + frac[i][1] * rows[1][c] +
             frac[i][2] * rows[2][c];
      if (!offset.empty())
        r[c] += offset[i][0] * rows[0][c] + offset[i][1] * rows[1][c] +
                offset[i][2] * rows[2][c];
    }
    st.add_atom(Atom(Kr, r[0], r[1], r[2], 0, 0, 0));
  }
  return st;
}

/* Heights h_k = V / |a_{k+1} x a_{k+2}| of the cell (the distances between
 * the lattice planes the rows span), computed here, not by the library. */
std::array<double, 3> cell_heights(const Structure &st) {
  double a[3][3];
  for (int k = 0; k < 3; ++k)
    for (int c = 0; c < 3; ++c) a[k][c] = st.cell(k, c);
  double cr[3][3];  // cr[k] = a_{k+1} x a_{k+2}
  for (int k = 0; k < 3; ++k) {
    const double *u = a[(k + 1) % 3], *v = a[(k + 2) % 3];
    cr[k][0] = u[1] * v[2] - u[2] * v[1];
    cr[k][1] = u[2] * v[0] - u[0] * v[2];
    cr[k][2] = u[0] * v[1] - u[1] * v[0];
  }
  const double vol = std::fabs(a[0][0] * cr[0][0] + a[0][1] * cr[0][1] +
                               a[0][2] * cr[0][2]);
  std::array<double, 3> h;
  for (int k = 0; k < 3; ++k)
    h[k] = vol / std::sqrt(cr[k][0] * cr[k][0] + cr[k][1] * cr[k][1] +
                           cr[k][2] * cr[k][2]);
  return h;
}

/* The n0 x n1 x n2 supercell along the cell rows, built here (the check
 * must not share code with StructureTransformation::replicate): row k is
 * scaled by n_k and copy (x, y, z) of atom i sits at
 * r_i + x a0 + y a1 + z a2, in the loop order x, y, z, then atoms, so atom
 * c * natoms + i is a copy of atom i. */
Structure supercell(const Structure &st, const std::array<int, 3> &n) {
  Structure out;
  out.cell.load(st.cell(0, 0) * n[0], st.cell(0, 1) * n[0], st.cell(0, 2) * n[0],
                st.cell(1, 0) * n[1], st.cell(1, 1) * n[1], st.cell(1, 2) * n[1],
                st.cell(2, 0) * n[2], st.cell(2, 1) * n[2], st.cell(2, 2) * n[2]);
  for (int x = 0; x < n[0]; ++x)
    for (int y = 0; y < n[1]; ++y)
      for (int z = 0; z < n[2]; ++z)
        for (const Atom &at : st.atoms) {
          Atom copy = at;
          for (int c = 0; c < 3; ++c)
            copy.position[c] = at.position[c] + x * st.cell(0, c) +
                               y * st.cell(1, c) + z * st.cell(2, c);
          out.add_atom(copy);
        }
  return out;
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
   * same file with `N N` pairs is refused wherever it is loaded. The
   * trainer is built from the files as they are (the retired keys of the
   * published fit are in its training record, which no loader reads): the
   * `N N` file is refused, the `* *` one is not. (Until the published file
   * kept them in its training record, its EWEIGHT, FWEIGHT and SWEIGHT
   * lines were deleted first, as a retrain converts them.) */
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
  std::vector<std::string> published;
  {
    std::ifstream in(kMlipsDir + "/tadah.n2");
    REQUIRE(in.is_open());
    for (std::string line; std::getline(in, line);) published.push_back(line);
  }

  /* tadah predict */
  CHECK_THROWS_WITH(Potential(n_n), Catch::Contains(msg));
  CHECK_NOTHROW(Potential(kMlipsDir + "/tadah.n2", Forces::no));

  /* tadah train */
  {
    tadah::mlip::memory::MLIPWorkspaceManager ws;
    Context ctx = CoreUserConfigParser().parse(lines);
    CHECK_THROWS_WITH(Trainer(ctx, ws), Catch::Contains(msg));
    Context as_published = CoreUserConfigParser().parse(published);
    CHECK_NOTHROW(Trainer(as_published, ws));
  }

#ifdef TADAH_ENABLE_NFP
  /* pair_style tadah (pair_coeff * * <file> N builds this object) */
  CHECK_THROWS_WITH(tadah::LammpsTadah(n_n), Catch::Contains(msg));
  CHECK_NOTHROW(
      tadah::LammpsTadah(load(kMlipsDir + "/tadah.n2", Forces::no)));
#endif
}

TEST_CASE("published potentials predict the same per-atom energy and forces "
          "on a thin or triclinic cell as on its supercell",
          "[published_potentials][published][nn]") {
  /* Kr fcc at a = 6.0 A, the atoms moved off their lattice sites so that
   * they feel forces. Each cell below is thinner than the cutoffs (Kr1
   * 11.23 A, Kr2 12.00 A) in at least one direction, and describes the
   * same crystal as its supercell along the rows with every height at
   * least the cutoff: the energy per atom, and the force on each atom and
   * on each of its copies, must be the same. The reference is the
   * supercell of the wrapped cell. A neighbour list that misses periodic
   * images of the thin cell changes both: before the fix, the list of the
   * conventional cell held 392 of its 536 entries at Kr2's cutoff (360 of
   * 438 at Kr1's), 138 with its atoms outside the cell, 428 in the sheared
   * basis, and that of the doubled primitive cell 248 of 268. A one-atom
   * cell only pairs the atom with its own images, and was exact. */
  PeriodicTable::initialize();
  const double a = 6.0, h = 0.5 * a;
  const Rows cubic = {{{a, 0.0, 0.0}, {0.0, a, 0.0}, {0.0, 0.0, a}}};
  const Rows primitive = {{{0.0, h, h}, {h, 0.0, h}, {h, h, 0.0}}};
  const Rows primitive_x2 = {{{0.0, a, a}, {h, 0.0, h}, {h, h, 0.0}}};
  // the cubic lattice in the basis a0, a0 + a1, a2
  const Rows sheared = {{{a, 0.0, 0.0}, {a, a, 0.0}, {0.0, 0.0, a}}};
  const std::vector<Frac> conventional = {{0.02, 0.01, 0.015},
                                          {0.0, 0.512, 0.48},
                                          {0.485, 0.018, 0.51},
                                          {0.52, 0.49, 0.007}};
  // the same atoms in the sheared basis: g0 = f0 - f1 (wrapped), g1 = f1
  std::vector<Frac> conventional_sheared;
  for (const Frac &f : conventional) {
    double g0 = f[0] - f[1];
    if (g0 < 0.0) g0 += 1.0;
    conventional_sheared.push_back({g0, f[1], f[2]});
  }
  const std::vector<Frac> doubled = {{0.01, 0.02, 0.03}, {0.52, 0.01, 0.97}};

  struct Case {
    std::string name;
    Rows rows;
    std::vector<Frac> frac;
    std::vector<Offset> offset;  // empty: wrapped
  };
  const std::vector<Case> cases = {
      {"conventional cubic cell (4 atoms)", cubic, conventional, {}},
      {"conventional cubic cell, atoms moved by lattice vectors", cubic,
       conventional, {{1, 0, -1}, {-2, 1, 0}, {0, 3, 1}, {1, 1, 1}}},
      {"conventional cell in the sheared basis a0, a0 + a1, a2", sheared,
       conventional_sheared, {}},
      {"primitive triclinic cell (1 atom)", primitive, {{0.1, 0.2, 0.3}}, {}},
      {"primitive triclinic cell, atom moved by a lattice vector", primitive,
       {{0.1, 0.2, 0.3}}, {{2, -1, 3}}},
      {"primitive triclinic cell doubled along a0 (2 atoms)", primitive_x2,
       doubled, {}},
      {"primitive triclinic cell doubled along a0, atoms moved by lattice "
       "vectors", primitive_x2, doubled, {{1, -1, 0}, {-1, 2, 1}}},
  };

  for (const std::string name : {"Tadah.Kr1", "Tadah.Kr2"}) {
    std::unique_ptr<Potential> pot;
    REQUIRE_NOTHROW(
        pot.reset(new Potential(kMlipsDir + "/" + name, Forces::yes)));
    const double rc = pot->context().get<double>("RCUTMAX");
    for (const Case &cs : cases) {
      const Structure wrapped = kr_crystal(cs.rows, cs.frac);
      const Structure st = kr_crystal(cs.rows, cs.frac, cs.offset);
      const std::array<double, 3> hk = cell_heights(wrapped);
      std::array<int, 3> n;
      for (int k = 0; k < 3; ++k)
        n[k] = static_cast<int>(std::ceil(rc / hk[k]));
      INFO(name << " (cutoff " << rc << " A): " << cs.name << "; heights "
                << hk[0] << ", " << hk[1] << ", " << hk[2] << " A; supercell "
                << n[0] << " x " << n[1] << " x " << n[2]);
      // the cell is thinner than the cutoff; the supercell is not
      REQUIRE(std::min({hk[0], hk[1], hk[2]}) < rc);
      for (int k = 0; k < 3; ++k) REQUIRE(n[k] * hk[k] >= rc);

      const Structure big = supercell(wrapped, n);
      const size_t nat = st.natoms();
      REQUIRE(big.natoms() == nat * n[0] * n[1] * n[2]);
      const Structure pred = pot->predict(st);
      const Structure ref = pot->predict(big);
      REQUIRE(pred.atoms.size() == nat);
      REQUIRE(ref.atoms.size() == big.natoms());

      const double e = pred.energy / nat;
      const double e_ref = ref.energy / big.natoms();
      INFO("E/N " << e << " eV, supercell " << e_ref << " eV");
      bool finite = is_finite_bits(e) && is_finite_bits(e_ref);
      CHECK(e == Approx(e_ref).epsilon(1e-10));

      // the largest difference between the force on an atom and on any
      // of its copies in the supercell
      double df_max = 0.0, f_ref_max = 0.0;
      size_t worst_atom = 0;
      for (size_t c = 0; c < big.natoms() / nat; ++c)
        for (size_t i = 0; i < nat; ++i)
          for (int k = 0; k < 3; ++k) {
            const double f = pred.atoms[i].force(k);
            const double f_ref = ref.atoms[c * nat + i].force(k);
            finite = finite && is_finite_bits(f) && is_finite_bits(f_ref);
            f_ref_max = std::max(f_ref_max, std::fabs(f_ref));
            if (!(std::fabs(f - f_ref) <= df_max)) {
              df_max = std::fabs(f - f_ref);
              worst_atom = i;
            }
          }
      INFO("largest force difference " << df_max << " eV/A (atom "
           << worst_atom << "); largest supercell force component "
           << f_ref_max << " eV/A");
      CHECK(finite);
      CHECK(df_max <= 1e-10);
      // off their lattice sites, the atoms of a multi-atom cell feel forces
      if (nat > 1) CHECK(f_ref_max > 1e-3);
    }
  }
}
