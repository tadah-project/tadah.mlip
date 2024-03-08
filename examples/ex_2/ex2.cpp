#include <tadah/MODELS/cutoffs/cut_all.h>
#include <tadah/MODELS/descriptors/d_all.h>
#include <tadah/MLIP/models/m_all.h>
#include <tadah/MLIP/descriptors_calc.h>
#include <tadah/CORE/config/config.h>
#include <tadah/MLIP/structure.h>
#include <tadah/MLIP/nn_finder.h>
#include <tadah/MLIP/output/output.h>
#include <tadah/MLIP/design_matrix/functions/dm_f_all.h>
#include <fstream>

/**  @file ex2.cpp
 * This example shows how to predict with a trained model.
 * Example model is provided in a `pot.tadah` file.
 *
 * To compile with `g++` and run:
 *
 * \code{.sh}
 *  $ g++ -std=c++17 -O3 ex2.cpp -o ex2.out -ltadah -llapack -fopenmp
 *  $ ./ex2.out
 * \endcode
 *
 * Ta-dah! models and descriptors are selected at compile time but all model
 * parameters are provided in the `pot.tadah` file. Model, cutoff and descriptors
 * in the `ex2.cpp` file must match those in the `pot.tadah` file.
 * See code comment bellow for more detail.
 *
 * Files:
 *
 *   - `ex2.cpp`
 *      Example c++ script for prediction using already available model.
 *   - `config_pred` 
 *      List of datasets used for prediction (\ref DBFILE key).
 *      Keys \ref FORCE and \ref STRESS controls whether forces and stresses
 *      are predicted.
 *   - `tdata.db`
 *      Dataset which we will use for prediction.
 *      The dataset is generated using EAM model for Ta by R.Ravelo.
 *      https://journals.aps.org/prb/abstract/10.1103/PhysRevB.88.134101
 */
int main() {
    
    // STEP 0: Load model saved in a `pot.tadah` as a Config object.
    Config param_file("pot.tadah");

    // STEP 1a: Select descriptors. All three types must be specified.
    // Use Dummy if given type is not required.
    // Look for keywords `TYPE2B` `TYPE3B` and `TYPEMB` in a `pot.tadah`
    // If keyword is not listed use `D2_Dummy` as a descriptor.

    // D2 - TWO-BODY
    // `pot.tadah`: TYPE2B      D2_Blip
    using D2=D2_Blip;

    // D3 - THREE-BODY
    // `pot.tadah` no keyword
    using D3=D3_Dummy;

    // DM - MANY-BODY
    // `pot.tadah` no keyword
    using DM=DM_Dummy;

    // STEP 2b: Select cutoffs for descriptors, C2 for D2, etc
    // Look for keywords `RCTYPE2B` `RCTYPE3B` and `RCTYPEMB` in the `pot.tadah`
    // If keyword is not listed use `Cut_Dummy`.
    // `pot.tadah`: RCTYPE2B      Cut_Cos
    using C2=Cut_Cos;
    // `pot.tadah` no keywords for three-body and many-body
    using C3=Cut_Dummy;
    using CM=Cut_Dummy;

    // STEP 2a: Select Basis Function (BF) or Kernels (K).
    // BF is used for M_BLR - Bayesian Linear Regression
    // K is used with M_KRR - Kernel Ridge Regression
    // KEYWORD `MODEL`: first argument is model, second BF/Kernel
    // `pot.tadah`: MODEL     M_KRR     Kern_Linear
    using K=DM_Kern_Linear;

    // STEP 2b: Select Model and instantiate object.
    // `pot.tadah`: MODEL     M_KRR     Kern_Linear
    using M=M_KRR<K>;
    M model(param_file);

    std::cout << "PREDICTION STAGE" << std::endl;
    // We will reuse param_file Config file and add to it
    // DBFILE(s) from config_pred file.
    // config_pred contain 
    // As we are reusing existing config we have
    // to remove FORCE and STRESS keys first
    param_file.remove("FORCE");
    param_file.remove("STRESS");
    param_file.add("config_pred");

    // Load DBFILE from config_pred
    std::cout << "StructureDB loading data..." << std::flush;
    StructureDB stdb(param_file);
    std::cout << "Done!" << std::endl;

    // Calculate nearest neighbours
    std::cout << "Calculating nearest neighbours..." << std::flush;
    NNFinder nnf2(param_file);
    nnf2.calc(stdb);
    std::cout << "Done!" << std::endl;

    // Calculate descriptors and store them in StDescriptorsDB
    //std::cout << "Calculating descriptors..." << std::flush;
    DescriptorsCalc<D2,D3,DM,C2,C3,CM> dc2(param_file);
    //StDescriptorsDB st_desc_db = dc2.calc(stdb);
    //std::cout << "Done!" << std::endl;

    // open file streams for energy and force prediction
    std::ofstream out_force("forces.pred");
    std::ofstream out_energy("energy.pred");
    std::ofstream out_stress("stress.pred");

    // predict energies (and forces if FORCE true). Result is saved
    // to new StructureDB object - it will only contain predicted values
    // so there are no atom positions, etc...
    //t_type pred_err;    // TODO dump it ...
    ////StructureDB stpred = model.predict(param_file, stdb,dc2,pred_err);
    //std::cout << "Predicting..." << std::flush;
    //StructureDB stpred = model.predict(param_file,st_desc_db, stdb);
    //std::cout << "Done!" << std::endl;

    bool err_bool=false;       // predict error, requires LAMBDA -1
    t_type predicted_error;    // container for prediction error
    std::cout << "Predicting..." << std::flush;
    StructureDB stpred = model.predict(param_file,stdb,dc2);
    //StructureDB stpred = model.predict(param_file,stdb,dc2,predicted_error);
    std::cout << "Done!" << std::endl;

    std::cout << "Dumping results to disk..." << std::flush;
    Output output(param_file,err_bool);
    output.print_predict_all(stdb,stpred,predicted_error);
    std::cout << "Done!" << std::endl;

    return 0;
}
