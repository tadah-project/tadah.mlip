#ifndef M_CORE_H
#define M_CORE_H

#include <tadah/core/core_types.h>

namespace tadah {
namespace models {

/**
 * @class M_Core
 * @brief Abstract base class for model cores.
 *
 * Provides common functionality and interface for model implementations, including training status
 * and weight management.
 */
class M_Core {
public:
    int verbose = 0; ///< Verbose level for logging.
    bool trained = false; ///< Indicates if the model has been trained.
    tadah::core::t_type weights; ///< Weights vector for the model.

  int is_verbose() {return verbose;}

    /**
     * @brief Virtual destructor for polymorphic deletion.
     */
    virtual ~M_Core() {}

    /**
     * @brief Checks if the model has been trained.
     *
     * @return True if the model is trained, otherwise false.
     */
    bool is_trained() const {
        return trained;
    }

    /** Reset trained state so the model can be re-trained in-place. */
    void reset_trained() {
        trained = false;
    }

    /**
     * @brief Retrieves the weights of the model.
     *
     * @return Constant reference to the weights vector.
     */
    const tadah::core::t_type &get_weights() const {
        return weights;
    }

    /**
     * @brief Sets the model weights.
     *
     * @param w New weights vector to be set.
     */
    void set_weights(const tadah::core::t_type w) {
        weights = w;
    }

    /**
     * @brief Pure virtual function for making predictions.
     *
     * Must be implemented by derived classes.
     *
     * @param v Input vector for prediction.
     * @return Predicted value.
     */
    virtual double predict(const tadah::core::aed_type &v) const = 0;

};

} // namespace models
} // namespace tadah
#endif // M_CORE_H

