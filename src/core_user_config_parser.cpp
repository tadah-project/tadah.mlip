#include "tadah/core/blueprint_parser.h"
#include "tadah/core/blueprint_validator.h"
#include "tadah/core/core_user_config_parser.h"
#include "tadah/core/key_notice.h"
#include "tadah/core/logger.h"
#include "tadah/core/retired_keys.h"
#include "tadah/core/utils.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <unordered_set>

namespace {
/* "A, B, C": the allowed values of a key, for its error messages. */
std::string joinAllowed(const std::vector<std::string> &values) {
  std::string out;
  for (std::size_t i = 0; i < values.size(); ++i)
    out += (i ? ", " : "") + values[i];
  return out;
}
} // namespace

namespace tadah {
namespace core {

namespace {
  /**
   * @brief Holds the recursion depth of parse() to identify top-level calls.
   *
   * parse() might be called recursively when encountering nested CONFIG lines.
   * If parseDepth is zero, it means a top-level parse() call starts.
   */
  static thread_local int parseDepth = 0;

  /**
   * @brief The files whose lines are being read, outermost first: the file
   *        of the parse, then the file of each CONFIG line being read, each
   *        by its path as given (ReadingFile).
   *
   * A file is pushed before its lines are read and popped after them. If
   * parseDepth resets to zero, the chain is cleared.
   */
  static thread_local std::vector<std::string> configChain;

  /**
   * @brief Holds the file @p path in configChain while its lines are read.
   */
  class ReadingFile {
  public:
    explicit ReadingFile(const std::string &path) {
      configChain.push_back(path);
    }
    ~ReadingFile() { configChain.pop_back(); }
    ReadingFile(const ReadingFile &) = delete;
    ReadingFile &operator=(const ReadingFile &) = delete;
  };

  /**
   * @brief Whether @p path names a file of configChain: the same file,
   *        whatever path names it ("run.train", "./run.train", its absolute
   *        path or a link to it; std::filesystem::equivalent). A path that
   *        names no existing file is none of them: the file does not open.
   */
  bool beingRead(const std::string &path) {
    for (const std::string &file : configChain) {
      std::error_code ec;
      if (std::filesystem::equivalent(file, path, ec)) return true;
    }
    return false;
  }

  /**
   * @brief Writes the notice for a deprecated key to standard error, once
   *        per key and process (report_ignored_key_once(), the rule of
   *        every ignored key: however many lines, CONFIG files or parses
   *        read it). Not a Logger message, so neither the Logger level nor
   *        a VERBOSE line hides it, in LAMMPS either.
   */
  void reportDeprecatedKey(const std::string &key, const std::string &reason) {
    report_ignored_key_once(key, "Deprecated key " + key, reason);
  }

  /**
   * @brief One parse() call: tracks the recursion depth and resets the
   *        per-parse state (the CONFIG chain) at the start and at the end
   *        of a top-level parse.
   *
   * RAII, so that a parse that throws (a malformed line, a missing CONFIG
   * file) cannot leave the depth raised: the next parse on this thread is
   * then a top-level parse again, with a fresh CONFIG chain.
   */
  class ParseScope {
  public:
    ParseScope() {
      if (parseDepth == 0) reset();
      ++parseDepth;
    }
    ~ParseScope() {
      --parseDepth;
      if (parseDepth == 0) reset();
    }
    ParseScope(const ParseScope &) = delete;
    ParseScope &operator=(const ParseScope &) = delete;

  private:
    static void reset() {
      configChain.clear();
    }
  };

  /**
   * @brief "line N" or "line N of <source>": where an error is.
   */
  std::string lineOf(std::size_t line, const std::string &source) {
    std::string out = "line " + std::to_string(line);
    if (!source.empty()) out += " of " + source;
    return out;
  }

  /**
   * @brief What one public parse knows of the lines it has read, over the
   *        files its CONFIG lines include (one Context holds them all: the
   *        configuration is the concatenation of its lines).
   */
  struct ParseState {
    /// The CONFIG lines that include the file being read, outermost first
    /// ("CONFIG on line 2 of run.train"); empty for the file of the parse.
    std::vector<std::string> includedBy;
    /// Each key stored so far -> the lines that gave its values, in line
    /// order (Context::set_key_lines() at the end of the parse).
    std::map<std::string, std::vector<KeyLine>> lines;
  };

  /**
   * @brief The CONFIG lines that include the file being read, innermost
   *        first: "CONFIG on line 3 of mid.train, which CONFIG on line 2 of
   *        run.train includes"; empty for the file of the parse.
   */
  std::string includedByText(const ParseState &state) {
    const std::vector<std::string> &by = state.includedBy;
    if (by.empty()) return std::string();
    std::string out = by.back();
    for (std::size_t k = by.size() - 1; k-- > 0;)
      out += ", which " + by[k] + " includes";
    return out;
  }

