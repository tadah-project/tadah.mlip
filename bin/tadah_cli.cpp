#include "tadah_cli.h"

#include <tadah/core/config.h>
#include <tadah/core/core_types.h>
#include <tadah/core/element.h>
#include <tadah/core/periodic_table.h>
#include <tadah/core/utils.h>

#include <tadah/mlip/analytics/analytics.h>
#include <tadah/mlip/dataset_readers/dataset_reader_selector.h>
#include <tadah/mlip/design_matrix/design_matrix.h>
#include <tadah/mlip/design_matrix/functions/dm_function_base.h>
#include <tadah/mlip/descriptors_calc.h>
#include <tadah/mlip/models/m_tadah_base.h>
#include <tadah/mlip/nn_finder.h>
#include <tadah/mlip/output/output.h>
#include <tadah/mlip/structure_db.h>
#include <tadah/mlip/trainer.h>
#include <tadah/mlip/version.h>

#include <tadah/models/dc_selector.h>

#include <CLI/Timer.hpp>

#ifdef TADAH_ENABLE_HPO
#include <tadah/hpo/hpo.h>
#include <tadah/hpo/hpo_worker.h>
#endif

#include <iostream>
#include <string>

TadahCLI::TadahCLI():
  app(CLI::App{logo})
{

#ifdef TADAH_BUILD_MPI
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &ncpu);
#endif

  PeriodicTable::initialize();

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
    ->check(CLI::ExistingFile);

  predict->add_option("-d,--datasets", datasets,
                      "One or more datasets for prediction.")
    ->excludes("-c")
    ->check(CLI::ExistingFile);

  predict->add_flag("-F,--Force", "Predict forces.");
  predict->add_flag("-S,--Stress", "Predict stresses.");
  predict->add_flag("-V,--Verbose", "Verbose mode on.");
  predict->add_flag("-e,--error", "Predict model error.");
  predict->add_flag("-a,--analytics",
                    "Compare predicted values to those in\nthe original dataset.");

  predict->add_option("-p,--potential", pot_file,
                      "Trained model")
    ->check(CLI::ExistingFile)
    ->required();

  /*---------------------------------------------------------------------------*/
  /*     Hyperparameter Optimizer                                              */
  /*---------------------------------------------------------------------------*/
  usage = ""; // reset previous value b/c of below if
#ifndef TADAH_ENABLE_HPO
  usage = "(UNAVAILABLE)\n";
