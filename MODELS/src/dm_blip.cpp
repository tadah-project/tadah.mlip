#include <tadah/models/descriptors/dm/dm_func.h>
#include <tadah/models/descriptors/dm/dm_blip.h>
#include <tadah/models/descriptors/dm/dm_impl_utils.h>
#include <tadah/models/descriptors/d_basis_functions.h>
#include <tadah/models/descriptors/density_reach.h>

#include <vector>
#include <stdexcept>

constexpr std::size_t MAX_GRID = 64;
static_assert(MAX_GRID>=1,"MAX_GRID must be > 0");

namespace {
using namespace tadah::models;
using namespace tadah::models::detail;

/* ------------ radial pre-computation (Blip) ----------------------- */
inline void precompute_radials(double r,double fc,
                               const std::vector<double>& sg,
                               const std::vector<double>& cg,
                               double* g,std::size_t n)
{
  for(std::size_t i=0;i<n;++i){
    const double x = sg[i]*(r-cg[i]);
    g[i] = B(x,fc);
  }
}
inline void precompute_radials(double r,double fc,double fcp,
                               const std::vector<double>& sg,
                               const std::vector<double>& cg,
                               double* g,double* dg,std::size_t n)
{
  for(std::size_t i=0;i<n;++i){
    const double s  = sg[i];
    const double x  = s*(r-cg[i]);
    g [i] = B (x,fc);
    dg[i] = dB(x,s,fc,fcp);          // derivative w.r.t. r
  }
}

} // namespace

/* ================================================================== *
 *  DM_Blip implementation                                            *
 * ================================================================== */
