
//int main(int argc, char** argv) {
//  return 0;
//}
#include "tadah_cli.h"
int main(int argc, char** argv) {

#ifdef TADAH_BUILD_MPI
    MPI_Init(&argc, &argv);
#endif

    TadahCLI cli;
/*---------------------------------------------------------------------------*/
    CLI11_PARSE(cli.app, argc, argv);
/*---------------------------------------------------------------------------*/
    cli.run(argc,argv);

#ifdef TADAH_BUILD_MPI
    MPI_Finalize();
#endif
    return 0;
}
