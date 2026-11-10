/* EMBF0: the DM_REAM embedding reference, runtime exactness and FREE
 * bit-identity.
 *
 *  - EMBF0 ZERO (default): the zero-referenced basis
 *    g~_m(rho) = g_m(rho) - g_m(0). At rho == 0 (+0.0 or -0.0) every
 *    embedding output is exactly 0.0 and F(0) == 0.0 for any weights.
 *  - EMBF0 FREE: the raw basis of tadah 1.5.0, bit-identical to the HEAD
 *    loops (reproduced verbatim below).
 *  - g'_m, so every force descriptor and F'(rho), is bitwise the same in
 *    both modes.
 *
 * The random draws (fixed std::mt19937 seeds, the same draws in every test
 * case) cover Blip, Gaussian, Knot5 deg 5, Knot5 deg 3 and Blip5 embeddings:
 * M in [1, 40], negative centres/knots, widths log-uniform up to 300, support
 * edges hit exactly (|s c| = 2, 3 and +-1e-12; knots at 0 and +-1e-12), and
 * in ~30% of the draws >= 5 weights of magnitude 13..54 whose raw F(0)
 * cancels to ~1e-6 S0 (S0 = sum_m |w_m g_m(0)|). Observed with these seeds:
 * max |F_raw(0)| / (eps S0) = 1/eps (single-term draws) and ~4.5e9
 * (= 1e-6 / eps) on every cancelling draw.
 */

#include "catch2/catch.hpp"
#include <tadah/core/core_types.h>
#include <tadah/core/periodic_table.h>
#include <tadah/models/cutoffs.h>
#include <tadah/models/descriptors/basis_dispatch.h>
#include <tadah/models/descriptors/dm/dm_ream.h>
#include <tadah/models/descriptors/dm/embf0.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <random>
#include <sstream>
#include <string>
#include <vector>

using namespace tadah::core;
using namespace tadah::models;

namespace {

struct ReamEmbf0Fixture {
  ReamEmbf0Fixture() { PeriodicTable::initialize(); }
};

std::uint64_t bits(double v) {
  std::uint64_t u;
  std::memcpy(&u, &v, sizeof u);
  return u;
}

bool same_bits(const std::vector<double> &a, const std::vector<double> &b) {
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i)
    if (bits(a[i]) != bits(b[i])) return false;
  return true;
}

// ------------------------------------------------------------------------
//  Random embedding draws
// ------------------------------------------------------------------------

constexpr double kEps = std::numeric_limits<double>::epsilon();

// One embedding family of the random draws: the TYPEMB basis code, the
// dispatcher kind and the knot degree (KDEGEMB; unused by the bump families).
struct Family {
  const char *name;
  int bcode;
  BasisKind kind;
  int deg;
};
const Family kFamilies[] = {
    {"Blip", 0, BasisKind::Blip, 5},
    {"Gaussian", 1, BasisKind::Gaussian, 5},
    {"Knot5 deg 5", 2, BasisKind::Knot5, 5},
    {"Knot5 deg 3", 2, BasisKind::Knot5, 3},
    {"Blip5", 3, BasisKind::Blip5, 5},
};
constexpr std::size_t kNumFamilies = sizeof(kFamilies) / sizeof(kFamilies[0]);

// An embedding basis {c_m, s_m} (s empty for the knot family: eta is a
// gauge) with the linear weights w_m of one draw.
struct EmbDraw {
  Family fam;
  std::vector<double> c, s, w;
  bool cancel = false;  // >= 5 weights of magnitude 13..54 cancel at rho = 0
  std::size_t M() const { return c.size(); }
  double sm(std::size_t m) const { return s.empty() ? 1.0 : s[m]; }
  // The public basis: the reference every DM_REAM output is checked against.
  double g(std::size_t m, double rho) const {
    return embedding_value(fam.kind, rho, sm(m), c[m], fam.deg);
  }
  double dg(std::size_t m, double rho) const {
    return embedding_deriv(fam.kind, rho, sm(m), c[m], fam.deg);
  }
};

double udraw(std::mt19937 &rng, double a, double b) {
  return std::uniform_real_distribution<double>(a, b)(rng);
}
int idraw(std::mt19937 &rng, int a, int b) {
  return std::uniform_int_distribution<int>(a, b)(rng);
}
double sign_draw(std::mt19937 &rng) { return idraw(rng, 0, 1) ? 1.0 : -1.0; }

constexpr double kSMin = 0.05;  // widths: log-uniform in [kSMin, 300]

// One embedding atom (c, s). act: 0 = anywhere, with forced support edges;
// 1 = active at rho = 0 with a sizeable g(0) > 0; 2 = inactive at rho = 0
// (g(0) == 0; Gaussian: g(0) < 3e-16).
void draw_atom(const Family &f, std::mt19937 &rng, int act, double &c,
               double &s) {
  s = 1.0;
  if (f.kind == BasisKind::Knot5) {  // RIGHT-active: g(0) = (-c)_+^deg
    if (act == 1) c = udraw(rng, -2.5, -0.3);
    else if (act == 2) c = udraw(rng, 0.0, 3.5);
    else c = udraw(rng, -2.5, 3.5);
    if (act == 0 && udraw(rng, 0.0, 1.0) < 0.1) {
      const double edge[3] = {0.0, -1e-12, 1e-12};
      c = edge[idraw(rng, 0, 2)];
    }
    return;
  }
  s = std::exp(udraw(rng, std::log(kSMin), std::log(300.0)));
  if (f.kind == BasisKind::Gaussian) {  // g(0) = exp(-v^2), v = sqrt(s) c
    double v;
    if (act == 1) {
      v = udraw(rng, -2.0, 2.0);
    } else if (act == 2) {
      const double sg = sign_draw(rng);
      v = sg * udraw(rng, 6.0, 12.0);
    } else {
      v = udraw(rng, -5.0, 5.0);
    }
    c = v / std::sqrt(s);
    return;
  }
  // Bumps: x(0) = s (0 - c) = -u with u = s c; support |x| < H.
  const double H = f.kind == BasisKind::Blip5 ? 3.0 : 2.0;
  double u;
  if (act == 1) {
    u = udraw(rng, -(H - 0.5), H - 0.5);
  } else if (act == 2) {
    const double sg = sign_draw(rng);
    u = sg * udraw(rng, H, 3.5);
  } else {
    u = udraw(rng, -3.5, 3.5);
  }
  if (act == 0 && udraw(rng, 0.0, 1.0) < 0.15) {
    // support edge |s c| = H, H - 1e-12, H + 1e-12 exactly: with s a power
    // of two, c = u / s and s * c are exact
    const double edge[3] = {H, H - 1e-12, H + 1e-12};
    s = std::ldexp(1.0, idraw(rng, -3, 8));
    const double sg = sign_draw(rng);
    u = sg * edge[idraw(rng, 0, 2)];
  }
  c = u / s;
}

