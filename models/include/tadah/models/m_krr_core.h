#ifndef M_KRR_Core_H
#define M_KRR_Core_H

#include <tadah/core/context.h>
#include <tadah/models/functions/function_base.h>
#include <tadah/models/functions/kernels/kern_base.h>
#include <tadah/models/m_core.h>

#include <iostream>
#include <stdexcept>
#include <string>

namespace tadah {
namespace models {

/**
 * @class M_KRR_Core
 * @brief Implements core functionality for Kernel Ridge Regression (KRR).
 *
 * This template class supports Kernel Ridge Regression using a specified kernel type.
 * 
 * @tparam K Kernel type, default is Function_Base.
 */
template <class K = Function_Base&>
class M_KRR_Core : public virtual M_Core {
public:
    tadah::core::Context &context; ///< Configuration reference.
    K kernel; ///< Kernel function.

    /**
     * @brief Constructor using a configuration object.
     * 
     * Initializes the kernel with the provided configuration and checks its type.
     *
     * @param c Configuration object reference.
     */
    M_KRR_Core(tadah::core::Context &c) :
        context(c),
        kernel(c)
    {
        static_assert(std::is_base_of<Kern_Base, K>::value,
                      "\nM_KRR takes a kernel, such as Kern_Linear; a basis "
                      "function, such as BF_Linear, is taken by M_BLR.\n");
        init();
    }

    /**
     * @brief Constructor using a kernel and configuration object.
     * 
     * Verifies the kernel is of the correct type and initializes.
     *
     * @param kernel Kernel object.
     * @param c Configuration object reference.
     */
    M_KRR_Core(K &kernel, tadah::core::Context &c) :
        context(c),
        kernel(kernel)
    {
        if (dynamic_cast<Kern_Base*>(&kernel) == nullptr)
            throw std::invalid_argument("Provided object is not of Kern_Base type");
        init();
    }

    /**
     * @brief Predicts output using the kernel function.
     * 
     * @param v Input vector for prediction.
     * @return Predicted value.
     */
    double predict(const tadah::core::aed_type &v) const {
        return kernel.epredict(weights, v);
    }

private:
    /**
     * @brief Initializes the model based on the configuration.
     * 
     * Loads weights if available in the configuration and sets the verbose
     * level. Loaded weights do not make the model trained (is_trained(), as
     * M_BLR_Core): train() fits it, replacing them. MLIP's Trainer, which
     * builds the models that tadah train and tadah hpo fit, gives it a
     * configuration without WEIGHTS and BASIS (a potential used as a
     * training configuration, `tadah train -c pot.tadah`, may have another
     * SBASIS than its BASIS: the fit draws a new basis).
     *
     * A non-linear kernel (any kernel but Kern_Linear) evaluates its
     * weights against its basis vectors, which it read from SBASIS and
     * BASIS (Kern_Base::read_basis_from_context): with WEIGHTS, the basis
     * must be there and hold one vector per weight, else this throws
     * naming the keys (without a basis the model predicted E = 0, and with
     * fewer vectors than weights it read past the basis).
     */
    void init() {
        if (context.contains("WEIGHTS")) {
            weights.resize(context.size("WEIGHTS"));
            context.get<tadah::core::t_type>("WEIGHTS", weights);
            // Loaded weights predict, but the model is not trained: train()
            // fits it (a potential used as the configuration), as M_BLR.
            check_basis_of_weights();
        }
        verbose = context.get<int>("VERBOSE");

    }

    void check_basis_of_weights() const {
        if (kernel.get_label() == "Kern_Linear") return;
        const Kern_Base *kb = dynamic_cast<const Kern_Base*>(&kernel);
        if (kb == nullptr) return;   // the constructors refuse a non-kernel
        const size_t nb = kb->get_basis().cols();
        if (nb == 0)
            throw std::runtime_error(
                "M_KRR " + kernel.get_label() + ": the potential has WEIGHTS "
                "but no basis vectors (SBASIS and BASIS): a non-linear kernel "
                "evaluates its weights against the basis vectors that "
                "training wrote with them.");
        if (nb != weights.size())
            throw std::runtime_error(
                "M_KRR " + kernel.get_label() + ": the potential has " +
                std::to_string(weights.size()) + " WEIGHTS but SBASIS " +
                std::to_string(nb) + " basis vectors (BASIS): a non-linear "
                "kernel has one weight per basis vector.");
    }
};

} // namespace models
} // namespace tadah
#endif // M_KRR_Core_H