#endif

  usage +=
    "Optimize the model architecture and find\n"
    "the best hyperparameters by optimizing\n"
    "a custom loss function.\n";
  hpo = app.add_subcommand("hpo", usage);

  usage =
    "Provide a config file with initial model parameters and training datasets.\n"
    "Parameters will be optimized within constraints defined in the target file.\n"
    "The energies predicted will be validated again the validation file.\n"
    "See documentation for more details.\n\n"
    "   Example: tadah hpo -c config.tadah -t targets -v valid.tadah";
  hpo->usage(usage);

  usage = 
    "Specify a config file with initial model\n"
    "parameters and training datasets.";
  hpo->add_option("-c,--config", config_file, usage)
    ->check(CLI::ExistingFile)
    ->required();

  usage = 
    "Specify a target file with model targets and\n"
    "hyperparameter constraints.";
  hpo->add_option("-t,--target", target_file, usage)
    ->required()
    ->check(CLI::ExistingFile);

  usage = "Provide a validation file with datasets for\n"
    "testing hyperparameters.";
  hpo->add_option("-v,--validation", validation_file, usage)
    ->required()
    ->check(CLI::ExistingFile);

  hpo->add_flag("-F,--Force", "Train with forces.");
  hpo->add_flag("-S,--Stress", "Train with stresses.");
  hpo->add_flag("-V,--Verbose", "Enable verbose mode.");
  //hpo->add_flag("-u,--uncertainty",
  //        "Dump uncertainty on weights."); // TODO check this


  /*---------------------------------------------------------------------------*/
  /*    DB CONVERSION                                                          */
  /*---------------------------------------------------------------------------*/

  usage = 
    "Converts and processes computational\n"
    "model datasets into the Tadah!\n"
    "database format.\n"; 

  convert = app.add_subcommand("convert", usage);

  usage =
    "This tool extracts necessary information from DFT\n"
    "calculations, such as atomic positions, chemical elements, forces,\n"
    "potential energy, and the virial stress tensor if available.\n\n"
    "Input Files:\n"
    "Currently supported input formats include:\n"
    "  - VASP: OUTCAR and vasprun.xml\n"
    "  - CASTEP: .castep, .md, and .geom files\n\n"
    "Example Usage:\n"
    "  tadah convert -d run1.outcar -o single_output.tadah\n"
    "  tadah convert -d run1.md run2.geom run3.castep -o mydata.tadah\n\n"
    "Output Files:\n"
    "  The converted dataset will be in a format suitable for use with Tadah!.\n";

  convert->usage(usage);

  usage =
    "Specify the output file.\n"
    "The converted data will be saved to this file.";

  convert->add_option("-o,--outfile", out_file, usage)
    ->check(CLI::NonexistentPath)
    ->required();

  usage = 
    "Specify one or more dataset files to convert.\n"
    "Supported formats include:\n"
    ".md, .geom, .castep, OUTCAR, and vasprun.xml.";

  convert->add_option("-d,--datasets", datasets, usage)
    ->required()
    //->excludes("-c")
    ->check(CLI::ExistingFile);

  convert->add_option("-p,--precision", outprec,
                      "Set the number of decimal places for numerical output.");

  convert->add_flag("-V,--Verbose", verbose, "Enable verbose mode.");

  usage = "A set of tools for editing databases";

  db = app.add_subcommand("db", "Perform operations on Tadah! datasets.");

  /*---------------------------------------------------------------------------*/
  /*     DB DUPLICATES                                                         */
  /*---------------------------------------------------------------------------*/
  usage = 
    "Identify duplicate structures within specified datasets. "
    "If no output file is specified, a summary is provided only.\n\n"
    "Structures are considered the same if:\n"
    "  1. Number of atoms matches.\n"
    "  2. Cell vectors are within a threshold.\n"
    "  3. Atomic coordinates and element types are identical (order-independent).\n\n"
    "Note: This tool cannot distinguish between rotated or translated configurations.";

  dups = db->add_subcommand("dups", usage);

  usage = 
    "Specify one or more Tadah! datasets to scan for duplicates. "
    "If using -c, provide a single output file. Otherwise, "
    "the number of output files must match the datasets.";

  dups->add_option("-d,--datasets", datasets, usage)
    ->required()
    ->check(CLI::ExistingFile);

  usage =
    "Specify the output file(s) for pruned data. Path must not exist.";

  dups->add_option("-o,--outfiles", out_files, usage)
    ->check(CLI::NonexistentPath);

  dups->add_option("-t,--threshold", val, 
                   "Threshold value for comparing atomic positions and box vectors. Default: 1e-6");

  dups->add_flag("-m,--merge", 
                 "Merge results into a single dataset.")
    ->needs("-o");

  dups->add_flag("-V,--verbose", verbose, 
                 "Enable verbose output for detailed information.");

  /*---------------------------------------------------------------------------*/
  /*     DB JOIN                                                               */
  /*---------------------------------------------------------------------------*/
  usage = 
    "Merge multiple datasets into a unified dataset file.";
  join = db->add_subcommand("join", usage);

  usage = "Provide a list of Tadah! datasets to merge.";
  join->add_option("-d,--datasets", datasets, usage)
    ->required()
    ->check(CLI::ExistingFile);

  usage =
    "Specify the output file for combined data. Path must not exist.";
  join->add_option("-o,--outfile", out_file, usage)
    ->required()
    ->check(CLI::NonexistentPath);

  join->add_flag("-V,--Verbose", verbose, "Enable verbose mode for detailed output.");

  /*---------------------------------------------------------------------------*/
  /*     DB SPLIT                                                              */
  /*---------------------------------------------------------------------------*/
  usage = 
    "Divide the dataset into smaller parts using specified algorithms.";
  split = db->add_subcommand("split", usage);

  usage = "Specify the Tadah! dataset to split.";
  split->add_option("-d,--datasets", in_file, usage)
    ->required()
    ->check(CLI::ExistingFile);

  usage =
    "List the output files for the resulting datasets. Paths must not exist.";
  split->add_option("-o,--outfiles", out_files, usage)
    ->required()
    ->check(CLI::NonexistentPath);

  split->add_flag("-r, --randomize", "Randomize the order of entries before splitting.");

  split_group = split->add_option_group("Split Algorithm", "Choose an algorithm for splitting the dataset.");
  split_group->required();

  split_group->add_flag("-e,--equal", "Split the dataset into equal-sized parts.");

  usage= 
    "Specify sizes for each part. Sum must not exceed total size.";
  split_group->add_option("-s,--sizes", sizes, usage)
    ->excludes("-e");

  usage =
    "Specify percentages for each part. Total must sum to 100%.";
  split_group->add_option("-p,--percentage", sizes, usage)
    ->excludes("-e")
    ->excludes("-s");

  split->add_flag("-V,--Verbose", verbose, "Enable verbose mode for detailed output.");

  /*---------------------------------------------------------------------------*/
  /*     DB SAMPLE                                                             */
  /*---------------------------------------------------------------------------*/
  usage = 
    "Create a new dataset by sampling from existing datasets using specified algorithms.";
  sample = db->add_subcommand("sample", usage);

  usage = "Specify the Tadah! datasets to sample from.";
  sample->add_option("-d,--datasets", datasets, usage)
    ->required()
    ->check(CLI::ExistingFile);

  usage =
    "Specify the location for the output sample file.";
  sample->add_option("-o,--outfile", out_file, usage)
    ->required()
    ->check(CLI::NonexistentPath);

  sample_group = sample->add_option_group("Sampling Algorithm", "Select a sampling algorithm.");
  sample_group->required();

  sample_group->add_option("-e,--every", N, "Sample every N-th entry from the dataset.");

  usage= 
    "Randomly select N entries from the dataset.";
  sample_group->add_option("-r,--random", N, usage)
    ->excludes("-e");

  sample_group->add_option("-i,--indices",
                           indices,
                           "Sample using specified indices.\n"
                           "Supports:\n"
                           "- Single numbers\n"
                           "- Ranges (START-STOP)\n"
                           "- Ranges with steps\n"
                           "  (START-STOP:STEP)\n"
                           "Separate tokens with commas.\n"
                           "Duplicates are removed.\n"
                           "Indexing starts at 1.\n"
                           "e.g., 1,2,4,4-6,10-14:2 expands to:\n"
                           "1,2,4,5,6,10,12,14.")
    ->excludes("-r")
    ->excludes("-e");

  sample->add_flag("-V,--Verbose", verbose, "Enable verbose mode for detailed output.");

  /*---------------------------------------------------------------------------*/
  /*     DB SUMMARY                                                            */
  /*---------------------------------------------------------------------------*/
  usage = 
    "Generate a summary report of the specified dataset(s).";
  summary = db->add_subcommand("summary", usage);

  usage = "Specify one or more Tadah! datasets to summarize.";
  summary->add_option("-d,--datasets", datasets, usage)
    ->required()
    ->check(CLI::ExistingFile);


  /*---------------------------------------------------------------------------*/
  /*     DESCRIPTORS                                                           */
  /*---------------------------------------------------------------------------*/

  usage = 
    "Descriptors tool.\n";

  desc = app.add_subcommand("desc", usage);

  /*---------------------------------------------------------------------------*/
  /*     DESC CALC                                                             */
  /*---------------------------------------------------------------------------*/
  usage = 
    "Calculate descriptors using a potential "
    "file for specified datasets.";

  dcalc = desc->add_subcommand("dcalc", usage);

  usage =
    "Specify the Tadah! datasets for "
    "descriptor calculation.";
  dcalc->add_option("-d,--datasets", datasets, usage)
    ->required()
    ->check(CLI::ExistingFile);

  usage =
    "Specify the output file location.";
  dcalc->add_option("-o,--outfile", out_files, usage)
    ->check(CLI::NonexistentPath);

  dcalc->add_option("-p,--potential", pot_file, "Trained model file")
    ->check(CLI::ExistingFile)
    ->required();

  dcalc->add_option("-i,--indices", indices,
                    "Specify structure numbers to calculate:\n"
                    "- Single numbers\n"
                    "- Ranges (START-STOP)\n"
                    "- Ranges with steps (START-STOP:STEP)\n"
                    "Separate tokens with commas.\n"
                    "Removes duplicates. Starts at 1.\n"
                    "e.g., 1,2,4,4-6,10-14:2 expands to:\n"
                    "1,2,4,5,6,10,12,14.");

  dcalc->add_flag("-m,--merge", "Merge result into single output file.")->needs("-o");
  dcalc->add_flag("-F,--Force", "Calculate force descriptors.");
  dcalc->add_flag("-V,--Verbose", verbose, "Enable verbose mode for detailed output.");
}

