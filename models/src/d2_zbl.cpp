#include <tadah/models/descriptors/d2/d2_zbl.h>
#include <tadah/models/descriptors/param_meta.h>
#include <tadah/core/stats/data_stats.h>
#include <tadah/models/descriptors/density_reach.h>
namespace tadah {
namespace models {


D2_ZBL::D2_ZBL() {
  init();
}
D2_ZBL::D2_ZBL(const tadah::core::Context &c): D2_Base(c)
{
  if (!c.get<bool>("INIT2B")) return;
  init();
  s=1;
  double temp;
  temp = c.get<double>("TYPE2B",1);
  if (temp!=-1) a0=temp;
  temp = c.get<double>("TYPE2B",2);
  if (temp!=-1) s0=temp;
  temp = c.get<double>("TYPE2B",3);
  if (temp!=-1) p0=temp;
  temp = c.get<double>("TYPE2B",4);
  if (temp!=-1) p1=temp;
  
  // if (verbose) 
  //   std::cout << label() << ": " << a0 << " "
  //     << s0 << " " << p0 << " " << p1 << std::endl;

  ntypes = c.size("ATOM");
  a = new double*[ntypes];
  for (int i = 0; i < ntypes; ++i) {
    a[i] = new double[ntypes];
  }
  for (int i=0; i<ntypes; ++i) {
    std::string symboli = c.get<std::string>("ATOM",i);
    int Zi = tadah::core::PeriodicTable::find_by_symbol(symboli).Z;
    types[Zi] = i;
    for (int j=0; j<ntypes; ++j) {
      std::string symbolj = c.get<std::string>("ATOM",j);
      int Zj = tadah::core::PeriodicTable::find_by_symbol(symbolj).Z;
      a[i][j] = screening_length(Zi,Zj);
    }
  }
  auto init_atoms = get_init_atoms(c);
  init_for_atoms(init_atoms);
}
D2_ZBL::~D2_ZBL() {
  if (a) {
    for (int i = 0; i < ntypes; ++i) {
      delete[] a[i];  
    }
    delete[] a;        
    a = nullptr;       
  }
}

void D2_ZBL::calc_aed(
  const int Zi,
  const int Zj,
  const double rij,
  const double,
  tadah::core::aed_type &aed,
  const double scale) const
{
  if (!is_init_for_atoms(Zi,Zj) || rij > get_rcut()) return;
  double fc_ij = fcut->calc(rij);
  double a_ = a[types[Zi]][types[Zj]];
  double x = rij / a_;
  double p = phi(x);
  aed(fidx) += scale*weights[Zi][Zj]*(Zi * Zj / rij) * p * fc_ij;
}
void D2_ZBL::calc_dXijdri(
  const int Zi,
  const int Zj,
  const double rij,
  const double,
  tadah::core::fd_type &fd_ij,
  const double scale) const
{
  if (!is_init_for_atoms(Zi,Zj) || rij > get_rcut()) return;
  double r_inv = 1.0/rij;
  double a_ = a[types[Zi]][types[Zj]];
  double x = rij / a_;
  double p = phi(x);
  double dp = dphi(x);
  double fc_ij = fcut->calc(rij);
  double fcp_ij = fcut->calc_prime(rij);

  double E_ZBL = (Zi * Zj * r_inv) * p;
  double dE_ZBL_r = -(Zi * Zj * r_inv*r_inv)*p + dp*(Zi * Zj * r_inv) / a_ ;
  fd_ij(fidx,0) = scale*weights[Zi][Zj]*(E_ZBL*fcp_ij + dE_ZBL_r*fc_ij);
}
void D2_ZBL::calc_all(
  const int Zi,
  const int Zj,
  const double rij,
  const double,
  tadah::core::aed_type &aed,
  tadah::core::fd_type &fd_ij,
  const double scale) const
{
  if (!is_init_for_atoms(Zi,Zj) || rij > get_rcut()) return;
  double r_inv = 1.0/rij;
  double a_ = a[types[Zi]][types[Zj]];
  double x = rij / a_;
  double p = phi(x);
  double dp = dphi(x);

  double fc_ij = fcut->calc(rij);
  double fcp_ij = fcut->calc_prime(rij);

  aed(fidx) += scale*weights[Zi][Zj]*(Zi * Zj * r_inv) * p * fc_ij;
  double E_ZBL = (Zi * Zj * r_inv) * p;
  double dE_ZBL_r = -(Zi * Zj * r_inv*r_inv)*p + dp*(Zi * Zj * r_inv) / a_ ;
  fd_ij(fidx,0) = scale*weights[Zi][Zj]*(E_ZBL*fcp_ij + dE_ZBL_r*fc_ij);
}
bool D2_ZBL::calc_dDdtheta(
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
    if (dfc == 0.0) return false;
    const double a_ = a[types[Zi]][types[Zj]];
    const double x  = rij / a_;
    const double p  = phi(x);
    dD_buf(fidx) += scale * weights[Zi][Zj] * (Zi * Zj / rij) * p * dfc;
    return true;
  }

