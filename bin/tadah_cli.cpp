#include "tadah_cli.h"
#include <tadah/core/core_types.h>
#include <tadah/core/config.h>
#include <tadah/core/element.h>
#include <tadah/core/periodic_table.h>
#include <tadah/models/dc_selector.h>
#include <tadah/mlip/output/output.h>
#include <tadah/mlip/descriptors_calc.h>
#include <tadah/mlip/design_matrix/functions/dm_function_base.h>
#include <tadah/mlip/models/m_tadah_base.h>
#include <tadah/mlip/structure_db.h>
#include <tadah/mlip/nn_finder.h>
#include <tadah/mlip/analytics/analytics.h>
#include <tadah/mlip/version.h>
#include <tadah/mlip/design_matrix/design_matrix.h>
#include <tadah/mlip/trainer.h>

#include <CLI/Timer.hpp>

#ifdef TADAH_ENABLE_HPO
#include <tadah/hpo/hpo.h>
#include <tadah/hpo/hpo_worker.h>
#endif

#include <iostream>
#include <stdexcept>
#include <chrono>

void TadahCLI::subcommand_train() {
  int rank = 0;
  int ncpu = 1;
#ifdef TADAH_BUILD_MPI
  // Initialize MPI variables to determine the rank and the number of CPUs
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &ncpu);
#endif

  // Check if verbose mode is enabled and set it
  if (train->count("--Verbose")) {
    set_verbose();
  }

  // Load the configuration from the specified file
  Config config(config_file);
  std::set<Element> unique_elements = StructureDB::find_unique_elements(config);
  StructureDB::check_atoms_key(config, unique_elements);
  StructureDB::check_watoms_key(config, unique_elements);

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
  // Execute host or worker code based on rank
  if (rank == 0) { // Host code
    // Ensure there are enough CPUs for MPI
    if (ncpu < 2) {
      std::cerr << "Minimum number of CPUs for an MPI version is 2" << std::endl;
      return;
    }

    // The uncertainty flag is incompatible with MPI
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
    TrainerHost host(config, rank, ncpu);
    host.run();

    // Perform the final computation and save parameters
    host.solve();

    Config param_file = host.model->get_param_file();
    param_file.check_pot_file();
    std::ofstream outfile("pot.tadah");
    if (outfile.is_open()) {
      outfile << param_file << std::endl;
    } else {
      std::cerr << "Error: Unable to open file pot.tadah" << std::endl;
    }
    if (is_verbose()) std::cout << timer_tot.to_string() << std::endl;
  }
  else { // Worker code
    // Initialize worker MPI trainer
    TrainerWorker worker(config, rank, ncpu);
    worker.run();
    worker.solve(); // Perform worker computations
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
  int rank=0;
#ifdef TADAH_BUILD_MPI
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
#endif
  if (rank!=0) return;

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

  int rank = 0;
  int ncpu = 1;
#ifdef TADAH_BUILD_MPI
  // Initialize MPI variables to determine the rank and the number of CPUs
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &ncpu);
#endif

  // Check if verbose mode is enabled and set it
  if (train->count("--Verbose")) {
    set_verbose();
  }

  // Load the configuration from the specified file
  Config config(config_file);
  std::set<Element> unique_elements = StructureDB::find_unique_elements(config);
  StructureDB::check_atoms_key(config, unique_elements);
  StructureDB::check_watoms_key(config, unique_elements);
  config.check_for_training();

  // Enable FORCE and STRESS flags in the configuration if specified
  if (hpo->count("--Force")) {
    config.remove("FORCE");
    config.add("FORCE", "true");
  }
  if (hpo->count("--Stress")) {
    config.remove("STRESS");
    config.add("STRESS", "true");
  }

