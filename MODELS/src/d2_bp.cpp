#include <tadah/models/descriptors/d2/d2_bp.h>
#include <tadah/models/descriptors/d_basis_functions.h>
#include <tadah/models/descriptors/param_meta.h>
#include <tadah/core/stats/data_stats.h>
#include <tadah/models/descriptors/density_reach.h>
namespace tadah {
namespace models {


D2_BP::D2_BP() {
  init();
}
D2_BP::D2_BP(const tadah::core::Context &c): D2_Base(c) {
  init();

  if (!c.get<bool>("INIT2B")) return;

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

  s = cgrid.size();
  auto init_atoms = get_init_atoms(c);
  init_for_atoms(init_atoms);
}

void D2_BP::calc_aed(
  const int Zi,
  const int Zj,
  const double rij,
  const double ,
  tadah::core::aed_type &aed,
  const double scale) const
{
  if (!is_init_for_atoms(Zi,Zj) || rij > get_rcut()) return;

  size_t i=fidx;
  double fc_ij = fcut->calc(rij);
  for (size_t c=0; c<cgrid.size(); c++) {         
    aed(i++) += scale*weights[Zi][Zj]*G(rij,sgrid[c],cgrid[c],fc_ij);
  }

}
void D2_BP::calc_dXijdri(
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
    fd_ij(i++,0) = scale*weights[Zi][Zj]*dG(rij,sgrid[c],cgrid[c],fc_ij,fcp_ij);
  }
}
void D2_BP::calc_all(
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
    aed(i) += scale*weights[Zi][Zj]*G(rij,sgrid[c],cgrid[c],fc_ij);
    fd_ij(i,0) = scale*weights[Zi][Zj]*dG(rij,sgrid[c],cgrid[c],fc_ij,fcp_ij);
    ++i;
  }
}
// Closed-form gradient of D2_BP w.r.t. its hyperparameters.
// See class doc-comment for the math. Mirrors calc_aed's loop-shape so
// a parity test against central-difference of calc_aed works component-
// wise without re-deriving the cutoff or grid bookkeeping.
bool D2_BP::calc_dDdtheta(
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
    // arg_pos is 1-based index into the cgrid vector. Only the matching
    // component receives a non-zero entry.
    if (arg_pos < 1 || static_cast<std::size_t>(arg_pos) > cgrid.size()) return false;
    const std::size_t k  = static_cast<std::size_t>(arg_pos - 1);
    const double      mu = cgrid[k];
    const double      eta = sgrid[k];
    const double      fc = fcut->calc(rij);
    const double      g  = std::exp(-eta * (rij - mu) * (rij - mu)) * fc;
    // dG/d mu = 2 eta (r - mu) G
    const double      dG_dmu = 2.0 * eta * (rij - mu) * g;
    dD_buf(fidx + k) += scale * weights[Zi][Zj] * dG_dmu;
    return true;
  }

  if (theta_name == "SGRID2B") {
    if (arg_pos < 1 || static_cast<std::size_t>(arg_pos) > sgrid.size()) return false;
    const std::size_t k  = static_cast<std::size_t>(arg_pos - 1);
    const double      mu = cgrid[k];
    const double      eta = sgrid[k];
    const double      fc = fcut->calc(rij);
    const double      g  = std::exp(-eta * (rij - mu) * (rij - mu)) * fc;
    // dG/d eta = -(r - mu)^2 G
    const double      dG_deta = -(rij - mu) * (rij - mu) * g;
    dD_buf(fidx + k) += scale * weights[Zi][Zj] * dG_deta;
    return true;
  }

  if (theta_name == "RCUT2B") {
    // RCUT2B is shared across grid components: every basis Gaussian is
    // multiplied by the same fc(r), so dG/d rcut = exp(...) * df_c/d rcut(r)
    // for every component k.
    const double dfc = fcut->calc_drcut(rij);
    if (dfc == 0.0) return false;  // discontinuous cutoff (Cut_Dummy);
                                   // caller will fall back to FD under HYBRID.
    for (std::size_t c = 0; c < cgrid.size(); ++c) {
      const double mu  = cgrid[c];
      const double eta = sgrid[c];
      const double gauss = std::exp(-eta * (rij - mu) * (rij - mu));
      dD_buf(fidx + c) += scale * weights[Zi][Zj] * gauss * dfc;
    }
    return true;
  }

  if (theta_name == "WATOM") {
    // d(aed(fidx+c))/d θ_WATOM = chain × G(...) where chain accounts for
    // the per-species geometric-mean rule (or the direct pair-triple
    // mapping). Claim ownership on every valid arg_pos so the
    // orchestrator does not route to FD when no pair contributes.
    if (!has_watom_arg_pos(arg_pos)) return false;
    const double chain = dweight_dw_n(Zi, Zj, arg_pos);
    if (chain != 0.0) {
      const double fc_ij = fcut->calc(rij);
      for (std::size_t c = 0; c < cgrid.size(); ++c) {
        dD_buf(fidx + c) += scale * chain * G(rij, sgrid[c], cgrid[c], fc_ij);
      }
    }
    return true;
  }

  return false;  // theta_name not owned by this descriptor.
}

