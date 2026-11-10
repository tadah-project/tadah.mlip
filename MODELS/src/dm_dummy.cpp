#include "tadah/models/descriptors/dm/dm_base.h"
#include <tadah/models/descriptors/dm/dm_dummy.h>
namespace tadah {
namespace models {


DM_Dummy::DM_Dummy() {
  init();
}
DM_Dummy::DM_Dummy(const tadah::core::Context&c): DM_Base(c) {
  init();
}
void DM_Dummy::calc_aed(
  tadah::core::rho_type&,
  tadah::core::aed_type & ) const {}
void DM_Dummy::calc_dXijdri_dXjidri(
  const int,
  const int,
  const double,
  const double,
  const tadah::core::Vec3d &,
  tadah::core::rho_type&,
  tadah::core::rho_type&,
  tadah::core::fd_type &,
  const double) const {}
void DM_Dummy::calc_dXijdri(
  const int,
  const int,
  const double,
  const double,
  const tadah::core::Vec3d &,
  tadah::core::rho_type&,
  tadah::core::fd_type &,
  const double) const {}
std::string DM_Dummy::label() const {return lab;}
void DM_Dummy::init_rhoi(tadah::core::rho_type&) {}
void DM_Dummy::calc_rho(
  const int,
  const int,
  const double,
  const double,
  const tadah::core::Vec3d &,
  tadah::core::rho_type&,
  const double) const {}

void DM_Dummy::init() {
  nparams=0;
}

} // namespace models
} // namespace tadah

