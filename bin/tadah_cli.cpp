#include "tadah_cli.h"
#include "CLI/CLI.hpp"
#include "tadah/mlip/dataset_readers/dataset_reader.h"
#include <iterator>
#include <string>
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
#include <tadah/mlip/dataset_readers/dataset_reader_selector.h>

#include <CLI/Timer.hpp>
#include <utility>

#ifdef TADAH_ENABLE_HPO
#include <tadah/hpo/hpo.h>
#include <tadah/hpo/hpo_worker.h>
#endif

#include <iostream>
#include <stdexcept>
#include <chrono>

void TadahCLI::sub_train() {
#ifdef TADAH_BUILD_MPI
  int rank = 0;
  int ncpu = 1;
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

void TadahCLI::sub_predict() {
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

void TadahCLI::sub_hpo(
  [[maybe_unused]]int argc,
  [[maybe_unused]]char**argv) {

#ifdef TADAH_ENABLE_HPO

#ifdef TADAH_BUILD_MPI
  int rank = 0;
  int ncpu = 1;
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
void TadahCLI::sub_convert() {
  int rank=0;
#ifdef TADAH_BUILD_MPI
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
#endif
  if (rank!=0) return;

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

  std::string pot_opt="-p,--potential";
  predict->add_option(pot_opt, pot_file,
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
  /*     CONVERSION                                                            */
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
  /*     FIND DUPLICATES                                                       */
  /*---------------------------------------------------------------------------*/
  usage = "Identify duplicate structures within the specified datasets. "
    "If no output file is specified, it will provide a summary only.";

  dups = db->add_subcommand("dups", usage);

  usage = "Specify one or more Tadah! datasets to scan for duplicates. Must exist.";
  dups->add_option("-d,--datasets", datasets, usage)
    ->required()
    ->check(CLI::ExistingFile);

  usage =
    "Specify the output file where pruned data will be saved. Path must not exist.";
  dups->add_option("-o,--outfile", out_file, usage)
    ->check(CLI::NonexistentPath);
  dups->add_flag("-V,--Verbose", verbose, "Enable verbose output for detailed processing information.");

  /*---------------------------------------------------------------------------*/
  /*     JOIN                                                                  */
  /*---------------------------------------------------------------------------*/
  usage = 
    "Merge multiple datasets into a unified dataset file.";
  join = db->add_subcommand("join", usage);

  usage = "Provide a list of Tadah! datasets to merge. Must exist.";
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
  /*     SPLIT                                                                 */
  /*---------------------------------------------------------------------------*/
  usage = 
    "Divide the dataset into smaller parts using specified algorithms.";
  split = db->add_subcommand("split", usage);

  usage = "Specify the Tadah! dataset to split. Must exist.";
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
  /*     SAMPLE                                                                */
  /*---------------------------------------------------------------------------*/
  usage = 
    "Create a new dataset by sampling from existing datasets using specified algorithms.";
  sample = db->add_subcommand("sample", usage);

  usage = "Specify the Tadah! datasets to sample from. Must exist.";
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

  sample->add_flag("-V,--Verbose", verbose, "Enable verbose mode for detailed output.");

  /*---------------------------------------------------------------------------*/
  /*     SUMMARY                                                               */
  /*---------------------------------------------------------------------------*/
  usage = 
    "Generate a summary report of the specified dataset(s).";
  summary = db->add_subcommand("summary", usage);

  usage = "Specify one or more Tadah! datasets to summarize. Must exist.";
  summary->add_option("-d,--datasets", datasets, usage)
    ->required()
    ->check(CLI::ExistingFile);

  PeriodicTable::initialize();
}

void TadahCLI::run(int argc, char** argv) {

  if(app.count_all()==1) {
    std::cout << "Type: `tadah -h` for help" << std::endl;
  }
  else if (app.count("--version")) {
    flag_version();
  }
  else if (*hpo) {
    sub_hpo(argc,argv);
  }
  else if (*train) {
    sub_train();
  }
  else if (*predict) {
    sub_predict();
  }
  else if (*convert) {
    sub_convert();
  }
  else if (*db) {
    if (*dups) {
    }
    else if (*join) {
      subsub_join();
    }
    else if (*split) {
      subsub_split();
    }
    else if (*summary) {
      subsub_summary();
    }
    else if (*sample) {
      subsub_sample();
    }
  }
}

void TadahCLI::subsub_join() {
  CLI::Timer timer_tot{"Joining", CLI::Timer::Big};
  StructureDB stdb;
  for (const auto &s: datasets) {
    stdb.add(s);
  }
  stdb.dump_to_file(out_file);
  if (verbose) std::cout << stdb.summary();
  if (verbose) std::cout << timer_tot.to_string() << std::endl;
}

// helper functions
std::vector<size_t> divide_into_parts(size_t number, size_t n) {
    std::vector<size_t> parts(n, number / n);
    size_t remainder = number % n;
    for (size_t i = 0; i < remainder; ++i) {
        parts[i] += 1;
    }
    return parts;
}

std::vector<size_t> divide_by_percentage(size_t number, const std::vector<size_t>& percentages) {
    std::vector<size_t> parts(percentages.size(), 0);
    size_t totalAssigned = 0;

    for (size_t i = 0; i < percentages.size(); ++i) {
        parts[i] = number * percentages[i] / 100;
        totalAssigned += parts[i];
    }

    size_t remainder = number - totalAssigned;

    // Distribute remaining amount due to rounding
    for (size_t i = 0; remainder > 0; ++i, --remainder) {
        parts[i]++;
    }

    return parts;
}

void TadahCLI::subsub_split() {
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
}
void TadahCLI::subsub_summary() {
  for (const auto &infile: datasets) {
    StructureDB stdb;
    stdb.add(infile);
    std::cout << stdb.summary();
  }
}
void TadahCLI::subsub_sample() {
  CLI::Timer timer_tot{"Sampling", CLI::Timer::Big};
  std::vector<StructureDB> in_stdbs(datasets.size());

  size_t total=0;
  for (size_t i=0; i < in_stdbs.size(); ++i) {
    in_stdbs[i].add(datasets[i]);
    total += in_stdbs[i].size();
  }

  if (N>total) throw std::runtime_error(
    "Selected number is larger than total number of configurations availble.");

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

  out_stdb.dump_to_file(out_file);
  if (verbose) std::cout << out_stdb.summary();
  if (verbose) std::cout << timer_tot.to_string() << std::endl;
}
