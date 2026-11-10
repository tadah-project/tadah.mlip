/* What the values of each descriptor depend on: D_Base::reads_watom()
 * (whether its values are weighted by the WATOM pair weights) and
 * D_Base::depends_on() (whether they depend on a value of a search
 * parameter, whatever the atoms of a structure). The analytical gradient of
 * tadah hpo takes a parameter's rows from the descriptors' closed forms
 * only when every descriptor that depends on the parameter has them, so a
 * descriptor that depends on a parameter and says it does not makes the
 * gradient lack its part without a word.
 *
 * Every descriptor registered for a model (the registries DC_Selector
 * builds the two-body and the many-body descriptor from) has a
 * configuration below and is checked on pairs of Ta atoms:
 *  - reads_watom() as the table gives it, and as context_reads_watom()
 *    reads it from the configuration's stored values;
 *  - a value of a parameter that changes its descriptor entries or their
 *    position derivatives is one depends_on() names;
 *  - a value of a parameter that a closed form (calc_dDdtheta,
 *    calc_dXijdri_dtheta; calc_drhoi_dtheta, calc_daed_dtheta,
 *    calc_dXijdri_dtheta) gives a derivative for is one depends_on() names.
 * Then depends_on() for WATOM with two elements, per species and in the
 * pair form, for the joined descriptors, for the component paths of a
 * DESCRIPTOR block and for the family parameters of D2_ZBL and D2_MIE.
 */

#include "catch2/catch.hpp"

#include <tadah/core/context.h>
#include <tadah/core/core_user_config_parser.h>
#include <tadah/core/periodic_table.h>
#include <tadah/core/registry.h>
#include <tadah/models/dc_selector.h>
#include <tadah/models/descriptors/dm/embed_spec.h>
#include <tadah/models/inert_keys.h>

#include <cmath>
#include <cstddef>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <typeinfo>
#include <vector>

using tadah::core::Context;
using namespace tadah::models;

