#include <tadah/cli/transform_cli.h>
#include <tadah/engines/transform_engine.h>
#include <tadah/cli/logging.h>
#include "CLI/Timer.hpp"
#include <iostream>
#include <cstdlib>

namespace tadah {
namespace cli {

void TransformCLI::register_module(CLI::App &app) {
  auto transform = app.add_subcommand("transform", "Structure transformation commands.");
  
  auto rescale = transform->add_subcommand("rescale",
    "Apply energy shift and scaling to dataset structures.\nExample: tadah transform rescale -d dataset.tadah --Scale --eshift 0.5 -n 6");
  rescale->add_option("-d,--dataset", datasets_, "Dataset file(s)")
         ->required()->check(CLI::ExistingFile);
  rescale->add_option("-e,--eshift", eshift_val_, "Energy shift per atom");
  rescale->add_flag("-S,--Scale", scale_, "Enable scaling");
  rescale->add_option("-n,--numeric", numeric_precision_, "Decimal precision");
  rescale->add_flag("-V,--verbose", verbose_, "Enable verbose output");
  rescale->callback([this]() { std::exit(this->run_rescale()); });
  
  auto rotate = transform->add_subcommand("rotate",
    "Rotate structures by a specified angle (stub).");
  rotate->callback([this]() { std::exit(this->run_rotate()); });
  
  auto shear = transform->add_subcommand("shear",
    "Apply shear transformation (stub).");
  shear->callback([this]() { std::exit(this->run_shear()); });
  
  auto change_coords = transform->add_subcommand("change-coords",
    "Convert coordinate systems (stub).");
  change_coords->callback([this]() { std::exit(this->run_change_coords()); });
}

int TransformCLI::run_rescale() {
  CLI::Timer timer_tot("Rescaling", CLI::Timer::Big);
  int ret = tadah::engines::TransformEngine::rescale(datasets_, eshift_val_, scale_, numeric_precision_);
  Logger::log(timer_tot.to_string());
  return ret;
}

int TransformCLI::run_rotate() {
  Logger::log("TransformCLI: Rotate operation stub executed.");
  return EXIT_SUCCESS;
}

int TransformCLI::run_shear() {
  Logger::log("TransformCLI: Shear operation stub executed.");
  return EXIT_SUCCESS;
}

int TransformCLI::run_change_coords() {
  Logger::log("TransformCLI: Change coordinate systems stub executed.");
  return EXIT_SUCCESS;
}

} // namespace cli
} // namespace tadah

