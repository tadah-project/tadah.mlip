#ifdef TADAH_BUILD_MPI
#include <mpi.h>
#endif

#ifndef TADAH_CLI_H
#define TADAH_CLI_H

#include <CLI/CLI.hpp>

#include <string>
#include <vector>

namespace fs = std::filesystem;
class TadahCLI {
  private:
    bool verbose=false;
    std::string logo=
"                                                                      \n\
                       _____           _         _      _              \n\
                      |_   _|__ _   __| |  __ _ | |__  | |             \n\
                        | | / _` | / _` | / _` || '_ \\ | |             \n\
                        | || (_| || (_| || (_| || | | ||_|             \n\
                        |_| \\__,_| \\__,_| \\__,_||_| |_|(_)             \n\
                                                                       \n\
        Software for Developing Machine Learning Interatomic Potentials\n\
                          https://tadah.readthedocs.io                 \n\
                                                                       \n\
                    Type `tadah -h SUBCOMMAND` for more info           \n\
                                                                       \n\
";
public:
  TadahCLI();
  void run(int argc, char** argv);
  CLI::App app;

  // subcommands:
  CLI::App *train;
  CLI::App *predict;
  CLI::App *hpo;
  CLI::App *convert;

  // PREDICTION
  std::string pot_file;
  std::string config_file;
  std::string out_file;
  std::string validation_file;
  std::string target_file;
  std::vector<std::string> datasets;
  size_t outprec;
  void subcommand_train();
  void subcommand_predict();
  void subcommand_hpo(int argc, char**argv);
  void subcommand_convert();
  void set_verbose();
  bool is_verbose();
  void flag_version();

  /* Return a vector of absolute paths to every file in a directory.
     * Subdirectories are ommited.
     */
  std::vector<fs::path> read_files(std::string path);

};
#endif