namespace {

std::string num(double v) {
  std::ostringstream o;
  o.precision(17);
  o << v;
  return o.str();
}

Context parse(const std::string &text) {
  tadah::core::PeriodicTable::initialize();
  std::istringstream in("VERBOSE 0\nBIAS false\nMODEL M_BLR BF_Linear\n" +
                        text);
  return tadah::core::CoreUserConfigParser().parse(in);
}

const std::string kTa = "ATOM Ta\nWATOM 1.3\n";
const std::string kCut2 = "RCTYPE2B Cut_Cos\nRCUT2B 4.5\n";
const std::string kCutM = "RCTYPEMB Cut_Cos\nRCUTMB 4.5\n";
const std::string kGrid2 = "CGRID2B 2.8 3.6\nSGRID2B 1.0 1.1\n";
const std::string kGridM = "CGRIDMB 2.8 3.6\nSGRIDMB 1.0 1.1\n";
const std::string kEmb = "CEMBFUNC 1.0 2.0\nSEMBFUNC 1.0 1.0\n";
const std::string kSetfl = "SETFL tests_data/eam.alloy\n";
const std::string kReam =
    "TYPEMB DM_REAM 3 3 3 3 5 5 0 0 Ta Ta\nCGRIDMB 2.8 3.6 4.4\n"
    "SGRIDMB 3.0 3.0 3.0\nAMPGRIDMB 0.5 0.3 0.1\n"
    "CEMBFUNC -0.2 0.3 0.8 1.3 1.8\nSEMBFUNC 2.0 2.0 2.0 2.0 2.0\n"
    "RHOFORM SIGNED\n";

/* A DESCRIPTOR block `id` of Ta Ta pairs (a DM_Embed component), cutoff
 * Cut_Cos 4.5 A. */
std::string block(const std::string &id) {
  return "DESCRIPTOR " + id +
         " DM_Embed\nPAIRS Ta Ta\nDENSITY\nBASIS Blip5\nFORM SIGNED\n"
         "CENTRES 2.8 3.6 4.4\nETA 3 3 3\nAMPLITUDES 0.5 0.3 0.1\nCUTOFF\n"
         "TYPE Cut_Cos\nRADIUS 4.5\nENDCUTOFF\nENDDENSITY\nEMBEDDING\n"
         "BASIS Gaussian\nCENTRES 0 0.5 1\nETA 2 2 2\nREFERENCE ZERO\n"
         "ENDEMBEDDING\nENDDESCRIPTOR\n";
}

/* A registered descriptor: the configuration that builds it alone (the
 * other term off, INIT2B or INITMB false) and whether its values are
 * weighted by WATOM. */
struct Entry {
  std::string config;
  bool reads_watom;
};

/* The configurations of the two-body descriptors. */
const std::map<std::string, Entry> &two_body() {
  static const std::map<std::string, Entry> m = {
      {"D2_Blip", {"INIT2B true\nTYPE2B D2_Blip 2 2 Ta Ta\n" + kGrid2 + kCut2,
                   true}},
      {"D2_Blip5",
       {"INIT2B true\nTYPE2B D2_Blip5 2 2 Ta Ta\n" + kGrid2 + kCut2, true}},
      {"D2_BP", {"INIT2B true\nTYPE2B D2_BP 2 2 Ta Ta\n" + kGrid2 + kCut2,
                 true}},
      {"D2_Knot5",
       {"INIT2B true\nTYPE2B D2_Knot5 2 2 Ta Ta\nCGRID2B 3.4 4.4\n"
        "KDEG2B 5 5\n" + kCut2,
        true}},
      {"D2_LJ", {"INIT2B true\nTYPE2B D2_LJ Ta Ta\n" + kCut2, true}},
      {"D2_MIE", {"INIT2B true\nTYPE2B D2_MIE 12 6 Ta Ta\n" + kCut2, true}},
      {"D2_ZBL",
       {"INIT2B true\nTYPE2B D2_ZBL 0.5292 0.88534 0.23 0.23 Ta Ta\n" + kCut2,
        true}},
      {"D2_EAM",
       {"INIT2B true\nTYPE2B D2_EAM 0 Ta Ta\n" + kSetfl +
            "RCTYPE2B Cut_Cos\nRCUT2B 5.58190742024\n",
        false}},
      {"D2_Dummy", {"INIT2B false\n", false}},
      {"D2_mJoin",
       {"INIT2B true\nTYPE2B D2_mJoin\nTYPE2B D2_BP 1 1 Ta Ta\n"
        "TYPE2B D2_Blip 1 1 Ta Ta\nRCTYPE2B Cut_Cos Cut_Cos\n"
        "RCUT2B 4.5 4.5\n" + kGrid2,
        true}},
  };
  return m;
}

/* The configurations of a DM_Poly channel function `fn` (SQ, SQRT, RLR,
 * Blip) of the radial family `prefix` (DM_PS_, DM_GPS_, DM_OW_, DM_GOW_). */
std::string poly(const std::string &prefix, const std::string &fn) {
  const std::string line = "INITMB true\nTYPEMB " + prefix + fn;
  if (fn == "Blip")
    return line + " 1 1 1 Ta Ta\nCGRIDMB 0.0\nSGRIDMB 0.3\n"
                  "CEMBFUNC 0.5 1.5\nSEMBFUNC 0.5 0.8\n" + kCutM;
  std::string extra;
  if (fn == "SQRT") extra = "SEMBFUNC 1 1\n";
  if (fn == "RLR") extra = "CEMBFUNC 1 2\nSEMBFUNC 0.1 0.2\n";
  return line + " 1 2 2 Ta Ta\n" + kGridM + extra + kCutM;
}

/* The configurations of the many-body descriptors. */
const std::map<std::string, Entry> &many_body() {
  static const std::map<std::string, Entry> m = [] {
    std::map<std::string, Entry> m = {
        {"DM_Blip",
         {"INITMB true\nTYPEMB DM_Blip 0 2 2 Ta Ta\n" + kGridM + kCutM, true}},
        {"DM_EAD",
         {"INITMB true\nTYPEMB DM_EAD 1 2 2 Ta Ta\n" + kGridM + kCutM, true}},
        {"DM_mBlip",
         {"INITMB true\nTYPEMB DM_mBlip 0 1 1 2 2 Ta Ta\nCGRIDMB 0.0\n"
          "SGRIDMB 0.2\n" + kEmb + kCutM,
          true}},
        {"DM_mBlipEnv",
         {"INITMB true\nTYPEMB DM_mBlipEnv 0 1 1 2 2 Ta Ta\nCGRIDMB 0.0\n"
          "SGRIDMB 0.2\n" + kEmb + "RCUTENV 4.0\n" + kCutM,
          true}},
        {"DM_mRLR",
         {"INITMB true\nTYPEMB DM_mRLR 0 2 2 2 2 Ta Ta\n" + kGridM + kEmb +
              kCutM,
          true}},
        {"DM_mSQRT",
         {"INITMB true\nTYPEMB DM_mSQRT 0 2 2 2 2 Ta Ta\n" + kGridM + kEmb +
              kCutM,
          true}},
        {"DM_REAM", {"INITMB true\n" + kReam + kCutM, true}},
        {"DM_EAM",
         {"INITMB true\nTYPEMB DM_EAM 0 Ta Ta\n" + kSetfl +
              "RCTYPEMB Cut_Cos\nRCUTMB 5.58190742024\n",
          false}},
        {"DM_Dummy", {"INITMB false\n", false}},
        {"DM_Embed", {"INITMB true\n" + block("ta"), true}},
        {"DM_mJoin",
         {"INITMB true\nTYPEMB DM_mJoin\nTYPEMB DM_mBlip 0 1 1 2 2 Ta Ta\n"
          "TYPEMB DM_REAM 3 3 3 3 5 5 0 0 Ta Ta\n"
          "RCTYPEMB Cut_Cos Cut_Cos\nRCUTMB 4.5 4.5\n"
          "CGRIDMB 0.0 2.8 3.6 4.4\nSGRIDMB 0.2 3.0 3.0 3.0\n"
          "AMPGRIDMB 0.5 0.3 0.1\nCEMBFUNC 1.0 2.0 -0.2 0.3 0.8 1.3 1.8\n"
          "SEMBFUNC 1.0 1.0 2.0 2.0 2.0 2.0 2.0\nRHOFORM SIGNED\n",
          true}},
    };
    for (const char *prefix : {"DM_PS_", "DM_GPS_", "DM_OW_", "DM_GOW_"})
      for (const char *fn : {"SQ", "SQRT", "RLR", "Blip"})
        m[std::string(prefix) + fn] = {poly(prefix, fn), true};
    return m;
  }();
  return m;
}

/* The configuration of the registered two-body descriptor `name` (with
 * INITMB false) or many-body descriptor (with INIT2B false). */
Context configuration(const std::string &name, bool two) {
  const Entry &e = two ? two_body().at(name) : many_body().at(name);
  return parse(kTa + (two ? "INITMB false\n" : "INIT2B false\n") + e.config);
}

/* ---- the values of a descriptor ------------------------------------- */

/* The distances of the Ta Ta pairs of a two-body descriptor. */
const std::vector<double> &distances() {
  static const std::vector<double> v = {2.3, 2.9, 3.5, 4.1};
  return v;
}

/* The neighbours of a Ta atom (all Ta) of a many-body descriptor. */
const std::vector<tadah::core::Vec3d> &neighbours() {
  static const std::vector<tadah::core::Vec3d> v = {
      tadah::core::Vec3d(1.3, 2.2, 0.4), tadah::core::Vec3d(-2.4, 1.0, 1.9),
      tadah::core::Vec3d(0.5, -1.6, 3.4), tadah::core::Vec3d(-2.9, -2.1, -1.0)};
  return v;
}

const int kZ = 73;  // Ta

/* The descriptor entries and their position derivatives of the descriptor
 * of `s` (its two-body descriptor when `two`, else its many-body one) on
 * the Ta pairs above. */
std::vector<double> values(const DC_Selector &s, bool two) {
  std::vector<double> out;
  const std::size_t n = s.descriptor_size();
  if (two) {
    const D2_Base &d = *s.d2b;
    for (double r : distances()) {
      tadah::core::aed_type aed(n);
      aed.set_zero();
      d.calc_aed(kZ, kZ, r, r * r, aed);
      tadah::core::fd_type fd(n);
      fd.set_zero();
      d.calc_dXijdri(kZ, kZ, r, r * r, fd);
      for (std::size_t k = 0; k < n; ++k) out.push_back(aed(k));
      for (std::size_t k = 0; k < n; ++k) out.push_back(fd(k, 0));
    }
    return out;
  }
  DM_Base &d = *s.dmb;
  tadah::core::rho_type rho;
  d.init_rhoi(rho);
  rho.set_zero();
  for (const auto &v : neighbours()) {
    const double rsq = v * v;
    d.calc_rho(kZ, kZ, std::sqrt(rsq), rsq, v, rho);
  }
  tadah::core::aed_type aed(n);
  aed.set_zero();
  d.calc_aed(rho, aed);
  for (std::size_t k = 0; k < n; ++k) out.push_back(aed(k));
  for (const auto &v : neighbours()) {
    const double rsq = v * v;
    tadah::core::fd_type fd(n);
    fd.set_zero();
    d.calc_dXijdri(kZ, kZ, std::sqrt(rsq), rsq, v, rho, fd);
    for (std::size_t k = 0; k < n; ++k)
      for (std::size_t x = 0; x < 3; ++x) out.push_back(fd(k, x));
  }
  return out;
}

/* Whether a closed form of the descriptor of `s` gives a derivative other
 * than 0 with respect to value `pos` of `key` on the Ta pairs above (the
 * walker's sequence for a many-body descriptor). */
bool closed_form_derivative(const DC_Selector &s, bool two,
                            const std::string &key, int pos) {
  const std::size_t n = s.descriptor_size();
  bool nonzero = false;
  auto any = [&](double v) {
    if (v != 0.0) nonzero = true;
  };
  if (two) {
    const D2_Base &d = *s.d2b;
    for (double r : distances()) {
      tadah::core::aed_type daed(n);
      daed.set_zero();
      d.calc_dDdtheta(key, pos, kZ, kZ, r, r * r, daed, 1.0);
      tadah::core::fd_type dfd(n);
      dfd.set_zero();
      d.calc_dXijdri_dtheta(key, pos, kZ, kZ, r, r * r, dfd, 1.0);
      for (std::size_t k = 0; k < n; ++k) {
        any(daed(k));
        any(dfd(k, 0));
      }
    }
    return nonzero;
  }
  DM_Base &d = *s.dmb;
  tadah::core::rho_type rho, drho;
  d.init_rhoi(rho);
  d.init_rhoi(drho);
  rho.set_zero();
  drho.set_zero();
  for (const auto &v : neighbours()) {
    const double rsq = v * v, r = std::sqrt(rsq);
    d.calc_rho(kZ, kZ, r, rsq, v, rho);
    d.calc_drhoi_dtheta(key, pos, kZ, kZ, r, rsq, v, drho, 1.0);
  }
  tadah::core::aed_type scratch(n), daed(n);
  scratch.set_zero();
  daed.set_zero();
  d.calc_aed(rho, scratch);
  d.calc_daed_dtheta(key, pos, rho, drho, daed, 1.0);
  for (std::size_t k = 0; k < n; ++k) any(daed(k));
  for (std::size_t k = 0; k < d.rhoi_size(); ++k) any(drho(k));
  for (const auto &v : neighbours()) {
    const double rsq = v * v;
    tadah::core::fd_type dfd(n);
    dfd.set_zero();
    d.calc_dXijdri_dtheta(key, pos, kZ, kZ, std::sqrt(rsq), rsq, v, rho,
                          drho, dfd, 1.0);
    for (std::size_t k = 0; k < n; ++k)
      for (std::size_t x = 0; x < 3; ++x) any(dfd(k, x));
  }
  return nonzero;
}

/* ---- the parameters -------------------------------------------------- */

const std::vector<std::string> &keys() {
  static const std::vector<std::string> v = {
      "WATOM",  "CGRID2B",   "SGRID2B",  "RCUT2B",   "CGRIDMB", "SGRIDMB",
      "RCUTMB", "AMPGRIDMB", "CEMBFUNC", "SEMBFUNC", "RCUTENV"};
  return v;
}

/* The values (key, 1-based position) of the parameters `c` sets: the
 * first two values of each key of keys(), each value of a component path
 * of a DESCRIPTOR block. */
std::vector<std::pair<std::string, int>> set_values(const Context &c) {
  std::vector<std::pair<std::string, int>> out;
  for (const std::string &k : keys()) {
    if (!c.has_stored(k)) continue;
    for (int p = 1; p <= std::min<int>(2, static_cast<int>(c.size(k))); ++p)
      out.push_back({k, p});
  }
  for (const auto &b : embed_bindings(c))
    for (std::size_t p = 1; p <= b.values.size(); ++p)
      out.push_back({b.path, static_cast<int>(p)});
  return out;
}

/* `c` with value `pos` of `key` moved by 1e-3 of its size (at least
 * 1e-3). */
Context moved(Context c, const std::string &key, int pos) {
  const std::size_t i = static_cast<std::size_t>(pos - 1);
  if (key.find('.') != std::string::npos) {
    const auto b = embed_binding(c, key);
    const double v = b.values.at(i);
    b.write(c, i, v + 1e-3 * std::max(1.0, std::abs(v)));
    return c;
  }
  std::vector<std::string> tokens(c.size(key));
  c.get(key, tokens);
  const double v = c.get<double>(key, i);
  tokens.at(i) = num(v + 1e-3 * std::max(1.0, std::abs(v)));
  c.remove(key);
  for (const auto &t : tokens) c.add(key, t);
  return c;
}

/* The descriptor of `s` (its two-body descriptor when `two`). */
const D_Base &descriptor(const DC_Selector &s, bool two) {
  if (two) return *s.d2b;
  return *s.dmb;
}

/* The type of the descriptor registered as `name` (the DM_Poly family
 * shares one label, DM_Poly). */
const std::type_info &registered_type(const std::string &name) {
  const std::unique_ptr<D_Base> d(tadah::core::factory<D_Base>(name));
  return typeid(*d);
}

/* The checks of the registered descriptor `name`. */
void check_descriptor(const std::string &name, bool two) {
  INFO("descriptor " << name);
  const Context c = configuration(name, two);
  const DC_Selector s(c);
  const D_Base &d = descriptor(s, two);
  REQUIRE(typeid(d) == registered_type(name));
  const Entry &e = two ? two_body().at(name) : many_body().at(name);
  CHECK(d.reads_watom() == e.reads_watom);
  CHECK(context_reads_watom(c) == d.reads_watom());

  /* A value that changes the descriptor's values is one it depends on.
   * The cutoff a setfl file fixes is not moved (the descriptor refuses
   * another value). */
  const std::vector<double> base = values(s, two);
  const std::string cutoff = two ? "RCUT2B" : "RCUTMB";
  for (const auto &[key, pos] : set_values(c)) {
    if (key == cutoff && !d.cutoff_fixed_by(0).empty()) continue;
    INFO(key << " (" << pos << ")");
    const Context m = moved(c, key, pos);
    const DC_Selector sm(m);
    if (values(sm, two) != base) CHECK(d.depends_on(key, pos));
  }

  /* A value a closed form differentiates is one it depends on. */
  std::vector<std::pair<std::string, int>> probed = set_values(c);
  for (const std::string &k : keys())
    for (int p = 1; p <= 3; ++p) probed.push_back({k, p});
  for (int p = 1; p <= 6; ++p) probed.push_back({"TYPE2B", p});
  for (const auto &[key, pos] : probed) {
    INFO("closed form for " << key << " (" << pos << ")");
    if (closed_form_derivative(s, two, key, pos))
      CHECK(d.depends_on(key, pos));
  }
  /* A key of the fit is none of a descriptor's. */
  CHECK_FALSE(d.depends_on("LAMBDA", 1));
}

}  // namespace

