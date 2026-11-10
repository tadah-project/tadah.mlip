#include "tadah/models/descriptors/d_base.h"
#include <tadah/models/descriptors/d2/d2_blip.h>
#include <tadah/models/descriptors/d_basis_functions.h>
#include <cstdlib>
#include <stdexcept>
#include <tadah/models/descriptors/density_reach.h>
namespace tadah {
namespace models {


D2_Blip::D2_Blip() {
  init();
}
D2_Blip::D2_Blip(const tadah::core::Context &c): D2_Base(c)
{
  if (!c.get<bool>("INIT2B")) return;
  init();

  get_grid(c,"CGRID2B",cgrid);
  get_grid(c,"SGRID2B",sgrid);
  D_Base::printGrid(cgrid,"CGRID2B");
  D_Base::printGrid(sgrid,"SGRID2B");

  if (cgrid.size()!=sgrid.size()) {
    throw std::runtime_error("SGRID2B and CGRID2B arrays differ in size.\n");
  }

  for (const auto &e:sgrid) {
    if(std::abs(e)<std::numeric_limits<double>::min())
      throw std::runtime_error("At least one of SGRID2B values is zero.\n");
  }

  s=cgrid.size();

  auto init_atoms = get_init_atoms(c);
  init_for_atoms(init_atoms);
  warn_if_bump_support_beyond_rcut("D2_Blip", "CGRID2B", "RCUT2B", cgrid,
                                   sgrid, BasisKind::Blip);
}

void D2_Blip::calc_aed(
  const int Zi,
  const int Zj,
  const double rij,
  const double,
  tadah::core::aed_type &aed,
  const double scale) const
{
  if (!is_init_for_atoms(Zi,Zj) || rij > get_rcut()) return;
  size_t i=fidx;
  double fc_ij = fcut->calc(rij);
  for (size_t c=0; c<cgrid.size(); c++) {
    double t = B(sgrid[c]*(rij-cgrid[c]),fc_ij);
    aed(i++) += scale*weights[Zi][Zj]*t;
  }

}
void D2_Blip::calc_dXijdri(
  const int Zi,
  const int Zj,
  const double rij,
  const double ,
  tadah::core::fd_type &fd_ij,
  const double scale) const
{
  if (!is_init_for_atoms(Zi,Zj) || rij > get_rcut()) return;
  size_t i=fidx;
  double fc_ij = fcut->calc(rij);
  double fcp_ij = fcut->calc_prime(rij);
  for (size_t c=0; c<cgrid.size(); c++) {
    fd_ij(i++,0) = scale*weights[Zi][Zj]*dB(sgrid[c]*(rij-cgrid[c]),sgrid[c],fc_ij,fcp_ij);
  }
}
void D2_Blip::calc_all(
  const int Zi,
  const int Zj,
  const double rij,
  const double ,
  tadah::core::aed_type &aed,
  tadah::core::fd_type &fd_ij,
  const double scale) const
{
  if (!is_init_for_atoms(Zi,Zj) || rij > get_rcut()) return;

  size_t i=fidx;
  double fc_ij = fcut->calc(rij);
  double fcp_ij = fcut->calc_prime(rij);
  for (size_t c=0; c<cgrid.size(); c++) {
    aed(i) += scale*weights[Zi][Zj]*B(sgrid[c]*(rij-cgrid[c]),fc_ij);
    fd_ij(i,0) = scale*weights[Zi][Zj]*dB(sgrid[c]*(rij-cgrid[c]),sgrid[c],fc_ij,fcp_ij);
    ++i;
  }
}

// Closed-form gradient of D2_Blip w.r.t. its hyperparameters.
// See class doc-comment in d2_blip.h for the math. Mirrors calc_aed's
// loop shape so a parity test against central-difference of calc_aed
// works component-wise without re-deriving the cutoff or grid bookkeeping.
//
// dB(x_c, 1.0) returns the bare dB/dx_c (eta factors out as a multiplier
// in the existing helper, see d_basis_functions.h).
bool D2_Blip::calc_dDdtheta(
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

  if (theta_name == "CGRID2B") {
    if (arg_pos < 1 || static_cast<std::size_t>(arg_pos) > cgrid.size()) return false;
    const std::size_t k   = static_cast<std::size_t>(arg_pos - 1);
    const double      mu  = cgrid[k];
    const double      eta = sgrid[k];
    const double      xc  = eta * (rij - mu);
    const double      fc  = fcut->calc(rij);
    const double      dB_dx = dB(xc, 1.0);   // bare dB/dx_c
    // d(x_c)/d(mu) = -eta
    const double      dD_dmu = -eta * dB_dx * fc;
    dD_buf(fidx + k) += scale * weights[Zi][Zj] * dD_dmu;
    return true;
  }

  if (theta_name == "SGRID2B") {
    if (arg_pos < 1 || static_cast<std::size_t>(arg_pos) > sgrid.size()) return false;
    const std::size_t k   = static_cast<std::size_t>(arg_pos - 1);
    const double      mu  = cgrid[k];
    const double      eta = sgrid[k];
    const double      xc  = eta * (rij - mu);
    const double      fc  = fcut->calc(rij);
    const double      dB_dx = dB(xc, 1.0);
    // d(x_c)/d(eta) = (r - mu)
    const double      dD_deta = (rij - mu) * dB_dx * fc;
    dD_buf(fidx + k) += scale * weights[Zi][Zj] * dD_deta;
    return true;
  }

  if (theta_name == "RCUT2B") {
    // RCUT2B enters only through f_c(r). dB/d rcut = 0 because B has no
    // direct rcut dependency in x_c = eta (r - mu).
    const double dfc = fcut->calc_drcut(rij);
    if (dfc == 0.0) return false;  // discontinuous cutoff (Cut_Dummy);
                                   // caller falls back to FD under HYBRID.
    for (std::size_t c = 0; c < cgrid.size(); ++c) {
      const double mu  = cgrid[c];
      const double eta = sgrid[c];
      const double xc  = eta * (rij - mu);
      const double b_val = B(xc);
      dD_buf(fidx + c) += scale * weights[Zi][Zj] * b_val * dfc;
    }
    return true;
  }

  if (theta_name == "WATOM") {
    // d(aed(fidx+c))/d θ_WATOM = chain × B(x_c) * fc(r), where chain
    // applies the per-species geometric-mean rule or the direct
    // pair-triple mapping.
    if (!has_watom_arg_pos(arg_pos)) return false;
    const double chain = dweight_dw_n(Zi, Zj, arg_pos);
    if (chain != 0.0) {
      const double fc_ij = fcut->calc(rij);
      for (std::size_t c = 0; c < cgrid.size(); ++c) {
        const double xc = sgrid[c] * (rij - cgrid[c]);
        dD_buf(fidx + c) += scale * chain * B(xc, fc_ij);
      }
    }
    return true;
  }

  return false;  // theta_name not owned by this descriptor.
}

namespace {
// Bare second derivative b''(x_c) of the blip basis function. Piecewise
// linear and CONTINUOUS (B is C^2 globally — it is the cubic B-spline;
// only B''' jumps, at the knots x_c = 0, +-1, +-2). Used only by
// D2_Blip::calc_dXijdri_dtheta; not in any public header to keep blip
// math localised here.
inline double d2B_bare(double x_c) {
  if (-2.0 < x_c && x_c <= -1.0) return 1.5 * (2.0 + x_c);
  if (-1.0 < x_c && x_c <= 1.0)  return -3.0 + 4.5 * std::fabs(x_c);
  if (1.0 < x_c && x_c < 2.0)    return 1.5 * (2.0 - x_c);
  return 0.0;
}
}  // namespace

// Closed-form mixed derivative d(fd_ij_radial)/d theta for D2_Blip.
// Math (with x_c = eta_c (r - mu_c), b = B(x_c), b1 = b'(x_c),
// b2 = b''(x_c), fc = f_c(r), fcp = f_c'(r)):
//   fd_radial = w(Zj) * (fc * eta * b1 + fcp * b)
//   d/d mu  = w(Zj) * (-eta) * (fc * eta * b2 + fcp * b1)
//   d/d eta = w(Zj) * [fc * b1 + (r-mu) * (fc * eta * b2 + fcp * b1)]
//   d/d rcut= w(Zj) * (dfc/drcut * eta * b1 + dfcp/drcut * b)
bool D2_Blip::calc_dXijdri_dtheta(
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

  const double fc  = fcut->calc(rij);
  const double fcp = fcut->calc_prime(rij);

  if (theta_name == "CGRID2B") {
    if (arg_pos < 1 || static_cast<std::size_t>(arg_pos) > cgrid.size()) return false;
    const std::size_t k   = static_cast<std::size_t>(arg_pos - 1);
    const double      mu  = cgrid[k];
    const double      eta = sgrid[k];
    const double      xc  = eta * (rij - mu);
    const double      b1  = dB(xc, 1.0);   // bare b'(x_c)
    const double      b2  = d2B_bare(xc);  // bare b''(x_c)
    // d/d mu of (fc eta b1 + fcp b) = -eta * (fc eta b2 + fcp b1)
    const double      val = -eta * (fc * eta * b2 + fcp * b1);
    dfd_ij(fidx + k, 0) += scale * weights[Zi][Zj] * val;
    return true;
  }

  if (theta_name == "SGRID2B") {
    if (arg_pos < 1 || static_cast<std::size_t>(arg_pos) > sgrid.size()) return false;
    const std::size_t k   = static_cast<std::size_t>(arg_pos - 1);
    const double      mu  = cgrid[k];
    const double      eta = sgrid[k];
    const double      xc  = eta * (rij - mu);
    const double      b1  = dB(xc, 1.0);
    const double      b2  = d2B_bare(xc);
    // d/d eta of (fc eta b1 + fcp b) = fc b1 + (r-mu) (fc eta b2 + fcp b1)
    const double      val = fc * b1 + (rij - mu) * (fc * eta * b2 + fcp * b1);
    dfd_ij(fidx + k, 0) += scale * weights[Zi][Zj] * val;
    return true;
  }

  if (theta_name == "RCUT2B") {
    const double dfc  = fcut->calc_drcut(rij);
    const double dfcp = fcut->calc_prime_drcut(rij);
    if (dfc == 0.0 && dfcp == 0.0) return false;   // Cut_Dummy fallback
    for (std::size_t c = 0; c < cgrid.size(); ++c) {
      const double mu  = cgrid[c];
      const double eta = sgrid[c];
      const double xc  = eta * (rij - mu);
      const double b   = B(xc);
      const double b1  = dB(xc, 1.0);
      const double val = dfc * eta * b1 + dfcp * b;
      dfd_ij(fidx + c, 0) += scale * weights[Zi][Zj] * val;
    }
    return true;
  }

  if (theta_name == "WATOM") {
    // d(fd_ij(fidx+c, 0))/d θ_WATOM = chain × dB-with-fc' (unweighted
    // per-pair force-radial contribution scaled by the WATOM chain
    // factor for the current mode).
    if (!has_watom_arg_pos(arg_pos)) return false;
    const double chain = dweight_dw_n(Zi, Zj, arg_pos);
    if (chain != 0.0) {
      for (std::size_t c = 0; c < cgrid.size(); ++c) {
        const double xc = sgrid[c] * (rij - cgrid[c]);
        dfd_ij(fidx + c, 0) +=
          scale * chain * dB(xc, sgrid[c], fc, fcp);
      }
    }
    return true;
  }

  return false;
}

std::string D2_Blip::label() const {
  return lab;
}
void D2_Blip::init() {
  keys.push_back("CGRID2B");
  keys.push_back("SGRID2B");
  nparams=2;
}
std::optional<DescriptorMeta> D2_Blip::meta() const {
  using DataStats = tadah::core::stats::DataStats;
  static constexpr std::size_t I_P10 = tadah::core::stats::IDX_P10;
  static constexpr std::size_t I_P90 = tadah::core::stats::IDX_P90;
  static constexpr std::size_t I_P99 = tadah::core::stats::IDX_P99;

  DescriptorMeta dm;
  dm.label = "D2_Blip";

  // RCUT2B — outer cutoff in r for the 2-body design matrix.
  ParamMeta p_rcut;
  p_rcut.key      = "RCUT2B";
  p_rcut.arg_pos  = -1;
  p_rcut.scale    = Scale::Distance;
  p_rcut.units    = "Å";
  p_rcut.role     = "Outer cutoff for the 2-body neighbour sum.";
  p_rcut.hard.lo  = 0.0;            // r >= 0
  p_rcut.crit     = Criticality::Critical;
  p_rcut.scope    = PairScope::Global;
  p_rcut.suggest = cutoff_suggestion;
  p_rcut.reach_each_pair = true;
  p_rcut.check = [](const DataStats& d, const SeedView& s) {
    return cutoff_reach_check(d, s, "RCUT2B");
  };
  dm.params.push_back(std::move(p_rcut));

  // CGRID2B — vector: centre of the i-th radial blip.
  ParamMeta p_cgrid;
  p_cgrid.key      = "CGRID2B";
  p_cgrid.arg_pos  = 0;
  p_cgrid.scale    = Scale::Distance;
  p_cgrid.units    = "Å";
  p_cgrid.role     = "Centre of the i-th radial blip, CGRID2B (i).";
  p_cgrid.hard.lo  = 0.0;
  p_cgrid.crit     = Criticality::Recommended;
  p_cgrid.scope    = PairScope::PerElementPair;
  p_cgrid.suggest  = [](const DataStats& d, const SeedView& s, int) -> BoundSuggestion {
    if (d.per_pair.empty()) return no_pair_suggestion(s);
    const auto& pp = d.per_pair[0];
    // Spread CGRID2B[i] uniformly between 0 and the data's r_ij p99.
    return std::make_pair(0.0, pp.r_percentiles[I_P99]);
  };
  dm.params.push_back(std::move(p_cgrid));

  // SGRID2B — vector: inverse width eta of the i-th blip, b(eta (r - c)).
  ParamMeta p_sgrid;
  p_sgrid.key      = "SGRID2B";
  p_sgrid.arg_pos  = 0;
  p_sgrid.scale    = Scale::InverseDistance;
  p_sgrid.units    = "Å^-1";
  p_sgrid.role     = "Inverse width eta of the i-th radial blip b(eta (r - c)), "
                     "SGRID2B (i), whose support is |r - c| < 2/eta.";
  // hard: SGRID2B = 0 makes the existing init() throw.
  p_sgrid.hard.lo  = kHardLoPositive;
  p_sgrid.crit     = Criticality::Recommended;
  p_sgrid.scope    = PairScope::Global;
  p_sgrid.suggest  = [](const DataStats& d, const SeedView& s,
                         int) -> BoundSuggestion {
    if (d.per_pair.empty()) return no_pair_suggestion(s);
    const auto& pp = d.per_pair[0];
    const double spread = pp.r_percentiles[I_P90] - pp.r_percentiles[I_P10];
    if (spread <= 0.0)
      return BoundSuggestion::fixed_range(
          0.1, 1.0, "fixed range: the neighbour distances have no spread "
                    "to take a width of the bump from");
    // eta in [1 / spread, 10 / spread] (inverse distance): the blip's
    // support half-width 2/eta runs from 2 spread (broad enough to cover
    // r_ij p10..p90) down to spread/5 (narrow).
    return std::make_pair(1.0 / spread, 10.0 / spread);
  };
  dm.params.push_back(std::move(p_sgrid));

  // Cross-param: CGRID and SGRID must have matching sizes (mirrors the
  // existing init() runtime check at d2_blip.cpp:24).
  dm.cross_param_checks.push_back(
    [](const DataStats&, const SeedView& s) -> HealthVerdict {
      if (!s.contains("CGRID2B") || !s.contains("SGRID2B")) return HealthOk{};
      if (s.size("CGRID2B") != s.size("SGRID2B"))
        return HealthFail{"CGRID2B and SGRID2B sizes differ."};
      return HealthOk{};
    });

  dm.expected_columns = [](const SeedView& s) -> std::size_t {
    if (!s.contains("CGRID2B")) return 0;
    return s.size("CGRID2B");
  };

  return dm;
}

} // namespace models
} // namespace tadah

