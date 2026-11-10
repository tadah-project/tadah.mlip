#include "tadah/models/descriptors/d2/d2_base.h"
#include <tadah/models/descriptors/d2/d2_lj.h>
#include <tadah/models/descriptors/closed_form_meta.h>
namespace tadah {
namespace models {


D2_LJ::D2_LJ() {
  init();
}
D2_LJ::D2_LJ(const tadah::core::Context &c): D2_Base(c)
{
  if (!c.get<bool>("INIT2B")) return;
  init();
  s=2;

  auto init_atoms = get_init_atoms(c);
  init_for_atoms(init_atoms);
}

void D2_LJ::calc_aed(
  const int Zi,
  const int Zj,
  const double rij,
  const double rij_sq,
  tadah::core::aed_type &aed,
  const double scale) const
{
  if (!is_init_for_atoms(Zi,Zj) || rij > get_rcut()) return;
  double r2_inv = 1.0/rij_sq;
  double r6_inv = r2_inv*r2_inv*r2_inv;
  double fc_ij = fcut->calc(rij);

  aed(fidx) -= scale*weights[Zi][Zj]*(r6_inv*fc_ij);
  aed(fidx+1) += scale*weights[Zi][Zj]*(r6_inv*r6_inv*fc_ij);
}
void D2_LJ::calc_dXijdri(
  const int Zi,
  const int Zj,
  const double rij,
  const double rij_sq,
  tadah::core::fd_type &fd_ij,
  const double scale) const
{
  if (!is_init_for_atoms(Zi,Zj) || rij > get_rcut()) return;

  double r2_inv = 1.0/rij_sq;
  double r6_inv = r2_inv*r2_inv*r2_inv;

  double fc_ij = fcut->calc(rij);
  double fcp_ij = fcut->calc_prime(rij);
  fd_ij(fidx,0) =scale*weights[Zi][Zj]*( 6.0*r6_inv*r2_inv*rij*fc_ij - fcp_ij*r6_inv);
  fd_ij(fidx+1,0) =scale*weights[Zi][Zj]*( -12.0*r6_inv*r6_inv*r2_inv*rij*fc_ij + fcp_ij*r6_inv*r6_inv);
}
void D2_LJ::calc_all(
  const int Zi,
  const int Zj,
  const double rij,
  const double rij_sq,
  tadah::core::aed_type &aed,
  tadah::core::fd_type &fd_ij,
  const double scale) const
{
  if (!is_init_for_atoms(Zi,Zj) || rij > get_rcut()) return;
  double r2_inv = 1.0/rij_sq;
  double r6_inv = r2_inv*r2_inv*r2_inv;

  double fc_ij = fcut->calc(rij);
  double fcp_ij = fcut->calc_prime(rij);
  aed(fidx) -= scale*weights[Zi][Zj]*r6_inv*fc_ij;
  aed(fidx+1) += scale*weights[Zi][Zj]*r6_inv*r6_inv*fc_ij;

  fd_ij(fidx,0) = scale*weights[Zi][Zj]*(6.0*r6_inv*r2_inv*rij*fc_ij - fcp_ij*r6_inv);
  fd_ij(fidx+1,0) = scale*weights[Zi][Zj]*(-12.0*r6_inv*r6_inv*r2_inv*rij*fc_ij + fcp_ij*r6_inv*r6_inv);
}
bool D2_LJ::calc_dDdtheta(
  const std::string& theta_name,
  int                arg_pos,
  const int          Zi,
  const int          Zj,
  const double       rij,
  const double       rij_sq,
  tadah::core::aed_type& dD_buf,
  const double       scale) const
{
  if (!is_init_for_atoms(Zi, Zj) || rij > get_rcut()) return false;

  if (theta_name == "RCUT2B") {
    const double dfc = fcut->calc_drcut(rij);
    if (dfc == 0.0) return false;  // discontinuous cutoff; caller FD-falls-back
    const double r2_inv = 1.0 / rij_sq;
    const double r6_inv = r2_inv * r2_inv * r2_inv;
    dD_buf(fidx)     -= scale * weights[Zi][Zj] * r6_inv          * dfc;
    dD_buf(fidx + 1) += scale * weights[Zi][Zj] * r6_inv * r6_inv * dfc;
    return true;
  }

  if (theta_name == "WATOM") {
    // d(aed)/d θ_WATOM = chain × (per-pair LJ contribution with the
    // weights[Zi][Zj] factor removed).
    if (!has_watom_arg_pos(arg_pos)) return false;
    const double chain = dweight_dw_n(Zi, Zj, arg_pos);
    if (chain != 0.0) {
      const double r2_inv = 1.0 / rij_sq;
      const double r6_inv = r2_inv * r2_inv * r2_inv;
      const double fc_ij  = fcut->calc(rij);
      dD_buf(fidx)     -= scale * chain * r6_inv          * fc_ij;
      dD_buf(fidx + 1) += scale * chain * r6_inv * r6_inv * fc_ij;
    }
    return true;
  }

  return false;  // not owned by this descriptor
}

bool D2_LJ::calc_dXijdri_dtheta(
  const std::string& theta_name,
  int                arg_pos,
  const int          Zi,
  const int          Zj,
  const double       rij,
  const double       rij_sq,
  tadah::core::fd_type& dfd_ij,
  const double       scale) const
{
  if (!is_init_for_atoms(Zi, Zj) || rij > get_rcut()) return false;

  if (theta_name == "RCUT2B") {
    const double dfc  = fcut->calc_drcut(rij);
    const double dfcp = fcut->calc_prime_drcut(rij);
    if (dfc == 0.0 && dfcp == 0.0) return false;   // Cut_Dummy fallback
    const double r2_inv = 1.0 / rij_sq;
    const double r6_inv = r2_inv * r2_inv * r2_inv;
    // d/d rcut of fd(fidx,0)   = w(Zj) * ( 6 r^-7 * dfc - r^-6  * dfcp)
    // d/d rcut of fd(fidx+1,0) = w(Zj) * (-12 r^-13 * dfc + r^-12 * dfcp)
    const double val6  =  6.0 * r6_inv * r2_inv * rij * dfc - dfcp * r6_inv;
    const double val12 = -12.0 * r6_inv * r6_inv * r2_inv * rij * dfc + dfcp * r6_inv * r6_inv;
    dfd_ij(fidx,     0) += scale * weights[Zi][Zj] * val6;
    dfd_ij(fidx + 1, 0) += scale * weights[Zi][Zj] * val12;
    return true;
  }

  if (theta_name == "WATOM") {
    // Force-side d(fd)/d θ_WATOM = chain × (unweighted LJ force-radial
    // contribution).
    if (!has_watom_arg_pos(arg_pos)) return false;
    const double chain = dweight_dw_n(Zi, Zj, arg_pos);
    if (chain != 0.0) {
      const double r2_inv = 1.0 / rij_sq;
      const double r6_inv = r2_inv * r2_inv * r2_inv;
      const double fc_ij  = fcut->calc(rij);
      const double fcp_ij = fcut->calc_prime(rij);
      dfd_ij(fidx,     0) +=
        scale * chain * ( 6.0 * r6_inv * r2_inv * rij * fc_ij - fcp_ij * r6_inv);
      dfd_ij(fidx + 1, 0) +=
        scale * chain * (-12.0 * r6_inv * r6_inv * r2_inv * rij * fc_ij + fcp_ij * r6_inv * r6_inv);
    }
    return true;
  }

  return false;
}

std::string D2_LJ::label() const {
  return lab;
}
void D2_LJ::init() {
  nparams=0;
}

std::optional<DescriptorMeta> D2_LJ::meta() const {
  return make_closed_form_meta("D2_LJ", "RCUT2B");
}

} // namespace models
} // namespace tadah

