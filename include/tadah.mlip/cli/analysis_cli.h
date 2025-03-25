#ifndef TADAH_MLIP_CLI_ANALYSIS_CLI_H
#define TADAH_MLIP_CLI_ANALYSIS_CLI_H

#include "CLI/CLI.hpp"
#include "cli_plugin.h"
#include <string>
#include <vector>

namespace tadah {
namespace mlip {
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

};

} // namespace cli
} // namespace mlip
} // namespace tadah

#endif // TADAH_MLIP_CLI_ANALYSIS_CLI_H

