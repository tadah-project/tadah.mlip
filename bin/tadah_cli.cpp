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
  // Check if verbose mode is enabled and set it
  if (train->count("--Verbose")) {
    set_verbose();
  }

  // Load the configuration from the specified file
  Config config(config_file);

  // Enable FORCE and STRESS flags in the configuration if specified
  if (train->count("--Force")) {
    config.remove("FORCE");
    config.add("FORCE", "true");
  }
  if (train->count("--Stress")) {
    config.remove("STRESS");
    config.add("STRESS", "true");
  }

#ifdef TADAH_BUILD_MPI
  // Initialize MPI variables to determine the rank and the number of CPUs
  int rank = 0;
  int ncpu = 1;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &ncpu);

  // Execute host or worker code based on rank
  if (rank == 0) { // Host code
                   // Ensure there are enough CPUs for MPI
    if (ncpu < 2) {
      std::cerr << "Minimum number of CPUs for an MPI version is 2" << std::endl;
      return;
    }

    // Check if the uncertainty flag is incompatible with MPI
    if (train->count("--uncertainty")) {
      std::cout << "-----------------------------------------------------" << std::endl;
      std::cout << "The --uncertainty flag is not supported by MPI build." << std::endl;
      std::cout << "-----------------------------------------------------" << std::endl;
      return;
    }

    // Start training timer if verbose mode is enabled
    if (is_verbose()) std::cout << "Training..." << std::endl;
    CLI::Timer timer_tot{"Training", CLI::Timer::Big};

    // Initialize host MPI trainer
    MPI_Trainer_HOST HOST(config, rank, ncpu);
    HOST.prep_wpckgs();

    // Process requests from workers
    while (true) {
      // Exit loop if there are no more packages
      if (!HOST.has_packages()) break;

      // Probe for incoming requests from any worker
      HOST.probe();

      // Handle requests based on their tags
      if (HOST.tag == MPI_Trainer::WORK_TAG) {
        HOST.work_tag();
      } else if (HOST.tag == MPI_Trainer::DATA_TAG) {
        HOST.data_tag();
      } else {
        throw std::runtime_error("HOST: Unexpected request from "
            + std::to_string(HOST.worker));
      }
    }

    // Collect remaining data and release all workers
    int count = 1;
    while (true) {
      HOST.probe();
      if (HOST.tag == MPI_Trainer::DATA_TAG) {
        HOST.data_tag();
      } else {
        HOST.release_tag(count);
      }
      if (count == ncpu) break; // Exit when all workers are released
    }

    // Perform the final computation and save parameters
    HOST.solve();
    Config param_file = HOST.model->get_param_file();
    param_file.check_pot_file();
    std::ofstream outfile("pot.tadah");
    if (outfile.is_open()) {
      outfile << param_file << std::endl;
    } else {
      std::cerr << "Error: Unable to open file pot.tadah" << std::endl;
    }
    if (is_verbose()) std::cout << timer_tot.to_string() << std::endl;
  } else { // Worker code
           // Initialize worker MPI trainer
    MPI_Trainer_WORKER WORKER(config, rank, ncpu);
    while (true) {
      // Send a request for more work to the host
      MPI_Send(&WORKER.rows_available, 1, MPI_INT, 0, MPI_Trainer::WORK_TAG, MPI_COMM_WORLD);

      // Probe and respond to host or worker requests
      WORKER.probe();

      // Handle requests based on their tags
      if (WORKER.tag == MPI_Trainer::RELEASE_TAG) {
        WORKER.release_tag();
        break;
      } else if (WORKER.tag == MPI_Trainer::WAIT_TAG) {
        WORKER.wait_tag();
      } else if (WORKER.tag == MPI_Trainer::DATA_TAG) {
        WORKER.data_tag();
      } else if (WORKER.tag == MPI_Trainer::WORK_TAG) {
        WORKER.work_tag();
      }
    }
    WORKER.solve(); // Perform worker computations
  }
#else // Non-MPI version
      // Start training with a timer in the non-MPI mode
  CLI::Timer timer_tot{"Training", CLI::Timer::Big};
  Trainer tr(config);

  // Load structures and provide feedback if verbose
  if (is_verbose()) std::cout << "Loading structures..." << std::flush;
  StructureDB stdb(tr.config);
  if (is_verbose()) std::cout << "Done!" << std::endl;

  // Calculate nearest neighbors and provide feedback
  if (is_verbose()) std::cout << "Finding nearest neighbours within: "
    << tr.config.get<double>("RCUTMAX") << " cutoff distance..." << std::flush;
  tr.nnf.calc(stdb);
  if (is_verbose()) std::cout << "Done!" << std::endl;

  // Begin the training process and inform if verbose
  if (is_verbose()) std::cout << "Training start..." << std::flush;
  tr.model->train(stdb, tr.dc);
  if (is_verbose()) std::cout << "Done!" << std::endl;

  // Save the trained model parameters to a file
  Config param_file = tr.model->get_param_file();
  param_file.check_pot_file();
  std::ofstream outfile("pot.tadah");
  if (outfile.is_open()) {
    outfile << param_file << std::endl;
  } else {
    std::cerr << "Error: Unable to open file pot.tadah" << std::endl;
  }

  // Output uncertainty if requested
  if (train->count("--uncertainty")) {
    t_type weights = tr.model->get_weights();
    t_type unc = tr.model->get_weights_uncertainty();
    Output(param_file, false).print_train_unc(weights, unc);
  }

  // Output total training time if verbose
  if (is_verbose()) std::cout << timer_tot.to_string() << std::endl;
