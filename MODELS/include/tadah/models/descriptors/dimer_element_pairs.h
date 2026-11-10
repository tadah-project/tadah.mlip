#ifndef TADAH_MODELS_DIMER_ELEMENT_PAIRS_H
#define TADAH_MODELS_DIMER_ELEMENT_PAIRS_H

#include <tadah/core/context.h>
#include <tadah/models/descriptors/d_base.h>

namespace tadah {
namespace models {

/** \brief Refuse a DIMER potential whose descriptors name specific element
 *  pairs.
 *
 *  The DIMER evaluators -- DescriptorsCalc::calc_dimer (Tadah.MLIP) and
 *  PairTadah::compute_dimers (pair_style tadah) -- evaluate every atom as
 *  Z = 1, whatever its element, so every pair gets the weight of the Z=1
 *  (H-H) pair: 1, unless WATOM sets one for H; the WATOM weight of the
 *  potential's own element is not applied. A two-body or many-body
 *  descriptor declared for specific element pairs (`TYPE2B D2_Blip 4 4 N
 *  N`) then applies to no pair the evaluation sees, and the model would
 *  predict from zero descriptors. Both evaluators call this
 *  when they load a potential.
 *
 *  When `DIMER` is on, each descriptor that is switched on (`INIT2B` for
 *  `d2`, `INITMB` for `dm`) and has a non-zero size -- the ones calc_dimer
 *  evaluates -- must apply to every element pair (D_Base::
 *  applies_to_every_element_pair(), i.e. be declared with `* *`). Nothing
 *  is checked when `DIMER` is off.
 *
 *  \throws std::runtime_error "DIMER evaluates every atom as Z=1; declare
 *          TYPE2B/TYPEMB with * * element pairs", followed by the offending
 *          TYPE2B or TYPEMB line.
 */
void require_dimer_element_pairs(const tadah::core::Context &c,
                                 const D_Base &d2, const D_Base &dm);

} // namespace models
} // namespace tadah
#endif
