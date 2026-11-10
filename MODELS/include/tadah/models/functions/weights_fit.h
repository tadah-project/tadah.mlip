#ifndef TADAH_MODELS_WEIGHTS_FIT_H
#define TADAH_MODELS_WEIGHTS_FIT_H

#include <tadah/core/context.h>
#include <tadah/models/functions/function_base.h>

#include <cstddef>

namespace tadah {
namespace models {

/** \brief Sizes of the trained arrays a model predicts with. */
struct TrainedSizes {
  std::size_t weights = 0;   ///< WEIGHTS
  std::size_t nmean = 0;     ///< NMEAN
  std::size_t nstdev = 0;    ///< NSTDEV
};

/** \brief The sizes of the WEIGHTS, NMEAN and NSTDEV keys stored in @p c
 *  (0 for an absent key). */
TrainedSizes trained_sizes(const tadah::core::Context &c);

/** \brief True for the linear models, BF_Linear and Kern_Linear (one
 *  weight per descriptor component): they predict with weights converted
 *  to the raw descriptors and never read NMEAN or NSTDEV. */
bool is_linear_function(const Function_Base &f);

/** \brief Refuses a potential whose trained arrays do not fit the
 *  descriptor it predicts from: the one check of a potential's fit, which
 *  tadah predict (MLIP) and pair_style tadah share.
 *
 *  The predictors' dot products run over the weights, so weights written
 *  for another descriptor layout give silently wrong energies and forces.
 *  Required, for a descriptor of @p dsize entries (DSIZE) and the basis
 *  function or kernel @p f:
 *    - @p s.weights == f.weights_count(dsize);
 *    - a kernel with a basis: basis vectors of dsize entries;
 *    - NORM true in @p c and a non-linear @p f: dsize values of NMEAN and
 *      of NSTDEV (a linear model does not read them: is_linear_function).
 *
 *  The message names WEIGHTS (NMEAN, NSTDEV, BASIS), DSIZE, BIAS and
 *  ATOM. When BIAS is true with N >= 2 ATOM species and the sizes are
 *  those of a descriptor with one intercept column instead of N (DSIZE -
 *  N + 1: how tadah before 1.6.0 trained BIAS true), it says so and that
 *  the potential has to be retrained; likewise for NMEAN and NSTDEV of
 *  2 x DSIZE values (appended by `tadah train -c <potential>` before
 *  1.6.0).
 *
 *  A linear potential with NORM true and BIAS false is refused whatever
 *  its sizes: tadah no longer trains it, and the conversion that wrote one
 *  had no intercept column for the normalisation's constant. M_KRR
 *  Kern_Linear (until 1.6.0) and M_BLR BF_Linear (up to 1.2.0) took
 *  descriptor component 0 for an intercept, so the energies and forces of
 *  such a potential are wrong; M_BLR BF_Linear from 1.3.0-beta.1 (written
 *  by tadah hpo and the API; tadah train refused the configuration)
 *  dropped the mean correction, so the energies are off by the same
 *  constant for every atom and the forces are right. The message says
 *  which, for the potential's function.
 *
 *  Called where trained weights are used to predict (MLIP M_BLR / M_KRR
 *  predictions, pair_style tadah when it loads a potential), never where
 *  a model is built for training: a training configuration may carry
 *  stale WEIGHTS, NMEAN and NSTDEV, which the fit replaces (MLIP's Trainer
 *  builds the model to be fitted without them).
 *
 *  @throws std::runtime_error */
void require_weights_fit(const tadah::core::Context &c, std::size_t dsize,
                         const Function_Base &f, const TrainedSizes &s);

/** \brief require_weights_fit with the sizes of the WEIGHTS, NMEAN and
 *  NSTDEV keys of @p c (a potential as it is loaded). */
void require_weights_fit(const tadah::core::Context &c, std::size_t dsize,
                         const Function_Base &f);

} // namespace models
} // namespace tadah

#endif
