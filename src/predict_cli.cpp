#include <tadah/cli/predict_cli.h>
#include <tadah/mlip/engines/predict_engine.h>
#include <tadah/core/config.h>
#include <tadah/mlip/StructureDB.h>
#include <tadah/cli/unified_input.h>
#include <tadah/cli/logging.h>
#include "CLI/Timer.hpp"
#include <iostream>
#include <cstdlib>

namespace tadah {
namespace cli {

void PredictCLI::register_module(CLI::App &app) {
  auto predict = app.add_subcommand("predict",
    "Perform prediction using a trained model.\nEnergy per atom is always calculated; forces/stresses are optional.");
  auto input_group = predict->add_option_group("Input Selection",
    "Exactly one of --config, --dataset or --structure must be provided");
  input_group->add_option("-c,--config", config_file_, "Path to configuration file")
              ->check(CLI::ExistingFile);
  input_group->add_option("-d,--dataset", datasets_, "Tadah! dataset file(s)")
              ->check(CLI::ExistingFile);
  input_group->add_option("-s,--structure", sources_, "Unified structure input (files/online)")
              ->required();
  predict->require_option(1,1);

  predict->add_flag("-F,--force", predict_force_, "Predict forces");
  predict->add_flag("-S,--stress", predict_stress_, "Predict stresses");
  predict->add_flag("-V,--verbose", verbose_, "Enable verbose output");
  predict->add_flag("-E,--error", predict_error_, "Predict model error");
  predict->add_flag("-A,--analytics", analytics_, "Perform analytics on predictions")
           ->excludes("-s");
  predict->add_option("-p,--potential", pot_file_, "Trained model file")
           ->required()->check(CLI::ExistingFile);
  predict->add_option("--numeric", numeric_precision_, "Numeric output precision");

  predict->callback([this]() { std::exit(this->run()); });
}

int PredictCLI::run() {
  CLI::Timer timer_tot("Prediction", CLI::Timer::Big);
  try {

  Logger::instance().log(LogLevel::INFO, "Loading structures...");
  StructureDB stdb;
  if(predict->count("--config")) {
    pot_config.add(config_file);
    stdb.add(pot_config);
  }
  else if(predict->count("--datasets")) {
    for (auto &ds: datasets) {
      pot_config.add("DBFILE",ds);
    }
    stdb.add(pot_config);
  }
  else if(predict->count("--sources")) {
    for (std::string &src : sources) {
      auto reader = StructureReaderSelector::getReader(src);
      reader->read(src);
      Structure s = reader->getStructure();
      stdb.add(s);
    }
  } else {
    std::runtime_error("Either provide config file\n\
or datasets in a command line");
  }

  if (is_verbose()) std::cout << "Done!" << std::endl;

  // If force or Stress flag is set add it to config
  // overwrite existing flags if present.
  // If flags are not set either keep existing
  // setting from the config file or set to false
  if(predict->count("--Force")) {
    pot_config.remove("FORCE");
    pot_config.add("FORCE", "true");
  }
  if(predict->count("--Stress")) {
    pot_config.remove("STRESS");
    pot_config.add("STRESS", "true");
  }



    /////////////
    tadah::core::Config pot_config(pot_file_);
    pot_config.check_for_predict();
    pot_config.remove("verbose");
    pot_config.add("verbose", 0);

    tadah::mlip::StructureDB stdb;
    auto allSources = UnifiedInput::processInput(sources_);
    if (!config_file_.empty()) {
      pot_config.add(config_file_);
      stdb.add(pot_config);
    } else if (!datasets_.empty()) {
      for (const auto &ds : datasets_)
        pot_config.add("DBFILE", ds);
      stdb.add(pot_config);
    } else {
      for (const auto &src : allSources) {
        std::string actual_src = src.substr(src.find(":") + 1);
        tadah::mlip::Structure dummy_structure;
        stdb.add(dummy_structure);
      }
    }

    if (predict_force_) {
      pot_config.remove("force");
      pot_config.add("force", "true");
    }
    if (predict_stress_) {
      pot_config.remove("stress");
      pot_config.add("stress", "true");
    }

    tadah::engines::PredictEngine::predict(pot_config, stdb);
    Logger::log(timer_tot.to_string());
    return EXIT_SUCCESS;
  } catch (const std::exception &e) {
    std::cerr << "Prediction error: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }
}

} // namespace cli
} // namespace tadah

