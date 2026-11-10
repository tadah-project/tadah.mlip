#include "tadah/models/descriptors/dm/dm_base.h"
#include <tadah/models/descriptors/dm/dm_eam.h>
#include <tadah/models/descriptors/closed_form_meta.h>
#include <tadah/models/descriptors/eam_placeholder.h>
#include <tadah/models/descriptors/eam_setfl_cutoff.h>
namespace tadah {
namespace models {

DM_EAM::DM_EAM(const tadah::core::Context& c)             // new, const version
  : DM_EAM::DM_EAM(const_cast<tadah::core::Context&>(c)) {}      // delegates to old one

DM_EAM::DM_EAM() {
  init();
}
DM_EAM::DM_EAM(core::Context &c): DM_Base(c)
{
  init();
  if (!c.get<bool>("INITMB")) return;
  s=1;
  report_eam_placeholder(c, "TYPEMB", lab);
  // Parsed and splined once per file contents in the process.
  eam_ = shared_setfl(c("SETFL")[0]);

  require_setfl_cutoff(c, "RCUTMB", lab, *eam_);

  rhoisize = 1;

  set_rfidx(0);
  set_drfidx(rhoisize);

  auto init_atoms = get_init_atoms(c);
  init_for_atoms(init_atoms);
}

void DM_EAM::calc_aed(
  tadah::core::rho_type& rho,
  tadah::core::aed_type &aed) const
{
  const auto &frho_spline = eam_->frho_spline();
  double p = rho(rfidx)*eam_->rdrho() + 1.0;
  int m = static_cast<int> (p);
  m = std::max(1,std::min(m,eam_->nrho()-1));
  p -= m;
  p = std::min(p,1.0);
  double phi = ((frho_spline[m][3]*p + frho_spline[m][4])*p + frho_spline[m][5])*p + frho_spline[m][6];

  rho(drfidx) = (frho_spline[m][0]*p + frho_spline[m][1])*p + frho_spline[m][2];    // lammps fp[i]

  if (rho(rfidx) > eam_->rhomax()) phi += rho(drfidx) * (rho(rfidx)-eam_->rhomax());

  aed(fidx) += phi;
}
void DM_EAM::calc_dXijdri_dXjidri(
  const int Zi,
  const int Zj,
  const double rij,
  const double,
  const tadah::core::Vec3d &vec_ij,
  tadah::core::rho_type& rhoi,
  tadah::core::rho_type& rhoj,
  tadah::core::fd_type &fd_ij,
  const double scale) const
{
  if (!is_init_for_atoms(Zi,Zj) || rij > get_rcut()) return;
  const auto &rhor_spline = eam_->rhor_spline();
  double r = rij;
  double rinv = 1/r;
  double p = r*eam_->rdr() + 1.0;
  int m = static_cast<int> (p);
  m = std::min(m,eam_->nr()-1);
  p -= m;
  p = std::min(p,1.0);
  double rhoip = (rhor_spline[m][0]*p + rhor_spline[m][1])*p + rhor_spline[m][2];
  double rhojp = (rhor_spline[m][0]*p + rhor_spline[m][1])*p + rhor_spline[m][2];

  double psip = rhoi(drfidx)*rhojp + rhoj(drfidx)*rhoip;

  double f = scale*psip*rinv;
  fd_ij(fidx,0) = f*vec_ij[0];
  fd_ij(fidx,1) = f*vec_ij[1];
  fd_ij(fidx,2) = f*vec_ij[2];
}
void DM_EAM::calc_dXijdri(
  const int Zi,
  const int Zj,
  const double rij,
  const double,
  const tadah::core::Vec3d &vec_ij,
  tadah::core::rho_type& rhoi,
  tadah::core::fd_type &fd_ij,
  const double scale) const
{
  if (!is_init_for_atoms(Zi,Zj) || rij > get_rcut()) return;
  const auto &rhor_spline = eam_->rhor_spline();
  double r = rij;
  double rinv = 1/r;
  double p = r*eam_->rdr() + 1.0;
  int m = static_cast<int> (p);
  m = std::min(m,eam_->nr()-1);
  p -= m;
  p = std::min(p,1.0);
  double rhojp = (rhor_spline[m][0]*p + rhor_spline[m][1])*p + rhor_spline[m][2];

  double psip = rhoi(drfidx)*rhojp;

  double f = scale*psip*rinv;
  fd_ij(fidx,0) = f*vec_ij[0];
  fd_ij(fidx,1) = f*vec_ij[1];
  fd_ij(fidx,2) = f*vec_ij[2];
}
std::string DM_EAM::label() const {
  return lab;
}

void DM_EAM::init_rhoi(tadah::core::rho_type& rhoi)
{
  rhoi.resize(2);
  rhoi.set_zero();
}
void DM_EAM::calc_rho(
  const int Zi,
  const int Zj,
  const double rij,
  const double,
  const tadah::core::Vec3d &,
  tadah::core::rho_type& rho,
  const double scale) const
{
  if (!is_init_for_atoms(Zi,Zj) || rij > get_rcut()) return;
  const auto &rhor_spline = eam_->rhor_spline();
  double r = rij;
  double p = r*eam_->rdr() + 1.0;
  int m = static_cast<int> (p);
  m = std::min(m,eam_->nr()-1);
  p -= m;
  p = std::min(p,1.0);
  const std::vector<double> &coeff = rhor_spline[m];
  rho(rfidx) += scale*( ((coeff[3]*p + coeff[4])*p + coeff[5])*p + coeff[6]);
}
void DM_EAM::init() {
  nparams=1;
}

std::string DM_EAM::cutoff_fixed_by(std::size_t) const {
  return eam_ ? setfl_cutoff_clause(lab, *eam_) : std::string();
}

std::optional<DescriptorMeta> DM_EAM::meta() const {
  return make_closed_form_meta("DM_EAM", "RCUTMB", /*cutoff_from_setfl=*/true);
}

} // namespace models
} // namespace tadah
