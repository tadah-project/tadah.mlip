#ifndef TADAH_MLIP_CLI_ANALYSIS_CLI_H
#define TADAH_MLIP_CLI_ANALYSIS_CLI_H

#include "CLI/CLI.hpp"
#include "cli_plugin.h"
#include <string>
#include <vector>

namespace tadah {
namespace cli {

/**
 * @brief Implements visual analysis commands.
 *
 * Provides subcommands for:
 *   descriptor, bfunc, and cutoff evaluation.
 */
class AnalysisCLI : public CLIPlugin {
public:
  void register_module(CLI::App &app) override;
  int run_descriptor();
  int run_bfunc();
  int run_cutoff();

private:
  std::string pot_file_;
  std::vector<std::string> datasets_;
  std::string config_file_;
  std::vector<double> range_;
  std::vector<std::string> types_;
  std::string out_file_;
  int numeric_precision_ = 0;
  bool force_descriptors_ = false;
};

} // namespace cli
} // namespace tadah

#endif // TADAH_MLIP_CLI_ANALYSIS_CLI_H