  /**
   * @brief Where line @p line of @p source is, as every error of the line
   *        names it: lineOf(), and for a line of an included file the CONFIG
   *        lines that include it, innermost first: "line 2 of inc.train
   *        (CONFIG on line 2 of run.train)", "line 1 of deep.train (CONFIG
   *        on line 3 of mid.train, which CONFIG on line 2 of run.train
   *        includes)".
   */
  std::string where(std::size_t line, const std::string &source,
                    const ParseState &state) {
    const std::string by = includedByText(state);
    return lineOf(line, source) + (by.empty() ? std::string() : " (" + by + ")");
  }

  /** "a", "a and b", "a, b and c": the lines @p lines, by where they are. */
  std::string linesText(const std::vector<KeyLine> &lines) {
    std::string out;
    for (std::size_t k = 0; k < lines.size(); ++k)
      out += (k == 0 ? "" : k + 1 == lines.size() ? " and " : ", ") +
             lines[k].where;
    return out;
  }

  /**
   * @brief What a refusal of a line of a key adds when the key's lines are a
   *        potential's and another file's (KeyLine::potential): how a
   *        potential's value is changed. Empty when they are all of one
   *        kind (two lines of one copy of a potential among them).
   */
  std::string copyAdvice(const std::vector<KeyLine> &before, bool potential) {
    for (const KeyLine &l : before)
      if (l.potential != potential)
        return "; a potential's value is changed by editing its line in a "
               "copy of the potential (the Training page of the "
               "documentation, Retraining from a potential)";
    return std::string();
  }

  /**
   * @brief The number of values the command @p task (a blueprint task name,
   *        "TRAIN"; empty: none) takes of each key of its table (value_N[0]
   *        of its BlueprintKey). A configuration file of the command is read
   *        with them: tadah train takes one OUTFILE value, as it writes one
   *        file, although OUTFILE takes any number.
   */
  std::map<std::string, int> commandValueCounts(const std::string &task) {
    std::map<std::string, int> out;
    if (task.empty()) return out;
    const auto &tasks = BlueprintParser::getTasksMapRef();
    const auto t = tasks.find(task);
    if (t == tasks.end()) return out;
    const auto take = [&out](const BlueprintKey &k) {
      if (k.isRetired() || k.value_N.empty()) return;
      const std::size_t dot = k.keyName.rfind('.');
      out[to_upper(dot == std::string::npos ? k.keyName
                                            : k.keyName.substr(dot + 1))] =
          k.value_N[0];
    };
    for (const BlueprintKey &k : t->second.blueprintKeys) take(k);
    for (const BlueprintKeyGroup &g : t->second.groups)
      for (const BlueprintKey &k : g.blueprintKeys) take(k);
    return out;
  }

  /**
   * @brief What the error of a value of @p key that its types do not take
   *        gives after its first line: "  Allowed schema:" and
   *        "    <key>   <type>" for each value type of the key, "  Format:"
   *        and "    <key>   <format>" for each of its formats and, when it
   *        has examples, "  Examples:" and "    <key>   <line>" for each
   *        non-blank line of each example (an example is a value of the
   *        key, the blueprint's rule).
   */
  std::vector<std::string> valueHelp(const std::string &key,
                                     const BlueprintKey &keyInfo) {
    std::vector<std::string> out{"  Allowed schema:"};
    for (const auto &schema : keyInfo.value_type)
      out.push_back("    " + key + "   " + schema);
    out.push_back("  Format:");
    for (const auto &fmt : keyInfo.value_format)
      out.push_back("    " + key + "   " + fmt);
    if (!keyInfo.examples.empty()) {
      out.push_back("  Examples:");
      for (const auto &ex : keyInfo.examples) {
        std::istringstream lines(ex);
        std::string line;
        while (std::getline(lines, line)) {
          line = trim(line);
          if (!line.empty()) out.push_back("    " + key + "   " + line);
        }
      }
    }
    return out;
  }

