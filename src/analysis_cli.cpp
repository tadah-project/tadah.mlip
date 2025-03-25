#include <tadah/core/blueprint_parser.h>
#include <tadah.mlip/cli/analysis_cli.h>
#include <tadah/mlip/engines/analysis_engine.h>
#include "CLI/Timer.hpp"
#include <iostream>
#include <cstdlib>

namespace tadah {
namespace mlip {
namespace cli {

void AnalysisCLI::register_module(CLI::App &app) {
  auto &taskMap = core::BlueprintParser::getTasksMapRef();
  auto taskAnalysis = taskMap.at("analysis");
  /*auto analysis = app.add_subcommand("analysis", "Visual analysis commands.");*/
  /**/
  /*auto descriptor = analysis->add_subcommand("descriptor",*/
  /*  "Calculate structure descriptors.\nExample: tadah analysis descriptor -d dataset.tadah -p pot.tadah -o out.txt --indices 1,3,5");*/
  /*descriptor->add_option("-d,--dataset", datasets_, "Tadah! dataset file (required; excludes --structure)")*/
  /*           ->required()->check(CLI::ExistingFile);*/
  /*descriptor->add_option("-s,--structure", sources_, "Unified structure input (required; excludes --dataset)")*/
  /*           ->check(CLI::ExistingFile);*/
  /*descriptor->add_option("-p,--potential", potFile_, "Trained model file")*/
  /*           ->required()->check(CLI::ExistingFile);*/
  /*descriptor->add_option("-o,--outfile", outFiles_, "Output file (optional)")*/
  /*           ->check(CLI::NonexistentPath);*/
  /*descriptor->add_option("-i,--indices", types_, "Indices (comma-separated)");*/
  /*descriptor->add_flag("-F,--force", force_, "Calculate force descriptors");*/
  /*descriptor->add_option("-V,--verbose", verbose_, "Enable verbose output");*/
  /*descriptor->callback([this]() { std::exit(this->run_descriptor()); });*/
  /**/
  /*auto bfunc = analysis->add_subcommand("bfunc",*/
  /*  "Evaluate and plot basis functions (bfunc).\nExample: tadah analysis bfunc -c config.tadah -o out.txt -r 0 5 500 -t \"2b Y\"");*/
  /*bfunc->add_option("-c,--config", configFile_, "Configuration file with grid data")*/
  /*     ->required()->check(CLI::ExistingFile);*/
  /*bfunc->add_option("-o,--outfile", outFiles_, "Output file")*/
  /*     ->required()->check(CLI::NonexistentPath);*/
  /*bfunc->add_option("-r,--range", range_, "Plotting range (START, STOP, NPOINTS)")*/
  /*     ->required();*/
  /*bfunc->add_option("-t,--type", types_, "Grid type and cutoff option")*/
  /*     ->required();*/
  /*bfunc->add_flag("--derivative", "Plot derivatives");*/
  /*bfunc->add_flag("--scale-by-cutoff",  "Rescale by cutoff value");*/
  /*bfunc->add_option("--indices", indicesStr_, "Basis function indices (optional)");*/
  /*bfunc->add_option("--numeric", numericPrecision_, "Decimal precision");*/
  /*bfunc->add_option("-V,--verbose", verbose_, "Enable verbose output");*/
  /*bfunc->callback([this]() { std::exit(this->run_bfunc()); });*/
  /**/
  /*auto cutoff = analysis->add_subcommand("cutoff",*/
  /*  "Evaluate and plot cutoff functions.\nExample: tadah analysis cutoff -o out.txt -r 0 3.14 100 -t \"CutCos\" --derivative");*/
  /*cutoff->add_option("-o,--outfile", outFiles_, "Output file")*/
  /*      ->required()->check(CLI::NonexistentPath);*/
  /*cutoff->add_option("-r,--range", range_, "Plotting range (START, STOP, NPOINTS)")*/
  /*      ->required();*/
  /*cutoff->add_option("-t,--type", types_, "Cutoff function type(s)")*/
  /*      ->required();*/
  /*cutoff->add_flag("--derivative", "Plot derivative");*/
  /*cutoff->add_option("--numeric", numericPrecision_, "Decimal precision");*/
  /*cutoff->add_option("-V,--verbose", verbose_, "Enable verbose output");*/
  /*cutoff->callback([this]() { std::exit(this->run_cutoff()); });*/
}

int AnalysisCLI::run_descriptor() {
  return EXIT_SUCCESS;
}

int AnalysisCLI::run_bfunc() {
  return EXIT_SUCCESS;
}

int AnalysisCLI::run_cutoff() {
  return EXIT_SUCCESS;
}

} // namespace cli
} // namespace mlip
} // namespace tadah

