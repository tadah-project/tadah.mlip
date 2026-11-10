#ifndef TADAH_CORE_CORE_USER_CONFIG_PARSER_H
#define TADAH_CORE_CORE_USER_CONFIG_PARSER_H

#include <tadah/core/iuser_config_parser.h>

#include <map>
#include <string>

namespace tadah {
namespace core {

/**
 * @brief The keys that set up a run, never a model: VALIDATION and
 *        HPOTARGET (the validation and target files of tadah hpo), CHECK
 *        (tadah hpo --check: the startup checks of the run alone), VERBOSE
 *        (the verbosity of the run: storing it sets the Logger level),
 *        NUMERIC and OUTFILE (the writer's significant digits and output
 *        file), AUDIT and AUDIT_SAMPLE (the pre-flight audit) and HEALTH_LOG
 *        (the health monitor of tadah hpo).
 *
 * A potential -- a file whose own key lines give WEIGHTS -- sets none of
 * them: CoreUserConfigParser checks its lines of them as
 * any line and does not store them, in every parse, so that a program that
 * loads a potential (tadah predict, pair_style tadah, refit --edit, a
 * retrain or tadah hpo -c of a copy of it) runs with its own settings.
 * tadah writes none of them as a key line (a potential's training record,
 * a comment block, lists the run's); a potential of an earlier version has
 * some (every 1.5.x best_pot.tadah has HPOTARGET, VERBOSE and NUMERIC, and
 * VALIDATION when its run had a validation file).
 */
const std::vector<std::string> &run_setting_keys();

/**
 * @brief Default “dumb” parser that reads key/value pairs from configuration input.
 *
 * This implementation assumes that the configuration file contains one key/value pair per line
 * (ignoring blank lines and comments). It validates every key against the allowed keywords
 * provided by the blueprint (via Context): a key that is none is an error at its line ("Invalid
 * key FOOBAR on line 3 of config.train (closest key: FORMAT)"; "on line 3" for lines of no
 * file), a line of a file included with CONFIG named with the CONFIG lines that include it
 * ("Invalid key FOOBAR on line 1 of inc.cfg (CONFIG on line 2 of config.train) (closest key:
 * FORMAT)"). So is every other error of a line, without a Logger line: a value the key does
 * not take ("Invalid value for key RCUT2B on line 3 of config.train: 'abc'", the key's schema,
 * format and examples on the lines that follow; a value outside its allowed values lists them),
 * more values than the key takes over its lines, with the lines before it ("Too many values for
 * key MODEL on line 4 of config.train: MODEL takes at most 3, and its lines read so far give 4,
 * with line 1 of config.train before it"), a second line of a key of one value ("Key BIAS is
 * set on line 1 of config.train and on line 2 of inc.cfg (CONFIG on line 3 of config.train): a
 * key of one value is set once in a configuration file and the files it includes with
 * CONFIG"), both ending, when the key's lines are a potential's and another file's, with the
 * way to change a potential's value ("; a potential's value is changed by editing its line in a
 * copy of the potential (the Training page of the documentation, Retraining from a
 * potential)"), a block that is not closed ("OPTIMIZER on line 2 of config.train is not closed;
 * expected ENDOPTIMIZER") and a CONFIG line whose file does not open ("Failed to open config
 * file: other.cfg (CONFIG on line 2 of config.train)"; in a nested include every CONFIG line
 * that includes it, "(CONFIG on line 1 of mid.cfg, which CONFIG on line 2 of config.train
 * includes)") or names a file whose lines are being read,
 * the file of the parse among them, whatever path names it ("Circular dependency detected
 * between CONFIG files: config.train -> inc.cfg -> config.train (CONFIG on line 1 of
 * inc.cfg)"), refused before a line of that file is read again.
 *
 * A configuration is the concatenation of its lines: a CONFIG line reads the lines of the file
 * it names at its place, into the same Context (a nested CONFIG line likewise; the path is
 * opened as given, relative to the working directory). A key of one value (|value_N[0]| 1) is
 * set by one line of the configuration: a second line of it, in the same file or in another,
 * above or below the CONFIG line, is refused naming both lines, each with its file and, for a
 * line of an included file, the CONFIG lines that include it ("line 2 of deep.cfg (CONFIG on
 * line 3 of mid.cfg, which CONFIG on line 2 of config.train includes)"); a key of several
 * values takes the values of its lines in line order, up to the number it takes, and the
 * structured blocks follow in line order too. A line a parse does not store (a deprecated or
 * retired key, a run setting of a potential, a line of a key of another command in a
 * potential, a line the caller's KeyLineRule selects) sets nothing.
 *
 * The returned Context records the lines that gave each key its values (Context::key_lines()),
 * in line order: where each line is, as its errors name it, and whether it is a line of a
 * potential, a file whose own key lines give WEIGHTS. A reader of a key tells the values of a
 * potential's own lines from those of the lines of a file that includes it with CONFIG (above
 * or below the CONFIG line) by them, and names the lines in its errors (D_mJoin: the RCTYPE,
 * RCUT and list values of a potential that its descriptors do not take).
 *
 * A key the blueprint marks as deprecated (BlueprintKey::deprecated) is accepted so that older
 * configuration and potential files still load: its value is validated like any other, a
 * notice naming the key and the reason (`WARNING: Deprecated key MBLOCK is ignored: ...`) is
 * written to standard error once per key and process (report_ignored_key_once(), key_notice.h:
 * however many lines, CONFIG files or parses read it), and the key is not stored in the
 * returned Context. The notice bypasses the Logger, so neither the Logger
 * level nor VERBOSE suppresses it: pair_style tadah in LAMMPS, which leaves the Logger at
 * ERROR, shows it too.
 *
 * A key the blueprint marks as retired (BlueprintKey::isRetired()) is accepted so that a
 * potential that carries it still loads: its value is validated, the line is recorded on the
 * returned Context (Context::retired_keys(): never a stored key, never written back, carried
 * into the Context of a file that includes it with CONFIG), and each parse that read some
 * gives one notice that lists them (retired_keys_notice(), through report_key_notice(): on
 * standard error, or to the IgnoredKeyNoticeCapture of pair_style tadah). A fit refuses a
 * Context that records one, with the conversion (require_no_retired_keys()).
 *
 * A line of a key of run_setting_keys() in a file whose own key lines give
 * WEIGHTS -- a potential -- is checked and not stored, whatever
 * the parse (no Logger level is set by a potential's VERBOSE line, the
 * program that loads the potential keeps its own); the overloads with a
 * KeyLineRule list it in their @c skipped list as a line the rule selects.
 * A file a CONFIG line includes is judged by its own lines: the lines of a
 * configuration file that includes a potential are stored as it gives them.
 */
class CoreUserConfigParser : public IUserConfigParser {
public:
  CoreUserConfigParser() = default;
  virtual ~CoreUserConfigParser() = default;

