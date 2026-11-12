#ifndef KERN_POLYNOMIAL_H
#define KERN_POLYNOMIAL_H

#include <tadah/models/functions/kernels/kern_base.h>

namespace tadah {
namespace models {

/** \brief Polynomial kernel.
 *
 * Defined for two vectors **x** and **y**:
 *
 * \f[
 * K(\mathbf{x}, \mathbf{y}) = \big( \gamma*\mathbf{x}^T \mathbf{y} +
 * \mathrm{c} \big)^{\mathrm{d}}
 * \f]
 *
 *  Required tadah::core::Context key: \ref MPARAMS <int> d  <double> \f$ \gamma \f$  c
 *
 *  @see Kern_Base
 */
class Kern_Polynomial : public virtual Kern_Base {
    public:
        Kern_Polynomial ();
        Kern_Polynomial (const tadah::core::Context &c);
        int d; // power
        double gamma;   // scalling
        double c;   // shift
        /**
         * Label used for this class
         */
        std::string label = "Kern_Polynomial";
        std::string get_label() const override;
        /** \brief See Function_Base::mparams(). */
        std::vector<MParamSlot> mparams() const override;
        /** \brief See Function_Base::intercept_columns(). */
        std::vector<size_t> intercept_columns(const tadah::core::Context &c) const override;
        double operator() (const tadah::core::aed_type& b, const tadah::core::aed_type& af) const override;
        tadah::core::aed_type derivative(const tadah::core::aed_type& b, const tadah::core::aed_type& af) const override;

        double prime(const tadah::core::aed_type& b, const tadah::core::aed_type& af, const tadah::core::aed_type& ff) const override;

        /** Closed-form analytical derivatives of K w.r.t. its continuous
         *  hyperparameters:
         *
         *    K(b, af) = (gamma * (b . af) + c)^d,  d integer.
         *    dK/d gamma = d * (b . af) * (gamma * (b . af) + c)^(d-1)
         *    dK/d c     = d              * (gamma * (b . af) + c)^(d-1)
         *
         *  d is an integer and is not tunable.
         *
         *  MPARAMS layout: [d, gamma, c]. arg_pos counts numerical
         *  tokens 1-based (see HPOTargets::ui_offset).
         *   - ("MPARAMS", 2) -> dK/d gamma
         *   - ("MPARAMS", 3) -> dK/d c
         *  Anything else returns false.
         */
        bool calc_dKdtheta(
            const std::string& theta_name,
            int                arg_pos,
            const tadah::core::aed_type& b,
            const tadah::core::aed_type& af,
            double& dK_out) const override;

};
} // namespace models
} // namespace tadah
#endif
