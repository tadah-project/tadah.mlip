#include <tadah/models/cut_all.h>
#include <tadah/models/descriptors/d_all.h>
#include <tadah/mlip/models/m_all.h>
#include <tadah/mlip/design_matrix/functions/basis_functions/dm_bf_all.h>
#include <tadah/models/functions/kernels/kern_all.h>
#include <tadah/core/context.h>
#include <tadah/core/core_user_config_parser.h>
#include <tadah/mlip/structure.h>
#include <tadah/mlip/descriptors_calc.h>
#include <tadah/mlip/nn_finder.h>
#include <tadah/mlip/output/output.h>
#include <fstream>

using namespace tadah::core;
using namespace tadah::models;
using namespace tadah::mlip;

/**  @file ex1.cpp
 * This example shows how to perform training and
 * how to predict with a trained model.
 *
 * To compile with `g++` and run:
 *
 * \code{.sh}
 *  $ g++ -std=c++17 -O3 ex1.cpp -o ex1.out -ltadah.mlip -ltadah.models -ltadah.core -llapack -lblas -fopenmp
 *  $ ./ex1.out
 * \endcode
 *
 * Tadah! models and descriptors are selected at compile time but all model
 * parameters are provided in the config file.
 *
 * Here we use D2_Blip two-body descriptor and train using both energies
 * and virial stresses.
 *
 * Files:
 *
 *   - `ex1.cpp`
 *      Example c++ script for training and prediction.
 *   - `config`
 *      Config file used for training, contains all model parameters.
 *   - `config_pred` 
 *      List of datasets used for prediction (\ref DBFILE key).
 *      Keys \ref FORCE and \ref STRESS controls whether forces and stresses
 *      are predicted.
 *   - `tdata.db`
 *      Dataset which we will use for both training and prediction.
 *      The dataset is generated using EAM model for Ta by R.Ravelo.
 *      https://journals.aps.org/prb/abstract/10.1103/PhysRevB.88.134101
 */
int main() {

  std::cout << "TRAINING STAGE" << std::endl;
  // Config file configures almost all model parameters.
  // See below for a more detailed explanation of used key-value(s) pairs.
  Context context = CoreUserConfigParser().parse("config");

  // First we load all training data from a list
  // of training datasets into StrutureDB object.
  // Paths to datasets are specified with a key DBFILE in a config file.
  std::cout << "StructureDB loading data..." << std::flush;
  StructureDB stdb(context);
  std::cout << "Done!" << std::endl;

  // Next we pass StructureDB object to the nearest neighbour calculator.
  // NNFinder will create full nearest neighbours lists for every atom
  // in every structure. These lists will be stored by individual Structures
  // in a StructureDB object.
  // The lists are calculated up to the max cutoff from the config file:
  // cutoff_max = max(RCUT2B, RCUTMB).
  std::cout << "Calculating nearest neighbours..." << std::flush;
  NNFinder nnf;
  nnf.calc(stdb,context);
  std::cout << "Done!" << std::endl;

  // STEP 1a: Select descriptors.
  // All three types must be specified.
  // Use Dummy if given type is not required.

  // D2 - TWO-BODY
  //using D2=D2_LJ;
  //using D2=D2_BP;
  using D2=D2_Blip;
  //using D2=D2_Dummy;
  //using D2=D2_EAM;

  // DM - MANY-BODY
  //using DM=DM_EAM;
  //using DM=DM_EAD;
  using DM=DM_Dummy;

  // STEP 1b: Select cutoffs, C2 for D2, CM for DM
  using C2=Cut_Cos;
  using CM=Cut_Dummy;

  // STEP 1c: Prepare descriptor calculator
  DescriptorsCalc<D2,DM,C2,CM> dc(context);

  // STEP 2a: Select Basis Function (BF) or Kernels (K).
  // BF is used for M_BLR - Bayesian Linear Regression
  // K is used with M_KRR - Kernel Ridge Regression
  // See documentation for more BF and K
  using BF=DM_BF_Linear;
  //using BF=BF_Polynomial2;
  //using K=Kern_Linear;
  //using K=Kern_Quadratic;

  // STEP 2b: Select Model
  using M=M_BLR<BF>;
  //using M=M_KRR<K>;

  //// STEP 2c: Instantiate a model
  M model(context);

  //std::cout << "TRAINING STAGE..." << std::flush;

  // STEP 3: Training - Option 1.
  // Train with StructureDB only. We have to provide calculators here.
  // Descriptors are calculated in batches to construct a design matrix
  // and then are discarded.
  // This is usually the best choice unless you need descriptors for something else
  // after the training is done.
  model.train(stdb,dc);

  // STEP 3: Training - Option 2.
  // Train with StructureDB and precalcualted StDescriptorsDB.
  //StDescriptorsDB st_desc_db = dc.calc(stdb);
  //model.train(st_desc_db,stdb);
  std::cout << "Done!" << std::endl;

  // STEP 4: Save model to a text file.
  // Once model is trained we can dump it to a file. 
  // Saved models can be used with LAMMPS or can be reloaded
  // to make predictions.
  std::cout << "Saving LAMMPS pot.tadah file..." << std::flush;
  Context param_file = model.get_param_file();
  std::ofstream outfile("pot.tadah");
  outfile << param_file << std::endl;
  outfile.close();
  std::cout << "Done!" << std::endl;

  std::cout << "PREDICTION STAGE..." << std::endl;
  // STEP 1: We will reuse LAMMPS param file and add to it
  // DBFILE(s) from config_pred file.
  // In other words training datasets go to the config file
  // and validation datasets are in the config_pred
  Context temp = CoreUserConfigParser().parse("config_pred");
  param_file = Context::merge(param_file,temp);

  // STEP 2: Load DBFILE from config_pred
  std::cout << "StructureDB loading data..." << std::flush;
  StructureDB stdb2(param_file);
  std::cout << "Done!" << std::endl;

  // STEP 3: Calculate nearest neighbours
  std::cout << "Calculating nearest neighbours..." << std::flush;
  NNFinder nnf2;
  nnf2.calc(stdb2,param_file);
  std::cout << "Done!" << std::endl;

  // STEP 4: Prepare DescriptorCalc
  DescriptorsCalc<D2,DM,C2,CM> dc2(param_file);

  // STEP 5: Results are saved to new StructureDB object 
  // - it will only contain predicted values
  // so there are no atom positions, etc...

  t_type predicted_error;    // container for prediction error
  param_file.add("OUTFILE","energy.pred");
  param_file.add("OUTFILE","forces.pred");
  param_file.add("OUTFILE","stress.pred");
  std::cout << "Predicting..." << std::flush;
  StructureDB stpred = model.predict(param_file,stdb2,dc2);
  //StructureDB stpred = model.predict(param_file,stdb2,dc2,predicted_error);
  std::cout << "Done!" << std::endl;

  std::cout << "Dumping results to disk..." << std::flush;
  Output output(param_file);
  output.print_predict_all(stdb,stpred,predicted_error);
  std::cout << "Done!" << std::endl;

  return 0;
}
