#ifndef M_BLR_CORE_H
#define M_BLR_CORE_H

#include <tadah/models/m_core.h>
#include <tadah/models/functions/function_base.h>
#include <tadah/models/functions/basis_functions/bf_base.h>
#include <tadah/core/context.h>

#include <iostream>

namespace tadah {
namespace models {

/**
 * @class M_BLR_Core
 * @brief Implements core functionality for Basis Linear Regression (BLR).
 *
 * This template class supports BLR using a specified basis function type.
 *
 * @tparam BF Basis function type, default is Function_Base.
 */
template <class BF = Function_Base&>
class M_BLR_Core : public virtual M_Core {
public:
    tadah::core::Context &context; ///< Configuration reference.
    BF bf; ///< Basis function.

    /**
     * @brief Constructor using a configuration object.
     *
     * Initializes the basis function with the provided configuration and checks its type.
     *
     * @param c Configuration object reference.
     */
    M_BLR_Core(tadah::core::Context &c) : context(c), bf(c) {
        static_assert(std::is_base_of<BF_Base, BF>::value,
                      "\nM_BLR takes a basis function, such as BF_Linear; "
                      "a kernel, such as Kern_Linear, is taken by M_KRR.\n");
        init();
    }

    /**
     * @brief Constructor using a basis function and configuration object.
     *
     * Verifies the basis function is of the correct type and initializes.
     *
     * @param bf Basis function object.
     * @param c Configuration object reference.
     */
    M_BLR_Core(BF &bf, tadah::core::Context &c) : context(c), bf(bf) {
        if (dynamic_cast<BF_Base*>(&bf) == nullptr)
            throw std::invalid_argument("Provided object is not of BF_Base type");
        init();
    }

    /**
     * @brief Predicts output using the basis function.
     *
     * @param v Input vector for prediction.
     * @return Predicted value.
     */
    double predict(const tadah::core::aed_type &v) const {
        return bf.epredict(get_weights(), v);
    }

private:
    /**
     * @brief Initializes the model based on the configuration.
     *
     * Loads weights if available in the configuration and sets the verbose level.
     */
    void init() {
        if (context.contains("WEIGHTS")) {
            weights.resize(context.size("WEIGHTS"));
            context.get<tadah::core::t_type>("WEIGHTS", weights);
            // trained = true;
        }
        verbose = context.get<int>("VERBOSE");

    }
};

} // namespace models
} // namespace tadah
#endif // M_BLR_CORE_H
