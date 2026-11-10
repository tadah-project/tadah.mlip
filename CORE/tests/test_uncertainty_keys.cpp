// The keys and options of uncertainty output, which is removed: the errors
// of Tadah! 1.0.0 to 1.5.0 were not standard deviations, and no calibrated
// estimate replaces them.
//
//  - WCOV and WNOISE (keys that no release of Tadah! wrote or read),
//    UNCERTAINTY (tadah train
//    --uncertainty; tadah train 1.0.0 to 1.5.0 wrote an UNCERTAINTY line,
//    true or false, into every potential) and ERROR (the key of --error)
//    are retired keys: a file with them parses, with one notice per parse,
//    the lines are recorded and never stored, and a fit refuses them
//    (retired_keys.h), each line marked as converted by deleting it and
//    each key with its reason, as nothing replaces it.
//  - --error is a retired option of tadah predict and of tadah properties
//    pairwise (the blueprint field `retired` of the task's key, with a
//    reason of its own): those tasks refuse it.
//  - SIGMA, the matrix Tadah! 1.0.0 to 1.5.0 wrote for an OALGO 3 fit,
//    stays a deprecated key: accepted so that those potentials still load,
//    with one notice, and never stored.
//
// ctest runs every test case in a process of its own, where no deprecated
// key has been reported yet; a case counts a notice only when
// core::ignored_key_reported() says the key was not reported before it.

#include "catch2/catch.hpp"

#include <tadah/core/blueprint_parser.h>
#include <tadah/core/context.h>
#include <tadah/core/core_user_config_parser.h>
#include <tadah/core/key_notice.h>
#include <tadah/core/logger.h>
#include <tadah/core/retired_keys.h>

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace tadah::core;
using Catch::Matchers::Contains;
using Catch::Matchers::StartsWith;

namespace {

/* The sentence every retired key and option of uncertainty output gives as
 * the reason it is retired: a statement of fact, with no version in which
 * uncertainty output is absent. */
const std::string kRemoved =
    "Uncertainty output is removed: the errors of Tadah! 1.0.0 to 1.5.0 "
    "were not standard deviations, and no calibrated estimate replaces "
    "them.";

/* Captures std::cerr (where the deprecation notice goes) and std::cout
 * (the Logger) at WARNING level; restores them afterwards. */
class StreamCapture {
public:
  StreamCapture()
      : old_level_(Logger::instance().getLogLevel()),
        old_out_(std::cout.rdbuf(out_.rdbuf())),
        old_err_(std::cerr.rdbuf(err_.rdbuf())) {
    Logger::instance().setLogLevel(LogLevel::WARNING);
  }
  ~StreamCapture() {
    std::cout.rdbuf(old_out_);
    std::cerr.rdbuf(old_err_);
    Logger::instance().setLogLevel(old_level_);
  }
  StreamCapture(const StreamCapture &) = delete;
  StreamCapture &operator=(const StreamCapture &) = delete;