// Rewrites w[0, nJ) (atoms active at rho = 0, g(0) > 0) as weights of
// magnitude 13..54 whose raw F(0) contributions cancel: a random split
// J1 | J2, w = +a on J1 and w = -lambda b on J2, lambda solving
// sum_m w_m g_m(0) = 0, then a 1e-6 S0 nudge on the largest J1 term.
// False when no split fits the magnitude window.
bool cancel_weights(EmbDraw &d, std::size_t nJ, std::mt19937 &rng) {
  const std::size_t M = d.M();
  std::vector<double> g0(M);
  for (std::size_t m = 0; m < M; ++m) g0[m] = d.g(m, 0.0);
  for (int t = 0; t < 256; ++t) {
    std::vector<double> w = d.w;
    std::vector<int> in2(nJ);
    std::size_t n2 = 0;
    for (std::size_t j = 0; j < nJ; ++j) n2 += (in2[j] = idraw(rng, 0, 1));
    if (n2 == 0 || n2 == nJ) continue;
    const double b = udraw(rng, 20.0, 40.0);
    double A = 0.0, B = 0.0;
    for (std::size_t j = 0; j < nJ; ++j) {
      if (in2[j]) {
        w[j] = -b * udraw(rng, 0.95, 1.05);
        B -= w[j] * g0[j];
      } else {
        w[j] = udraw(rng, 13.5, 53.5);
      }
    }
    for (std::size_t m = 0; m < M; ++m)
      if (m >= nJ || !in2[m]) A += w[m] * g0[m];
    const double lambda = A / B;
    bool ok = lambda > 0.0;
    for (std::size_t j = 0; j < nJ; ++j) {
      if (!in2[j]) continue;
      w[j] *= lambda;
      ok = ok && -w[j] >= 13.5 && -w[j] <= 53.5;
    }
    if (!ok) continue;
    double S0 = 0.0;
    for (std::size_t m = 0; m < M; ++m) S0 += std::fabs(w[m] * g0[m]);
    std::size_t L = nJ;
    for (std::size_t j = 0; j < nJ; ++j)
      if (!in2[j] && (L == nJ || g0[j] > g0[L])) L = j;
    const double sg = sign_draw(rng);
    w[L] += sg * 1e-6 * S0 / g0[L];
    d.w = w;
    return true;
  }
  return false;
}

// One draw: M in [1, 40] (cancel draws: M >= 5, nJ >= 5 active atoms, the
// rest inactive at rho = 0), weights uniform in [-60, 60].
EmbDraw draw_emb(const Family &f, std::mt19937 &rng, bool try_cancel) {
  EmbDraw d;
  d.fam = f;
  const std::size_t M =
      static_cast<std::size_t>(idraw(rng, try_cancel ? 5 : 1, 40));
  const std::size_t nJ =
      try_cancel ? static_cast<std::size_t>(
                       idraw(rng, 5, std::min<int>(static_cast<int>(M), 16)))
                 : 0;
  d.c.resize(M);
  d.s.resize(M);
  for (std::size_t m = 0; m < M; ++m)
    draw_atom(f, rng, try_cancel ? (m < nJ ? 1 : 2) : 0, d.c[m], d.s[m]);
  if (f.kind == BasisKind::Knot5) d.s.clear();
  d.w.resize(M);
  for (std::size_t m = 0; m < M; ++m) d.w[m] = udraw(rng, -60.0, 60.0);
  if (try_cancel) d.cancel = cancel_weights(d, nJ, rng);
  return d;
}

constexpr int kDraws = 1000;  // per family

// The draws of family fi, in order (seeded per family, so every test case
// sees the same draws): fn(draw_index, draw).
template <class Fn> void for_each_draw(std::size_t fi, Fn &&fn) {
  std::mt19937 rng(20260925u + 97u * static_cast<unsigned>(fi));
  for (int it = 0; it < kDraws; ++it) {
    const bool try_cancel = udraw(rng, 0.0, 1.0) < 0.3;
    const EmbDraw d = draw_emb(kFamilies[fi], rng, try_cancel);
    fn(it, d);
  }
}

// ------------------------------------------------------------------------
//  Descriptors and evaluation helpers
// ------------------------------------------------------------------------

