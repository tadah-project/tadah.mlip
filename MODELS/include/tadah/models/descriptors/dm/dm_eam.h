#ifndef DM_EAM_H
#define DM_EAM_H
#include <tadah/models/descriptors/dm/dm_base.h>
#include <tadah/models/eam/eam_setfl_io.h>

#include <memory>

namespace tadah {
namespace models {

/** \brief many-body part for the Embedded Atom Method descriptor.
 *
 * \f[
 * V_i = F\Bigg(\sum_{j \neq i} \rho(r_{ij}) \Bigg)
 * \f]
 *
 * This descriptor will load tabulated values for the density \f$ \rho \f$
 * and embedded energy \f$ F \f$ from the setfl file (single element, LAMMPS
 * `eam/alloy` or `eam/fs` layout) named by the \ref SETFL key.
 *
 * The TYPEMB line is `TYPEMB DM_EAM 0 <El El ...>`. The one parameter is a
 * placeholder, written 0: the TYPEMB format requires an unsigned integer
 * there, and the descriptor does not read it. Another value is ignored and
 * reported on standard error once per process (report_eam_placeholder()).
 *
 * This descriptor is usually used together with the two-body descriptor \ref D2_EAM,
 * which reads the pair function from the same SETFL file (one SETFL key
 * serves both; see \ref D2_EAM for a configuration),
 * although this is not required and user can mix it with any other descriptors
 * or use it on its own.
 *
 * \ref RCUTMB must be the cutoff of the setfl file (in a DM_mJoin, this
 * descriptor's own RCUTMB value): the file defines its functions up to that
 * cutoff. Another value is refused, naming both (require_setfl_cutoff();
 * see \ref D2_EAM).
 *
 * \ref WATOM does not weight this descriptor: the functions of the file
 * are used as they are. A fit refuses a WATOM with a value other than 1
 * when no descriptor of its configuration reads it (D2_EAM, DM_EAM and the
 * dummies D2_Dummy and DM_Dummy read none), naming the line; a potential
 * with one loads with a notice.
 *
 * The setfl parsing, cubic splines and extrapolation are provided by the shared
 * \ref EAMSetflIO class (also used by \ref D2_EAM and the `tadah refit` machinery).
 *
 * Required tadah::core::Context keys:
 * \ref INITMB \ref TYPEMB \ref RCUTMB \ref SETFL
 */
class DM_EAM: public DM_Base {
private:
  /** The tables of the SETFL file, shared with every descriptor of the
   *  process that read the same file (shared_setfl()); null until built
   *  from a context. */
  std::shared_ptr<const EAMSetflIO> eam_;
  std::string lab="DM_EAM";

public:
  DM_EAM();
  DM_EAM(tadah::core::Context&);
  DM_EAM(const tadah::core::Context&);
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
  std::optional<DescriptorMeta> meta() const override;
  /** The cutoff of the setfl file, for every value of the cutoff key ("" when
   *  the descriptor read no file: built with INIT2B/INITMB false). */
  std::string cutoff_fixed_by(std::size_t i) const override;
  /** The tables of the setfl file are evaluated unweighted. */
  bool reads_watom() const override { return false; }
};
} // namespace models
} // namespace tadah
#endif
