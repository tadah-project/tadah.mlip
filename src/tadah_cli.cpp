#include <tadah.mlip/cli/tadah_cli.h>
#include <tadah.mlip/cli/train_cli.h>
#include <tadah.mlip/cli/predict_cli.h>
#include <tadah.mlip/cli/hpo_cli.h>
#include <tadah.mlip/cli/data_cli.h>
#include <tadah.mlip/cli/transform_cli.h>
#include <tadah.mlip/cli/properties_cli.h>
#include <tadah.mlip/cli/analysis_cli.h>
#include <tadah.mlip/cli/common_cli.h>
#include <iostream>
#include <memory>

namespace tadah {
namespace mlip {
namespace cli {

TadahCLI::TadahCLI()
    : app_("Tadah! - Machine Learning Interatomic Potentials\n"
           "------------------------------------------------\n"
           "Type 'tadah -h <subcommand>' for help (use --help for extended "
           "help).\n") {
  registerModules();
}

void TadahCLI::registerModules() {
  /*train_cli_ = std::make_unique<TrainCLI>();*/
  /*train_cli_->register_module(app_);*/

  /**/
  predict_cli_ = std::make_unique<PredictCLI>();
  predict_cli_->register_module(app_);
  /**/
  /*hpo_cli_ = std::make_unique<HPOCLI>();*/
  /*hpo_cli_->register_module(app_);*/
  /**/
  /*data_cli_ = std::make_unique<DataCLI>();*/
  /*data_cli_->register_module(app_);*/
  /**/
  /*transform_cli_ = std::make_unique<TransformCLI>();*/
  /*transform_cli_->register_module(app_);*/
  /**/
  /*properties_cli_ = std::make_unique<PropertiesCLI>();*/
  /*properties_cli_->register_module(app_);*/

  /*analysis_cli_ = std::make_unique<AnalysisCLI>();*/
  /*analysis_cli_->register_module(app_);*/
}

int TadahCLI::run(int argc, char **argv) {
  checkFlagSuggestions(argc, argv);
  try {
    app_.parse(argc, argv);
    if (app_.count_all() == 1) {
      std::cout << "Type 'tadah --help' for help." << std::endl;
    }
  } catch (const CLI::CallForHelp &e) {
    std::cout << app_.help() << std::endl;
    return 0;
  } catch (const CLI::ParseError &e) {
    return app_.exit(e);
  }
  return EXIT_SUCCESS;
}

} // namespace cli
} // namespace mlip
} // namespace tadah

