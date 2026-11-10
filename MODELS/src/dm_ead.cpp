#include <tadah/models/descriptors/dm/dm_func.h>
#include <tadah/models/descriptors/dm/dm_ead.h>
#include <tadah/models/descriptors/d_basis_functions.h>
#include <tadah/models/descriptors/param_meta.h>
#include <tadah/models/descriptors/density_reach.h>
#include <tadah/core/stats/data_stats.h>

#include <array>
#include <memory>
#include <vector>
#include <cmath>
#include <stdexcept>

constexpr std::size_t MAX_GRID = 64;      // tune to your input deck
static_assert(MAX_GRID>=1,"MAX_GRID must be > 0");

namespace {
using namespace tadah::models;

/* ------------------------------------------------------------------ *
 *  Fast, stack/heap scratch buffer (for G and dG/dr)                 *
 * ------------------------------------------------------------------ */
template<std::size_t N = 64>
class StackOrHeapBuffer {
public:
  explicit StackOrHeapBuffer(std::size_t n)
  : sz(n), use_heap(n>N), heap(use_heap?new double[2*n]:nullptr) {}

  double* g ()       { return use_heap?  heap.get()      : stack;     }
  double* dg()       { return use_heap? &heap.get()[sz] : &stack[N]; }
  std::size_t size() const { return sz; }

private:
  std::size_t              sz;
  bool                     use_heap;
  std::unique_ptr<double[]> heap;
  alignas(64) double       stack[2*N];
};

/* -------- factorials & small integer powers (0…6) ----------------- */
constexpr std::array<double,7> FACT_TBL = {1.,1.,2.,6.,24.,120.,720.};

inline double fact_cached(int n)      { return FACT_TBL[n]; }

inline double pow_i06(double x,int n)
{
  switch(n){
    case 0: return 1.;
    case 1: return x;
    case 2: return x*x;
    case 3: return x*x*x;
    case 4: { double x2=x*x; return x2*x2; }
    case 5: { double x2=x*x; return x2*x2*x; }
    case 6: { double x2=x*x; return x2*x2*x2; }
    default:return std::pow(x,n);
  }
}

/* -------- Cartesian powers and their derivatives ------------------ */
inline void precompute_powers(double x,int L,double p[7])
{
  p[0]=1.; for(int n=1;n<=L;++n) p[n]=p[n-1]*x;
#if defined(__SANITIZE_ADDRESS__)||defined(ADDRESS_SANITIZER)
  for(int n=L+1;n<=6;++n) p[n]=0.;
#endif
}
inline void precompute_powers(double x,int L,double p[7],double dp[7])
{
  p[0]=1.; dp[0]=0.;
  for(int n=1;n<=L;++n){ p[n]=p[n-1]*x; dp[n]=n*p[n-1]; }
#if defined(__SANITIZE_ADDRESS__)||defined(ADDRESS_SANITIZER)
  for(int n=L+1;n<=6;++n){ p[n]=0.; dp[n]=0.; }
#endif
}

/* -------- pre-compute Gaussian (and derivative) for a whole grid --- */
inline void precompute_radials(double r,double fc,
                               const std::vector<double>& sg,
                               const std::vector<double>& cg,
                               double *g,std::size_t n)
{
  for(std::size_t i=0;i<n;++i) g[i]=G(r,sg[i],cg[i],fc);
}
inline void precompute_radials(double r,double fc,double fcp,
                               const std::vector<double>& sg,
                               const std::vector<double>& cg,
                               double *g,double* dg,std::size_t n)
{
  for(std::size_t i=0;i<n;++i){
    g [i]=G (r,sg[i],cg[i],fc );
    dg[i]=dG(r,sg[i],cg[i],fc,fcp);
  }
}

/* sign-flip without run-time branch */
inline double neg(double v,int n){ return (n&1)?-v:v; }

} // unnamed namespace


/* ================================================================== *
 *  DM_EAD  (ρ² embedding)                                           *
 * ================================================================== */
