#include "tadah_cli.h"
#include "../MLIP/output/output.h"
#include "../CORE/core_types.h"
#include "../LIBS/CLI11/Timer.hpp"
#include "../CORE/config/config.h"
#include "../MODELS/dc_selector.h"
#include "../MLIP/descriptors_calc.h"
#include "../MLIP/design_matrix/functions/dm_function_base.h"
#include "../MLIP/models/m_tadah_base.h"
#include "../MLIP/structure_db.h"
#include "../MLIP/nn_finder.h"
#include "../MLIP/analytics/analytics.h"
#include "../MLIP/version.h"
#include "../MLIP/design_matrix/design_matrix.h"
#include "../MLIP/trainer.h"
#ifdef TADAH_ENABLE_HPO
#include "../HPO/hpo.h"
#endif

#include <iostream>
#include <stdexcept>
#include <chrono>

void TadahCLI::subcommand_train() {

  int rank = 0;

#ifdef TADAH_BUILD_MPI
  int ncpu = 1;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &ncpu);
  if (ncpu<2) {
    std::cout << "Minimum number of cpus for an mpi version is 2" << std::endl;
    return;
  }
  if (train->count("--uncertainty")) {
    if (rank==0) {
      std::cout << "-----------------------------------------------------" << std::endl;
      std::cout << "The --uncertainty flag is not supported by MPI build." << std::endl;
      std::cout << "-----------------------------------------------" << std::endl;
    }
    return;
  }
#endif

  CLI::Timer timer_tot {"Training", CLI::Timer::Big};

  if(train->count("--verbose"))
    set_verbose();

  Config config(config_file);

  if(train->count("--Force")) {
    config.remove("FORCE");
    config.add("FORCE", "true");
  }
  if(train->count("--Stress")) {
    config.remove("STRESS");
    config.add("STRESS", "true");
  }

  if (rank==0)
    if (is_verbose()) std::cout << "Training mode" << std::endl;

#ifdef TADAH_BUILD_MPI
  /* MPI CODE:
   * The PHI matrix is divided into local phi matrices.
   *
   * The code follows host-worker pattern. Where host (rank 0)
   * distributes work between remaining workers (rank>0).
   * The work is distributed in packages. 
   * Each package contains: filename, first structure to be read
   * and the number of structures to process.
   *
   * Host does not calculate phi but still allocates memory to it.
   * Each worker fills its local phi. 
   * Once the local phi matrix is full a worker will ask host
   * for a rank to send excees data.
   * Host will first try to fill its own local phi.
   * Once host phi matrix is full, the host will search for a worker
   * with with free space in its local phi.
   *
   * 1. each rank reads config file
   * 2. host calculates total number of structures
   * 3. host calculates the dimensions of the PHI matrix
   *    based on force and stress flags and type of the regression
   * 4. host broadcasts PHI number of cols and rows to all workers
   * 5. compute sizes for local phi matrices
   * 6. Each worker allocates memory for a local matrix phi
   */

  MPI_Trainer tr(config);
  tr.init(rank, ncpu);

  // BEGIN HOST-WORKER
  if (rank==0) {
    // HOST is waiting for workers requests
    MPI_Trainer_HOST HOST(tr, rank, ncpu);
    HOST.prep_wpckgs();
    while (true) {
      if (!HOST.has_packages()) {
        // no more packages, just skip remaining workers
        // we will collect remaining data and release them outside of this loop
        break;
      }

      // probe ANY request from ANY worker
      tr.probe();

      if (tr.tag==MPI_Trainer::WORK_TAG) {
        HOST.work_tag();
      }
      else if (tr.tag==MPI_Trainer::DATA_TAG) {
        HOST.data_tag();
      }
      else {
        throw std::runtime_error("HOST1: Unexpected request from "
            + std::to_string(tr.worker));
      }
    }

    // work finised, collect remaining data and release all workers
    int count=1;  // count number of release workers, skip host
    while(true) {
      tr.probe();
      if (tr.tag==MPI_Trainer::DATA_TAG) {
        HOST.data_tag();
      }
      else {
        HOST.release_tag(count);
      }
      if (count==ncpu) {  break; }  // count starts from 1
    }
  } 
  else { // WORKER
    MPI_Trainer_WORKER WORKER(tr, rank, ncpu);
    while (true) {
      // ask for more work...
      MPI_Send (&tr.rows_available, 1, MPI_INT, 0, MPI_Trainer::WORK_TAG, MPI_COMM_WORLD);

      // request from root or from other workers
      tr.probe();

      // release a worker
      if (tr.tag == MPI_Trainer::RELEASE_TAG) {
        WORKER.release_tag();
        break;
      }
      else if (tr.tag == MPI_Trainer::WAIT_TAG) {
        // do nothing here; ask for more work in the next cycle
        WORKER.wait_tag();
      }
      else if (tr.tag == MPI_Trainer::DATA_TAG) {
        WORKER.data_tag();
      }
      else if (tr.tag == MPI_Trainer::WORK_TAG) {
        WORKER.work_tag();
      }
    }
  }
  // END HOST-WORKER
  // All local phi matrices are computed by this point
  //
  tr.solve();


