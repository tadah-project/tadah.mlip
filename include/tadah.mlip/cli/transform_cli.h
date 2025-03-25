#ifndef TADAH_MLIP_CLI_TRANSFORM_CLI_H
#define TADAH_MLIP_CLI_TRANSFORM_CLI_H

#include "CLI/CLI.hpp"
#include "cli_plugin.h"
#include <string>
#include <vector>

namespace tadah {
namespace mlip {
namespace cli {

/**
 * @brief Implements structure transformation commands.
 *
 * Provides subcommands for rescale, rotate, shear, and change-coords.
 */
class TransformCLI : public CLIPlugin {
public:
  void register_module(CLI::App &app) override;
  int run_rescale();
  int run_rotate();
  int run_shear();
  int run_change_coords();

private:
  std::vector<std::string> datasets_;
  std::string out_file_;
  double eshift_val_ = 0.0;
  bool scale_ = false;
  int numeric_precision_ = 0;
};

} // namespace cli
} // namespace mlip
} // namespace tadah

#endif // TADAH_MLIP_CLI_TRANSFORM_CLI_H

