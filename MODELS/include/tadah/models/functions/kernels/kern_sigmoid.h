#ifndef KERN_SIGMOID_H
#define KERN_SIGMOID_H

#include <tadah/models/functions/kernels/kern_base.h>

namespace tadah {
namespace models {

/** \brief Sigmoid kernel.
 *
 * Defined for two vectors **x** and **y**:
 *
 * \f[
 * K(\mathbf{x}, \mathbf{y}) = \tanh\big( \gamma*\mathbf{x}^T \mathbf{y}
 * + \mathrm{c} \big)
 * \f]
 *
 *  Required tadah::core::Context key: \ref MPARAMS <double> \f$ \gamma \f$  c
 *
 *  @see Kern_Base
 */
class Kern_Sigmoid : public virtual Kern_Base {
    public:
        double gamma;   // scalling
        double c;   // shift
        /**
         * Label used for this class
         */
        Kern_Sigmoid ();
        Kern_Sigmoid (const tadah::core::Context &c);
        std::string label = "Kern_Sigmoid";
        double operator() (const tadah::core::aed_type& b, const tadah::core::aed_type& af) const override;
        tadah::core::aed_type derivative(const tadah::core::aed_type& b, const tadah::core::aed_type& af) const override;
        double prime(const tadah::core::aed_type& b, const tadah::core::aed_type& af, const tadah::core::aed_type& ff) const override;

        /** Closed-form analytical derivatives of K w.r.t. gamma and c.
         *
         *    K(b, af) = tanh(gamma * (b . af) + c)
         *    dK/d gamma = (1 - K^2) * (b . af)
         *    dK/d c     = (1 - K^2)
         *
         *  MPARAMS layout: [gamma, c]. arg_pos counts numerical tokens
         *  1-based (see HPOTargets::ui_offset).
         *   - ("MPARAMS", 1) -> dK/d gamma
         *   - ("MPARAMS", 2) -> dK/d c
         *  Anything else returns false.
         */
        bool calc_dKdtheta(
            const std::string& theta_name,
            int                arg_pos,
            const tadah::core::aed_type& b,
            const tadah::core::aed_type& af,
            double& dK_out) const override;

        std::string get_label() const override;
        /** \brief See Function_Base::mparams(). */
        std::vector<MParamSlot> mparams() const override;
        /** \brief See Function_Base::intercept_columns(). */
        std::vector<size_t> intercept_columns(const tadah::core::Context &c) const override;

};
} // namespace models
} // namespace tadah
#endif
