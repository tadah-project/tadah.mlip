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
    CLI::App *train;
    CLI::App *predict;
    CLI::App *hpo;
    CLI::App *convert;
    CLI::App *db;
      CLI::App *dups;
      CLI::App *sample;
        CLI::Option_group *sample_group;
      CLI::App *join;
      CLI::App *split;
        CLI::Option_group *split_group;
      CLI::App *summary;
    CLI::App *desc;
      CLI::App *calc;

  std::string pot_file;
  std::string config_file;
  std::string validation_file;
  std::string target_file;
  std::string in_file;
  std::vector<std::string> in_files;
  std::string out_file;
  std::vector<std::string> out_files;
  std::vector<std::string> datasets;
  std::vector<std::size_t> sizes;
  size_t outprec;
  size_t N;
  double val;

  void sub_train();
  void sub_predict();
  void sub_hpo(int argc, char**argv);
  void sub_convert();
  void subsub_dups();
  void subsub_sample();
  void subsub_join();
  void subsub_split();
  void subsub_summary();
  void set_verbose();
  bool is_verbose();
  void flag_version();

  /* Return a vector of absolute paths to every file in a directory.
     * Subdirectories are ommited.
     */
  std::vector<fs::path> read_files(std::string path);

};
#endif