  /**
   * @brief The error of the value @p val of the key @p uKey (upper case),
   *        which its types or its allowed values do not take, at @p where
   *        ("line 3 of config.train"): "Invalid value for key <KEY> on
   *        <where>: ", then for a value of the key's types outside its
   *        allowed values the values allowed; for words a comma or a '+'
   *        joins, which blanks would make a value, the separator and the
   *        line to write; for any other value, or none, the key's schema,
   *        format and examples on the lines that follow (valueHelp).
   */
  std::string valueRefusal(const std::string &uKey, const std::string &val,
                           const BlueprintKey &keyInfo,
                           const std::string &where) {
    const std::string value = trim(val);
    const std::string head =
        "Invalid value for key " + uKey + " on " + where + ": ";
    if (!keyInfo.allowed_values.empty() &&
        CoreUserConfigParser::validateSchemaOnly(val, keyInfo))
      return head + "'" + value + "' (allowed: " +
             joinAllowed(keyInfo.allowed_values) + ")";
    std::string respaced;
    const std::string seps =
        CoreUserConfigParser::misusedSeparators(val, keyInfo, respaced);
    if (!seps.empty())
      return head + "'" + value + "' separates values with " + seps +
             ", which a configuration line is not split at (values are "
             "separated by blanks: " + uKey + " " + respaced + ")";
    std::string out =
        head + (value.empty() ? std::string("no value") : "'" + value + "'");
    for (const std::string &line : valueHelp(uKey, keyInfo))
      out += "\n" + line;
    return out;
  }

  /**
   * @brief The keys whose lines open a verbatim multi-line block (closed by
   *        END<key>), whose body is not made of key lines.
   */
  const std::unordered_set<std::string> &verbatimBlocks() {
    static const std::unordered_set<std::string> blocks = {
        "OPTIMIZER", "INIT", "EXPLORE", "REFINE", "FUSE"};
    return blocks;
  }

  /**
   * @brief The keys of the key lines of @p lines (KeyLineRule's file_keys):
   *        the first word, upper case, of each line that is neither blank
   *        nor a comment, the lines inside a verbatim block excepted (the
   *        block's own name is one of them).
   */
  std::set<std::string> fileKeys(const std::vector<std::string> &lines) {
    std::set<std::string> keys;
    std::string blockEnd;  // the END<block> awaited; empty outside a block
    for (const std::string &raw : lines) {
      std::string line = trim(raw);
      if (line.empty() || line[0] == '#') continue;
      line = trim(line.substr(0, line.find('#')));
      if (line.empty()) continue;
      if (!blockEnd.empty()) {
        if (to_upper(line) == blockEnd) blockEnd.clear();
        continue;
      }
      std::istringstream iss(line);
      std::string first;
      iss >> first;
      const std::string uKey = to_upper(first);
      keys.insert(uKey);
      if (verbatimBlocks().count(uKey) || uKey == "DESCRIPTOR")
        blockEnd = "END" + uKey;
    }
    return keys;
  }

  /**
   * @brief Whether the line of @p key (upper case) of a file whose own key
   *        lines give @p file_keys is a run setting of a potential: the
   *        file has WEIGHTS and @p key is one of run_setting_keys(). No
   *        parse stores it.
   */
  bool potentialRunSetting(const std::set<std::string> &file_keys,
                           const std::string &key) {
    if (!file_keys.count("WEIGHTS")) return false;
    const std::vector<std::string> &keys = run_setting_keys();
    return std::find(keys.begin(), keys.end(), key) != keys.end();
  }

  /**
   * @brief What a parse refuses: the keys of refuse_keys() with their
   *        reasons, and the command whose configuration file it reads
   *        (set_command(): its blueprint task name, "TRAIN"; empty: none),
   *        whose file refuses a key only other commands read
   *        (BlueprintKey::command_only).
   */
  struct Refusal {
    const std::map<std::string, std::string> &keys;
    const std::string &command;
  };

  /**
   * @brief The key line rule of a parse and the list its skipped lines go
   *        to (both null: no rule).
   */
  struct LineSkip {
    const KeyLineRule *rule = nullptr;
    std::vector<SkippedKeyLine> *skipped = nullptr;
  };

  void parseFile(const std::string &filename, Context &ctx, ParseState &state,
                 const Refusal &refused, const LineSkip &skip,
                 const std::string &includedAt = "");