  std::vector<std::string> err_lines_with(const std::string &needle) const {
    std::vector<std::string> out;
    std::istringstream in(err_.str());
    for (std::string line; std::getline(in, line);)
      if (line.find(needle) != std::string::npos) out.push_back(line);
    return out;
  }

private:
  std::ostringstream out_;
  std::ostringstream err_;
  LogLevel old_level_;
  std::streambuf *old_out_;
  std::streambuf *old_err_;
};

std::string serialized(const Context &c) {
  std::ostringstream os;
  os << c;
  return os.str();
}

std::vector<std::string> key_line(const std::string &text,
                                  const std::string &key) {
  std::istringstream in(text);
  for (std::string line; std::getline(in, line);) {
    std::istringstream ls(line);
    std::string first;
    if ((ls >> first) && first == key) {
      std::vector<std::string> t;
      for (std::string w; ls >> w;) t.push_back(w);
      return t;
    }
  }
  return {};
}

std::string refusal(const std::vector<std::string> &lines) {
  try {
    CoreUserConfigParser().parse(lines);
  } catch (const std::exception &e) {
    return e.what();
  }
  return "<no exception>";
}

std::vector<std::string> keys_of(const std::vector<RetiredKeyLine> &lines) {
  std::vector<std::string> out;
  for (const auto &l : lines) out.push_back(l.key);
  return out;
}

/* The option @p name ("error") of the blueprint task @p task ("PREDICT"),
 * or nullptr. */
const BlueprintKey *task_option(const std::string &task,
                                const std::string &name) {
  const auto &t = BlueprintParser::getTasksMapRef().at(task);
  const std::string full = task + "." + name;
  for (const BlueprintKey &k : t.blueprintKeys)
    if (k.keyName == full) return &k;
  for (const auto &g : t.groups)
    for (const BlueprintKey &k : g.blueprintKeys)
      if (k.keyName == full) return &k;
  return nullptr;
}

/* The checks of a retired key or option of uncertainty output: why (the
 * output it gave, @p what, and kRemoved), with no claim that the output it
 * gave was a standard deviation (neither in the reason nor in the
 * description), that nothing replaces it ("nothing"), its conversion
 * (deleting it; the fit, and the predictions, unaffected), no version of
 * Tadah! that lacks it (no "1.6.0" in the description, the long
 * description, the reason or the replacement), and no default or
 * example. */
void check_retired_output(const BlueprintKey &k, const std::string &what) {
  INFO(k.keyName);
  REQUIRE(k.isRetired());
  CHECK_FALSE(k.isDeprecated());
  CHECK_THAT(k.retired.reason, Contains(what) && Contains(kRemoved));
  const std::string gave =
      k.retired.reason.substr(0, k.retired.reason.find(kRemoved));
  CHECK(gave.find("standard deviation") == std::string::npos);
  CHECK(k.description.find("standard deviation") == std::string::npos);
  CHECK(k.retired.reason.find("too narrow") == std::string::npos);
  CHECK(k.retired.replacement == "nothing");
  for (const std::string *text :
       {&k.description, &k.longDescription, &k.retired.reason,
        &k.retired.replacement}) {
    INFO(*text);
    CHECK(text->find("1.6.0") == std::string::npos);
  }
  CHECK_THAT(k.retired.conversion, StartsWith("Deleting the") &&
                                       Contains("unaffected"));
  CHECK(k.default_value.empty());
  CHECK(k.examples.empty());
}

}  // namespace

/* Pins: WCOV, WNOISE, UNCERTAINTY and ERROR are retired keys, and --error
 * a retired option of tadah predict and of tadah properties pairwise
 * (PREDICT.error, PROPERTIES.pairwise.error, each with a reason of its
 * own): each says the output it gave, which was no standard deviation
 * (UNCERTAINTY: "tadah train --uncertainty wrote weights.tadah, each
 * coefficient of the fit beside a diagonal entry of SIGMA"; --error of
 * predict: "wrote an error computed from SIGMA beside every predicted
 * value"), and that uncertainty output is removed, the errors of Tadah!
 * 1.0.0 to 1.5.0 not being standard deviations and no calibrated estimate
 * replacing them; nothing replaces it ("nothing"), and deleting it
 * converts it, the fit (and the predictions) unaffected; no text names a
 * version of Tadah! without uncertainty output; none has a default or an
 * example. The four keys are converted by deleting their
 * lines (the field by_deletion of `retired`). The command-line options
 * stay, to give the reason: --uncertainty of UNCERTAINTY, --error of the
 * task options and of ERROR. UNCERTAINTY's help says that tadah train of
 * Tadah! 1.0.0 to 1.5.0 wrote the line into every potential.
 * Before the fix: the reasons said that the options gave standard
 * deviations, the error bars of which were too narrow for many structures
 * (what a development build gave, not a release), ERROR was a key of a
 * configuration (written into the potential of a fit, read by nothing),
 * and the help of UNCERTAINTY named tadah train 1.5.0 alone; then each
 * said that uncertainty output was not part of Tadah! 1.6.0, its
 * replacement "nothing in Tadah! 1.6.0", and each description that
 * Tadah! 1.6.0 did not give it; and WCOV and WNOISE said that each held a
 * quantity for the uncertainty output of tadah predict --error, which no
 * release wrote (WCOV and WNOISE say that no release wrote or read them).
 */
