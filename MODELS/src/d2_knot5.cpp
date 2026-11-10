#include "tadah/models/descriptors/d_base.h"
#include <tadah/models/descriptors/d2/d2_knot5.h>
#include <tadah/models/descriptors/d_basis_functions.h>
#include <tadah/models/descriptors/param_meta.h>
#include <tadah/core/logger.h>
#include <tadah/core/number_format.h>
#include <tadah/core/stats/data_stats.h>
#include <algorithm>
#include <cstdlib>
#include <stdexcept>
#include <tadah/models/descriptors/density_reach.h>
namespace tadah {
namespace models {


D2_Knot5::D2_Knot5() {
  init();
}
D2_Knot5::D2_Knot5(const tadah::core::Context &c): D2_Base(c)
{
  if (!c.get<bool>("INIT2B")) return;
  init();

  get_grid(c,"CGRID2B",cgrid);
  if (c.contains("SGRID2B") && c.size("SGRID2B") > 0)
    throw std::runtime_error(
        "D2_Knot5: SGRID2B must not be present — eta is a pure gauge for "
        "the knot family ((c,eta)->(c s^5, eta/s)); the knot positions "
        "CGRID2B define the basis alone.\n");
  if (c.contains("KDEG2B") && c.size("KDEG2B") > 0)
    get_grid(c,"KDEG2B",dgrid);   // optional per-atom degree (3|5)
  D_Base::printGrid(cgrid,"CGRID2B");

  if (!dgrid.empty()) {
    if (dgrid.size() != cgrid.size())
      throw std::runtime_error("KDEG2B and CGRID2B arrays differ in size.\n");
    for (const auto &d : dgrid)
      if (d != 3.0 && d != 5.0)
        throw std::runtime_error("KDEG2B entries must be 3 or 5.\n");
  }

  s=cgrid.size();

  auto init_atoms = get_init_atoms(c);
  init_for_atoms(init_atoms);
  warn_if_knots_beyond_rcut();
}

void D2_Knot5::warn_if_knots_beyond_rcut() {
  if (cgrid.empty() || !fcut) return;
  const double mx = *std::max_element(cgrid.begin(), cgrid.end());
  const double rc = get_rcut();
  if (mx > rc + 1e-12) {
    // tadah sets RCUT2B to the largest knot on a TYPE2B line of D2_Knot5
    // alone (potedit::normalize_knot_rcut), so the warning comes where the
    // stated cutoff is kept: a D2_mJoin line, or a program or pair_style
    // tadah that reads a potential file as written.
    log_warning_once(
        "D2_Knot5: knot past RCUT2B",
        "D2_Knot5: the largest CGRID2B knot, " +
            tadah::core::format_roundtrip(mx) + ", lies beyond RCUT2B, " +
            tadah::core::format_roundtrip(rc) +
            ", so the knot functions are cut off at RCUT2B; an RCUT2B at "
            "the largest knot keeps them whole.");
  }
}

void D2_Knot5::calc_aed(
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
    double t = kval(c, rij-cgrid[c], fc_ij);
    aed(i++) += scale*weights[Zi][Zj]*t;
  }
}
void D2_Knot5::calc_dXijdri(
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
    fd_ij(i++,0) = scale*weights[Zi][Zj]*kder(c, rij-cgrid[c], 1.0, fc_ij, fcp_ij);
  }
}
void D2_Knot5::calc_all(
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
    aed(i) += scale*weights[Zi][Zj]*kval(c, rij-cgrid[c], fc_ij);
    fd_ij(i,0) = scale*weights[Zi][Zj]*kder(c, rij-cgrid[c], 1.0, fc_ij, fcp_ij);
    ++i;
  }
}

