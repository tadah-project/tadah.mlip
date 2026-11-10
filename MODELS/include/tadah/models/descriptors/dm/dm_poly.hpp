#ifndef DM_POLY_HPP
#define DM_POLY_HPP
#include <tadah/models/descriptors/dm/dm_func.h>
#include <tadah/models/descriptors/dm/dm_poly.h>
#include <tadah/models/descriptors/dm/dm_impl_utils.h>
#include <tadah/models/descriptors/warn_once.h>
#include <tadah/models/descriptors/dm/radial_policies.h>
#include <tadah/models/descriptors/density_reach.h>

#include <stdexcept>
#include <type_traits>

constexpr std::size_t DM_POLY_MAX_GRID = 64;
static_assert(DM_POLY_MAX_GRID >= 1, "DM_POLY_MAX_GRID must be > 0");

namespace {
using namespace tadah::models::detail;
} // namespace

namespace tadah {
namespace models {

template <typename ChannelFn, typename RadialBasis, typename AggMode>
DM_Poly<ChannelFn, RadialBasis, AggMode>::DM_Poly() { init(); }

template <typename ChannelFn, typename RadialBasis, typename AggMode>
DM_Poly<ChannelFn, RadialBasis, AggMode>::DM_Poly(const tadah::core::Context& ctx)
: DM_Base(ctx)
, rc(ctx.get<double>("RCUTMBMAX"))
{
  if (!ctx.get<bool>("INITMB")) return;
  init();

  get_grid(ctx, "CGRIDMB", cgrid);
  get_grid(ctx, "SGRIDMB", sgrid);
  D_Base::printGrid(cgrid, "CGRIDMB");
  D_Base::printGrid(sgrid, "SGRIDMB");

  if (cgrid.size() != sgrid.size())
    throw std::runtime_error("DM_Poly: CGRIDMB and SGRIDMB sizes differ.");

  emb = ChannelFn(ctx, cgrid.size(), sgrid.size());

  Lmax = ctx.get<int>("TYPEMB", 1);
  if (Lmax > 6)
    throw std::runtime_error("DM_Poly: Lmax > 6 is not supported (requested "
        + std::to_string(Lmax) + ").");

  /* OrbitalWise + F≠F_SQ + L>0 is not rotationally invariant; warned of
   * once per process (log_warning_once): tadah hpo builds the descriptors
   * of every evaluation. */
  if constexpr (std::is_same_v<AggMode, OrbitalWise>) {
    if (!ChannelFn::rotationally_invariant && Lmax > 0)
      log_warning_once(
        label() + " (OrbitalWise): channel not rotationally invariant for L>0",
        label() + " (OrbitalWise): the channel function is not F_SQ, so the "
        "descriptor is not rotationally invariant for L>0 (the PowerSpectrum "
        "mode is). Its stress is not the strain derivative of the energy (the "
        "virial is not symmetric).");
  }
  odim     = emb.output_dim();
  const std::size_t ng = cgrid.size();
  s        = odim * (Lmax + 1);
  const std::size_t norbs = gen_atomic_orbitals(Lmax);
  rhoisize  = norbs * ng;      // ρ storage
  rhoipsize = norbs * odim;    // dF/dρ storage
  set_rfidx(0);
  set_drfidx(rhoisize);

  init_for_atoms(get_init_atoms(ctx));

}

/* ================================================================== *
 *  calc_aed                                                           *
 *                                                                     *
 *  PowerSpectrum mode:                                                *
 *    ρ_L[c] = Σ_o fac · ρ[o,c]²   then  V = F(ρ_L[c])              *
 *    derivative: ∂V/∂ρ[o,c] = F'(ρ_L) · 2·fac · ρ[o,c]             *
 *                                                                     *
 *  OrbitalWise mode:                                                  *
 *    V[L,c] = Σ_o fac · F(ρ[o,c])                                   *
 *    derivative: ∂V/∂ρ[o,c] = fac · F'(ρ[o,c])                      *
 *                                                                     *
 *  rhoi layout (each half of size rhoisize):                          *
 *    [0, rhoisize)          — orbital densities ρ[L,o,c]             *
 *    [rhoisize, 2*rhoisize) — per-orbital derivatives (see above)     *
 * ================================================================== */
template <typename ChannelFn, typename RadialBasis, typename AggMode>
void DM_Poly<ChannelFn, RadialBasis, AggMode>::calc_aed(
    tadah::core::rho_type& rhoi,
    tadah::core::aed_type& aed) const
{
  const std::size_t ng = cgrid.size();
  std::size_t Lshift  = 0;
  std::size_t ii_base = rfidx;
  std::size_t dii_base = drfidx;

  for (int L = 0; L <= Lmax; ++L) {
    const std::size_t N_orb = orbitals[L].size();

    if constexpr (std::is_same_v<AggMode, PowerSpectrum>) {

      if constexpr (ChannelFn::expands_dimensions) {
        /* PowerSpectrum + dimension-expanding (ng==1):
         * ρ_L = Σ_o fac · ρ[o]²  (single ρ per orbital)
         * then V[p] = F(ρ_L, p) for each embedding point p */
        double rho_L = 0.0;
        for (std::size_t o = 0; o < N_orb; ++o) {
          const double rho_o = rhoi(ii_base + o);
          rho_L += fac[L][o] * rho_o * rho_o;
        }
        for (std::size_t p = 0; p < odim; ++p) {
          const double FF  = emb.calc_F (rho_L, p);
          const double dFF = emb.calc_dF(rho_L, p);
          aed(p + Lshift + fidx) += FF;
          for (std::size_t o = 0; o < N_orb; ++o) {
            const double rho_o = rhoi(ii_base + o);
            rhoi(dii_base + o*odim + p) = dFF * 2.0 * fac[L][o] * rho_o;
          }
        }
      } else {
        /* PowerSpectrum + standard (odim == ng) */
        for (std::size_t c = 0; c < ng; ++c) {
          double rho_L = 0.0;
          for (std::size_t o = 0; o < N_orb; ++o) {
            const double rho_oc = rhoi(ii_base + o*ng + c);
            rho_L += fac[L][o] * rho_oc * rho_oc;
          }
          const double FF  = emb.calc_F (rho_L, c);
          const double dFF = emb.calc_dF(rho_L, c);
          aed(c + Lshift + fidx) += FF;
          for (std::size_t o = 0; o < N_orb; ++o) {
            const double rho_oc = rhoi(ii_base + o*ng + c);
            rhoi(dii_base + o*ng + c) = dFF * 2.0 * fac[L][o] * rho_oc;
          }
        }
      }

    } else {
      /* ---- OrbitalWise ---- */

      if constexpr (ChannelFn::expands_dimensions) {
        /* OrbitalWise + dimension-expanding (ng==1):
         * V[p] = Σ_o fac · F(ρ[o], p) */
        std::size_t ii = ii_base;
        std::size_t dii = dii_base;
        for (std::size_t o = 0; o < N_orb; ++o, ++ii) {
          const double f   = fac[L][o];
          const double rho = rhoi(ii);
          for (std::size_t p = 0; p < odim; ++p, ++dii) {
            const double FF  = emb.calc_F (rho, p);
            const double dFF = emb.calc_dF(rho, p);
            aed(p + Lshift + fidx) += f * FF;
            rhoi(dii) = f * dFF;
          }
        }
      } else {
        /* OrbitalWise + standard (odim == ng) */
        std::size_t ii = ii_base;
        std::size_t dii = dii_base;
        for (std::size_t o = 0; o < N_orb; ++o) {
          const double f = fac[L][o];
          for (std::size_t c = 0; c < ng; ++c, ++ii, ++dii) {
            const double rho  = rhoi(ii);
            const double FF   = emb.calc_F (rho, c);
            const double dFF  = emb.calc_dF(rho, c);
            aed(c + Lshift + fidx) += f * FF;
            rhoi(dii) = f * dFF;
          }
        }
      }
    }

    Lshift  += odim;
    ii_base += N_orb * ng;
    dii_base += N_orb * (ChannelFn::expands_dimensions ? odim : ng);
  }
}

/* ================================================================== *
 *  calc_dXijdri_dXjidri — half-list, symmetric pair contribution     *
 *  (identical for both AggModes — derivative layout is the same)     *
 * ================================================================== */
template <typename ChannelFn, typename RadialBasis, typename AggMode>
void DM_Poly<ChannelFn, RadialBasis, AggMode>::calc_dXijdri_dXjidri(
    int Zi, int Zj, double rij, double,
    const tadah::core::Vec3d& v,
    tadah::core::rho_type& rhoi,
    tadah::core::rho_type& rhoj,
    tadah::core::fd_type& fd_ij,
    double scale) const
{
  if (!is_init_for_atoms(Zi, Zj) || rij > get_rcut()) return;

  const double x = v[0], y = v[1], z = v[2];
  const double w_pair = weights[Zi][Zj];

  const double fc  = fcut->calc       (rij);
  const double fcp = fcut->calc_prime (rij);

  const std::size_t ng = cgrid.size();
  StackOrHeapBuffer<DM_POLY_MAX_GRID> buf(ng);
  RadialBasis::eval(rij, fc, fcp, sgrid, cgrid, buf.g(), buf.dg(), ng);

  double px[7], py[7], pz[7], dpx[7], dpy[7], dpz[7];
  precompute_powers(x, Lmax, px, dpx);
  precompute_powers(y, Lmax, py, dpy);
  precompute_powers(z, Lmax, pz, dpz);

  const double   rinv   = 1.0/rij;
  std::size_t    Lshift = 0;
  std::size_t    ii     = drfidx;

  for (int L = 0; L <= Lmax; ++L) {
    const double s_L = (L & 1) ? 1.0 : -1.0;

    for (std::size_t o = 0; o < orbitals[L].size(); ++o) {
      const int lx = orbitals[L][o][0],
                ly = orbitals[L][o][1],
                lz = orbitals[L][o][2];

      const double pxij  = px[lx], pyij = py[ly], pzij = pz[lz];
      const double pqrij = pxij*pyij*pzij;
      const double txij  = dpx[lx]*pyij*pzij;
      const double tyij  = dpy[ly]*pxij*pzij;
      const double tzij  = dpz[lz]*pyij*pxij;

      if constexpr (ChannelFn::expands_dimensions) {
        const double g  = buf.g ()[0];
        const double dg = buf.dg()[0];
        const double Bx = g*txij + pqrij*dg*x*rinv;
        const double By = g*tyij + pqrij*dg*y*rinv;
        const double Bz = g*tzij + pqrij*dg*z*rinv;

        for (std::size_t p = 0; p < odim; ++p, ++ii) {
          const double coef = scale * w_pair *
              ( rhoi(ii) - s_L * rhoj(ii) );
          const std::size_t row = p + Lshift + fidx;
          fd_ij(row, 0) += coef * Bx;
          fd_ij(row, 1) += coef * By;
          fd_ij(row, 2) += coef * Bz;
        }
      } else {
        for (std::size_t c = 0; c < ng; ++c, ++ii) {
          const double g  = buf.g ()[c];
          const double dg = buf.dg()[c];

          const double Bx = g*txij + pqrij*dg*x*rinv;
          const double By = g*tyij + pqrij*dg*y*rinv;
          const double Bz = g*tzij + pqrij*dg*z*rinv;

          const double coef = scale * w_pair *
              ( rhoi(ii) - s_L * rhoj(ii) );

          const std::size_t row = c + Lshift + fidx;
          fd_ij(row, 0) += coef * Bx;
          fd_ij(row, 1) += coef * By;
          fd_ij(row, 2) += coef * Bz;
        }
      }
    }
    Lshift += odim;
  }
}

/* ================================================================== *
 *  calc_dXijdri — full-list, one-sided                               *
 * ================================================================== */
template <typename ChannelFn, typename RadialBasis, typename AggMode>
void DM_Poly<ChannelFn, RadialBasis, AggMode>::calc_dXijdri(
    int Zi, int Zj, double rij, double,
    const tadah::core::Vec3d& v,
    tadah::core::rho_type& rhoi,
    tadah::core::fd_type& fd_ij,
    double scale) const
{
  if (!is_init_for_atoms(Zi, Zj) || rij > get_rcut()) return;

  const double x = v[0], y = v[1], z = v[2];
  const double w_pair = weights[Zi][Zj];

  const double fc  = scale*w_pair * fcut->calc       (rij);
  const double fcp = scale*w_pair * fcut->calc_prime (rij);

  const double      rinv = 1.0/rij;
  const std::size_t ng   = cgrid.size();

  StackOrHeapBuffer<DM_POLY_MAX_GRID> buf(ng);
  RadialBasis::eval(rij, fc, fcp, sgrid, cgrid, buf.g(), buf.dg(), ng);

  double px[7], py[7], pz[7], dpx[7], dpy[7], dpz[7];
  precompute_powers(x, Lmax, px, dpx);
  precompute_powers(y, Lmax, py, dpy);
  precompute_powers(z, Lmax, pz, dpz);

  std::size_t Lshift = 0;
  std::size_t ii     = drfidx;

  for (int L = 0; L <= Lmax; ++L) {
    for (std::size_t o = 0; o < orbitals[L].size(); ++o) {
      const int lx = orbitals[L][o][0],
                ly = orbitals[L][o][1],
                lz = orbitals[L][o][2];

      const double pxij  = px[lx], pyij = py[ly], pzij = pz[lz];
      const double pqrij = pxij*pyij*pzij;
      const double txij  = dpx[lx]*pyij*pzij;
      const double tyij  = dpy[ly]*pxij*pzij;
      const double tzij  = dpz[lz]*pyij*pxij;

      if constexpr (ChannelFn::expands_dimensions) {
        const double g  = buf.g ()[0];
        const double dg = buf.dg()[0];
        const double termx = g*txij + dg*pqrij*rinv*x;
        const double termy = g*tyij + dg*pqrij*rinv*y;
        const double termz = g*tzij + dg*pqrij*rinv*z;

        for (std::size_t p = 0; p < odim; ++p, ++ii) {
          const double f_r = rhoi(ii);
          const std::size_t row = p + Lshift + fidx;
          fd_ij(row, 0) += f_r * termx;
          fd_ij(row, 1) += f_r * termy;
          fd_ij(row, 2) += f_r * termz;
        }
      } else {
        const double t2x = pqrij * rinv * x;
        const double t2y = pqrij * rinv * y;
        const double t2z = pqrij * rinv * z;

#pragma omp simd
        for (std::size_t c = 0; c < ng; ++c) {
          const double g  = buf.g ()[c];
          const double dg = buf.dg()[c];

          const double termx = g*txij + dg*t2x;
          const double termy = g*tyij + dg*t2y;
          const double termz = g*tzij + dg*t2z;

          const double f_r = rhoi(ii + c);

          const std::size_t row = c + Lshift + fidx;
          fd_ij(row, 0) += f_r * termx;
          fd_ij(row, 1) += f_r * termy;
          fd_ij(row, 2) += f_r * termz;
        }
        ii += ng;
      }
    }
    Lshift += odim;
  }
}

/* ================================================================== *
 *  calc_rho — density accumulation (same for both AggModes)          *
 * ================================================================== */
template <typename ChannelFn, typename RadialBasis, typename AggMode>
void DM_Poly<ChannelFn, RadialBasis, AggMode>::calc_rho(
    int Zi, int Zj, double rij, double,
    const tadah::core::Vec3d& v,
    tadah::core::rho_type& rhoi,
    double scale) const
{
  if (!is_init_for_atoms(Zi, Zj) || rij > get_rcut()) return;

  const double fc = scale * weights[Zi][Zj] * fcut->calc(rij);

  const std::size_t ng = cgrid.size();
  StackOrHeapBuffer<DM_POLY_MAX_GRID> buf(ng);
  RadialBasis::eval(rij, fc, sgrid, cgrid, buf.g(), ng);

  double px[7], py[7], pz[7];
  precompute_powers(v[0], Lmax, px);
  precompute_powers(v[1], Lmax, py);
  precompute_powers(v[2], Lmax, pz);

  std::size_t ii = rfidx;

  for (int L = 0; L <= Lmax; ++L) {
    for (std::size_t o = 0; o < orbitals[L].size(); ++o) {
      const int lx = orbitals[L][o][0],
                ly = orbitals[L][o][1],
                lz = orbitals[L][o][2];

      const double poly = px[lx]*py[ly]*pz[lz];

      for (std::size_t c = 0; c < ng; ++c)
        rhoi(ii++) += buf.g()[c] * poly;
    }
  }
}

/* ================================================================== *
 *  Utilities                                                          *
 * ================================================================== */
template <typename ChannelFn, typename RadialBasis, typename AggMode>
void DM_Poly<ChannelFn, RadialBasis, AggMode>::init_rhoi(tadah::core::rho_type& rhoi)
{
  rhoi.resize(rhoisize+rhoipsize);
  rhoi.set_zero();
}

template <typename ChannelFn, typename RadialBasis, typename AggMode>
std::string DM_Poly<ChannelFn, RadialBasis, AggMode>::label() const { return lab; }

template <typename ChannelFn, typename RadialBasis, typename AggMode>
void DM_Poly<ChannelFn, RadialBasis, AggMode>::init()
{
  keys = {"CGRIDMB", "SGRIDMB"};
  context_keys = ChannelFn::context_keys();
  context_keys_required =
      !context_keys.empty() && !ChannelFn::context_keys_optional;
  nparams = 2 + 1;
}

template <typename ChannelFn, typename RadialBasis, typename AggMode>
size_t DM_Poly<ChannelFn, RadialBasis, AggMode>::gen_atomic_orbitals(int Lm)
{
  orbitals.assign(Lm + 1, {});   // clear inner vectors so re-calls don't accumulate
  fac     .assign(Lm + 1, {});

  size_t cnt = 0;
  for (int L = 0; L <= Lm; ++L) {
    for (int lx = 0; lx <= L; ++lx) {
      for (int ly = 0; ly <= L-lx; ++ly) {
        const int lz = L - lx - ly;
        orbitals[L].push_back({lx, ly, lz});
        const double f = detail::fact_cached(L) /
            (detail::fact_cached(lx) * detail::fact_cached(ly) * detail::fact_cached(lz)) /
            detail::pow_i06(4*rc, L);
        fac[L].push_back(f);
        ++cnt;
      }
    }
  }
  return cnt;
}

/* ================================================================== *
 *  meta() — DM_Poly<F, R, M> shares the same parameter shape as
 *  DM_mEAD<F>: RCUTMB scalar, CGRIDMB / SGRIDMB paired vectors. The
 *  per-channel `F` may add CEMBFUNC/SEMBFUNC; those are picked up by
 *  the audit only when the seed declares them.
 * ================================================================== */
template <typename ChannelFn, typename RadialBasis, typename AggMode>
std::optional<DescriptorMeta>
DM_Poly<ChannelFn, RadialBasis, AggMode>::meta() const {
  using DataStats = tadah::core::stats::DataStats;

  DescriptorMeta dm;
  dm.label = label();   // runtime descriptor label

  ParamMeta p_rcut;
  p_rcut.key      = "RCUTMB";
  p_rcut.arg_pos  = -1;
  p_rcut.scale    = Scale::Distance;
  p_rcut.units    = "Å";
  p_rcut.role     = "Real-space cutoff for the MB neighbour sum.";
  p_rcut.hard.lo  = 0.0;
  p_rcut.crit     = Criticality::Critical;
  p_rcut.scope    = PairScope::Global;
  p_rcut.suggest = cutoff_suggestion;
  p_rcut.reach_each_pair = true;
  p_rcut.check = [](const DataStats& d, const SeedView& s) {
    return cutoff_reach_check(d, s, "RCUTMB");
  };
  dm.params.push_back(std::move(p_rcut));

  ParamMeta p_cgrid;
  p_cgrid.key      = "CGRIDMB";
  p_cgrid.arg_pos  = 0;
  p_cgrid.scale    = Scale::Distance;
  p_cgrid.units    = "Å";
  p_cgrid.role     = RadialBasis::kind == BasisKind::Gaussian
      ? "Centre of the radial GTO exp(-SGRIDMB (r - CGRIDMB)^2). A GTO below "
        "1e-6 at every neighbour distance collapses ρ."
      : "Centre of the radial blip b(SGRIDMB (r - CGRIDMB)). A blip whose "
        "support holds no neighbour distance collapses ρ.";
  p_cgrid.hard.lo  = 0.0;
  p_cgrid.crit     = Criticality::Critical;
  p_cgrid.scope    = PairScope::PerElementPair;
  // The suggestions read the seed's centre (density_reach.h).
  if constexpr (RadialBasis::kind == BasisKind::Gaussian)
    p_cgrid.suggest = gaussian_centre_suggestion;
  else
    p_cgrid.suggest = blip_centre_suggestion;
  p_cgrid.check    = [](const DataStats& d, const SeedView& s) {
    return radial_reach_check(d, s, RadialBasis::kind);
  };
  dm.params.push_back(std::move(p_cgrid));

  // The width of the radial basis: eta of exp(-eta (r - c)^2) (Å^-2) for
  // the Gaussian, eta of b(eta (r - c)) (Å^-1, support |r - c| < 2/eta)
  // for the blip.
  constexpr bool gaussian = RadialBasis::kind == BasisKind::Gaussian;
  ParamMeta p_sgrid;
  p_sgrid.key      = "SGRIDMB";
  p_sgrid.arg_pos  = 0;
  p_sgrid.scale    = gaussian ? Scale::InverseDistanceSq : Scale::InverseDistance;
  p_sgrid.units    = gaussian ? "Å^-2" : "Å^-1";
  p_sgrid.role     = gaussian
      ? "Inverse Gaussian width η of exp(-η (r - CGRIDMB)^2)."
      : "Inverse width η of the blip b(η (r - CGRIDMB)), whose support is "
        "|r - CGRIDMB| < 2/η.";
  p_sgrid.hard.lo  = kHardLoPositive;
  p_sgrid.crit     = Criticality::Critical;
  p_sgrid.scope    = PairScope::Global;
  if constexpr (gaussian)
    p_sgrid.suggest = gaussian_width_suggestion;
  else
    p_sgrid.suggest = blip_width_suggestion;
  dm.params.push_back(std::move(p_sgrid));

  dm.cross_param_checks.push_back(
    [](const DataStats&, const SeedView& s) -> HealthVerdict {
      if (!s.contains("CGRIDMB") || !s.contains("SGRIDMB")) return HealthOk{};
      if (s.size("CGRIDMB") != s.size("SGRIDMB"))
        return HealthFail{"CGRIDMB and SGRIDMB sizes differ."};
      return HealthOk{};
    });

  dm.expected_columns = [](const SeedView& s) -> std::size_t {
    if (!s.contains("CGRIDMB")) return 0;
    return s.size("CGRIDMB");
  };

  return dm;
}

} // namespace models
} // namespace tadah

#endif