int TadahCLI::run(int argc, char** argv) {

  if(rank==0 && app.count_all()==1) {
    std::cout << "Type: `tadah -h` for help" << std::endl;
    return 0;
  }
  else if (rank==0 && app.count("--version")) {
    return flag_version();
  }
  else if (*hpo) {
    return sub_hpo(argc,argv);
  }
  else if (*train) {
    return sub_train();
  }
  else if (*predict) {
    return sub_predict();
  }
  else if (*convert) {
    return sub_convert();
  }
  else if (*db) {
    if (*dups) {
      return subsub_dups();
    }
    else if (*join) {
      return subsub_join();
    }
    else if (*split) {
      return subsub_split();
    }
    else if (*summary) {
      return subsub_summary();
    }
    else if (*sample) {
      return subsub_sample();
    }
  }
  else if (*desc) {
    if (*dcalc) {
      return subsub_dcalc();
    }
  }
  return 0;
}

bool TadahCLI::is_verbose() {
  return verbose;
}

void TadahCLI::set_verbose() {
  verbose=true;
}

int TadahCLI::flag_version() {
  int v_world = TADAH_WORLD_VERSION;
  int v_major = TADAH_MAJOR_VERSION;
  int v_minor = TADAH_MINOR_VERSION;
  std::string v= "tadah version " + std::to_string(v_world)
    + "." + std::to_string(v_major) + "." + std::to_string(v_minor);
  std::cout << v << std::endl;
  return 0;
}