  /**
   * @brief Parse lines into an existing Context object.
   *
   * lines[i] is line first_line + i of source (empty: no name), which
   * every error of a line names, with its key and, for a line of an
   * included file, the CONFIG lines that include it (where()): an unknown
   * key, a key the caller refuses, a key of another command, a value the
   * key does not take, more values than the key takes, a second line of a
   * key of one value, a block that is not closed and a CONFIG line whose
   * file does not open or includes a file of the chain again.
   *
   * A CONFIG line reads the lines of its file into @p ctx at its place
   * (the configuration is the concatenation of its lines), with @p state:
   * a key of one value (|value_N[0]| 1) is set by one line of the
   * configuration, and a second line of it, in this file or another, is
   * refused naming both; a key of several values takes the values of its
   * lines in line order, up to the number it takes, and a line past it is
   * refused naming the lines before it. A line of a potential and a line
   * of another file of one such key are refused with the way to change a
   * potential's value (copyAdvice()). In a configuration file of a command
   * (refused.command), a key of the command's table takes the number of
   * values the table gives it (commandValueCounts(): OUTFILE of tadah
   * train, one).
   * A CONFIG line that names a file whose lines are being read (configChain:
   * the file of the parse and the files of the CONFIG lines being read) is
   * refused as a circular include before a line of the file is read again.
   *
   * A run setting of a potential (potentialRunSetting, by the keys of
   * @p lines: fileKeys) is checked and not stored, with or without a rule;
   * with a rule (@p skip), so is a key line the rule selects, and both are
   * appended to skip.skipped: such a line sets nothing. A CONFIG include is
   * read under the same rules, by the keys of its own lines.
   */
  void parseLines(const std::vector<std::string> &lines, Context &ctx,
                  ParseState &state, std::size_t first_line,
                  const std::string &source, const Refusal &refused,
                  const LineSkip &skip = LineSkip()) {
    const auto keyMap = BlueprintParser::getKeysMapRef();
    const std::set<std::string> ownKeys = fileKeys(lines);
    const bool potential = ownKeys.count("WEIGHTS") > 0;
    // The number of each line among the lines that hold more than blanks
    // and a comment (KeyLine::ordinal), from 1.
    std::vector<std::size_t> ordinal(lines.size(), 0);
    for (std::size_t i = 0, n = 0; i < lines.size(); ++i) {
      if (!trim(lines[i].substr(0, lines[i].find('#'))).empty()) ++n;
      ordinal[i] = n;
    }
    const std::map<std::string, int> commandCounts =
        commandValueCounts(refused.command);
    // A line of a key that only some commands read
    // (BlueprintKey::command_only), at line @p at (its text @p text, "KEY
    // values" or "KEY" and its block): in a potential it is not read, with
    // one notice per key (a potential written before 1.6.0 may hold such a
    // line, as tadah train wrote what its configuration held), but for a
    // run setting (OUTFILE), which the potential's lines of the run that
    // wrote it hold and the run-setting rule below skips and lists; in a
    // configuration file of a command that does not read it, it is
    // refused (a configuration file of a command whose -c needs the key's
    // option is refused before, by neededOptionLine). True when the line
    // is not read.
    auto commandOnlyLine = [&](const std::string &uKey, std::size_t at,
                               const std::string &text,
                               std::size_t block_end = 0) {
      if (potential && potentialRunSetting(ownKeys, uKey)) return false;
      if (potential) {
        report_ignored_key_once(
            uKey, "Key " + uKey,
            text + " on " + lineOf(at, source) +
                (block_end ? " (a block, to END" + uKey + " on line " +
                                 std::to_string(block_end) + ")"
                           : std::string()) +
                " is " + command_key_role(uKey) + ", not a key of a "
                "potential; the " + (block_end ? "block" : "line") +
                " is not read");
        return true;
      }
      if (refused.command.empty()) return false;
      const std::string command = task_command_words(refused.command);
      const std::vector<std::string> readers = commands_reading_key(uKey);
      if (std::find(readers.begin(), readers.end(), command) != readers.end())
        return false;
      throw std::runtime_error("Key " + uKey + " on " +
                               where(at, source, state) +
                               " is not accepted here: " +
                               command_key_text(uKey) +
                               ", not a key of a configuration file of " +
                               command);
    };
    // A line of a key whose option -c of the command needs on the command
    // line (config_needs_option: OUTFILE of tadah analysis dataset_stats
    // and suggest_bounds, HPOTARGET of tadah hpo, RANGE and TYPE of tadah
    // analysis bfunc), at line @p at, in a configuration file of that
    // command (not a potential, whose lines of a run setting the rule of
    // run settings skips): refused, as the option's value would replace
    // it, whether other commands read the key from a configuration file
    // (HPOTARGET: tadah analysis check_bounds) or only from their command
    // line (BlueprintKey::command_only).
    auto neededOptionLine = [&](const std::string &uKey, std::size_t at) {
      if (potential || refused.command.empty()) return;
      if (!config_needs_option(refused.command, uKey)) return;
      const std::string command = task_command_words(refused.command);
      throw std::runtime_error(
          "Key " + uKey + " on " + where(at, source, state) +
          " is not accepted here: " + uKey + " is the option " +
          option_text(uKey) + " that " + option_text("CONFIG") + " of " +
          command + " needs on the command line, not a key of a "
          "configuration file of " + command);
    };
    for (size_t i = 0; i < lines.size(); ++i) {
      std::string rawLine = trim(lines[i]);
      if (rawLine.empty() || rawLine[0] == '#') {
        continue;
      }
      // Remove in-line comments, and the blanks before them: the value of a
      // key is the text up to the last non-blank character before the '#'
      // (CONFIG uses it as a path, also in its circular-include check).
      rawLine = trim(rawLine.substr(0, rawLine.find('#')));
      std::istringstream iss(rawLine);
      std::vector<std::string> tokens;
      {
        std::string token;
        while (iss >> token) {
          tokens.push_back(token);
        }
      }
      if (tokens.empty()) {
        continue;
      }

      // first token is the key
      const std::string &key = tokens[0];
      std::string uKey = to_upper(key);

      // A key the caller refuses for this input (refuse_keys()).
      if (const auto r = refused.keys.find(uKey); r != refused.keys.end()) {
        throw std::runtime_error("Key " + uKey + " on " +
                                 where(first_line + i, source, state) +
                                 " is not accepted here: " + r->second);
      }

      if (uKey == "DESCRIPTOR") {
        ConfigBlock block{uKey, source, first_line + i, {lines[i]}};
        std::vector<std::string> ends{"ENDDESCRIPTOR"};
        std::size_t k = i + 1;
        for (; k < lines.size(); ++k) {
          block.lines.push_back(lines[k]);
          const auto line = trim(lines[k].substr(0, lines[k].find('#')));
          if (line.empty()) continue;
          std::istringstream words(line);
          std::string word, extra;
          words >> word;
          word = to_upper(word);
          const bool has_extra = static_cast<bool>(words >> extra);
          if (word.rfind("END", 0) == 0) {
            if (has_extra || word != ends.back())
              throw std::runtime_error("Mismatched " + word + " on " +
                                       where(first_line + k, source, state) +
                                       "; expected " + ends.back());
            ends.pop_back();
            if (ends.empty()) break;
          } else if (!has_extra) {
            ends.push_back("END" + word);
          }
        }
        if (!ends.empty())
          throw std::runtime_error("DESCRIPTOR on " +
                                   where(first_line + i, source, state) +
                                   " is not closed; expected " + ends.back());
        ctx.append_block(block);
        i = k;
        continue;
      }

      // Verbatim multi-line blocks (not flat keys). Slurp every token between
      // `<BLOCK>` and the matching `END<BLOCK>` into the single (unbounded)
      // Context STRINGS key <BLOCK>; the consuming engine (e.g. `tadah refit`)
      // parses that token stream itself. Mirrors the CONFIG special-case below.
      //   OPTIMIZER  - legacy refit optimiser block (now nested; flagged by
      //                the refit parser if it appears at top level)
      //   INIT/EXPLORE/REFINE/FUSE - the refit stage blocks. A stage's nested
      //                OPTIMIZER...ENDOPTIMIZER is captured as body tokens
      //                because each opener closes ONLY on its own END<opener>
      //                (distinct end tokens => no depth counter needed).
      if (verbatimBlocks().count(uKey)) {
        const std::string endTok = "END" + uKey;
        std::string body;
        bool closed = false;
        size_t k = i + 1;
        for (; k < lines.size(); ++k) {
          std::string bl = trim(lines[k]);
          if (bl.empty() || bl[0] == '#') {
            continue;
          }
          bl = trim(bl.substr(0, bl.find('#')));
          if (bl.empty()) {
            continue;
          }
          if (to_upper(bl) == endTok) {
            closed = true;
            break;
          }
          body += bl + " ";
        }
        if (!closed) {
          throw std::runtime_error(uKey + " on " +
                                   where(first_line + i, source, state) +
                                   " is not closed; expected " + endTok);
        }
        neededOptionLine(uKey, first_line + i);
        const auto blockKey = keyMap.find(uKey);
        if (blockKey != keyMap.end() && blockKey->second.command_only &&
            commandOnlyLine(uKey, first_line + i, uKey, first_line + k)) {
          i = k;
          continue;
        }
        i = k;  // resume after END<block>
        if (!body.empty()) {
          ctx.add(uKey, body);  // STRINGS key: add() splits on whitespace
        }
        continue;
      }

      // A key that is no configuration key (a TASK line among them: a
      // file holds the keys of one command), at its line and file, with
      // the closest key.
      if (keyMap.find(uKey) == keyMap.end()) {
        const std::string suggested =
            BlueprintParser::findClosestKey(uKey, keyMap);
        throw std::runtime_error(
            "Invalid key " + uKey + " on " +
            where(first_line + i, source, state) +
            (suggested.empty() ? std::string()
                               : " (closest key: " + suggested + ")"));
      }
      // The value against the key's types and allowed values: a value it
      // does not take is an error at its line, with no Logger line.
      std::string val = removeFirstWord(rawLine, key);
      const BlueprintKey &keyInfo = keyMap.at(uKey);
      if (!CoreUserConfigParser::validateSchemaOnly(val, keyInfo) ||
          !keyInfo.isAllowedValue(trim(val))) {
        throw std::runtime_error(valueRefusal(
            uKey, val, keyInfo, where(first_line + i, source, state)));
      }
      neededOptionLine(uKey, first_line + i);
      if (keyInfo.command_only) {
        std::string text = uKey;
        for (std::size_t j = 1; j < tokens.size(); ++j) text += " " + tokens[j];
        if (commandOnlyLine(uKey, first_line + i, text)) continue;
      }
      if (keyInfo.isDeprecated()) {
        // Accepted so that older files still load, but never stored: the
        // key has no effect and must not be written back to a potential.
        // The line is recorded on the result, so that a fit refuses it
        // naming the line and the file (MODELS inert_keys.h).
        reportDeprecatedKey(uKey, keyInfo.deprecated);
        ctx.record_deprecated_key(RetiredKeyLine{
            uKey, source, first_line + i,
            std::vector<std::string>(tokens.begin() + 1, tokens.end()),
            trim(lines[i])});
        continue;
      }
      if (keyInfo.isRetired()) {
        // A line the caller's rule selects is the caller's, as any other
        // line: listed in its skipped lines, not recorded.
        if (skip.rule && (*skip.rule)(ownKeys, uKey)) {
          if (skip.skipped)
            skip.skipped->push_back(SkippedKeyLine{
                source, first_line + i, uKey,
                std::vector<std::string>(tokens.begin() + 1, tokens.end())});
          continue;
        }
        // Accepted so that a potential that carries it still loads, and
        // recorded on the result, never stored: a fit refuses the
        // configuration with the conversion (retired_keys.h), and the parse
        // gives one notice listing its retired lines (finishParse).
        ctx.record_retired_key(RetiredKeyLine{
            uKey, source, first_line + i,
            std::vector<std::string>(tokens.begin() + 1, tokens.end()),
            trim(lines[i])});
        continue;
      }
      // A line no parse stores (a run setting of a potential: the program
      // that loads the potential keeps its own; a potential's VERBOSE line
      // would set the Logger level) or the caller's rule does not read
      // (checked above as any line).
      if (uKey != "CONFIG" &&
          (potentialRunSetting(ownKeys, uKey) ||
           (skip.rule && (*skip.rule)(ownKeys, uKey)))) {
        if (skip.skipped)
          skip.skipped->push_back(SkippedKeyLine{
              source, first_line + i, uKey,
              std::vector<std::string>(tokens.begin() + 1, tokens.end())});
        continue;
      }
      if (uKey == "CONFIG") {
        // CONFIG is a special case; check for circular dependency: a file
        // whose lines are being read, the file of the parse among them, is
        // refused before a line of it is read again.
        const std::string configLine =
            "CONFIG on " + lineOf(first_line + i, source);
        if (beingRead(val)) {
          std::string chainStr;
          for (const auto &c : configChain) {
            chainStr += c + " -> ";
          }
          chainStr += val;
          throw std::runtime_error(
              "Circular dependency detected between CONFIG files: " +
              chainStr + " (" + configLine + ")");
        }
        state.includedBy.push_back(configLine);

        // The included lines, read at this line into the same context,
        // under the same refusal and rule: their keys add to the lines
        // read so far, and their structured blocks follow the declarations
        // made so far. A file that does not open is named with every CONFIG
        // line that includes it.
        try {
          parseFile(val, ctx, state, refused, skip, includedByText(state));
        } catch (...) {
          state.includedBy.pop_back();
          throw;
        }
        state.includedBy.pop_back();
        continue; // Skip adding "CONFIG" key or its value to the context
      }
      // Other keys store their values. A key of one value is set by one
      // line of the configuration: a second line is an error naming both.
      // A key of several values adds the values of each of its lines, up
      // to the number it takes (value_N; in a configuration file of a
      // command, the command's): more is an error at the line that exceeds
      // it, naming the lines before it.
      const std::size_t keyMost =
          static_cast<std::size_t>(std::abs(keyInfo.value_N.at(0)));
      const auto own = commandCounts.find(uKey);
      const std::size_t most =
          own == commandCounts.end()
              ? keyMost
              : static_cast<std::size_t>(std::abs(own->second));
      const std::string here = where(first_line + i, source, state);
      const std::vector<KeyLine> &before = state.lines[uKey];
      const std::size_t given =
          (ctx.has_stored(uKey) ? ctx.size(uKey) : 0) + tokens.size() - 1;
      if (most == 1 && !before.empty())
        throw std::runtime_error(
            "Key " + uKey + " is set on " + before.front().where + " and on " +
            here +
            ": a key of one value is set once in a configuration file and "
            "the files it includes with CONFIG" +
            copyAdvice(before, potential));
      if (given > most)
        throw std::runtime_error(
            "Too many values for key " + uKey + " on " + here + ": " + uKey +
            (most == keyMost ? std::string()
                             : " of " + task_command_words(refused.command)) +
            " takes at most " + std::to_string(most) +
            ", and its lines read so far give " + std::to_string(given) +
            (before.empty() ? std::string()
                            : ", with " + linesText(before) + " before it") +
            copyAdvice(before, potential));
      for (size_t j = 1; j < tokens.size(); ++j) {
        ctx.add(key, tokens[j]);
      }
      state.lines[uKey].push_back(KeyLine{here, potential, source,
                                          tokens.size() - 1, first_line + i,
                                          ordinal[i]});
    }
  }

