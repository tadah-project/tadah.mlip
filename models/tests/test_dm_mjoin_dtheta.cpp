// Analytical theta-derivatives routed through DM_mJoin.
//
// The HPO walker calls the string overloads of calc_drhoi_dtheta,
// calc_daed_dtheta and calc_dXijdri_dtheta on the many-body descriptor of
// the DC_Selector. For a joined descriptor that is DM_mJoin, which must
// either give the derivative of the JOINED descriptor with respect to the
// named parameter or report that it does not own it (the walker then uses
// finite differences under GRAD_STRATEGY HYBRID, and ANALYTICAL names the
// parameter it cannot differentiate).
//
// OPTIM CGRIDMB (k), SGRIDMB (k) and RCUTMB (k) index the joined lists of
// the parent context; each child reads its own list. A named component path
// (<ID>.DENSITY.CENTRES, ...) names one DM_Embed child by its ID.

#include "catch2/catch.hpp"
#include <tadah/core/core_user_config_parser.h>
#include <tadah/core/periodic_table.h>
#include <tadah/models/dc_selector.h>
#include <tadah/models/descriptors/dm/embed_spec.h>

#include <cmath>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

using namespace tadah::core;
using namespace tadah::models;

namespace {

std::string list(const std::vector<double> &v) {
  std::ostringstream o;
  o.precision(17);
  for (std::size_t i = 0; i < v.size(); ++i) o << (i ? " " : "") << v[i];
  return o.str();
}

// A legacy DM_mJoin of two DM_Blip children (3 and 2 functions, L = 0 and
// L = 1), each with its own cutoff value.
struct LegacyJoin {
  std::vector<double> cgrid{1.0, 2.0, 3.0, 1.6, 2.2};
  std::vector<double> sgrid{1.0, 1.2, 0.9, 1.1, 0.8};
  std::vector<double> rcut{5.0, 4.5};
  Context context() const {
    PeriodicTable::initialize();
    std::istringstream in(
        "VERBOSE 0\nATOM Ta\nWATOM 1\nBIAS false\nINIT2B false\n"
        "INITMB true\nMODEL M_BLR BF_Linear\nTYPEMB DM_mJoin\n"
        "TYPEMB DM_Blip 0 3 3 Ta Ta\nTYPEMB DM_Blip 1 2 2 Ta Ta\n"
        "RCTYPEMB Cut_Cos Cut_Cos\nRCUTMB " + list(rcut) +
        "\nCGRIDMB " + list(cgrid) + "\nSGRIDMB " + list(sgrid) + "\n");
    return CoreUserConfigParser().parse(in);
  }
};

// A DM_Embed component joined with a legacy DM_Blip term.
Context mixed_join() {
  PeriodicTable::initialize();
  std::istringstream in(
      "VERBOSE 0\nINIT2B false\nINITMB true\nATOM Ta\nWATOM 1\nBIAS false\n"
      "MODEL M_BLR BF_Linear\n"
      "RCTYPEMB Cut_Cos\nRCUTMB 5\nCGRIDMB 1.6 2.2\nSGRIDMB 1.1 0.8\n"
      "DESCRIPTOR ta DM_Embed\nPAIRS Ta Ta\nDENSITY\nBASIS Blip5\n"
      "FORM SIGNED\nCENTRES 1 2\nETA 2 2\nAMPLITUDES 0.4 0.2\nCUTOFF\n"
      "TYPE Cut_Poly\nRADIUS 6\nENDCUTOFF\nENDDENSITY\nEMBEDDING\n"
      "BASIS Gaussian\nCENTRES 0 0.5 1\nETA 2 2 2\nREFERENCE ZERO\n"
      "ENVELOPE\nTYPE OFF\nENDENVELOPE\nENDEMBEDDING\nENDDESCRIPTOR\n");
  Context c = CoreUserConfigParser().parse(in);
  // The explicit order of the joined terms: the component, then the legacy
  // term (as test_dm_embed.cpp composes them).
  c.add("TYPEMB", "@ta DM_Blip 0 2 2 Ta Ta");
  return c;
}

// One atom with three neighbours inside both cutoffs.
const std::vector<Vec3d> &neighbours() {
  static const std::vector<Vec3d> v{
      Vec3d(1.3, 0.2, 0.0), Vec3d(-0.4, 2.0, 0.3), Vec3d(0.5, -0.6, 2.6)};
  return v;
}

struct Values {
  std::vector<double> rho, aed, force;
};

// rho_i, aed_i and the fd row of the first pair, as the design-matrix
// builder computes them.
Values evaluate(const Context &c) {
  DC_Selector s(c);
  DM_Base &d = *s.dmb;
  rho_type rho;
  d.init_rhoi(rho);
  rho.set_zero();
  for (const auto &v : neighbours()) {
    const double rsq = v * v;
    d.calc_rho(73, 73, std::sqrt(rsq), rsq, v, rho);
  }
  aed_type aed(d.size());
  aed.set_zero();
  d.calc_aed(rho, aed);
  fd_type fd(d.size());
  fd.set_zero();
  const Vec3d &v = neighbours()[0];
  const double rsq = v * v;
  d.calc_dXijdri(73, 73, std::sqrt(rsq), rsq, v, rho, fd);
  Values out;
  for (std::size_t i = 0; i < d.rhoi_size(); ++i) out.rho.push_back(rho(i));
  for (std::size_t i = 0; i < d.size(); ++i) out.aed.push_back(aed(i));
  for (std::size_t i = 0; i < d.size(); ++i)
    for (std::size_t k = 0; k < 3; ++k) out.force.push_back(fd(i, k));
  return out;
}

struct Analytic {
  bool rho_owned = false, aed_owned = false, force_owned = false;
  Values values;
};

// The walker's sequence for one parameter: drho over the neighbours,
// F'(rho) through calc_aed, daed, and the force row of the first pair.
Analytic analytic(const Context &c, const std::string &name, int pos) {
  DC_Selector s(c);
  DM_Base &d = *s.dmb;
  rho_type rho, drho;
  d.init_rhoi(rho);
  d.init_rhoi(drho);
  rho.set_zero();
  drho.set_zero();
  Analytic out;
  for (const auto &v : neighbours()) {
    const double rsq = v * v, r = std::sqrt(rsq);
    d.calc_rho(73, 73, r, rsq, v, rho);
    out.rho_owned =
        d.calc_drhoi_dtheta(name, pos, 73, 73, r, rsq, v, drho) ||
        out.rho_owned;
  }
  aed_type scratch(d.size()), daed(d.size());
  scratch.set_zero();
  daed.set_zero();
  d.calc_aed(rho, scratch);
  out.aed_owned = d.calc_daed_dtheta(name, pos, rho, drho, daed);
  fd_type dfd(d.size());
  dfd.set_zero();
  const Vec3d &v = neighbours()[0];
  const double rsq = v * v;
  out.force_owned = d.calc_dXijdri_dtheta(name, pos, 73, 73, std::sqrt(rsq),
                                          rsq, v, rho, drho, dfd);
  for (std::size_t i = 0; i < d.rhoi_size(); ++i)
    out.values.rho.push_back(drho(i));
  for (std::size_t i = 0; i < d.size(); ++i)
    out.values.aed.push_back(daed(i));
  for (std::size_t i = 0; i < d.size(); ++i)
    for (std::size_t k = 0; k < 3; ++k) out.values.force.push_back(dfd(i, k));
  return out;
}

// Every value an owned derivative writes agrees with the central
// difference of the joined descriptor.
void require_matches(const std::vector<double> &analytic,
                     const std::vector<double> &plus,
                     const std::vector<double> &minus, double h,
                     const char *what) {
  REQUIRE(analytic.size() == plus.size());
  for (std::size_t i = 0; i < analytic.size(); ++i) {
    const double fd = (plus[i] - minus[i]) / (2 * h);
    CAPTURE(what, i, analytic[i], fd);
    REQUIRE(analytic[i] == Approx(fd).margin(2e-6).epsilon(1e-5));
  }
}

}  // namespace

