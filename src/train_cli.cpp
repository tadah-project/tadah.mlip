#include <tadah/cli/train_cli.h>
#include <tadah/engines/train_engine.h>
#include <tadah/core/config.h>
#include <tadah/cli/logging.h>
#include "CLI/Timer.hpp"
#include <iostream>
#include <cstdlib>

namespace tadah {
namespace cli {

void TrainCLI::register_module(CLI::App &app) {
  auto train = app.add_subcommand("train",
    "Train a model using the specified configuration file.\nExample: tadah train -c config.tadah");
  train->add_option("-c,--config", config_file_, "Path to configuration file")
         ->required()->check(CLI::ExistingFile);
  train->add_flag("-V,--verbose", verbose_, "Enable verbose output");
  train->add_flag("-F,--force", include_force_, "Include forces in training");
  train->add_flag("-S,--stress", include_stress_, "Include stresses in training");
  train->callback([this]() { std::exit(this->run()); });
}

int TrainCLI::run() {
  CLI::Timer timer_tot("Training", CLI::Timer::Big);
  try {
    tadah::core::Config config(config_file_);
    if (include_force_) {
      config.remove("FORCE");
      config.add("FORCE", "true");
    }
    if (include_stress_) {
      config.remove("STRESS", "true");
      config.add("STRESS", "true");
    }
std::set<tadah::core::Element> unique_elements =
    StructureDB::find_unique_elements(config);
StructureDB::check_atoms_key(config, unique_elements);
StructureDB::check_watoms_key(config, unique_elements);

    int ret = tadah::engines::TrainEngine::train(config);
    Logger::log(timer_tot.to_string());
    return ret;
  } catch (const std::exception &ex) {
    std::cerr << "Training failed: " << ex.what() << std::endl;
    return EXIT_FAILURE;
  }
}

} // namespace cli
} // namespace tadah