  /**
   * @brief Parses configuration from a file.
   * @param filename The configuration file path.
   * @return A Context containing all key/value pairs.
   * @throws std::runtime_error if any key is not allowed or a parsing error occurs.
   */
  Context parse(const std::string &filename) const override;

  /**
   * @brief Parses configuration from an input stream.
   * @param in The input stream.
   * @return A Context containing all key/value pairs.
   * @throws std::runtime_error if any key is not allowed or a parsing error occurs.
   */
  Context parse(std::istream &in) const override;

  /**
   * @brief Parses configuration from a vector of strings.
   * @param lines The vector of configuration lines.
   * @return A Context containing all key/value pairs.
   * @throws std::runtime_error if any key is not allowed or a parsing error occurs.
   */
  Context parse(const std::vector<std::string>& lines) const override;

  /**
   * @brief parse(filename) that does not store the key lines @p skip
   *        selects, and lists them in @p skipped.
   *
   * Every line is read and checked as parse(filename) reads it: an unknown
   * key, an invalid value or a refused key is an error at its line. A key
   * line (not a CONFIG line, not a block) for which @p skip(file_keys,
   * key) is true, or that no parse stores (a run setting of a potential),
   * is appended to @p skipped instead, in the order read; so is a line of a
   * retired key that @p skip selects, which is then not recorded on the
   * result (Context::retired_keys()) and not named in the parse's notice. A
   * file a CONFIG line includes is read under the same rule, with the keys
   * of its own lines.
   */
  Context parse(const std::string &filename, const KeyLineRule& skip,
                std::vector<SkippedKeyLine>& skipped) const;

  /**
   * @brief Refuses the keys of @p reasons (key -> why) in everything this
   *        parser reads.
   *
   * A line that sets one of them, in any letter case, is an error at that
   * line: "Key FORCE on line 2 of val.tadah is not accepted here: <reason>".
   * A file included with CONFIG is read under the same refusal, so its
   * error names the included file and its line. A later call replaces the
   * set; an empty map refuses nothing (the default).
   *
   * For a file whose role leaves no meaning to a key that is valid
   * elsewhere: HPO's --validation file refuses every key but DBFILE,
   * EFILTER, FFILTER and CONFIG (FORCE and STRESS among them, since what
   * validation predicts follows PC_FRMSE and PC_SRMSE_GPA).
   */
  void refuse_keys(std::map<std::string, std::string> reasons);