/* Pins: through a legacy DM_mJoin, CGRIDMB (k), SGRIDMB (k) and RCUTMB (k)
 * of the joined lists are not owned by the analytical derivative (no
 * function claims them, so HYBRID takes finite differences and ANALYTICAL
 * names them), and whatever is owned agrees with the central difference of
 * the joined descriptor. Before the fix DM_mJoin passed the joined index to
 * every child, which read it against its own grid: CGRIDMB (1) was owned
 * and also wrote rho[3] (the second child's first function; the finite
 * difference there is 0), and CGRIDMB (4) was not owned at all. */
TEST_CASE("DM_mJoin analytical derivatives of joined legacy grids are left "
          "to finite differences",
          "[unified-dm][derivatives][mjoin]") {
  const LegacyJoin base;
  struct Key {
    std::string name;
    std::size_t count;
    std::function<std::vector<double> &(LegacyJoin &)> list;
  };
  const std::vector<Key> keys{
      {"CGRIDMB", base.cgrid.size(),
       [](LegacyJoin &j) -> std::vector<double> & { return j.cgrid; }},
      {"SGRIDMB", base.sgrid.size(),
       [](LegacyJoin &j) -> std::vector<double> & { return j.sgrid; }},
      {"RCUTMB", base.rcut.size(),
       [](LegacyJoin &j) -> std::vector<double> & { return j.rcut; }}};
  const double h = 1e-6;
  for (const auto &key : keys) {
    for (std::size_t k = 1; k <= key.count; ++k) {
      CAPTURE(key.name, k);
      const auto a = analytic(base.context(), key.name, static_cast<int>(k));
      LegacyJoin plus = base, minus = base;
      key.list(plus)[k - 1] += h;
      key.list(minus)[k - 1] -= h;
      const Values p = evaluate(plus.context()), m = evaluate(minus.context());
      if (a.rho_owned) require_matches(a.values.rho, p.rho, m.rho, h, "rho");
      if (a.aed_owned) require_matches(a.values.aed, p.aed, m.aed, h, "aed");
      if (a.force_owned)
        require_matches(a.values.force, p.force, m.force, h, "force");
      REQUIRE_FALSE(a.rho_owned);
      REQUIRE_FALSE(a.aed_owned);
      REQUIRE_FALSE(a.force_owned);
    }
  }
}