namespace tadah {
namespace models {

DM_Blip::DM_Blip() { init(); }

/* ------------------------------------------------------------------ */
DM_Blip::DM_Blip(const core::Context& ctx)
: DM_Base(ctx)
, rc(ctx.get<double>("RCUTMBMAX"))
{
  if(!ctx.get<bool>("INITMB")) return;
  init();

  get_grid(ctx,"CGRIDMB",cgrid);
  get_grid(ctx,"SGRIDMB",sgrid);
  D_Base::printGrid(cgrid,"CGRIDMB");
  D_Base::printGrid(sgrid,"SGRIDMB");

  if(cgrid.size()!=sgrid.size())
    throw std::runtime_error("CGRIDMB and SGRIDMB sizes differ.");

  Lmax = ctx.get<int>("TYPEMB",1);
  if(Lmax>6)
    throw std::runtime_error("DM_Blip: Lmax>6 not supported.");

  s         = cgrid.size()*(Lmax+1);
  rhoisize  = gen_atomic_orbitals(Lmax)*cgrid.size();
  set_rfidx(0);
  set_drfidx(rhoisize);

  init_for_atoms( get_init_atoms(ctx) );
}

/* ------------------ embedding energy / derivative ----------------- */
void DM_Blip::calc_aed(core::rho_type& rhoi, core::aed_type& aed) const
{
  std::size_t Lshift=0, ii=rfidx, dii=drfidx;
  const std::size_t ng=cgrid.size();

  for(int L=0;L<=Lmax;++L){
    for(std::size_t o=0;o<orbitals[L].size();++o){
      const double f=fac[L][o];
      for(std::size_t c=0;c<ng;++c,++ii,++dii){
        const double rho=rhoi(ii);
        aed (c+Lshift+fidx) += f*rho*rho;    // F(ρ)=ρ²
        rhoi(dii)   = 2.0*f*rho;     // F′(ρ)=2ρ
      }
    }
    Lshift+=ng;
  }
}
/* ------------------------------------------------------------------ *
 *  dX_ij / dr_i   &   dX_ji / dr_i  (pair contribution, symmetric)
 *  --- single-evaluation version (no duplicate j-branch) ------------
 * ------------------------------------------------------------------ */
void DM_Blip::calc_dXijdri_dXjidri(
        int Zi,int Zj,double rij,double,
        const core::Vec3d& v,
        core::rho_type&  rhoi,
        core::rho_type&  rhoj,
        core::fd_type&   fd_ij,
        double           scale) const
{
  if(!is_init_for_atoms(Zi,Zj) || rij>get_rcut()) return;

  const double x=v[0], y=v[1], z=v[2];
  const double w_pair = weights[Zi][Zj];

  /* --- radial part ------------------------------------------------- */
  const double fc  = fcut->calc       (rij);
  const double fcp = fcut->calc_prime (rij);

  const std::size_t ng = cgrid.size();
  StackOrHeapBuffer<MAX_GRID> buf(ng);
  precompute_radials(rij,fc,fcp,sgrid,cgrid,buf.g(),buf.dg(),ng);

  /* --- Cartesian powers (and derivatives) -------------------------- */
  double px[7],py[7],pz[7],dpx[7],dpy[7],dpz[7];
  precompute_powers(x,Lmax,px,dpx);
  precompute_powers(y,Lmax,py,dpy);
  precompute_powers(z,Lmax,pz,dpz);

  const double rinv = 1.0/rij;
  std::size_t   Lshift = 0;
  std::size_t   ii     = drfidx;      // derivative part

  for(int L=0; L<=Lmax; ++L)
  {
    const double s_L = (L & 1) ? 1.0 : -1.0;    //  (-1)^{L+1}

    for(std::size_t o=0; o<orbitals[L].size(); ++o)
    {
      const int lx = orbitals[L][o][0],
                ly = orbitals[L][o][1],
                lz = orbitals[L][o][2];

      /* polynomials and their Cartesian derivatives (partner i) ----- */
      const double pxij = px[lx],   pyij = py[ly],   pzij = pz[lz];
      const double pqrij = pxij*pyij*pzij;

      const double txij = dpx[lx]*pyij*pzij;
      const double tyij = dpy[ly]*pxij*pzij;
      const double tzij = dpz[lz]*pyij*pxij;

      for(std::size_t c=0; c<ng; ++c, ++ii)
      {
        const double g  = buf.g ()[c];
        const double dg = buf.dg()[c];

        /* common Cartesian brackets --------------------------------- */
        const double Bx = g*txij + pqrij*dg*x*rinv;
        const double By = g*tyij + pqrij*dg*y*rinv;
        const double Bz = g*tzij + pqrij*dg*z*rinv;

        /* i/j prefactor --------------------------------------------- */
        const double coef = scale * w_pair *
                            ( rhoi(ii) - s_L * rhoj(ii) );

        const std::size_t row = c + Lshift + fidx;
        fd_ij(row,0) += coef * Bx;
        fd_ij(row,1) += coef * By;
        fd_ij(row,2) += coef * Bz;
      }
    }
    Lshift += ng;
  }
}

/* -------------------- dXij/dr_i (one-sided) ----------------------- */
void DM_Blip::calc_dXijdri(
        int Zi,int Zj,double rij,double,
        const core::Vec3d& v,
        core::rho_type&   rhoi,
        core::fd_type&    fd_ij,
        double            scale) const
{
  if (!is_init_for_atoms(Zi,Zj) || rij > get_rcut()) return;

  const double x = v[0],  y = v[1],  z = v[2];
  const double w_pair = weights[Zi][Zj];

  const double fc  = scale*w_pair * fcut->calc       (rij);
  const double fcp = scale*w_pair * fcut->calc_prime (rij);

  const double rinv = 1.0/rij;
  const std::size_t ng = cgrid.size();

  /* radial arrays -------------------------------------------------- */
  StackOrHeapBuffer<MAX_GRID> buf(ng);
  precompute_radials(rij,fc,fcp,sgrid,cgrid,buf.g(),buf.dg(),ng);

  /* Cartesian powers & derivatives --------------------------------- */
  double px[7],py[7],pz[7],dpx[7],dpy[7],dpz[7];
  precompute_powers(x,Lmax,px ,dpx);
  precompute_powers(y,Lmax,py ,dpy);
  precompute_powers(z,Lmax,pz ,dpz);

  std::size_t Lshift = 0;                 // row offset in fd
  std::size_t ii     = drfidx;  // derivative part of rho_i

  /* ========================== main loops ========================== */
  for (int L = 0; L <= Lmax; ++L)
  {
    for (std::size_t o = 0; o < orbitals[L].size(); ++o)
    {
      const int lx = orbitals[L][o][0],
                ly = orbitals[L][o][1],
                lz = orbitals[L][o][2];

      const double pxij = px[lx],  pyij = py[ly],  pzij = pz[lz];

      const double pqrij = pxij*pyij*pzij;          // x^lx y^ly z^lz

      const double txij = dpx[lx]*pyij*pzij;        // ∂/∂x
      const double tyij = dpy[ly]*pxij*pzij;        // ∂/∂y
      const double tzij = dpz[lz]*pyij*pxij;        // ∂/∂z

      /* coord-dependent part of radial derivative (constant for c) -- */
      const double t2x_pref = pqrij * rinv * x;
      const double t2y_pref = pqrij * rinv * y;
      const double t2z_pref = pqrij * rinv * z;

      /* ----------------------- grid loop --------------------------- */
#pragma omp simd
      for (std::size_t c = 0; c < ng; ++c)
      {
        const double g  = buf.g ()[c];
        const double dg = buf.dg()[c];

        const double termx = g*txij + dg*t2x_pref;
        const double termy = g*tyij + dg*t2y_pref;
        const double termz = g*tzij + dg*t2z_pref;

        const double f_r = rhoi(ii + c);            // ρ′_i for (o,c)

        const std::size_t row = c + Lshift + fidx;
        fd_ij(row,0) += f_r * termx;
        fd_ij(row,1) += f_r * termy;
        fd_ij(row,2) += f_r * termz;
      }
      ii += ng;                                     // advance derivative index
    }
    Lshift += ng;
  }
}

/* ----------------------- ρ_i accumulation ------------------------- */
void DM_Blip::calc_rho(int Zi,int Zj,double rij,double,
                       const core::Vec3d& v,
                       core::rho_type&    rhoi,
                       double             scale) const
{
  if(!is_init_for_atoms(Zi,Zj) || rij>get_rcut()) return;

  const double fc = scale*weights[Zi][Zj]*fcut->calc(rij);

  const std::size_t ng=cgrid.size();
  StackOrHeapBuffer<MAX_GRID> buf(ng);
  precompute_radials(rij,fc,sgrid,cgrid,buf.g(),ng);

  double px[7],py[7],pz[7];
  precompute_powers(v[0],Lmax,px);
  precompute_powers(v[1],Lmax,py);
  precompute_powers(v[2],Lmax,pz);

  std::size_t ii=rfidx;

  for(int L=0;L<=Lmax;++L){
    for(std::size_t o=0;o<orbitals[L].size();++o){
      const int lx=orbitals[L][o][0],
                ly=orbitals[L][o][1],
                lz=orbitals[L][o][2];

      const double poly = px[lx]*py[ly]*pz[lz];

      for(std::size_t c=0;c<ng;++c)
        rhoi(ii++) += buf.g()[c]*poly;
    }
  }
}

/* ------------------------------------------------------------------ *
 *  drho_i/dtheta — per-pair accumulator (analog of calc_rho)         *
 * ------------------------------------------------------------------ */
bool DM_Blip::calc_drhoi_dtheta(
    DMThetaKey         key,
    int                arg_pos,
    const int          Zi,
    const int          Zj,
    const double       rij,
    const double       /*rij_sq*/,
    const core::Vec3d& v,
    core::rho_type&    drhoi,
    const double       scale) const
{
  /* Cheap key short-circuit before any pair-level math. The walker
   * decoded the theta_name once for the whole gradient call. */
  const bool owned = (key == DMThetaKey::CGRIDMB ||
                      key == DMThetaKey::SGRIDMB ||
                      key == DMThetaKey::RCUTMB  ||
                      key == DMThetaKey::WATOM);
  if (!owned) return false;

  if(!is_init_for_atoms(Zi,Zj) || rij>get_rcut()) return false;

  if (key == DMThetaKey::WATOM) {
    /* d(rho)/d θ_WATOM = chain × poly × B(x_c) × fc(r), the per-pair
     * calc_rho contribution with the weights[Zi][Zj] factor replaced by
     * the WATOM chain factor for the current mode. */
    if (!has_watom_arg_pos(arg_pos)) return false;
    const double chain = dweight_dw_n(Zi, Zj, arg_pos);
    if (chain == 0.0) return true;

    const double fc_ij = fcut->calc(rij);
    const std::size_t ng = cgrid.size();
    StackOrHeapBuffer<MAX_GRID> buf(ng);
    precompute_radials(rij, fc_ij, sgrid, cgrid, buf.g(), ng);

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
        const double poly = px[lx] * py[ly] * pz[lz];
        for (std::size_t c = 0; c < ng; ++c)
          drhoi(ii + c) += scale * chain * poly * buf.g()[c];
        ii += ng;
      }
    }
    return true;
  }