/* Pins: for each two-body descriptor registered for a model (the registry
 * DC_Selector builds it from), check_descriptor: reads_watom() is false for
 * D2_EAM (setfl tables, unweighted) and D2_Dummy and true for every other,
 * context_reads_watom() of its configuration agrees, a value of a
 * parameter that changes its descriptor entries or their position
 * derivatives on Ta Ta pairs is one depends_on() names, and so is a value
 * a closed form (calc_dDdtheta, calc_dXijdri_dtheta) differentiates; a
 * descriptor the table does not have fails (a descriptor is classified
 * when it is added).
 * Before the fix: does not compile (D_Base has no reads_watom() and no
 * depends_on()). */
TEST_CASE("depends_on: every registered two-body descriptor names what its "
          "values depend on",
          "[descriptor][depends_on][watom]") {
  for (const auto &kv :
       tadah::core::Registry<D2_Base, const Context &>::view()) {
    INFO("registered two-body descriptor " << kv.first);
    if (!two_body().count(kv.first))
      FAIL("no configuration for " << kv.first << " in this test");
    check_descriptor(kv.first, true);
  }
}

/* Pins: for each many-body descriptor registered for a model (DM_Embed
 * built from a DESCRIPTOR block), check_descriptor as for the two-body
 * descriptors: reads_watom() false for DM_EAM and DM_Dummy and true for
 * every other (DM_REAM, DM_Embed, DM_mJoin and the DM_Poly family
 * included), context_reads_watom() agreeing, the values of each parameter
 * that change the density, the descriptor entries or their position
 * derivatives of a Ta atom with four Ta neighbours, and those a closed form
 * (calc_drhoi_dtheta, calc_daed_dtheta, calc_dXijdri_dtheta) differentiates,
 * named by depends_on().
 * Before the fix: does not compile (D_Base has no reads_watom() and no
 * depends_on()). */
