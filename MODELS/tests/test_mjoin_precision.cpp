/* Regression: the mJoin meta-descriptors must not round the grids
 * they split into per-child sub-contexts.
 *
 * D_mJoin::parse_context() used to re-render every CGRID/SGRID/CEMBFUNC/
 * SEMBFUNC/AMPGRIDMB/KDEG* value with a fixed NUMERIC (default 12) number of
 * DECIMALS, on every construction. Two grid values closer
 * than 1e-12 collapsed to the same number, values below 1e-12 became 0, and
 * a small HPO step on a grid was invisible to the descriptor. The call also
 * inserted a NUMERIC key into the caller's context (and from there into every
 * HPO potential file).
 */

#include "catch2/catch.hpp"
#include <tadah/core/core_types.h>
#include <tadah/core/utils.h>
#include <tadah/models/descriptors/d_all.h>
#include <tadah/models/descriptors/d_mjoin.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

using namespace tadah::core;
using namespace tadah::models;

namespace {

struct MjoinPrecisionFixture {
  MjoinPrecisionFixture() { PeriodicTable::initialize(); }
};

std::uint64_t mj_bits(double v) {
  std::uint64_t u;
  std::memcpy(&u, &v, sizeof u);
  return u;
}

// 3e-13 apart: identical after the old 12-decimal rendering.
constexpr double kG1 = 0.962983118529000759;
constexpr double kG2 = 0.962983118528700777;

// D2_mJoin with two D2_Blip children (2 + 1 grid points).
Context make_d2_ctx(const std::vector<double> &cgrid) {
  Context ctx;
  ctx.add("RCTYPE2B", "Cut_Cos");
  ctx.add("RCTYPE2B", "Cut_Cos");
  ctx.add("RCUT2B", 5.0);
  ctx.add("RCUT2B", 5.0);
  ctx.remove("INIT2B");
  ctx.add("INIT2B", "true");
  ctx.add("TYPE2B", "D2_mJoin");
  ctx.add("TYPE2B", "D2_Blip 2 2 H H");
  ctx.add("TYPE2B", "D2_Blip 1 1 H H");
  for (double g : cgrid) ctx.add("CGRID2B", g);
  for (double s : {0.5, 0.7, 0.9}) ctx.add("SGRID2B", s);
  return ctx;
}

// DM_mJoin with two DM_Blip children (Lmax 0; 2 + 1 grid points).
Context make_dm_ctx(const std::vector<double> &cgrid) {
  Context ctx;
  ctx.add("RCTYPEMB", "Cut_Cos");
  ctx.add("RCTYPEMB", "Cut_Cos");
  ctx.add("RCUTMB", 5.0);
  ctx.add("RCUTMB", 5.0);
  ctx.remove("INITMB");
  ctx.add("INITMB", "true");
  ctx.add("TYPEMB", "DM_mJoin");
  ctx.add("TYPEMB", "DM_Blip 0 2 2 H H");
  ctx.add("TYPEMB", "DM_Blip 0 1 1 H H");
  for (double g : cgrid) ctx.add("CGRIDMB", g);
  for (double s : {0.5, 0.7, 0.9}) ctx.add("SGRIDMB", s);
  return ctx;
}

std::vector<double> eval_d2(const D2_Base &d, double r) {
  aed_type aed(d.size());
  aed.set_zero();
  d.calc_aed(1, 1, r, r * r, aed);
  return std::vector<double>(aed.data(), aed.data() + d.size());
}

std::vector<double> eval_dm(DM_Base &d) {
  Vec3d del(1.0, 0.6, 0.4);
  const double rsq = del * del;
  rho_type rho;
  d.init_rhoi(rho);
  d.calc_rho(1, 1, std::sqrt(rsq), rsq, del, rho);
  aed_type aed(d.size());
  aed.set_zero();
  d.calc_aed(rho, aed);
  return std::vector<double>(aed.data(), aed.data() + d.size());
}

bool bitwise_equal(const std::vector<double> &a, const std::vector<double> &b) {
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i)
    if (mj_bits(a[i]) != mj_bits(b[i])) return false;
  return true;
}

} // namespace