// DM_REAM context for one draw: a fixed 3-term density (bumps: centres and
// widths; knots: LEFT-active knots, no width channel) and the draw's
// embedding. embf0 == nullptr leaves EMBF0 absent (default ZERO).
Context make_draw_ctx(const EmbDraw &d, const char *embf0) {
  const bool knot = d.fam.kind == BasisKind::Knot5;
  const bool kdeg = knot && d.fam.deg == 3;
  const int M = static_cast<int>(d.M());
  Context c;
  c.add("VERBOSE", 0);
  c.add("INITMB", "true");
  c.add("RCUTMB", 6.0);
  c.add("RCTYPEMB", "Cut_Cos");
  c.add("TYPEMB", "DM_REAM");
  c.add("TYPEMB", d.fam.bcode);
  // size tokens per keys {C,S,AMP,Ec,Es} then data_keys {Kd,Ke}
  c.add("TYPEMB", 3); c.add("TYPEMB", knot ? 0 : 3); c.add("TYPEMB", 3);
  c.add("TYPEMB", M); c.add("TYPEMB", knot ? 0 : M);
  c.add("TYPEMB", 0); c.add("TYPEMB", kdeg ? M : 0);
  c.add("TYPEMB", "H");
  c.add("TYPEMB", "H");
  c.add("CGRIDMB", "2.0 3.5 5.0");
  if (!knot) c.add("SGRIDMB", "1.0 1.0 1.0");
  c.add("AMPGRIDMB", knot ? "0.010 0.00625 0.003" : "0.40 0.25 0.12");
  c.add("RHOFORM", "SIGNED");
  c.add("CEMBFUNC", d.c);  // lossless: the descriptor sees these exact doubles
  if (!knot) c.add("SEMBFUNC", d.s);
  if (kdeg) c.add("KDEGEMB", std::vector<double>(d.M(), 3.0));
  if (embf0) c.add("EMBF0", embf0);
  return c;
}

// The two descriptors of one draw: EMBF0 absent (default ZERO) and FREE.
struct Twins {
  Context cz, cf;
  DM_REAM z, f;
  explicit Twins(const EmbDraw &d)
      : cz(make_draw_ctx(d, nullptr)), cf(make_draw_ctx(d, "FREE")), z(cz),
        f(cf) {}
};

t_type weights(const EmbDraw &d) {
  t_type w(d.M());
  for (std::size_t m = 0; m < d.M(); ++m) w(m) = d.w[m];
  return w;
}

// calc_aed at a density rho_i on a fresh density array: the embedding block
// of aed (every entry pre-set to `pre`) and the stored g'_m(rho_i).
struct AedOut {
  std::vector<double> aed, gp;
};
AedOut eval_aed(DM_REAM &dm, double rho_i, double pre = 0.0) {
  rho_type rho;
  dm.init_rhoi(rho);
  rho(dm.get_rfidx()) = rho_i;
  aed_type aed(dm.size());
  for (std::size_t k = 0; k < dm.size(); ++k) aed(k) = pre;
  dm.calc_aed(rho, aed);
  AedOut out;
  for (std::size_t m = 0; m < dm.size(); ++m) {
    out.aed.push_back(aed(dm.get_fidx() + m));
    out.gp.push_back(rho(dm.get_drfidx() + m));
  }
  return out;
}

// tadah 1.5.0 DM_REAM::eam_embed, verbatim: the EMBF0 FREE contract.
void head_eam_embed(const EmbDraw &d, const t_type &w_full, std::size_t fidx,
                    const double rho_bar, double &F, double &Fp) {
  F = 0.0; Fp = 0.0;
  const std::size_t M = d.M();
  for (std::size_t m = 0; m < M; ++m) {
    const double w = w_full(fidx + m);
    const double s = (m < d.s.size()) ? d.s[m] : 1.0;
    double emb_value, emb_deriv;
    embedding_value_deriv(d.fam.kind, rho_bar, s, d.c[m],
                          emb_value, emb_deriv, d.fam.deg);
    F  += w * emb_value;
    Fp += w * emb_deriv;
  }
}

// Bound on |F~(h)| for a small h > 0 under EMBF0 ZERO (F~(0) = 0):
//   h L + 4 n0 eps S0 + 8 eps R.
//  - L = sum_m |w_m| max(|g'_m(0)|, |g'_m(h)|), the mean-value bound per
//    term over [0, h]. |F'(0)| alone is not a bound: a support edge (|s c| =
//    H - 1e-12, a knot at 0 or -1e-12) lies within s h of rho = 0, so
//    g'_m(0) ~ 0 while g_m(h) != 0, and the terms of F'(0) may cancel.
//  - 4 n0 eps S0: the reference subtraction, n0 = #{m : g_m(0) != 0}.
//  - R = sum_m |w_m| (kappa_m G_m + |c_m| G'_m), G_m = max(|g_m(0)|,
//    |g_m(h)|): the round-off of evaluating g_m near 0 (kappa_m: the
//    primitive's error in units of G_m -- the bump polynomials cancel
//    internally, the Gaussian exponent carries s c^2) and of its argument
//    s (h - c) (|c| eps in rho, times the slope).
double continuity_bound(const EmbDraw &d, double h) {
  double L = 0.0, S0 = 0.0, R = 0.0;
  int n0 = 0;
  for (std::size_t m = 0; m < d.M(); ++m) {
    const double g0 = d.g(m, 0.0);
    const double G = std::max(std::fabs(g0), std::fabs(d.g(m, h)));
    const double Gp =
        std::max(std::fabs(d.dg(m, 0.0)), std::fabs(d.dg(m, h)));
    double kappa = 8.0;  // knots: products only
    if (d.fam.kind == BasisKind::Blip) kappa = 32.0;
    if (d.fam.kind == BasisKind::Blip5) kappa = 64.0;
    if (d.fam.kind == BasisKind::Gaussian)
      kappa = 4.0 * (1.0 + d.sm(m) * d.c[m] * d.c[m]);
    const double aw = std::fabs(d.w[m]);
    L += aw * Gp;
    S0 += aw * std::fabs(g0);
    n0 += g0 != 0.0;
    R += aw * (kappa * G + std::fabs(d.c[m]) * Gp);
  }
  return h * L + 4.0 * n0 * kEps * S0 + 8.0 * kEps * R;
}

