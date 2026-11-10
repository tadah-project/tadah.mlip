#ifndef TADAH_CORE_IUSER_CONFIG_PARSER_H
#define TADAH_CORE_IUSER_CONFIG_PARSER_H

#include <cstddef>
#include <functional>
#include <set>
#include <string>
#include <vector>
#include <tadah/core/context.h>

namespace tadah {
namespace core {

/**
 * @brief A key line that a parse read and did not store, because its
 *        KeyLineRule selected it or because it is a run setting of a
 *        potential (CoreUserConfigParser: run_setting_keys()).
 */
struct SkippedKeyLine {
  /** The file the line is in: as given to the parse, or as the CONFIG line
   *  that includes it names it; empty for lines without a source. */
  std::string source;
  /** Its number (1-based) in @c source. */
  std::size_t line = 0;
  /** The key, upper case. */
  std::string key;
  /** Its values, as written. */
  std::vector<std::string> values;
};

/**
 * @brief Whether a parse skips the line of @p key (upper case) of a file
 *        whose key lines give the keys @p file_keys (upper case).
 *
 * @p file_keys are the keys of the lines of the file that holds the line,
 * a CONFIG line and the name of a block (INIT, OPTIMIZER, ...) among them:
 * not the words inside a block, and not the keys of a file it includes
 * with CONFIG, which is judged by its own lines.
 */
using KeyLineRule = std::function<bool(const std::set<std::string> &file_keys,
                                       const std::string &key)>;

/**
 * @brief Abstract interface for a user configuration parser.
 *
 * Defines methods to parse configuration (a keywords/values file) from various input sources.
 * The parsed key/value pairs are stored in a Context object.
 */
class IUserConfigParser {
public:
  virtual ~IUserConfigParser() {}

  /**
   * @brief Parses a configuration from a file.
   * @param filename The configuration filename.
   * @return A Context holding key/value pairs from the file.
   * @throws std::runtime_error on any parsing or validation error.
   */
  virtual Context parse(const std::string &filename) const = 0;

  /**
   * @brief Parses a configuration from an input stream.
   * @param in The input stream with configuration data.
   * @return A Context holding key/value pairs.
   * @throws std::runtime_error on any parsing or validation error.
   */
  virtual Context parse(std::istream &in) const = 0;

  /**
   * @brief Parses a configuration from a vector of lines.
   * @param lines The vector of strings representing configuration lines.
   * @return A Context holding key/value pairs.
   * @throws std::runtime_error on any parsing or validation error.
   */
  virtual Context parse(const std::vector<std::string>& lines) const = 0;
};

} // namespace core
} // namespace tadah

#endif // TADAH_CORE_IUSER_CONFIG_PARSER_H

