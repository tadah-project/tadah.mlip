/* EMBF0 through DM_mJoin: the DM_REAM children of a joined many-body
 * descriptor read copies of the parent context, so the one global EMBF0 key
 * (absent = ZERO) reaches every DM_REAM block in the constructor. DM_mJoin
 * forwards embf0_mode() and sums raw_embedding_f0() over its children, each
 * indexing its own block of the full weight vector.
 */

#include "catch2/catch.hpp"
#include <tadah/core/core_types.h>
#include <tadah/core/periodic_table.h>
#include <tadah/models/descriptors/basis_dispatch.h>
#include <tadah/models/descriptors/d_all.h>
#include <tadah/models/descriptors/dm/embf0.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

using namespace tadah::core;
using namespace tadah::models;

namespace {

struct MjoinEmbf0Fixture {
  MjoinEmbf0Fixture() { PeriodicTable::initialize(); }
};

std::uint64_t mj_bits(double v) {
  std::uint64_t u;
  std::memcpy(&u, &v, sizeof u);
  return u;
}

// The two DM_REAM children: a Blip embedding (bcode 0) and a Knot5
// embedding (bcode 2) with negative knots, so g_m(0) != 0 in both.
const std::vector<double> kBumpC = {2.0, 3.5, 5.0};
const std::vector<double> kBumpS = {1.0, 1.0, 1.0};
const std::vector<double> kBumpA = {0.40, 0.25, 0.12};
const std::vector<double> kBumpEc = {-0.3, 0.0, 0.4, 0.8};
const std::vector<double> kBumpEs = {1.5, 1.5, 1.5, 1.5};
const std::vector<double> kKnotC = {4.0, 5.5};
const std::vector<double> kKnotA = {0.01, 0.004};
const std::vector<double> kKnotEc = {-0.5, -0.1, 0.3};
constexpr std::size_t kMb = 4, kMk = 3;  // embedding sizes

double g_bump(std::size_t m, double rho) {
  return embedding_value(BasisKind::Blip, rho, kBumpEs[m], kBumpEc[m]);
}
double g_knot(std::size_t m, double rho) {
  return embedding_value(BasisKind::Knot5, rho, 1.0, kKnotEc[m], 5);
}

// DM_mJoin: DM_Blip | DM_REAM (Blip) | DM_REAM (Knot5), so both DM_REAM
// blocks sit at non-zero aed / density offsets. embf0 == nullptr leaves the
// key absent.
Context make_mjoin_ctx(const char *embf0) {
  Context c;
  c.add("VERBOSE", 0);
  for (int k = 0; k < 3; ++k) c.add("RCTYPEMB", "Cut_Cos");
  c.add("RCUTMB", 5.0);
  c.add("RCUTMB", 6.0);
  c.add("RCUTMB", 6.0);
  c.remove("INITMB");
  c.add("INITMB", "true");
  c.add("TYPEMB", "DM_mJoin");
  c.add("TYPEMB", "DM_Blip 0 2 2 H H");
  c.add("TYPEMB", "DM_REAM 0 3 3 3 4 4 0 0 H H");
  c.add("TYPEMB", "DM_REAM 2 2 0 2 3 0 0 0 H H");
  c.add("CGRIDMB", "1.7 2.3");
  c.add("CGRIDMB", kBumpC);
  c.add("CGRIDMB", kKnotC);
  c.add("SGRIDMB", "0.5 0.7");
  c.add("SGRIDMB", kBumpS);
  c.add("AMPGRIDMB", kBumpA);
  c.add("AMPGRIDMB", kKnotA);
  c.add("RHOFORM", "SIGNED");  // one global form for every DM_REAM child
  c.add("CEMBFUNC", kBumpEc);
  c.add("CEMBFUNC", kKnotEc);
  c.add("SEMBFUNC", kBumpEs);
  if (embf0) c.add("EMBF0", embf0);
  return c;
}

// The same DM_REAM blocks as standalone descriptors.
Context make_ream_ctx(bool knot, const char *embf0) {
  Context c;
  c.add("VERBOSE", 0);
  c.add("INITMB", "true");
  c.add("RCUTMB", 6.0);
  c.add("RCTYPEMB", "Cut_Cos");
  c.add("TYPEMB", knot ? "DM_REAM 2 2 0 2 3 0 0 0 H H"
                       : "DM_REAM 0 3 3 3 4 4 0 0 H H");
  c.add("CGRIDMB", knot ? kKnotC : kBumpC);
  if (!knot) c.add("SGRIDMB", kBumpS);
  c.add("AMPGRIDMB", knot ? kKnotA : kBumpA);
  c.add("RHOFORM", "SIGNED");
  c.add("CEMBFUNC", knot ? kKnotEc : kBumpEc);
  if (!knot) c.add("SEMBFUNC", kBumpEs);
  if (embf0) c.add("EMBF0", embf0);
  return c;
}

// aed of one atom: isolated (rho == 0) or with a single neighbour.
std::vector<double> eval_aed(DM_Base &d, bool neighbour) {
  rho_type rho;
  d.init_rhoi(rho);
  if (neighbour) {
    Vec3d del(2.1, 0.3, 0.4);
    d.calc_rho_mb(1, 1, del, rho);
  }
  aed_type aed(d.size());
  aed.set_zero();
  d.calc_aed(rho, aed);
  return std::vector<double>(aed.data(), aed.data() + d.size());
}

// The DM_REAM blocks of a DM_mJoin aed (they are the last kMb + kMk).
void check_ream_blocks_at_zero(DM_mJoin &dm, EmbF0Mode mode) {
  REQUIRE(dm.embf0_mode() == mode);
  const std::vector<double> aed = eval_aed(dm, false);
  REQUIRE(aed.size() > kMb + kMk);
  const std::size_t ob = aed.size() - kMb - kMk, ok = ob + kMb;
  for (std::size_t m = 0; m < kMb; ++m) {
    INFO("Blip block m=" << m);
    if (mode == EmbF0Mode::Zero)
      REQUIRE(aed[ob + m] == 0.0);
    else
      REQUIRE(mj_bits(aed[ob + m]) == mj_bits(g_bump(m, 0.0)));
  }
  for (std::size_t m = 0; m < kMk; ++m) {
    INFO("Knot5 block m=" << m);
    if (mode == EmbF0Mode::Zero)
      REQUIRE(aed[ok + m] == 0.0);
    else
      REQUIRE(mj_bits(aed[ok + m]) == mj_bits(g_knot(m, 0.0)));
  }
}

}  // namespace