int TadahCLI::sub_train() {

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
  return 0;
}

int TadahCLI::sub_predict() {
  if (rank!=0) return 0;

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

  return 0;
}

int TadahCLI::sub_hpo(
  [[maybe_unused]]int argc,
  [[maybe_unused]]char**argv) {

#ifdef TADAH_ENABLE_HPO


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

  return 0;
}

int TadahCLI::sub_convert() {
  if (rank!=0) return 0;

  CLI::Timer timer_tot {"Conversion", CLI::Timer::Big};
  if(convert->count("--Verbose"))
    set_verbose();

  StructureDB stdb;

  std::vector<std::string> sum_str;
  for (auto &ds: datasets) {
    StructureDB stdb_;
    std::unique_ptr<DatasetReader> reader = DatasetReaderSelector::get_reader(ds,stdb_);
    reader->parse_data();
    if (reader)
      sum_str.push_back(reader->get_summary());
    stdb.add(stdb_);
  }

  if (convert->count("--precision"))
    stdb.dump_to_file(out_file, outprec);
  else
    stdb.dump_to_file(out_file);

  if (is_verbose()) {
    int count = 0;
    for (const auto &s : sum_str) {
      std::cout << "Set: " << std::to_string(count+1) << " | File: " << datasets[count] << std::endl;
      std::cout<< "   " << s;
      count++;
    }
    std::cout << "-----------------------------------------" << std::endl;
    std::cout << "|                SUMMARY                |" << std::endl;
    std::cout << "-----------------------------------------" << std::endl;
    std::cout << "Total number of sets: " << sum_str.size() << std::endl;
    std::cout << "Total number of configurations: " << stdb.size() << std::endl;
    std::cout << "Total number of atoms: " << stdb.calc_natoms() << std::endl;
    std::cout << "Elements: ";
    for (const auto &e: stdb.get_unique_elements()) {
      std::cout << e.symbol << " ";
    }
    std::cout << std::endl;
    std::cout << "Dataset dumped to: " << out_file << std::endl;
    std::cout << timer_tot.to_string() << std::endl;
    std::cout << "Done!" << std::endl;
  }
  return 0;
}

int TadahCLI::subsub_join() {
  if (rank!=0) return 0;
  CLI::Timer timer_tot{"Joining", CLI::Timer::Big};
  StructureDB stdb;
  for (const auto &s: datasets) {
    stdb.add(s);
  }
  stdb.dump_to_file(out_file);
  if (verbose) std::cout << stdb.summary();
  if (verbose) std::cout << timer_tot.to_string() << std::endl;
  return 0;
}

