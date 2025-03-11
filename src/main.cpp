#include <tadah/cli/tadah_cli.h>

/**
 * @brief Main entry point.
 *
 * Instantiates the TadahCLI and parses command-line arguments.
 */
int main(int argc, char** argv) {
  tadah::cli::TadahCLI cli;
  return cli.run(argc, argv);
}