// Collects violations of many cheap checks and keeps the first message, so
// the random-draw loops report a few assertions instead of millions.
struct Violations {
  std::size_t n = 0;
  std::string first;
  template <class Msg> void check(bool ok, Msg &&msg) {
    if (ok) return;
    if (n++ == 0) first = msg();
  }
};

std::string describe(const EmbDraw &d, int it, std::size_t m, double rho,
                     const std::string &what) {
  std::ostringstream os;
  os.precision(17);
  os << d.fam.name << " draw " << it << " (M=" << d.M()
     << (d.cancel ? ", cancelling" : "") << ") m=" << m;
  if (m < d.M())
    os << " c=" << d.c[m] << " s=" << d.sm(m) << " w=" << d.w[m];
  os << " rho=" << rho << ": " << what;
  return os.str();
}

std::string vals(double a, double b) {
  std::ostringstream os;
  os.precision(17);
  os << a << " vs " << b;
  return os.str();
}

bool finite_all(const std::vector<double> &v) {
  for (double x : v)
    if (!std::isfinite(x)) return false;
  return true;
}

// Rho grid of the FREE bit-identity and ZERO-vs-FREE checks.
const double kRhoGrid[] = {0.0, 1e-9, 0.1, 0.207, 0.5, 1.0,
                           1.5, 2.03, 3.0, 5.3};

// Every output of a descriptor for draw d, for bitwise comparisons:
// calc_aed (block + stored g') and eam_embed at the grid and at -0.0 and
// 1e-12, and raw_embedding_f0.
std::vector<double> all_outputs(DM_REAM &dm, const EmbDraw &d) {
  std::vector<double> out;
  const t_type w = weights(d);
  std::vector<double> rhos(std::begin(kRhoGrid), std::end(kRhoGrid));
  rhos.push_back(-0.0);
  rhos.push_back(1e-12);
  for (double rho : rhos) {
    const AedOut a = eval_aed(dm, rho);
    out.insert(out.end(), a.aed.begin(), a.aed.end());
    out.insert(out.end(), a.gp.begin(), a.gp.end());
    double F, Fp;
    dm.eam_embed(w, rho, F, Fp);
    out.push_back(F);
    out.push_back(Fp);
  }
  out.push_back(dm.raw_embedding_f0(w).value_or(1e300));
  return out;
}

// Small cluster for the force descriptors (atom 4 is isolated: rho == 0).
const std::vector<Vec3d> kCluster = {{0.0, 0.0, 0.0},
                                     {2.6, 0.3, -0.2},
                                     {-0.4, 2.8, 0.5},
                                     {1.9, -2.2, 1.4},
                                     {40.0, 40.0, 40.0}};

// Per-atom density arrays of a cluster, g'_m slots filled by calc_aed.
std::vector<rho_type> cluster_rho(DM_REAM &dm, const std::vector<Vec3d> &pos) {
  std::vector<rho_type> rho(pos.size());
  for (std::size_t i = 0; i < pos.size(); ++i) {
    dm.init_rhoi(rho[i]);
    for (std::size_t j = 0; j < pos.size(); ++j) {
      if (j == i) continue;
      Vec3d del = pos[i] - pos[j];
      dm.calc_rho_mb(1, 1, del, rho[i]);
    }
    aed_type aed(dm.size());
    dm.calc_aed(rho[i], aed);
  }
  return rho;
}

// Densities and both force descriptors of every ordered pair of a cluster.
std::vector<double> cluster_forces(DM_REAM &dm, const std::vector<Vec3d> &pos) {
  std::vector<rho_type> rho = cluster_rho(dm, pos);
  std::vector<double> out;
  for (std::size_t i = 0; i < pos.size(); ++i)
    for (std::size_t k = 0; k < rho[i].size(); ++k) out.push_back(rho[i](k));
  for (std::size_t i = 0; i < pos.size(); ++i) {
    for (std::size_t j = 0; j < pos.size(); ++j) {
      if (j == i) continue;
      Vec3d del = pos[i] - pos[j];
      const double r = std::sqrt(del * del);
      fd_type fd2(dm.size()), fd1(dm.size());
      fd2.set_zero();
      fd1.set_zero();
      dm.calc_dXijdri_dXjidri(1, 1, r, r * r, del, rho[i], rho[j], fd2, 1.0);
      dm.calc_dXijdri(1, 1, r, r * r, del, rho[i], fd1, 1.0);
      for (std::size_t m = 0; m < dm.size(); ++m)
        for (std::size_t x = 0; x < 3; ++x) {
          out.push_back(fd2(m, x));
          out.push_back(fd1(m, x));
        }
    }
  }
  return out;
}

// Total cluster energy E = sum_i sum_m w_m aed_m(rho_i), from scratch.
double cluster_energy(DM_REAM &dm, const std::vector<Vec3d> &pos,
                      const t_type &w) {
  double E = 0.0;
  for (std::size_t i = 0; i < pos.size(); ++i) {
    rho_type rhoi;
    dm.init_rhoi(rhoi);
    for (std::size_t j = 0; j < pos.size(); ++j) {
      if (j == i) continue;
      Vec3d del = pos[i] - pos[j];
      dm.calc_rho_mb(1, 1, del, rhoi);
    }
    aed_type aed(dm.size());
    aed.set_zero();
    dm.calc_aed(rhoi, aed);
    for (std::size_t m = 0; m < dm.size(); ++m) E += w(m) * aed(m);
  }
  return E;
}

}  // namespace