  /**
   * @brief Reads the lines of the file @p filename into @p ctx, under the
   *        refusal @p refused and the rule @p skip (a top-level parse, or
   *        the file of the CONFIG line @p includedAt, "CONFIG on line 2 of
   *        config.train", which the error of a file that does not open
   *        names: "Failed to open config file: <file> (<includedAt>)").
   */
  void parseFile(const std::string &filename, Context &ctx, ParseState &state,
                 const Refusal &refused, const LineSkip &skip,
                 const std::string &includedAt) {
    ParseScope scope;

    std::ifstream infile(filename.c_str());
    if (!infile.is_open()) {
      throw std::runtime_error(
          "Failed to open config file: " + filename +
          (includedAt.empty() ? std::string() : " (" + includedAt + ")"));
    }

    std::vector<std::string> lines;
    {
      std::string line;
      while (std::getline(infile, line)) {
        lines.push_back(line);
      }
    }
    infile.close();

    // The file is among the files being read while its lines are read, so
    // that a CONFIG line naming it is refused as a circular include.
    const ReadingFile reading(filename);
    parseLines(lines, ctx, state, 1, filename, refused, skip);
  }
  /**
   * @brief The end of a public parse: one notice for the retired lines it
   *        read (retired_keys_notice()), when it is a top-level parse (a
   *        parse that a parse in progress on this thread runs gives none:
   *        its lines are those of the outer parse).
   */
  Context finishParse(Context ctx) {
    if (parseDepth == 0) {
      const std::vector<RetiredKeyLine> retired = ctx.retired_keys();
      if (!retired.empty()) report_key_notice(retired_keys_notice(retired));
    }
    return ctx;
  }
} // end anonymous namespace

const std::vector<std::string> &run_setting_keys() {
  static const std::vector<std::string> keys = {
      "VALIDATION", "HPOTARGET", "CHECK",        "VERBOSE",   "NUMERIC",
      "OUTFILE",    "AUDIT",     "AUDIT_SAMPLE", "HEALTH_LOG"};
  return keys;
}

Context CoreUserConfigParser::parse(const std::string &filename) const {
  Context ctx;
  ParseState state;
  parseFile(filename, ctx, state, Refusal{refused_, command_}, LineSkip());
  ctx.set_key_lines(std::move(state.lines));
  return finishParse(std::move(ctx));
}

Context CoreUserConfigParser::parse(const std::string &filename,
                                    const KeyLineRule &skip,
                                    std::vector<SkippedKeyLine> &skipped) const {
  Context ctx;
  ParseState state;
  parseFile(filename, ctx, state, Refusal{refused_, command_},
            skip ? LineSkip{&skip, &skipped} : LineSkip());
  ctx.set_key_lines(std::move(state.lines));
  return finishParse(std::move(ctx));
}

Context CoreUserConfigParser::parse(std::istream &in) const {
  Context ctx;
  {
    ParseScope scope;
    ParseState state;
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line)) lines.push_back(line);
    parseLines(lines, ctx, state, 1, "", Refusal{refused_, command_});
    ctx.set_key_lines(std::move(state.lines));
  }
  return finishParse(std::move(ctx));
}

