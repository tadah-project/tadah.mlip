#include "tadah_cli.h"

int main(int argc, char** argv) {

  int rank=0;
  int retval=0;

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
  } 
  catch (const CLI::CallForHelp& e) {
    std::cout << cli.app.help() << std::endl;
    return 0;
  } 
  catch (const CLI::ParseError &e) {
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

  retval = cli.run(argc,argv);

#ifdef TADAH_BUILD_MPI
  MPI_Finalize();
#endif
  return retval;
}
