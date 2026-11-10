#ifndef DM_DUMMY_H
#define DM_DUMMY_H
#include <tadah/models/descriptors/dm/dm_base.h>

namespace tadah {
namespace models {

/**
 * Dummy many-body descriptor.
 *
 * Use it to satisfy DescriptorsCalc requirements in case
 * when many-body descriptor is not required.
 *
 */
class DM_Dummy: public DM_Base {
private:
  std::string lab="DM_Dummy";
public:
  DM_Dummy();
  DM_Dummy(const tadah::core::Context&);
  void calc_aed(
    tadah::core::rho_type& rho,
    tadah::core::aed_type &aed) const override;
  void calc_dXijdri_dXjidri(
    const int Zi,
    const int Zj,
    const double rij,
    const double rij_sq,
    const tadah::core::Vec3d &vec_ij,
    tadah::core::rho_type& rhoi,
    tadah::core::rho_type& rhoj,
    tadah::core::fd_type &fd_ij,
    const double scale=1) const override;
  void calc_dXijdri(
    const int Zi,
    const int Zj,
    const double rij,
    const double rij_sq,
    const tadah::core::Vec3d &vec_ij,
    tadah::core::rho_type& rhoi,
    tadah::core::fd_type &fd_ij, 
    const double scale=1) const override;
  std::string label() const override;
  /** No values: WATOM weighs none, and nothing changes them. */
  bool reads_watom() const override { return false; }
  bool depends_on(const std::string &, int) const override { return false; }
  void init_rhoi(tadah::core::rho_type& rhoi) override;
  void calc_rho(
    const int Zi,
    const int Zj,
    const double rij,
    const double rij_sq,
    const tadah::core::Vec3d &vec_ij,
    tadah::core::rho_type& rho,
    const double scale=1) const override;
  void init() override;
};
} // namespace models
} // namespace tadah
#endif