Context
CoreUserConfigParser::parse(const std::vector<std::string> &lines) const {
  Context ctx;
  {
    ParseScope scope;
    ParseState state;
    parseLines(lines, ctx, state, 1, "", Refusal{refused_, command_});
    ctx.set_key_lines(std::move(state.lines));
  }
  return finishParse(std::move(ctx));
}

void CoreUserConfigParser::refuse_keys(
    std::map<std::string, std::string> reasons) {
  refused_.clear();
  for (auto &kv : reasons)
    refused_[to_upper(kv.first)] = std::move(kv.second);
}

void CoreUserConfigParser::set_command(const std::string &task) {
  if (!task.empty() &&
      BlueprintParser::getTasksMapRef().count(task) == 0)
    throw std::invalid_argument(
        "CoreUserConfigParser::set_command: no blueprint command '" + task +
        "'");
  command_ = task;
}

bool CoreUserConfigParser::validateWithBlueprint(
    const std::string &key,
    const std::string &val,
    const BlueprintKey &keyInfo)
{
  return validateWithBlueprintImpl(key, val, keyInfo);
}

bool CoreUserConfigParser::validateWithBlueprint(
    const std::string &key,
    const std::vector<std::string> &vals,
    const BlueprintKey &keyInfo)
{
  std::string concatenated = core::concat(vals);
  return validateWithBlueprintImpl(key, concatenated, keyInfo);
}

