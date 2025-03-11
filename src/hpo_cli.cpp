#include <tadah/cli/hpo_cli.h>
#include <tadah/engines/hpo_engine.h>
#include <tadah/core/config.h>
#include <tadah/cli/logging.h>
#include "CLI/Timer.hpp"
#include <iostream>
#include <cstdlib>

namespace tadah {
namespace cli {

void HPOCLI::register_module(CLI::App &app) {
  auto hpo = app.add_subcommand("hpo",
    "Optimize hyperparameters for model training.\nExample: tadah hpo -c config.tadah -h hpotargets.txt -v valid.tadah");
  hpo->add_option("-c,--config", config_file_, "Configuration file")
      ->required()->check(CLI::ExistingFile);
  hpo->add_option("-h,--hpotarget", hpotarget_file_, "HPO target file")
      ->required()->check(CLI::ExistingFile);
  hpo->add_option("-v,--validation", validation_file_, "Validation dataset file(s)")
      ->required()->check(CLI::ExistingFile);
  hpo->add_flag("-F,--force", include_force_, "Train with forces");
  hpo->add_flag("-S,--stress", include_stress_, "Train with stresses");
  hpo->add_flag("-V,--verbose", verbose_, "Enable verbose output");
  hpo->callback([this]() { std::exit(this->run()); });
}

int HPOCLI::run() {
  CLI::Timer timer_tot("HPO", CLI::Timer::Big);
  try {
    tadah::core::Config config(config_file_);
    int ret = tadah::engines::HPOEngine::optimize(config);
    Logger::log(timer_tot.to_string());
    return ret;
  } catch (const std::exception &e) {
    std::cerr << "HPO error: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }
}

} // namespace cli
} // namespace tadah

