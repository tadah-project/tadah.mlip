#ifndef TADAH_MLIP_ENGINES_TRAIN_ENGINE_H
#define TADAH_MLIP_ENGINES_TRAIN_ENGINE_H

#include <tadah/core/config.h>

namespace tadah {
namespace engines {

/**
 * @brief Provides core training functionality.
 *
 * Delegates model training to the underlying Trainer.
 *
 * @param config The configuration object.
 * @return int EXIT_SUCCESS or EXIT_FAILURE.
 */
class TrainEngine {
public:
  static int train(const tadah::core::Config &config);
};

} // namespace engines
} // namespace tadah

#endif // TADAH_MLIP_ENGINES_TRAIN_ENGINE_H

