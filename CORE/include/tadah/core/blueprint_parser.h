#ifndef TADAH_CORE_BLUEPRINT_PARSER_H
#define TADAH_CORE_BLUEPRINT_PARSER_H

#include <string>
#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <toml.hpp>  // toml11 v4.4.0

namespace tadah {
namespace core {

/**
 * @brief The blueprint field `retired` of a key: a table of three non-empty
 *        strings, why the key was retired, what replaces it and how a value
 *        of it converts, and the optional fields by_deletion and
 *        unconverted.
 *
 * A retired key is accepted in a file so that the potentials that carry it
 * still load: CoreUserConfigParser checks its value against the key's
 * format, records the line on the Context it returns
 * (Context::retired_keys(): never a stored key, never written back) and
 * gives one notice per parse that lists them. Every fit refuses a
 * configuration that records one, with the conversion
 * (retired_keys_refusal(), retired_keys.h); Context::add() refuses the key.
 * A retired key has no default_value and no examples; it may keep a CLI
 * flag, which then gives the conversion.
 */
struct RetiredKey {
  std::string reason;       ///< Why the key was retired.
  std::string replacement;  ///< The key(s) that replace it, or what to use.
  std::string conversion;   ///< How a value of it converts.
  /**
   * The optional field `by_deletion` (true or false; false when absent):
   * deleting a line of the key converts it, whatever the configuration,
   * as nothing replaces the key (UNCERTAINTY, WCOV, WNOISE, ERROR). A
   * fit's refusal (retired_keys_refusal()) marks each such line "converted
   * by deleting the line" and gives the key's reason, which its conversion
   * does not say.
   */
  bool by_deletion = false;
  /**
   * The optional field `unconverted` (true or false; false when absent):
   * no key converts a line of the key, and deleting the line changes the
   * fit (EWEIGHT_TEMP, whose Boltzmann factor no key reproduces). A fit's
   * refusal marks each such line "no key converts it", gives the key's
   * reason and says to delete the line, and that no key reproduces it. A
   * key is not both by_deletion and unconverted.
   */
  bool unconverted = false;
};

/**
 * @brief Structure describing one allowed key as defined in the blueprint.
 *
 * Contains allowed data types, formatting instructions, token count bounds,
 * description, and additional metadata.
 */
struct BlueprintKey {
  std::string keyName;                            ///< Fully qualified key name (e.g., "ALLOWED_KEYS.CONFIG")
  std::vector<std::string> value_type;            ///< List of accepted types (e.g., "STRING", "INT", "DOUBLE", etc.)
  std::vector<std::string> value_format;          ///< Expected token format (e.g., "<file>", "<path>", "N", etc.)
  std::vector<int> value_N;                       ///< Allowed token counts (for scalar or array values)
  std::string description;                        ///< Short description for help
  std::string longDescription;                    ///< Long description for documentation and help
  std::vector<std::string> formatting;            ///< Output formatting options
  std::vector<std::string> examples;              ///< Example usages
  std::vector<std::string> default_value;         ///< Default value (if any)
  std::string cli_long_flag;                      ///< CLI long flag/option (if applicable)
  std::string cli_short_flag;                     ///< CLI short flag/option (if applicable)
  /// The options of the same command this option excludes, each named by
  /// its long flag without "--" (BlueprintParser::checkCommandOptions).
  std::vector<std::string> cli_excludes;
  /// The options of the same command this option needs, named as in
  /// cli_excludes.
  std::vector<std::string> cli_needs;
  bool cli_required;                              ///< Indicates if required in a cli context
  /**
   * Non-empty for a deprecated key: why it is deprecated and what replaces
   * it (blueprint field `deprecated`). A configuration or potential file may
   * still contain the key; its value is checked against the key's format,
   * then CoreUserConfigParser writes a notice to standard error once per
   * parse and does not store it.
   * A deprecated key has no default_value (Context::contains() would insert
   * it), no CLI flag and no examples.
   */
  std::string deprecated;
  /**
   * The only values a STRING key takes (blueprint field `allowed_values`),
   * matched without regard to case; empty for any value of its type.
   * CoreUserConfigParser rejects any other value at the line that sets it
   * ("Invalid value for key K on line N of <file>: 'v' (allowed: A, B)";
   * without "of <file>" for lines that come from no file), and
   * validateWithBlueprint -- the command-line path -- refuses it too. The
   * key's default_value and examples must be allowed values.
   */
  std::vector<std::string> allowed_values;
  /**
   * True for a key whose numbers a potential file records exactly whatever
   * NUMERIC is (blueprint field `exact_in_potential`): a load checks them
   * exactly (the cutoffs RCUT2B/RCUTMB, which D2_EAM/DM_EAM compare with
   * their setfl file and the knot families with their largest knot, and
   * the radial grids CGRID2B, CGRIDMB, SGRID2B and SGRIDMB those checks
   * relate them to).
   * Context::write_potential writes each non-integer decimal literal of
   * such a key with NUMERIC significant digits at NUMERIC 17 and above
   * (exact), and below 17 as the shortest text that reads back as the same
   * double (format_roundtrip()). The field must be a boolean, and a key that
   * sets it true must have a DOUBLE position in its value_type.
   */
  bool exact_in_potential = false;

