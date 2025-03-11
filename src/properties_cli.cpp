#include <tadah/cli/properties_cli.h>
#include <tadah/engines/properties_engine.h>
#include <tadah/cli/logging.h>
#include "CLI/Timer.hpp"
#include <iostream>
#include <cstdlib>

namespace tadah {
namespace cli {

void PropertiesCLI::register_module(CLI::App &app) {
  auto props = app.add_subcommand("properties", "Physical property evaluation commands.");
  
  auto twobody = props->add_subcommand("twobody",
    "Compute two-body potential (E vs. r).\nExample: tadah properties twobody -p pot.tadah -o out.txt -r 0 10 100 -a \"Kr Kr\"");
  twobody->add_option("-p,--potential", pot_file_, "Trained model file")
         ->required()->check(CLI::ExistingFile);
  twobody->add_option("-o,--outfile", out_file_, "Output file")
         ->required()->check(CLI::NonexistentPath);
  twobody->add_option("-r,--range", range_, "Plotting range (START, STOP, NPOINTS)")
         ->required();
  twobody->add_option("-a,--atoms", atoms_, "Chemical element pair")
         ->required();
  twobody->add_flag("-F,--force", predict_force_, "Predict forces");
  twobody->add_option("--numeric", numeric_precision_, "Decimal precision");
  twobody->add_flag("-E,--error", predict_error_, "Predict error");
  twobody->add_flag("-V,--verbose", verbose_, "Enable verbose output");
  twobody->callback([this]() { std::exit(this->run_twobody()); });
  
  auto ecurve = props->add_subcommand("ecurve",
    "Compute energy vs. volume/density/pressure curve.\nExample: tadah properties ecurve -s structure_file -r 10 100 200 --quantity volume -o out.txt");
  ecurve->add_option("-s,--structure", structures_, "Unified structure input")
         ->required();
  ecurve->add_option("-r,--range", range_, "Plotting range (START, STOP, NPOINTS)")
         ->required();
  ecurve->add_option("--quantity", quantity_, "Quantity to vary (volume/density/pressure)")
         ->required();
  ecurve->add_option("-o,--outfile", out_file_, "Output file")
         ->required()->check(CLI::NonexistentPath);
  ecurve->add_option("--numeric", numeric_precision_, "Decimal precision");
  ecurve->add_flag("-V,--verbose", verbose_, "Enable verbose output");
  ecurve->callback([this]() { std::exit(this->run_ecurve()); });
  
  auto eos = props->add_subcommand("eos",
    "Compute Equation of State (stub).");
  eos->add_option("-s,--structure", structures_, "Unified structure input")
     ->required();
  eos->add_option("-r,--range", range_, "Range for EOS (START, STOP, NPOINTS)")
     ->required();
  eos->add_option("-o,--outfile", out_file_, "Output file")
     ->required()->check(CLI::NonexistentPath);
  eos->add_flag("-V,--verbose", verbose_, "Enable verbose output");
  eos->callback([this]() { std::exit(this->run_eos()); });
  
  auto defect = props->add_subcommand("defect",
    "Compute defect formation energies (stub).");
  defect->callback([this]() { std::exit(this->run_defect()); });
  
  auto surface = props->add_subcommand("surface",
    "Compute surface energies (stub).");
  surface->callback([this]() { std::exit(this->run_surface()); });
  
  auto mechanics = props->add_subcommand("mechanics",
    "Compute bulk modulus and elastic constants (stub).");
  mechanics->callback([this]() { std::exit(this->run_mechanics()); });
}

int PropertiesCLI::run_twobody() {
  CLI::Timer timer_tot("Twobody", CLI::Timer::Big);
  int ret = PropertiesEngine::twobody(pot_file_, out_file_, range_, atoms_, false, false, numeric_precision_);
  Logger::log(timer_tot.to_string());
  return ret;
}

int PropertiesCLI::run_ecurve() {
  CLI::Timer timer_tot("Ecurve", CLI::Timer::Big);
  int ret = PropertiesEngine::ecurve(structures_, range_, out_file_, quantity_);
  Logger::log(timer_tot.to_string());
  return ret;
}

int PropertiesCLI::run_eos() {
  Logger::log("PropertiesCLI: EOS stub executed.");
  return PropertiesEngine::eos(structures_, range_, out_file_);
}

int PropertiesCLI::run_defect() {
  Logger::log("PropertiesCLI: Defect energies stub executed.");
  return PropertiesEngine::defect();
}

int PropertiesCLI::run_surface() {
  Logger::log("PropertiesCLI: Surface energy stub executed.");
  return PropertiesEngine::surface();
}

int PropertiesCLI::run_mechanics() {
  Logger::log("PropertiesCLI: Mechanics stub executed.");
  return PropertiesEngine::mechanics();
}

} // namespace cli
} // namespace tadah