#endif
}

void TadahCLI::subcommand_predict() {
  CLI::Timer timer_tot {"Prediction", CLI::Timer::Big};
  if(predict->count("--Verbose"))
    set_verbose();
  // if (is_verbose()) std::cout << "Prediction..." << std::endl;
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

  if (is_verbose()) std::cout << "Prediction..." << std::flush;
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


#ifdef TADAH_ENABLE_HPO

  if(hpo->count("--Verbose"))
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
  int rank=0;
  int ncpu=1;
  MPI_Comm_size(MPI_COMM_WORLD, &ncpu);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  if (rank==0) {
    if (ncpu<2) {
      std::cout << "Minimum number of cpus for an mpi version is 2" << std::endl;
      return;
    }
    if (is_verbose()) std::cout << "Optimising HPs..." << std::endl;
    CLI::Timer timer_tot {"HPO", CLI::Timer::Big};
    hpo_run(config, target_file); // TODO pass prediction file
    if (is_verbose()) std::cout << timer_tot.to_string() << std::endl;
  }
  else { // WORKER
    while (true) {
      int cs_size;
      MPI_Bcast(&cs_size, 1, MPI_INT, 0, MPI_COMM_WORLD);
      if (!cs_size)
        break;

      std::vector<char> serialized(cs_size);
      MPI_Bcast(serialized.data(), cs_size, MPI_CHAR, 0, MPI_COMM_WORLD);
      Config config;
      config.deserialize(serialized);
      config.postprocess();

      MPI_Trainer_WORKER WORKER(config, rank, ncpu);
      while (true) {
        // ask for more work...
        MPI_Send (&WORKER.rows_available, 1, MPI_INT, 0, MPI_Trainer::WORK_TAG, MPI_COMM_WORLD);

        // request from root or from other workers
        WORKER.probe();

        // release a worker
        if (WORKER.tag == MPI_Trainer::RELEASE_TAG) {
          WORKER.release_tag();
          break;
        }
        else if (WORKER.tag == MPI_Trainer::WAIT_TAG) {
          // do nothing here; ask for more work in the next cycle
          WORKER.wait_tag();
        }
        else if (WORKER.tag == MPI_Trainer::DATA_TAG) {
          WORKER.data_tag();
        }
        else if (WORKER.tag == MPI_Trainer::WORK_TAG) {
          WORKER.work_tag();
        }
      }
      WORKER.solve();
    }
  }
#else
  CLI::Timer timer_tot {"HPO", CLI::Timer::Big};
  if (is_verbose()) std::cout << "Optimising HPs..." << std::endl;
  hpo_run(config, target_file);
  if (is_verbose()) std::cout << timer_tot.to_string() << std::endl;
#endif

#else // HPO is disabled
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
  train->add_flag("-V,--Verbose", "Verbose mode on.");
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
  predict->add_flag("-V,--Verbose", "Verbose mode on.");
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
#ifdef TADAH_ENABLE_HPO
  ss << "Optimize the model's architecture and determine\n";
  ss << "the best hyperparameters within predefined constraints.\n";
  ss << "This option uses hyperparameter optimization\n";
  ss << "to minimize a custom loss function.\n";
  ss << "See documentation for more details.\n";

  hpo = app.add_subcommand("hpo", ss.str());

  ss.str(std::string());
  ss << "A config file containing model inital parameters\n";
  ss << "and training dataset(s).\n";
  ss << "The model parameters will be optimised\n";
  ss << "within constraints defined in the -t target file\n";

  auto c_opt=hpo->add_option("-c,--config", config_file, ss.str())
    ->option_text("CONFIG_FILE")
    ->check(CLI::ExistingFile)
    ->required();

  ss.str(std::string());
  ss << "A config file containing model's targets\n";
  ss << "and hyperparameters constraints.\n";
  auto t_opt=hpo->add_option("-t,--target", target_file, ss.str())
    ->option_text("TARGET_FILE")
    ->required()
    ->check(CLI::ExistingFile);

  ss.str(std::string());
  ss << "A config file containing validation datasets only.\n";
  ss << "The validation set will be used to test a set of HPs.\n";
  auto v_opt=hpo->add_option("-v,--validation", validation_file, ss.str())
    ->option_text("VALIDATION_FILE")
    ->required()
    ->check(CLI::ExistingFile);

  hpo->add_flag("-F,--Force", "Train with forces.");
  hpo->add_flag("-S,--Stress", "Train with stresses.");
  hpo->add_flag("-V,--Verbose", "Verbose mode on.");
  //hpo->add_flag("-u,--uncertainty",
  //        "Dump uncertainty on weights."); // TODO check this

#else
  ss << "(UNAVAILABLE)" << std::endl;
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
