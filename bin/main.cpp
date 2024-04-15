#include "tadah_cli.h"
int main(int argc, char** argv) {

#ifdef TADAH_BUILD_MPI
  MPI_Init(&argc, &argv);
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
    return cli.app.exit(e);
  }

  cli.run(argc,argv);

#ifdef TADAH_BUILD_MPI
  MPI_Finalize();
#endif
  return 0;
}