// ------------------------------------------------------------------------
//  1. Key handling
// ------------------------------------------------------------------------

TEST_CASE_METHOD(ReamEmbf0Fixture, "DM_REAM EMBF0: key handling",
                 "[ream][embf0]") {
  EmbDraw d;
  d.fam = kFamilies[0];
  d.c = {-0.3, 0.2, 0.9};
  d.s = {1.5, 1.5, 1.5};
  d.w = {0.5, -0.2, 0.1};

  SECTION("absent: default ZERO, materialised into the ctor's context") {
    Context c = make_draw_ctx(d, nullptr);
    REQUIRE_FALSE(c.has_stored("EMBF0"));
    DM_REAM dm(c);
    REQUIRE(dm.embf0_mode() == EmbF0Mode::Zero);
    REQUIRE(c.stored_value("EMBF0").value_or("") == "ZERO");
    REQUIRE(stored_embf0(c) == EmbF0Mode::Zero);
    // standalone layout used by the helpers below
    REQUIRE(dm.get_fidx() == 0);
    REQUIRE(dm.get_rfidx() == 0);
    REQUIRE(dm.get_drfidx() == 1);
  }
  SECTION("explicit FREE and ZERO") {
    Context cf = make_draw_ctx(d, "FREE");
    DM_REAM f(cf);
    REQUIRE(f.embf0_mode() == EmbF0Mode::Free);
    REQUIRE(cf.stored_value("EMBF0").value_or("") == "FREE");
    Context cz = make_draw_ctx(d, "ZERO");
    DM_REAM z(cz);
    REQUIRE(z.embf0_mode() == EmbF0Mode::Zero);
  }
  SECTION("case-insensitive values") {
    Context cf = make_draw_ctx(d, "free");
    DM_REAM f(cf);
    REQUIRE(f.embf0_mode() == EmbF0Mode::Free);
    Context cz = make_draw_ctx(d, "Zero");
    DM_REAM z(cz);
    REQUIRE(z.embf0_mode() == EmbF0Mode::Zero);
  }
  SECTION("an invalid value throws, naming ZERO and FREE") {
    Context c = make_draw_ctx(d, "ON");
    using Catch::Matchers::Contains;
    REQUIRE_THROWS_WITH(DM_REAM(c),
                        Contains("ZERO") && Contains("FREE") && Contains("ON"));
  }
  SECTION("INITMB false: nothing is read, no key materialised") {
    Context c = make_draw_ctx(d, nullptr);
    c.add("INITMB", "false");  // single-valued key: overwrites
    DM_REAM dm(c);
    REQUIRE(dm.size() == 0);
    REQUIRE_FALSE(c.has_stored("EMBF0"));
  }
}

TEST_CASE("EMBF0: absent-key consequence text (DM_REAM notice, LAMMPS "
          "pair_coeff WARNING)",
          "[ream][embf0][notice]") {
  using Catch::Matchers::Contains;
  const std::string lin = embf0_absent_consequence(true);
  const std::string nl = embf0_absent_consequence(false);
  for (const std::string *t : {&lin, &nl}) {
    REQUIRE_THAT(*t, Contains("default EMBF0 ZERO"));
    REQUIRE_THAT(*t, Contains("tadah 1.5.0 or earlier"));
    /* It said "add 'EMBF0 FREE' to the potential ...". */
    REQUIRE_THAT(*t, Contains("an EMBF0 FREE line in the potential "
                              "reproduces"));
  }
  REQUIRE_THAT(lin, Contains("forces and stresses are unchanged"));
  REQUIRE_THAT(nl, Contains("energies AND forces change"));
}

TEST_CASE_METHOD(ReamEmbf0Fixture,
                 "DM_REAM EMBF0: the reference follows the block offset (fidx)",
                 "[ream][embf0]") {
  // The g(0) arrays are indexed by m, the weights and aed by fidx + m: a
  // shifted block gives the unshifted results bitwise, in both modes.
  EmbDraw d;
  d.fam = kFamilies[4];
  d.c = {-0.4, 0.0, 0.5, 1.0, -2.1};
  d.s = {1.5, 1.5, 1.5, 1.5, 0.8};
  d.w = {0.7, -1.3, 2.1, -0.9, 0.4};
  const std::size_t M = d.M(), off = 3;
  for (const char *key : {static_cast<const char *>(nullptr), "FREE"}) {
    INFO("EMBF0 " << (key ? key : "(absent)"));
    Context c0 = make_draw_ctx(d, key), c1 = make_draw_ctx(d, key);
    DM_REAM dm0(c0), dm1(c1);
    dm1.set_fidx(off);
    const t_type w0 = weights(d);
    t_type w1(off + M);
    for (std::size_t k = 0; k < off; ++k) w1(k) = 1e3 * (k + 1.0);  // not ours
    for (std::size_t m = 0; m < M; ++m) w1(off + m) = d.w[m];
    for (double rho : {0.0, 1e-12, 0.3, 1.7}) {
      INFO("rho " << rho);
      double F0, Fp0, F1, Fp1;
      dm0.eam_embed(w0, rho, F0, Fp0);
      dm1.eam_embed(w1, rho, F1, Fp1);
      REQUIRE(bits(F1) == bits(F0));
      REQUIRE(bits(Fp1) == bits(Fp0));
      rho_type r0, r1;
      dm0.init_rhoi(r0);
      dm1.init_rhoi(r1);
      r0(0) = rho;
      r1(0) = rho;
      aed_type a0(M), a1(off + M);
      dm0.calc_aed(r0, a0);
      dm1.calc_aed(r1, a1);
      for (std::size_t k = 0; k < off; ++k) REQUIRE(a1(k) == 0.0);
      for (std::size_t m = 0; m < M; ++m) {
        REQUIRE(bits(a1(off + m)) == bits(a0(m)));
        REQUIRE(bits(r1(1 + m)) == bits(r0(1 + m)));
      }
    }
    REQUIRE(bits(dm1.raw_embedding_f0(w1).value()) ==
            bits(dm0.raw_embedding_f0(w0).value()));
    REQUIRE_FALSE(dm1.raw_embedding_f0(w0).has_value());  // too short
  }
}

