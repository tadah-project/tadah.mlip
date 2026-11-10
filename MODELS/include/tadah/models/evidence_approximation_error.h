#ifndef TADAH_MODELS_EVIDENCE_APPROXIMATION_ERROR_H
#define TADAH_MODELS_EVIDENCE_APPROXIMATION_ERROR_H

// A header of its own, without the LAPACK declarations of ea.h: a caller
// built without them (tadah hpo) catches the type.

#include <stdexcept>

namespace tadah {
namespace models {

/** The refusal of an evidence approximation that cannot estimate the prior
 *  and the noise from the targets of its fit: alpha or beta not finite, or
 *  no more observations than effective weights (EA::run()). Its targets
 *  and design matrix cause it, not the settings of the fit: a caller that
 *  fits many candidates (tadah hpo) fails the candidate. */
class EvidenceApproximationError : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

}  // namespace models
}  // namespace tadah

#endif  // TADAH_MODELS_EVIDENCE_APPROXIMATION_ERROR_H