TEST_CASE_METHOD(MjoinPrecisionFixture,
                 "mJoin parse_context keeps grid values bit-exact",
                 "[mjoin][precision]") {
  const std::vector<double> cgrid = {kG1, kG2, 1e-20};
  const Context ctx = make_d2_ctx(cgrid);
  const auto subs = D_mJoin::parse_context(ctx, "TYPE2B");
  REQUIRE(subs.size() == 2);
  REQUIRE(subs[0].size("CGRID2B") == 2);
  REQUIRE(subs[1].size("CGRID2B") == 1);
  CHECK(mj_bits(subs[0].get<double>("CGRID2B", 0)) == mj_bits(kG1));
  CHECK(mj_bits(subs[0].get<double>("CGRID2B", 1)) == mj_bits(kG2));
  CHECK(subs[0].get<double>("CGRID2B", 0) != subs[0].get<double>("CGRID2B", 1));
  CHECK(mj_bits(subs[1].get<double>("CGRID2B", 0)) == mj_bits(1e-20));
  CHECK(subs[1].get<double>("CGRID2B", 0) != 0.0);

  // expand_grids writes the same exact values back into the master context.
  Context c = ctx;
  D_mJoin::expand_grids(c);
  REQUIRE(c.size("CGRID2B") == 3);
  for (std::size_t i = 0; i < cgrid.size(); ++i)
    CHECK(mj_bits(c.get<double>("CGRID2B", i)) == mj_bits(cgrid[i]));
}

TEST_CASE_METHOD(MjoinPrecisionFixture,
                 "mJoin LIN grid expansion equals core::linspace bitwise",
                 "[mjoin][precision]") {
  Context ctx;
  ctx.add("RCTYPE2B", "Cut_Cos");
  ctx.add("RCTYPE2B", "Cut_Cos");
  ctx.add("RCUT2B", 5.0);
  ctx.add("RCUT2B", 5.0);
  ctx.remove("INIT2B");
  ctx.add("INIT2B", "true");
  ctx.add("TYPE2B", "D2_mJoin");
  ctx.add("TYPE2B", "D2_Blip 4 4 H H");
  ctx.add("TYPE2B", "D2_Blip 3 3 H H");
  ctx.add("CGRID2B", "LIN 4 0.1 3.0 LIN 3 0.25 2.9");
  ctx.add("SGRID2B", "GEOM 4 0.3 2.7 LIN 3 0.5 0.9");

  std::vector<double> cexp;
  for (double v : linspace(0.1, 3.0, 4)) cexp.push_back(v);
  for (double v : linspace(0.25, 2.9, 3)) cexp.push_back(v);
  std::vector<double> sexp;
  for (double v : geomspace(0.3, 2.7, 4, std::exp(1))) sexp.push_back(v);
  for (double v : linspace(0.5, 0.9, 3)) sexp.push_back(v);

  D_mJoin::expand_grids(ctx);
  REQUIRE(ctx.size("CGRID2B") == cexp.size());
  REQUIRE(ctx.size("SGRID2B") == sexp.size());
  for (std::size_t i = 0; i < cexp.size(); ++i)
    CHECK(mj_bits(ctx.get<double>("CGRID2B", i)) == mj_bits(cexp[i]));
  for (std::size_t i = 0; i < sexp.size(); ++i)
    CHECK(mj_bits(ctx.get<double>("SGRID2B", i)) == mj_bits(sexp[i]));
}

TEST_CASE_METHOD(MjoinPrecisionFixture,
                 "mJoin splits integer data keys as integers (KDEG2B)",
                 "[mjoin][precision]") {
  Context ctx;
  ctx.add("RCTYPE2B", "Cut_Dummy");
  ctx.add("RCTYPE2B", "Cut_Dummy");
  ctx.add("RCUT2B", 5.0);
  ctx.add("RCUT2B", 5.0);
  ctx.remove("INIT2B");
  ctx.add("INIT2B", "true");
  ctx.add("TYPE2B", "D2_mJoin");
  ctx.add("TYPE2B", "D2_Knot5 2 2 H H");
  ctx.add("TYPE2B", "D2_Knot5 1 1 H H");
  for (double g : {1.5, 2.5, 3.5}) ctx.add("CGRID2B", g);
  // eam_refit adds the degrees as doubles
  for (double d : {5.0, 3.0, 5.0}) ctx.add("KDEG2B", d);

  const auto subs = D_mJoin::parse_context(ctx, "TYPE2B");
  REQUIRE(subs.size() == 2);
  CHECK(subs[0]("KDEG2B") == std::vector<std::string>{"5", "3"});
  CHECK(subs[1]("KDEG2B") == std::vector<std::string>{"5"});
  CHECK(subs[0]("CGRID2B") == std::vector<std::string>{"1.5", "2.5"});
}

TEST_CASE_METHOD(MjoinPrecisionFixture,
                 "mJoin does not insert NUMERIC into the caller's context",
                 "[mjoin][precision]") {
  Context ctx = make_d2_ctx({kG1, 1.7, 2.3});
  REQUIRE_FALSE(ctx.has_stored("NUMERIC"));
  (void)D_mJoin::parse_context(ctx, "TYPE2B");
  D_mJoin::expand_grids(ctx);
  D2_mJoin d2(ctx);
  CHECK_FALSE(ctx.has_stored("NUMERIC"));

  Context ctxm = make_dm_ctx({kG1, 1.7, 2.3});
  DM_mJoin dm(ctxm);
  CHECK_FALSE(ctxm.has_stored("NUMERIC"));
}

