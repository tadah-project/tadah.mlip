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


#include <chrono>
#include <thread>

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
  if (ncpu<2) {
    std::cout << "Minimum number of cpus for an mpi version is 2" << std::endl;
    return;
  }
  MPI_Status status;  

  // BEGIN SHARED MEMORY
  // Allocate node shared buffer to store overflow phi rows
  // TODO for now this is just an idea, i.e. the shared memory is not being used
  // Note that the host will stll have to distribute data itself as there is no node-node access
  // to a shared resource. 
//  MPI_Comm nodecomm;
//  int nodesize, noderank;
//  MPI_Win window;
//
//  MPI_Comm_split_type(MPI_COMM_WORLD, MPI_COMM_TYPE_SHARED, rank,
//      MPI_INFO_NULL, &nodecomm);
//
//  MPI_Comm_size(nodecomm, &nodesize);
//  MPI_Comm_rank(nodecomm, &noderank);
//
//  int buffersize=10;  // TODO
//  int localbuffersize = 0;
//
//  if (noderank == 0) localbuffersize = buffersize;
//
//  double *buffer, *localbuffer;
//  MPI_Win_allocate_shared(localbuffersize*sizeof(double), sizeof(double),
//      MPI_INFO_NULL, nodecomm, &localbuffer, &window);
//
//  int windisp; // local unit size for displacements, in bytes (positive integer)
//  MPI_Aint winsize; // size of the segment at the given rank
//  MPI_Win_shared_query(window, 0, &winsize, &windisp, &buffer);
//  MPI_Win_fence(0, window);
//  // All table pointers should now point to copy on noderank 0
//
//  // testing....
//  if (noderank == 0) {
//    for (int i=0; i < buffersize; i++) {
//      buffer[i] = rank*buffersize + i;
//    }
//  }
//
//  MPI_Win_fence(0, window);
//
//  // Check we did it right
//  for (int i=0; i < buffersize; i++) {
//    printf("rank %d, noderank %d, table[%d] = %f\n",
//        rank, noderank, i, buffer[i]);
//  }
//  // END SHARED MEMORY

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

  NNFinder nnf(config);