#ifdef TADAH_BUILD_MPI
  MPI_Status status;
  MPI_Group world_group;
  MPI_Comm_group(MPI_COMM_WORLD, &world_group);

  if (rank==0) {
    if (ncpu<2) {
      std::cout << "Minimum number of cpus for an MPI version is 2" << std::endl;
      return;
    }
    if (is_verbose()) std::cout << "Optimising HPs..." << std::endl;
    CLI::Timer timer_tot {"HPO", CLI::Timer::Big};
    hpo_run(config, target_file, validation_file);
    if (is_verbose()) std::cout << "Done" << std::endl;

    // shut all workers
    int cont=0;
    MPI_Bcast(&cont, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if (is_verbose()) std::cout << timer_tot.to_string() << std::endl;
  }
  else { // WORKER
    HPO_Worker hpo_worker(rank, ncpu, world_group);

    while (true) {
      int cs_size;
      MPI_Bcast(&cs_size, 1, MPI_INT, 0, MPI_COMM_WORLD);
      if (!cs_size)
        break;

      // Obtain updated config file from the HOST
      std::vector<char> serialized(cs_size);
      MPI_Bcast(serialized.data(), cs_size, MPI_CHAR, 0, MPI_COMM_WORLD);
      Config config;
      config.deserialize(serialized);
      config.postprocess();

      TrainerWorker worker(config, rank, ncpu);
      worker.run();
      worker.solve();

      // Run LAMMPS
      if (hpo_worker.use_lammps)
        hpo_worker.run();
    } // end of WORKER external while loop
  }
  // Free the communicator and groups when done
  //MPI_Comm_free(&lmp_comm);
  MPI_Group_free(&world_group);
#else
  CLI::Timer timer_tot {"HPO", CLI::Timer::Big};
  if (is_verbose()) std::cout << "Optimising HPs..." << std::endl;
  hpo_run(config, target_file, validation_file);
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

  std::string usage;

  app.add_flag("-v, --version",
               "Print version number and exit");

  /*---------------------------------------------------------------------------*/
  /*     Training                                                              */
  usage = "Train a model using a configuration file.\n";

  train = app.add_subcommand("train", usage);

  usage = 
    "Output Files:\n"
    "pot.tadah\n\n"
    "The train command requires a config file. It results in a pot.tadah file,\n"
    "a trained interatomic potential. This file can be used with LAMMPS for\n"
    "molecular dynamics using `pair_style tadah`. The model trains on energies\n"
    "by default; training with forces and stresses is optional.\n"
    "See documentation for details.\n\n"
    "   Example: tadah train -c config.tadah";
  train->usage(usage);

  usage = 
    "Specify the configuration file with model\n"
    "parameters and datasets.\n"
    "Contains user-defined KEY-VALUE pairs.\n"
    "See documentation for details.\n";

  train->add_option("-c,--config", config_file, usage)
    ->option_text("CONFIG_FILE")
    ->check(CLI::ExistingFile)
    ->required();

  train->add_flag("-F,--Force", "Include forces in training.");
  train->add_flag("-S,--Stress", "Include stresses in training.");
  train->add_flag("-V,--Verbose", "Enable verbose output.");
  train->add_flag("-u,--uncertainty", "Output uncertainty on weights.");
  /*---------------------------------------------------------------------------*/


  /*---------------------------------------------------------------------------*/
  /*     Prediction                                                            */
  /*---------------------------------------------------------------------------*/
  usage =
    "Predict using an already trained model.\n"
    "Energy per atom is always calculated.\n"
    "Calculating forces and stresses is optional.\n";

  predict = app.add_subcommand("predict", usage);

  usage =
    "Output Files:\n"
    "Energies are written to `energy.pred`. Forces and stresses, if calculated,\n"
    "are written to `forces.pred` and `stress.pred` files, respectively\n\n"
    "Option 1:\n"
    "Use a config file with the DBFILE key. Include FORCE and STRESS keys as needed.\n"
    "Note that --Force and --Stress flags override config keys.\n\n"
    "   Example: tadah predict -c config.tadah -p pot.tadah\n\n"
    "Option 2:\n"
    "Provide datasets and flags via command line.\n\n"
    "   Example: tadah predict -p pot.tadah -FS -d db1.tadah";

  predict->usage(usage);

  usage=
    "A config file containing prediction dataset(s).\n"
    "Required Config KEYS: DBFILE.\n"
    "Optional Config KEYS: FORCE, STRESS.\n"
    "See documentation for more details.\n";
  predict->add_option("-c,--config", config_file,usage)
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
                    "Compare predicted values to those in\nthe original dataset.");

  std::string pot_opt="-p,--potential";
  predict->add_option(pot_opt, pot_file,
                      "Trained model")
    ->option_text("POT_FILE")
    ->check(CLI::ExistingFile)
    ->required();

  /*---------------------------------------------------------------------------*/
  /*     Hyperparameter Optimizer                                              */
  /*---------------------------------------------------------------------------*/
#ifndef TADAH_ENABLE_HPO
  usage = "(UNAVAILABLE)\n";
#endif

  usage +=
    "Optimize model architecture and find the best\n"
    "hyperparameters. This process uses hyperparameter\n"
    "optimization to minimize a custom loss function.\n"
    "See documentation for more details.\n";
  hpo = app.add_subcommand("hpo", usage);

  usage =
    "Provide a config file with initial model parameters and training datasets.\n"
    "Parameters will be optimized within constraints defined in the target file.\n\n"
    "   Example: tadah hpo -c config.tadah -t targets -v valid.tadah";
  hpo->usage(usage);

  usage = 
    "Specify a config file with initial model\n"
    "parameters and training datasets.";
  auto c_opt=hpo->add_option("-c,--config", config_file, usage)
    ->option_text("CONFIG_FILE")
    ->check(CLI::ExistingFile)
    ->required();

  usage = 
    "Specify a target file with model targets and\n"
    "hyperparameter constraints.";
  auto t_opt=hpo->add_option("-t,--target", target_file, usage)
    ->option_text("TARGET_FILE")
    ->required()
    ->check(CLI::ExistingFile);

  usage = "Provide a validation file with datasets for\n"
    "testing hyperparameters.";
  auto v_opt=hpo->add_option("-v,--validation", validation_file, usage)
    ->option_text("VALIDATION_FILE")
    ->required()
    ->check(CLI::ExistingFile);

  hpo->add_flag("-F,--Force", "Train with forces.");
  hpo->add_flag("-S,--Stress", "Train with stresses.");
  hpo->add_flag("-V,--Verbose", "Enable verbose mode.");
  //hpo->add_flag("-u,--uncertainty",
  //        "Dump uncertainty on weights."); // TODO check this

  PeriodicTable::initialize();
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
