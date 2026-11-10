#ifndef DM_mEAD_HPP
#define DM_mEAD_HPP
#include <tadah/models/descriptors/dm/dm_func.h>
#include <tadah/models/descriptors/dm/dm_mEAD.h>
#include <tadah/models/descriptors/dm/dm_impl_utils.h>
#include <tadah/models/descriptors/d_basis_functions.h>
#include <tadah/models/descriptors/density_reach.h>
#include <tadah/models/descriptors/warn_once.h>

constexpr std::size_t MAX_GRID = 64;
static_assert(MAX_GRID >= 1, "MAX_GRID must be > 0");

namespace {
using namespace tadah::models;
using namespace tadah::models::detail;

inline void precompute_radials(double rij,
      double fc,
      const std::vector<double>& sgrid,
      const std::vector<double>& cgrid,
      double* g, size_t ng)
  {
    for(std::size_t c=0; c<ng; ++c){
      g [c] = G (rij, sgrid[c], cgrid[c], fc );
    }
  }
inline void precompute_radials(double rij,
      double fc, double fcp,
      const std::vector<double>& sgrid,
      const std::vector<double>& cgrid,
      double* g, double* dg, size_t ng)
  {
    for(std::size_t c=0; c<ng; ++c){
      g [c] = G (rij, sgrid[c], cgrid[c], fc );
      dg[c] = dG(rij, sgrid[c], cgrid[c], fc , fcp);
    }
  }
}   // unnamed namespace