  const double wj = weights[Zi][Zj] * scale;
  const double fc = fcut->calc(rij);

  const std::size_t ng = cgrid.size();
  double px[7],py[7],pz[7];
  precompute_powers(v[0],Lmax,px);
  precompute_powers(v[1],Lmax,py);
  precompute_powers(v[2],Lmax,pz);

  if (key == DMThetaKey::CGRIDMB) {
    if (arg_pos < 1 || static_cast<std::size_t>(arg_pos) > ng) return false;
    const std::size_t k   = static_cast<std::size_t>(arg_pos - 1);
    const double      mu  = cgrid[k];
    const double      eta = sgrid[k];
    const double      xc  = eta * (rij - mu);
    /* dB/dr_s = -eta dB/dx_c; fc factor multiplies. Use dB(x, 1.0) to
     * obtain the bare dB/dx_c (eta is the r-chain factor in the existing
     * dB(x_c, eta) helper, see d_basis_functions.h). */
    const double      dB_dx = dB(xc, 1.0);
    const double      dB_dmu_fc = -eta * dB_dx * fc;
    std::size_t ii = rfidx;
    for(int L = 0; L <= Lmax; ++L){
      for(std::size_t o = 0; o < orbitals[L].size(); ++o){
        const int lx = orbitals[L][o][0],
                  ly = orbitals[L][o][1],
                  lz = orbitals[L][o][2];
        const double poly = px[lx] * py[ly] * pz[lz];
        drhoi(ii + k) += wj * poly * dB_dmu_fc;
        ii += ng;
      }
    }
    return true;
  }