// Closed-form gradient of D2_Knot5 w.r.t. its hyperparameters — the same
// shift-scale chain rule as D2_Blip (mirrors calc_aed's loop shape so a
// parity test against central-difference of calc_aed works component-wise).
// dK5L(x_c, 1.0) is the bare dK5L/dx_c.
bool D2_Knot5::calc_dDdtheta(
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
    const double      xc  = rij - mu;       // eta == 1 (gauge)
    const double      fc  = fcut->calc(rij);
    const double      dK_dx = (deg(k) == 3) ? dK3L(xc, 1.0)
                                            : dK5L(xc, 1.0);  // bare dK/dx_c
    // d(x_c)/d(mu) = -1
    const double      dD_dmu = -dK_dx * fc;
    dD_buf(fidx + k) += scale * weights[Zi][Zj] * dD_dmu;
    return true;
  }
  // no SGRID2B branch: the knot family has no width parameter

  if (theta_name == "RCUT2B") {
    // RCUT2B enters only through f_c(r).
    const double dfc = fcut->calc_drcut(rij);
    if (dfc == 0.0) return false;  // discontinuous cutoff (Cut_Dummy);
                                   // caller falls back to FD under HYBRID.
    for (std::size_t c = 0; c < cgrid.size(); ++c) {
      const double xc  = rij - cgrid[c];
      dD_buf(fidx + c) += scale * weights[Zi][Zj] * kval(c, xc, 1.0) * dfc;
    }
    return true;
  }

  if (theta_name == "WATOM") {
    if (!has_watom_arg_pos(arg_pos)) return false;
    const double chain = dweight_dw_n(Zi, Zj, arg_pos);
    if (chain != 0.0) {
      const double fc_ij = fcut->calc(rij);
      for (std::size_t c = 0; c < cgrid.size(); ++c) {
        const double xc = rij - cgrid[c];
        dD_buf(fidx + c) += scale * chain * kval(c, xc, fc_ij);
      }
    }
    return true;
  }

  return false;  // theta_name not owned by this descriptor.
}

// Closed-form mixed derivative d(fd_ij_radial)/d theta — same math as
// D2_Blip with (b, b1, b2) -> (K5L, K5L', K5L''). The family is C^4 so
// b2 = d2K5L is CONTINUOUS everywhere (no FD-probe hazards at knots).
bool D2_Knot5::calc_dXijdri_dtheta(
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
    const double      xc  = rij - mu;        // eta == 1 (gauge)
    const double      b1  = (deg(k) == 3) ? dK3L(xc, 1.0) : dK5L(xc, 1.0);
    const double      b2  = (deg(k) == 3) ? d2K3L(xc) : d2K5L(xc);
    const double      val = -(fc * b2 + fcp * b1);
    dfd_ij(fidx + k, 0) += scale * weights[Zi][Zj] * val;
    return true;
  }
  // no SGRID2B branch: the knot family has no width parameter

  if (theta_name == "RCUT2B") {
    const double dfc  = fcut->calc_drcut(rij);
    const double dfcp = fcut->calc_prime_drcut(rij);
    if (dfc == 0.0 && dfcp == 0.0) return false;   // Cut_Dummy fallback
    for (std::size_t c = 0; c < cgrid.size(); ++c) {
      const double xc  = rij - cgrid[c];
      const double b   = kval(c, xc, 1.0);
      const double b1  = (deg(c) == 3) ? dK3L(xc, 1.0) : dK5L(xc, 1.0);
      const double val = dfc * b1 + dfcp * b;
      dfd_ij(fidx + c, 0) += scale * weights[Zi][Zj] * val;
    }
    return true;
  }

  if (theta_name == "WATOM") {
    if (!has_watom_arg_pos(arg_pos)) return false;
    const double chain = dweight_dw_n(Zi, Zj, arg_pos);
    if (chain != 0.0) {
      for (std::size_t c = 0; c < cgrid.size(); ++c) {
        const double xc = rij - cgrid[c];
        dfd_ij(fidx + c, 0) +=
          scale * chain * kder(c, xc, 1.0, fc, fcp);
      }
    }
    return true;
  }

  return false;
}