  if (theta_name == "TYPE2B") {
    if (arg_pos < 2 || arg_pos > 5) return false;

    const double a_     = a[types[Zi]][types[Zj]];
    const double x      = rij / a_;
    const double dphi_x = dphi(x);
    const double fc_ij  = fcut->calc(rij);

    const double Zi_d   = static_cast<double>(Zi);
    const double Zj_d   = static_cast<double>(Zj);
    const double D_zz   = std::pow(Zi_d, p0) + std::pow(Zj_d, p1);

    double da_dtheta = 0.0;
    if (arg_pos == 2) {
      da_dtheta = a_ / a0;
    } else if (arg_pos == 3) {
      da_dtheta = a_ / s0;
    } else if (arg_pos == 4) {
      da_dtheta = -a_ * std::pow(Zi_d, p0) * std::log(Zi_d) / D_zz;
    } else {
      da_dtheta = -a_ * std::pow(Zj_d, p1) * std::log(Zj_d) / D_zz;
    }

    if (da_dtheta == 0.0) return true;

    const double dx_dtheta   = -(x / a_) * da_dtheta;
    const double dphi_dtheta = dphi_x * dx_dtheta;
    dD_buf(fidx) += scale * weights[Zi][Zj] * (Zi * Zj / rij) * dphi_dtheta * fc_ij;
    return true;
  }

  if (theta_name == "WATOM") {
    // d(aed)/d θ_WATOM = chain × (Zi Zj / r) phi(x) fc(r) (unweighted
    // calc_aed contribution).
    if (!has_watom_arg_pos(arg_pos)) return false;
    const double chain = dweight_dw_n(Zi, Zj, arg_pos);
    if (chain != 0.0) {
      const double fc_ij = fcut->calc(rij);
      const double a_    = a[types[Zi]][types[Zj]];
      const double x     = rij / a_;
      const double p     = phi(x);
      dD_buf(fidx) += scale * chain * (Zi * Zj / rij) * p * fc_ij;
    }
    return true;
  }

