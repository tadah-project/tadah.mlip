#ifndef TADAH_MLIP_CLI_UNIFIED_INPUT_H
#define TADAH_MLIP_CLI_UNIFIED_INPUT_H

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace tadah {
namespace mlip {
namespace cli {

/**
 * @brief Provides unified input detection.
 *
 * Determines whether an input string refers to a local file or an online source.
 * If detection fails, an error is raised and a suggestion is provided.
 */
class UnifiedInput {
public:
  /**
   * @brief Processes input strings and returns standardized source strings.
   * @param inputs Vector of input strings.
   * @param forceLocal If true, all inputs will be treated as local files.
   * @param forceOnline If true, all inputs will be treated as online sources.
   * @return Vector of strings prefixed with "local:" or "online:".
   */
  static std::vector<std::string> processInput(const std::vector<std::string> &inputs,
                                                 bool forceLocal = false,
                                                 bool forceOnline = false) {
    std::vector<std::string> sources;
    for (const auto &inp : inputs) {
      if (forceLocal) {
        if (!std::filesystem::exists(inp)) {
          std::cerr << "Error: Forced local input but file '" << inp << "' does not exist." << std::endl;
          std::exit(EXIT_FAILURE);
        }
        sources.push_back("local:" + inp);
      }
      else if (forceOnline) {
        sources.push_back("online:" + inp);
      }
      else {
        if (std::filesystem::exists(inp)) {
          sources.push_back("local:" + inp);
        }
        else if (isOnlineString(inp)) {
          sources.push_back("online:" + inp);
        }
        else {
          std::cerr << "Error: Could not determine input type for '" << inp  
                    << "'. Use --force-local or --force-online to override." << std::endl;
          std::exit(EXIT_FAILURE);
        }
      }
    }
    return sources;
  }
private:
  /**
   * @brief Checks if the string represents an online source.
   * @param str The input string.
   * @return True if string begins with a known online prefix.
   */
  static bool isOnlineString(const std::string &str) {
    return (str.rfind("mp:", 0) == 0 ||
            str.rfind("http://", 0) == 0 ||
            str.rfind("https://", 0) == 0);
  }
};

} // namespace cli
} // namespace mlip
} // namespace tadah

#endif // TADAH_MLIP_CLI_UNIFIED_INPUT_H