  /**
   * True for a key whose STRING values are names (blueprint field `names`:
   * element symbols, descriptor, cutoff and model names), which hold no
   * comma and no '+'. CoreUserConfigParser::validateSchemaOnly (and so the
   * parser and TaskFactory) refuses a value of such a key that holds a
   * comma, or a '+' joined to the character before it, as a separator typo
   * (ATOM Ta,Nb); the STRING values of other keys keep them (a path, an
   * option string).
   */
  bool names = false;

  /**
   * True for a key that only the commands whose [TASKS.*] table lists it
   * read (blueprint field `command_only`): an option of their command line
   * (ANALYTICS of tadah predict, INDEX of tadah data print) or a key of
   * their configuration file (REFIT_MODE of tadah refit), never a key of a
   * model, a potential or a training configuration. CoreUserConfigParser
   * refuses its line in a configuration file of another command
   * (CoreUserConfigParser::set_command) and does not read it in a potential
   * (a file whose lines give WEIGHTS), with a notice. The field must be a
   * boolean.
   */
  bool command_only = false;

  /**
   * The blueprint field `retired` (RetiredKey); its strings are empty for a
   * key that is not retired.
   */
  RetiredKey retired;

  BlueprintKey() : cli_required(false) {}

  /// Whether `value` is one of allowed_values (without regard to case), or
  /// the key has no list.
  bool isAllowedValue(const std::string &value) const;

  /// True if the blueprint marks this key as deprecated.
  bool isDeprecated() const { return !deprecated.empty(); }

  /// True if the blueprint marks this key as retired (field `retired`).
  bool isRetired() const { return !retired.reason.empty(); }
};
/**
 * @brief An option group of a command ([TASKS.<command>.groups.<name>]):
 *        its fields are description, required_min, required_max and keys,
 *        and a group with another field is refused when the blueprint
 *        loads (the excludes and needs of the command line belong to an
 *        option).
 */
struct BlueprintKeyGroup {
  std::string groupName;                          ///< Name of the group 
  std::vector<std::string> description;           ///< Description of the group
  std::vector<BlueprintKey> blueprintKeys;        ///< BlueprintKey objects in this group
  size_t requiredMin;                             ///< Minimum number of required keys which must be selected
  size_t requiredMax;                             ///< Maximum number of required keys which must be selected
  BlueprintKeyGroup() : requiredMin(1), requiredMax(1) {}
};

struct BlueprintTask {
  std::string taskName;                     ///< The task's name, e.g., "TASK_A".
  std::string description;                  ///< Short description (mandatory).
  std::string longDescription;              ///< Extended description (mandatory).
  std::vector<std::string> examples;        ///< Usage examples (mandatory).
  std::vector<BlueprintKey> blueprintKeys;  ///< Multiple key-value pairs (optional).
  std::vector<BlueprintKeyGroup> groups;    ///< Key groups (optional).

  BlueprintTask() {}
};

/**
 * @brief The table [TADAH]: what `tadah` alone prints (-h, --version). It
 *        has no keys table: tadah alone takes no option but -h and
 *        --version, and every other option belongs to a command.
 */
struct BlueprintTadah: public BlueprintTask {
  std::string logo;                         ///< Name of the Tadah! software.
  std::string version;                      ///< Version of the Tadah! software.
  std::string website;                      ///< Version of the Tadah! software.
};

/**
 * @brief Provides access to the static blueprint map.
 *
 * This module builds the blueprint map only once using the hardcoded blueprint file,
 * and subsequent calls return either a const reference (to avoid copying) or a copy.
 *
 * The blueprint is parsed from a TOML file and processed for any inheritance blocks.
 *
 * Usage example:
 * @code
 *   const auto &blueprint = tadah::core::BlueprintParser::getKeysMapRef();
 *   // Use the blueprint for further processing.
 * @endcode
 */
class BlueprintParser {
public:
  /**
   * @brief Retrieves a const reference to the statically cached ALLOWED_KEYS blueprint map.
   *
   * Access does not cause a copy and the map is built on first invocation.
   *
   * @return Const reference to the unordered_map mapping blueprint keys to BlueprintKey objects.
   * @throws std::runtime_error If blueprint parsing or inheritance expansion fails.
   */
  static const std::unordered_map<std::string, BlueprintKey>& getKeysMapRef();

