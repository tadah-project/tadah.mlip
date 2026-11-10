#ifndef BF_LINEAR_H
#define BF_LINEAR_H
#include <tadah/models/functions/basis_functions/bf_base.h>

namespace tadah {
namespace models {

/**
 * @brief Linear basis function for modeling linear relationships.
 *
 * This structure provides methods for predicting energy and forces using a linear basis function.
 */
struct BF_Linear : public virtual BF_Base {
    
    /**
     * @brief Default constructor.
     */
    BF_Linear();

    /**
     * @brief Constructor with configuration.
     * @param c Configuration object reference.
     */
    BF_Linear(const tadah::core::Context &c);

    /**
     * @brief Retrieve the label of the basis function.
     * @return String label of the basis function.
     */
    std::string get_label() const override;
    /** \brief See Function_Base::intercept_columns(). */
    std::vector<size_t> intercept_columns(const tadah::core::Context &c) const override;

    /** @brief One weight per descriptor entry: dsize. */
    std::size_t weights_count(std::size_t dsize) const override;

    std::string label = "BF_Linear"; ///< Label identifying the basis function.

    /**
     * @brief Predicts energy using the linear basis function.
     * @param weights Weights vector.
     * @param aed Atomic descriptors.
     * @return Predicted energy.
     */
    double epredict(const tadah::core::t_type &weights, const tadah::core::aed_type &aed) const override;

    /**
     * @brief Predicts force for a specific dimension using the linear basis function.
     * @param weights Weights vector.
     * @param fdij Force descriptor between atoms.
     * @param aedi Atomic descriptors for an individual atom.
     * @param k Dimension index.
     * @return Predicted force component.
     */
    double fpredict(const tadah::core::t_type &weights, const tadah::core::fd_type &fdij,
                    const tadah::core::aed_type &aedi, const size_t k) const override;

    /**
     * @brief Predicts forces using the linear basis function.
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