TEST_CASE("depends_on: every registered many-body descriptor names what its "
          "values depend on",
          "[descriptor][depends_on][watom]") {
  for (const auto &kv :
       tadah::core::Registry<DM_Base, const Context &>::view()) {
    INFO("registered many-body descriptor " << kv.first);
    if (!many_body().count(kv.first))
      FAIL("no configuration for " << kv.first << " in this test");
    check_descriptor(kv.first, false);
  }
}

/* Pins: depends_on("WATOM", n) of a descriptor with two elements (ATOM Ta
 * W), per species (WATOM 1.0 2.0) and in the pair form (WATOM Ta Ta 1.0
 * Ta W 0.8 W W 2.0): true for value n when a pair of the descriptor's
 * elements has a weight that value n sets, else false:
 *   - D2_Blip and DM_Blip of Ta Ta pairs: per species WATOM (1) true,
 *     WATOM (2) false; pair form (1) true, (2) and (3) false;
 *   - the same of Ta W pairs: per species (1) and (2) true; pair form (2)
 *     true, (1) and (3) false;
 *   - the same of * * pairs: every value true;
 *   - D2_LJ of W W pairs: per species (1) false, (2) true;
 *   - a value past the WATOM list, (3) per species: false.
 * Before the fix: does not compile (D_Base has no depends_on()). */
