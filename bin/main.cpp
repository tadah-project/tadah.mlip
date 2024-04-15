#include "tadah_cli.h"
int main(int argc, char** argv) {

  int rank=0;

#ifdef TADAH_BUILD_MPI
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
#endif

  TadahCLI cli;
  /*---------------------------------------------------------------------------*/
  //CLI11_PARSE(cli.app, argc, argv);
  /*---------------------------------------------------------------------------*/
  try {
    cli.app.parse(argc, argv);
  } catch (const CLI::ParseError &e) {
#ifdef TADAH_BUILD_MPI
    MPI_Finalize();
#endif
    // exit all but rank 0 normally
    if (rank==0) {
      return cli.app.exit(e);
    }
    else {
      return 0;
    }
  }

  if (cli.app.count_all()==1) {
    // no argument given to tadah 
    // print help message by rank 0 only
    if (rank==0) {
      cli.run(argc,argv);
    }
  }
  else {
    cli.run(argc,argv);
  }

#ifdef TADAH_BUILD_MPI
  MPI_Finalize();
#endif
  return 0;
}