int TadahCLI::subsub_split() {
  if (rank!=0) return 0;
  CLI::Timer timer_tot{"Splitting", CLI::Timer::Big};
  //target db split db1 -o db2 db3 db4 -e -- equal
  //target db split db1 -o db2 db3 db4 -p --percentage 30 40 70 
  //target db split db1 -o db2 db3 db4 -s --size 120 240 230
  // -r randomize
  StructureDB main_stdb;
  main_stdb.add(in_file);

  if (split->count("--equal")) {
    sizes = divide_into_parts(main_stdb.size(),out_files.size());
  }
  else if (split->count("--sizes")) {
    size_t total=0;
    for (const auto &s: sizes) total+= s;
    if (total>main_stdb.size()) throw std::runtime_error("Parts sizes > available total.");
  }
  else if (split->count("--percentage")) {
    size_t total=0;
    for (const auto &s: sizes) total+= s;
    if (total!=100) throw std::runtime_error("Percentages do not add up to 100.");
    sizes = divide_by_percentage(main_stdb.size(),sizes);
  }

  std::vector<StructureDB> stdbs(out_files.size());

  std::vector<size_t> indices(main_stdb.size());
  std::iota(indices.begin(), indices.end(), 0);

  if (split->count("--randomize")) {
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(indices.begin(), indices.end(), g);

  }

  if (out_files.size()!=sizes.size()) throw std::runtime_error("size(percentages)!=size(out_files)");

  size_t total = 0;
  for (size_t i=0; i < stdbs.size(); ++i) {
    StructureDB &stdb = stdbs[i];
    for (size_t j=0; j < sizes[i]; ++j) {
      stdb.add(main_stdb(indices[total++]));
    }
    stdb.dump_to_file(out_files[i]);
       if (verbose) std::cout << stdb.summary();
  }
  if (verbose) std::cout << timer_tot.to_string() << std::endl;
  return 0;
}

int TadahCLI::subsub_summary() {
  if (rank!=0) return 0;
  for (const auto &infile: datasets) {
    StructureDB stdb;
    stdb.add(infile);
    std::cout << stdb.summary();
  }
  return 0;
}

int TadahCLI::subsub_sample() {
  if (rank!=0) return 0;
  CLI::Timer timer_tot{"Sampling", CLI::Timer::Big};
  std::vector<StructureDB> in_stdbs(datasets.size());

  size_t total=0;
  for (size_t i=0; i < in_stdbs.size(); ++i) {
    in_stdbs[i].add(datasets[i]);
    total += in_stdbs[i].size();
  }

  if (sample->count("--every") || sample->count("--random")) {
    if (N>total) throw std::runtime_error(
      "Selected number is larger than total number of configurations availble.");
  }
  if (sample->count("--indices") && datasets.size()!=1) {
    throw std::runtime_error("-i option requires single datafile.");
  }

  StructureDB out_stdb;
  if (sample->count("--every")) {
    size_t idx=0;
    for (const auto &stdb: in_stdbs) {
      for (size_t j=0; j < stdb.size(); ++j) {
        if (!(idx++ % N)) out_stdb.add(stdb(j));
      }
    }
  }
  else if (sample->count("--random")) {
    std::vector<std::pair<size_t, size_t>> indices(total);
    size_t idx=0;
    for (size_t i=0; i < in_stdbs.size(); ++i) {
      for (size_t j=0; j < in_stdbs[i].size(); ++j) {
        indices[idx++] = std::make_pair(i,j);
      }
    }
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(indices.begin(), indices.end(), g);
    for (size_t idx=0; idx < N; ++idx) {
      size_t i = indices[idx].first;
      size_t j = indices[idx].second;
      out_stdb.add(in_stdbs[i](j));
    }
  }
  else if (sample->count("--indices")) {
    std::cout << indices << std::endl;
    std::vector<size_t> idx = parse_indices(indices);
    size_t max_index = *std::max_element(idx.begin(), idx.end());
    if (max_index>in_stdbs[0].size()) 
      throw std::runtime_error("Selected index is larger than the number of available structures.");
    for (const auto &i : idx) {
      out_stdb.add(in_stdbs[0](i-1));
    }
  }

  out_stdb.dump_to_file(out_file);
  if (verbose) std::cout << out_stdb.summary();
  if (verbose) std::cout << timer_tot.to_string() << std::endl;
  return 0;
}

