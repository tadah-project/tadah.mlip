#include <tadah/cli/tadah_cli.h>
#include <tadah/cli/train_cli.h>
#include <tadah/cli/predict_cli.h>
#include <tadah/cli/hpo_cli.h>
#include <tadah/cli/data_cli.h>
#include <tadah/cli/transform_cli.h>
#include <tadah/cli/properties_cli.h>
#include <tadah/cli/analysis_cli.h>
#include <tadah/cli/common_cli.h>
#include <iostream>
#include <memory>

namespace tadah {
namespace cli {

TadahCLI::TadahCLI() : app_("Tadah! - Machine Learning Interatomic Potentials\n"
"------------------------------------------------\n"
"Type 'tadah -h <subcommand>' for help (use --help for extended help).\n") {
  registerModules();
}

void TadahCLI::registerModules() {
  train_cli_ = std::make_unique<TrainCLI>();
  train_cli_->register_module(app_);

  predict_cli_ = std::make_unique<PredictCLI>();
  predict_cli_->register_module(app_);

  hpo_cli_ = std::make_unique<HPOCLI>();
  hpo_cli_->register_module(app_);

  data_cli_ = std::make_unique<DataCLI>();
  data_cli_->register_module(app_);

  transform_cli_ = std::make_unique<TransformCLI>();
  transform_cli_->register_module(app_);

  properties_cli_ = std::make_unique<PropertiesCLI>();
  properties_cli_->register_module(app_);

  analysis_cli_ = std::make_unique<AnalysisCLI>();
  analysis_cli_->register_module(app_);
}

int TadahCLI::run(int argc, char** argv) {
  tadah::cli::checkFlagSuggestions(argc, argv);
  try {
    app_.parse(argc, argv);
    if (app_.parsed_options().empty())
      std::cout << "Type 'tadah --help' for help." << std::endl;
  } catch (const CLI::ParseError &e) {
    return app_.exit(e);
  }
  return EXIT_SUCCESS;
}

} // namespace cli
} // namespace tadah