#else // NON MPI VERSION
  Trainer tr(config);
  if (is_verbose()) std::cout << "Loading structures..." << std::flush;
  StructureDB stdb(tr.config);
  if (is_verbose()) std::cout << "Done!" << std::endl;

  if (is_verbose()) std::cout << "Finding nearest neighbours within: " <<
    tr.config.get<double>("RCUTMAX") << " cutoff distance..." << std::flush;
  tr.nnf.calc(stdb);
  if (is_verbose()) std::cout << "Done!" << std::endl;

  if (is_verbose()) std::cout << "Training start..." << std::flush;

  // here we want to train with the locally allocated phi
  tr.model->train(stdb,tr.dc);

  if (is_verbose()) std::cout << "Done!" << std::endl;

#endif

  if (rank==0) {
    Config param_file = tr.model->get_param_file();
    param_file.check_pot_file();
    std::ofstream outfile;
    outfile.open ("pot.tadah");
    outfile << param_file << std::endl;;

#ifndef TADAH_BUILD_MPI
    if(train->count("--uncertainty")) {
      t_type weights = tr.model->get_weights();
      t_type unc = tr.model->get_weights_uncertainty();
      Output(param_file,false).print_train_unc(weights, unc);
    }
#endif

    if (is_verbose()) std::cout << timer_tot.to_string() << std::endl;
  }

}

void TadahCLI::subcommand_predict() {
  CLI::Timer timer_tot {"Prediction", CLI::Timer::Big};
  if(predict->count("--verbose"))
    set_verbose();
  if (is_verbose()) std::cout << "Prediction mode" << std::endl;
  Config pot_config(pot_file);
  pot_config.check_for_predict();
  pot_config.remove("VERBOSE");
  if(predict->count("--config")) {
    pot_config.add(config_file);
    if (!pot_config.exist("VERBOSE"))
      pot_config.add("VERBOSE",0);
  }
  else if(predict->count("--datasets")) {
    for (auto &ds: datasets) {
      pot_config.add("DBFILE",ds);
    }
    pot_config.add("VERBOSE",0);
  }
  else {
    std::runtime_error("Either provide config file\n\
        or datasets in a command line");
  }

  // If force or Stress flag is set add it to config
  // overwrite existing flags if present.
  // If flags are not set either keep existing
  // setting from the config file or set to false
  if(predict->count("--Force")) {
    pot_config.remove("FORCE");
    pot_config.add("FORCE", "true");
  }
  if(predict->count("--Stress")) {
    pot_config.remove("STRESS");
    pot_config.add("STRESS", "true");
  }

  DC_Selector DCS(pot_config);

  if (is_verbose()) std::cout << "Loading structures..." << std::flush;
  StructureDB stdb(pot_config);
  if (is_verbose()) std::cout << "Done!" << std::endl;

  if (is_verbose()) std::cout << "Finding nearest neighbours within: " <<
    pot_config.get<double>("RCUTMAX") << " cutoff distance..." << std::flush;
  NNFinder nnf(pot_config);
  nnf.calc(stdb);
  if (is_verbose()) std::cout << "Done!" << std::endl;

  DescriptorsCalc<> dc(pot_config,*DCS.d2b,*DCS.d3b,*DCS.dmb,
      *DCS.c2b,*DCS.c3b,*DCS.cmb);

  if (is_verbose()) std::cout << "Prediction start..." << std::flush;
  DM_Function_Base *fb = CONFIG::factory<DM_Function_Base,Config&>(
      pot_config.get<std::string>("MODEL",1),pot_config);
  M_Tadah_Base *modelp = CONFIG::factory<M_Tadah_Base,DM_Function_Base&,Config&>(
      pot_config.get<std::string>("MODEL",0),*fb,pot_config);

  StructureDB stpred;
  aed_type2 predicted_error;
  if (predict->count("--error")) {
    stpred = modelp->predict(pot_config,stdb,dc,predicted_error);
  }
  else {

    stpred = modelp->predict(pot_config,stdb,dc);
  }


  if (is_verbose()) std::cout << "Done!" << std::endl;

  if (is_verbose()) std::cout << "Dumping output..." << std::flush;

  Output output(pot_config,predict->count("--error"));
  output.print_predict_all(stdb,stpred,predicted_error);

  if (is_verbose()) std::cout << "Done!" << std::endl;
  if (is_verbose()) std::cout << timer_tot.to_string() << std::endl;

  if(predict->count("--analytics")) {
    Analytics a(stdb,stpred);

    std::cout << "Energy MAE (meV/atom): " << 1000*a.calc_e_mae() << std::endl;
    std::cout << "Energy RMSE (meV/atom): " << 1000*a.calc_e_rmse() << std::endl;
    std::cout << "Energy R^2: " << a.calc_e_r_sq() << std::endl;

    if (predict->count("--Force")) {
      std::cout << "Force MAE (eV/A): "<< a.calc_f_mae() << std::endl;
      std::cout << "Force RMSE (eV/A): "<< a.calc_f_rmse() << std::endl;
      std::cout << "Force R^2: " << a.calc_f_r_sq() << std::endl;
    }

    if (predict->count("--Stress")) {
      std::cout << "Stress MAE (eV): "<< a.calc_s_mae() << std::endl;
      std::cout << "Stress RMSE (eV): "<< a.calc_s_rmse() << std::endl;
      std::cout << "Stress R^2: " << a.calc_s_r_sq() << std::endl;
    }
  }

  if(modelp)
    delete modelp;
  if(fb)
    delete fb;
}