  /**
   * @brief The command whose configuration file this parser reads, by its
   *        blueprint task name ("TRAIN", "HPO", "ANALYSIS.check_bounds";
   *        empty: none, the default).
   *
   * With a command, a line of a key that only other commands read
   * (BlueprintKey::command_only: an option of their command line, or a key
   * of their configuration file) is an error at that line, in the file and
   * in the files it includes with CONFIG: "Key ANALYTICS on line 3 of
   * config.train is not accepted here: ANALYTICS is the option --analytics
   * (-A) of tadah predict, not a key of a configuration file of tadah
   * train"; a key the command reads (REFIT_MODE in a file of tadah refit)
   * is read. Every parse, with a command or without, reads a potential (a
   * file whose own lines give WEIGHTS) without such a line, with one notice
   * per key and process (report_ignored_key_once(): "WARNING: Key ANALYTICS
   * is ignored: ANALYTICS true on line 40 of pot.tadah is the option
   * --analytics (-A) of tadah predict, not a key of a potential; the line is
   * not read"), so that a potential written before 1.6.0, whose training
   * configuration held such a key, still loads.
   *
   * With a command, a key of the command's table takes the number of
   * values that table gives it: tadah train, tadah refit and tadah analysis
   * bfunc, suggest_bounds and dataset_stats take one OUTFILE value (each
   * writes one file), although OUTFILE takes any number, so a second
   * OUTFILE line is refused as a second line of a key of one value, and a
   * line of two values as too many ("Too many values for key OUTFILE on
   * line 1 of config.train: OUTFILE of tadah train takes at most 1, and its
   * lines read so far give 2").
   * @throws std::invalid_argument for a name that is no blueprint task.
   */
  void set_command(const std::string &task);

  /**
   * @brief Validate a (key, value) pair against Blueprint schema (the
   *        values of a command-line option; a parse checks the values of
   *        its lines itself, and names the line in its error).
   *
   * Logs any errors if the value does not comply with the schema for this key
   * (including showing allowed schemas, formats, and examples).
   *
   * @param key         The user-supplied key.
   * @param val         The raw, trimmed value string to validate.
   * @param keyInfo     The BlueprintKey struct containing the schema,
   *                    formats, and examples for this key.
   * @return            True if validation passes, otherwise false (and logs why).
   */
  static bool validateWithBlueprint(
      const std::string &key,
      const std::string &val,
      const BlueprintKey &keyInfo);

  /**
   * @brief Validates a vector of string values against the blueprint.
   *  
   *  The same logic as validateWithBlueprint, but for a vector of values.
   */
  static bool validateWithBlueprint(
      const std::string &key,
      const std::vector<std::string> &vals,
      const BlueprintKey &keyInfo);

  /**
   * @brief Whether @p text matches the key's value_type schema (the
   *        BlueprintValidator), without the allowed_values check; no log.
   *        A value of a names key (BlueprintKey::names) that holds a comma,
   *        or a '+' joined to the character before it (see
   *        misusedSeparators), does not.
   */
  static bool validateSchemaOnly(const std::string &text,
                                 const BlueprintKey &keyInfo);

  /**
   * @brief The separators that join the values of @p text where only
   *        blanks separate values: "','", "'+'" or "',' and '+'", or ""
   *        when there are none.
   *
   * A configuration line (and a command-line option) is split into values
   * at blanks only, and its key's types take a comma or a '+' between two
   * values nowhere but inside the STRING word of a key that is not a names
   * key (a path, an option string) and between the entries of an index
   * list. For a @p text that fails the key's types (validateSchemaOnly),
   * this returns the separator(s) whose replacement by blanks makes it a
   * value of the key (types and allowed values): every comma, every '+'
   * joined to the character before it (not after a blank or a comma, nor
   * the sign of an exponent: an e or an E after a digit or a point, with a
   * digit after the '+', as in 1e+5), or both, tried in that order;
   * @p respaced is then that text with its words joined by one blank
   * ("0.1,0.2" gives "','" and "0.1 0.2"; "Fe+Fe" gives "'+'" and
   * "Fe Fe"). An empty result (and @p respaced untouched) when @p text holds
   * neither, or when blanks would not make it valid either.
   */
  static std::string misusedSeparators(const std::string &text,
                                       const BlueprintKey &keyInfo,
                                       std::string &respaced);

private:
  /**
   * @brief Internal helper that actually calls the BlueprintValidator and logs on failure.
   *
   * @param key        The user’s key (for logging)
   * @param text       The concatenated or original text to validate
   * @param keyInfo    The blueprint specification (schema, format, examples, etc.)
   * @return           true if validation passes, false otherwise (and logs).
   *                   A value outside the key's allowed_values fails too.
   */
  static bool validateWithBlueprintImpl(
      const std::string &key,
      const std::string &text,
      const BlueprintKey &keyInfo);

  /// Upper-case key -> the reason it is refused (refuse_keys()).
  std::map<std::string, std::string> refused_;
  /// The blueprint task whose configuration file this parser reads
  /// (set_command()); empty: none.
  std::string command_;
};
} // namespace core
} // namespace tadah

#endif // TADAH_CORE_CORE_USER_CONFIG_PARSER_H