TEST_CASE_METHOD(MjoinEmbf0Fixture,
                 "DM_mJoin with DM_REAM children: EMBF0 ZERO by default",
                 "[mjoin][embf0]") {
  // the fixture's reference really has g(0) != 0 in both blocks
  REQUIRE(g_bump(1, 0.0) == 1.0);
  REQUIRE(g_knot(0, 0.0) > 0.0);

  Context c = make_mjoin_ctx(nullptr);
  DM_mJoin dm(c);
  dm.set_fidx(0);
  check_ream_blocks_at_zero(dm, EmbF0Mode::Zero);

  // at a non-zero density each block equals its standalone DM_REAM
  const std::vector<double> aed = eval_aed(dm, true);
  const std::size_t ob = aed.size() - kMb - kMk, ok = ob + kMb;
  Context cb = make_ream_ctx(false, nullptr), ck = make_ream_ctx(true, nullptr);
  DM_REAM rb(cb), rk(ck);
  const std::vector<double> ab = eval_aed(rb, true), ak = eval_aed(rk, true);
  for (std::size_t m = 0; m < kMb; ++m) {
    REQUIRE(ab[m] != 0.0);
    REQUIRE(mj_bits(aed[ob + m]) == mj_bits(ab[m]));
  }
  for (std::size_t m = 0; m < kMk; ++m) {
    REQUIRE(ak[m] != 0.0);
    REQUIRE(mj_bits(aed[ok + m]) == mj_bits(ak[m]));
  }
}