void TadahCLI::subcommand_hpo(
    [[maybe_unused]]int argc,
    [[maybe_unused]]char**argv) {

  int rank=0;

  // HOST
#ifdef TADAH_ENABLE_HPO

#ifdef TADAH_BUILD_MPI
  int ncpu=1;
  MPI_Comm_size(MPI_COMM_WORLD, &ncpu);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  if (ncpu<2) {
    std::cout << "Minimum number of cpus for an mpi version is 2" << std::endl;
    return;
  }
#endif

    if (rank==0)
      if (is_verbose()) std::cout << "HPO mode" << std::endl;

    CLI::Timer timer_tot {"HPO", CLI::Timer::Big};

    if(hpo->count("--verbose"))
      set_verbose();
    Config config(config_file);
    config.remove("CHECKPRESS");
    config.add("CHECKPRESS", "true");
    config.check_for_training();

    if(hpo->count("--Force")) {
      config.remove("FORCE");
      config.add("FORCE", "true");
    }
    if(hpo->count("--Stress")) {
      config.remove("STRESS");
      config.add("STRESS", "true");
    }

#ifdef TADAH_BUILD_MPI
  if (rank==0) {
      std::cout << "ROOT START: " << std::endl;
    hpo_run(config, target_file);
      std::cout << "ROOT END: " << std::endl;
  }
  else { // WORKER
    while (true) {
      std::cout << "WORKER START: " << std::endl;
      int ready=0;
      std::cout << "WORKER READ CONFIG 1 : " << std::endl;
      MPI_Bcast(&ready, 1, MPI_INT, 0, MPI_COMM_WORLD);
      std::cout << "WORKER READ CONFIG 2 : " << std::endl;

      if (!ready)
        break;

      Config config("config.temp");
      std::cout << "WORKER READ CONFIG DONE: " << std::endl;

      MPI_Trainer tr(config);
      tr.init(rank, ncpu);
      MPI_Trainer_WORKER WORKER(tr, rank, ncpu);
      std::cout << "WORKER INIT DONE: " << std::endl;
      while (true) {
        // ask for more work...
        MPI_Send (&tr.rows_available, 1, MPI_INT, 0, MPI_Trainer::WORK_TAG, MPI_COMM_WORLD);

        // request from root or from other workers
        tr.probe();

        // release a worker
        if (tr.tag == MPI_Trainer::RELEASE_TAG) {
          WORKER.release_tag();
          break;
        }
        else if (tr.tag == MPI_Trainer::WAIT_TAG) {
          // do nothing here; ask for more work in the next cycle
          WORKER.wait_tag();
        }
        else if (tr.tag == MPI_Trainer::DATA_TAG) {
          WORKER.data_tag();
        }
        else if (tr.tag == MPI_Trainer::WORK_TAG) {
          WORKER.work_tag();
        }
      }

      std::cout << "WORKER SOLVE: " << std::endl;
      tr.solve();
      std::cout << "WORKER SOLVE DONE: " << std::endl;
      std::cout << "WORKER STOP: " << std::endl;
    }
  }
#else
    hpo_run(config, target_file);
#endif

    if (rank==0)
      if (is_verbose()) std::cout << timer_tot.to_string() << std::endl;
#else
    std::cout << "-----------------------------------------------" << std::endl;
    std::cout << "This subcommand is not supported by this build." << std::endl;
    std::cout << "Tadah! Must by compiled with HPO support." << std::endl;
    std::cout << "See documentation for details." << std::endl;
    std::cout << "-----------------------------------------------" << std::endl;
#endif

}

