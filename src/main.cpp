#include <tadah.mlip/cli/tadah_cli.h>

#include <exception>
#include <iostream>

#if defined(TADAH_BUILD_MPI) || defined(TADAH_HPO_MPI)
#include <mpi.h>
#endif

/**
 * @brief Main entry point.
 *
 * Instantiates the TadahCLI and parses command-line arguments. Uncaught
 * exceptions from tasks surface as a clean one-line ERROR instead of a
 * std::terminate core dump.
 */
int main(int argc, char** argv) {
#if defined(TADAH_BUILD_MPI)
  MPI_Init(&argc, &argv);
#elif defined(TADAH_HPO_MPI)
  /* The HPO parallel-annealing drivers (TADAH:ANNEAL_PT / ANNEAL_PA)
   * distribute replicas over ranks while every rank's evaluation
   * threads run their own LAMMPS instances on MPI_COMM_SELF -- those
   * instances make MPI calls concurrently, so MPI_THREAD_MULTIPLE is
   * required (Cray MPICH additionally needs
   * `export MPICH_MAX_THREAD_SAFETY=multiple`). */
  {
    int provided = 0;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    if (provided < MPI_THREAD_MULTIPLE) {
      int rank = 0;
      MPI_Comm_rank(MPI_COMM_WORLD, &rank);
      if (rank == 0)
        std::cerr << "ERROR: this MPI library provides thread level "
                  << provided << " < MPI_THREAD_MULTIPLE, which the "
                  << "TADAH_HPO_MPI build requires (concurrent LAMMPS "
                  << "instances on MPI_COMM_SELF). On Cray MPICH set "
                  << "MPICH_MAX_THREAD_SAFETY=multiple; otherwise use an "
                  << "MPI built with full thread support." << std::endl;
      MPI_Finalize();
      return 1;
    }
    /* Some MPI implementations initialise MCA frameworks LAZILY on
     * first use -- OpenMPI's topo framework, for instance, comes up on
     * the first MPI_Cart_create, which LAMMPS calls during domain
     * decomposition. That lazy path is NOT thread-safe even under
     * MPI_THREAD_MULTIPLE, and the parallel annealing drivers create
     * their first LAMMPS instances from concurrent worker threads.
     * Touch it once here, single-threaded. */
    {
      int dims[1] = {1}, periods[1] = {0};
      MPI_Comm cart = MPI_COMM_NULL;
      MPI_Cart_create(MPI_COMM_SELF, 1, dims, periods, 0, &cart);
      if (cart != MPI_COMM_NULL) MPI_Comm_free(&cart);
    }
  }
#endif

  int ret = 0;
  try {
    tadah::mlip::cli::TadahCLI cli;
    ret = cli.run(argc, argv);
  } catch (const std::exception &e) {
    std::cerr << "ERROR: " << e.what() << std::endl;
    ret = 1;
  }

#if defined(TADAH_BUILD_MPI) || defined(TADAH_HPO_MPI)
  MPI_Finalize();
#endif

  return ret;
}

