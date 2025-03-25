#ifndef TADAH_MLIP_CLI_TADAH_CLI_H
#define TADAH_MLIP_CLI_TADAH_CLI_H

#include "CLI/CLI.hpp"
#include "cli_plugin.h"
#include <memory>
#include <vector>
#include <string>

#include <tadah.mlip/cli/all_cli_files.h>

namespace tadah {
namespace mlip {
namespace cli {

/**
 * @brief Main CLI driver.
 *
 * Sets up the full command hierarchy and dispatches user input.
 */
class TadahCLI {
public:
  /**
   * @brief Constructs a new TadahCLI.
   */
  TadahCLI();

  /**
   * @brief Parses command-line arguments and runs the corresponding command.
   * @param argc Argument count.
   * @param argv Argument vector.
   * @return int Exit code.
   */
  int run(int argc, char** argv);

private:
  CLI::App app_;
  std::vector<std::unique_ptr<CLIPlugin>> plugins_;
  /*std::unique_ptr<class TrainCLI> train_cli_;*/
  std::unique_ptr<class PredictCLI> predict_cli_;
  /*std::unique_ptr<class HPOCLI> hpo_cli_;*/
  /*std::unique_ptr<class DataCLI> data_cli_;*/
  /*std::unique_ptr<class TransformCLI> transform_cli_;*/
  /*std::unique_ptr<class PropertiesCLI> properties_cli_;*/
  /*std::unique_ptr<class AnalysisCLI> analysis_cli_;*/

  /**
   * @brief Registers all command modules.
   */
  void registerModules();
};

} // namespace cli
} // namespace mlip
} // namespace tadah

#endif // TADAH_MLIP_CLI_TADAH_CLI_H