// Closed-form mixed derivative of fd_ij(radial) w.r.t. (CGRID2B[k],
// SGRID2B[k], RCUT2B). See d2_bp.h doc-comment for the math; the
// implementation mirrors calc_dDdtheta's branch structure and reuses
// the same fc / fc' values calc_all reads from fcut->calc / calc_prime.
bool D2_BP::calc_dXijdri_dtheta(
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
    const std::size_t k    = static_cast<std::size_t>(arg_pos - 1);
    const double      mu   = cgrid[k];
    const double      eta  = sgrid[k];
    const double      rm   = rij - mu;
    const double      gexp = std::exp(-eta * rm * rm);
    // dG/dr = gexp * (-2 eta rm fc + fcp); d/d mu of this is
    //   2 eta rm * dG/dr + 2 eta fc * gexp
    const double      dGdr = gexp * (-2.0 * eta * rm * fc + fcp);
    const double      val  = 2.0 * eta * rm * dGdr + 2.0 * eta * fc * gexp;
    dfd_ij(fidx + k, 0) += scale * weights[Zi][Zj] * val;
    return true;
  }

  if (theta_name == "SGRID2B") {
    if (arg_pos < 1 || static_cast<std::size_t>(arg_pos) > sgrid.size()) return false;
    const std::size_t k    = static_cast<std::size_t>(arg_pos - 1);
    const double      mu   = cgrid[k];
    const double      eta  = sgrid[k];
    const double      rm   = rij - mu;
    const double      gexp = std::exp(-eta * rm * rm);
    // d/d eta of dG/dr is -rm^2 * dG/dr - 2 rm fc * gexp.
    const double      dGdr = gexp * (-2.0 * eta * rm * fc + fcp);
    const double      val  = -rm * rm * dGdr - 2.0 * rm * fc * gexp;
    dfd_ij(fidx + k, 0) += scale * weights[Zi][Zj] * val;
    return true;
  }

  if (theta_name == "RCUT2B") {
    const double dfc  = fcut->calc_drcut(rij);
    const double dfcp = fcut->calc_prime_drcut(rij);
    if (dfc == 0.0 && dfcp == 0.0) return false;   // Cut_Dummy: HYBRID FD fallback.
    for (std::size_t c = 0; c < cgrid.size(); ++c) {
      const double mu   = cgrid[c];
      const double eta  = sgrid[c];
      const double rm   = rij - mu;
      const double gexp = std::exp(-eta * rm * rm);
      // G doesn't depend on rcut; only fc and fc' do.
      const double val  = gexp * (-2.0 * eta * rm * dfc + dfcp);
      dfd_ij(fidx + c, 0) += scale * weights[Zi][Zj] * val;
    }
    return true;
  }

  if (theta_name == "WATOM") {
    // d(fd_ij(fidx+c, 0))/d θ_WATOM = chain × dG/dr (unweighted force-radial
    // contribution). chain encodes the per-species geometric-mean or
    // pair-triple structure of weights[Zi][Zj].
    if (!has_watom_arg_pos(arg_pos)) return false;
    const double chain = dweight_dw_n(Zi, Zj, arg_pos);
    if (chain != 0.0) {
      for (std::size_t c = 0; c < cgrid.size(); ++c) {
        dfd_ij(fidx + c, 0) +=
          scale * chain * dG(rij, sgrid[c], cgrid[c], fc, fcp);
      }
    }
    return true;
  }

  return false;
}

