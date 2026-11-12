#ifndef D2_DUMMY_H
#define D2_DUMMY_H
#include <tadah/models/descriptors/d2/d2_base.h>

namespace tadah {
namespace models {

/**
 * Dummy two-body descriptor.
 *
 * Use it to satisfy DescriptorsCalc requirements in case
 * when two-body descriptor is not required.
 *
 */
class D2_Dummy : public D2_Base {
private:
  std::string lab="D2_Dummy";
public:
  D2_Dummy();
  D2_Dummy(const tadah::core::Context &);
  void calc_aed(
    const int Zi,
    const int Zj,
    const double rij,
    const double ,
    tadah::core::aed_type &aed,
    const double scale=1) const override;
  void calc_dXijdri(
    const int Zi,
    const int Zj,
    const double rij,
    const double rij_sq,
    tadah::core::fd_type &fd_ij,
    const double scale=1) const override;
  void calc_all(
    const int Zi,
    const int Zj,
    const double rij,
    const double rij_sq,
    tadah::core::aed_type &aed,
    tadah::core::fd_type &fd_ij,
    const double scale=1) const override;

  std::string label() const override;
  void init() override;
  /** No values: WATOM weighs none, and nothing changes them. */
  bool reads_watom() const override { return false; }
  bool depends_on(const std::string &, int) const override { return false; }
};
} // namespace models
} // namespace tadah
#endif