/* Pins: a named component path reaches the DM_Embed child of a mixed
 * DM_mJoin through all three derivative functions, each derivative agrees
 * with the central difference of the joined descriptor, and the legacy
 * child's CGRIDMB is left to finite differences. */
TEST_CASE("DM_mJoin routes named component derivatives to its DM_Embed child",
          "[unified-dm][derivatives][mjoin]") {
  const Context c = mixed_join();
  {
    DC_Selector s(c);
    REQUIRE(s.dmb->label() == "DM_mJoin");
  }
  const double h = 1e-6;
  const auto bindings = embed_bindings(c);
  REQUIRE_FALSE(bindings.empty());
  for (const auto &binding : bindings) {
    for (std::size_t index = 0; index < binding.values.size(); ++index) {
      CAPTURE(binding.path, index);
      const auto a = analytic(c, binding.path, static_cast<int>(index + 1));
      REQUIRE(a.aed_owned);
      REQUIRE(a.force_owned);
      Context plus = c, minus = c;
      binding.write(plus, index, binding.values[index] + h);
      binding.write(minus, index, binding.values[index] - h);
      const Values p = evaluate(plus), m = evaluate(minus);
      require_matches(a.values.aed, p.aed, m.aed, h, "aed");
      require_matches(a.values.force, p.force, m.force, h, "force");
      if (binding.path.find(".DENSITY.") != std::string::npos) {
        REQUIRE(a.rho_owned);
        require_matches(a.values.rho, p.rho, m.rho, h, "rho");
      }
    }
  }
  for (int k = 1; k <= 2; ++k) {
    const auto a = analytic(c, "CGRIDMB", k);
    REQUIRE_FALSE(a.rho_owned);
    REQUIRE_FALSE(a.aed_owned);
    REQUIRE_FALSE(a.force_owned);
  }
}