// ------------------------------------------------------------------------
//  2 + 7. ZERO: exact zero at rho == 0, continuity, raw F(0)
// ------------------------------------------------------------------------

TEST_CASE_METHOD(ReamEmbf0Fixture,
                 "DM_REAM EMBF0 ZERO: F(0) == 0 exactly and continuous "
                 "(random draws)",
                 "[ream][embf0]") {
  const double h = 1e-12;
  for (std::size_t fi = 0; fi < kNumFamilies; ++fi) {
    Violations v;
    int ncancel = 0;
    double max_raw_ratio = 0.0;             // max |F_raw(0)| / (eps S0)
    double min_cancel_ratio = 1e300;        // ... over cancelling draws
    double max_cont = 0.0;                  // max |F~(h)| / bound
    for_each_draw(fi, [&](int it, const EmbDraw &d) {
      Twins tw(d);
      const std::size_t M = d.M();
      const t_type w = weights(d);
      v.check(tw.z.embf0_mode() == EmbF0Mode::Zero &&
                  tw.f.embf0_mode() == EmbF0Mode::Free &&
                  tw.cz.stored_value("EMBF0").value_or("") == "ZERO",
              [&] { return describe(d, it, M, 0.0, "modes / materialised key"); });

      double S0 = 0.0, raw_ref = 0.0;
      for (std::size_t m = 0; m < M; ++m) {
        S0 += std::fabs(d.w[m] * d.g(m, 0.0));
        raw_ref += w(m) * d.g(m, 0.0);  // same order as raw_embedding_f0
      }

      double Ff0, Fpf0;
      tw.f.eam_embed(w, 0.0, Ff0, Fpf0);
      for (double rho0 : {0.0, -0.0}) {
        // calc_aed: nothing added (exact zero on a zeroed aed, a pre-filled
        // aed untouched), g'_m(0) stored as in FREE
        const AedOut az = eval_aed(tw.z, rho0);
        const AedOut af = eval_aed(tw.f, rho0);
        const AedOut ap = eval_aed(tw.z, rho0, 0.37);
        double sum = 0.0;
        for (std::size_t m = 0; m < M; ++m) {
          v.check(az.aed[m] == 0.0, [&] {
            return describe(d, it, m, rho0, "ZERO aed != 0: " + vals(az.aed[m], 0.0));
          });
          v.check(bits(ap.aed[m]) == bits(0.37), [&] {
            return describe(d, it, m, rho0, "ZERO added to a pre-filled aed");
          });
          v.check(bits(az.gp[m]) == bits(af.gp[m]), [&] {
            return describe(d, it, m, rho0, "stored g' differs from FREE: " +
                                                vals(az.gp[m], af.gp[m]));
          });
          sum += w(m) * az.aed[m];
        }
        v.check(sum == 0.0, [&] {
          return describe(d, it, M, rho0, "sum w aed != 0: " + vals(sum, 0.0));
        });
        v.check(finite_all(az.gp) && finite_all(af.aed) && finite_all(af.gp),
                [&] { return describe(d, it, M, rho0, "non-finite output"); });

        // eam_embed: F exactly 0, F' bitwise the FREE twin's
        double F, Fp, Ff, Fpf;
        tw.z.eam_embed(w, rho0, F, Fp);
        tw.f.eam_embed(w, rho0, Ff, Fpf);
        v.check(F == 0.0, [&] {
          return describe(d, it, M, rho0, "eam_embed F(0) != 0: " + vals(F, 0.0));
        });
        v.check(bits(Fp) == bits(Fpf) && bits(Fpf) == bits(Fpf0) &&
                    bits(Ff) == bits(Ff0),
                [&] {
                  return describe(d, it, M, rho0, "F'(0) / FREE(-0.0) mismatch: " +
                                                      vals(Fp, Fpf));
                });
        v.check(std::isfinite(Fp) && std::isfinite(Ff), [&] {
          return describe(d, it, M, rho0, "non-finite F / F'");
        });
      }

      // continuity just above 0: eam_embed and the w-weighted aed
      const double bound = continuity_bound(d, h);
      double Fh, Fph;
      tw.z.eam_embed(w, h, Fh, Fph);
      const AedOut ah = eval_aed(tw.z, h);
      double sumh = 0.0;
      for (std::size_t m = 0; m < M; ++m) sumh += w(m) * ah.aed[m];
      v.check(std::fabs(Fh) <= bound && std::fabs(sumh) <= bound, [&] {
        return describe(d, it, M, h, "|F~(h)| above the continuity bound: " +
                                         vals(std::max(std::fabs(Fh), std::fabs(sumh)), bound));
      });
      v.check(std::isfinite(Fh) && std::isfinite(Fph) && finite_all(ah.aed),
              [&] { return describe(d, it, M, h, "non-finite output"); });
      if (bound > 0.0)
        max_cont = std::max(max_cont, std::max(std::fabs(Fh), std::fabs(sumh)) / bound);

      // 7. raw F(0): the independent sum, mode-independent, == FREE F(0)
      const auto raw_z = tw.z.raw_embedding_f0(w);
      const auto raw_f = tw.f.raw_embedding_f0(w);
      v.check(raw_z && raw_f && bits(*raw_z) == bits(raw_ref) &&
                  bits(*raw_f) == bits(raw_ref),
              [&] {
                return describe(d, it, M, 0.0, "raw_embedding_f0 != sum w g(0): " +
                                                   vals(raw_z.value_or(0.0), raw_ref));
              });
      v.check(std::fabs(raw_ref - Ff0) <= 4.0 * M * kEps * S0, [&] {
        return describe(d, it, M, 0.0, "raw_embedding_f0 != FREE F(0): " +
                                           vals(raw_ref, Ff0));
      });
      if (it < 50) {
        const t_type w_short(M - 1);
        v.check(!tw.z.raw_embedding_f0(w_short), [&] {
          return describe(d, it, M, 0.0, "raw_embedding_f0 on a short w");
        });
      }

      if (S0 > 0.0) max_raw_ratio = std::max(max_raw_ratio, std::fabs(Ff0) / (kEps * S0));
      if (d.cancel) {
        ++ncancel;
        std::size_t nlarge = 0;
        for (std::size_t m = 0; m < M; ++m)
          nlarge += std::fabs(d.w[m]) >= 13.0 && std::fabs(d.w[m]) <= 54.0;
        const double r = std::fabs(Ff0) / S0;
        min_cancel_ratio = std::min(min_cancel_ratio, r / kEps);
        v.check(nlarge >= 5 && r >= 0.5e-6 && r <= 2e-6, [&] {
          return describe(d, it, M, 0.0, "cancelling draw off target: |F(0)|/S0 = " +
                                             vals(r, 1e-6));
        });
      }
    });
    INFO(kFamilies[fi].name << ": " << v.n << " violations; first: " << v.first);
    INFO("cancelling draws " << ncancel << "/" << kDraws
                             << "; max |F_raw(0)|/(eps S0) " << max_raw_ratio
                             << "; min over cancelling draws " << min_cancel_ratio
                             << "; max |F~(1e-12)|/bound " << max_cont);
    CHECK(v.n == 0);
    CHECK(ncancel * 5 >= kDraws);  // >= 20% cancelling draws
  }
}

