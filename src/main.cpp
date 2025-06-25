#include <tadah.mlip/cli/tadah_cli.h>

#ifdef TADAH_BUILD_MPI
#include <mpi.h>
#endif

/**
 * @brief Main entry point.
 *
 * Instantiates the TadahCLI and parses command-line arguments.
 */
int main(int argc, char** argv) {
#ifdef TADAH_BUILD_MPI
  MPI_Init(&argc, &argv);
#endif

  tadah::mlip::cli::TadahCLI cli;
  int ret = cli.run(argc, argv);

#ifdef TADAH_BUILD_MPI
  MPI_Finalize();
#endif

  return ret;
}

