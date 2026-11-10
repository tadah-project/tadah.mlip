#ifndef D2_EAM_H
#define D2_EAM_H
#include <tadah/models/descriptors/d2/d2_base.h>
#include <tadah/models/eam/eam_setfl_io.h>

#include <memory>

namespace tadah {
namespace models {

/** \brief Pair-wise part for the Embedded Atom Method descriptor.
 *
 * \f[
 * V_i = \frac{1}{2} \sum_{j \neq i} \phi(r_{ij})
 * \f]
 *
 * This descriptor will load tabulated values for the two-body potential \f$ \phi \f$
 * from the setfl file (single element, LAMMPS `eam/alloy` or `eam/fs` layout)
 * named by the \ref SETFL key.
 *
 * The TYPE2B line is `TYPE2B D2_EAM 0 <El El ...>`. The one parameter is a
 * placeholder, written 0: the line must carry it (a line without it is
 * refused), and the descriptor does not read it. Another value is ignored and
 * reported on standard error once per process (report_eam_placeholder()).
 *
 * This descriptor is usually used together with the many-body descriptor \ref DM_EAM,
 * which reads the density and the embedding function from the same SETFL file,
 * although this is not required and user can mix it with any other descriptors
 * or use it on its own:
 *
 *     INIT2B   true
 *     TYPE2B   D2_EAM 0 Ta Ta
 *     RCTYPE2B Cut_Dummy
 *     RCUT2B   5.58190742024    # the setfl file's cutoff
 *     INITMB   true
 *     TYPEMB   DM_EAM 0 Ta Ta
 *     RCTYPEMB Cut_Dummy
 *     RCUTMB   5.58190742024
 *     SETFL    Ta.eam.alloy
 *
 * \ref RCUT2B must be the cutoff of the setfl file (in a D2_mJoin, this
 * descriptor's own RCUT2B value): the file defines its functions up to that
 * cutoff. Another value is refused, naming both, e.g.
 * `D2_EAM: RCUT2B 3 differs from the cutoff 5.58190742024 of the setfl file
 * 'Ta.eam.alloy' (SETFL); set RCUT2B to the file's cutoff, 5.58190742024.`
 * (require_setfl_cutoff()).
 *
 * \ref WATOM does not weight this descriptor: the functions of the file
 * are used as they are. A fit refuses a WATOM with a value other than 1
 * when no descriptor of its configuration reads it (D2_EAM, DM_EAM and the
 * dummies D2_Dummy and DM_Dummy read none), naming the line; a potential
 * with one loads with a notice.
 *
 * The setfl parsing, cubic splines and extrapolation are provided by the shared
 * \ref EAMSetflIO class (also used by \ref DM_EAM and the `tadah refit` machinery).
 *
 * Required tadah::core::Context keys:
 * \ref INIT2B \ref TYPE2B \ref RCUT2B \ref SETFL
 */
class D2_EAM : public D2_Base {
private:
  /** The tables of the SETFL file, shared with every descriptor of the
   *  process that read the same file (shared_setfl()); null until built
   *  from a context. */
  std::shared_ptr<const EAMSetflIO> eam_;
  std::string lab="D2_EAM";

public:
  D2_EAM();
  D2_EAM(tadah::core::Context &context);
  D2_EAM(const tadah::core::Context& c);
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