TEST_CASE("Blueprint: uncertainty output is retired: WCOV, WNOISE, "
          "UNCERTAINTY, ERROR and --error of predict and properties "
          "pairwise",
          "[BlueprintParser][retired][uncertainty]") {
  const auto &keys = BlueprintParser::getKeysMapRef();
  check_retired_output(keys.at("WCOV"),
                       "No release of Tadah! wrote or read WCOV.");
  check_retired_output(keys.at("WNOISE"),
                       "No release of Tadah! wrote or read WNOISE.");
  check_retired_output(keys.at("UNCERTAINTY"),
                       "tadah train --uncertainty wrote weights.tadah, each "
                       "coefficient of the fit beside a diagonal entry of "
                       "SIGMA.");
  CHECK(keys.at("UNCERTAINTY").cli_long_flag == "--uncertainty");
  CHECK_THAT(keys.at("UNCERTAINTY").longDescription,
             Contains("tadah train of Tadah! 1.0.0 to 1.5.0 wrote one into "
                      "every potential"));
  check_retired_output(keys.at("ERROR"),
                       "ERROR, the key of --error, asked tadah predict and "
                       "tadah properties pairwise for errors computed from "
                       "SIGMA.");
  CHECK(keys.at("ERROR").cli_long_flag == "--error");
  for (const char *key : {"WCOV", "WNOISE", "UNCERTAINTY", "ERROR"}) {
    INFO(key);
    CHECK(keys.at(key).retired.by_deletion);
  }

  const BlueprintKey *predict = task_option("PREDICT", "error");
  REQUIRE(predict != nullptr);
  check_retired_output(*predict,
                       "tadah predict --error wrote an error computed from "
                       "SIGMA beside every predicted value (some of the "
                       "errors negative).");
  CHECK_THAT(predict->retired.conversion, Contains("predictions"));
  CHECK(predict->cli_long_flag == "--error");

  const BlueprintKey *pairwise = task_option("PROPERTIES.pairwise", "error");
  REQUIRE(pairwise != nullptr);
  check_retired_output(*pairwise,
                       "tadah properties pairwise --error wrote an error "
                       "computed from SIGMA beside every value of the "
                       "energy curve.");
  CHECK(pairwise->cli_long_flag == "--error");

  const BlueprintKey *train = task_option("TRAIN", "uncertainty");
  REQUIRE(train != nullptr);
  CHECK(train->isRetired());
}

/* Pins: a potential with WCOV (N = 2, then the 2 x 2 matrix) and WNOISE
 * (row type and noise variance pairs), as a development build of Tadah!
 * 1.6.0 wrote for an OALGO 3 fit, parses: both lines are recorded
 * (Context::retired_keys(), with their values) and neither is stored, a
 * Context written with operator<< has neither line, and the parse gives
 * one notice, "Retired keys WCOV (line 2) and WNOISE (line 3) are
 * ignored by a prediction, as they change nothing it gives, and refused by
 * a fit (tadah explain WCOV and tadah explain WNOISE give the reasons and
 * the conversions)": in a fit's configuration, as in a prediction's, the
 * notice says what each does.
 * Before the fix: both keys were stored as written, with no notice; then
 * the notice said "are ignored: ... a fit refuses them", and a fit's
 * refusal followed it. */
TEST_CASE("Config parser: WCOV and WNOISE are retired: recorded, not "
          "stored, one notice",
          "[config][retired][uncertainty]") {
  IgnoredKeyNoticeCapture cap;
  Context c;
  REQUIRE_NOTHROW(c = CoreUserConfigParser().parse(std::vector<std::string>{
                      "ATOM Ta", "WCOV 2 0.5 0.1 0.1 0.25",
                      "WNOISE E 0.04 F 0.0025 S 1e-6"}));
  CHECK_FALSE(c.has_stored("WCOV"));
  CHECK_FALSE(c.has_stored("WNOISE"));
  CHECK(c.get<std::string>("ATOM") == "Ta");
  const std::vector<RetiredKeyLine> r = c.retired_keys();
  REQUIRE(keys_of(r) == (std::vector<std::string>{"WCOV", "WNOISE"}));
  CHECK(r[0].line == 2);
  CHECK(r[0].values ==
        (std::vector<std::string>{"2", "0.5", "0.1", "0.1", "0.25"}));
  CHECK(r[1].values == (std::vector<std::string>{"E", "0.04", "F", "0.0025",
                                                 "S", "1e-6"}));
  const std::string pot = serialized(c);
  INFO(pot);
  CHECK(key_line(pot, "WCOV").empty());
  CHECK(key_line(pot, "WNOISE").empty());
  REQUIRE(cap.notices().size() == 1);
  CHECK(cap.notices()[0] ==
        "Retired keys WCOV (line 2) and WNOISE (line 3) are ignored by a "
        "prediction, as they change nothing it gives, and refused by a fit "
        "(tadah explain WCOV and tadah explain WNOISE give the reasons and "
        "the conversions).");
}