int TadahCLI::subsub_dups() {
  if (rank!=0) return 0;
  CLI::Timer timer_tot{"Duplicates", CLI::Timer::Big};
  std::vector<StructureDB> in_stdbs(datasets.size());

  if (!dups->count("--threshold")) {
    val=1e-6;
  }
  if (dups->count("--merge") && out_files.size()!=1) {
    throw std::runtime_error("-m option requires single outfile.");
  }

  size_t total=0;
  sizes.resize(in_stdbs.size());
  for (size_t i=0; i < in_stdbs.size(); ++i) {
    in_stdbs[i].add(datasets[i]);
    total += in_stdbs[i].size();
    sizes[i] = in_stdbs[i].size();
  }

  std::vector<std::pair<size_t, size_t>> indices(total);
  size_t idx=0;
  for (size_t i=0; i < in_stdbs.size(); ++i) {
    for (size_t j=0; j < in_stdbs[i].size(); ++j) {
      indices[idx++] = std::make_pair(i,j);
    }
  }

  std::vector<std::pair<size_t, size_t>> duplicates;
  for (size_t i=0; i < indices.size(); ++i) {
    for (size_t j=i+1; j < indices.size(); ++j) {
      Structure &st_i = in_stdbs[indices[i].first](indices[i].second);
      Structure &st_j = in_stdbs[indices[j].first](indices[j].second);
      if (st_i.is_the_same(st_j,val)) {
        duplicates.push_back(std::make_pair(i,j));
      }
    }
  }

  if (verbose || !dups->count("-o")) {
    size_t d=0;
    for (const auto &db : datasets) if (d<db.size()) d=db.size()+2;
    size_t w = 10;
    for (const auto &dup: duplicates) {
      size_t i1 = dup.first;
      size_t i2 = dup.second;
      std::cout  << std::setw(d) << std::left
        << datasets[indices[i1].first]
        << " : #"
        << std::setw(w)
        << indices[i1].second + 1
        << "  "
        << std::setw(d)
        << datasets[indices[i2].first]
        << " : #"
        << std::setw(w)
        << indices[i2].second + 1
        << std::endl;
    }
  }

  // finaly prune...
  for (size_t idx = duplicates.size(); idx-- > 0;) {
    auto &dup = duplicates[idx];
    size_t i1 = dup.first;  
    in_stdbs[indices[i1].first].remove(indices[i1].second);
  }

  size_t new_total=0;
  for (size_t i=0; i < in_stdbs.size(); ++i) {
    new_total += in_stdbs[i].size();
  }

  if (dups->count("-o")) {
    if (dups->count("-m")) {
      StructureDB stdb;
      for (const auto& _stdb : in_stdbs) stdb.add(_stdb);
      stdb.dump_to_file(out_files[0]);
    }
    else {
      std::cout << std::endl; 
      for (size_t i=0; i<in_stdbs.size(); ++i) {
        in_stdbs[i].dump_to_file(out_files[i]);
        if (verbose) {
          std::cout << datasets[i] << " --> " << out_files[i] << "   "
            << sizes[i] << " --> " << in_stdbs[i].size()
          << std::endl;
        }
      }
    }
  }

  std::cout << std::endl; 
  std::cout << "Total number of duplicates found: " << duplicates.size() <<std::endl; 
  std::cout << "Total number of unique structures: " << new_total <<std::endl; 

  //if (verbose) std::cout << out_stdb.summary();
  if (verbose) std::cout << timer_tot.to_string() << std::endl;
  return 0;
}