  return false;
}

// Closed-form mixed derivative of fd_ij(radial) w.r.t. θ ∈
// {RCUT2B, TYPE2B[a0/s0/p0/p1]}. See d2_zbl.h doc-comment for the math.
bool D2_ZBL::calc_dXijdri_dtheta(
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

  const double r_inv  = 1.0 / rij;
  const double a_     = a[types[Zi]][types[Zj]];
  const double x      = rij / a_;
  const double p_x    = phi(x);
  const double dp_x   = dphi(x);
  const double fc_ij  = fcut->calc(rij);
  const double fcp_ij = fcut->calc_prime(rij);
  const double E_ZBL  = (Zi * Zj * r_inv) * p_x;
  const double dE_ZBL_r = -(Zi * Zj * r_inv * r_inv) * p_x + dp_x * (Zi * Zj * r_inv) / a_;

  if (theta_name == "RCUT2B") {
    const double dfc  = fcut->calc_drcut(rij);
    const double dfcp = fcut->calc_prime_drcut(rij);
    if (dfc == 0.0 && dfcp == 0.0) return false;   // Cut_Dummy fallback
    // fd = w(Zj) (E·fcp + dE/dr·fc); only fc/fcp depend on rcut.
    const double val = E_ZBL * dfcp + dE_ZBL_r * dfc;
    dfd_ij(fidx, 0) += scale * weights[Zi][Zj] * val;
    return true;
  }

  if (theta_name == "TYPE2B") {
    if (arg_pos < 2 || arg_pos > 5) return false;

    const double Zi_d = static_cast<double>(Zi);
    const double Zj_d = static_cast<double>(Zj);
    const double D_zz = std::pow(Zi_d, p0) + std::pow(Zj_d, p1);

    double da_dtheta = 0.0;
    if      (arg_pos == 2) da_dtheta =  a_ / a0;
    else if (arg_pos == 3) da_dtheta =  a_ / s0;
    else if (arg_pos == 4) da_dtheta = -a_ * std::pow(Zi_d, p0) * std::log(Zi_d) / D_zz;
    else                   da_dtheta = -a_ * std::pow(Zj_d, p1) * std::log(Zj_d) / D_zz;

    if (da_dtheta == 0.0) return true;   // Z=1 vanishing case for p0/p1

    // d/d theta (fd_radial) = -w(Zj) (Zi Zj / a²) * da/d theta
    //                         * [dphi(x) fcp + d²phi(x) fc / a]
    const double d2phi_x = d2phi(x);
    const double bracket = dp_x * fcp_ij + d2phi_x * fc_ij / a_;
    const double pref    = -(Zi * Zj / (a_ * a_)) * da_dtheta;
    dfd_ij(fidx, 0) += scale * weights[Zi][Zj] * pref * bracket;
    return true;
  }

  if (theta_name == "WATOM") {
    // Force-side d(fd)/d θ_WATOM = chain × (unweighted ZBL force-radial
    // contribution).
    if (!has_watom_arg_pos(arg_pos)) return false;
    const double chain = dweight_dw_n(Zi, Zj, arg_pos);
    if (chain != 0.0) {
      dfd_ij(fidx, 0) += scale * chain * (E_ZBL * fcp_ij + dE_ZBL_r * fc_ij);
    }
    return true;
  }

  return false;
}

std::string D2_ZBL::label() const {
  return lab;
}
void D2_ZBL::init() {
  nparams=4;
}
double D2_ZBL::phi(double x) const {
  return 0.1818 * std::exp(-3.2 * x) +
  0.5099 * std::exp(-0.9423 * x) +
  0.2802 * std::exp(-0.4029 * x) +
  0.02817 * std::exp(-0.2016 * x);
}
double D2_ZBL::dphi(double x) const {
  return -3.2 * 0.1818 * std::exp(-3.2 * x) -
  0.9423 * 0.5099 * std::exp(-0.9423 * x) -
  0.4029 * 0.2802 * std::exp(-0.4029 * x) -
  0.2016 * 0.02817 * std::exp(-0.2016 * x);
}
double D2_ZBL::d2phi(double x) const {
  // d²/dx² of sum_k c_k exp(-α_k x) = sum_k α_k² c_k exp(-α_k x).
  return  3.2     * 3.2     * 0.1818  * std::exp(-3.2    * x) +
          0.9423  * 0.9423  * 0.5099  * std::exp(-0.9423 * x) +
          0.4029  * 0.4029  * 0.2802  * std::exp(-0.4029 * x) +
          0.2016  * 0.2016  * 0.02817 * std::exp(-0.2016 * x);
}
double D2_ZBL::screening_length(int Zi, int Zj) const {
  return (s0 * a0) / (std::pow(Zi, p0) + std::pow(Zj, p1));
}

// ----------------------------------------------------------------
// meta() — D2_ZBL exposes RCUT2B (real-space cutoff, shared with the
// cutoff family) plus the four TYPE2B-positional screening params
// (a0, s0, p0, p1) sitting at config indices 1..4 of the TYPE2B value
// list "D2_ZBL a0 s0 p0 p1 ELEM_i ELEM_j".
//
// Their ParamMeta::arg_pos is the position of the value on the
// descriptor's TYPE2B line, from 1, the name D2_ZBL being 1: a0 to p1 are
// 2 to 5, the arg_pos calc_dDdtheta takes for them. They have no bound
// suggester: tadah hpo searches no value of a TYPE2B line (it refuses
// OPTIM TYPE2B), and tadah analysis suggest_bounds gives each a row
// without an OPTIM line.
// ----------------------------------------------------------------
std::optional<DescriptorMeta> D2_ZBL::meta() const {
  using DataStats = tadah::core::stats::DataStats;

  DescriptorMeta dm;
  dm.label = "D2_ZBL";

  ParamMeta p_rcut;
  p_rcut.key      = "RCUT2B";
  p_rcut.arg_pos  = -1;
  p_rcut.scale    = Scale::Distance;
  p_rcut.units    = "Å";
  p_rcut.role     = "Outer cutoff for the ZBL pair sum.";
  p_rcut.hard.lo  = 0.0;
  p_rcut.crit     = Criticality::Critical;
  p_rcut.scope    = PairScope::Global;
  p_rcut.suggest = cutoff_suggestion;
  p_rcut.reach_each_pair = true;
  p_rcut.check = [](const DataStats& d, const SeedView& s) {
    return cutoff_reach_check(d, s, "RCUT2B");
  };
  dm.params.push_back(std::move(p_rcut));

  // a0 — universal length scale (default Bohr radius 0.529 Å).
  ParamMeta p_a0;
  p_a0.key     = "TYPE2B";
  p_a0.arg_pos = 2;
  p_a0.scale   = Scale::Distance;
  p_a0.units   = "Å";
  p_a0.role    = "ZBL screening length scale (default: Bohr radius 0.5292 Å).";
  p_a0.hard.lo = kHardLoPositive;
  p_a0.crit    = Criticality::Optional;
  p_a0.scope   = PairScope::Global;
  dm.params.push_back(std::move(p_a0));

  // s0 — dimensionless screening multiplier (default 0.88534).
  ParamMeta p_s0;
  p_s0.key     = "TYPE2B";
  p_s0.arg_pos = 3;
  p_s0.scale   = Scale::Dimensionless;
  p_s0.units   = "";
  p_s0.role    = "ZBL screening multiplier s0 (default 0.88534).";
  p_s0.hard.lo = kHardLoPositive;
  p_s0.crit    = Criticality::Optional;
  p_s0.scope   = PairScope::Global;
  dm.params.push_back(std::move(p_s0));

  // p0 — exponent on Z_i in the screening-length denominator (default 0.23).
  ParamMeta p_p0;
  p_p0.key     = "TYPE2B";
  p_p0.arg_pos = 4;
  p_p0.scale   = Scale::Dimensionless;
  p_p0.units   = "";
  p_p0.role    = "ZBL exponent on Zi in screening-length denominator (default 0.23).";
  p_p0.hard.lo = kHardLoPositive;
  p_p0.crit    = Criticality::Optional;
  p_p0.scope   = PairScope::Global;
  dm.params.push_back(std::move(p_p0));

  // p1 — exponent on Z_j in the screening-length denominator (default 0.23).
  ParamMeta p_p1;
  p_p1.key     = "TYPE2B";
  p_p1.arg_pos = 5;
  p_p1.scale   = Scale::Dimensionless;
  p_p1.units   = "";
  p_p1.role    = "ZBL exponent on Zj in screening-length denominator (default 0.23).";
  p_p1.hard.lo = kHardLoPositive;
  p_p1.crit    = Criticality::Optional;
  p_p1.scope   = PairScope::Global;
  dm.params.push_back(std::move(p_p1));

  dm.expected_columns = [](const SeedView&) -> std::size_t { return 1; };
  return dm;
}

} // namespace models
} // namespace tadah