std::string D2_Knot5::label() const {
  return lab;
}
void D2_Knot5::init() {
  // contract (see d_base.h): TYPE2B D2_Knot5 <nC> <nK> El El —
  // one SIZE token per keys entry then per data_keys entry. No width key:
  // eta is a pure gauge for the knot family.
  keys.push_back("CGRID2B");
  data_keys.push_back("KDEG2B");
  nparams = 2;
}
std::optional<DescriptorMeta> D2_Knot5::meta() const {
  using DataStats = tadah::core::stats::DataStats;
  static constexpr std::size_t I_P99 = tadah::core::stats::IDX_P99;

  DescriptorMeta dm;
  dm.label = "D2_Knot5";

  // RCUT2B — outer cutoff. Knots at or below rcut vanish smoothly there.
  // The suggestion of every cutoff: tadah hpo searches the stated cutoff of
  // a D2_Knot5 on a D2_mJoin line; on a line of D2_Knot5 alone it derives
  // the cutoff from the largest knot, and tadah analysis suggest_bounds
  // gives that row no OPTIM line (Tadah.MLIP).
  ParamMeta p_rcut;
  p_rcut.key      = "RCUT2B";
  p_rcut.arg_pos  = -1;
  p_rcut.scale    = Scale::Distance;
  p_rcut.units    = "Å";
  p_rcut.role     = "Outer cutoff for the 2-body neighbour sum.";
  p_rcut.hard.lo  = 0.0;
  p_rcut.crit     = Criticality::Critical;
  p_rcut.scope    = PairScope::Global;
  p_rcut.suggest  = cutoff_suggestion;
  p_rcut.reach_each_pair = true;
  p_rcut.check    = [](const DataStats& d, const SeedView& s) {
    return cutoff_reach_check(d, s, "RCUT2B");
  };
  dm.params.push_back(std::move(p_rcut));

  // CGRID2B — vector: knot positions (the only true shape parameters).
  ParamMeta p_cgrid;
  p_cgrid.key      = "CGRID2B";
  p_cgrid.arg_pos  = 0;
  p_cgrid.scale    = Scale::Distance;
  p_cgrid.units    = "Å";
  p_cgrid.role     = "Knot position r_s of the i-th truncated power "
                     "(r_s - r)_+^p, CGRID2B (i), with p = 5 unless KDEG2B (i) "
                     "gives 3; on a TYPE2B line of D2_Knot5 alone the "
                     "cutoff RCUT2B is the largest knot.";
  p_cgrid.hard.lo  = 0.0;
  p_cgrid.crit     = Criticality::Recommended;
  p_cgrid.scope    = PairScope::PerElementPair;
  p_cgrid.suggest  = [](const DataStats& d, const SeedView& s, int) -> BoundSuggestion {
    if (d.per_pair.empty()) return no_pair_suggestion(s);
    const auto& pp = d.per_pair[0];
    return std::make_pair(0.0, pp.r_percentiles[I_P99]);
  };
  dm.params.push_back(std::move(p_cgrid));

  // KDEG2B — per-atom degree DATA (3|5): a data_keys entry, split by
  // D2_mJoin but never optimizable (out-of-range values are rejected at
  // construction — the de-facto OPTIM guard).
  ParamMeta p_kdeg;
  p_kdeg.key      = "KDEG2B";
  p_kdeg.arg_pos  = 0;
  p_kdeg.scale    = Scale::Dimensionless;
  p_kdeg.units    = "";
  p_kdeg.role     = "Degree of each knot function, one entry per CGRID2B "
                    "knot: 3 (cubic, an exact f''' corner) or 5 (quintic, "
                    "the degree when KDEG2B is absent); not optimisable.";
  p_kdeg.hard.lo  = 3.0;
  p_kdeg.crit     = Criticality::Optional;
  p_kdeg.scope    = PairScope::Global;
  dm.params.push_back(std::move(p_kdeg));

  dm.cross_param_checks.push_back(
    [](const DataStats&, const SeedView& s) -> HealthVerdict {
      if (s.contains("SGRID2B") && s.size("SGRID2B") > 0)
        return HealthFail{"D2_Knot5 carries no width key: SGRID2B must be "
                          "absent (eta is a pure gauge)."};
      if (s.contains("KDEG2B") && s.size("KDEG2B") > 0 &&
          s.contains("CGRID2B") && s.size("KDEG2B") != s.size("CGRID2B"))
        return HealthFail{"KDEG2B and CGRID2B sizes differ."};
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
