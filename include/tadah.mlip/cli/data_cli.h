#ifndef TADAH_MLIP_CLI_DATA_CLI_H
#define TADAH_MLIP_CLI_DATA_CLI_H

#include "CLI/CLI.hpp"
#include "cli_plugin.h"
#include <string>
#include <vector>

namespace tadah {
namespace mlip {
namespace cli {

/**
 * @brief Implements dataset management commands.
 *
 * Provides subcommands: convert, read, write, merge, split, dedup, sample, balance.
 */
class DataCLI : public CLIPlugin {
public:
  void register_module(CLI::App &app) override;
  int run_convert();
  int run_read();
  int run_write();
  int run_merge();
  int run_split();
  int run_dedup();
  int run_sample();
  int run_balance();

private:
  std::vector<std::string> datasets_;
  std::string out_file_;
  std::vector<std::string> out_files_;
};

} // namespace cli
} // namespace mlip
} // namespace tadah

#endif // TADAH_MLIP_CLI_DATA_CLI_H
