#include <tadah/hpo/hpo_host.h>
#include <tadah/hpo/hpo_targets.h>
#include <tadah/hpo/runners.h>
#include <tadah/hpo/hpo.h>
#include <tadah/core/context.h>
#include <tadah/core/core_user_config_parser.h>
#include <dlib/global_optimization/find_max_global.h>

using namespace tadah::core;
using namespace tadah::models;
using namespace tadah::mlip;
using namespace tadah::hpo;

/**
 * @file ex_hpo_1.cpp
 * @brief Example for hyperparameter optimization (HPO) using an external minimizer.
 *
 * This example demonstrates how to optimize the hyperparameters (HP) of a model
 * to reproduce provided physical quantities of interest. The method can be used
 * for any numerical HP specified in the Tadah! configuration file. This example
 * specifically focuses on optimizing RCUT2B, SGRID2B, and CGRID2B. The optimizer
 * requires appropriate ranges for these HPs, as defined in the `targets` file.
 *
 * @note
 * - Tadah! must be compiled with -DTADAH_ENABLE_HPO=ON for this example to function.
 * - The optimizer varies HPs and evaluates them using Tadah!, which retrains the model
 *   for each set of HPs, executes LAMMPS simulations, parses the output, and scores it.
 * - Execution time can be significant; monitor output files and terminate execution when
 *   convergence is achieved, or define the maximum number of steps in the `targets` file.
 * - Key files to monitor include:
 *   - `loss.tadah`: Provides scores for each contribution and the total score per iteration.
 *   - `outvar.tadah`: Lists obtained physical quantities at each stage.
 *
 * @recommendation
 * Start with simpler optimizations, such as the lattice parameter, and gradually include
 * more complex parameters. Use the `in.lata` LAMMPS input file for HPO, referencing the 
 * LAMMPS line in the `targets` file.
 *
 * @algorithm
 * Tadah! HPO utilizes the maxLIPO algorithm from the Dlib library. Read more about the algorithm at:
 * http://blog.dlib.net/2017/12/a-global-optimization-algorithm-worth.html.
 *
 * The code is adaptable to various optimizers from external libraries.
 *
 * @files
 * - `ex_hpo_1.cpp`: Example C++ script for hyperparameter optimization.
 * - `config.train`: Training config file containing model parameters.
 * - `config.val`: Validation dataset, same as training for this example.
 * - `targets`: Defines parameters to optimize and target physical values (e.g., lattice parameter, surface energy).
 * - `tdata.db`: Training dataset generated using the EAM model for Ta by R. Ravelo.
 *   See: https://journals.aps.org/prb/abstract/10.1103/PhysRevB.88.134101.
 * - `in.lata`: LAMMPS input file utilized by HPO.
 *
 * @compilation
 * To compile the program, use the following command:
 * ```bash
 *   g++ ex_hpo_1.cpp -I /path/to/lammps/src \
 *    -I /path/to/lammps/src/STUBS \
 *    -ltadah.hpo -L /path/to/lammps/lib/tadah.lammps/build -ltadah.lammps \
 *    -L /path/to/lammps/src -llammps_serial  \
 *    -ltadah.mlip -ltadah.models -ltadah.core -llapack -lblas -ldlib 
 * ```
 * Replace `/path/to/lammps/src/` with the actual path to your LAMMPS source directory.
 */
 int main() {

   using R = LammpsRunner;
   using T = dlib::matrix<double,0,1>;

   Context context = CoreUserConfigParser().parse("config.train");

   std::string trg_file="targets";
   std::string val_file=".val";
   HPOTargets trg(context, trg_file);
   HPO_Host<T, R> hpo(context,trg, val_file);

   const dlib::matrix<double,0,1> low = dlib::mat(trg.optim_low);
   const dlib::matrix<double,0,1> high = dlib::mat(trg.optim_high);

   auto result1 = dlib::find_min_global(hpo,low,high,
       dlib::max_function_calls(trg.MAX_CALLS),
       dlib::FOREVER,
       trg.EPS
       );
   return 0;
 }