  /**
   * @brief Retrieves a const reference to the statically cached TASKS blueprint map.
   *
   * Access does not cause a copy and the map is built on first invocation.
   *
   * @return Const reference to the unordered_map mapping task names to BlueprintKey objects.
   * @throws std::runtime_error If blueprint parsing or inheritance expansion fails.
   */
  static const std::unordered_map<std::string, BlueprintTask>& getTasksMapRef();

  /**
   * @brief Retrieves a const reference to the statically cached INTERNAL_KEYS blueprint map.
   *
   * Access does not cause a copy and the map is built on first invocation.
   *
   * @return Const reference to the unordered_map mapping internal keys to BlueprintKey objects.
   * @throws std::runtime_error If blueprint parsing or inheritance expansion fails.
   */
  static const std::unordered_map<std::string, BlueprintKey>& getInternalKeysMapRef();

  /**
   * @brief Retrieves a copy of the statically cached ALLOWED_KEYS blueprint map.
   *
   * @return A copy of the unordered_map mapping blueprint keys to BlueprintKey objects.
   * @throws std::runtime_error If blueprint parsing or inheritance expansion fails.
   */
  static std::unordered_map<std::string, BlueprintKey> getKeysMap();

  /**
   * @brief Retrieves a copy of the statically cached TASKS blueprint map.
   *
   * @return A copy of the unordered_map mapping blueprint keys to BlueprintKey objects.
   * @throws std::runtime_error If blueprint parsing or inheritance expansion fails.
   */
  static std::unordered_map<std::string, BlueprintTask> getTasksMap();

  /**
   * @brief Retrieves a copy of the statically cached blueprint map.
   *
   * @return A copy of the BlueprintTadah object containing the blueprint data.
   * @throws std::runtime_error If blueprint parsing or inheritance expansion fails.
   */
  static BlueprintTadah getBlueprintTadah();

  /**
   * @brief Retrieves a const reference to the statically cached blueprint map.
   *
   * Access does not cause a copy and the map is built on first invocation.
   *
   * @return Const reference to the BlueprintTadah object containing the blueprint data.
   * @throws std::runtime_error If blueprint parsing or inheritance expansion fails.
   */
  static const BlueprintTadah &getBlueprintTadahRef();

  /**
   * @brief Retrieves a copy of the statically cached INTERNAL_KEYS blueprint map.
   *
   * @return A copy of the unordered_map mapping blueprint keys to BlueprintKey objects.
   * @throws std::runtime_error If blueprint parsing or inheritance expansion fails.
   */
  static std::unordered_map<std::string, BlueprintKey> getInternalKeysMap();

  /**
   * @brief Builds the commands of a parsed blueprint, its [TASKS] table,
   *        as the tasks map does for the embedded blueprint.
   *
   * Expands the INHERIT references of @p blueprint, builds a BlueprintTask
   * for every table of [TASKS] that has a description, a long_description
   * and examples (nested tables are commands named with dots,
   * "PROPERTIES.PAIRWISE"), and checks the options of every command
   * (checkCommandOptions). getTasksMapRef() is this function applied to the
   * embedded blueprint; it is public so that the rules of the load can be
   * checked on a blueprint of their own.
   *
   * @param blueprint The parsed blueprint (toml::parse).
   * @return The commands by their dot-separated, uppercase names.
   * @throws std::runtime_error "Error building blueprint tasks map: ..."
   *         with the reason: a command missing a mandatory field, a key
   *         that buildKey refuses, an option group with a field other
   *         than description, required_min, required_max and keys, an
   *         option whose cli_excludes or cli_needs names no other option
   *         of its command.
   */
  static std::unordered_map<std::string, BlueprintTask>
  buildTasksMap(const toml::value &blueprint);

  /**
   * @brief Builds the [TADAH] table of a parsed blueprint, as
   *        getBlueprintTadahRef() does for the embedded blueprint.
   *
   * The fields logo, version, website, description, long_description and
   * examples are mandatory. [TADAH] has no keys table: tadah alone takes
   * no option but -h and --version, and an option belongs to a command.
   * Public so that the rules of the load can be checked on a blueprint of
   * their own.
   *
   * @param blueprint The parsed blueprint (toml::parse).
   * @throws std::runtime_error for a missing field, and for a keys table
   *         ("[TADAH] must not have a keys table: ...").
   */
  static BlueprintTadah buildBlueprintTadah(const toml::value &blueprint);