bool TadahCLI::is_verbose() {
  return verbose;
}
void TadahCLI::set_verbose() {
  verbose=true;
}
void TadahCLI::flag_version() {
  int v_world = TADAH_WORLD_VERSION;
  int v_major = TADAH_MAJOR_VERSION;
  int v_minor = TADAH_MINOR_VERSION;
  std::string v= "tadah version " + std::to_string(v_world)
    + "." + std::to_string(v_major) + "." + std::to_string(v_minor);
  std::cout << v << std::endl;
}
std::vector<fs::path> TadahCLI::read_targets(std::string path) {
  std::vector<fs::path> s=read_files(path);
  if(s.size()==0) 
    throw std::runtime_error(
        "No files found in the targets directory: "
        +targets_dir+"\n");

  return s;
}
std::vector<fs::path> TadahCLI::read_files(std::string path) {
  std::vector<fs::path> s;
  for (const auto & entry : fs::directory_iterator(path))
    if(fs::is_regular_file(entry))
      s.push_back(fs::absolute(entry.path()));
  return s;
}

TadahCLI::TadahCLI():
  app(CLI::App{logo})
{

  std::stringstream ss;

  app.add_flag("-v, --version",
      "Print version number and exit");

  /*---------------------------------------------------------------------------*/
  /*     Training                                                              */
  /*---------------------------------------------------------------------------*/
  ss.str(std::string());
  ss << "Train model using a config file.\n";

  train = app.add_subcommand("train", ss.str());

  ss.str(std::string());
  ss << "A config file containing all model parameters\n";
  ss << "and training dataset(s).\n";
  ss << "See documentation for more details.\n";
  train->add_option("-c,--config", config_file, ss.str())
    ->option_text("CONFIG_FILE")
    ->check(CLI::ExistingFile)
    ->required();

  train->add_flag("-F,--Force", "Train with forces.");
  train->add_flag("-S,--Stress", "Train with stresses.");
  train->add_flag("-v,--verbose", "Verbose mode on.");
  train->add_flag("-u,--uncertainty", "Dump uncertainty on weights.");


  /*---------------------------------------------------------------------------*/
  /*     Prediction                                                            */
  /*---------------------------------------------------------------------------*/
  ss.str(std::string());
  ss << "Predict using already trained model.\n";
  ss << "Energy per atom is always calculated.\n";
  ss << "Calculation of forces and stresses is optional.\n\n";
  ss << "Option 1:\n Use the config file with\n";
  ss << "with DBFILE, FORCE and STRESS keys.\n";
  ss << "Note: --Force and --Stress flags take\n";
  ss << "priority over config keys.\n";
  ss << "-e.g. tadah predict -c config.tadah -p pot.tadah\n\n";
  ss << "Option 2:\n Provide datasets and flags in the command line.\n";
  ss << "-e.g. tadah predict -p pot-tadah -FS -d db1.tadah\n";

  predict = app.add_subcommand("predict", ss.str());

  ss.str(std::string());
  ss << "A config file containing prediction dataset(s).\n";
  ss << "Required Config KEYS: DBFILE.\n";
  ss << "Optional Config KEYS: FORCE, STRESS.\n";
  ss << "See documentation for more details.\n";
  predict->add_option("-c,--config", config_file,ss.str())
    ->check(CLI::ExistingFile)
    ->option_text("CONFIG_FILE");


  predict->add_option("-d,--datasets", datasets,
      "One or more datasets for prediction.")
    ->option_text("DATASETS ...")
    ->excludes("-c")
    ->check(CLI::ExistingFile);

  predict->add_flag("-F,--Force", "Predict forces.");
  predict->add_flag("-S,--Stress", "Predict stresses.");
  predict->add_flag("-v,--verbose", "Verbose mode on.");
  predict->add_flag("-e,--error", "Predict model error.");
  predict->add_flag("-a,--analytics",
      "Compare predicted values to those in the original dataset.");

  std::string pot_opt="-p,--potential";
  predict->add_option(pot_opt, pot_file,
      "Trained model")
    ->option_text("POT_FILE")
    ->check(CLI::ExistingFile)
    ->required();

  /*---------------------------------------------------------------------------*/
  /*     Hyperparameter Optimizer                                              */
  /*---------------------------------------------------------------------------*/
  ss.str(std::string());
#ifndef TADAH_ENABLE_HPO
  ss << "(UNAVAILABLE)" << std::endl;
#endif
  ss << "Optimise model's architecture.\n";
  ss << "Find the best set of hyperparameters (HPs)\n";
  ss << "subject to predefined constraints\n";
  ss << "Minimises custom made loss function.\n";

  hpo = app.add_subcommand("hpo", ss.str());

  ss.str(std::string());
  ss << "A config file containing all model parameters\n";
  ss << "and training dataset(s).\n";
  ss << "The model parameters will be optimised\n";
  ss << "within constraints defined in the -t target file\n";
  ss << "See documentation for more details.\n";

#ifdef TADAH_ENABLE_HPO
  auto c_opt=hpo->add_option("-c,--config", config_file, ss.str())
    ->option_text("CONFIG_FILE")
    ->check(CLI::ExistingFile)
    ->required();

  ss.str(std::string());
  ss << "A config file containing model's targets\n";
  ss << "and hyperparameters constraints.\n";
  ss << "See documentation for more details.\n";
  auto t_opt=hpo->add_option("-t,--target", target_file, ss.str())
    ->option_text("TARGET_FILE")
    ->check(CLI::ExistingFile);

  // ss.str(std::string());
  // ss << "Output directory for -d option.\n";
  // auto o_opt=hpo->add_option("-o,--out_dir", targets_out_dir, ss.str())
  //   ->option_text("OUTPUT_DIRECTORY")
  //   ->check(CLI::ExistingDirectory)
  //   ->excludes(t_opt)
  //   ->needs(c_opt);

  //ss.str(std::string());
  //ss << "A direcory containing model's target files.\n";
  //ss << "Each target file must contain a set of\n";
  //ss << "hyperparameter constraints as in the -t option.\n";
  //ss << "This option can be run with MPI.\n";
  //ss << "Each MPI process will run independent\n";
  //ss << "optimisation for every target in the directory\n";
  //ss << "resulting in N models for N targets.\n";
  //ss << "See documentation for more details.\n";
  //hpo->add_option("-d,--dtargets", targets_dir, ss.str())
  //  ->option_text("TARGETS_DIRECTORY")
  //  ->check(CLI::ExistingDirectory)
  //  ->excludes(t_opt)
  //  ->needs(c_opt)
  //  ->needs(o_opt);


  hpo->add_flag("-F,--Force", "Train with forces.");
  hpo->add_flag("-S,--Stress", "Train with stresses.");
  hpo->add_flag("-v,--verbose", "Verbose mode on.");
  //hpo->add_flag("-u,--uncertainty",
  //        "Dump uncertainty on weights."); // TODO check this
#endif
}

void TadahCLI::run(int argc, char** argv) {

  if(app.count_all()==1) {
    std::cout << "Type: `tadah -h` for help" << std::endl;
  }

  if (app.count("--version")) {
    flag_version();
  }
  // HPO
  if (hpo->count_all()) {
    subcommand_hpo(argc,argv);
  }
  // TRAINING
  if (train->count_all()) {
    subcommand_train();
  }

  // PREDICTION
  else if (predict->count_all()) {
    subcommand_predict();
  }
}