TEST_CASE_METHOD(MjoinEmbf0Fixture,
                 "DM_mJoin: a parent EMBF0 reaches every DM_REAM child in "
                 "the ctor",
                 "[mjoin][embf0]") {
  const Context cz = make_mjoin_ctx(nullptr);
  const Context cf = make_mjoin_ctx("FREE");

  SECTION("ctor") {
    Context c = cf;
    DM_mJoin dm(c);
    dm.set_fidx(0);
    check_ream_blocks_at_zero(dm, EmbF0Mode::Free);
  }
  SECTION("ctor, no EMBF0 (ZERO)") {
    Context c = cz;
    DM_mJoin dm(c);
    dm.set_fidx(0);
    check_ream_blocks_at_zero(dm, EmbF0Mode::Zero);
  }
  SECTION("ctor, explicit ZERO") {
    Context c = make_mjoin_ctx("ZERO");
    DM_mJoin dm(c);
    dm.set_fidx(0);
    check_ream_blocks_at_zero(dm, EmbF0Mode::Zero);
  }
}

TEST_CASE_METHOD(MjoinEmbf0Fixture,
                 "DM_mJoin forwards raw_embedding_f0 (sum over DM_REAM children)",
                 "[mjoin][embf0]") {
  for (const char *key : {static_cast<const char *>(nullptr), "FREE"}) {
    INFO("EMBF0 " << (key ? key : "(absent)"));
    Context c = make_mjoin_ctx(key);
    DM_mJoin dm(c);
    dm.set_fidx(0);
    const std::size_t n = dm.size();
    const std::size_t ob = n - kMb - kMk, ok = ob + kMb;
    t_type w(n);
    for (std::size_t k = 0; k < n; ++k)
      w(k) = (k % 2 ? -0.1 : 0.1) * static_cast<double>(k + 1);

    // each child indexes its own block: sum_m w(fidx + m) g_m(0), summed in
    // child order
    double raw_b = 0.0, raw_k = 0.0;
    for (std::size_t m = 0; m < kMb; ++m) raw_b += w(ob + m) * g_bump(m, 0.0);
    for (std::size_t m = 0; m < kMk; ++m) raw_k += w(ok + m) * g_knot(m, 0.0);
    const auto raw = dm.raw_embedding_f0(w);
    REQUIRE(raw.has_value());
    REQUIRE(mj_bits(*raw) == mj_bits(raw_b + raw_k));
    REQUIRE(*raw != 0.0);

    // mode-independent: the raw F(0) is the same for a ZERO/FREE twin
    Context c2 = make_mjoin_ctx(key ? nullptr : "FREE");
    DM_mJoin twin(c2);
    twin.set_fidx(0);
    REQUIRE(twin.embf0_mode() != dm.embf0_mode());
    REQUIRE(mj_bits(twin.raw_embedding_f0(w).value()) == mj_bits(*raw));

    // a weight vector that reaches no DM_REAM block: no child reports
    const t_type w_short(ob);
    REQUIRE_FALSE(dm.raw_embedding_f0(w_short).has_value());
  }

  // no DM_REAM child: nothing to forward
  Context c;
  c.add("VERBOSE", 0);
  c.add("RCTYPEMB", "Cut_Cos");
  c.add("RCTYPEMB", "Cut_Cos");
  c.add("RCUTMB", 5.0);
  c.add("RCUTMB", 5.0);
  c.remove("INITMB");
  c.add("INITMB", "true");
  c.add("TYPEMB", "DM_mJoin");
  c.add("TYPEMB", "DM_Blip 0 2 2 H H");
  c.add("TYPEMB", "DM_Blip 0 1 1 H H");
  c.add("CGRIDMB", "1.7 2.3 1.1");
  c.add("SGRIDMB", "0.5 0.7 0.9");
  DM_mJoin dm(c);
  dm.set_fidx(0);
  t_type w(dm.size());
  REQUIRE_FALSE(dm.embf0_mode().has_value());
  REQUIRE_FALSE(dm.raw_embedding_f0(w).has_value());
}