#ifdef TADAH_BUILD_MPI

  const int WAIT_TAG = 3;
  const int RELEASE_TAG = 2;
  const int DATA_TAG = 1;
  const int WORK_TAG = 0;

  int PHI_cols;
  int PHI_rows;

  if (rank==0) {
    int nstruct_tot = StructureDB::count(config).first;
    int natoms_tot = StructureDB::count(config).second;
    PHI_cols = fb->get_phi_cols(config);
    PHI_rows = DesignMatrixBase::phi_rows_num(config, nstruct_tot, natoms_tot);
  }
  MPI_Bcast(&PHI_rows, 1, MPI_INT, 0, MPI_COMM_WORLD);
  MPI_Bcast(&PHI_cols, 1, MPI_INT, 0, MPI_COMM_WORLD);
  std::cout << "PHI ROWS: " << PHI_rows << std::endl;
  std::cout << "PHI COLS: " << PHI_cols << std::endl;

  // Initialize BLACS
  int b_rank;
  int b_row;
  int b_col;
  int context;
  int b_nrows;   // Number of row procs
  int b_ncols;   // Number of column procs
  int rnb = 22;   // Row block size TODO
  int cnb = 22;   // Column block size TODO

  blacs_pinfo_(&b_rank, &ncpu) ; // BLACS rank and world size
  std::cout << "RANK: " << rank << " BRANK: " << b_rank << " ncpu: " << ncpu << std::endl;

  // for now assume that the CPU grid is tall and narrow TODO
  // the required function must take into considersation:
  // the size of the blok rnb, number of cols and rows in Phi
  // ideally the grid should be as square as possible, unless PHI is narrow
  // Probably we want the number of column processes to be <= ~sqrt(ncpu)/cnb
  b_ncols = 1;
  b_nrows = ncpu;
  assert(b_nrows * b_ncols == ncpu);

  std::cout << "b_ncols: " << b_ncols << " b_nrows: " << b_nrows << std::endl;
  int zero = 0;
  blacs_get_(&zero,&zero, &context ); // -> Create context
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
  std::cout << "BRANK: " << b_rank << " phi_rows: " << phi_rows << " phi_col " << phi_cols << std::endl;

  size_t phi_row = 0; // next row to be filled in the local phi array
  int rows_available=phi_rows;  // number of available rows in the local phi array

  // once we know the size of local phi, we can allocate memory to it
  // including host as well. The host will collect excees computations from
  // workers
  DesignMatrix<DM_Function_Base&> dm(*fb, config);
  dm.Phi.resize(phi_rows,phi_cols);
  //Matrix phi;
  //if (rank!=0) {
  //  // allocate matrix for each worker
  //  phi = Matrix(phi_rows,phi_cols);
  //}

  // BEGIN HOST-WORKER
  if (rank==0) {
    int count=1;  // count number of release workers, skip host
    // HOST: prepare work packages
    // filename, first structure index, number of structures to read
    std::vector<std::tuple<std::string,int,int>> wpckgs;
    int nstruc = 100;  // TODO the number of structures in a single work package
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

    // HOST is waiting for workers requests
    while (true) {

      if (wpckgs.size()==0) {
        // no more packages, just skip remaining workers
        // we will collect remaining data and release them outside of this loop
        break;
      }

      // receive ANY request from ANY worker

      MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
      int worker = status.MPI_SOURCE;
      int tag = status.MPI_TAG;
      std::cout << "HOST1 request from WORKER: " << worker << " TAG: " << tag << std::endl; 


      if (tag==WORK_TAG) {
        int wsize; // array size for DATA_TAG, rows_available for WORK_TAG
        MPI_Recv (&wsize, 1, MPI_INT, worker, tag, MPI_COMM_WORLD, &status);

        std::tuple<std::string,int,int> wpckg = wpckgs.back();
        wpckgs.pop_back();

        // send dataset filename
        const char *fn = std::get<0>(wpckg).c_str();
        int fn_length = std::get<0>(wpckg).length()+1;  // +1 for char
        MPI_Send (&fn_length, 1, MPI_INT, worker, tag, MPI_COMM_WORLD);
        MPI_Send (fn, fn_length, MPI_CHAR, worker, tag, MPI_COMM_WORLD);

        // send index of the first structure to load
        int first = std::get<1>(wpckg);
        MPI_Send (&first, 1, MPI_INT, worker, tag, MPI_COMM_WORLD);

        // send number of structures to load
        int nstruc = std::get<2>(wpckg);
        MPI_Send (&nstruc, 1, MPI_INT, worker, tag, MPI_COMM_WORLD);

        std::cout << "HOST: " << fn << " " << first << " " << nstruc << std::endl;
        //}
      }
      else if (tag==DATA_TAG) {
        int rows_needed;
        MPI_Recv (&rows_needed, 1, MPI_INT, worker, tag, MPI_COMM_WORLD, &status);
        if (rows_available>0) {
          std::cout << "HOST1 received data transfer request from: " << worker << " rows needed: " << rows_needed << " rows_avail:" << rows_available << std::endl;
          int rows_accepted = rows_available < rows_needed ? rows_available : rows_needed;
          MPI_Send (&b_rank, 1, MPI_INT, worker, tag, MPI_COMM_WORLD);
          MPI_Send (&rows_accepted, 1, MPI_INT, worker, tag, MPI_COMM_WORLD);
          MPI_Recv (&rows_accepted, 1, MPI_INT, worker, tag, MPI_COMM_WORLD, &status);
          // get data...
          rows_available -= rows_accepted;
          phi_row+=rows_accepted;
          //int start=phi_row*phi_cols;
          if (rows_available==0 ) { std::cout << "!!! HOST1 LOCAL MATRIX IS FILLED !!!" << std::endl;}
          if (rows_available<0 ) { throw std::runtime_error(" HOST1: The number of rows in the local array is smaller than requested.");}
          //MPI_Recv (&dm.Phi.data()[0], wsize, MPI_DOUBLE, worker, DATA_TAG, MPI_COMM_WORLD, &status);
        }
        else {
          std::cout << "HOST1 received data transfer request from: " << worker << " rows needed: " << rows_needed << " rows_avail:" << rows_available << std::endl;
          // host is unable to fit data we have to ask workers for their storage availability
          //MPI_Probe(MPI_ANY_SOURCE, WORK_TAG, MPI_COMM_WORLD, &status);
          int w_rows_available;
          // find a worker to accept at least some data
          MPI_Status status2;
          int worker2;
          // find a worker capable of accepting data
          while (true) {
            std::cout << "HOST1 searching for a WORKER!" << std::endl;
            MPI_Recv (&w_rows_available, 1, MPI_INT, MPI_ANY_SOURCE, WORK_TAG, MPI_COMM_WORLD, &status2);
            worker2 = status2.MPI_SOURCE;
            if (worker==worker2) {throw std::runtime_error("worker and worker2 are the same."); }
            if (w_rows_available==0 ) {
              // give up on this worker
              MPI_Send (&worker2, 1, MPI_INT, worker2, WAIT_TAG, MPI_COMM_WORLD);
              std::cout << "HOST1 skipping a WORKER!" << std::endl;
            } 
            else {
              std::cout << "HOST1 found a WORKER: " << worker2 << " WORKER NEEDS: " << w_rows_available << std::endl;
              break;
            }
          }

          int rows_accepted = w_rows_available < rows_needed ? w_rows_available : rows_needed;

          MPI_Send (&worker2, 1, MPI_INT, worker, DATA_TAG, MPI_COMM_WORLD);
          MPI_Send (&rows_accepted, 1, MPI_INT, worker, DATA_TAG, MPI_COMM_WORLD);
        }
      }
      else {
        throw std::runtime_error("HOST: Unexpected request from " + std::to_string(worker));
      }
    }

    std::cout << "---------LOOP 1 FINISHED---------" << std::endl;

    // work finised, collect remaining data and release all workers
    while(true) {
      int wsize; // array size for DATA_TAG, rows_available for WORK_TAG
      MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
      int worker = status.MPI_SOURCE;
      int tag = status.MPI_TAG;

      if (tag==DATA_TAG) {
        int rows_needed;
        MPI_Recv (&rows_needed, 1, MPI_INT, worker, DATA_TAG, MPI_COMM_WORLD, &status);

        if (rows_available>0) {
          std::cout << "HOST2 received data transfer request from: " << worker << " rows needed: " << rows_needed << " rows_avail:" << rows_available << std::endl;
          int rows_accepted = rows_available < rows_needed ? rows_available : rows_needed;
          MPI_Send (&b_rank, 1, MPI_INT, worker, DATA_TAG, MPI_COMM_WORLD);
          MPI_Send (&rows_accepted, 1, MPI_INT, worker, DATA_TAG, MPI_COMM_WORLD);

          MPI_Recv (&rows_accepted, 1, MPI_INT, worker, DATA_TAG, MPI_COMM_WORLD, &status);
          // get data...
          rows_available -= rows_accepted;
          phi_row+=rows_accepted;
          //int start=phi_row*phi_cols;
          if (rows_available==0 ) { std::cout << "!!! HOST2 LOCAL MATRIX IS FILLED !!!" << std::endl;}
          if (rows_available<0 ) { throw std::runtime_error(" HOST2: The number of rows in the local array is smaller than requested.");}
          //MPI_Recv (&dm.Phi.data()[0], wsize, MPI_DOUBLE, worker, DATA_TAG, MPI_COMM_WORLD, &status);
        }
        else {
          std::cout << "HOST2 received data transfer request from: " << worker << " rows needed: " << rows_needed << " rows_avail:" << rows_available << std::endl;
          // host is unable to fit data we have to ask workers for their storage availability
          //MPI_Probe(MPI_ANY_SOURCE, WORK_TAG, MPI_COMM_WORLD, &status);
          int w_rows_available;
          // find a worker to accept at least some data
          MPI_Status status2;
          int worker2;

          // find a worker capable of accepting data
          while (true) {
            std::cout << "HOST2 searching for a WORKER!" << std::endl;
            MPI_Recv (&w_rows_available, 1, MPI_INT, MPI_ANY_SOURCE, WORK_TAG, MPI_COMM_WORLD, &status2);
            worker2 = status2.MPI_SOURCE;
            if (worker==worker2) {throw std::runtime_error("worker and worker2 are the same."); }
            if (w_rows_available==0 ) {
              // give up on this worker and release him as there is no more work to be done
              //MPI_Send (&worker2, 1, MPI_INT, worker2, WAIT_TAG, MPI_COMM_WORLD);
              MPI_Send (0, 0, MPI_INT, worker2, RELEASE_TAG, MPI_COMM_WORLD);
              count++;
              std::cout << "HOST2 skipping+releasing a WORKER: " << worker2 << std::endl;
            } 
            else {
              std::cout << "HOST2 found a WORKER: " << worker2 << " WORKER NEEDS: " << w_rows_available << std::endl;
              break;
            }
          }

          int rows_accepted = w_rows_available < rows_needed ? w_rows_available : rows_needed;

          MPI_Send (&worker2, 1, MPI_INT, worker, DATA_TAG, MPI_COMM_WORLD);
          MPI_Send (&rows_accepted, 1, MPI_INT, worker, DATA_TAG, MPI_COMM_WORLD);

              //MPI_Send (&worker2, 1, MPI_INT, worker2, WAIT_TAG, MPI_COMM_WORLD);
        }
      }
      else {
        MPI_Recv (&wsize, 1, MPI_INT, worker, WORK_TAG, MPI_COMM_WORLD, &status);
        // there is no more work so release a worker
        if (wsize==0) {
          MPI_Send (0, 0, MPI_INT, worker, RELEASE_TAG, MPI_COMM_WORLD);
          count++;
        }
        else {
          MPI_Send (0, 0, MPI_INT, worker, WAIT_TAG, MPI_COMM_WORLD);
        }
        if (count==ncpu) {  break; }  // count starts from 1
      }

    }
    std::cout << "HOST: " << "r_avail: " << rows_available << std::endl;
    std::cout << "HOST EXIT" << std::endl;
  } 
  else {
    std::cout << "WORKER INIT: " << b_rank << std::endl;
    // WORKER
    int fn_length;  // length of the filename char array
    int first;  // index of the first structure to read from the file
    int nstruc; // number of structures to be processed

    while (true) {
      // ask for more work...
      std::cout << "WORKER REQUEST: " << b_rank << std::endl;
      MPI_Send (&rows_available, 1, MPI_INT, 0, WORK_TAG, MPI_COMM_WORLD);
      std::cout << "WORKER REQUEST SEND: " << b_rank << std::endl;
      //
      // request from root or from other workers
      //MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
      MPI_Recv (&fn_length, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
      std::cout << "WORKER: " << b_rank  << "RECEIVED FROM " << status.MPI_SOURCE << " TAG: " << status.MPI_TAG << std::endl;



      // if release tag but the worker's local phi is not full
      // the worker sends request to the host for the remaining data

      // release worker
      if (status.MPI_TAG == RELEASE_TAG) {
        std::cout << "WORKER: " << b_rank << " RELEASE TAG" << std::endl;
        //MPI_Send (NULL, NULL, MPI_INT, 0, DATA_TAG, MPI_COMM_WORLD);
        if (rows_available>0 ) { throw std::runtime_error("Attempting to release worker... but the workers requires more data !!");}
        break;
      }
      else if (status.MPI_TAG == WAIT_TAG) {
        std::cout << "WORKER: " << b_rank << " WAIT TAG" << std::endl;
        // do nothing; ask again for work
        // std::this_thread::sleep_for(std::chrono::milliseconds(1000)); //TODO
      }
      else if (status.MPI_TAG == DATA_TAG) {
        // other worker is giving me some data
        std::cout << "WORKER: " << b_rank << " DATA TAG" << std::endl;
        int rows_accepted = fn_length;
        rows_available -= rows_accepted;
        phi_row += rows_accepted;
        // phi_row
        // MPI_Recv (&rows, 1, MPI_INT, 0, DATA_TAG, MPI_COMM_WORLD, &status);
      }
      else if (status.MPI_TAG == WORK_TAG) {
        std::cout << "WORKER: " << b_rank << " WORK TAG" << std::endl;

        // otherwise get work package
        char *fn  = (char *) malloc(fn_length+1);
        MPI_Recv (fn, fn_length, MPI_CHAR, 0, WORK_TAG, MPI_COMM_WORLD, &status);
        MPI_Recv (&first, 1, MPI_INT, 0, WORK_TAG, MPI_COMM_WORLD, &status);
        MPI_Recv (&nstruc, 1, MPI_INT, 0, WORK_TAG, MPI_COMM_WORLD, &status);
        std::cout << "WORKER: " << b_rank << "file: " << fn << " first: " << first << " nstruc: " << nstruc << std::endl;

        // do work
        StructureDB stdb;
        stdb.add(std::string(fn,fn_length),first,nstruc);
        std::cout << "WORKER: " << b_rank << " stdb.size(): " << stdb.size() << std::endl;
        nnf.calc(stdb);

        // temp solution is to compute temp_phi inside DM and then copy data local phi matrix
        //Config c = config;
        //DesignMatrix<DM_Function_Base&> dm(*fb, c);
        //Normaliser norm(c); // <-- we don't want this
        //dm.build(stdb,norm,dc); // this is just for resizing for now

        // size_t phi_row = 0; // local just for now, this is of course wrong 

        // one way to find out what we need in terms of phi size

        int rows_needed = 0;
        for (size_t s=0; s<stdb.size(); ++s) {
          int natoms = stdb(s).natoms();
          rows_needed += DesignMatrixBase::phi_rows_num(config, 1, natoms);
        }

        std:: cout << "WORKER BEGIN COMP: " << b_rank << " R avail: " << rows_available << " R needed: " << rows_needed << std::endl;
        if (rows_available<rows_needed) {
          // we do not have enough rows in the local phi matrix
          // so we create temp DM of required size
          DesignMatrix<DM_Function_Base&> temp_dm(*fb, config);
          temp_dm.Phi.resize(rows_needed,phi_cols);

          // and compute all rows
          size_t temp_phi_row=0;
          for (size_t s=0; s<stdb.size(); ++s) {
            StDescriptors st_d = dc.calc(stdb(s));
            temp_dm.build(temp_phi_row,stdb(s),st_d); // phi_row++
          }

          // first we try to fill remaining rows in the local phi matrix
          if (rows_available>0) {
            for (size_t r=0; r<rows_available; r++) {
              for (size_t c=0; c<phi_cols; c++) {
                dm.Phi(phi_row,c) = temp_dm.Phi(r,c); 
              }
              phi_row++;
              rows_needed--;
            }
          }
          // there are no more available rows
          rows_available=0;
          std::cout << "FULL WORKER: " << b_rank << " rows_available: " << rows_available << " rows_needed: " << rows_needed << std::endl; 
          // send remaining data to available processes
          while (rows_needed > 0) {
            // request host 
            std::cout << "  FULL WORKER: " << b_rank << " request rows_needed a: " << rows_needed << std::endl; 
            MPI_Send (&rows_needed, 1, MPI_INT, 0, DATA_TAG, MPI_COMM_WORLD);
            std::cout << "  FULL WORKER: " << b_rank << " request rows_needed b: " << rows_needed << std::endl; 
            int rows_accepted; // number of accepted rows
            int proc; // receiving process
            // host returns which proc can accept and how much
            MPI_Recv (&proc, 1, MPI_INT, 0, DATA_TAG, MPI_COMM_WORLD, &status);
            std::cout << "  FULL WORKER: " << b_rank << " request rows_needed c: " << rows_needed << std::endl; 
            MPI_Recv (&rows_accepted, 1, MPI_INT, 0, DATA_TAG, MPI_COMM_WORLD, &status);
            std::cout << "  FULL WORKER: " << b_rank << " get rows_accepted: " << rows_accepted << std::endl; 

            // we send data to the host or a willing worker
            MPI_Send (&rows_accepted, 1, MPI_INT, proc, DATA_TAG, MPI_COMM_WORLD);
            std::cout << "  FULL WORKER: " << b_rank << " send rows_accepted: " << rows_accepted << std::endl; 
            rows_needed -= rows_accepted;
            std::cout << "  FULL WORKER: " << b_rank << " rows_needed: " << rows_needed << std::endl; 
            // TODO
            //int start=0;
            //MPI_Send (&temp_dm.Phi.data()[start], arr_size, MPI_DOUBLE, 0, DATA_TAG, MPI_COMM_WORLD);
          }

        }
        else {
          // just fill local phi array
          for (size_t s=0; s<stdb.size(); ++s) {
            //std::cout << "phi_row:" << phi_row << " R avail: " << rows_available << " phi rows: " << phi_rows << std::endl;
            StDescriptors st_d = dc.calc(stdb(s));
            dm.build(phi_row,stdb(s),st_d); // phi_row++
          }
          rows_available-=rows_needed;
        }

        // for every new structure stdb we want to calculate PHI rows
        // using local phi. We have to make sure we do not exceed phi
        // Also we have to keep number of filled rows which will
        // serve as an index for subsequent calls.

        if (fn)
          delete fn;
      }
    }
  }
    std::cout << "RANK" << rank << " HOST-WORKER EXIT SUCCESS" << std::endl;
  // END HOST-WORKER
#endif


//  if (is_verbose()) std::cout << "Loading structures..." << std::flush;
//  StructureDB stdb(config);
//  if (is_verbose()) std::cout << "Done!" << std::endl;
//
//  if (is_verbose()) std::cout << "Finding nearest neighbours within: " <<
//    config.get<double>("RCUTMAX") << " cutoff distance..." << std::flush;
//  nnf.calc(stdb);
//  if (is_verbose()) std::cout << "Done!" << std::endl;
//
//  if (is_verbose()) std::cout << "Training start..." << std::flush;
//
//  // here we want to train with the locally allocated phi
//  model->train(stdb,dc);
//
//  //if (is_verbose()) std::cout << "Done!" << std::endl;
//
//  Config param_file = model->get_param_file();
//  //param_file.check_pot_file();
//  //std::ofstream outfile;
//  //outfile.open ("pot.tadah");
//  //outfile << param_file << std::endl;;
//
//  //if(train->count("--uncertainty")) {
//  //  t_type weights = model->get_weights();
//  //  t_type unc = model->get_weights_uncertainty();
//  //  Output(param_file,false).print_train_unc(weights, unc);
//  //}
//
//  //if (is_verbose()) std::cout << timer_tot.to_string() << std::endl;
//
  if(model)
    delete model;
  if(fb)
    delete fb;

    blacs_gridexit_(&context);
    //MPI_Win_free(&window);
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
      // Each process will receive an array of integers.
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
