// Catch2 v2 tests of the arg_pos convention of the descriptors' parameter
// metadata (ParamMeta, DescriptorMeta of D_Base::meta()).
//
// ParamMeta::arg_pos is -1 for a key of one value, 0 for every value of a
// list key, and n >= 1 for the value at 1-based position n of the key in
// the descriptor's own context (its own TYPE2B line for a value of that
// line, where position 1 is the descriptor's name): the arg_pos the
// descriptor's calc_dDdtheta takes for that value.

#include "catch2/catch.hpp"

#include <tadah/core/context.h>
#include <tadah/core/core_types.h>
#include <tadah/core/periodic_table.h>
#include <tadah/core/registry.h>
#include <tadah/models/descriptors/d2/d2_base.h>
#include <tadah/models/descriptors/d_base.h>
#include <tadah/models/descriptors/param_meta.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using tadah::core::Context;
using tadah::models::D2_Base;
using tadah::models::D_Base;

namespace {

/* A TYPE2B line of each descriptor with values of its TYPE2B line among its
 * parameters (ParamMeta arg_pos >= 1), every value distinct, for a pair of
 * Si atoms (Z = 14: ln Z is not 0, and the ZBL screening length puts r =
 * 1.2 to 1.5 A where the screening function varies smoothly). */
const std::map<std::string, std::string> &probe_lines() {
  static const std::map<std::string, std::string> lines = {
      {"D2_ZBL", "D2_ZBL 0.52917721067 0.88534 0.23 0.27 Si Si"},
      {"D2_MIE", "D2_MIE 12 6 Si Si"},
  };
  return lines;
}

/* The descriptor @p label built from its TYPE2B line @p tokens, with
 * RCUT2B 3 and Cut_Cos. */
std::unique_ptr<D2_Base> build(const std::string &label,
                               const std::vector<std::string> &tokens) {
  Context c;
  c.add("ATOM", "Si");
  c.add("WATOM", 1);
  c.add("INIT2B", "true");
  c.add("VERBOSE", 0);
  c.add("RCUT2B", 3.0);
  c.add("RCTYPE2B", "Cut_Cos");
  for (const auto &t : tokens) c.add("TYPE2B", t);
  return std::unique_ptr<D2_Base>(
      tadah::core::factory<D2_Base, Context &>(label, c));
}

/* The descriptor of a Si pair at distance @p r. */
tadah::core::aed_type aed_at(const D2_Base &d, double r) {
  tadah::core::aed_type aed(d.size());
  d.calc_aed(14, 14, r, r * r, aed);
  return aed;
}

}  // namespace

/* Pins: in every registered descriptor's metadata, ParamMeta::arg_pos is
 * -1, 0, or a position n >= 1 of a value of the descriptor's TYPE2B line;
 * for each such n, the value at index n - 1 of a probe line (probe_lines())
 * is a number, and the descriptor's calc_dDdtheta("TYPE2B", n) at a Si
 * pair (r = 1.2 and 1.5 A) equals the central finite difference of the
 * descriptor in that value (relative step 1e-6, tolerance 1e-5 relative),
 * which moves some component of the descriptor:
 * the arg_pos addresses the value its role names, in the convention of
 * calc_dDdtheta. A descriptor with such a parameter and no probe line
 * fails (one is to be added).
 * Before the fix: passes as well (a pin: D2_ZBL and D2_MIE give the
 * values of their TYPE2B line by that position, while the documentation of
 * ParamMeta::arg_pos called it the 1-based OPTIM index, one less for a
 * TYPE2B value). */
TEST_CASE("ParamMeta arg_pos addresses the value it names in the convention "
          "of calc_dDdtheta",
          "[metadata][arg_pos]") {
  tadah::core::PeriodicTable::initialize();
  std::size_t positional = 0;
  for (const auto &label : tadah::core::Registry<D_Base>::names()) {
    std::unique_ptr<D_Base> d(tadah::core::factory<D_Base>(label));
    const auto meta = d->meta();
    if (!meta.has_value()) continue;
    for (const auto &pm : meta->params) {
      CAPTURE(label, pm.key, pm.arg_pos);
      CHECK(pm.arg_pos >= -1);
      if (pm.arg_pos < 1) continue;
      ++positional;
      CHECK(pm.key == "TYPE2B");
      const auto probe = probe_lines().find(label);
      REQUIRE(probe != probe_lines().end());
      std::vector<std::string> tokens;
      {
        std::istringstream in(probe->second);
        for (std::string t; in >> t;) tokens.push_back(t);
      }
      REQUIRE(tokens.at(0) == label);
      const std::size_t at = static_cast<std::size_t>(pm.arg_pos - 1);
      REQUIRE(at < tokens.size());
      double value = 0.0;
      {
        std::istringstream in(tokens[at]);
        REQUIRE(static_cast<bool>(in >> value));
      }
      const double h = 1e-6 * std::fabs(value);
      std::vector<std::string> up = tokens, down = tokens;
      {
        std::ostringstream o1, o2;
        o1.precision(17);
        o2.precision(17);
        o1 << value + h;
        o2 << value - h;
        up[at] = o1.str();
        down[at] = o2.str();
      }
      const auto d0 = build(label, tokens);
      const auto dp = build(label, up);
      const auto dm = build(label, down);
      for (const double r : {1.2, 1.5}) {
        CAPTURE(r);
        const auto ap = aed_at(*dp, r);
        const auto am = aed_at(*dm, r);
        tadah::core::aed_type grad(d0->size());
        for (std::size_t k = 0; k < d0->size(); ++k) grad(k) = 0.0;
        REQUIRE(d0->calc_dDdtheta("TYPE2B", pm.arg_pos, 14, 14, r, r * r, grad,
                                  1.0));
        double largest = 0.0;
        for (std::size_t k = 0; k < d0->size(); ++k) {
          const double fd = (ap(k) - am(k)) / (2.0 * h);
          CAPTURE(k, fd, grad(k));
          CHECK(grad(k) == Approx(fd).epsilon(1e-5).margin(1e-12));
          largest = std::max(largest, std::fabs(fd));
        }
        CHECK(largest > 0.0);
      }
    }
  }
  CHECK(positional == 6u);
}
