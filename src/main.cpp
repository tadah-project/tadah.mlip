#include <tadah.mlip/cli/tadah_cli.h>

#include <exception>
#include <iostream>

#ifdef TADAH_BUILD_MPI
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
#ifdef TADAH_BUILD_MPI
  MPI_Init(&argc, &argv);
#endif

  int ret = 0;
  try {
    tadah::mlip::cli::TadahCLI cli;
    ret = cli.run(argc, argv);
  } catch (const std::exception &e) {
    std::cerr << "ERROR: " << e.what() << std::endl;
    ret = 1;
  }

#ifdef TADAH_BUILD_MPI
  MPI_Finalize();
#endif

  return ret;
}

