#include <tadah.mlip/cli/train_cli.h>
#include <tadah/mlip/engines/train_engine.h>
#include <tadah/core/config.h>
#include "CLI/Timer.hpp"
#include <iostream>
#include <cstdlib>

namespace tadah {
namespace mlip {
namespace cli {

void TrainCLI::register_module(CLI::App &app) {
  /*auto train = app.add_subcommand("train",*/
  /*  "Train a model using the specified configuration file.\nExample: tadah train -c config.tadah");*/
  /*train->add_option("-c,--config", config_file_, "Path to configuration file")*/
  /*       ->required()->check(CLI::ExistingFile);*/
  /*train->add_flag("-V,--verbose", verbose_, "Enable verbose output");*/
  /*train->add_flag("-F,--force", include_force_, "Include forces in training");*/
  /*train->add_flag("-S,--stress", include_stress_, "Include stresses in training");*/
  /*train->callback([this]() { std::exit(this->run()); });*/
}

int TrainCLI::run() {
  return EXIT_SUCCESS;
}

} // namespace cli
} // namespace mlip
} // namespace tadah
