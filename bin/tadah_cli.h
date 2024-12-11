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
  int rank=0;
  int ncpu = 1;
  bool verbose=false;
  std::string pot_file;
  std::string config_file;
  std::string validation_file;
  std::string target_file;
  std::string in_file;
  std::string format;
  size_t index;
  std::string indices;
  std::vector<std::string> in_files;
  std::string out_file;
  std::vector<std::string> out_files;
  std::vector<std::string> datasets;
  std::vector<std::size_t> sizes;
  size_t outprec;
  size_t N;
  double val;

  void set_verbose();
  bool is_verbose();
  int sub_train();
  int sub_predict();
  int sub_hpo(int argc, char**argv);
  int sub_convert();
  int subsub_dups();
  int subsub_sample();
  int subsub_join();
  int subsub_split();
  int subsub_summary();
  int subsub_dcalc();
  int sub_swriter();
  int flag_version();

  /* Return a vector of absolute paths to every file in a directory.
     * Subdirectories are ommited.
     */
  std::vector<fs::path> read_files(std::string path);

public:
  TadahCLI();
  int run(int argc, char** argv);
  CLI::App app;
    CLI::App *train=nullptr;
    CLI::App *predict=nullptr;
    CLI::App *hpo=nullptr;
    CLI::App *convert=nullptr;
    CLI::App *db=nullptr;
      CLI::App *dups=nullptr;
      CLI::App *sample=nullptr;
        CLI::Option_group *sample_group=nullptr;
      CLI::App *join=nullptr;
      CLI::App *split=nullptr;
        CLI::Option_group *split_group=nullptr;
      CLI::App *summary=nullptr;
    CLI::App *desc=nullptr;
      CLI::App *dcalc=nullptr;
    CLI::App *swriter=nullptr;

};
#endif

