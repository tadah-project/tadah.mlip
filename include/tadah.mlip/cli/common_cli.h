#ifndef TADAH_MLIP_CLI_COMMON_CLI_H
#define TADAH_MLIP_CLI_COMMON_CLI_H

#include "CLI/CLI.hpp"
#include <string>

namespace tadah {
namespace mlip {
namespace cli {

/**
 * @brief Attaches common options to a CLI subcommand.
 *
 * Attaches standard options such as --verbose and --numeric to reduce redundancy.
 *
 * @param app Pointer to a CLI::App instance.
 * @param verbose Reference to a boolean flag for verbose output.
 * @param numeric_precision Reference to an integer variable for numeric precision.
 */
inline void addCommonOptions(CLI::App* app, bool &verbose, int &numeric_precision) {
    app->add_flag("-V,--verbose", verbose, "Enable verbose output");
    app->add_option("--numeric", numeric_precision, "Decimal output precision");
}

/**
 * @brief Checks for common flag mis-usage and provides suggestions.
 *
 * Scans the command-line arguments for common errors (e.g. using '-v' instead of '-V')
 * and prints an error message with a suggestion.
 *
 * @param argc Number of arguments.
 * @param argv Array of argument strings.
 */
inline void checkFlagSuggestions(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if(arg == "-v" || arg == "--V") {
            std::cerr << "Error: Did you mean '-V, --verbose' instead of '" 
                      << arg << "'?" << std::endl;
            std::exit(EXIT_FAILURE);
        }
    }
}

} // namespace cli
} // namespace mlip
} // namespace tadah

#endif // TADAH_MLIP_CLI_COMMON_CLI_H