/* Pins: an UNCERTAINTY line (tadah train 1.0.0 to 1.5.0 wrote one, true
 * or false, into every potential) parses as a retired key: recorded, not
 * stored, one notice "Retired key UNCERTAINTY (line 2) is
 * ignored by a prediction, as it changes nothing it gives, and refused by
 * a fit (tadah explain UNCERTAINTY gives the reason and the
 * conversion)."; a
 * notice of retired keys of several kinds says no more than that (not
 * that they weighted the training data).
 * Before the fix: UNCERTAINTY was stored, with no notice. */
TEST_CASE("Config parser: UNCERTAINTY is a retired key",
          "[config][retired][uncertainty]") {
  IgnoredKeyNoticeCapture cap;
  Context c;
  REQUIRE_NOTHROW(c = CoreUserConfigParser().parse(std::vector<std::string>{
                      "ATOM Ta", "UNCERTAINTY true"}));
  CHECK_FALSE(c.has_stored("UNCERTAINTY"));
  REQUIRE(keys_of(c.retired_keys()) ==
          std::vector<std::string>{"UNCERTAINTY"});
  REQUIRE(cap.notices().size() == 1);
  CHECK(cap.notices()[0] ==
        "Retired key UNCERTAINTY (line 2) is ignored by a prediction, as it "
        "changes nothing it gives, and refused by a fit (tadah explain "
        "UNCERTAINTY gives the reason and the conversion).");

  CoreUserConfigParser().parse(std::vector<std::string>{
      "EWEIGHT 1", "UNCERTAINTY true", "WCOV 1 0.5"});
  REQUIRE(cap.notices().size() == 2);
  CHECK_THAT(cap.notices()[1],
             StartsWith("Retired keys EWEIGHT (line 1), UNCERTAINTY (line "
                        "2) and WCOV (line 3) are ignored by a prediction, "
                        "as they change nothing it gives"));
  CHECK(cap.notices()[1].find("training data") == std::string::npos);
}

/* Pins: an ERROR line (the key of --error of tadah predict and tadah
 * properties pairwise) in a configuration parses as a retired key:
 * recorded, not stored (a potential written from the Context has no ERROR
 * line), one notice "Retired key ERROR (line 2) is ignored by a
 * prediction, as it changes nothing it gives, and refused by a fit (tadah
 * explain ERROR gives the reason and the conversion)."; Context::add
 * refuses it with the
 * reason.
 * Before the fix: ERROR was stored with no notice, so a fit took it and
 * wrote it into its potential, where nothing read it. */
TEST_CASE("Config parser: ERROR is a retired key",
          "[config][retired][uncertainty]") {
  IgnoredKeyNoticeCapture cap;
  Context c;
  REQUIRE_NOTHROW(c = CoreUserConfigParser().parse(std::vector<std::string>{
                      "ATOM Ta", "ERROR true"}));
  CHECK_FALSE(c.has_stored("ERROR"));
  REQUIRE(keys_of(c.retired_keys()) == std::vector<std::string>{"ERROR"});
  CHECK(key_line(serialized(c), "ERROR").empty());
  REQUIRE(cap.notices().size() == 1);
  CHECK(cap.notices()[0] ==
        "Retired key ERROR (line 2) is ignored by a prediction, as it "
        "changes nothing it gives, and refused by a fit (tadah explain ERROR "
        "gives the reason and the conversion).");
  Context d;
  REQUIRE_THROWS_WITH(d.add("ERROR", true),
                      Contains("ERROR is a retired key") && Contains(kRemoved));
  CHECK_FALSE(d.has_stored("ERROR"));
}

/* Pins: a fit refuses the lines of UNCERTAINTY, WCOV, WNOISE and ERROR,
 * keys that nothing replaces, marking each line as converted by deleting
 * it ("UNCERTAINTY false (line 2; converted by deleting the line)") and
 * giving each key's reason before its conversion ("UNCERTAINTY: tadah
 * train --uncertainty wrote ... Deleting the option, or the UNCERTAINTY
 * line, converts it: ..."), and ends "With these lines deleted (in the
 * file that has them, a potential's included), the configuration trains"
 * ("With this line deleted ..." for one line). Beside a line that a key
 * replaces (EWEIGHT 1), which is not marked and gets no reason, it ends
 * "With the converted keys in place of these lines, or the marked lines
 * deleted (...)".
 * Before the fix: no line was marked, no reason given, and the refusal
 * said to put the converted keys in place of every line, which these keys
 * do not have. */
