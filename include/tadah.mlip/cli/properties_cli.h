#ifndef TADAH_MLIP_CLI_PROPERTIES_CLI_H
#define TADAH_MLIP_CLI_PROPERTIES_CLI_H

#include "CLI/CLI.hpp"
#include "cli_plugin.h"
#include <string>
#include <vector>

namespace tadah {
namespace mlip {
namespace cli {

/**
 * @brief Implements physical property evaluation commands.
 *
 * Provides subcommands for evaluating properties:
 *   twobody, ecurve, eos, defect, surface, mechanics.
 */
class PropertiesCLI : public CLIPlugin {
public:
  void register_module(CLI::App &app) override;
  int run_twobody();
  int run_ecurve();
  int run_eos();
  int run_defect();
  int run_surface();
  int run_mechanics();

private:
  std::string pot_file_;
  std::vector<std::string> structures_;  ///< Unified structure input.
  std::vector<double> range_;
  std::vector<std::string> atoms_;
  std::string quantity_;
  int numeric_precision_ = 0;
  bool predict_force_ = false;
  bool predict_stress_ = false;
  bool predict_error_ = false;
};

} // namespace cli
} // namespace mlip
} // namespace tadah

#endif // TADAH_MLIP_CLI_PROPERTIES_CLI_H

