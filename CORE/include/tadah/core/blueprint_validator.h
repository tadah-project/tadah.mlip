#ifndef TADAH_CORE_BLUEPRINT_VALIDATOR_H
#define TADAH_CORE_BLUEPRINT_VALIDATOR_H

/**
 * @file blueprint_validator.h
 * @brief Public API for compiling and matching strings against
 *        blueprint patterns (with optional tokens, repeated groups, etc.).
 *
 * @details
 * This header places only the essential interface in the tadah::core namespace.
 * Implementation details reside in "blueprint_validator.cpp".
 *
 * Usage example:
 * @code
 *   #include <tadah/core/blueprint_validator.h>
 *   using namespace tadah::core;
 *
 *   BlueprintValidator validator("INT [DOUBLE] {STRING}");
 *   bool matched = validator.validate("10 3.14 hello");
 *   auto captures = validator.getCaptures("10 3.14 hello");
 *   // captures[0] -> "10", captures[1] -> "3.14", captures[2] -> "hello"
 *
 *   // For single checks:
 *   bool singleOk = BlueprintValidator::validateOnce("INT [DOUBLE]", "10 3.14");
 * @endcode
 */

#include <string>
#include <vector>

namespace tadah {
namespace core {

/**
 * @class BlueprintValidator
 * @brief Verifies if a given input matches a blueprint, and retrieves token
 *        captures.
 *
 * validate() splits the text into words at blanks, as a configuration line
 * is split into values, and matches the words against the blueprint: a
 * comma or a '+' between two values is not a separator (a STRING word keeps
 * them; an INDEX_PATTERN list takes commas, and blanks around its ',', '-'
 * and ':'). A line of any length is decided in time linear in its number of
 * words, without std::regex. getPattern() and getCaptures() use the
 * blueprint's anchored regex (std::regex), which takes the same lines but
 * may backtrack for long before it refuses one and overflows the stack on a
 * long one.
 */
class BlueprintValidator {
public:
  /**
   * @brief Construct a validator for the given blueprint.
   * @param blueprint A blueprint such as "BOOL INT {INT} [DOUBLE] DOUBLE DOUBLE".
   *
   * Example usage:
   * @code
   *  BlueprintValidator v("INT [DOUBLE] {STRING}");
   *  bool ok = v.validate("10 3.14 Hello");
   * @endcode
   */
  explicit BlueprintValidator(const std::string &blueprint);

  /** @brief Destructor releases the compiled blueprints. */
  ~BlueprintValidator();

  /**
   * @brief Construct a validator for the given set of blueprint.
   * @param blueprints A vector of blueprints to validate against.
   */
  explicit BlueprintValidator(const std::vector<std::string> &blueprints);

  /**
   * @brief Checks if the given text fully matches the blueprint.
   *
   * If this object was constructed with multiple blueprints, all are tested.
   *
   * @param text The input string to test.
   * @return True if any bluprint matched, false otherwise.
   */
  bool validate(const std::string &text) const;

  /**
   * @brief Checks if the given list of tokens concatenated by a space match the blueprint.
   *
   * The tokens are concatenated with a single space delimiter and then validated against
   * the compiled blueprint pattern.
   *
   * @param tokens The vector of string tokens.
   * @return True if the concatenated tokens form a valid match, false otherwise.
   */
  bool validate(const std::vector<std::string> &tokens) const;

  /**
   * @brief Retrieves captured groups from a matching string, in order.
   * @param text The string to match.
   * @return A vector of captured substrings; empty if not matched.
   */
  std::vector<std::string> getCaptures(const std::string &text) const;

  /**
   * @brief Convenience method to validate text once, without storing a validator object.
   * @param blueprint The blueprint string.
   * @param text The input string to test.
   * @return True if matched, false otherwise.
   */
  static bool validateOnce(const std::string &blueprint, const std::string &text);

  /**
   * @brief Convenience method to validate text once, without storing a validator object.
   * @param blueprints The vector containing blueprints.
   * @param text The input string to test.
   * @return True if any matched, false otherwise.
   */
  static bool validateOnce(const std::vector<std::string> &blueprints, const std::string &text);

  /**
   * @brief Returns the final anchored regex pattern used to match the blueprint.
   * @return The pattern string (e.g., "^([-+]?\\d+)(?:\\s+([-+]?\\d+))?...$").
   */
  std::string getPattern() const;

private:
  std::string m_patterns;  ///< Holds the final anchored pattern.
  // Implementation detail: store the compiled regex as well.
  // <regex> is kept out of this header, so the regex is hidden behind a pointer.
  // The actual object is defined in the .cpp.
  class Impl;             ///< Forward declaration for pImpl idiom
  Impl* m_impl;           ///< Private pointer to the compiled regex and helper data.
};

} // namespace core
} // namespace tadah


#endif // TADAH_CORE_BLUEPRINT_VALIDATOR_H