  if (key == DMThetaKey::SGRIDMB) {
    if (arg_pos < 1 || static_cast<std::size_t>(arg_pos) > ng) return false;
    const std::size_t k   = static_cast<std::size_t>(arg_pos - 1);
    const double      mu  = cgrid[k];
    const double      eta = sgrid[k];
    const double      xc  = eta * (rij - mu);
    /* dB/deta = (r - mu) dB/dx_c; fc factor multiplies. */
    const double      dB_dx = dB(xc, 1.0);
    const double      dB_deta_fc = (rij - mu) * dB_dx * fc;
    std::size_t ii = rfidx;
    for(int L = 0; L <= Lmax; ++L){
      for(std::size_t o = 0; o < orbitals[L].size(); ++o){
        const int lx = orbitals[L][o][0],
                  ly = orbitals[L][o][1],
                  lz = orbitals[L][o][2];
        const double poly = px[lx] * py[ly] * pz[lz];
        drhoi(ii + k) += wj * poly * dB_deta_fc;
        ii += ng;
      }
    }
    return true;
  }

  if (key == DMThetaKey::RCUTMB) {
    /* RCUTMB enters fc only; B has no direct rcut dependency. */
    const double dfc = fcut->calc_drcut(rij);
    if (dfc == 0.0) return false;
    StackOrHeapBuffer<MAX_GRID> buf(ng);
    /* B(x_c) * dfc — re-use precompute_radials with dfc playing fc's role. */
    precompute_radials(rij, dfc, sgrid, cgrid, buf.g(), ng);
    std::size_t ii = rfidx;
    for(int L = 0; L <= Lmax; ++L){
      for(std::size_t o = 0; o < orbitals[L].size(); ++o){
        const int lx = orbitals[L][o][0],
                  ly = orbitals[L][o][1],
                  lz = orbitals[L][o][2];
        const double poly = px[lx] * py[ly] * pz[lz];
        for(std::size_t c = 0; c < ng; ++c)
          drhoi(ii + c) += wj * poly * buf.g()[c];
        ii += ng;
      }
    }
    return true;
  }

