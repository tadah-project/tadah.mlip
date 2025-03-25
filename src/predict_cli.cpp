#include <tadah.mlip/cli/predict_cli.h>
#include <tadah.mlip/cli/unified_input.h>
#include <tadah/mlip/engines/predict_engine.h>
#include <tadah/core/config.h>
#include <tadah/core/blueprint_parser.h>
#include <tadah/mlip/structure_db.h>
#include "CLI/Timer.hpp"
#include <iostream>
#include <cstdlib>

namespace tadah {
namespace mlip {
namespace cli {

void PredictCLI::register_module(CLI::App &app) {
  auto &tasksMap = core::BlueprintParser::getTasksMapRef();
  auto &predictTask = tasksMap.at("predict");
  for (const auto &param : predictTask) {
    std::cout << param.first << " " << param.second << std::endl;
  }



  auto predict = app.add_subcommand("predict",
    "Perform prediction using a trained model.\nEnergy per atom is always calculated; forces/stresses are optional.");
  /*auto input_group = predict->add_option_group("Input Selection",*/
  /*  "Exactly one of --config, --dataset or --structure must be provided");*/
  /*input_group->add_option("-c,--config", config_file_, "Path to configuration file")*/
  /*            ->check(CLI::ExistingFile);*/
  /*input_group->add_option("-d,--dataset", datasets_, "Tadah! dataset file(s)")*/
  /*            ->check(CLI::ExistingFile);*/
  /*input_group->add_option("-s,--structure", sources_, "Unified structure input (files/online)")*/
  /*            ->required();*/
  /*predict->require_option(1,1);*/
  /**/
  predict->add_flag("-F,--force", force_, "Predict forces");
  /*predict->add_flag("-S,--stress", predict_stress_, "Predict stresses");*/
  /*predict->add_flag("-V,--verbose", verbose_, "Enable verbose output");*/
  /*predict->add_flag("-E,--error", predict_error_, "Predict model error");*/
  /*predict->add_flag("-A,--analytics", analytics_, "Perform analytics on predictions")*/
  /*         ->excludes("-s");*/
  /*predict->add_option("-p,--potential", pot_file_, "Trained model file")*/
  /*         ->required()->check(CLI::ExistingFile);*/
  /*predict->add_option("--numeric", numeric_precision_, "Numeric output precision");*/
  /**/
  predict->callback([this]() { std::exit(this->run()); });
}

int PredictCLI::run() {
  std::cout << "Predicting..." << std::endl;
    return EXIT_FAILURE;
}

} // namespace cli
} // namespace mlip
} // namespace tadah

