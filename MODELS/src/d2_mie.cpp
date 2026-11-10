#include "tadah/models/descriptors/d2/d2_base.h"
#include <tadah/models/descriptors/d2/d2_mie.h>
#include <tadah/models/descriptors/param_meta.h>
#include <tadah/core/stats/data_stats.h>
#include <tadah/core/number_format.h>
#include <cmath>
#include <tadah/models/descriptors/density_reach.h>
namespace tadah {
namespace models {


D2_MIE::D2_MIE() {
  init();
}
D2_MIE::D2_MIE(const tadah::core::Context &c): D2_Base(c)
{
  if (!c.get<bool>("INIT2B")) return;
  init();
  s=2;
  n = c.get<double>("TYPE2B",1);
  m = c.get<double>("TYPE2B",2);
  if (n<0 || m<0) {
    throw std::runtime_error("Both Mie exponents must by positive\n");
  }
  auto init_atoms = get_init_atoms(c);
  init_for_atoms(init_atoms);
}

void D2_MIE::calc_aed(
  const int Zi,
  const int Zj,
  const double rij,
  const double,
  tadah::core::aed_type &aed,
  const double scale) const
{
  if (!is_init_for_atoms(Zi,Zj) || rij > get_rcut()) return;
  double rn_inv = 1.0/pow(rij,n);
  double rm_inv = 1.0/pow(rij,m);
  double fc_ij = fcut->calc(rij);

  aed(fidx) -= scale*weights[Zi][Zj]*rn_inv*fc_ij;
  aed(fidx+1) += scale*weights[Zi][Zj]*rm_inv*fc_ij;
}
void D2_MIE::calc_dXijdri(
  const int Zi,
  const int Zj,
  const double rij,
  const double,
  tadah::core::fd_type &fd_ij,
  const double scale) const
{
  if (!is_init_for_atoms(Zi,Zj) || rij > get_rcut()) return;
  double fc_ij = fcut->calc(rij);
  double fcp_ij = fcut->calc_prime(rij);

  fd_ij(fidx,0)   = scale*weights[Zi][Zj]*(n/pow(rij,n+1)*fc_ij - fcp_ij/pow(rij,n));
  fd_ij(fidx+1,0) = scale*weights[Zi][Zj]*(-m/pow(rij,m+1)*fc_ij + fcp_ij/pow(rij,m));
}
void D2_MIE::calc_all(
  const int Zi,
  const int Zj,
  const double rij,
  const double,
  tadah::core::aed_type &aed,
  tadah::core::fd_type &fd_ij,
  const double scale) const
{
  if (!is_init_for_atoms(Zi,Zj) || rij > get_rcut()) return;

  double rn_inv = 1.0/pow(rij,n);
  double rm_inv = 1.0/pow(rij,m);

  double fc_ij = fcut->calc(rij);
  double fcp_ij = fcut->calc_prime(rij);

  aed(fidx) -= scale*weights[Zi][Zj]*rn_inv*fc_ij;
  aed(fidx+1) += scale*weights[Zi][Zj]*rm_inv*fc_ij;

  fd_ij(fidx,0) = scale*weights[Zi][Zj]*(n*rn_inv/rij*fc_ij - fcp_ij*rn_inv);
  fd_ij(fidx+1,0) = scale*weights[Zi][Zj]*(-m*rm_inv/rij*fc_ij + fcp_ij*rm_inv);

}
std::string D2_MIE::label() const {
  return lab;
}
void D2_MIE::init() {
  nparams=2;
}
bool D2_MIE::calc_dDdtheta(
  const std::string& theta_name,
  int                arg_pos,
  const int          Zi,
  const int          Zj,
  const double       rij,
  const double       /*rij_sq*/,
  tadah::core::aed_type& dD_buf,
  const double       scale) const
{
  if (!is_init_for_atoms(Zi, Zj) || rij > get_rcut()) return false;

  if (theta_name == "RCUT2B") {
    const double dfc = fcut->calc_drcut(rij);
    if (dfc == 0.0) return false;  // discontinuous cutoff; caller FD-falls-back
    const double rn_inv = 1.0 / std::pow(rij, n);
    const double rm_inv = 1.0 / std::pow(rij, m);
    dD_buf(fidx)     -= scale * weights[Zi][Zj] * rn_inv * dfc;
    dD_buf(fidx + 1) += scale * weights[Zi][Zj] * rm_inv * dfc;
    return true;
  }

  if (theta_name == "TYPE2B") {
    if (arg_pos != 2 && arg_pos != 3) return false;

    const double fc_ij = fcut->calc(rij);
    const double ln_r  = std::log(rij);

    if (arg_pos == 2) {
      // d(-r^{-n})/dn = +ln(r) r^{-n}
      const double rn_inv = 1.0 / std::pow(rij, n);
      dD_buf(fidx) += scale * weights[Zi][Zj] * ln_r * rn_inv * fc_ij;
    } else {
      // d(+r^{-m})/dm = -ln(r) r^{-m}
      const double rm_inv = 1.0 / std::pow(rij, m);
      dD_buf(fidx + 1) -= scale * weights[Zi][Zj] * ln_r * rm_inv * fc_ij;
    }
    return true;
  }

  if (theta_name == "WATOM") {
    // d(aed)/d θ_WATOM = chain × (unweighted Mie energy contribution).
    if (!has_watom_arg_pos(arg_pos)) return false;
    const double chain = dweight_dw_n(Zi, Zj, arg_pos);
    if (chain != 0.0) {
      const double fc_ij  = fcut->calc(rij);
      const double rn_inv = 1.0 / std::pow(rij, n);
      const double rm_inv = 1.0 / std::pow(rij, m);
      dD_buf(fidx)     -= scale * chain * rn_inv * fc_ij;
      dD_buf(fidx + 1) += scale * chain * rm_inv * fc_ij;
    }
    return true;
  }

  return false;  // not owned by this descriptor
}

// Closed-form ∂(fd_radial)/∂ theta for D2_MIE. See d2_mie.h doc-comment.
bool D2_MIE::calc_dXijdri_dtheta(
  const std::string& theta_name,
  int                arg_pos,
  const int          Zi,
  const int          Zj,
  const double       rij,
  const double       /*rij_sq*/,
  tadah::core::fd_type& dfd_ij,
  const double       scale) const
{
  if (!is_init_for_atoms(Zi, Zj) || rij > get_rcut()) return false;

  const double fc_ij  = fcut->calc(rij);
  const double fcp_ij = fcut->calc_prime(rij);

  if (theta_name == "RCUT2B") {
    const double dfc  = fcut->calc_drcut(rij);
    const double dfcp = fcut->calc_prime_drcut(rij);
    if (dfc == 0.0 && dfcp == 0.0) return false;   // Cut_Dummy fallback
    const double rn_inv  = 1.0 / std::pow(rij, n);
    const double rm_inv  = 1.0 / std::pow(rij, m);
    const double rn1_inv = rn_inv / rij;   // r^{-n-1}
    const double rm1_inv = rm_inv / rij;
    dfd_ij(fidx,     0) += scale * weights[Zi][Zj] * ( n * rn1_inv * dfc - dfcp * rn_inv);
    dfd_ij(fidx + 1, 0) += scale * weights[Zi][Zj] * (-m * rm1_inv * dfc + dfcp * rm_inv);
    return true;
  }

  if (theta_name == "TYPE2B") {
    if (arg_pos != 2 && arg_pos != 3) return false;

    const double ln_r = std::log(rij);

    if (arg_pos == 2) {
      // d/dn — touches only fidx (repulsive column).
      const double rn_inv  = 1.0 / std::pow(rij, n);
      const double rn1_inv = rn_inv / rij;
      const double val = rn1_inv * fc_ij * (1.0 - n * ln_r) + fcp_ij * ln_r * rn_inv;
      dfd_ij(fidx, 0) += scale * weights[Zi][Zj] * val;
    } else {
      // d/dm — touches only fidx+1 (attractive column).
      const double rm_inv  = 1.0 / std::pow(rij, m);
      const double rm1_inv = rm_inv / rij;
      const double val = rm1_inv * fc_ij * (1.0 - m * ln_r) + fcp_ij * ln_r * rm_inv;
      dfd_ij(fidx + 1, 0) -= scale * weights[Zi][Zj] * val;
    }
    return true;
  }

  if (theta_name == "WATOM") {
    // Force-side d(fd)/d θ_WATOM = chain × (unweighted Mie force-radial
    // contribution).
    if (!has_watom_arg_pos(arg_pos)) return false;
    const double chain = dweight_dw_n(Zi, Zj, arg_pos);
    if (chain != 0.0) {
      const double rn_inv  = 1.0 / std::pow(rij, n);
      const double rm_inv  = 1.0 / std::pow(rij, m);
      dfd_ij(fidx,     0) += scale * chain * ( n * rn_inv / rij * fc_ij - fcp_ij * rn_inv);
      dfd_ij(fidx + 1, 0) += scale * chain * (-m * rm_inv / rij * fc_ij + fcp_ij * rm_inv);
    }
    return true;
  }

  return false;
}

// ----------------------------------------------------------------
// meta() — D2_MIE exposes RCUT2B (Critical, shared with cutoff family)
// plus the two TYPE2B-positional Mie exponents n (config index 1) and
// m (config index 2). For an LJ-like setup these would be 12 and 6;
// the descriptor itself enforces only positivity (see ctor throw).
//
// Their ParamMeta::arg_pos is the position of the value on the
// descriptor's TYPE2B line, from 1, the name D2_MIE being 1: n is 2 and m
// is 3, the arg_pos calc_dDdtheta takes for them. They have no bound
// suggester: tadah hpo searches no value of a TYPE2B line (it refuses
// OPTIM TYPE2B), and tadah analysis suggest_bounds gives each a row
// without an OPTIM line.
// ----------------------------------------------------------------
std::optional<DescriptorMeta> D2_MIE::meta() const {
  using DataStats = tadah::core::stats::DataStats;

  DescriptorMeta dm;
  dm.label = "D2_MIE";

  ParamMeta p_rcut;
  p_rcut.key      = "RCUT2B";
  p_rcut.arg_pos  = -1;
  p_rcut.scale    = Scale::Distance;
  p_rcut.units    = "Å";
  p_rcut.role     = "Outer cutoff for the Mie pair sum.";
  p_rcut.hard.lo  = 0.0;
  p_rcut.crit     = Criticality::Critical;
  p_rcut.scope    = PairScope::Global;
  p_rcut.suggest = cutoff_suggestion;
  p_rcut.reach_each_pair = true;
  p_rcut.check = [](const DataStats& d, const SeedView& s) {
    return cutoff_reach_check(d, s, "RCUT2B");
  };
  dm.params.push_back(std::move(p_rcut));

  // n — repulsive exponent (default LJ-like 12; constructor throws on n<0).
  ParamMeta p_n;
  p_n.key     = "TYPE2B";
  p_n.arg_pos = 2;
  p_n.scale   = Scale::Dimensionless;
  p_n.units   = "";
  p_n.role    = "Mie repulsive exponent n (LJ-like default 12).";
  p_n.hard.lo = kHardLoPositive;     // strictly positive (constructor throws on <0)
  p_n.crit    = Criticality::Recommended;
  p_n.scope   = PairScope::Global;
  dm.params.push_back(std::move(p_n));

  // m — attractive exponent (default LJ-like 6; must satisfy m < n
  // mathematically for the Mie C prefactor to be finite, but the
  // descriptor itself does not enforce that — left to the user).
  ParamMeta p_m;
  p_m.key     = "TYPE2B";
  p_m.arg_pos = 3;
  p_m.scale   = Scale::Dimensionless;
  p_m.units   = "";
  p_m.role    = "Mie attractive exponent m (LJ-like default 6).";
  p_m.hard.lo = kHardLoPositive;
  p_m.crit    = Criticality::Recommended;
  p_m.scope   = PairScope::Global;
  dm.params.push_back(std::move(p_m));

  // Cross-param sanity: if both n and m are seeded, n > m keeps the
  // Mie prefactor C = (n/(n-m)) (n/m)^(m/(n-m)) finite and positive.
  dm.cross_param_checks.push_back(
    [](const DataStats&, const SeedView& s) -> HealthVerdict {
      if (!s.contains("TYPE2B")) return HealthOk{};
      if (s.size("TYPE2B") < 3) return HealthOk{};
      const double n = s.get<double>("TYPE2B", 1);
      const double m = s.get<double>("TYPE2B", 2);
      if (n == -1 || m == -1) return HealthOk{};
      if (n <= m)
        return HealthFail{"D2_MIE needs n > m for a finite prefactor (got "
                          "n = " + tadah::core::format_roundtrip(n) +
                          ", m = " + tadah::core::format_roundtrip(m) + ")."};
      return HealthOk{};
    });

  dm.expected_columns = [](const SeedView&) -> std::size_t { return 2; };
  return dm;
}

} // namespace models
} // namespace tadah