  return false;
}

/* ------------------------------------------------------------------ *
 *  daed/dtheta — per-atom reducer (identical math to DM_EAD)         *
 * ------------------------------------------------------------------ */
bool DM_Blip::calc_daed_dtheta(
    DMThetaKey         key,
    int                arg_pos,
    const core::rho_type& rhoi,
    const core::rho_type& drhoi,
    core::aed_type&    daed_out,
    const double       scale) const
{
  if (key != DMThetaKey::CGRIDMB && key != DMThetaKey::SGRIDMB &&
      key != DMThetaKey::RCUTMB  && key != DMThetaKey::WATOM)
    return false;

  const std::size_t ng = cgrid.size();
  if (ng == 0) return false;

  const bool is_rcut  = (key == DMThetaKey::RCUTMB);
  const bool is_watom = (key == DMThetaKey::WATOM);
  if (!is_rcut && !is_watom) {
    if (arg_pos < 1 || static_cast<std::size_t>(arg_pos) > ng) return false;
  }

  const std::size_t k = (is_rcut || is_watom)
                        ? 0
                        : static_cast<std::size_t>(arg_pos - 1);

  std::size_t Lshift = 0;
  std::size_t ii     = rfidx;
  std::size_t dii    = drfidx;

  for(int L = 0; L <= Lmax; ++L){
    const double dfac_over_f = is_rcut ? (-static_cast<double>(L) / rc) : 0.0;
    for(std::size_t o = 0; o < orbitals[L].size(); ++o){
      const double f = fac[L][o];
      for(std::size_t c = 0; c < ng; ++c, ++ii, ++dii){
        const double rho    = rhoi(ii);
        const double Fprime = rhoi(dii);
        const double drho   = drhoi(ii);
        double contrib      = Fprime * drho;
        if (is_rcut)
          contrib += (dfac_over_f * f) * rho * rho;
        if (!is_rcut && !is_watom && c != k)
          continue;
        daed_out(c + Lshift + fidx) += scale * contrib;
      }
    }
    Lshift += ng;
  }
  return true;
}

/* ----------------------- rho_i memory ----------------------------- */
void DM_Blip::init_rhoi(core::rho_type& rhoi)
{
  rhoi.resize(2*rhoisize);
  rhoi.set_zero();
}

/* ------------------- orbital enumeration & factors ---------------- */
std::size_t DM_Blip::gen_atomic_orbitals(int Lm)
{
  orbitals.assign(Lm+1, {});   // clear inner vectors so re-calls don't accumulate
  fac     .assign(Lm+1, {});

  std::size_t cnt=0;
  for(int L=0;L<=Lm;++L){
    for(int lx=0;lx<=L;++lx){
      for(int ly=0;ly<=L-lx;++ly){
        int lz=L-lx-ly;
        orbitals[L].push_back({lx,ly,lz});
        const double f = fact_cached(L)/
                         (fact_cached(lx)*fact_cached(ly)*fact_cached(lz))/
                         pow_i06(4*rc,L);
        fac[L].push_back(f);
        ++cnt;
      }
    }
  }
  return cnt;
}

/* ------------------------------- misc ----------------------------- */
std::string DM_Blip::label() const { return lab; }

void DM_Blip::init()
{
  keys = {"CGRIDMB","SGRIDMB"};
  nparams = 2+1;
}