namespace tadah {
namespace models {

template <typename F>
  DM_mEAD<F>::DM_mEAD() {
    init();
  }

/* Index ii iterates rhoi array
 * rhoi array is split into density and derivative
 * of the embedding function
 */
template <typename F>
  DM_mEAD<F>::DM_mEAD(const tadah::core::Context &context): DM_Base(context),
  rc(context.get<double>("RCUTMBMAX"))
{

  if (!context.get<bool>("INITMB")) return;
  init();


  get_grid(context,"CGRIDMB",cgrid);
  get_grid(context,"SGRIDMB",sgrid);

  D_Base::printGrid(cgrid,"CGRIDMB");
  D_Base::printGrid(sgrid,"SGRIDMB");


  core::Logger::instance().log(core::LogLevel::INFO,
      label()+" - No valid task found.");

  if (cgrid.size()!=sgrid.size()) {
    throw std::runtime_error("SGRIDMB and CGRIDMB arrays differ in size.\n");
  }

  emb = F(context, cgrid.size(), sgrid.size());

  Lmax = context.get<int>("TYPEMB",1);
  if (Lmax > 6)
    throw std::runtime_error(
        "DM_mEAD: Lmax > 6 is not supported (requested "
        + std::to_string(Lmax) + ").");

  // Once per process (log_warning_once): tadah hpo builds the descriptors
  // of every evaluation.
  if constexpr (!F::rotationally_invariant) {
    if (Lmax > 0)
      log_warning_once(
        label() + ": channel not rotationally invariant for L>0",
        label() + ": the channel function is not F_SQ, so the descriptor is "
        "not rotationally invariant for L>0 (the DM_PS_ descriptors are). "
        "Its stress is not the strain derivative of the energy (the virial "
        "is not symmetric).");
  }

  odim = emb.output_dim();
  const std::size_t ng = cgrid.size();
  const std::size_t norbs = gen_atomic_orbitals(Lmax);

  s = odim * (Lmax+1);
  rhoisize  = norbs * ng;      // ρ storage
  rhoipsize = norbs * odim;    // dF/dρ storage

  set_rfidx(0);
  set_drfidx(rhoisize);

  auto init_atoms = get_init_atoms(context);
  init_for_atoms(init_atoms);
}

/* ================================================================== *
 *  calc_aed – compute atomic energy descriptor from accumulated ρ    *
 * ================================================================== */
template <typename F>
  void DM_mEAD<F>::calc_aed(core::rho_type& rhoi, core::aed_type& aed) const
  {
    const std::size_t ng = cgrid.size();

    if constexpr (F::expands_dimensions) {
      /* --- dimension-expanding path (e.g. F_Blip, ng==1) ------------ */
      std::size_t Lshift = 0;           // offset into aed / fd_ij rows
      std::size_t ii_rho = rfidx;       // index into ρ   (one per orbital)
      std::size_t ii_drho = drfidx;     // index into dF/dρ storage

      for (int L = 0; L <= Lmax; ++L) {
        for (std::size_t o = 0; o < orbitals[L].size(); ++o) {
          const double f = fac[L][o];
          const double rho = rhoi(ii_rho);     // single ρ for this orbital
          ++ii_rho;

          for (std::size_t p = 0; p < odim; ++p, ++ii_drho) {
            const double FF  = emb.calc_F (rho, p);
            const double dFF = emb.calc_dF(rho, p);

            aed(p + Lshift + fidx) += f * FF;
            rhoi(ii_drho)           = f * dFF;
          }
        }
        Lshift += odim;
      }
    } else {
      /* --- standard path (odim == ng) ------------------------------- */
      std::size_t Lshift = 0;
      std::size_t ii     = rfidx;
      std::size_t dii    = drfidx;

      for (int L = 0; L <= Lmax; ++L) {
        for (std::size_t o = 0; o < orbitals[L].size(); ++o) {
          const double f = fac[L][o];

          for (std::size_t c = 0; c < ng; ++c, ++ii, ++dii) {
            const double rho = rhoi(ii);
            const double FF  = emb.calc_F (rho, c);
            const double dFF = emb.calc_dF(rho, c);

            aed(c + Lshift + fidx) += f * FF;
            rhoi(dii) = f * dFF;
          }
        }
        Lshift += ng;
      }
    }
  }

/* ------------------------------------------------------------------ *
 *  dX_ij/dr_i  and  dX_ji/dr_i   (pair contribution, symmetric)     *
 * ------------------------------------------------------------------ */
template <typename F>
void DM_mEAD<F>::calc_dXijdri_dXjidri(
        int Zi, int Zj, double rij, double,
        const core::Vec3d& v,
        core::rho_type&   rhoi,
        core::rho_type&   rhoj,
        core::fd_type&    fd_ij,
        double            scale) const
{
  if (!is_init_for_atoms(Zi,Zj) || rij > get_rcut()) return;

  /* --- geometry & constants --------------------------------------- */
  const double x=v[0],  y=v[1],  z=v[2];
  const double w_pair = weights[Zi][Zj];

  const double fc  = fcut->calc       (rij);
  const double fcp = fcut->calc_prime (rij);

  const double rinv = 1.0/rij;
  const std::size_t ng = cgrid.size();

  /* --- radial Gaussians ------------------------------------------- */
  StackOrHeapBuffer<MAX_GRID> buf(ng);
  precompute_radials(rij,fc,fcp,sgrid,cgrid,buf.g(),buf.dg(),ng);

  /* --- Cartesian powers and derivatives (up to L=6) ---------------- */
  double px[7], py[7], pz[7], dpx[7], dpy[7], dpz[7];
  precompute_powers(x,Lmax,px ,dpx);
  precompute_powers(y,Lmax,py ,dpy);
  precompute_powers(z,Lmax,pz ,dpz);

  if constexpr (F::expands_dimensions) {
    /* --- dimension-expanding path --------------------------------- */
    std::size_t Lshift = 0;
    std::size_t ii_drho = drfidx;  // dF/dρ storage index

    for (int L = 0; L <= Lmax; ++L)
    {
      const double s_L = (L & 1) ?  1.0 : -1.0;

      for (std::size_t o = 0; o < orbitals[L].size(); ++o)
      {
        const int lx = orbitals[L][o][0],
                  ly = orbitals[L][o][1],
                  lz = orbitals[L][o][2];

        const double pxij = px[lx], pyij = py[ly], pzij = pz[lz];
        const double pqrij = pxij*pyij*pzij;

        const double txij = dpx[lx]*pyij*pzij;
        const double tyij = dpy[ly]*pxij*pzij;
        const double tzij = dpz[lz]*pyij*pxij;

        /* single radial basis (ng==1) */
        const double g  = buf.g ()[0];
        const double dg = buf.dg()[0];

        const double Bx = g*txij + pqrij*dg*x*rinv;
        const double By = g*tyij + pqrij*dg*y*rinv;
        const double Bz = g*tzij + pqrij*dg*z*rinv;

        /* loop over embedding output channels */
        for (std::size_t p = 0; p < odim; ++p, ++ii_drho)
        {
          const double coef = scale * w_pair *
                              ( rhoi(ii_drho) - s_L * rhoj(ii_drho) );

          const std::size_t row = p + Lshift + fidx;
          fd_ij(row,0) += coef * Bx;
          fd_ij(row,1) += coef * By;
          fd_ij(row,2) += coef * Bz;
        }
      }
      Lshift += odim;
    }
  } else {
    /* --- standard path -------------------------------------------- */
    std::size_t Lshift = 0;
    std::size_t ii     = drfidx;

    for (int L = 0; L <= Lmax; ++L)
    {
      const double s_L = (L & 1) ?  1.0 : -1.0;

      for (std::size_t o = 0; o < orbitals[L].size(); ++o)
      {
        const int lx = orbitals[L][o][0],
                  ly = orbitals[L][o][1],
                  lz = orbitals[L][o][2];

        const double pxij = px[lx], pyij = py[ly], pzij = pz[lz];
        const double pqrij = pxij*pyij*pzij;

        const double txij = dpx[lx]*pyij*pzij;
        const double tyij = dpy[ly]*pxij*pzij;
        const double tzij = dpz[lz]*pyij*pxij;

        for (std::size_t c = 0; c < ng; ++c, ++ii)
        {
          const double g  = buf.g ()[c];
          const double dg = buf.dg()[c];

          const double Bx = g*txij + pqrij*dg*x*rinv;
          const double By = g*tyij + pqrij*dg*y*rinv;
          const double Bz = g*tzij + pqrij*dg*z*rinv;

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
}

/* ================================================================== *
 *  one–sided descriptor derivative     dX_ij / dr_i                  *
 * ================================================================== */
template <typename F>
void DM_mEAD<F>::calc_dXijdri(
        int Zi,int Zj,double rij,double,
        const core::Vec3d& v,
        core::rho_type&    rhoi,
        core::fd_type&     fd_ij,
        double             scale) const
{
  if (!is_init_for_atoms(Zi,Zj) || rij > get_rcut()) return;

  /* ---- geometry & cut-off ---------------------------------------- */
  const double x = v[0],  y = v[1],  z = v[2];
  const double w_pair = weights[Zi][Zj];

  const double fc  = scale*w_pair * fcut->calc       (rij);
  const double fcp = scale*w_pair * fcut->calc_prime (rij);

  const double rinv = 1.0/rij;
  const std::size_t ng = cgrid.size();

  /* ---- radial Gaussians ------------------------------------------ */
  StackOrHeapBuffer<MAX_GRID> buf(ng);
  precompute_radials(rij,fc,fcp,sgrid,cgrid,buf.g(),buf.dg(),ng);

  /* ---- Cartesian powers & derivatives ---------------------------- */
  double px[7], py[7], pz[7], dpx[7], dpy[7], dpz[7];
  precompute_powers(x,Lmax,px ,dpx);
  precompute_powers(y,Lmax,py ,dpy);
  precompute_powers(z,Lmax,pz ,dpz);

  if constexpr (F::expands_dimensions) {
    /* ---- dimension-expanding path -------------------------------- */
    std::size_t Lshift = 0;
    std::size_t ii_drho = drfidx;

    for (int L = 0; L <= Lmax; ++L)
    {
      const auto& orbs = orbitals[L];

      for (std::size_t o = 0; o < orbs.size(); ++o)
      {
        const int lx = orbs[o][0],
                  ly = orbs[o][1],
                  lz = orbs[o][2];

        const double pxij = px[lx], pyij = py[ly], pzij = pz[lz];
        const double pqrij = pxij*pyij*pzij;

        const double txij = dpx[lx]*pyij*pzij;
        const double tyij = dpy[ly]*pxij*pzij;
        const double tzij = dpz[lz]*pyij*pxij;

        /* single radial basis (ng==1) */
        const double g  = buf.g ()[0];
        const double dg = buf.dg()[0];

        const double termx = g*txij + dg*pqrij*rinv*x;
        const double termy = g*tyij + dg*pqrij*rinv*y;
        const double termz = g*tzij + dg*pqrij*rinv*z;

        /* loop over embedding output channels */
        for (std::size_t p = 0; p < odim; ++p, ++ii_drho)
        {
          const double f_r = rhoi(ii_drho);

          const std::size_t row = p + Lshift + fidx;
          fd_ij(row,0) += f_r * termx;
          fd_ij(row,1) += f_r * termy;
          fd_ij(row,2) += f_r * termz;
        }
      }
      Lshift += odim;
    }
  } else {
    /* ---- standard path ------------------------------------------- */
    std::size_t Lshift = 0;
    std::size_t ii     = drfidx;

    for (int L = 0; L <= Lmax; ++L)
    {
      const auto& orbs = orbitals[L];

      for (std::size_t o = 0; o < orbs.size(); ++o)
      {
        const int lx = orbs[o][0],
                  ly = orbs[o][1],
                  lz = orbs[o][2];

        const double pxij = px[lx],  pyij = py[ly],  pzij = pz[lz];

        const double pqrij = pxij*pyij*pzij;

        const double txij = dpx[lx]*pyij*pzij;
        const double tyij = dpy[ly]*pxij*pzij;
        const double tzij = dpz[lz]*pyij*pxij;

        const double t2x_pref = pqrij * rinv * x;
        const double t2y_pref = pqrij * rinv * y;
        const double t2z_pref = pqrij * rinv * z;

#pragma omp simd
        for (std::size_t c = 0; c < ng; ++c)
        {
          const double g  = buf.g ()[c];
          const double dg = buf.dg()[c];

          const double termx = g*txij + dg*t2x_pref;
          const double termy = g*tyij + dg*t2y_pref;
          const double termz = g*tzij + dg*t2z_pref;

          const double f_r = rhoi(ii + c);

          const std::size_t row = c + Lshift + fidx;
          fd_ij(row,0) += f_r * termx;
          fd_ij(row,1) += f_r * termy;
          fd_ij(row,2) += f_r * termz;
        }
        ii += ng;
      }
      Lshift += ng;
    }
  }
}


template <typename F>
  std::string DM_mEAD<F>::label() const {
    return lab;
  }

template <typename F>
  void DM_mEAD<F>::init_rhoi(tadah::core::rho_type& rhoi)
  {
    rhoi.resize(rhoisize+rhoipsize);
    rhoi.set_zero();
  }
template <typename F>
  void DM_mEAD<F>::calc_rho(
      int Zi,int Zj,double rij,double,const core::Vec3d& v,
      core::rho_type& rhoi,double scale) const
  {
    if(!is_init_for_atoms(Zi,Zj) || rij > get_rcut()) return;

    const double fc = scale*weights[Zi][Zj]*fcut->calc(rij);

    const std::size_t ng = cgrid.size();
    StackOrHeapBuffer<MAX_GRID> buf(ng);
    precompute_radials(rij, fc, sgrid, cgrid, buf.g(), ng);
    double px[7], py[7], pz[7];
    precompute_powers(v[0],Lmax,px);
    precompute_powers(v[1],Lmax,py);
    precompute_powers(v[2],Lmax,pz);

    std::size_t ii = rfidx;

    for(int L=0; L<=Lmax; ++L){
      const auto& orbs = orbitals[L];
      for(std::size_t o=0;o<orbs.size();++o){
        const int lx=orbs[o][0], ly=orbs[o][1], lz=orbs[o][2];
        const double poly = px[lx]*py[ly]*pz[lz];
        for(std::size_t c=0;c<ng;++c) {
          rhoi(ii++) += buf.g()[c]*poly;
        }
      }
    }
  }

template <typename F>
  size_t DM_mEAD<F>::gen_atomic_orbitals(int Lmax) {
    orbitals.assign(Lmax+1, {});   // clear inner vectors so re-calls don't accumulate
    fac.assign(Lmax+1, {});

    size_t count = 0;
    for (int L = 0; L <= Lmax; ++L) {
      for (int lx = 0; lx <= L; ++lx) {
        for (int ly = 0; ly <= L-lx; ++ly) {
          int lz = L - lx - ly;
          if (lx+ly+lz == L) {
            std::vector<int> o = {lx, ly, lz};
            orbitals[L].push_back(o);
            const double f = fact_cached(L)/(fact_cached(lx)*fact_cached(ly)*fact_cached(lz))/pow_i06(4*rc,L);
            fac[L].push_back(f);
            count++;
          }
        }
      }
    }
    return count;
  }
template <typename F>
  void DM_mEAD<F>::init() {
    keys.push_back("CGRIDMB");
    keys.push_back("SGRIDMB");
    keys.push_back("CEMBFUNC");
    keys.push_back("SEMBFUNC");
    nparams=5;
  }
/* ================================================================== *
 *  calc_drhoi_dtheta — closed-form drho_i/dtheta for the radial side
 *  (CGRIDMB[k], SGRIDMB[k], RCUTMB). Mirrors DM_EAD::calc_drhoi_dtheta
 *  byte-for-byte: the density rho is a radial-GTO sum that does NOT
 *  depend on the embedding function F, so the same chain rule covers
 *  every DM_mEAD<F> instantiation.
 * ================================================================== */
template <typename F>
bool DM_mEAD<F>::calc_drhoi_dtheta(
    DMThetaKey         key,
    int                arg_pos,
    const int          Zi,
    const int          Zj,
    const double       rij,
    const double       /*rij_sq*/,
    const tadah::core::Vec3d& v,
    tadah::core::rho_type&    drhoi,
    const double       scale) const
{
  const bool owned = (key == DMThetaKey::CGRIDMB ||
                      key == DMThetaKey::SGRIDMB ||
                      key == DMThetaKey::RCUTMB  ||
                      key == DMThetaKey::WATOM);
  if (!owned) return false;

  if (!is_init_for_atoms(Zi,Zj) || rij > get_rcut()) return false;

  if (key == DMThetaKey::WATOM) {
    /* d(rho)/d θ_WATOM = chain × poly × G(r;eta,mu) × fc(r), the
     * per-pair calc_rho contribution with the weights[Zi][Zj] factor
     * replaced by the WATOM chain factor for the current mode. */
    if (!has_watom_arg_pos(arg_pos)) return false;
    const double chain = dweight_dw_n(Zi, Zj, arg_pos);
    if (chain == 0.0) return true;

    const double fc_ij = fcut->calc(rij);
    const std::size_t ng_w = cgrid.size();
    detail::StackOrHeapBuffer<MAX_GRID> buf_w(ng_w);
    precompute_radials(rij, fc_ij, sgrid, cgrid, buf_w.g(), ng_w);

    double pxw[7], pyw[7], pzw[7];
    precompute_powers(v[0], Lmax, pxw);
    precompute_powers(v[1], Lmax, pyw);
    precompute_powers(v[2], Lmax, pzw);

    std::size_t ii_w = rfidx;
    for (int L = 0; L <= Lmax; ++L) {
      for (std::size_t o = 0; o < orbitals[L].size(); ++o) {
        const int lx = orbitals[L][o][0],
                  ly = orbitals[L][o][1],
                  lz = orbitals[L][o][2];
        const double poly = pxw[lx] * pyw[ly] * pzw[lz];
        for (std::size_t c = 0; c < ng_w; ++c)
          drhoi(ii_w + c) += scale * chain * poly * buf_w.g()[c];
        ii_w += ng_w;
      }
    }
    return true;
  }

  const double wj = weights[Zi][Zj] * scale;
  const double fc = fcut->calc(rij);
  const std::size_t ng = cgrid.size();

  double px[7], py[7], pz[7];
  precompute_powers(v[0], Lmax, px);
  precompute_powers(v[1], Lmax, py);
  precompute_powers(v[2], Lmax, pz);

  if (key == DMThetaKey::CGRIDMB) {
    if (arg_pos < 1 || static_cast<std::size_t>(arg_pos) > ng) return false;
    const std::size_t k   = static_cast<std::size_t>(arg_pos - 1);
    const double      mu  = cgrid[k];
    const double      eta = sgrid[k];
    const double      g       = std::exp(-eta * (rij - mu) * (rij - mu)) * fc;
    const double      dG_dmu  = 2.0 * eta * (rij - mu) * g;
    std::size_t ii = rfidx;
    for (int L = 0; L <= Lmax; ++L) {
      for (std::size_t o = 0; o < orbitals[L].size(); ++o) {
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
    const double      g        = std::exp(-eta * (rij - mu) * (rij - mu)) * fc;
    const double      dG_deta  = -(rij - mu) * (rij - mu) * g;
    std::size_t ii = rfidx;
    for (int L = 0; L <= Lmax; ++L) {
      for (std::size_t o = 0; o < orbitals[L].size(); ++o) {
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
    const double dfc = fcut->calc_drcut(rij);
    if (dfc == 0.0) return false;
    StackOrHeapBuffer<MAX_GRID> buf(ng);
    /* Reuse the bare radial precomputer with dfc playing fc's role. */
    precompute_radials(rij, dfc, sgrid, cgrid, buf.g(), ng);
    std::size_t ii = rfidx;
    for (int L = 0; L <= Lmax; ++L) {
      for (std::size_t o = 0; o < orbitals[L].size(); ++o) {
        const int lx = orbitals[L][o][0],
                  ly = orbitals[L][o][1],
                  lz = orbitals[L][o][2];
        const double poly = px[lx] * py[ly] * pz[lz];
        for (std::size_t c = 0; c < ng; ++c)
          drhoi(ii + c) += wj * poly * buf.g()[c];
        ii += ng;
      }
    }
    return true;
  }

  return false;
}

/* ================================================================== *
 *  calc_daed_dtheta — closed-form daed/dtheta. Handles both the
 *  expands_dimensions path (F_Blip / F_mEnv: single radial, odim
 *  outputs per orbital from the embedding-function grid) and the
 *  standard path (ng outputs per orbital, one F invocation per cgrid
 *  index). Chain rule:
 *
 *    daed_p/dtheta = f[L,o] * (dF/drho)(rho, p) * drho/dtheta
 *                  + (df[L,o]/dtheta) * F(rho, p)
 *
 *  with df[L,o]/dRCUTMB = -L/rc * f[L,o] (and zero for CGRIDMB/SGRIDMB).
 * ================================================================== */
template <typename F>
bool DM_mEAD<F>::calc_daed_dtheta(
    DMThetaKey         key,
    int                arg_pos,
    const tadah::core::rho_type& rhoi,
    const tadah::core::rho_type& drhoi,
    tadah::core::aed_type&       daed_out,
    const double       scale) const
{
  const bool radial = (key == DMThetaKey::CGRIDMB ||
                       key == DMThetaKey::SGRIDMB ||
                       key == DMThetaKey::RCUTMB);
  const bool is_watom = (key == DMThetaKey::WATOM);

  const std::size_t ng = cgrid.size();
  if (ng == 0) return false;

  if (!radial && !is_watom) {
    /* Embedding-side theta — try delegating to F. rho doesn't depend
     * on these (drhoi side comes through as zero from the walker's
     * skipped calc_drhoi_dtheta path), so the chain rule simplifies
     * to daed_p = scale * f[L,o] * dF/dtheta. F decides ownership;
     * component-local theta (CEMBFUNC[i] / SEMBFUNC[i] for F_Blip /
     * F_mEnv) return false on p != i-1. */
    /* F_Base::calc_dF_dtheta still takes std::string — build once
     * here, reuse across the (L,o,p) loop. One allocation per call. */
    const std::string theta_name_str(dm_theta_name(key));
    bool any_contrib = false;
    if constexpr (F::expands_dimensions) {
      std::size_t Lshift = 0;
      std::size_t ii_rho = rfidx;
      for (int L = 0; L <= Lmax; ++L) {
        for (std::size_t o = 0; o < orbitals[L].size(); ++o) {
          const double f   = fac[L][o];
          const double rho = rhoi(ii_rho);
          ++ii_rho;
          for (std::size_t p = 0; p < odim; ++p) {
            double dFdtheta = 0.0;
            if (!emb.calc_dF_dtheta(rho, p, theta_name_str, arg_pos, dFdtheta))
              continue;
            daed_out(p + Lshift + fidx) += scale * f * dFdtheta;
            any_contrib = true;
          }
        }
        Lshift += odim;
      }
    } else {
      /* Standard path — ng == odim, one rho per output channel. */
      std::size_t Lshift = 0;
      std::size_t ii     = rfidx;
      for (int L = 0; L <= Lmax; ++L) {
        for (std::size_t o = 0; o < orbitals[L].size(); ++o) {
          const double f = fac[L][o];
          for (std::size_t c = 0; c < ng; ++c, ++ii) {
            const double rho = rhoi(ii);
            double dFdtheta = 0.0;
            if (!emb.calc_dF_dtheta(rho, c, theta_name_str, arg_pos, dFdtheta))
              continue;
            daed_out(c + Lshift + fidx) += scale * f * dFdtheta;
            any_contrib = true;
          }
        }
        Lshift += ng;
      }
    }
    return any_contrib;
  }

  /* CGRIDMB[k]/SGRIDMB[k] are component-local and validate arg_pos
   * here. RCUTMB fans out across every c (no arg_pos to validate).
   * WATOM also fans out — its arg_pos validation against the ATOM list
   * already happened in calc_drhoi_dtheta. */
  if (radial && key != DMThetaKey::RCUTMB) {
    if (arg_pos < 1 || static_cast<std::size_t>(arg_pos) > ng) return false;
  }

  const bool        is_rcut = (key == DMThetaKey::RCUTMB);
  const std::size_t k       = (is_rcut || is_watom)
                              ? 0
                              : static_cast<std::size_t>(arg_pos - 1);

  if constexpr (F::expands_dimensions) {
    /* expanding path (F_Blip / F_mEnv): ng == 1 enforced by F's ctor,
     * so the orbital ρ-stride is 1; the aed has odim outputs per orbital. */
    std::size_t Lshift = 0;
    std::size_t ii_rho = rfidx;
    for (int L = 0; L <= Lmax; ++L) {
      /* WATOM does not enter f[L,o], so dfac=0 for it. */
      const double dfac_over_f = is_rcut ? (-static_cast<double>(L) / rc) : 0.0;
      for (std::size_t o = 0; o < orbitals[L].size(); ++o) {
        const double f    = fac[L][o];
        const double rho  = rhoi(ii_rho);
        const double drho = drhoi(ii_rho);
        ++ii_rho;
        for (std::size_t p = 0; p < odim; ++p) {
          const double FF  = emb.calc_F (rho, p);
          const double dFF = emb.calc_dF(rho, p);
          double contrib = f * dFF * drho;
          if (is_rcut) contrib += (dfac_over_f * f) * FF;
          daed_out(p + Lshift + fidx) += scale * contrib;
        }
      }
      Lshift += odim;
    }
  } else {
    /* standard path: ng outputs per orbital, one F invocation per cgrid
     * index. CGRIDMB[k]/SGRIDMB[k] are component-local (only c==k
     * contributes); RCUTMB and WATOM both fan out across every c. */
    std::size_t Lshift = 0;
    std::size_t ii     = rfidx;
    for (int L = 0; L <= Lmax; ++L) {
      const double dfac_over_f = is_rcut ? (-static_cast<double>(L) / rc) : 0.0;
      for (std::size_t o = 0; o < orbitals[L].size(); ++o) {
        const double f = fac[L][o];
        for (std::size_t c = 0; c < ng; ++c, ++ii) {
          if (!is_rcut && !is_watom && c != k) continue;
          const double rho  = rhoi(ii);
          const double drho = drhoi(ii);
          const double FF   = emb.calc_F (rho, c);
          const double dFF  = emb.calc_dF(rho, c);
          double contrib = f * dFF * drho;
          if (is_rcut) contrib += (dfac_over_f * f) * FF;
          daed_out(c + Lshift + fidx) += scale * contrib;
        }
      }
      Lshift += ng;
    }
  }
  return true;
}

/* ================================================================== *
 *  calc_dXijdri_dtheta — closed-form d(fd_ij)/dtheta for the
 *  many-body force descriptor. Mirrors calc_dXijdri's loop shape with
 *  the chain rule applied at every layer: radial (g, dg), embedding
 *  (F', F''), and the per-L multinomial prefactor f[L,o]. See the
 *  class-level doc-comment in dm_mEAD.h for the math.
 *
 *  Implementation notes:
 *   - Mirrors calc_dXijdri's scaling convention: fc, fcp are pre-scaled
 *     by (scale * weights[Zj]) before entering the orbital loop. The
 *     resulting dg/dθ, ddg/dθ inherit that scaling, so the closed-form
 *     output drops into the walker without extra multipliers.
 *   - Radial θ: needs F'' via F::calc_d2F (false ⇒ unowned for the
 *     radial branch). Embedding θ: F::calc_dFp_dtheta with rho fixed.
 *   - The walker pre-pass populates rhoi[drfidx..] = f[L,o] * F'(rho_i)
 *     (via calc_aed) and drhoi_dtheta[rfidx..] = drho_i/dθ (via
 *     calc_drhoi_dtheta). For embedding θ, drhoi_dtheta is zero and
 *     the F''*drho/dθ branch contributes 0.
 * ================================================================== */
template <typename F>
bool DM_mEAD<F>::calc_dXijdri_dtheta(
    DMThetaKey         key,
    int                arg_pos,
    const int          Zi,
    const int          Zj,
    const double       rij,
    const double       /*rij_sq*/,
    const tadah::core::Vec3d& v,
    const tadah::core::rho_type& rhoi,
    const tadah::core::rho_type& drhoi_dtheta,
    tadah::core::fd_type& fd_ij,
    const double       scale) const
{
  const bool radial = (key == DMThetaKey::CGRIDMB ||
                       key == DMThetaKey::SGRIDMB ||
                       key == DMThetaKey::RCUTMB);
  const bool is_rcut  = (key == DMThetaKey::RCUTMB);
  const bool is_watom = (key == DMThetaKey::WATOM);
  /* CEMBFUNC / SEMBFUNC / RCUTENV are F-side. Any other key is
   * unowned. */
  const bool embedding = (key == DMThetaKey::CEMBFUNC ||
                          key == DMThetaKey::SEMBFUNC ||
                          key == DMThetaKey::RCUTENV);
  if (!radial && !embedding && !is_watom) return false;

  if (!is_init_for_atoms(Zi, Zj) || rij > get_rcut()) return false;

  const std::size_t ng = cgrid.size();
  if (ng == 0) return false;

  if (radial && !is_rcut) {
    if (arg_pos < 1 || static_cast<std::size_t>(arg_pos) > ng) return false;
  }

  /* WATOM resolution: validate arg_pos against current watom_mode.
   * Out-of-range arg_pos is a configuration error — route to FD. */
  double w_chain = 0.0;
  if (is_watom) {
    if (!has_watom_arg_pos(arg_pos)) return false;
    w_chain = dweight_dw_n(Zi, Zj, arg_pos);
  }

  const double x  = v[0], y = v[1], z = v[2];
  const double wj = weights[Zi][Zj];
  const double sc_wj = scale * wj;

  const double fc_unscaled  = fcut->calc       (rij);
  const double fcp_unscaled = fcut->calc_prime (rij);
  const double fc  = sc_wj * fc_unscaled;   /* matches calc_dXijdri */
  const double fcp = sc_wj * fcp_unscaled;
  const double rinv = 1.0 / rij;

  /* Radial pre-pass: g, dg at every c, plus dg/dθ, ddg/dθ for the
   * radial branch (zero for embedding-θ; we still allocate the buffers
   * to keep the loop body uniform). */
  detail::StackOrHeapBuffer<MAX_GRID> buf(ng);
  /* Hand-rolled per-c θ-derivative scratch (small, lives on stack via
   * std::array — bounded by MAX_GRID = 64). */
  std::array<double, 64> dg_dtheta_arr{};
  std::array<double, 64> ddg_dtheta_arr{};
  for (std::size_t c = 0; c < ng; ++c) {
    dg_dtheta_arr[c]  = 0.0;
    ddg_dtheta_arr[c] = 0.0;
  }

  precompute_radials(rij, fc, fcp, sgrid, cgrid, buf.g(), buf.dg(), ng);

  if (is_watom && w_chain != 0.0) {
    /* d(g_full)/dθ_WATOM = scale * chain * E * fc_unscaled
     *    (the scale*weights[Zi][Zj] factor in buf.g differentiates via
     *     the WATOM chain factor for the current mode)
     * d(dg_full)/dθ_WATOM = scale * chain * E * (-2*eta*rm*fc_unscaled + fcp_unscaled)
     * Fans out across all c. For chain == 0 the arrays stay zero. */
    for (std::size_t c = 0; c < ng; ++c) {
      const double eta = sgrid[c];
      const double mu  = cgrid[c];
      const double rm  = rij - mu;
      const double E   = std::exp(-eta * rm * rm);
      dg_dtheta_arr[c]  = scale * w_chain * E * fc_unscaled;
      ddg_dtheta_arr[c] = scale * w_chain * E * (-2.0 * eta * rm * fc_unscaled + fcp_unscaled);
    }
  }

  if (radial) {
    const std::size_t k = is_rcut ? 0
                                  : static_cast<std::size_t>(arg_pos - 1);

    if (key == DMThetaKey::CGRIDMB) {
      const double eta = sgrid[k];
      const double mu  = cgrid[k];
      const double rm  = rij - mu;
      const double g_k  = buf.g ()[k];
      const double dg_k = buf.dg()[k];
      /* d/dmu of E*fc = (2*eta*rm) * E*fc = 2*eta*rm * g_k.
       * d/dmu of E*(-2*eta*rm*fc + fcp) =
       *   dE/dmu * X + E * dX/dmu
       *   = 2*eta*rm * dg_k + 2*eta * g_k     (E*fc = g_k, E*X = dg_k) */
      dg_dtheta_arr[k]  = 2.0 * eta * rm * g_k;
      ddg_dtheta_arr[k] = 2.0 * eta * (rm * dg_k + g_k);
    } else if (key == DMThetaKey::SGRIDMB) {
      const double mu  = cgrid[k];
      const double rm  = rij - mu;
      const double g_k  = buf.g ()[k];
      const double dg_k = buf.dg()[k];
      /* d/deta of E*fc = -rm^2 * g_k.
       * d/deta of E*X (X = -2 eta rm fc + fcp) =
       *   -rm^2 * dg_k + E * (-2 rm fc)
       *   = -rm^2 * dg_k - 2 rm * g_k          (E*fc = g_k) */
      dg_dtheta_arr[k]  = -rm * rm * g_k;
      ddg_dtheta_arr[k] = -rm * rm * dg_k - 2.0 * rm * g_k;
    } else { /* RCUTMB */
      const double dfc  = sc_wj * fcut->calc_drcut(rij);
      const double dfcp = sc_wj * fcut->calc_prime_drcut(rij);
      if (dfc == 0.0 && dfcp == 0.0) {
        /* Discontinuous cutoff (Cut_Dummy) — analytical RCUTMB chain
         * collapses to zero everywhere except r=rcut. Return false so
         * the orchestrator routes to FD. */
        return false;
      }
      for (std::size_t c = 0; c < ng; ++c) {
        const double eta = sgrid[c];
        const double mu  = cgrid[c];
        const double rm  = rij - mu;
        const double E   = std::exp(-eta * rm * rm);
        dg_dtheta_arr[c]  = E * dfc;
        ddg_dtheta_arr[c] = E * (-2.0 * eta * rm * dfc + dfcp);
      }
    }
  }

  /* Cartesian powers and gradients (parallel to calc_dXijdri). */
  double px[7], py[7], pz[7], dpx[7], dpy[7], dpz[7];
  detail::precompute_powers(x, Lmax, px, dpx);
  detail::precompute_powers(y, Lmax, py, dpy);
  detail::precompute_powers(z, Lmax, pz, dpz);

  /* F_Base::calc_dFp_dtheta still takes std::string; build once for
   * the embedding branch and reuse across (L,o,p). One allocation
   * per kernel call, hit only when this kernel handles an embedding θ. */
  const std::string theta_name_str = embedding
                                     ? std::string(dm_theta_name(key))
                                     : std::string{};

  bool any_contrib = false;

  if constexpr (F::expands_dimensions) {
    /* Expanding path (F_Blip / F_mEnv, ng==1, odim outputs per orbital). */
    std::size_t Lshift   = 0;
    std::size_t ii_rho   = rfidx;       /* index into rho (one per orbital) */
    std::size_t ii_drho  = drfidx;      /* index into rhoi[F'(rho)] storage */

    for (int L = 0; L <= Lmax; ++L) {
      const double dfac_over_f = (radial && is_rcut)
                                 ? (-static_cast<double>(L) / rc)
                                 : 0.0;

      for (std::size_t o = 0; o < orbitals[L].size(); ++o) {
        const int lx = orbitals[L][o][0],
                  ly = orbitals[L][o][1],
                  lz = orbitals[L][o][2];

        const double pxij = px[lx], pyij = py[ly], pzij = pz[lz];
        const double pqrij = pxij * pyij * pzij;
        const double txij  = dpx[lx] * pyij * pzij;
        const double tyij  = dpy[ly] * pxij * pzij;
        const double tzij  = dpz[lz] * pyij * pxij;

        const double f   = fac[L][o];
        const double rho = rhoi(ii_rho);
        const double drho = (radial || is_watom) ? drhoi_dtheta(ii_rho) : 0.0;
        ++ii_rho;

        /* Single radial (ng==1). */
        const double g_0  = buf.g ()[0];
        const double dg_0 = buf.dg()[0];
        const double Bx_0 = g_0 * txij + dg_0 * pqrij * rinv * x;
        const double By_0 = g_0 * tyij + dg_0 * pqrij * rinv * y;
        const double Bz_0 = g_0 * tzij + dg_0 * pqrij * rinv * z;

        double dg_t_0 = 0.0, ddg_t_0 = 0.0;
        if (radial || is_watom) {
          dg_t_0  = dg_dtheta_arr [0];
          ddg_t_0 = ddg_dtheta_arr[0];
        }
        const double dBx_0 = dg_t_0 * txij + ddg_t_0 * pqrij * rinv * x;
        const double dBy_0 = dg_t_0 * tyij + ddg_t_0 * pqrij * rinv * y;
        const double dBz_0 = dg_t_0 * tzij + ddg_t_0 * pqrij * rinv * z;

        for (std::size_t p = 0; p < odim; ++p, ++ii_drho) {
          /* rhoi(ii_drho) = f * F'(rho, p)  (populated by calc_aed). */
          const double f_Fp = rhoi(ii_drho);
          /* d(f_Fp)/dθ = (df/dθ) * F' + f * F''(rho) * drho/dθ  (radial/WATOM)
           *            = f * (∂F'/∂θ|rho fixed)                  (embedding) */
          double d_f_Fp_dtheta = 0.0;

          if (radial || is_watom) {
            /* F' chain term: f * F''(rho) * drho/dθ. Always attempt the
             * F'' lookup so the function fails fast when F has no
             * analytical F'' (e.g. F_mEnv, F_RLR). */
            double d2F = 0.0;
            if (!emb.calc_d2F(rho, p, d2F)) return false;
            if (drho != 0.0) {
              d_f_Fp_dtheta += f * d2F * drho;
            }
            /* RCUTMB-only fac-term: (df/dRCUTMB) * F'. WATOM doesn't
             * enter f[L,o], so this fires only for RCUTMB. */
            if (is_rcut) {
              const double Fp = emb.calc_dF(rho, p);
              d_f_Fp_dtheta += (dfac_over_f * f) * Fp;
            }
          } else { /* embedding */
            double dFp_dtheta = 0.0;
            if (!emb.calc_dFp_dtheta(rho, p, theta_name_str, arg_pos, dFp_dtheta))
              continue;
            d_f_Fp_dtheta = f * dFp_dtheta;
          }

          any_contrib = true;

          const std::size_t row = p + Lshift + fidx;
          if (radial || is_watom) {
            /* d(fd_ij)/dθ =
             *      (d(f_Fp)/dθ) * term_k          (F'/fac chain)
             *    + f_Fp * d(term_k)/dθ            (radial g/dg chain or
             *                                      WATOM direct-factor)
             *
             * Always include both branches — the f_Fp * d(term_k)/dθ
             * piece contributes even when drho == 0 (e.g. orbital with
             * vanishing polynomial at this cluster geometry, or WATOM
             * with Zj == Z_target on a pair where drho_i happens to be
             * zero for this orbital). */
            fd_ij(row, 0) += d_f_Fp_dtheta * Bx_0 + f_Fp * dBx_0;
            fd_ij(row, 1) += d_f_Fp_dtheta * By_0 + f_Fp * dBy_0;
            fd_ij(row, 2) += d_f_Fp_dtheta * Bz_0 + f_Fp * dBz_0;
          } else {
            /* d(term_k)/dθ_emb = 0; only the F' chain contributes. */
            fd_ij(row, 0) += d_f_Fp_dtheta * Bx_0;
            fd_ij(row, 1) += d_f_Fp_dtheta * By_0;
            fd_ij(row, 2) += d_f_Fp_dtheta * Bz_0;
          }
        }
      }
      Lshift += odim;
    }
  } else {
    /* Standard path (ng == odim, one rho per output channel). For
     * radial θ != RCUTMB, the chain is component-local on c == k;
     * RCUTMB fans out. */
    std::size_t Lshift = 0;
    std::size_t ii     = rfidx;
    std::size_t dii    = drfidx;

    /* For radial theta, k is the component index (0-based). */
    const std::size_t kc = (radial && !is_rcut)
                           ? static_cast<std::size_t>(arg_pos - 1)
                           : 0;

    for (int L = 0; L <= Lmax; ++L) {
      const double dfac_over_f = (radial && is_rcut)
                                 ? (-static_cast<double>(L) / rc)
                                 : 0.0;

      for (std::size_t o = 0; o < orbitals[L].size(); ++o) {
        const int lx = orbitals[L][o][0],
                  ly = orbitals[L][o][1],
                  lz = orbitals[L][o][2];

        const double pxij = px[lx], pyij = py[ly], pzij = pz[lz];
        const double pqrij = pxij * pyij * pzij;
        const double txij  = dpx[lx] * pyij * pzij;
        const double tyij  = dpy[ly] * pxij * pzij;
        const double tzij  = dpz[lz] * pyij * pxij;

        const double f = fac[L][o];

        for (std::size_t c = 0; c < ng; ++c, ++ii, ++dii) {
          const double rho  = rhoi(ii);
          const double drho = (radial || is_watom) ? drhoi_dtheta(ii) : 0.0;

          const double g_c  = buf.g ()[c];
          const double dg_c = buf.dg()[c];
          const double Bx_c = g_c * txij + dg_c * pqrij * rinv * x;
          const double By_c = g_c * tyij + dg_c * pqrij * rinv * y;
          const double Bz_c = g_c * tzij + dg_c * pqrij * rinv * z;

          const double f_Fp = rhoi(dii);
          double d_f_Fp_dtheta = 0.0;

          double dg_t_c = 0.0, ddg_t_c = 0.0;
          if (radial || is_watom) {
            /* Component-local for CGRIDMB[k]/SGRIDMB[k] — only c==kc
             * touches dg/dθ. RCUTMB and WATOM fan out across all c. */
            if (is_rcut || is_watom || c == kc) {
              dg_t_c  = dg_dtheta_arr [c];
              ddg_t_c = ddg_dtheta_arr[c];
            }
            double d2F = 0.0;
            if (!emb.calc_d2F(rho, c, d2F)) return false;
            if (drho != 0.0) {
              d_f_Fp_dtheta += f * d2F * drho;
            }
            if (is_rcut) {
              const double Fp = emb.calc_dF(rho, c);
              d_f_Fp_dtheta += (dfac_over_f * f) * Fp;
            }
          } else {
            double dFp_dtheta = 0.0;
            if (!emb.calc_dFp_dtheta(rho, c, theta_name_str, arg_pos, dFp_dtheta))
              continue;
            d_f_Fp_dtheta = f * dFp_dtheta;
          }

          const double dBx_c = dg_t_c * txij + ddg_t_c * pqrij * rinv * x;
          const double dBy_c = dg_t_c * tyij + ddg_t_c * pqrij * rinv * y;
          const double dBz_c = dg_t_c * tzij + ddg_t_c * pqrij * rinv * z;

          /* For radial CGRIDMB[k]/SGRIDMB[k] with c != kc, neither dg/dθ
           * nor df/dθ contributes; if drho is also zero, this c
           * contributes nothing — skip. Does not apply to WATOM (all c
           * receive dg_t_c contributions when Zj == Z_target). */
          if (radial && !is_rcut && c != kc
              && d_f_Fp_dtheta == 0.0) continue;
          any_contrib = true;

          const std::size_t row = c + Lshift + fidx;
          fd_ij(row, 0) += d_f_Fp_dtheta * Bx_c + f_Fp * dBx_c;
          fd_ij(row, 1) += d_f_Fp_dtheta * By_c + f_Fp * dBy_c;
          fd_ij(row, 2) += d_f_Fp_dtheta * Bz_c + f_Fp * dBz_c;
        }
      }
      Lshift += ng;
    }
  }

  return any_contrib;
}

/* ================================================================== *
 *  meta() — default templated body: covers the parameter shape every
 *  DM_mEAD<F> instantiation shares (RCUTMB + CGRIDMB + SGRIDMB), plus
 *  the per-param ρ-collapse check on CGRIDMB. F-specific extras
 *  (CEMBFUNC/SEMBFUNC for F_RLR, RCUTENV/CEMBFUNC/SEMBFUNC for
 *  F_mEnv<Cut_*>) are added by explicit specialisations declared in
 *  dm_all.cpp; those specialisations call back into the templated
 *  body to share the common parameter declarations.
 * ================================================================== */
template <typename F>
std::optional<DescriptorMeta> DM_mEAD<F>::meta() const {
  using DataStats = tadah::core::stats::DataStats;

  DescriptorMeta dm;
  dm.label = label();   // runtime label string (e.g. "DM_mRLR")

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
  p_cgrid.role     = "GTO centre r_s of exp(-SGRIDMB (r - CGRIDMB)^2). A GTO below 1e-6 at every neighbour distance collapses ρ.";
  p_cgrid.hard.lo  = 0.0;
  p_cgrid.crit     = Criticality::Critical;
  p_cgrid.scope    = PairScope::PerElementPair;
  p_cgrid.suggest  = gaussian_centre_suggestion;
  p_cgrid.check    = [](const DataStats& d, const SeedView& s) {
    return radial_reach_check(d, s, BasisKind::Gaussian);
  };
  dm.params.push_back(std::move(p_cgrid));

  ParamMeta p_sgrid;
  p_sgrid.key      = "SGRIDMB";
  p_sgrid.arg_pos  = 0;
  p_sgrid.scale    = Scale::InverseDistanceSq;
  p_sgrid.units    = "Å^-2";
  p_sgrid.role     = "Inverse Gaussian width η.";
  p_sgrid.hard.lo  = kHardLoPositive;
  p_sgrid.crit     = Criticality::Critical;
  p_sgrid.scope    = PairScope::Global;
  p_sgrid.suggest  = gaussian_width_suggestion;
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