bool CoreUserConfigParser::validateWithBlueprintImpl(
    const std::string &key,
    const std::string &text,
    const BlueprintKey &keyInfo)
{
  // 1. Validate against the key's schema, then its allowed values
  //    (blueprint field `allowed_values`, matched without regard to case).
  if (validateSchemaOnly(text, keyInfo)) {
    if (keyInfo.isAllowedValue(trim(text))) {
      return true;
    }
    Logger::instance().log(
        LogLevel::ERROR,
        "CoreUserConfigParser::parse -> Invalid value for key: ", key, ": ",
        text);
    Logger::instance().log(LogLevel::ERROR, "  Allowed values: ",
                           joinAllowed(keyInfo.allowed_values));
    return false;
  }

  // 3. If validation fails, log details once:
  Logger::instance().log(
      LogLevel::ERROR,
      "CoreUserConfigParser::parse -> Invalid format for key: value -> ",
      key, ": ", text);

  // The key's schema, format and examples, one Logger line each.
  for (const std::string &line : valueHelp(key, keyInfo)) {
    Logger::instance().log(LogLevel::ERROR, line);
  }

  // 4. Indicate failure
  return false;
}

namespace {
bool isBlankChar(char c) {
  return std::isspace(static_cast<unsigned char>(c)) != 0;
}
bool isDigitChar(char c) { return c >= '0' && c <= '9'; }

/* Whether text[i] is a '+' joined to the character before it, which no
 * value of a configuration line holds: not a sign after a blank or a comma,
 * nor an exponent's sign (an e or an E after a digit or a point, with a
 * digit after the '+': 1e+5, 2.5E+3). */
bool joiningPlus(const std::string &text, std::size_t i) {
  if (text[i] != '+' || i == 0) return false;
  const char before = text[i - 1];
  if (isBlankChar(before) || before == ',') return false;
  const bool exponent =
      (before == 'e' || before == 'E') && i >= 2 &&
      (isDigitChar(text[i - 2]) || text[i - 2] == '.') && i + 1 < text.size() &&
      isDigitChar(text[i + 1]);
  return !exponent;
}

/* Whether text holds a comma or a joining '+'. */
bool holdsSeparator(const std::string &text) {
  for (std::size_t i = 0; i < text.size(); ++i)
    if (text[i] == ',' || joiningPlus(text, i)) return true;
  return false;
}
}  // namespace

