#define CATCH_CONFIG_RUNNER  // the runner initialises MPI in a TADAH_HPO_MPI build
#include "catch2/catch.hpp"

#ifdef TADAH_HPO_MPI
#include <mpi.h>

#include <cstdio>
#endif

int main(int argc, char** argv) {
#ifdef TADAH_HPO_MPI
  /* The MPI build creates every LAMMPS instance on MPI_COMM_SELF, which
   * needs an initialised MPI: tadah's main() initialises it, and so does
   * this runner, as tadah does -- a one-rank job with
   * MPI_THREAD_MULTIPLE, and the topology framework touched once before
   * any thread creates a LAMMPS instance. Without it a case that creates
   * one (the Quick Start's in.ml) aborts ("MPI_Comm_rank() function was
   * called before MPI_INIT"). */
  int provided = 0;
  MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
  if (provided < MPI_THREAD_MULTIPLE) {
    std::fprintf(stderr, "test_tadah: the MPI library provides thread level "
                         "%d < MPI_THREAD_MULTIPLE\n", provided);
    MPI_Finalize();
    return 1;
  }
  {
    int dims[1] = {1}, periods[1] = {0};
    MPI_Comm cart = MPI_COMM_NULL;
    MPI_Cart_create(MPI_COMM_SELF, 1, dims, periods, 0, &cart);
    if (cart != MPI_COMM_NULL) MPI_Comm_free(&cart);
  }
  const int status = Catch::Session().run(argc, argv);
  MPI_Finalize();
  return status;
#else
  return Catch::Session().run(argc, argv);
#endif
}
