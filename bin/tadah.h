#ifdef TADAH_ENABLE_MPI
#include <mpi.h>
#endif

#ifndef TADAH_CLI_H
#define TADAH_CLI_H

#include "../LIBS/CLI11/CLI11.hpp"
#include "../LIBS/CLI11/Timer.hpp"
#include "../CORE/config/config.h"
#include "../MODELS/dc_selector.h"
#include "../MLIP/descriptors_calc.h"
#include "../MLIP/design_matrix/functions/dm_function_base.h"
#include "../MLIP/models/m_tadah_base.h"
#include "../MLIP/structure_db.h"
#include "../MLIP/nn_finder.h"
#include "../CORE/utils/utils.h"
#include "../MLIP/analytics/analytics.h"
#include "../HPO/hpo.h"
#include "../MLIP/version.h"

#include <ios>
#include <iostream>
#include <iomanip>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
class TadahCLI {
    private:
        CLI::App &app;
        bool verbose=false;
    public:
        // PREDICTION
        std::string pot_file;
        std::string config_file;
        std::string config_tpred;
        std::string target_file;
        std::string targets_dir;
        std::string targets_out_dir;
        std::vector<std::string> datasets;
        TadahCLI(CLI::App &_app):
            app(_app)
    { }
        void subcommand_train();
        void subcommand_predict();
        void subcommand_hpo(int argc, char**argv);
        void set_verbose();
        bool is_verbose();
        void flag_version();
        std::vector<fs::path> read_targets(std::string path);

        /* Return a vector of absolute paths to every file in a directory.
         * Subdirectories are ommited.
         */
        std::vector<fs::path> read_files(std::string path);

};

#endif

