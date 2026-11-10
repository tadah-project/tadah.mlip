#ifndef KERN_RBF_H
#define KERN_RBF_H

#include <tadah/models/functions/kernels/kern_base.h>

namespace tadah {
namespace models {

/** \brief Radial Basis Function kernel
 *
 * Defined for two vectors **x** and **y**:
 *
 * \f[
 * K(\mathbf{x}, \mathbf{y}) = \exp\Big( -\frac{||\mathbf{x}-\mathbf{y}||^2}
 * {2\sigma^2} \Big)
 * \f]
 *
 *  Required tadah::core::Context key: \ref MPARAMS <double> \f$ \sigma \f$
 *
 *  @see Kern_Base Kern_Linear Kern_Quadratic
 */
class Kern_RBF : public virtual Kern_Base {
    public:
        double sigma;
        /**
         * Label used for this class
         */
        Kern_RBF ();
        Kern_RBF (const tadah::core::Context &c);
        std::string label = "Kern_RBF";
        double operator() (const tadah::core::aed_type& b, const tadah::core::aed_type& af) const override;
        tadah::core::aed_type derivative(const tadah::core::aed_type& b, const tadah::core::aed_type& af) const override;
        double prime(const tadah::core::aed_type& b, const tadah::core::aed_type& af, const tadah::core::aed_type& ff) const override;

        /** Closed-form analytical derivative of K w.r.t. sigma:
         *
         *    K(b, af) = exp(-||b - af||^2 / (2 sigma^2))
         *    dK / d sigma = K * ||b - af||^2 / sigma^3
         *
         *  Supported (theta_name, arg_pos): ("MPARAMS", 1) -> dK/d sigma.
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

    private:

        double gamma;

};
} // namespace models
} // namespace tadah
#endif
