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
#ifdef TADAH_ENABLE_HPO
#include "../HPO/hpo.h"
#endif

#include <iostream>
#include <stdexcept>

#ifdef TADAH_BUILD_MPI
extern "C" void blacs_get_(int*, int*, int*);
extern "C" void blacs_pinfo_(int*, int*);
extern "C" void blacs_gridinit_(int*, char*, int*, int*);
extern "C" void blacs_gridinfo_(int*, int*, int*, int*, int*);
extern "C" void descinit_(int*, int*, int*, int*, int*, int*, int*, int*, int*, int*);
extern "C" void pdpotrf_(char*, int*, double*, int*, int*, int*, int*);
extern "C" void blacs_gridexit_(int*);
extern "C" int numroc_(int*, int*, int*, int*, int*);
#endif

void TadahCLI::subcommand_train() {

  int rank = 0;
  int ncpu = 1;


#ifdef TADAH_BUILD_MPI
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &ncpu);
#endif

  /* MPI CODE:
   * The PHI matrix is divided into local phi matrices.
   *
   * 1. each rank reads config file
   * 2. host calculates total number of structures
   * 3. host calculates the dimensions of the PHI matrix
   *    based on force and stress flags and type of the regression
   * 4. host broadcasts PHI number of cols and rows to all workers
   * 5. compute sizes for local phi matrices
   * 6. Each worker allocates memory for a local matrix phi
   */


  CLI::Timer timer_tot {"Training", CLI::Timer::Big};

  if(train->count("--verbose"))
    set_verbose();

  Config config(config_file);
  config.check_for_training();

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

  DC_Selector DCS(config);

  // must set DSIZE key as early as possible,
  // this will allow us to querry for the number of columns later on
  DescriptorsCalc<> dc(config,*DCS.d2b,*DCS.d3b,*DCS.dmb,
      *DCS.c2b,*DCS.c3b,*DCS.cmb);

  DM_Function_Base *fb = CONFIG::factory<DM_Function_Base,Config&>(
      config.get<std::string>("MODEL",1),config);

  M_Tadah_Base *model = CONFIG::factory<M_Tadah_Base,DM_Function_Base&,Config&>
    (config.get<std::string>("MODEL",0),*fb,config);

  int PHI_cols;
  int PHI_rows;

  if (rank==0) {
    int nstruct_tot = StructureDB::count(config).first;
    int natoms_tot = StructureDB::count(config).second;
    PHI_cols = fb->get_phi_cols(config);
    PHI_rows = DesignMatrixBase::phi_rows_num(config, nstruct_tot, natoms_tot);
  }
#ifdef TADAH_BUILD_MPI
  MPI_Bcast(&PHI_rows, 1, MPI_INT, 0, MPI_COMM_WORLD);
  MPI_Bcast(&PHI_cols, 1, MPI_INT, 0, MPI_COMM_WORLD);
  std::cout << "PHI ROWS: " << PHI_rows << std::endl;
  std::cout << "PHI COLS: " << PHI_cols << std::endl;
#endif

  // Initialize BLACS
  int b_rank;
  int b_row;
  int b_col;
  int context;
  int b_nrows;   // Number of row procs
  int b_ncols;   // Number of column procs
  int rnb = 22;   // Row block size
  int cnb = 22;   // Column block size

#ifdef TADAH_BUILD_MPI
  blacs_pinfo_(&b_rank, &ncpu) ; // BLACS rank and world size
  std::cout << "RANK: " << rank << " BRANK: " << b_rank << " ncpu: " << ncpu << std::endl;

  // for now assume that the CPU grid is tall and narrow
  b_ncols = 1;
  b_nrows = ncpu;
  assert(b_nrows * b_ncols == ncpu);

  std::cout << "b_ncols: " << b_ncols << " b_nrows: " << b_nrows << std::endl;
  int zero = 0;
  blacs_get_(&context,&zero, &zero ); // -> Create context
  std::cout << "ZERO: " << zero << " CONTEXT: " << context << std::endl;
  char layout='R'; // Block cyclic, Row major processor mapping
  blacs_gridinit_(&context, &layout, &b_nrows, &b_ncols ); // Context -> Initialize the grid
  blacs_gridinfo_(&context, &b_nrows, &b_ncols, &b_row, &b_col );
  std::cout << "BRANK: " << b_rank << " BROW: " << b_row << " BCOL: " << b_col << std::endl;

  // Compute the size of the local phi matrices
  int izero = 0;
  // int PHI_size = PHI_rows*PHI_cols;
  int phi_rows = numroc_( &PHI_rows, &rnb, &b_row, &izero, &b_nrows );
  int phi_cols = numroc_( &PHI_cols, &cnb, &b_col, &izero, &b_ncols );
  std::cout << "BRANK: " << b_rank << " phi_row: " << phi_rows << " phi_col " << phi_cols << std::endl;

  // once we know the size of the PHI, we can assign local phi matrices
  Matrix phi;
  if (rank!=0) {
    // allocate matrix for each worker
    phi = Matrix(phi_rows,phi_cols);
  }