// ------------------------------------------------------------------------
//  3 + 4 + 5. FREE bit-identity, ZERO == FREE - g(0), forces unchanged
// ------------------------------------------------------------------------

TEST_CASE_METHOD(ReamEmbf0Fixture,
                 "DM_REAM EMBF0: FREE is tadah 1.5.0 bitwise, ZERO is FREE - "
                 "g(0), g' and forces identical (random draws)",
                 "[ream][embf0]") {
  constexpr int kClusterDraws = 100;  // per family: force descriptors
  for (std::size_t fi = 0; fi < kNumFamilies; ++fi) {
    Violations v;
    for_each_draw(fi, [&](int it, const EmbDraw &d) {
      Twins tw(d);
      const std::size_t M = d.M();
      const t_type w = weights(d);
      double Sg0 = 0.0;  // sum |w g(0)|
      for (std::size_t m = 0; m < M; ++m) Sg0 += std::fabs(d.w[m] * d.g(m, 0.0));

      for (double rho : kRhoGrid) {
        const AedOut af = eval_aed(tw.f, rho);
        const AedOut az = eval_aed(tw.z, rho);
        double sum_f = 0.0, sum_z = 0.0, abs_f = 0.0, abs_z = 0.0;
        for (std::size_t m = 0; m < M; ++m) {
          const double g = d.g(m, rho);
          const double g0 = d.g(m, 0.0);
          // 3. FREE: aed == the public basis, bitwise
          v.check(bits(af.aed[m]) == bits(g), [&] {
            return describe(d, it, m, rho, "FREE aed != g: " + vals(af.aed[m], g));
          });
          // 4. ZERO == FREE - g(0), bitwise; stored g' identical
          v.check(bits(az.aed[m]) == bits(af.aed[m] - g0), [&] {
            return describe(d, it, m, rho, "ZERO aed != FREE aed - g(0): " +
                                               vals(az.aed[m], af.aed[m] - g0));
          });
          v.check(bits(az.gp[m]) == bits(af.gp[m]) &&
                      bits(af.gp[m]) == bits(d.dg(m, rho)),
                  [&] {
                    return describe(d, it, m, rho, "stored g' differs: " +
                                                       vals(az.gp[m], af.gp[m]));
                  });
          sum_f += w(m) * af.aed[m];
          sum_z += w(m) * az.aed[m];
          abs_f += std::fabs(w(m) * af.aed[m]);
          abs_z += std::fabs(w(m) * az.aed[m]);
        }
        v.check(finite_all(af.aed) && finite_all(az.aed) && finite_all(af.gp),
                [&] { return describe(d, it, M, rho, "non-finite aed / g'"); });

        // 3. FREE eam_embed == the HEAD loop, bitwise; ZERO F' == FREE F'
        double Ff, Fpf, Fz, Fpz, Fh, Fph;
        tw.f.eam_embed(w, rho, Ff, Fpf);
        tw.z.eam_embed(w, rho, Fz, Fpz);
        head_eam_embed(d, w, tw.f.get_fidx(), rho, Fh, Fph);
        v.check(bits(Ff) == bits(Fh) && bits(Fpf) == bits(Fph), [&] {
          return describe(d, it, M, rho, "FREE eam_embed != HEAD loop: " + vals(Ff, Fh));
        });
        v.check(bits(Fpz) == bits(Fpf), [&] {
          return describe(d, it, M, rho, "ZERO F' != FREE F': " + vals(Fpz, Fpf));
        });
        // 5. eam_embed == w . calc_aed in both modes
        const double tol_f = 4.0 * M * kEps * (abs_f + Sg0);
        const double tol_z = 4.0 * M * kEps * (abs_z + Sg0);
        v.check(std::fabs(Ff - sum_f) <= tol_f && std::fabs(Fz - sum_z) <= tol_z,
                [&] {
                  return describe(d, it, M, rho, "eam_embed != w.aed: FREE " +
                                                     vals(Ff, sum_f) + ", ZERO " +
                                                     vals(Fz, sum_z));
                });
        v.check(std::isfinite(Ff) && std::isfinite(Fz) && std::isfinite(Fpf),
                [&] { return describe(d, it, M, rho, "non-finite F / F'"); });
      }

      // 4. force descriptors on a cluster: bitwise identical in both modes
      if (it < kClusterDraws) {
        const std::vector<double> fz = cluster_forces(tw.z, kCluster);
        const std::vector<double> ff = cluster_forces(tw.f, kCluster);
        v.check(same_bits(fz, ff) && finite_all(ff), [&] {
          return describe(d, it, M, 0.0, "cluster densities / force descriptors differ");
        });
      }
    });
    INFO(kFamilies[fi].name << ": " << v.n << " violations; first: " << v.first);
    CHECK(v.n == 0);
  }
}