  /**
   * @brief Builds one BlueprintKey from its (inheritance-expanded) TOML table.
   *
   * This is the constructor used for every key of the embedded blueprint;
   * it is public so that the per-key rules can be checked directly.
   * value_type, value_format and value_N are mandatory. A `deprecated`
   * field must be a non-empty string, and a deprecated key must define
   * neither `default_value`, a CLI flag nor examples (after inheritance:
   * `examples = []` overrides those of a TYPES table). A `retired` field
   * must be a table of three non-empty strings, `reason`, `replacement`
   * and `conversion`, and optionally `by_deletion` and `unconverted`, each
   * true or false and not both true (RetiredKey), and of no other field; a retired key must define neither
   * `default_value` nor examples, may keep a CLI flag, and is not
   * deprecated as well.
   *
   * @param fullKey Fully qualified key name (used in error messages).
   * @param table TOML table of the key.
   * @throws std::runtime_error If a mandatory field is missing or a rule is
   *         violated.
   */
  static BlueprintKey buildKey(const std::string &fullKey,
                               const toml::value &table);

  /**
   * @brief Checks the command-line fields of the options of the command
   *        @p task (its own options and those of its groups).
   *
   * Each name in the cli_excludes and cli_needs of an option is the long
   * flag, without "--", of another option of the same command: the command
   * line excludes or needs exactly that option, and a name that names none
   * would leave the rule without effect. buildTasksMap() applies it to
   * every command of a blueprint it loads.
   *
   * @throws std::runtime_error naming the command, the option, the field
   *         and the name, for the first name that names no other option of
   *         the command.
   */
  static void checkCommandOptions(const BlueprintTask &task);

  /**
 * @brief Computes the closest matching key name by measuring the edit distance to each valid blueprint key.
 *
 * The method calculates the Levenshtein distance between the provided key and each key in the map, returning
 * the one with the smallest distance. Deprecated and retired keys are skipped. This behavior helps guide users toward valid keys when typos or
 * similar mistakes are made in configuration files or command-line input.
 *
 * @param key The user-provided (potentially erroneous) key.
 * @param map The map of valid key names associated with BlueprintKey objects.
 * @return The key name with the smallest edit distance, or an empty string if the map holds no
 *         key that is neither deprecated nor retired.
 *
 * Example usage:
 * @code
 *   std::unordered_map<std::string, BlueprintKey> blueprintMap = {{"ALLOWED_KEYS", {}}};
 *   std::string closest = BlueprintParser::findClosestKey("ALOWED_KEYS", blueprintMap);
 *   // 'closest' would likely be "ALLOWED_KEYS"
 * @endcode
 */
  static std::string findClosestKey(const std::string &key,
      const std::unordered_map<std::string, BlueprintKey> &map);
};

/**
 * @brief The commands that read the key @p key (upper case): those whose
 *        [TASKS.*] table lists it among its options or the keys of its
 *        option groups, as typed ("tadah predict", "tadah data print"),
 *        sorted. A command whose table retires the key (BlueprintKey::
 *        isRetired(): an option it refuses with the reason, such as
 *        --rescale of tadah data balance) does not read it.
 */
std::vector<std::string> commands_reading_key(const std::string &key);

/**
 * @brief The command line of the blueprint task @p task as typed: "tadah
 *        train" for "TRAIN", "tadah data print" for "DATA.print".
 */
std::string task_command_words(const std::string &task);

/**
 * @brief What the key @p key (upper case) of BlueprintKey::command_only is,
 *        for a message: "the option --analytics (-A) of tadah predict", "a
 *        key of tadah refit", "the option --index of tadah analysis bfunc,
 *        ... and tadah data write", "the argument of tadah explain" (a
 *        cli_long_flag that does not start with '-' names a positional
 *        argument), or "read by no tadah command".
 */
std::string command_key_role(const std::string &key);

/**
 * @brief command_key_role() after the key: "ANALYTICS is the option
 *        --analytics (-A) of tadah predict".
 */
std::string command_key_text(const std::string &key);

/**
 * @brief The command-line option of the key @p key (upper case), for a
 *        message: "--outfile (-o)", "--type"; empty for a key without a
 *        "--" flag.
 */
std::string option_text(const std::string &key);

/**
 * @brief True when the option -c (--config) of the blueprint task @p task
 *        needs the option of the key @p key (upper case) on the command
 *        line (the cli_needs of the task's CONFIG key name it: OUTFILE of
 *        tadah analysis dataset_stats, RANGE and TYPE of tadah analysis
 *        bfunc). The command line then always gives the key with -c, and
 *        its value replaces a line of the key in the configuration file.
 */
bool config_needs_option(const std::string &task, const std::string &key);

} // namespace core
} // namespace tadah

#endif // TADAH_CORE_BLUEPRINT_PARSER_H