namespace tadah {
namespace models {
    

DM_EAD::DM_EAD() { init(); }

/* ------------------------------------------------------------------ */
DM_EAD::DM_EAD(const core::Context& ctx)
: DM_Base(ctx)
, rc(ctx.get<double>("RCUTMBMAX"))
{
  if(!ctx.get<bool>("INITMB"))
    throw std::runtime_error("INITMB=false; cannot initialise "+label()+"\n");

  init();

  get_grid(ctx,"CGRIDMB",cgrid);
  get_grid(ctx,"SGRIDMB",sgrid);
  D_Base::printGrid(cgrid,"CGRIDMB");
  D_Base::printGrid(sgrid,"SGRIDMB");

  if(cgrid.size()!=sgrid.size())
    throw std::runtime_error("SGRIDMB and CGRIDMB array sizes differ!");

  Lmax = ctx.get<int>("TYPEMB",1);
  if(Lmax>6)
    throw std::runtime_error("DM_EAD: Lmax>6 not supported (requested "+
                             std::to_string(Lmax)+").");

  s         = cgrid.size()*(Lmax+1);
  rhoisize  = gen_atomic_orbitals(Lmax)*cgrid.size();
  set_rfidx(0);
  set_drfidx(rhoisize);

  init_for_atoms(get_init_atoms(ctx));
}

/* ------------------------------------------------------------------ *
 *  embedding energy + derivative store                               *
 * ------------------------------------------------------------------ */
void DM_EAD::calc_aed(core::rho_type& rhoi, core::aed_type& aed) const
{
  std::size_t Lshift = 0;
  std::size_t ii     = rfidx;          // start of density part
  std::size_t dii    = drfidx;         // start of derivative part

  for(int L=0; L<=Lmax; ++L){
    const std::size_t ng = cgrid.size();
    for(std::size_t o=0;o<orbitals[L].size();++o){
      const double f = fac[L][o];

      for(std::size_t c=0;c<ng;++c,++ii,++dii){
        const double rho =  rhoi(ii);
        aed (c+Lshift+fidx) += f * rho * rho;   // F(ρ)=ρ²
        rhoi(dii)  = 2.0 * f * rho;   // F′(ρ)=2ρ
      }
    }
    Lshift += ng;
  }
}

/* ------------------------------------------------------------------ *
 *  dX_ij / dr_i   &   dX_ji / dr_i  (pair contribution, symmetric)
 *  --- single-evaluation version (no duplicate j-branch) ------------
 * ------------------------------------------------------------------ */
void DM_EAD::calc_dXijdri_dXjidri(
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

/* ------------------------------------------------------------------ *
 *  dX_ij/dr_i  (only one-sided, used e.g. for forces on atom i)      *
 * ------------------------------------------------------------------ */
void DM_EAD::calc_dXijdri(
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

/* ------------------------------------------------------------------ *
 *  neighbour-density accumulation                                    *
 * ------------------------------------------------------------------ */
void DM_EAD::calc_rho(int Zi,int Zj,double rij,double,
                      const core::Vec3d& v,
                      core::rho_type&    rhoi,
                      double             scale) const
{
  if(!is_init_for_atoms(Zi,Zj) || rij>get_rcut()) return;

  const double fc = scale*weights[Zi][Zj]*fcut->calc(rij);

  const std::size_t ng = cgrid.size();
  StackOrHeapBuffer<MAX_GRID> buf(ng);
  precompute_radials(rij,fc,sgrid,cgrid,buf.g(),ng);

  double px[7],py[7],pz[7];
  precompute_powers(v[0],Lmax,px);
  precompute_powers(v[1],Lmax,py);
  precompute_powers(v[2],Lmax,pz);

  std::size_t ii = rfidx;

  for(int L=0; L<=Lmax; ++L){
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
bool DM_EAD::calc_drhoi_dtheta(
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
  /* Cheap key short-circuit before any pair-level math. */
  const bool owned = (key == DMThetaKey::CGRIDMB ||
                      key == DMThetaKey::SGRIDMB ||
                      key == DMThetaKey::RCUTMB  ||
                      key == DMThetaKey::WATOM);
  if (!owned) return false;

  if(!is_init_for_atoms(Zi,Zj) || rij>get_rcut()) return false;

  if (key == DMThetaKey::WATOM) {
    /* d(rho)/d θ_WATOM = chain × poly × G(r; eta, mu) × fc(r), the
     * per-pair calc_rho contribution with the weights[Zi][Zj] factor
     * replaced by the WATOM chain factor for the current mode. */
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
    /* dG/dmu = 2 eta (r - mu) G(r; eta, mu) — fc already factored. */
    const double      g       = std::exp(-eta * (rij - mu) * (rij - mu)) * fc;
    const double      dG_dmu  = 2.0 * eta * (rij - mu) * g;
    /* Walk (L, o), only the c==k column is non-zero. */
    std::size_t ii = rfidx;
    for(int L = 0; L <= Lmax; ++L){
      for(std::size_t o = 0; o < orbitals[L].size(); ++o){
        const int lx = orbitals[L][o][0],
                  ly = orbitals[L][o][1],
                  lz = orbitals[L][o][2];
        const double poly = px[lx] * py[ly] * pz[lz];
        drhoi(ii + k) += wj * poly * dG_dmu;
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
    /* dG/deta = -(r - mu)^2 G. */
    const double      g        = std::exp(-eta * (rij - mu) * (rij - mu)) * fc;
    const double      dG_deta  = -(rij - mu) * (rij - mu) * g;
    std::size_t ii = rfidx;
    for(int L = 0; L <= Lmax; ++L){
      for(std::size_t o = 0; o < orbitals[L].size(); ++o){
        const int lx = orbitals[L][o][0],
                  ly = orbitals[L][o][1],
                  lz = orbitals[L][o][2];
        const double poly = px[lx] * py[ly] * pz[lz];
        drhoi(ii + k) += wj * poly * dG_deta;
        ii += ng;
      }
    }
    return true;
  }

  if (key == DMThetaKey::RCUTMB) {
    /* RCUTMB enters through fc only; G's exponent does not depend on rcut.
     * dG_bare/drcut = exp(-eta(r-mu)^2) * dfc/drcut, fans out across all c. */
    const double dfc = fcut->calc_drcut(rij);
    if (dfc == 0.0) return false;
    StackOrHeapBuffer<MAX_GRID> buf(ng);
    /* Re-use the radial precomputer but with dfc playing the role of fc. */
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

  return false;  /* key not owned by DM_EAD. */
}

/* ------------------------------------------------------------------ *
 *  daed/dtheta — per-atom reducer (analog of calc_aed)               *
 * ------------------------------------------------------------------ */
bool DM_EAD::calc_daed_dtheta(
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
  /* CGRIDMB[k] / SGRIDMB[k] are component-local and need arg_pos in
   * [1, ng]. RCUTMB / WATOM fan out across all c — RCUTMB has no
   * arg_pos to validate; WATOM's arg_pos validation already happened
   * in calc_drhoi_dtheta. */
  if (!is_rcut && !is_watom) {
    if (arg_pos < 1 || static_cast<std::size_t>(arg_pos) > ng) return false;
  }

  const std::size_t k = (is_rcut || is_watom)
                        ? 0
                        : static_cast<std::size_t>(arg_pos - 1);

  /* Walk (L, o, c) mirroring calc_aed's index pattern. */
  std::size_t Lshift = 0;
  std::size_t ii     = rfidx;          /* ρ part of rhoi */
  std::size_t dii    = drfidx;         /* F'(ρ) part of rhoi (= 2 f ρ) */

  for(int L = 0; L <= Lmax; ++L){
    /* WATOM doesn't enter the multinomial prefactor f, so dfac=0. */
    const double dfac_over_f = is_rcut ? (-static_cast<double>(L) / rc) : 0.0;
    for(std::size_t o = 0; o < orbitals[L].size(); ++o){
      const double f = fac[L][o];
      for(std::size_t c = 0; c < ng; ++c, ++ii, ++dii){
        const double rho      = rhoi(ii);
        const double Fprime   = rhoi(dii);          /* = 2 f rho */
        const double drho     = drhoi(ii);          /* dρ/dθ at (L,o,c) */
        double contrib        = Fprime * drho;
        if (is_rcut)
          contrib += (dfac_over_f * f) * rho * rho;
        if (!is_rcut && !is_watom && c != k)
          continue;                                  /* component-local */
        daed_out(c + Lshift + fidx) += scale * contrib;
      }
    }
    Lshift += ng;
  }
  return true;
}

/* ------------------------------------------------------------------ *
 *  allocate & zero rho_i                                             *
 * ------------------------------------------------------------------ */
void DM_EAD::init_rhoi(core::rho_type& rhoi)
{
  rhoi.resize(2*rhoisize);
  rhoi.set_zero();
}

/* ------------------------------------------------------------------ *
 *  multinomial prefactors & orbital list (unique, L≤6)               *
 * ------------------------------------------------------------------ */
std::size_t DM_EAD::gen_atomic_orbitals(int Lmax)
{
  orbitals.assign(Lmax+1, {});   // clear inner vectors so re-calls don't accumulate
  fac     .assign(Lmax+1, {});

  std::size_t count=0;
  for(int L=0;L<=Lmax;++L){
    for(int lx=0;lx<=L;++lx){
      for(int ly=0;ly<=L-lx;++ly){
        int lz=L-lx-ly;
        std::vector<int> orb{lx,ly,lz};
        orbitals[L].push_back(orb);

        const double f = fact_cached(L) /
                         (fact_cached(lx)*fact_cached(ly)*fact_cached(lz)) /
                         pow_i06(4*rc,L);
        fac[L].push_back(f);
        ++count;
      }
    }
  }
  return count;
}

/* ------------------------------------------------------------------ */
std::string DM_EAD::label() const { return lab; }

/* ------------------------------------------------------------------ */
void DM_EAD::init()
{
  keys = {"CGRIDMB","SGRIDMB"};
  nparams = 2+1;  // (two grids + r_c)
}
// ----------------------------------------------------------------
// meta() — DM_EAD shares the (RCUTMB, CGRIDMB, SGRIDMB) parameter
// shape with DM_mBlipEnv: per-pair radial GTO with
// centre/width vectors. The ρ-collapse cross-param check applies to
// the same per-descriptor key (`DM_EAD`) — the suggester anchors all
// numbers to the dataset's r_ij percentiles.
// ----------------------------------------------------------------
std::optional<DescriptorMeta> DM_EAD::meta() const {
  using DataStats = tadah::core::stats::DataStats;

  DescriptorMeta dm;
  dm.label = "DM_EAD";

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
  p_rcut.check = [](const DataStats& d, const SeedView& s) {
    return cutoff_reach_check(d, s, "RCUTMB");
  };
  dm.params.push_back(std::move(p_rcut));

  // CGRIDMB — vector of GTO centres (r_s in the EAD formula).
  ParamMeta p_cgrid;
  p_cgrid.key      = "CGRIDMB";
  p_cgrid.arg_pos  = 0;
  p_cgrid.scale    = Scale::Distance;
  p_cgrid.units    = "Å";
  p_cgrid.role     = "GTO centre r_s of exp(-SGRIDMB (r - CGRIDMB)^2). A GTO below 1e-6 at every neighbour distance collapses ρ.";
  p_cgrid.hard.lo  = 0.0;
  p_cgrid.crit     = Criticality::Critical;
  p_cgrid.scope    = PairScope::PerElementPair;
  p_cgrid.suggest  = gaussian_centre_suggestion;
  p_cgrid.check    = [](const DataStats& d, const SeedView& s) {
    return radial_reach_check(d, s, BasisKind::Gaussian);
  };
  dm.params.push_back(std::move(p_cgrid));

  // SGRIDMB — vector of GTO inverse widths (η).
  ParamMeta p_sgrid;
  p_sgrid.key      = "SGRIDMB";
  p_sgrid.arg_pos  = 0;
  p_sgrid.scale    = Scale::InverseDistanceSq;
  p_sgrid.units    = "Å^-2";
  p_sgrid.role     = "Inverse Gaussian width η. Narrow GTOs combined with off-shell centres collapse ρ to 0.";
  p_sgrid.hard.lo  = kHardLoPositive;
  p_sgrid.crit     = Criticality::Critical;
  p_sgrid.scope    = PairScope::Global;
  p_sgrid.suggest  = gaussian_width_suggestion;
  dm.params.push_back(std::move(p_sgrid));

  // Cross-param: CGRIDMB and SGRIDMB sizes must agree.
  dm.cross_param_checks.push_back(
    [](const DataStats&, const SeedView& s) -> HealthVerdict {
      if (!s.contains("CGRIDMB") || !s.contains("SGRIDMB")) return HealthOk{};
      if (s.size("CGRIDMB") != s.size("SGRIDMB"))
        return HealthFail{"CGRIDMB and SGRIDMB sizes differ."};
      return HealthOk{};
    });

  // expected_columns: |CGRIDMB| × (Lmax + 1).
  dm.expected_columns = [](const SeedView& s) -> std::size_t {
    if (!s.contains("CGRIDMB")) return 0;
    const std::size_t n = s.size("CGRIDMB");
    int Lmax = 0;
    if (s.contains("TYPEMB")) {
      try { Lmax = s.context().get<int>("TYPEMB", 1); } catch (...) {}
    }
    return n * static_cast<std::size_t>(Lmax + 1);
  };

  return dm;
}

} // namespace models
} // namespace tadah