// ------------------------------------------------------------------------
//  8. Energy/force consistency under ZERO (negative embedding centres)
// ------------------------------------------------------------------------

TEST_CASE_METHOD(ReamEmbf0Fixture,
                 "DM_REAM EMBF0 ZERO: forces match FD of the energy, energies "
                 "shift by -F(0) per atom",
                 "[ream][embf0][knot5][blip5]") {
  // Knot5 (deg 5 and deg 3) with negative embedding knots (g(0) != 0) and
  // Blip5 with a negative centre; the cluster's densities sit among the
  // knots/centres. Atom 4 is isolated (rho == 0): exactly 0 under ZERO.
  const Family &k5 = kFamilies[2], &k3 = kFamilies[3], &b5 = kFamilies[4];
  EmbDraw cases[3];
  cases[0].fam = k5;
  cases[0].c = {-0.6, -0.2, 0.05, 0.15};
  cases[0].w = {0.7, -1.3, 2.1, -0.9};
  cases[1] = cases[0];
  cases[1].fam = k3;
  cases[2].fam = b5;
  cases[2].c = {-0.4, 0.0, 0.5, 1.0};
  cases[2].s = {1.5, 1.5, 1.5, 1.5};
  cases[2].w = {0.7, -1.3, 2.1, -0.9};

  for (const EmbDraw &d : cases) {
    INFO(d.fam.name);
    Twins tw(d);
    Cut_Cos fcz(6.0), fcf(6.0);
    tw.z.set_fcut(&fcz, false);
    tw.f.set_fcut(&fcf, false);
    const t_type w = weights(d);
    const std::size_t N = kCluster.size();
    const std::size_t M = d.M();

    std::vector<rho_type> rho = cluster_rho(tw.z, kCluster);
    REQUIRE(rho[N - 1](tw.z.get_rfidx()) == 0.0);  // isolated atom
    {
      rho_type r4 = rho[N - 1];
      aed_type aed(M);
      tw.z.calc_aed(r4, aed);
      for (std::size_t m = 0; m < M; ++m) REQUIRE(aed(m) == 0.0);
    }
    for (std::size_t i = 0; i + 1 < N; ++i) {
      const double ri = rho[i](tw.z.get_rfidx());
      INFO("atom " << i << " rho " << ri);
      REQUIRE(ri > 0.03);  // the embedding is excited
      REQUIRE(ri < 1.5);
    }

    // analytic dE/dx_a vs central differences, atoms 0 and 1
    const double h = 1e-6;
    for (std::size_t a : {std::size_t(0), std::size_t(1)}) {
      Vec3d grad(0.0, 0.0, 0.0);
      for (std::size_t j = 0; j < N; ++j) {
        if (j == a) continue;
        Vec3d del = kCluster[a] - kCluster[j];
        const double r = std::sqrt(del * del);
        fd_type FD(M);
        FD.set_zero();
        tw.z.calc_dXijdri_dXjidri(1, 1, r, r * r, del, rho[a], rho[j], FD, 1.0);
        for (std::size_t m = 0; m < M; ++m)
          for (int k = 0; k < 3; ++k) grad[k] += w(m) * FD(m, k);
      }
      for (int k = 0; k < 3; ++k) {
        std::vector<Vec3d> pp = kCluster, pm = kCluster;
        pp[a][k] += h;
        pm[a][k] -= h;
        const double num =
            (cluster_energy(tw.z, pp, w) - cluster_energy(tw.z, pm, w)) / (2 * h);
        INFO("atom " << a << " k " << k);
        REQUIRE(grad[k] == Approx(num).margin(1e-6));
      }
    }

    // E_FREE - E_ZERO = N F_raw(0), with F_raw(0) = raw_embedding_f0(w)
    const double Ez = cluster_energy(tw.z, kCluster, w);
    const double Ef = cluster_energy(tw.f, kCluster, w);
    const double f0 = tw.z.raw_embedding_f0(w).value();
    double S0 = 0.0, Sa = 0.0;
    for (std::size_t m = 0; m < M; ++m) S0 += std::fabs(d.w[m] * d.g(m, 0.0));
    for (std::size_t i = 0; i < N; ++i) {
      const double ri = rho[i](tw.z.get_rfidx());
      for (std::size_t m = 0; m < M; ++m) Sa += std::fabs(d.w[m] * d.g(m, ri));
    }
    REQUIRE(f0 != 0.0);
    REQUIRE(std::fabs((Ef - Ez) - N * f0) <= 4.0 * N * M * kEps * (Sa + N * S0));
    // forces are bitwise the same in both modes
    REQUIRE(same_bits(cluster_forces(tw.z, kCluster),
                      cluster_forces(tw.f, kCluster)));
  }
}