TEST_CASE("A fit refuses UNCERTAINTY, WCOV, WNOISE and ERROR lines as "
          "converted by deleting them, with the reason",
          "[retired][uncertainty]") {
  IgnoredKeyNoticeCapture quiet;
  const auto &keys = BlueprintParser::getKeysMapRef();
  const std::string mark = "; converted by deleting the line)";
  const Context c = CoreUserConfigParser().parse(std::vector<std::string>{
      "ATOM Ta", "UNCERTAINTY false", "WCOV 1 0.5", "WNOISE E 0.04",
      "ERROR true"});
  const std::string msg = retired_keys_refusal(c.retired_keys(), "tadah train");
  INFO(msg);
  CHECK_THAT(msg, Contains("UNCERTAINTY false (line 2" + mark) &&
                      Contains("WCOV 1 0.5 (line 3" + mark) &&
                      Contains("WNOISE E 0.04 (line 4" + mark) &&
                      Contains("ERROR true (line 5" + mark));
  for (const char *key : {"UNCERTAINTY", "WCOV", "WNOISE", "ERROR"}) {
    INFO(key);
    const RetiredKey &r = keys.at(key).retired;
    CHECK_THAT(msg, Contains(std::string(" ") + key + ": " + r.reason + " " +
                             r.conversion));
  }
  CHECK_THAT(msg, Contains(" With these lines deleted (in the file that has "
                           "them, a potential's included), the configuration "
                           "trains; no coefficients were fitted."));
  CHECK(msg.find("converted keys") == std::string::npos);

  const Context one = CoreUserConfigParser().parse(
      std::vector<std::string>{"UNCERTAINTY true"});
  CHECK_THAT(retired_keys_refusal(one.retired_keys(), "A fit"),
             Contains("UNCERTAINTY true (line 1" + mark) &&
                 Contains(" With this line deleted (in the file that has "
                          "it, a potential's included), the configuration "
                          "trains"));

  const Context mixed = CoreUserConfigParser().parse(
      std::vector<std::string>{"EWEIGHT 1", "UNCERTAINTY false"});
  const std::string m = retired_keys_refusal(mixed.retired_keys(), "A fit");
  INFO(m);
  CHECK_THAT(m, Contains("EWEIGHT 1 (line 1) and UNCERTAINTY false (line 2" +
                         mark) &&
                    Contains(" EWEIGHT: " +
                             keys.at("EWEIGHT").retired.conversion) &&
                    Contains(" With the converted keys in place of these "
                             "lines, or the marked lines deleted (in the "
                             "file that has them"));
  CHECK(m.find(keys.at("EWEIGHT").retired.reason) == std::string::npos);
}

/* Pins: Context::add() refuses WCOV, WNOISE and UNCERTAINTY with the
 * reason ("<KEY> is a retired key: ... Uncertainty output is removed:
 * ..."), and stores nothing.
 * Before the fix: each was stored. */
TEST_CASE("Context::add refuses WCOV, WNOISE and UNCERTAINTY with the reason",
          "[context][retired][uncertainty]") {
  Context c;
  REQUIRE_THROWS_WITH(c.add("WCOV", "2"),
                      Contains("WCOV is a retired key") && Contains(kRemoved));
  REQUIRE_THROWS_WITH(c.add("wnoise", "E"),
                      Contains("WNOISE is a retired key") &&
                          Contains(kRemoved));
  REQUIRE_THROWS_WITH(c.add("UNCERTAINTY", true),
                      Contains("UNCERTAINTY is a retired key") &&
                          Contains(kRemoved));
  CHECK_FALSE(c.has_stored("WCOV"));
  CHECK_FALSE(c.has_stored("WNOISE"));
  CHECK_FALSE(c.has_stored("UNCERTAINTY"));
}

/* Pins: a fit refuses WCOV and WNOISE with the conversion of each
 * (retired_keys_refusal), and names a line of more than six values by its
 * first three and the number of the others: "WCOV 3 1 0 and 7 more values
 * (line 2; converted by deleting the line)" for the ten values of N = 3; a
 * line of six values or fewer is named whole ("WNOISE E 0.04 F 0.0025 S
 * 1e-6 (line 3; converted by deleting the line)").
 * Before the fix: neither was retired, so a fit read none (and a line of
 * a retired key was named with all its values); then neither line was
 * marked. */