std::optional<DescriptorMeta> DM_Blip::meta() const {
  using DataStats = tadah::core::stats::DataStats;
  static constexpr std::size_t I_P99 = tadah::core::stats::IDX_P99;

  DescriptorMeta dm;
  dm.label = "DM_Blip";
  // Single source for the DataStats rho-percentile key (captured by the
  // cross-param check below instead of hardcoding the label literal there).
  const std::string self = dm.label;

  // RCUTMB — real-space cutoff for the MB neighbour sum.
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
  p_rcut.check    = [](const DataStats& d, const SeedView& s) {
    return cutoff_reach_check(d, s, "RCUTMB");
  };
  dm.params.push_back(std::move(p_rcut));

  // CGRIDMB — vector: centre of the i-th radial blip.
  ParamMeta p_cgrid;
  p_cgrid.key      = "CGRIDMB";
  p_cgrid.arg_pos  = 0;
  p_cgrid.scale    = Scale::Distance;
  p_cgrid.units    = "Å";
  p_cgrid.role     = "Centre of the i-th radial blip "
                     "b(SGRIDMB (r - CGRIDMB)) of the density, CGRIDMB (i). A "
                     "blip whose support holds no neighbour distance "
                     "collapses ρ.";
  p_cgrid.hard.lo  = 0.0;
  p_cgrid.crit     = Criticality::Critical;
  p_cgrid.scope    = PairScope::PerElementPair;
  p_cgrid.suggest = blip_centre_suggestion;
  p_cgrid.check    = [](const DataStats& d, const SeedView& s) {
    return radial_reach_check(d, s, BasisKind::Blip);
  };
  dm.params.push_back(std::move(p_cgrid));

  // SGRIDMB — vector: inverse width eta of the i-th blip, b(eta (r - c)).
  ParamMeta p_sgrid;
  p_sgrid.key      = "SGRIDMB";
  p_sgrid.arg_pos  = 0;
  p_sgrid.scale    = Scale::InverseDistance;
  p_sgrid.units    = "Å^-1";
  p_sgrid.role     = "Inverse width eta of the i-th radial blip "
                     "b(eta (r - c)) of the density, SGRIDMB (i), whose "
                     "support is |r - c| < 2/eta.";
  p_sgrid.hard.lo  = kHardLoPositive;
  p_sgrid.crit     = Criticality::Critical;
  p_sgrid.scope    = PairScope::Global;
  p_sgrid.suggest = blip_width_suggestion;
  dm.params.push_back(std::move(p_sgrid));

  // Cross-param: CGRIDMB.size() == SGRIDMB.size()
  dm.cross_param_checks.push_back(
    [](const DataStats&, const SeedView& s) -> HealthVerdict {
      if (!s.contains("CGRIDMB") || !s.contains("SGRIDMB")) return HealthOk{};
      if (s.size("CGRIDMB") != s.size("SGRIDMB"))
        return HealthFail{"CGRIDMB and SGRIDMB sizes differ."};
      return HealthOk{};
    });

  // Cross-param: ρ-collapse warning (uses descriptor stats if available)
  dm.cross_param_checks.push_back(
    [self](const DataStats& d, const SeedView&) -> HealthVerdict {
      auto it = d.rho_percentiles_per_descriptor.find(self);
      if (it == d.rho_percentiles_per_descriptor.end()) return HealthOk{};
      const auto& rho = it->second;
      if (rho[I_P99] < kDensityReachFloor)
        return HealthFail{"ρ collapses to ≤ 1e-6 for 99% of atoms: the MB "
                          "radial blips do not reach the neighbour shells."};
      const double ratio_p1 = rho[tadah::core::stats::IDX_P1];
      const double ratio_denom = ratio_p1 > 0 ? ratio_p1 : 1.0e-30;
      if (rho[I_P99] / ratio_denom < 1.05)
        return HealthWarn{"ρ varies < 5% across the training pool — "
                          "DM_Blip carries little information."};
      return HealthOk{};
    });

  dm.expected_columns = [](const SeedView& s) -> std::size_t {
    if (!s.contains("CGRIDMB") || !s.contains("TYPEMB")) return 0;
    const std::size_t ng = s.size("CGRIDMB");
    int Lmax = 0;
    try { Lmax = s.get<int>("TYPEMB", 1); } catch (...) { Lmax = 0; }
    return ng * static_cast<std::size_t>(Lmax + 1);
  };

  return dm;
}

} // namespace models
} // namespace tadah