TEST_CASE("depends_on: WATOM per species and in the pair form names the "
          "values that weigh a pair of the descriptor's elements",
          "[descriptor][depends_on][watom]") {
  const std::string species = "ATOM Ta W\nWATOM 1.0 2.0\n";
  const std::string pairs = "ATOM Ta W\nWATOM Ta Ta 1.0 Ta W 0.8 W W 2.0\n";
  auto two_body_of = [&](const std::string &head, const std::string &els) {
    return DC_Selector(parse(head + "INITMB false\nINIT2B true\n"
                             "TYPE2B D2_Blip 2 2 " + els + "\n" + kGrid2 +
                             kCut2));
  };
  auto many_body_of = [&](const std::string &head, const std::string &els) {
    return DC_Selector(parse(head + "INIT2B false\nINITMB true\n"
                             "TYPEMB DM_Blip 0 2 2 " + els + "\n" + kGridM +
                             kCutM));
  };
  struct Case {
    std::string els;
    std::vector<bool> per_species, pair_form;
  };
  const std::vector<Case> cases = {
      {"Ta Ta", {true, false, false}, {true, false, false}},
      {"Ta W", {true, true, false}, {false, true, false}},
      {"* *", {true, true, false}, {true, true, true}},
  };
  for (const Case &c : cases) {
    INFO("pairs " << c.els);
    const DC_Selector s2 = two_body_of(species, c.els);
    const DC_Selector sm = many_body_of(species, c.els);
    const DC_Selector p2 = two_body_of(pairs, c.els);
    const DC_Selector pm = many_body_of(pairs, c.els);
    for (int n = 1; n <= 3; ++n) {
      INFO("WATOM (" << n << ")");
      CHECK(s2.d2b->depends_on("WATOM", n) == c.per_species[n - 1]);
      CHECK(sm.dmb->depends_on("WATOM", n) == c.per_species[n - 1]);
      CHECK(p2.d2b->depends_on("WATOM", n) == c.pair_form[n - 1]);
      CHECK(pm.dmb->depends_on("WATOM", n) == c.pair_form[n - 1]);
    }
  }
  const DC_Selector lj(parse(species + "INITMB false\nINIT2B true\n"
                                       "TYPE2B D2_LJ W W\n" + kCut2));
  CHECK_FALSE(lj.d2b->depends_on("WATOM", 1));
  CHECK(lj.d2b->depends_on("WATOM", 2));
}