bool CoreUserConfigParser::validateSchemaOnly(const std::string &text,
                                              const BlueprintKey &keyInfo)
{
  // A name holds no ',' and no '+': there they separate two names.
  if (keyInfo.names && holdsSeparator(text)) return false;
  BlueprintValidator validator(keyInfo.value_type);
  return validator.validate(text);
}

std::string CoreUserConfigParser::misusedSeparators(const std::string &text,
                                                    const BlueprintKey &keyInfo,
                                                    std::string &respaced)
{
  struct Attempt {
    bool commas, pluses;
    const char *name;
  };
  const Attempt attempts[] = {{true, false, "','"},
                              {false, true, "'+'"},
                              {true, true, "',' and '+'"}};
  for (const Attempt &a : attempts) {
    std::string candidate = text;
    bool replaced_comma = false, replaced_plus = false;
    for (std::size_t i = 0; i < text.size(); ++i) {
      if (a.commas && text[i] == ',') {
        candidate[i] = ' ';
        replaced_comma = true;
      } else if (a.pluses && joiningPlus(text, i)) {
        candidate[i] = ' ';
        replaced_plus = true;
      }
    }
    if (replaced_comma != a.commas || replaced_plus != a.pluses) continue;
    if (!validateSchemaOnly(candidate, keyInfo) ||
        !keyInfo.isAllowedValue(trim(candidate)))
      continue;
    std::istringstream words(candidate);
    std::string word, joined;
    while (words >> word) joined += (joined.empty() ? "" : " ") + word;
    respaced = joined;
    return a.name;
  }
  return "";
}

} // namespace core
} // namespace tadah
