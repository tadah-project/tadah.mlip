#include <tadah/core/blueprint_parser.h>
#include <tadah.mlip/cli/data_cli.h>
#include <tadah/mlip/engines/data_engine.h>
#include "CLI/Timer.hpp"
#include <iostream>
#include <cstdlib>

namespace tadah {
namespace mlip {
namespace cli {

void DataCLI::register_module(CLI::App &app) {
  auto data = app.add_subcommand("data", "Dataset management commands.");

  /*auto convert = data->add_subcommand("convert",*/
  /*  "Convert DFT output files to Tadah! dataset format.\nExample: tadah data convert --dft-files run1.outcar -o output.tadah");*/
  /*convert->add_option("--dft-files", datasets_, "Input DFT file(s)")*/
  /*       ->required()->check(CLI::ExistingFile);*/
  /*convert->add_option("-o,--outfile", out_file_, "Output file")*/
  /*       ->required()->check(CLI::NonexistentPath);*/
  /*convert->add_option("--numeric", 0, "Decimal precision"); // placeholder*/
  /*convert->add_flag("-V,--verbose", verbose_, "Enable verbose logging");*/
  /*convert->callback([this]() { std::exit(this->run_convert()); });*/
  /**/
  /*auto read = data->add_subcommand("read",*/
  /*  "Read and print structures from dataset or structure files.\nExample: tadah data read -d dataset.tadah");*/
  /*read->add_option("-d,--dataset", datasets_, "Tadah! dataset file(s)")*/
  /*     ->check(CLI::ExistingFile);*/
  /*read->add_option("-s,--structure", sources_, "Unified structure source(s)")*/
  /*     ->check(CLI::ExistingFile);*/
  /*read->add_option("-i,--indices", out_file_, "Structure indices (stub)"); // placeholder*/
  /*read->add_option("--numeric", 0, "Decimal precision");*/
  /*read->add_flag("-V,--verbose", verbose_, "Enable verbose logging");*/
  /*read->callback([this]() { std::exit(this->run_read()); });*/
  /**/
  /*auto write = data->add_subcommand("write",*/
  /*  "Write structures from a dataset into a chosen format.\nExample: tadah data write -d dataset.tadah -o output.cell -f cell -i 7");*/
  /*write->add_option("-d,--dataset", datasets_, "Tadah! dataset file(s)")*/
  /*     ->required()->check(CLI::ExistingFile);*/
  /*write->add_option("-o,--outfile", out_file_, "Output file")*/
  /*     ->required()->check(CLI::NonexistentPath);*/
  /*write->add_option("-i,--indices", out_file_, "Structure index (stub)");*/
  /*write->add_option("-f,--format", "", "Output format (cif, cell, poscar)")  // if needed, otherwise stub.*/
  /*     ->required();*/
  /*write->add_option("--numeric", 0, "Decimal precision");*/
  /*write->add_flag("-V,--verbose", verbose_, "Enable verbose logging");*/
  /*write->callback([this]() { std::exit(this->run_write()); });*/
  /**/
  /*auto merge = data->add_subcommand("merge",*/
  /*  "Merge multiple dataset files.\nExample: tadah data merge -d ds1.tadah ds2.tadah -o merged.tadah");*/
  /*merge->add_option("-d,--dataset", datasets_, "Dataset file(s)")*/
  /*     ->required()->check(CLI::ExistingFile);*/
  /*merge->add_option("-o,--outfile", outfiles_, "Output file")*/
  /*     ->required()->check(CLI::NonexistentPath);*/
  /*merge->add_flag("-V,--verbose", verbose_, "Enable verbose logging");*/
  /*merge->callback([this]() { std::exit(this->run_merge()); });*/
  /**/
  /*auto split = data->add_subcommand("split",*/
  /*  "Split a dataset file into parts.\nExample: tadah data split -d big.tadah -o part1.tadah part2.tadah");*/
  /*split->add_option("-d,--dataset", datasets_, "Dataset file")*/
  /*     ->required()->check(CLI::ExistingFile);*/
  /*split->add_option("-o,--outfiles", out_files_, "Output file names")*/
  /*     ->required()->check(CLI::NonexistentPath);*/
  /*split->add_flag("--shuffle", false, "Randomize entries before splitting");*/
  /*split->add_flag("-V,--verbose", verbose_, "Enable verbose logging");*/
  /*split->callback([this]() { std::exit(this->run_split()); });*/
  /**/
  /*auto dedup = data->add_subcommand("dedup",*/
  /*  "Deduplicate structures in dataset file(s).\nExample: tadah data dedup -d db.tadah");*/
  /*dedup->add_option("-d,--dataset", datasets_, "Dataset file(s)")*/
  /*     ->required()->check(CLI::ExistingFile);*/
  /*dedup->add_option("-o,--outfiles", out_files_, "Output file(s)")*/
  /*     ->check(CLI::NonexistentPath);*/
  /*dedup->add_option("--threshold", 1e-6, "Duplicate threshold (default: 1e-6)");*/
  /*dedup->add_flag("--merge", false, "Merge deduplication results into one file");*/
  /*dedup->add_flag("-V,--verbose", verbose_, "Enable verbose logging");*/
  /*dedup->callback([this]() { std::exit(this->run_dedup()); });*/
  /**/
  /*auto sample = data->add_subcommand("sample",*/
  /*  "Sample configurations from a dataset.\nExample: tadah data sample -d full.tadah -o sample.tadah");*/
  /*sample->add_option("-d,--dataset", datasets_, "Dataset file(s)")*/
  /*     ->required()->check(CLI::ExistingFile);*/
  /*sample->add_option("-o,--outfile", outfiles_, "Output file")*/
  /*     ->required()->check(CLI::NonexistentPath);*/
  /*sample->add_flag("-V,--verbose", verbose_, "Enable verbose logging");*/
  /*// Options for --every, --random, --indices are stubs.*/
  /*sample->callback([this]() { std::exit(this->run_sample()); });*/
  /**/
  /*auto balance = data->add_subcommand("balance",*/
  /*  "Apply energy shift and/or rescaling to dataset structures.\nExample: tadah data balance -d sample.tadah --rescale --eshift 0.5 --numeric 6");*/
  /*balance->add_option("-d,--dataset", datasets_, "Dataset file(s)")*/
  /*       ->required()->check(CLI::ExistingFile);*/
  /*balance->add_flag("--rescale", false, "Enable rescaling of training weights");*/
  /*balance->add_option("--eshift", 0.0, "Energy shift per atom");*/
  /*balance->add_option("--numeric", 0, "Decimal precision");*/
  /*balance->add_flag("-V,--verbose", verbose_, "Enable verbose logging");*/
  /*balance->callback([this]() { std::exit(this->run_balance()); });*/
}

int DataCLI::run_convert() {
  return EXIT_SUCCESS;
}

int DataCLI::run_read() {
  return EXIT_SUCCESS;
}

int DataCLI::run_write() {
  return EXIT_SUCCESS;
}

int DataCLI::run_merge() {
  return EXIT_SUCCESS;
}

int DataCLI::run_split() {
  return EXIT_SUCCESS;
}

int DataCLI::run_dedup() {
  return EXIT_SUCCESS;
}

int DataCLI::run_sample() {
  return EXIT_SUCCESS;
}

int DataCLI::run_balance() {
  return EXIT_SUCCESS;
}

} // namespace cli
} // namespace mlip
} // namespace tadah