TEST_CASE("A fit refuses WCOV and WNOISE, naming a long line by its first "
          "values",
          "[retired][uncertainty]") {
  IgnoredKeyNoticeCapture quiet;
  const Context c = CoreUserConfigParser().parse(std::vector<std::string>{
      "ATOM Ta", "WCOV 3 1 0 0 0 1 0 0 0 1", "WNOISE E 0.04 F 0.0025 S 1e-6"});
  const std::string msg = retired_keys_refusal(c.retired_keys(), "tadah train");
  INFO(msg);
  CHECK_THAT(msg, StartsWith("tadah train: "));
  CHECK_THAT(msg, Contains("WCOV 3 1 0 and 7 more values (line 2; "
                           "converted by deleting the line)") &&
                      Contains("WNOISE E 0.04 F 0.0025 S 1e-6 (line 3; "
                               "converted by deleting the line)"));
  const auto &keys = BlueprintParser::getKeysMapRef();
  CHECK_THAT(msg, Contains(keys.at("WCOV").retired.conversion) &&
                      Contains(keys.at("WNOISE").retired.conversion));
  REQUIRE_THROWS_WITH(require_no_retired_keys(c, "A fit"),
                      StartsWith("A fit: ") && Contains("WCOV 3 1 0"));
}

/* Pins: a potential with a SIGMA line (as Tadah! 1.0.0 to 1.5.0 wrote for
 * an OALGO 3 fit, here "SIGMA 2 1 0 0 1") parses: one WARNING on standard
 * error, "Deprecated key SIGMA is ignored", naming the transposed formula
 * and saying that uncertainty output is removed, without
 * pointing to WCOV or a retrain; the key is not stored (has_stored false)
 * and a potential written from the Context has no SIGMA line; the other
 * keys are kept.
 * Before the fix: the notice said that a retrain with OALGO 3 writes WCOV
 * and WNOISE, which tadah predict --error needs. */
TEST_CASE("Config parser: SIGMA parses, warns once and is not stored",
          "[config][deprecated][uncertainty]") {
  const std::size_t expected = ignored_key_reported("SIGMA") ? 0 : 1;
  StreamCapture cap;
  Context c;
  REQUIRE_NOTHROW(c = CoreUserConfigParser().parse(std::vector<std::string>{
                      "ATOM Ta", "SIGMA 2 1 0 0 1",
                      "SIGMA 2 1 0 0 1   # a second line"}));
  const auto warned = cap.err_lines_with("Deprecated key SIGMA is ignored");
  REQUIRE(warned.size() == expected);
  if (!warned.empty()) {
    INFO(warned[0]);
    CHECK(warned[0].find("transposed") != std::string::npos);
    CHECK(warned[0].find(kRemoved) != std::string::npos);
    CHECK(warned[0].find("WCOV") == std::string::npos);
    CHECK(warned[0].find("retrain") == std::string::npos);
  }
  CHECK_FALSE(c.has_stored("SIGMA"));
  CHECK(c.get<std::string>("ATOM") == "Ta");
  CHECK(key_line(serialized(c), "SIGMA").empty());
}

/* Pins: a WNOISE that is not row type and number pairs ("WNOISE E", "WNOISE
 * E 0.1 F") and a WCOV whose first value is not an integer ("WCOV 1.5 1")
 * are refused at parse with the parser's error naming the key and its line
 * ("Invalid value for key WNOISE on line 2: 'E'"): the value of a retired
 * key is still checked.
 * Before the fix: refused alike (a pin: the keys kept their types), as
 * "Invalid schema for key: WNOISE", without the line. */
TEST_CASE("Config parser: a malformed WNOISE or WCOV is refused",
          "[config][uncertainty]") {
  for (const std::string line :
       {"WNOISE E", "WNOISE E 0.1 F", "WCOV 1.5 1"}) {
    INFO(line);
    const std::string msg = refusal({"ATOM Ta", line});
    INFO("message: " << msg);
    const std::string key = line.substr(0, line.find(' '));
    CHECK(msg.find("Invalid value for key " + key + " on line 2: '" +
                   line.substr(line.find(' ') + 1) + "'") !=
          std::string::npos);
  }
}