/* Pins: depends_on() of the joined descriptors and of DESCRIPTOR blocks:
 *   - D2_mJoin of D2_BP (Ta Ta) and D2_LJ (W W), ATOM Ta W, WATOM per
 *     species: WATOM (1) and (2) true (a joined descriptor depends on
 *     each), CGRID2B, SGRID2B and RCUT2B true (its children's and its own
 *     keys), CGRIDMB false; reads_watom() true;
 *   - DM_mJoin of DM_EAM and DM_Blip: reads_watom() true (DM_Blip reads
 *     it), WATOM (1) true; DM_mJoin of two DM_EAM: reads_watom() false,
 *     WATOM (1) false;
 *   - a DESCRIPTOR block ta (DM_Embed): its paths TA.DENSITY.CENTRES (1),
 *     TA.EMBEDDING.ETA (2) and TA.DENSITY.CUTOFF.RADIUS (1) true, the path
 *     of another block TB.DENSITY.CENTRES (1) false, WATOM (1) true, the
 *     legacy keys CGRIDMB and RCUTMB false;
 *   - DM_mJoin of the blocks ta and tb: the paths of each block true, of a
 *     block it does not have false.
 * Before the fix: does not compile (D_Base has no depends_on()). */
TEST_CASE("depends_on: a joined descriptor depends on what any descriptor "
          "it joins depends on, and a DESCRIPTOR block on its own paths",
          "[descriptor][depends_on][watom][mjoin][embed]") {
  const DC_Selector j2(parse(
      "ATOM Ta W\nWATOM 1.0 2.0\nINITMB false\nINIT2B true\n"
      "TYPE2B D2_mJoin\nTYPE2B D2_BP 1 1 Ta Ta\nTYPE2B D2_LJ W W\n"
      "RCTYPE2B Cut_Cos Cut_Cos\nRCUT2B 4.5 4.5\nCGRID2B 2.8\n"
      "SGRID2B 1.0\n"));
  REQUIRE(j2.d2b->label() == "D2_mJoin");
  CHECK(j2.d2b->reads_watom());
  CHECK(j2.d2b->depends_on("WATOM", 1));
  CHECK(j2.d2b->depends_on("WATOM", 2));
  for (const char *k : {"CGRID2B", "SGRID2B", "RCUT2B"})
    CHECK(j2.d2b->depends_on(k, 1));
  CHECK_FALSE(j2.d2b->depends_on("CGRIDMB", 1));

  const std::string eam = "TYPEMB DM_EAM 0 Ta Ta\n";
  const DC_Selector jm(parse(
      kTa + "INIT2B false\nINITMB true\nTYPEMB DM_mJoin\n" + eam +
      "TYPEMB DM_Blip 0 2 2 Ta Ta\n" + kSetfl +
      "RCTYPEMB Cut_Cos Cut_Cos\nRCUTMB 5.58190742024 4.5\n" + kGridM));
  REQUIRE(jm.dmb->label() == "DM_mJoin");
  CHECK(jm.dmb->reads_watom());
  CHECK(jm.dmb->depends_on("WATOM", 1));
  const DC_Selector je(parse(
      kTa + "INIT2B false\nINITMB true\nTYPEMB DM_mJoin\n" + eam + eam +
      kSetfl + "RCTYPEMB Cut_Cos Cut_Cos\n"
               "RCUTMB 5.58190742024 5.58190742024\n"));
  REQUIRE(je.dmb->label() == "DM_mJoin");
  CHECK_FALSE(je.dmb->reads_watom());
  CHECK_FALSE(je.dmb->depends_on("WATOM", 1));

  const DC_Selector b(parse(kTa + "INIT2B false\nINITMB true\n" +
                            block("ta")));
  REQUIRE(b.dmb->label() == "DM_Embed");
  CHECK(b.dmb->depends_on("TA.DENSITY.CENTRES", 1));
  CHECK(b.dmb->depends_on("TA.EMBEDDING.ETA", 2));
  CHECK(b.dmb->depends_on("TA.DENSITY.CUTOFF.RADIUS", 1));
  CHECK_FALSE(b.dmb->depends_on("TB.DENSITY.CENTRES", 1));
  CHECK(b.dmb->depends_on("WATOM", 1));
  CHECK_FALSE(b.dmb->depends_on("CGRIDMB", 1));
  CHECK_FALSE(b.dmb->depends_on("RCUTMB", 1));

  const DC_Selector bb(parse(kTa + "INIT2B false\nINITMB true\n" +
                             block("ta") + block("tb")));
  REQUIRE(bb.dmb->label() == "DM_mJoin");
  CHECK(bb.dmb->depends_on("TA.DENSITY.CENTRES", 1));
  CHECK(bb.dmb->depends_on("TB.DENSITY.AMPLITUDES", 3));
  CHECK_FALSE(bb.dmb->depends_on("TC.DENSITY.CENTRES", 1));
}

