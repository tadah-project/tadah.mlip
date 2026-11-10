#ifndef TADAH_MODELS_DESCRIPTORS_EAM_SETFL_CUTOFF_H
#define TADAH_MODELS_DESCRIPTORS_EAM_SETFL_CUTOFF_H

#include <tadah/core/context.h>
#include <tadah/models/eam/eam_setfl_io.h>

#include <string>

namespace tadah {
namespace models {

/** \brief Refuses a cutoff of \ref D2_EAM or \ref DM_EAM other than the
 *  cutoff of their setfl file.
 *
 * Both descriptors evaluate the tabulated functions of the setfl file named
 * by SETFL, which the file defines up to its cutoff. Every value of
 * @p rcut_key (`RCUT2B` or `RCUTMB`; inside a D2_mJoin or DM_mJoin, the
 * descriptor's own value) must be that cutoff, as the file writes it or in
 * any text that reads as the same number; another is refused, e.g.
 *
 *     D2_EAM: RCUT2B 3 differs from the cutoff 5.58190742024 of the setfl
 *     file 'Ta.eam.alloy' (SETFL); set RCUT2B to the file's cutoff,
 *     5.58190742024.
 *
 * (one line). The context is not changed: the descriptor's cutoff function,
 * the model's cutoffs and the neighbour lists are built from the key, so a
 * value replaced afterwards is not the cutoff the descriptor is evaluated
 * with, while a potential file records the replaced one.
 *
 * @param c        the descriptor's context
 * @param rcut_key `RCUT2B` or `RCUTMB`
 * @param label    the descriptor name, `D2_EAM` or `DM_EAM`
 * @param eam      the setfl file the descriptor read
 * @throws std::runtime_error for a value other than eam.rcut()
 */
void require_setfl_cutoff(const tadah::core::Context &c,
                          const std::string &rcut_key,
                          const std::string &label, const EAMSetflIO &eam);

/** \brief "<label> takes the cutoff <rc> of its setfl file '<path>'
 *  (SETFL)": what D2_EAM::cutoff_fixed_by() and DM_EAM::cutoff_fixed_by()
 *  answer, @p rc the file's cutoff as require_setfl_cutoff() writes it. */
std::string setfl_cutoff_clause(const std::string &label,
                                const EAMSetflIO &eam);

}  // namespace models
}  // namespace tadah

#endif  // TADAH_MODELS_DESCRIPTORS_EAM_SETFL_CUTOFF_H
