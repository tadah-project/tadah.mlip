/**
 * @file clitask.h
 * @brief Extends the ITaskInput interface to encapsulate CLI-derived task data.
 *
 * This file declares the CLITask structure, which inherits from ITaskInput
 * so that it can be used directly by TaskFactory without requiring
 * TaskFactory to know about CLI details.
 */

#ifndef TADAH_MLIP_CLI_CLITASK_H
#define TADAH_MLIP_CLI_CLITASK_H

#include <string>
#include <vector>
#include <unordered_map>

#include <tadah/mlip/itask_input.h>

namespace tadah {
namespace mlip {
namespace cli {

/**
 * @struct CLITask
 * @brief Stores parsed command-line data and blueprintTaskName, while implementing ITaskInput.
 *
 * CLITask is directly filled by TadahCLI during command-line parsing.
 * The blueprintTaskName is the fully resolved identifier for the task,
 * such as "data.split" or "train.model". The storage map holds keys
 * and their associated values.
 */
struct CLITask : public tadah::mlip::ITaskInput {
  /**
   * @brief Holds user-provided data from the CLI, keyed by the final command-line
   *        argument name. For example: "rcut2b" -> {"5.0"}.
   *
   * The keys are lowercase or normalized as needed by TadahCLI, but the factory
   * itself may uppercase them when building the Context.
   */
  std::unordered_map<std::string, std::vector<std::string>> storage;

  /**
   * @brief The name of the task per blueprint, e.g. "data.split".
   */
  std::string blueprintTaskName;

  /**
   * @brief Provides the task name (in blueprint format).
   *
   * @return The string name, e.g. "data.split".
   */
  std::string getTaskName() const override;

  /**
   * @brief Converts internal storage to a std::map for stable key ordering.
   *
   * @return A map of string -> vector of string representing the CLI data.
   */
  std::unordered_map<std::string, std::vector<std::string>> getKeyValueData() const override;
};

} // namespace cli
} // namespace mlip
} // namespace tadah

#endif // TADAH_MLIP_CLI_CLITASK_H