/* Pins: the family parameters of D2_ZBL (a0, s0, p0, p1) and D2_MIE (n,
 * m), at the TYPE2B positions their closed forms take them (2 to 5 and 2
 * to 3: the label is position 1), are values they depend on, the label
 * and the element tokens are not; RCUTENV is a value DM_mBlipEnv depends
 * on and DM_mBlip does not; D2_Dummy and DM_Dummy depend on nothing.
 * Before the fix: does not compile (D_Base has no depends_on()). */
TEST_CASE("depends_on: the family parameters of D2_ZBL and D2_MIE, "
          "RCUTENV, and the dummies",
          "[descriptor][depends_on]") {
  const DC_Selector zbl(configuration("D2_ZBL", true));
  const DC_Selector mie(configuration("D2_MIE", true));
  for (int p = 1; p <= 7; ++p) {
    INFO("TYPE2B (" << p << ")");
    CHECK(zbl.d2b->depends_on("TYPE2B", p) == (p >= 2 && p <= 5));
    CHECK(mie.d2b->depends_on("TYPE2B", p) == (p >= 2 && p <= 3));
  }
  const DC_Selector env(configuration("DM_mBlipEnv", false));
  const DC_Selector blip(configuration("DM_mBlip", false));
  CHECK(env.dmb->depends_on("RCUTENV", 1));
  CHECK_FALSE(blip.dmb->depends_on("RCUTENV", 1));
  const DC_Selector none(parse(kTa + "INIT2B false\nINITMB false\n"));
  for (const std::string &k : keys()) {
    INFO(k);
    CHECK_FALSE(none.d2b->depends_on(k, 1));
    CHECK_FALSE(none.dmb->depends_on(k, 1));
  }
}