std::string D2_BP::label() const {
  return lab;
}
void D2_BP::init() {
  keys.push_back("CGRID2B");
  keys.push_back("SGRID2B");
  nparams=2;
}
// ----------------------------------------------------------------
// meta() — see param_meta.h. D2_BP shares its parameter shape with
// D2_Blip (RCUT2B + paired CGRID2B/SGRID2B vectors) so the
// declaration mirrors d2_blip.cpp's; only the descriptor label and
// the role-strings differ.
// ----------------------------------------------------------------
std::optional<DescriptorMeta> D2_BP::meta() const {
  using DataStats = tadah::core::stats::DataStats;
  static constexpr std::size_t I_P10 = tadah::core::stats::IDX_P10;
  static constexpr std::size_t I_P90 = tadah::core::stats::IDX_P90;
  static constexpr std::size_t I_P99 = tadah::core::stats::IDX_P99;

  DescriptorMeta dm;
  dm.label = "D2_BP";

  ParamMeta p_rcut;
  p_rcut.key      = "RCUT2B";
  p_rcut.arg_pos  = -1;
  p_rcut.scale    = Scale::Distance;
  p_rcut.units    = "Å";
  p_rcut.role     = "Outer cutoff for the 2-body neighbour sum (Behler-Parrinello).";
  p_rcut.hard.lo  = 0.0;
  p_rcut.crit     = Criticality::Critical;
  p_rcut.scope    = PairScope::Global;
  p_rcut.suggest = cutoff_suggestion;
  p_rcut.reach_each_pair = true;
  p_rcut.check = [](const DataStats& d, const SeedView& s) {
    return cutoff_reach_check(d, s, "RCUT2B");
  };
  dm.params.push_back(std::move(p_rcut));

  ParamMeta p_cgrid;
  p_cgrid.key      = "CGRID2B";
  p_cgrid.arg_pos  = 0;
  p_cgrid.scale    = Scale::Distance;
  p_cgrid.units    = "Å";
  p_cgrid.role     = "Centre of the i-th radial Gaussian, CGRID2B (i).";
  p_cgrid.hard.lo  = 0.0;
  p_cgrid.crit     = Criticality::Recommended;
  p_cgrid.scope    = PairScope::PerElementPair;
  p_cgrid.suggest  = [](const DataStats& d, const SeedView& s, int) -> BoundSuggestion {
    if (d.per_pair.empty()) return no_pair_suggestion(s);
    return std::make_pair(0.0, d.per_pair[0].r_percentiles[I_P99]);
  };
  dm.params.push_back(std::move(p_cgrid));

  ParamMeta p_sgrid;
  p_sgrid.key      = "SGRID2B";
  p_sgrid.arg_pos  = 0;
  p_sgrid.scale    = Scale::InverseDistanceSq;
  p_sgrid.units    = "Å^-2";
  p_sgrid.role     = "eta of the i-th radial Gaussian exp(-eta (r - c)^2), "
                     "SGRID2B (i): an inverse squared width.";
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
                    "to take a width of the Gaussian from");
    return std::make_pair(0.25 / (spread * spread),
                          25.0 / (spread * spread));
  };
  dm.params.push_back(std::move(p_sgrid));

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

