#include <iostream>
#include <tadah/HPO/hyper_param_opti.h>
#include <tadah/HPO/hpo_targets.h>
#include <tadah/HPO/lammps_scores.h>
#include <tadah/config.h>
#include <dlib/global_optimization/find_max_global.h>

using T = dlib::matrix<double,0,1>;
using LS = LammpsScores;

/**  @file ex_hpo_1.cpp
 * This example shows how to optimise hyperparameters (HP)
 * of the model with an external minimiser. The model will be optimised
 * to reproduce provided physical quantities of interest.
 *
 * In principle it should be able to optimise any numerical HP found in
 * the config input file for Ta-dah!, but here we focus on RCUT2B,SGRID2B
 * and CGRID2B. The optimiser requires appropraite ranges for HP.
 * See `targets` file which provides example configuration.
 *
 * Ta-dah! must be compiled with -DTADAH_ENABLE_HPO=ON
 * for this code to work. This means OpenMP, MPI and LAMMPS compiled
 * as shared library are available.
 *
 * In a nutshell, the optimiser varies HP and Ta-dah! scores them.
 * For each set of HP, the model is retrained, next appropriate LAMMPS
 * simulations are ran and then LAMMPS output is parsed and scores it.
 *
 * Note that running this code takes some time. It is up to the user 
 * to monitor output files and terminate code when satisfactory level
 * of convergence is achieved. In particular, monitor the `scores.dat`
 * file which provides scores for all individual contributions
 * and the total score at every itaration.  The `properties.dat` file
 * lists obtained values for physical quantities at each stage.
 *
 * In general, when tackling HP optimisation it is recommended
 * to start small, e.g. begin with optimisation of the lattice parameter
 * and then slowly add and test further options.
 *
 * The code is parallelised with OpenMP, so make sure that
 * OMP_NUM_THREADS variable is set to something greater than one.
 *
 * Also, here we attempt to explain EAM generated data with a simple
 * two-body model. Don't expect miracles!
 *
 * Available targets:
 *
 *   - Lattice parameter (cubic only)
 *   - Surface energy (100,110,111)
 *   - Cohesieve energy
 *   - Vacancy formation energy at pressure
 *   - Vacancy formation enthalpy at pressure
 *   - Elastic constants
 *
 * Here we use maxLIPO algorithm provied by the excellent Dlib library
 * (http://dlib.net/). If not available, CMake will attempt to fetch it from
 * Git and link with the code.
 
 * You can read more about the algorithm here
 * http://blog.dlib.net/2017/12/a-global-optimization-algorithm-worth.html
 *
 * In principle, the code should be modifiable to work with majority
 * of optimisers from the wide range of external libraries.
 *
 *   - MAX_SCORE Return this in case of LAMMPS failure
 *   - ERMSE target for Energy RMSE in meV/atom
 *
 * To compile use provided CMake config file:
 *
 * \code{.sh}
 *  $ mkdir build
 *  $ cd build
 *  $ cmake ..
 *  $ make
 *  $ ./ex_hpo_1
 * \endcode
 *
 *
 * Files:
 *
 *   - `ex_hpo_1.cpp`
 *      Example c++ script for hyper parameters optimisation.
 *   - `config.train`
 *      Config file used for training, contains all model parameters.
 *   - `targets`
 *      Config file used by the optimiser. Defines what params to optimise
 *      and physical target values such as lattica param, surface energy, etc
 *   - `tdata.db`
 *      Dataset which we will use for training during optimisation process.
 *      The dataset is generated using EAM model for Ta by R.Ravelo.
 *      https://journals.aps.org/prb/abstract/10.1103/PhysRevB.88.134101
 */
int main() {
    Config config("config.train");

    HPOTargets trg("targets");

    HyperParamOpti<T, LS> hpo(config,trg);

    const T lowbound = dlib::mat(trg.configmin);
    const T highbound = dlib::mat(trg.configmax);

    auto result1 = dlib::find_min_global(hpo,lowbound,highbound,
            dlib::max_function_calls(trg.maxcalls),
            dlib::FOREVER,
            trg.eps
            );
    std::cout << result1.x << std::endl;
    std::cout << result1.y << std::endl;

    return 0;
}
