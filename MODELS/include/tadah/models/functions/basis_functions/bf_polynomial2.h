#ifndef BF_POLYNOMIAL2_H
#define BF_POLYNOMIAL2_H
#include <tadah/models/functions/basis_functions/bf_base.h>

namespace tadah {
namespace models {

/**
 * @brief Polynomial order 2 basis function with coefficient c=0.
 *
 * This structure provides methods for predicting energy and forces using a second-order polynomial basis function.
 */
struct BF_Polynomial2 : public virtual BF_Base {

    /**
     * @brief Default constructor.
     */
    BF_Polynomial2();

    /**
     * @brief Constructor with configuration.
     * @param c Configuration object reference.
     */
    BF_Polynomial2(const tadah::core::Context &c);

    std::string label = "BF_Polynomial2"; ///< Label identifying the basis function.

    /**
     * @brief Retrieve the label of the basis function.
     * @return String label of the basis function.
     */
    std::string get_label() const override;
    /** \brief See Function_Base::intercept_columns(). */
    std::vector<size_t> intercept_columns(const tadah::core::Context &c) const override;

    /**
     * @brief One weight per product aed(i)*aed(ii), i <= ii: dsize (dsize + 1) / 2.
     *
     * With BIAS true the products of two different intercept columns are
     * identically zero (an atom has one species), so N (N - 1) / 2 of these
     * weights have no data: N = 1 has none.
     */
    std::size_t weights_count(std::size_t dsize) const override;

    /**
     * @brief Predicts energy using the polynomial basis function.
     * @param weights Weights vector.
     * @param aed Atomic descriptors.
     * @return Predicted energy.
     */
    double epredict(const tadah::core::t_type &weights, const tadah::core::aed_type &aed) const override;

    /**
     * @brief Predicts force for a specific dimension using the polynomial basis function.
     * @param weights Weights vector.
     * @param fdij Force descriptor between atoms.
     * @param aedi Atomic descriptors for an individual atom.
     * @param k Dimension index.
     * @return Predicted force component.
     */
    double fpredict(const tadah::core::t_type &weights, const tadah::core::fd_type &fdij,
                    const tadah::core::aed_type &aedi, const size_t k) const override;

    /**
     * @brief Predicts forces using the polynomial basis function.
     * @param weights Weights vector.
     * @param fdij Force descriptor between atoms.
     * @param aedi Atomic descriptors for an individual atom.
     * @return Predicted forces.
     */

    tadah::core::force_type fpredict(const tadah::core::t_type &weights, const tadah::core::fd_type &fdij,
                        const tadah::core::aed_type &aedi) const override;
};
} // namespace models
} // namespace tadah
#endif