int TadahCLI::subsub_dcalc() {
  if (rank!=0) return 0;
  CLI::Timer timer_tot{"Descriptors Calculator", CLI::Timer::Big};
  std::vector<StructureDB> stdbs(datasets.size());

  if (dcalc->count("--indices") && datasets.size()!=1) {
    throw std::runtime_error("-i option requires single datafile.");
  }
  if (dcalc->count("-o")) {
    if (dcalc->count("--merge") && out_files.size()!=1) {
      throw std::runtime_error("-m option requires single outfile.");
    }
    else if (!dcalc->count("--merge") && datasets.size()!=out_files.size())
      throw std::runtime_error("Number of datasets != number of out files");
  }

  Config pot_config(pot_file);
  if (dcalc->count("--Force")) {
    pot_config.remove("FORCE");
    pot_config.add("FORCE", "true");
  }
  if (dcalc->count("--Stress")) {
    pot_config.remove("STRESS");
    pot_config.add("STRESS", "true");
  }

  DC_Selector DCS(pot_config);
  NNFinder nnf(pot_config);


  // size_t total=0;
  //sizes.resize(stdbs.size());
  for (size_t i=0; i < stdbs.size(); ++i) {
    stdbs[i].add(datasets[i]);
    // total += stdbs[i].size();
    //sizes[i] = stdbs[i].size();
    nnf.calc(stdbs[i]);
  }

  std::vector<std::vector<size_t>> idxs;
  if (dcalc->count("-i")) {
    idxs.push_back(parse_indices(indices));
  }
  else {
    for (size_t i=0; i < stdbs.size(); ++i) {
      std::vector<size_t> idx;
      for (size_t j=0; j < stdbs[i].size(); ++j) {
        idx.push_back(j+1);
      }
      idxs.push_back(idx);
    }
  }

  DescriptorsCalc<> dc(pot_config,*DCS.d2b,*DCS.d3b,*DCS.dmb,
                       *DCS.c2b,*DCS.c3b,*DCS.cmb);

  //StDescriptors st_d = dc.calc(stdbs[0](0));

  std::vector<StDescriptorsDB> st_desc_dbs(datasets.size());
  if (dcalc->count("-i")) {
    // inidces start from 1
    size_t max_index = *std::max_element(idxs[0].begin(), idxs[0].end());
    if (max_index>stdbs[0].size()) 
      throw std::runtime_error("Selected index is larger than the number of available structures.");

    for (auto const &i : idxs[0]) {
      StDescriptors st_d = dc.calc(stdbs[0](i-1));
      st_desc_dbs[0].add(st_d);
    }
  }
  else {
    for (size_t i=0; i < datasets.size(); ++i) {
      //st_desc_dbs.push_back(dc.calc(stdbs[i])); // does not work!
      for (auto const &j : idxs[i]) {
        StDescriptors st_d = dc.calc(stdbs[i](j-1));
        st_desc_dbs[i].add(st_d);
      }
    }
  }

  std::vector<std::string> output;
  size_t hw = 12;
  size_t prec = 6;
  size_t w = prec + 8;
  std::string dim[] = {"x","y","z"}; 
  for (size_t b=0; b < idxs.size(); ++b) {
    std::ostringstream oss;
    for (size_t i=0; i < idxs[b].size(); ++i) {
      const auto &st_d = st_desc_dbs[b](i);
      const auto &st = stdbs[b](idxs[b][i]-1);
      oss << "Label: " << st.label << std::endl;
      for (size_t a=0; a<st.natoms(); ++a) {
        const aed_type2 &aed = st_d.get_aed(a);
        oss << std::left << std::setw(hw) << ("E " + std::to_string(a+1)) << " ";
        oss << std::scientific << std::setprecision(prec);
        for (size_t s=0; s<aed.size(); ++s) {
          oss << std::right << std::setw(w) <<  aed(s) << "  ";
        }
        oss<<std::endl;
      }

      if (dcalc->count("--Force")) {
        for (size_t a1=0; a1<st.natoms(); ++a1) {
          for (size_t jj=0; jj<st_d.fd[a1].size(); ++jj) {
            const size_t a2=st.near_neigh_idx[a1][jj];
            const fd_type &fij = st_d.fd[a1][jj];
            for (size_t k=0; k<3; ++k) {
              oss << std::left << std::setw(hw) << ("F"+dim[k]+ " " + std::to_string(a1+1)) + " - " +  std::to_string(a2+1) << " ";
              oss << std::scientific << std::setprecision(prec);
              for (size_t d=0; d<fij.rows(); ++d) {
                oss << std::right << std::setw(w) <<  fij(d,k) << "  ";
              }
              oss<<std::endl;
            }
          }
        }
      }
      oss << std::endl;
    }
    output.push_back(oss.str());
  }

  if (dcalc->count("-o")) {
    if (dcalc->count("-m")) {
      auto ds = datasets.begin();
      std::ofstream outfile(out_files[0]);
      for (auto const &out : output) {
        outfile << "Dataset: " << *ds++ << std::endl;
        outfile << out;
      }
    }
    else {
      auto ds = datasets.begin();
      auto of = out_files.begin();
      for (auto const &out : output) {
        std::ofstream outfile(*of++);
        outfile << "Dataset: " << *ds++ << std::endl;
        outfile << out;
      }
    }
  }
  else {
    auto ds = datasets.begin();
    for (auto const &out : output) {
      std::cout << "Dataset: " << *ds++ << std::endl;
      std::cout << out;
    }
  }
  if (verbose) std::cout << timer_tot.to_string() << std::endl;
  return 0;
}
