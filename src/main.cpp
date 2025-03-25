#include <tadah.mlip/cli/tadah_cli.h>

/**
 * @brief Main entry point.
 *
 * Instantiates the TadahCLI and parses command-line arguments.
 */
int main(int argc, char** argv) {
  tadah::mlip::cli::TadahCLI cli;
  return cli.run(argc, argv);
}

