#include "tadah/models/descriptors/d2/d2_base.h"
#include <tadah/models/descriptors/d2/d2_eam.h>
#include <tadah/models/descriptors/closed_form_meta.h>
#include <tadah/models/descriptors/eam_placeholder.h>
#include <tadah/models/descriptors/eam_setfl_cutoff.h>
namespace tadah {
namespace models {

D2_EAM::D2_EAM(const tadah::core::Context& c)             // new, const version
  : D2_EAM::D2_EAM(const_cast<tadah::core::Context&>(c)) {}      // delegates to old one

D2_EAM::D2_EAM() {
  init();
}
D2_EAM::D2_EAM(tadah::core::Context &c): D2_Base(c)
{
  init();
  if (!c.get<bool>("INIT2B")) return;
  s=1;
  report_eam_placeholder(c, "TYPE2B", lab);
  // Parsed and splined once per file contents in the process.
  eam_ = shared_setfl(c("SETFL")[0]);

  require_setfl_cutoff(c, "RCUT2B", lab, *eam_);

  auto init_atoms = get_init_atoms(c);
  init_for_atoms(init_atoms);
}

void D2_EAM::calc_aed(
  const int Zi,
  const int Zj,
  const double rij,
  const double,
  tadah::core::aed_type &aed,
  const double scale) const
{
  if (!is_init_for_atoms(Zi,Zj) || rij > get_rcut()) return;
  const auto &z2r_spline = eam_->z2r_spline();
  double r = rij;
  const double recip = 1.0/r;
  double p = r*eam_->rdr() + 1.0;
  int m = static_cast<int> (p);
  m = std::min(m,eam_->nr()-1);
  p -= m;
  p = std::min(p,1.0);
  double z2 = ((z2r_spline[m][3]*p + z2r_spline[m][4])*p + z2r_spline[m][5])*p + z2r_spline[m][6];
  aed(fidx) += scale*z2*recip;
}
void D2_EAM::calc_dXijdri(
  const int Zi,
  const int Zj,
  const double rij,
  const double,
  tadah::core::fd_type &fd_ij,
  const double scale) const
{
  if (!is_init_for_atoms(Zi,Zj) || rij > get_rcut()) return;
  const auto &z2r_spline = eam_->z2r_spline();
  const double r = rij;
  const double recip = 1.0/r;
  double p = r*eam_->rdr() + 1.0;
  int m = static_cast<int> (p);
  m = std::min(m,eam_->nr()-1);
  p -= m;
  p = std::min(p,1.0);
  double z2p = (z2r_spline[m][0]*p + z2r_spline[m][1])*p + z2r_spline[m][2];
  double z2 = ((z2r_spline[m][3]*p + z2r_spline[m][4])*p + z2r_spline[m][5])*p + z2r_spline[m][6];

  fd_ij(fidx,0) = scale*(z2p*recip - z2*recip*recip);
}
void D2_EAM::calc_all(
  const int Zi,
  const int Zj,
  const double rij,
  const double,
  tadah::core::aed_type &aed,
  tadah::core::fd_type &fd_ij,
  const double scale) const
{
  if (!is_init_for_atoms(Zi,Zj) || rij > get_rcut()) return;
  const auto &z2r_spline = eam_->z2r_spline();
  double r = rij;
  const double recip = 1.0/r;
  double p = r*eam_->rdr() + 1.0;
  int m = static_cast<int> (p);
  m = std::min(m,eam_->nr()-1);
  p -= m;
  p = std::min(p,1.0);
  double z2 = ((z2r_spline[m][3]*p + z2r_spline[m][4])*p + z2r_spline[m][5])*p + z2r_spline[m][6];
  aed(fidx) += scale*z2*recip;

  double z2p = (z2r_spline[m][0]*p + z2r_spline[m][1])*p + z2r_spline[m][2];
  fd_ij(fidx,0) = scale*(z2p*recip - z2*recip*recip);
}
std::string D2_EAM::label() const {
  return lab;
}

void D2_EAM::init() {
  nparams=1;
}

std::string D2_EAM::cutoff_fixed_by(std::size_t) const {
  return eam_ ? setfl_cutoff_clause(lab, *eam_) : std::string();
}

std::optional<DescriptorMeta> D2_EAM::meta() const {
  return make_closed_form_meta("D2_EAM", "RCUT2B", /*cutoff_from_setfl=*/true);
}

} // namespace models
} // namespace tadah
