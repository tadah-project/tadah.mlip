#ifndef KERN_LINEAR_H
#define KERN_LINEAR_H

#include <tadah/models/functions/kernels/kern_base.h>

namespace tadah {
namespace models {

/**
 * Linear kernel also knows as dot product kernel
 *
 * Defined for two vectors **x** and **y**:
 *
 * \f[
 * K(\mathbf{x}, \mathbf{y}) = \mathbf{x}^T \mathbf{y} = \sum_i x^{(i)} y^{(i)}
 * \f]
 *
 *  @see Kern_Base BF_Linear
 */
class Kern_Linear :  public virtual Kern_Base {
    public:
        Kern_Linear ();
        Kern_Linear (const tadah::core::Context &c);
        std::string get_label() const override;
        /** \brief See Function_Base::intercept_columns(). */
        std::vector<size_t> intercept_columns(const tadah::core::Context &c) const override;

        /**
         * Label used for this class
         */
        std::string label = "Kern_Linear";
        double operator() (const tadah::core::aed_type& b, const tadah::core::aed_type& af) const override;
        tadah::core::aed_type derivative(const tadah::core::aed_type& b, const tadah::core::aed_type& ) const override;
        double prime(const tadah::core::aed_type& b, const tadah::core::aed_type& af, const tadah::core::aed_type& ff) const override;
        void set_basis(const tadah::core::Matrix ) override {}

        /** One weight per descriptor entry (dsize): the linear kernel
         *  acts on the descriptor itself and keeps no basis. */
        std::size_t weights_count(std::size_t dsize) const override;
        /** 0: no basis. */
        std::size_t basis_vector_size() const override;

        double epredict(const tadah::core::t_type &weights, const tadah::core::aed_type& aed) const override;
        double fpredict(const tadah::core::t_type &weights, const tadah::core::fd_type &fdij,
                const tadah::core::aed_type& aedi, const size_t k) const override;

        tadah::core::force_type fpredict(const tadah::core::t_type &weights, const tadah::core::fd_type &fdij,
                const tadah::core::aed_type& aedi) const override;
};
} // namespace models
} // namespace tadah
#endif