#endif

  const int RELEASE_TAG = 2;
  const int WORK_TAG = 0;


  if (rank==0) {

    // prepare work packages
    // filename, first structure index, number of structures to read
    std::vector<std::tuple<std::string,int,int>> wpckgs;
    int nstruc = 88;  // TODO 
    for (const std::string &fn : config("DBFILE")) {
      // get number of structures
      int dbsize = StructureDB::count(fn).first;
      std::cout << "DBSIZE: " << dbsize << std::endl;
      int first=0;
      while(true) {
        if (nstruc < dbsize) {
          wpckgs.push_back(std::make_tuple(fn,first,nstruc));
          first += nstruc;
        } else {
          wpckgs.push_back(std::make_tuple(fn,first,dbsize));
          break;
        }
        dbsize-=nstruc;
      }
    }


    // host
    // seed all workers
    // implement host function to request next work package
    // the workpackage must contain:
    //  - dataset file name as *char
    //  - length of above name
    for (int r= 1; r< ncpu; ++r) {

      if (wpckgs.size()==0) {
        // no more packages, just skip remaining workers
        // we will release them soon...
        break;
      }
      std::tuple<std::string,int,int> wpckg = wpckgs.back();
      wpckgs.pop_back();

      // send dataset filename
      const char *fn = std::get<0>(wpckg).c_str();
      int fn_length = std::get<0>(wpckg).length()+1;  // +1 for char
      MPI_Send (&fn_length, 1, MPI_INT, r, WORK_TAG, MPI_COMM_WORLD);
      MPI_Send (fn, fn_length, MPI_CHAR, r, WORK_TAG, MPI_COMM_WORLD);

      // send index of the first structure to load
      int first = std::get<1>(wpckg);
      MPI_Send (&first, 1, MPI_INT, r, WORK_TAG, MPI_COMM_WORLD);

      // send number of structures to load
      int nstruc = std::get<2>(wpckg);
      MPI_Send (&nstruc, 1, MPI_INT, r, WORK_TAG, MPI_COMM_WORLD);

      std::cout << "HOST: " << fn << " " << first << " " << nstruc << std::endl;
    }

    // work finised, release all workers
    for (int r= 1; r< ncpu; ++r) {
      MPI_Send (0, 0, MPI_INT, r, RELEASE_TAG, MPI_COMM_WORLD);
    }
  }
  else {
    // worker
    int fn_length;
    int first;
    int nstruc;
    MPI_Status status;

    while (true) {
      std::cout << "WORKER NUM: " << b_rank << " waiting..." << std::endl;
      MPI_Recv (&fn_length, 1, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
      std::cout << "WORKER NUM: " << b_rank << " got message!" << std::endl;
      std::cout << "Lenght of char fn: " << fn_length << std::endl;
      std::cout << "MPI_TAG: " << status.MPI_TAG << std::endl;
      if (status.MPI_TAG == RELEASE_TAG) {
        std::cout << "WORKER: " << b_rank << "RELEASE TAG" << std::endl;
        break;
      }
      char *fn  = (char *) malloc(fn_length+1);
      MPI_Recv (fn, fn_length, MPI_CHAR, 0, WORK_TAG, MPI_COMM_WORLD, &status);
      MPI_Recv (&first, 1, MPI_INT, 0, WORK_TAG, MPI_COMM_WORLD, &status);
      MPI_Recv (&nstruc, 1, MPI_INT, 0, WORK_TAG, MPI_COMM_WORLD, &status);
      std::cout << "WORKER: " << fn << " " << first << " " << nstruc << std::endl;

      StructureDB stdb;
      stdb.add(std::string(fn,fn_length),first,nstruc);
      std::cout << "WORKER: " << stdb << std::endl;

      if (fn)
        delete fn;
    }
  }


  if (is_verbose()) std::cout << "Loading structures..." << std::flush;
  StructureDB stdb(config);
  if (is_verbose()) std::cout << "Done!" << std::endl;



  if (is_verbose()) std::cout << "Finding nearest neighbours within: " <<
    config.get<double>("RCUTMAX") << " cutoff distance..." << std::flush;
  NNFinder nnf(config);
  nnf.calc(stdb);
  if (is_verbose()) std::cout << "Done!" << std::endl;

  if (is_verbose()) std::cout << "Training start..." << std::flush;



  // here we want to train with the locally allocated phi
  model->train(stdb,dc);

  if (is_verbose()) std::cout << "Done!" << std::endl;

  Config param_file = model->get_param_file();
  param_file.check_pot_file();
  std::ofstream outfile;
  outfile.open ("pot.tadah");
  outfile << param_file << std::endl;;

  if(train->count("--uncertainty")) {
    t_type weights = model->get_weights();
    t_type unc = model->get_weights_uncertainty();
    Output(param_file,false).print_train_unc(weights, unc);
  }

  if (is_verbose()) std::cout << timer_tot.to_string() << std::endl;

  if(model)
    delete model;
  if(fb)
    delete fb;
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
#ifdef TADAH_ENABLE_HPO
  CLI::Timer timer_tot {"HPO", CLI::Timer::Big};
  // the number of processes in MPI_COMM_WORLD
  int ncpu=1;
  // the rank of this process in MPI_COMM_WORLD
  int rank=0;

#ifdef TADAH_BUILD_MPI
  MPI_Comm_size(MPI_COMM_WORLD, &ncpu);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Status status;  
#endif

  if(hpo->count("--verbose"))
    set_verbose();
  Config config(config_file);
  config.remove("CHECKPRESS");
  config.add("CHECKPRESS", "true");
  config.check_for_training();
  if (is_verbose()) std::cout << "HPO mode" << std::endl;

  if(hpo->count("--Force")) {
    config.remove("FORCE");
    config.add("FORCE", "true");
  }
  if(hpo->count("--Stress")) {
    config.remove("STRESS");
    config.add("STRESS", "true");
  }

  if (hpo->count("--dtargets")) {
    // list of indices to the trg vec to process by this proc
    std::vector<int>local_trg_indices;

    // Everyone read files from the provided directory
    std::vector<fs::path> trg=read_targets(targets_dir);

    // the number of files the process will work on
    // also the size of an array it will get from the host process.
    int s;
    if ( rank == 0 ) {
      // host proc distributes work equally between available processes
      // Each process will recieve an array of integers.
      // Integers correspond to indices in the trg vector
      // e.g. indices 3,4 indicate that the process
      // should work on target files trg[3] and trg[4]

      // prep indices array and fill from 0 to trg.size()-1
      std::vector<int> trg_idx(trg.size());
      std::iota (std::begin(trg_idx), std::end(trg_idx), 0);

      // Establish the number of target files per process.
      // The work should be evenly distributed.
      // e.g.
      // For 4 processes and 19 files
      // 5 5 5 4
      std::vector<int> counts(ncpu,0);
      for (size_t i=0;i<trg.size();++i) {
        counts[i%ncpu]++;
      }

      // Keep first index to sent to each proc
      // For example above 0,5,10,15
      std::vector<int> first_idx(ncpu);
      int sum=0;
      for (int i=0;i<ncpu; ++i) {
        first_idx[i]=sum;
        sum+=counts[i];
      }

      // First send expected size of a chunk
      for (int p = 1; p < ncpu; p++ ){
        s=counts[p];
#ifdef TADAH_BUILD_MPI
        MPI_Send ( &s, 1, MPI_INT, p, 99, MPI_COMM_WORLD );
#endif
      }

      // and prepare host process for its own work
      s=counts[rank];
      local_trg_indices.resize(s);
#ifdef TADAH_BUILD_MPI
      MPI_Scatterv(trg_idx.data(), counts.data(), first_idx.data(),
          MPI_INT, local_trg_indices.data(), s, MPI_INT, 0, MPI_COMM_WORLD);
#endif
    }
    else  {
#ifdef TADAH_BUILD_MPI
      // Get the size of work to be done
      MPI_Recv ( &s, 1, MPI_DOUBLE, 0, 99, MPI_COMM_WORLD, &status );
#endif
      // We know the amount of work, so can resize array
      local_trg_indices.resize(s);


#ifdef TADAH_BUILD_MPI
      // Finally get indices to the trg array
      MPI_Scatterv(NULL, NULL, NULL, MPI_INT, local_trg_indices.data(),
          s, MPI_INT, 0, MPI_COMM_WORLD);
#endif
    }

    // Finally, we can start working on each target file assigned.
    // But first we have to create sensible directory structure
    // where we can dump all the output. 
    // The user provides the outdir which we assume that is empty 
    // and create subdirectories which corresponds to the names
    // of the target files. All paths should be absolute.
    // Note that we do not parallelise here over threads
    // because parallelisation is done elsewhere.

    // Keep program current working directory
    fs::path cwd = fs::absolute(fs::current_path());

    // Prepare outdir path
    fs::path outdir = cwd.append(targets_out_dir);

    for (const int idx: local_trg_indices) {
      // Build an absolute path for target computations
      // inside the user specified outdir.
      // Use the name of the target file without extenstion
      fs::path outdir_target = outdir;
      outdir_target /= trg[idx].filename().replace_extension("");

      // Create output dir for trg[idx] target
      std::filesystem::create_directory(outdir_target);

      // Copy target file to its output directory
      // so the user has a copy for future reference
      fs::copy(fs::absolute(trg[idx]),outdir_target);

      // Change to target working directory
      std::filesystem::current_path(outdir_target);

      // Get the name for a target file
      std::string target_file = trg[idx].filename();

      // Run computation
      hpo_run(config, target_file);

      // Just in case return to where we started
      std::filesystem::current_path(cwd);
    }
  }
  else if(hpo->count("--target")) {
    hpo_run(config, target_file);
  }

  if (is_verbose()) std::cout << timer_tot.to_string() << std::endl;
#else
  std::cout << "-----------------------------------------------" << std::endl;
  std::cout << "This subcommand is not supported by this build." << std::endl;
  std::cout << "Ta-dah! Must by compiled with HPO support." << std::endl;
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
  std::string v= "ta-dah version " + std::to_string(v_world)
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
  ss << "-e.g. ta-dah predict -c config.tadah -p pot.tadah\n\n";
  ss << "Option 2:\n Provide datasets and flags in the command line.\n";
  ss << "-e.g. ta-dah predict -p pot-tadah -FS -d db1.tadah\n";

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

  ss.str(std::string());
  ss << "Output directory for -d option.\n";
  auto o_opt=hpo->add_option("-o,--out_dir", targets_out_dir, ss.str())
    ->option_text("OUTPUT_DIRECTORY")
    ->check(CLI::ExistingDirectory)
    ->excludes(t_opt)
    ->needs(c_opt);

  ss.str(std::string());
  ss << "A direcory containing model's target files.\n";
  ss << "Each target file must contain a set of\n";
  ss << "hyperparameter constraints as in the -t option.\n";
  ss << "This option can be run with MPI.\n";
  ss << "Each MPI process will run independent\n";
  ss << "optimisation for every target in the directory\n";
  ss << "resulting in N models for N targets.\n";
  ss << "See documentation for more details.\n";
  hpo->add_option("-d,--dtargets", targets_dir, ss.str())
    ->option_text("TARGETS_DIRECTORY")
    ->check(CLI::ExistingDirectory)
    ->excludes(t_opt)
    ->needs(c_opt)
    ->needs(o_opt);


  hpo->add_flag("-F,--Force", "Train with forces.");
  hpo->add_flag("-S,--Stress", "Train with stresses.");
  hpo->add_flag("-v,--verbose", "Verbose mode on.");
  //hpo->add_flag("-u,--uncertainty",
  //        "Dump uncertainty on weights."); // TODO check this
#endif
}

void TadahCLI::run(int argc, char** argv) {

  if(app.count_all()==1) {
    std::cout << "Type: `ta-dah -h` for help" << std::endl;
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
