#include <tadah/cli/analysis_cli.h>
#include <tadah/engines/analysis_engine.h>
#include <tadah/cli/logging.h>
#include "CLI/Timer.hpp"
#include <iostream>
#include <cstdlib>

namespace tadah {
namespace cli {

void AnalysisCLI::register_module(CLI::App &app) {
  auto analysis = app.add_subcommand("analysis", "Visual analysis commands.");
  
  auto descriptor = analysis->add_subcommand("descriptor",
    "Calculate structure descriptors.\nExample: tadah analysis descriptor -d dataset.tadah -p pot.tadah -o out.txt --indices 1,3,5");
  descriptor->add_option("-d,--dataset", datasets_, "Tadah! dataset file (required; excludes --structure)")
             ->required()->check(CLI::ExistingFile);
  descriptor->add_option("-s,--structure", sources_, "Unified structure input (required; excludes --dataset)")
             ->check(CLI::ExistingFile);
  descriptor->add_option("-p,--potential", pot_file_, "Trained model file")
             ->required()->check(CLI::ExistingFile);
  descriptor->add_option("-o,--outfile", out_file_, "Output file (optional)")
             ->check(CLI::NonexistentPath);
  descriptor->add_option("-i,--indices", types_, "Indices (comma-separated)");
  descriptor->add_flag("-F,--force", force_descriptors_, "Calculate force descriptors");
  descriptor->add_flag("-V,--verbose", verbose_, "Enable verbose output");
  descriptor->callback([this]() { std::exit(this->run_descriptor()); });
  
  auto bfunc = analysis->add_subcommand("bfunc",
    "Evaluate and plot basis functions (bfunc).\nExample: tadah analysis bfunc -c config.tadah -o out.txt -r 0 5 500 -t \"2b Y\"");
  bfunc->add_option("-c,--config", config_file_, "Configuration file with grid data")
       ->required()->check(CLI::ExistingFile);
  bfunc->add_option("-o,--outfile", out_file_, "Output file")
       ->required()->check(CLI::NonexistentPath);
  bfunc->add_option("-r,--range", range_, "Plotting range (START, STOP, NPOINTS)")
       ->required();
  bfunc->add_option("-t,--type", types_, "Grid type and cutoff option")
       ->required();
  bfunc->add_flag("--derivative", false, "Plot derivatives");
  bfunc->add_flag("--scale-by-cutoff", false, "Rescale by cutoff value");
  bfunc->add_option("--indices", "", "Basis function indices (optional)", true);
  bfunc->add_option("--numeric", numeric_precision_, "Decimal precision");
  bfunc->add_flag("-V,--verbose", verbose_, "Enable verbose output");
  bfunc->callback([this]() { std::exit(this->run_bfunc()); });
  
  auto cutoff = analysis->add_subcommand("cutoff",
    "Evaluate and plot cutoff functions.\nExample: tadah analysis cutoff -o out.txt -r 0 3.14 100 -t \"CutCos\" --derivative");
  cutoff->add_option("-o,--outfile", out_file_, "Output file")
        ->required()->check(CLI::NonexistentPath);
  cutoff->add_option("-r,--range", range_, "Plotting range (START, STOP, NPOINTS)")
        ->required();
  cutoff->add_option("-t,--type", types_, "Cutoff function type(s)")
        ->required();
  cutoff->add_flag("--derivative", false, "Plot derivative");
  cutoff->add_option("--numeric", numeric_precision_, "Decimal precision");
  cutoff->add_flag("-V,--verbose", verbose_, "Enable verbose output");
  cutoff->callback([this]() { std::exit(this->run_cutoff()); });
}

int AnalysisCLI::run_descriptor() {
  CLI::Timer timer_tot("Descriptor", CLI::Timer::Big);
  int ret = AnalysisEngine::descriptor(datasets_, pot_file_, out_file_, types_);
  Logger::log(timer_tot.to_string());
  return ret;
}

int AnalysisCLI::run_bfunc() {
  Logger::log("AnalysisCLI: Evaluating basis functions (bfunc)...");
  int ret = AnalysisEngine::bfunc(config_file_, out_file_, range_, types_, false, false, numeric_precision_);
  Logger::log("AnalysisCLI: Basis function evaluation completed.");
  return ret;
}

int AnalysisCLI::run_cutoff() {
  Logger::log("AnalysisCLI: Evaluating cutoff functions...");
  int ret = AnalysisEngine::cutoff(out_file_, range_, types_, false, numeric_precision_);
  Logger::log("AnalysisCLI: Cutoff function evaluation completed.");
  return ret;
}

} // namespace cli
} // namespace tadah

