#include <tadah/core/blueprint_parser.h>
#include <tadah/core/logger.h>
#include <tadah/core/utils.h>
#include <algorithm>
#include <cctype>
#include <iostream>
#include <boost/algorithm/string.hpp>
#include <blueprint_tadah.hpp>


namespace tadah {
namespace core {

namespace { // Anonymous namespace for internal helper functions
#define BLUEPRINT_ORIGIN "embedded blueprint_tadah.toml"
/**
 * @brief Recursively expands any INHERIT blocks in a TOML table.
 *
 * Expands inheritance by merging keys from inherited tables (referenced by the
 * "INHERIT" key) into the current table. Processes subtables recursively.
 * Handles nested inheritance by recursively expanding each inherited table
 * first, ensuring all inheritance chains are processed.
 *
 * @param table TOML table to process (modified in place).
 * @param root The root TOML value used for lookup of inherited tables.
 * @param parentScope Dot-separated path representing the current scope
 * (primarily for diagnostics).
 * @throws std::runtime_error If any errors occur during inheritance expansion.
 *
 * Example usage:
 * @code
 * toml::value config = toml::parse("input.toml");
 * expandInheritance(config, config);
 * // config now has all keys from inherited sections recursively merged
 * @endcode
 */
void expandInheritance(toml::value &table, const toml::value &root,
                       const std::string &parentScope = "") {
  // Process only if the current node is a table
  if (!table.is_table()) {
    return;
  }

  // Process all sub-keys recursively first, so they are fully expanded.
  for (auto &kv : table.as_table()) {
    if (kv.second.is_table()) {
      std::string newScope =
          parentScope.empty() ? kv.first : (parentScope + "." + kv.first);
      expandInheritance(kv.second, root, newScope);
    }
  }

  // Check if the current table has an INHERIT key
  if (table.contains("INHERIT")) {
    try {
      // Retrieve all mentioned paths in the INHERIT array
      auto inheritList = toml::find<std::vector<std::string>>(table, "INHERIT");

      // For each path, locate the base table in 'root' and merge it into
      // 'table'
      for (const std::string &inheritPath : inheritList) {
        // Split the dot-separated path into individual parts
        std::vector<std::string> parts;
        boost::split(parts, inheritPath, boost::is_any_of("."));

        // Traverse the root using the path parts
        const toml::value *curr = &root;
        for (const std::string &part : parts) {
          if (!curr->is_table()) {
            throw std::runtime_error("Invalid inheritance path: " +
                                     inheritPath);
          }
          curr = &toml::find(*curr, part);
        }
        if (!curr->is_table()) {
          throw std::runtime_error("Inheritance source is not a table: " +
                                   inheritPath);
        }

        // Make a local copy of the inherited table so it can be expanded
        // recursively
        toml::value expandedBase = *curr;
        expandInheritance(expandedBase, root, inheritPath);

        // Merge the fully expanded base table into the current table
        auto &baseTable = expandedBase.as_table();
        auto &destTable = table.as_table();
        for (auto &basePair : baseTable) {
          if (destTable.find(basePair.first) == destTable.end()) {
            destTable[basePair.first] = basePair.second;
          }
        }
      }
    } catch (const std::exception &ex) {
      throw std::runtime_error("Error during inheritance expansion: " +
                               std::string(ex.what()));
    }

    // Remove the INHERIT key so it is not merged again
    table.as_table().erase("INHERIT");
  }
}


/**
 * @brief Constructs a BlueprintKey from a TOML table.
 *
 * Validates and converts the given TOML subtable into a BlueprintKey
 * structure.
 *
 * value_type, value_format, and value_N are mandatory fields. A runtime_error
 * is thrown if any are missing. All other fields are optional and, if missing,
 * are either set to a default value or generate a warning.
 *
 * Example usage:
 * @code
 * toml::value table = toml::parse("some_file.toml");
 * BlueprintKey item = buildBlueprintKey("ALLOW_SOMETHING",
 * table["ALLOW_SOMETHING"]);
 * @endcode
 *
 * @param fullKey The fully qualified key name.
 * @param table Reference to the TOML subtable for the blueprint item.
 * @return A BlueprintKey constructed from the given TOML table.
 * @throws std::runtime_error If any mandatory field (value_type, value_format,
 * or value_N) is missing.
 */
BlueprintKey buildBlueprintKey(const std::string &fullKey,
                                 const toml::value &table) {
  BlueprintKey item;
  item.keyName = fullKey;

  // Throws an exception if 'value_type' is not found
  if (!table.contains("value_type")) {
    throw std::runtime_error("Missing mandatory field 'value_type' in key: " +
                             fullKey);
  }
  // item.value_type = toml::find<std::vector<std::string>>(table, "value_type");
  {
    // Extract value_type[] safely
    auto const &node = table.at("value_type");
    if (!node.is_array()) {
      throw std::runtime_error("value_type must be an array of strings in key: " + fullKey);
    }
    auto const &arr = node.as_array();
    item.value_type.clear();
    item.value_type.reserve(arr.size());
    for (auto const &e : arr) {
      if (!e.is_string()) {
        throw std::runtime_error("Non-string element in value_type of key: " + fullKey);
      }
      item.value_type.push_back(e.as_string());
    }
  }

  // Throws an exception if 'value_format' is not found
  if (!table.contains("value_format")) {
    throw std::runtime_error("Missing mandatory field 'value_format' in key: " +
                             fullKey);
  }
  //item.value_format = toml::find<std::vector<std::string>>(table, "value_format");
    {
    // Extract value_format[] safely
    auto const &node = table.at("value_format");
    if (!node.is_array()) {
      throw std::runtime_error("value_format must be an array of strings in key: " + fullKey);
    }
    auto const &arr = node.as_array();
    item.value_format.clear();
    item.value_format.reserve(arr.size());
    for (auto const &e : arr) {
      if (!e.is_string()) {
        throw std::runtime_error("Non-string element in value_format of key: " + fullKey);
      }
      item.value_format.push_back(e.as_string());
    }
  }

  // Throws an exception if 'value_N' is not found
  if (!table.contains("value_N")) {
    throw std::runtime_error("Missing mandatory field 'value_N' in key: " +
                             fullKey);
  }
  // item.value_N = toml::find<std::vector<int>>(table, "value_N");
  {
    // Extract value_N[] safely
    auto const &node = table.at("value_N");
    if (!node.is_array()) {
      throw std::runtime_error("value_N must be an array of integers in key: " + fullKey);
    }
    auto const &arr = node.as_array();
    item.value_N.clear();
    item.value_N.reserve(arr.size());
    for (auto const &e : arr) {
      if (!e.is_integer()) {
        throw std::runtime_error("Non-integer element in value_N of key: " + fullKey);
      }
      item.value_N.push_back(static_cast<int>(e.as_integer()));
    }
  }

  // Warns if optional fields are missing
  try {
    item.description = toml::find<std::string>(table, "description");
    item.examples = toml::find<std::vector<std::string>>(table, "examples");
  } catch (const std::exception &ex) {
    Logger::instance().log(LogLevel::WARNING,
                           "Missing optional fields for key " + fullKey + ": " +
                               std::string(ex.what()));
  }
  // quietly ignore below
  try {
    item.longDescription = toml::find<std::string>(table, "long_description");
  } catch (const std::exception &ex) {
    // Ignore the error
  }
  try {
    item.formatting = toml::find<std::vector<std::string>>(table, "formatting");
  } catch (const std::exception &ex) {
    // Ignore the error
  }

  // Handles other optional fields
  if (table.contains("default_value")) {
    item.default_value = toml::find<std::vector<std::string>>(table, "default_value");
  }
  if (table.contains("cli_long_flag")) {
    item.cli_long_flag = toml::find<std::string>(table, "cli_long_flag");
  }
  if (table.contains("cli_short_flag")) {
    item.cli_short_flag = toml::find<std::string>(table, "cli_short_flag");
  }
  if (table.contains("cli_excludes")) {
    item.cli_excludes =
        toml::find<std::vector<std::string>>(table, "cli_excludes");
  }
  if (table.contains("cli_needs")) {
    item.cli_needs =
        toml::find<std::vector<std::string>>(table, "cli_needs");
  }
  if (table.contains("cli_required")) {
    item.cli_required = toml::find<bool>(table, "cli_required");
  } else {
    item.cli_required = false;
  }

  // Deprecated keys: still accepted in files, warned about and ignored by
  // CoreUserConfigParser. A default would be inserted by Context::contains()
  // and so reappear in every potential written afterwards, and a CLI flag
  // would offer the key to new users.
  if (table.contains("deprecated")) {
    const toml::value &node = table.at("deprecated");
    const std::string reason =
        node.is_string() ? toml::get<std::string>(node) : std::string();
    if (reason.find_first_not_of(" \t\r\n") == std::string::npos) {
      throw std::runtime_error(
          "'deprecated' must be a non-empty string (the reason and what "
          "replaces the key) in key: " + fullKey);
    }
    item.deprecated = reason;
    if (!item.default_value.empty()) {
      throw std::runtime_error("Deprecated key " + fullKey +
                               " must not define default_value");
    }
    if (!item.cli_long_flag.empty() || !item.cli_short_flag.empty()) {
      throw std::runtime_error("Deprecated key " + fullKey +
                               " must not define a CLI flag");
    }
    // Examples (the config-keys page, the invalid-value error) would show
    // how to use a key that files should drop. `examples = []` also
    // overrides the examples of an inherited TYPES table.
    if (!item.examples.empty()) {
      throw std::runtime_error("Deprecated key " + fullKey +
                               " must not define examples (set examples = [])");
    }
  }

  // Retired keys: still accepted in files, recorded on the parse result and
  // refused by every fit with the conversion (retired_keys.h). A default
  // would be inserted by Context::contains() and written into every
  // potential, and examples would show how to use the key; a CLI flag stays
  // to give the conversion.
  if (table.contains("retired")) {
    const toml::value &node = table.at("retired");
    const std::string fields =
        "'retired' must be a table of three non-empty strings, reason, "
        "replacement and conversion, and optionally by_deletion and "
        "unconverted, each true or false and not both true, in key: " +
        fullKey;
    const bool del = node.is_table() && node.contains("by_deletion");
    const bool unc = node.is_table() && node.contains("unconverted");
    if (!node.is_table() ||
        node.as_table().size() != 3u + (del ? 1u : 0u) + (unc ? 1u : 0u))
      throw std::runtime_error(fields);
    auto field = [&](const char *name) {
      if (!node.contains(name) || !node.at(name).is_string())
        throw std::runtime_error(fields);
      const std::string s = toml::get<std::string>(node.at(name));
      if (s.find_first_not_of(" \t\r\n") == std::string::npos)
        throw std::runtime_error(fields);
      return s;
    };
    item.retired.reason = field("reason");
    item.retired.replacement = field("replacement");
    item.retired.conversion = field("conversion");
    // Deleting a line of the key converts it (retired_keys_refusal marks
    // the line and gives the reason).
    if (del) {
      if (!node.at("by_deletion").is_boolean())
        throw std::runtime_error(fields);
      item.retired.by_deletion = node.at("by_deletion").as_boolean();
    }
    // No key converts a line of the key, and deleting it changes the fit
    // (retired_keys_refusal marks the line so): not a deletion that
    // converts it as well.
    if (unc) {
      if (!node.at("unconverted").is_boolean())
        throw std::runtime_error(fields);
      item.retired.unconverted = node.at("unconverted").as_boolean();
      if (item.retired.unconverted && item.retired.by_deletion)
        throw std::runtime_error(fields);
    }
    if (!item.deprecated.empty())
      throw std::runtime_error("Key " + fullKey + " is both deprecated and "
                               "retired: a key is one or the other");
    if (!item.default_value.empty())
      throw std::runtime_error("Retired key " + fullKey +
                               " must not define default_value");
    if (!item.examples.empty())
      throw std::runtime_error("Retired key " + fullKey +
                               " must not define examples (set examples = [])");
  }

  // Names: the key's STRING values are names, which hold no ',' or '+'.
  if (table.contains("names")) {
    const toml::value &node = table.at("names");
    if (!node.is_boolean()) {
      throw std::runtime_error("'names' must be true or false in key: " +
                               fullKey);
    }
    item.names = node.as_boolean();
    if (item.names &&
        std::none_of(item.value_type.begin(), item.value_type.end(),
                     [](const std::string &t) {
                       return t.find("STRING") != std::string::npos;
                     })) {
      throw std::runtime_error(
          "'names' needs a key whose value_type has STRING values: " +
          fullKey);
    }
  }

  // Allowed values: the only values of a single-string key. The default
  // and the examples are values users copy, so they must be allowed ones.
  if (table.contains("allowed_values")) {
    const toml::value &node = table.at("allowed_values");
    std::vector<std::string> values;
    bool ok = node.is_array() && !node.as_array().empty();
    if (ok) {
      for (const auto &e : node.as_array()) {
        if (!e.is_string() ||
            toml::get<std::string>(e).find_first_not_of(" \t\r\n") ==
                std::string::npos) {
          ok = false;
          break;
        }
        values.push_back(toml::get<std::string>(e));
      }
    }
    if (!ok) {
      throw std::runtime_error(
          "'allowed_values' must be a non-empty list of non-empty strings "
          "in key: " + fullKey);
    }
    if (item.value_type != std::vector<std::string>{"STRING"} ||
        item.value_N != std::vector<int>{1}) {
      throw std::runtime_error(
          "'allowed_values' needs a key of value_type STRING with one value "
          "(value_N = [1]): " + fullKey);
    }
    item.allowed_values = values;
    for (const auto &v : item.default_value) {
      if (!item.isAllowedValue(v)) {
        throw std::runtime_error("default_value '" + v + "' of key " +
                                 fullKey + " is not in its allowed_values");
      }
    }
    for (const auto &v : item.examples) {
      if (!item.isAllowedValue(v)) {
        throw std::runtime_error("examples entry '" + v + "' of key " +
                                 fullKey + " is not in its allowed_values");
      }
    }
  }

  // A key only the commands that list it read (an option of theirs, or a
  // key of their configuration file): refused in a configuration file of
  // another command and not read from a potential (CoreUserConfigParser).
  if (table.contains("command_only")) {
    const toml::value &node = table.at("command_only");
    if (!node.is_boolean()) {
      throw std::runtime_error(
          "'command_only' must be true or false in key: " + fullKey);
    }
    item.command_only = node.as_boolean();
  }

  // Written exactly in a potential whatever NUMERIC is: governs the
  // non-integer numbers Context::write_potential writes, so it needs a
  // DOUBLE position (on another key it would govern nothing).
  if (table.contains("exact_in_potential")) {
    const toml::value &node = table.at("exact_in_potential");
    if (!node.is_boolean()) {
      throw std::runtime_error(
          "'exact_in_potential' must be true or false in key: " + fullKey);
    }
    item.exact_in_potential = node.as_boolean();
    bool has_double = false;
    for (const auto &vt : item.value_type)
      if (vt.find("DOUBLE") != std::string::npos) has_double = true;
    if (item.exact_in_potential && !has_double) {
      throw std::runtime_error(
          fullKey + ": exact_in_potential = true needs a DOUBLE position in "
          "value_type (it governs how a potential file writes the key's "
          "non-integer numbers, and " + fullKey + " has none)");
    }
  }

  return item;
}

/**
 * @brief Builds the blueprint map from the TOML blueprint file.
 *
 * Parses and processes the TOML configuration file, applying inheritance
 * expansion, and then extracts the table to build the mapping of blueprint keys
 * to BlueprintKey objects.
 *
 * @return An unordered_map mapping blueprint key names to BlueprintKey
 * objects.
 * @throws std::runtime_error If any error occurs during parsing or processing.
 */
std::unordered_map<std::string, BlueprintKey>
buildBlueprintMap(const std::string &table) {
  std::unordered_map<std::string, BlueprintKey> map;
  try {
    // Parse the blueprint file.
    std::istringstream ss(
        std::string(blueprint_tadah,
          blueprint_tadah_size));           // embed-to-stream
    toml::value blueprint = toml::parse(ss);
    // Create a copy to process inheritance.
    toml::value bpCopy = blueprint;
    // Recursively expand inheritance.
    expandInheritance(bpCopy, blueprint, "");
    // Locate the table.
    const auto &items = toml::find<toml::table>(bpCopy, table);
    // Process each item in the table.
    for (const auto &itemPair : items) {
      std::string key = itemPair.first;
      // Convert key to uppercase.
      std::transform(key.begin(), key.end(), key.begin(), ::toupper);
      map[key] = buildBlueprintKey(key, itemPair.second);
    }
  } catch (const std::exception &ex) {
    throw std::runtime_error("Error in blueprint map initialization: " +
                             std::string(ex.what()));
  }
  return map;
}


///////////////////////////////////////////////////////////////////////////////
// TASKS blueprint
///////////////////////////////////////////////////////////////////////////////
/**
 * @brief Handles a single-level "keys" subtable by splitting multi-path INHERIT 
 *        into per-key subtables, ensuring user-defined entries won't be overwritten.
 *
 * If [someTask.keys] has INHERIT = ["ALLOWED_KEYS.a", "ALLOWED_KEYS.b"] plus a 
 * subtable [someTask.keys.a] with existing fields, those fields are kept, and 
 * we add "ALLOWED_KEYS.a" to [someTask.keys.a].INHERIT so that a subsequent 
 * expandInheritance call merges only the missing fields from ALLOWED_KEYS.a. 
 * 
 * Behavior:
 *  - If a subtable "keys.<lastPart>" already exists, append the inheritance path
 *    to its existing array named INHERIT (creating that array if none exists).
 *  - If "keys.<lastPart>" does not yet exist, create it with INHERIT=[inheritPath].
 *  - Remove the original [someTask.keys].INHERIT so it's not processed again.
 *
 * @param table TOML subtree (often [TASKS.someTask]) in which to look 
 *              for a "keys" subtable at this level.
 */
static void prepareKeysInheritanceOneLevelWithMerge(toml::value &table) {
  if (!table.is_table()) {
    return;
  }

  // Traverse all child entries
  for (auto &kv : table.as_table()) {
    if (kv.first == "keys" && kv.second.is_table()) {
      toml::value &keysTbl = kv.second; // [someTask.keys]
      if (keysTbl.contains("INHERIT")) {
        // e.g. INHERIT = ["ALLOWED_KEYS.dbfile", "ALLOWED_KEYS.structure", ...]
        std::vector<std::string> inheritList =
          toml::find<std::vector<std::string>>(keysTbl, "INHERIT");

        for (const auto &inheritPath : inheritList) {
          // Use the last token as the subtable name. For "ALLOWED_KEYS.structure", 
          // we call it "structure".
          std::vector<std::string> parts;
          boost::split(parts, inheritPath, boost::is_any_of("."));
          if (parts.empty()) {
            throw std::runtime_error("Empty inheritance path encountered.");
          }
          const std::string lastName = parts.back();

          // Check if [keys.<lastName>] already exists
          auto &keysTableAsTable = keysTbl.as_table();
          auto found = keysTableAsTable.find(lastName);
          if (found != keysTableAsTable.end()) {
            // Subtable already exists. Append inheritPath to its INHERIT array.
            if (!found->second.is_table()) {
              throw std::runtime_error(
                "'keys." + lastName + "' is not a table but found under 'keys'."
              );
            }
            toml::table &existingSubtable = found->second.as_table();

            // If subtable doesn’t have an INHERIT array, create one.
            if (!existingSubtable.count("INHERIT")) {
              existingSubtable["INHERIT"] = toml::value(std::vector<std::string>());
            }
            // Append
            toml::value &existingInheritVal = existingSubtable["INHERIT"];
            if (!existingInheritVal.is_array()) {
              throw std::runtime_error(
                "'keys." + lastName + ".INHERIT' exists but is not an array."
              );
            }
            existingInheritVal.as_array().push_back(toml::value(inheritPath));
          } else {
            // Subtable [keys.<lastName>] does not exist yet, create it
            toml::table t;
            t["INHERIT"] = toml::value(std::vector<std::string>{ inheritPath });
            keysTbl[lastName] = toml::value(t);
          }
        }
        // Erase the original top-level INHERIT from [keys]
        keysTbl.as_table().erase("INHERIT");
      }
    }
    else if (kv.second.is_table()) {
      // Recurse deeper into any other table but skip if it's "keys"
      // to ensure we only process first-level "keys" table
      prepareKeysInheritanceOneLevelWithMerge(kv.second);
    }
  }
}

/**
 * @brief Helper that parses a single group's "keys" subtable,
 *        similarly to how parseTaskRec handles normal keys.
 */
static void parseGroupKeys(const toml::value &keysNode, std::vector<tadah::core::BlueprintKey> &outKeys, const std::string &parentGroup) {
  using namespace tadah::core;

  if (!keysNode.is_table()) {
    throw std::runtime_error("[groups] 'keys' must be a table in group: " + parentGroup);
  }

  // Each field in 'keysNode' might use an INHERIT array or define fields directly
  for (const auto &kv : keysNode.as_table()) {
    const std::string localKeyName = kv.first;  // e.g. "dbfile"
    std::string dottedKeyName = parentGroup + "." + localKeyName;

    // buildBlueprintKey or handle INHERIT:
    BlueprintKey newKey = buildBlueprintKey(dottedKeyName, kv.second);
    outKeys.push_back(newKey);
  }
}

/**
 * @brief Parse the [TASKS.<task>.groups] block, storing the results in a vector
 *        of BlueprintKeyGroup.
 */
static void parseGroups(const toml::value &groupsTbl,
                        std::vector<tadah::core::BlueprintKeyGroup> &outGroups,
                        const std::string &fullTaskName) {

  if (!groupsTbl.is_table()) {
    // "groups" block is not a table => ignore or throw
    throw std::runtime_error(fullTaskName + " has 'groups' but it is not a table.");
  }

  for (const auto &groupPair : groupsTbl.as_table()) {
    tadah::core::BlueprintKeyGroup group;
    group.groupName = groupPair.first; // e.g. "input"

    const toml::value &groupNode = groupPair.second;
    if (!groupNode.is_table()) {
      throw std::runtime_error("Group '" + group.groupName + "' under '" + fullTaskName + "' must be a table.");
    }
    // A group takes these fields and no other: a field no code reads
    // would be accepted without effect (cli_excludes, cli_needs and
    // cli_required are fields of an option; the options a group requires
    // are required_min and required_max).
    for (const auto &field : groupNode.as_table()) {
      if (field.first == "description" || field.first == "required_min" ||
          field.first == "required_max" || field.first == "keys")
        continue;
      throw std::runtime_error(
          "The group " + group.groupName + " of the command " +
          fullTaskName + " has the field " + field.first +
          ", which a group does not take: a group takes description, "
          "required_min, required_max and keys (cli_excludes, cli_needs and "
          "cli_required are fields of an option)");
    }

    // 1) description
    if (groupNode.contains("description")) {
      // could be string or array of strings. If it's a single string, store in group.description[0].
      const toml::value &descVal = groupNode.at("description");
      if (descVal.is_string()) {
        group.description.push_back(descVal.as_string());
      }
      else if (descVal.is_array()) {
        for (auto &elem : descVal.as_array()) {
          group.description.push_back(elem.as_string());
        }
      }
    }

    // 2) required_min
    if (groupNode.contains("required_min")) {
      group.requiredMin = toml::find<size_t>(groupNode, "required_min");
    }
    // 3) required_max
    if (groupNode.contains("required_max")) {
      group.requiredMax = toml::find<size_t>(groupNode, "required_max");
    }
    // 4) The [TASKS.<task>.groups.<groupName>.keys] subtable
    if (groupNode.contains("keys")) {
      parseGroupKeys(groupNode.at("keys"), group.blueprintKeys, fullTaskName + "." + group.groupName);
    }

    // Done
    outGroups.push_back(group);
  }
}


/**
 * @brief Helper function that parses a TOML value and (if valid) constructs a BlueprintTask.
 *
 * If the table includes the mandatory fields 'description', 'long_description', and 'examples',
 * the function creates a BlueprintTask. It also checks for a "keys" subtable to parse
 * multiple BlueprintKey objects. Any optional nested subtables (e.g., "twobody") are 
 * recursively processed as sub-tasks of the current task. Each nested sub-task has its 
 * dotted name (e.g. "PROPERTIES.TWOBODY").
 *
 * @param fullTaskName Dot-separated task name (e.g. "PROPERTIES.TWOBODY").
 * @param tomlNode TOML subtable representing the current task or sub-task.
 * @param outTasks A map that stores the final tasks by their dot-separated names.
 *
 * Example usage:
 * @code
 * std::unordered_map<std::string, BlueprintTask> tasks;
 * parseTaskRec("PROPERTIES", tomlSubtable, tasks);
 * @endcode
 */
static void parseTaskRec(const std::string &fullTaskName,
                         const toml::value &tomlNode,
                         std::unordered_map<std::string, BlueprintTask> &outTasks) {
  // Attempt to parse mandatory fields to decide if it's a valid 'task' block
  // Note: if the subtable does not contain these, it is either not a valid task 
  // (throw) or a purely parent container with no direct commands (can skip or handle differently).
  bool hasDescription      = tomlNode.contains("description");
  bool hasLongDescription  = tomlNode.contains("long_description");
  bool hasExamples         = tomlNode.contains("examples");

  // If all mandatory fields are present, parse them
  // Otherwise, either treat it as a non-task container or throw an exception
  if (hasDescription && hasLongDescription && hasExamples) {
    BlueprintTask newTask;
    newTask.taskName       = fullTaskName;
    newTask.description    = toml::find<std::string>(tomlNode, "description");
    newTask.longDescription = toml::find<std::string>(tomlNode, "long_description");
    newTask.examples       = toml::find<std::vector<std::string>>(tomlNode, "examples");

    // If there's a 'keys' subtable, parse blueprint keys.
    // Typically: [TASKS.properties.twobody.keys]
    if (tomlNode.contains("keys")) {
      const toml::value keysNode = toml::find(tomlNode, "keys");
      if (!keysNode.is_table()) {
        throw std::runtime_error("'keys' must be a table for task: " + fullTaskName);
      }
      // Iterate over each key entry (e.g., potential, outfile, range, ...)
      for (const auto &kv : keysNode.as_table()) {
        const std::string localKeyName = kv.first;
        // Build a fully qualified key name for debugging/trace
        std::string dottedKeyName = fullTaskName + "." + localKeyName;
        newTask.blueprintKeys.push_back(buildBlueprintKey(dottedKeyName, kv.second));
      }
    }

    if (tomlNode.contains("groups")) {
      const toml::value &groupsNode = tomlNode.at("groups");
      parseGroups(groupsNode, newTask.groups, fullTaskName);
    }

    // Store the parsed task in the map
    // Key = dot-separated name, value = the BlueprintTask object
    outTasks[fullTaskName] = std::move(newTask);
  }
  else {
    // If partial fields are present, throw with more information
    // (Alternatively, treat it as a container only if the user wants partial tasks to be ignored)
    if (hasDescription || hasLongDescription || hasExamples) {
      throw std::runtime_error("Task '" + fullTaskName +
        "' is missing one of the mandatory fields: description, long_description, or examples.");
    }
  }

  // Now check for deeper subcommands in the current node
  // We skip known fields (description, long_description, examples, keys and groups) and see if the child is a table.
  if (tomlNode.is_table()) {
  for (const auto &child : tomlNode.as_table()) {
    const std::string &k = child.first;

    // Skip any keys that are not sub-tasks
    if (k == "description" || k == "long_description" || k == "examples" ||
        k == "keys"        || k == "groups") {
      continue;
    }

    if (child.second.is_table()) {
      std::string subCommandName = fullTaskName.empty() ? k : (fullTaskName + "." + k);
      parseTaskRec(subCommandName, child.second, outTasks);
    }
  }
}
}

/* The embedded blueprint, blueprint/blueprint_tadah.toml, parsed. */
static toml::value parseEmbeddedBlueprint() {
  std::istringstream ss(
      std::string(blueprint_tadah,
        blueprint_tadah_size));           // embed-to-stream
  return toml::parse(ss);
}

} // end anonymous namespace

/**
 * Builds the commands of @p blueprint (its [TASKS] table), handling nested
 * (dot-separated) commands:
 * - expands inheritance for each table;
 * - locates the "TASKS" subtable to parse top-level and nested tasks;
 * - calls parseTaskRec(...) to handle each subtable recursively;
 * - checks the options of every command (checkCommandOptions).
 */
std::unordered_map<std::string, BlueprintTask>
BlueprintParser::buildTasksMap(const toml::value &blueprint) {
  std::unordered_map<std::string, BlueprintTask> tasksMap;

  try {
    // Make a copy for safe expansion
    toml::value bpCopy = blueprint;
    prepareKeysInheritanceOneLevelWithMerge(bpCopy);
    expandInheritance(bpCopy, blueprint, "");

    // Locate the top-level tasks subtable
    // e.g. [TASKS.transform], [TASKS.properties], ...
    const auto &tasksRootTable = toml::find<toml::table>(bpCopy, "TASKS");

    // For each entry in the "TASKS" table, parse recursively
    for (const auto &kv : tasksRootTable) {
      std::string topName = kv.first;
      // Example: "transform" or "properties"
      // Convert to uppercase for consistency
      std::transform(topName.begin(), topName.end(), topName.begin(), ::toupper);

      // Recurse to parse subcommands. The parseTaskRec function will:
      //  - parse the current table if it has mandatory fields
      //  - parse deeper subcommands if present
      if (kv.second.is_table()) {
        parseTaskRec(topName, kv.second, tasksMap);
      }
    }
    // Each name in the cli_excludes and cli_needs of an option names
    // another option of its command.
    for (const auto &kv : tasksMap) checkCommandOptions(kv.second);
  }
  catch (const std::exception &ex) {
    throw std::runtime_error("Error building blueprint tasks map: " + std::string(ex.what()));
  }

  return tasksMap;
}

bool BlueprintKey::isAllowedValue(const std::string &value) const {
  if (allowed_values.empty()) return true;
  const std::string u = to_upper(value);
  for (const auto &a : allowed_values)
    if (to_upper(a) == u) return true;
  return false;
}


///////////////////////////////////////////////////////////////////////////////
/// Retrieves a copy of the statically cached blueprint map.
/// @return A copy of the unordered_map mapping blueprint keys to BlueprintKey
/// objects.
/// @throws std::runtime_error If blueprint parsing or inheritance expansion
/// fails.
///////////////////////////////////////////////////////////////////////////////
std::unordered_map<std::string, BlueprintKey> BlueprintParser::getKeysMap() {
  // Return a copy of the static blueprint map.
  return getKeysMapRef();
}

std::unordered_map<std::string, BlueprintKey>
BlueprintParser::getInternalKeysMap() {
  // Return a copy of the static blueprint map.
  return getInternalKeysMapRef();
}

std::unordered_map<std::string, BlueprintTask> BlueprintParser::getTasksMap() {
  // Return a copy of the static blueprint map.
  return getTasksMapRef();
}

BlueprintTadah BlueprintParser::getBlueprintTadah() {
  // Return a copy of the static blueprint map.
  return getBlueprintTadahRef();
}
/**
 * Builds the [TADAH] table of @p blueprint: the core fields (logo, version,
 * website, description, long_description, examples), all mandatory. A
 * keys table is refused: tadah alone takes no option but -h and --version.
 */
BlueprintTadah BlueprintParser::buildBlueprintTadah(const toml::value &blueprintIn) {
  BlueprintTadah bt;

  // ------------------------------------------------------------
  // 1) Prepare single-level "keys" inheritance if required
  // ------------------------------------------------------------
  toml::value blueprint = blueprintIn;
  prepareKeysInheritanceOneLevelWithMerge(blueprint);

  // ------------------------------------------------------------
  // 2) Expand all inheritance references recursively
  // ------------------------------------------------------------
  toml::value bpCopy = blueprint;
  expandInheritance(bpCopy, blueprint, "");

  // ------------------------------------------------------------
  // 3) Access the [TADAH] subtable
  // ------------------------------------------------------------
  const toml::value tadahValue = toml::find(bpCopy, "TADAH");
  if (!tadahValue.is_table()) {
    throw std::runtime_error("Section [TADAH] must be a table in the blueprint");
  }
  const toml::table &tadahTable = tadahValue.as_table();

  // ------------------------------------------------------------
  // 4) Retrieve mandatory fields
  // ------------------------------------------------------------
  // logo
  {
    toml::table::const_iterator it = tadahTable.find("logo");
    if (it == tadahTable.end()) {
      throw std::runtime_error("[TADAH] is missing mandatory field 'logo'");
    }
    bt.logo = toml::get<std::string>(it->second);
  }
  // version
  {
    toml::table::const_iterator it = tadahTable.find("version");
    if (it == tadahTable.end()) {
      throw std::runtime_error("[TADAH] is missing mandatory field 'version'");
    }
    bt.version = toml::get<std::string>(it->second);
  }
  // website
  {
    toml::table::const_iterator it = tadahTable.find("website");
    if (it == tadahTable.end()) {
      throw std::runtime_error("[TADAH] is missing mandatory field 'website'");
    }
    bt.website = toml::get<std::string>(it->second);
  }
  // description
  {
    toml::table::const_iterator it = tadahTable.find("description");
    if (it == tadahTable.end()) {
      throw std::runtime_error("[TADAH] is missing mandatory field 'description'");
    }
    bt.description = toml::get<std::string>(it->second);
  }
  // longDescription
  {
    toml::table::const_iterator it = tadahTable.find("long_description");
    if (it == tadahTable.end()) {
      throw std::runtime_error("[TADAH] is missing mandatory field 'long_description'");
    }
    bt.longDescription = toml::get<std::string>(it->second);
  }
  // examples (array of strings)
  {
    toml::table::const_iterator it = tadahTable.find("examples");
    if (it == tadahTable.end()) {
      throw std::runtime_error("[TADAH] is missing mandatory field 'examples'");
    }
    // throws if not an array of strings
    bt.examples = toml::get<std::vector<std::string>>(it->second);
  }

  // ------------------------------------------------------------
  // 5) No keys: tadah alone takes no option but -h and --version, and
  //    every other option belongs to a command
  // ------------------------------------------------------------
  if (tadahTable.find("keys") != tadahTable.end()) {
    throw std::runtime_error(
        "[TADAH] must not have a keys table: tadah alone takes no option "
        "but -h and --version, and an option belongs to a command "
        "([TASKS.<command>.keys])");
  }

  // ------------------------------------------------------------
  // Return the fully-constructed structure
  // ------------------------------------------------------------
  return bt;
}

/**
 * @brief Returns a reference to a statically cached BlueprintTadah object,
 *        built from the embedded blueprint (buildBlueprintTadah).
 */
const BlueprintTadah &BlueprintParser::getBlueprintTadahRef() {
  static BlueprintTadah blueprintTadah =
      buildBlueprintTadah(parseEmbeddedBlueprint());
  return blueprintTadah;
}
///////////////////////////////////////////////////////////////////////////////
/// Retrieves a const reference to the statically cached blueprint map.
/// Uses a static local variable to guarantee one-time initialization.
/// @return Const reference to the unordered_map mapping blueprint keys to
/// BlueprintKey objects.
/// @throws std::runtime_error If blueprint parsing or inheritance expansion
/// fails.
///////////////////////////////////////////////////////////////////////////////
const std::unordered_map<std::string, BlueprintKey> &
BlueprintParser::getKeysMapRef() {
  static std::unordered_map<std::string, BlueprintKey> blueprintAllowedMap =
      buildBlueprintMap("ALLOWED_KEYS");
  return blueprintAllowedMap;
}
const std::unordered_map<std::string, BlueprintKey> &
BlueprintParser::getInternalKeysMapRef() {
  static std::unordered_map<std::string, BlueprintKey> blueprintInternalMap =
      buildBlueprintMap("INTERNAL_KEYS");
  return blueprintInternalMap;
}
const std::unordered_map<std::string, BlueprintTask> &
BlueprintParser::getTasksMapRef() {
  static std::unordered_map<std::string, BlueprintTask> blueprintTasksMap =
      buildTasksMap(parseEmbeddedBlueprint());
  return blueprintTasksMap;
}

void BlueprintParser::checkCommandOptions(const BlueprintTask &task) {
  std::vector<const BlueprintKey *> options;
  for (const BlueprintKey &k : task.blueprintKeys) options.push_back(&k);
  for (const BlueprintKeyGroup &g : task.groups)
    for (const BlueprintKey &k : g.blueprintKeys) options.push_back(&k);
  for (const BlueprintKey *k : options) {
    for (const auto &field :
         {std::make_pair("cli_excludes", &k->cli_excludes),
          std::make_pair("cli_needs", &k->cli_needs)}) {
      for (const std::string &name : *field.second) {
        const bool named = std::any_of(
            options.begin(), options.end(), [&](const BlueprintKey *o) {
              return o != k && !o->cli_long_flag.empty() &&
                     o->cli_long_flag == "--" + name;
            });
        if (!named)
          throw std::runtime_error(
              "The option " +
              (k->cli_long_flag.empty() ? k->keyName : k->cli_long_flag) +
              " of the command " + task.taskName + " has " + field.first +
              " '" + name +
              "', which names no other option of the command (an option "
              "is named by its long flag without --)");
      }
    }
  }
}

BlueprintKey BlueprintParser::buildKey(const std::string &fullKey,
                                       const toml::value &table) {
  return buildBlueprintKey(fullKey, table);
}

std::string BlueprintParser::findClosestKey(
    const std::string &key,
    const std::unordered_map<std::string, BlueprintKey> &map) {
  // Early return if the map is empty
  if (map.empty()) {
    return "";
  }

  // Helper function computing Levenshtein distance using dynamic programming
  auto computeLevenshteinDistance = [](const std::string &s1,
                                       const std::string &s2) {
    const std::size_t m = s1.size();
    const std::size_t n = s2.size();

    // Vectors store the cost for transforming prefixes up to the current
    // character
    std::vector<int> prevRow(n + 1), currRow(n + 1);

    // Initial boundary conditions for s1= empty prefix to full length
    for (std::size_t j = 0; j <= n; ++j) {
      prevRow[j] = static_cast<int>(j);
    }

    // Fill the DP table row by row
    for (std::size_t i = 0; i < m; ++i) {
      currRow[0] = static_cast<int>(i + 1);
      for (std::size_t j = 0; j < n; ++j) {
        // Cost of substituting s1[i] with s2[j]
        int cost = (s1[i] == s2[j]) ? 0 : 1;

        // Compute minimal cost from neighboring states
        currRow[j + 1] = std::min(std::min(prevRow[j + 1] + 1, // Deletion
                                           currRow[j] + 1),    // Insertion
                                  prevRow[j] + cost            // Substitution
        );
      }
      // Swap the current row into previous to reuse storage
      prevRow.swap(currRow);
    }
    // After filling the table, the last cell of prevRow holds the edit distance
    return prevRow[n];
  };

  int bestDistance = std::numeric_limits<int>::max();
  std::string bestKey;

  // Loop over all keys in the map, computing the edit distance and tracking the
  // minimal result. Deprecated and retired keys are never suggested.
  for (const auto &pair : map) {
    if (pair.second.isDeprecated() || pair.second.isRetired()) {
      continue;
    }
    int dist = computeLevenshteinDistance(key, pair.first);
    if (dist < bestDistance) {
      bestDistance = dist;
      bestKey = pair.first;
    }
  }

  return bestKey;
}
namespace {

/* The key name of a blueprint key of a task ("TASKS.predict.keys.analytics",
 * "PREDICT.analytics"): its last word, upper case. */
std::string last_word_upper(const std::string &name) {
  const std::size_t dot = name.rfind('.');
  std::string key = dot == std::string::npos ? name : name.substr(dot + 1);
  std::transform(key.begin(), key.end(), key.begin(),
                 [](unsigned char c) { return std::toupper(c); });
  return key;
}

/* "a", "a and b", "a, b and c". */
std::string and_list(const std::vector<std::string> &items) {
  std::string s;
  for (std::size_t i = 0; i < items.size(); ++i)
    s += (i == 0 ? "" : i + 1 == items.size() ? " and " : ", ") + items[i];
  return s;
}

}  // namespace

std::string task_command_words(const std::string &task) {
  std::string words = "tadah " + task;
  std::transform(words.begin(), words.end(), words.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  std::replace(words.begin(), words.end(), '.', ' ');
  return words;
}

std::vector<std::string> commands_reading_key(const std::string &key) {
  // A task key that the task retires (an option it refuses with the
  // reason, such as --rescale of tadah data balance) is not read by it.
  auto reads_it = [&key](const BlueprintKey &k) {
    return !k.isRetired() && last_word_upper(k.keyName) == key;
  };
  std::vector<std::string> tasks;
  for (const auto &kv : BlueprintParser::getTasksMapRef()) {
    bool reads = false;
    for (const BlueprintKey &k : kv.second.blueprintKeys)
      if (reads_it(k)) reads = true;
    for (const BlueprintKeyGroup &g : kv.second.groups)
      for (const BlueprintKey &k : g.blueprintKeys)
        if (reads_it(k)) reads = true;
    if (reads) tasks.push_back(kv.first);
  }
  std::sort(tasks.begin(), tasks.end());
  std::vector<std::string> commands;
  for (const std::string &t : tasks) commands.push_back(task_command_words(t));
  return commands;
}

std::string command_key_role(const std::string &key) {
  const std::vector<std::string> commands = commands_reading_key(key);
  if (commands.empty()) return "read by no tadah command";
  const auto &keys = BlueprintParser::getKeysMapRef();
  const auto found = keys.find(key);
  const std::string flag =
      found == keys.end() ? std::string() : found->second.cli_long_flag;
  // A flag that does not start with '-' names a positional argument
  // (OPTION of tadah explain).
  if (!flag.empty() && flag[0] != '-')
    return "the argument of " + and_list(commands);
  if (flag.rfind("--", 0) == 0) {
    std::string option = flag;
    if (!found->second.cli_short_flag.empty())
      option += " (" + found->second.cli_short_flag + ")";
    return "the option " + option + " of " + and_list(commands);
  }
  return "a key of " + and_list(commands);
}

std::string command_key_text(const std::string &key) {
  return key + " is " + command_key_role(key);
}

std::string option_text(const std::string &key) {
  const auto &keys = BlueprintParser::getKeysMapRef();
  const auto found = keys.find(key);
  if (found == keys.end() || found->second.cli_long_flag.rfind("--", 0) != 0)
    return std::string();
  std::string option = found->second.cli_long_flag;
  if (!found->second.cli_short_flag.empty())
    option += " (" + found->second.cli_short_flag + ")";
  return option;
}

bool config_needs_option(const std::string &task, const std::string &key) {
  const auto &tasks = BlueprintParser::getTasksMapRef();
  const auto t = tasks.find(task);
  if (t == tasks.end()) return false;
  auto needs_it = [&key](const BlueprintKey &k) {
    if (k.isRetired() || last_word_upper(k.keyName) != "CONFIG") return false;
    for (const std::string &n : k.cli_needs)
      if (last_word_upper(n) == key) return true;
    return false;
  };
  for (const BlueprintKey &k : t->second.blueprintKeys)
    if (needs_it(k)) return true;
  for (const BlueprintKeyGroup &g : t->second.groups)
    for (const BlueprintKey &k : g.blueprintKeys)
      if (needs_it(k)) return true;
  return false;
}

} // namespace core
} // namespace tadah
