// Retired configuration keys (blueprint field `retired`) in files and in
// the Context: EWEIGHT, FWEIGHT, SWEIGHT, WDBFILE_AUTO and EWEIGHT_TEMP. A
// potential that tadah train or tadah hpo of Tadah! 1.0.0 to 1.5.0 wrote
// has EWEIGHT, FWEIGHT and SWEIGHT lines, and from 1.3.0 a WDBFILE_AUTO
// line.
//
// A file that has them still parses (a potential keeps loading): the
// parser checks the value, records the line on the Context it returns
// (Context::retired_keys()) instead of storing the key, and gives one
// notice per parse that lists them. Context::add() refuses the key, and a
// fit refuses a configuration that records one, with the conversion
// (retired_keys_refusal()).

#include "catch2/catch.hpp"

#include <tadah/core/blueprint_parser.h>
#include <tadah/core/context.h>
#include <tadah/core/core_user_config_parser.h>
#include <tadah/core/key_notice.h>
#include <tadah/core/logger.h>
#include <tadah/core/retired_keys.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>
#include <unistd.h>

using namespace tadah::core;
using Catch::Matchers::Contains;
using Catch::Matchers::StartsWith;

namespace {

/* A scratch directory unique to this process, removed with its files. */
class ScratchDir {
public:
  ScratchDir()
      : dir_(std::filesystem::temp_directory_path() /
             ("tadah_core_retired_keys_" + std::to_string(::getpid()))) {
    std::filesystem::create_directories(dir_);
  }
  ~ScratchDir() {
    std::error_code ec;
    std::filesystem::remove_all(dir_, ec);
  }
  ScratchDir(const ScratchDir &) = delete;
  ScratchDir &operator=(const ScratchDir &) = delete;

  std::string write_file(const std::string &name,
                         const std::string &body) const {
    const auto p = dir_ / name;
    std::ofstream(p) << body;
    return p.string();
  }

private:
  std::filesystem::path dir_;
};

/* Captures std::cerr; restores it afterwards. */
class ErrCapture {
public:
  ErrCapture() : old_(std::cerr.rdbuf(err_.rdbuf())) {}
  ~ErrCapture() { std::cerr.rdbuf(old_); }
  ErrCapture(const ErrCapture &) = delete;
  ErrCapture &operator=(const ErrCapture &) = delete;
  std::vector<std::string> lines_with(const std::string &needle) const {
    std::vector<std::string> out;
    std::istringstream in(err_.str());
    for (std::string line; std::getline(in, line);)
      if (line.find(needle) != std::string::npos) out.push_back(line);
    return out;
  }

private:
  std::ostringstream err_;
  std::streambuf *old_;
};

/* The lines of a 1.5.0 potential that set the retired keys, with the keys
 * of its model around them. */
const std::string kOldPotential = "ATOM           Ta\n"
                                  "EWEIGHT        1.0\n"
                                  "FWEIGHT        100   # forces\n"
                                  "LAMBDA         0\n"
                                  "SWEIGHT        0\n"
                                  "WDBFILE_AUTO   0.0\n";

bool has_key_line(const std::string &text, const std::string &key) {
  std::istringstream in(text);
  for (std::string line; std::getline(in, line);) {
    std::istringstream ls(line);
    std::string first;
    if ((ls >> first) && first == key) return true;
  }
  return false;
}

std::vector<std::string> keys_of(const std::vector<RetiredKeyLine> &lines) {
  std::vector<std::string> out;
  for (const auto &l : lines) out.push_back(l.key);
  return out;
}

}  // namespace

TEST_CASE("Config parser: a retired key line is checked, recorded on the "
          "result and not stored",
          "[config][retired]") {
  IgnoredKeyNoticeCapture notices;
  Context c;
  REQUIRE_NOTHROW(c = CoreUserConfigParser().parse(std::vector<std::string>{
                      "ATOM Ta", "EWEIGHT 1.0", "FWEIGHT 100  # forces",
                      "LAMBDA 0", "SWEIGHT 0", "WDBFILE_AUTO 0.0",
                      "EWEIGHT_TEMP 300"}));
  CHECK(c.get<std::string>("ATOM") == "Ta");
  for (const std::string key :
       {"EWEIGHT", "FWEIGHT", "SWEIGHT", "WDBFILE_AUTO", "EWEIGHT_TEMP"}) {
    INFO(key);
    CHECK_FALSE(c.has_stored(key));
    CHECK_FALSE(c.contains(key));  // no default either
  }
  const std::vector<RetiredKeyLine> r = c.retired_keys();
  REQUIRE(keys_of(r) == (std::vector<std::string>{"EWEIGHT", "FWEIGHT",
                                                  "SWEIGHT", "WDBFILE_AUTO",
                                                  "EWEIGHT_TEMP"}));
  CHECK(r[0].line == 2);
  CHECK(r[0].values == std::vector<std::string>{"1.0"});
  CHECK(r[1].line == 3);
  CHECK(r[1].values == std::vector<std::string>{"100"});
  CHECK(r[1].text == "FWEIGHT 100  # forces");
  CHECK(r[4].line == 7);
  CHECK(r[4].source.empty());
}

/* Pins: the value of a retired key is checked as any value, and refused
 * naming the key and its line ("Invalid value for key EWEIGHT on line 1:
 * 'heavy'", the key's schema, format and examples after it).
 * Before the fix: refused as "Invalid schema for key: EWEIGHT", without the
 * line. */
TEST_CASE("Config parser: a retired key's value is still checked",
          "[config][retired]") {
  REQUIRE_THROWS_WITH(CoreUserConfigParser().parse(std::vector<std::string>{
                          "EWEIGHT heavy"}),
                      StartsWith("Invalid value for key EWEIGHT on line 1: "
                                 "'heavy'\n"));
}

TEST_CASE("Config parser: one notice per parse lists the retired keys of "
          "the file, with their lines",
          "[config][retired]") {
  const ScratchDir dir;
  const std::string pot = dir.write_file("old.tadah", kOldPotential);
  const std::string plain = dir.write_file("new.tadah", "ATOM Ta\n");

  SECTION("under a capture, as pair_style tadah loads a potential") {
    IgnoredKeyNoticeCapture cap;
    CoreUserConfigParser().parse(pot);
    REQUIRE(cap.notices().size() == 1);
    const std::string &n = cap.notices()[0];
    CHECK_THAT(n, StartsWith("Retired keys"));
    CHECK_THAT(n, Contains("EWEIGHT (line 2 of " + pot + ")") &&
                      Contains("FWEIGHT (line 3 of " + pot + ")") &&
                      Contains("SWEIGHT (line 5 of " + pot + ")") &&
                      Contains("WDBFILE_AUTO (line 6 of " + pot + ")"));
    CHECK_THAT(n, Contains("ignored") && Contains("refuse") &&
                      Contains("tadah explain"));
    /* Every load gives its notice: a second parse of the same file too. */
    CoreUserConfigParser().parse(pot);
    CHECK(cap.notices().size() == 2);
    /* A file without retired keys gives none. */
    CoreUserConfigParser().parse(plain);
    CHECK(cap.notices().size() == 2);
  }
  SECTION("on standard error, whatever the Logger level") {
    const LogLevel old = Logger::instance().getLogLevel();
    Logger::instance().setLogLevel(LogLevel::ERROR);
    ErrCapture err;
    CoreUserConfigParser().parse(pot);
    Logger::instance().setLogLevel(old);
    const auto lines = err.lines_with("Retired keys");
    REQUIRE(lines.size() == 1);
    CHECK_THAT(lines[0], StartsWith("WARNING: Retired keys"));
  }
  SECTION("one retired key") {
    IgnoredKeyNoticeCapture cap;
    CoreUserConfigParser().parse(std::vector<std::string>{"EWEIGHT 1"});
    REQUIRE(cap.notices().size() == 1);
    CHECK_THAT(cap.notices()[0],
               StartsWith("Retired key EWEIGHT (line 1) is ignored"));
  }
}

TEST_CASE("Config parser: a retired key of a CONFIG include is recorded with "
          "its file, and the parse gives one notice",
          "[config][retired]") {
  const ScratchDir dir;
  const std::string sub = dir.write_file("old.tadah", kOldPotential);
  const std::string top = dir.write_file(
      "top.train", "CONFIG " + sub + "\nEWEIGHT_TEMP 500\nDBFILE db.tadah\n");
  IgnoredKeyNoticeCapture cap;
  const Context c = CoreUserConfigParser().parse(top);
  const auto r = c.retired_keys();
  REQUIRE(keys_of(r) == (std::vector<std::string>{"EWEIGHT", "FWEIGHT",
                                                  "SWEIGHT", "WDBFILE_AUTO",
                                                  "EWEIGHT_TEMP"}));
  CHECK(r[0].source == sub);
  CHECK(r[4].source == top);
  CHECK(r[4].line == 2);
  REQUIRE(cap.notices().size() == 1);
  CHECK_THAT(cap.notices()[0], Contains("EWEIGHT_TEMP (line 2 of " + top));
}

TEST_CASE("Context: the retired lines travel with copies, merges and "
          "serialisation, and no writer writes them",
          "[context][retired]") {
  IgnoredKeyNoticeCapture quiet;
  const Context c = CoreUserConfigParser().parse(std::vector<std::string>{
      "ATOM Ta", "EWEIGHT 1.0", "WDBFILE_AUTO 0.5"});
  const Context copy = c;
  CHECK(keys_of(copy.retired_keys()) ==
        (std::vector<std::string>{"EWEIGHT", "WDBFILE_AUTO"}));

  Context other;
  other.add("LAMBDA", 0.1);
  /* Either side of a merge keeps them, and a merge with itself adds no
   * second copy. */
  CHECK(Context::merge(other, c).retired_keys().size() == 2);
  CHECK(Context::merge(c, other).retired_keys().size() == 2);
  CHECK(Context::merge(c, c).retired_keys().size() == 2);
  CHECK(Context::merge(c, c, true, Context::BlockMerge::Append)
            .retired_keys()
            .size() == 2);

  Context back;
  back.deserialize(c.serialize());
  const auto r = back.retired_keys();
  REQUIRE(r.size() == 2);
  CHECK(r[0].key == "EWEIGHT");
  CHECK(r[0].values == std::vector<std::string>{"1.0"});
  CHECK(r[1].line == 3);

  std::ostringstream as_text, as_pot;
  as_text << c;
  c.write_potential(as_pot, 17);
  for (const std::string key : {"EWEIGHT", "WDBFILE_AUTO"}) {
    INFO(key);
    CHECK_FALSE(has_key_line(as_text.str(), key));
    CHECK_FALSE(has_key_line(as_pot.str(), key));
  }
  CHECK(has_key_line(as_pot.str(), "ATOM"));
  CHECK(as_text.str().find("@") == std::string::npos);
}

TEST_CASE("Context::add refuses a retired key with its conversion, and a "
          "read of one names it as retired",
          "[context][retired]") {
  Context c;
  REQUIRE_THROWS_WITH(c.add("EWEIGHT", 1.0),
                      Contains("EWEIGHT") && Contains("retired") &&
                          Contains("ESIGMA = 1000/(N sqrt(EWEIGHT))"));
  REQUIRE_THROWS_WITH(c.add("wdbfile_auto", "0.5"),
                      Contains("WDBFILE_AUTO") && Contains("retired") &&
                          Contains("n_d^-a"));
  CHECK_FALSE(c.has_stored("EWEIGHT"));
  REQUIRE_THROWS_WITH(c.get<double>("FWEIGHT"),
                      Contains("FWEIGHT") && Contains("retired") &&
                          Contains("FSIGMA = 1/sqrt(FWEIGHT)"));
}

TEST_CASE("The error scales, ENORM and ENERGY are keys of a configuration",
          "[config][retired]") {
  const Context c = CoreUserConfigParser().parse(std::vector<std::string>{
      "ESIGMA 0.5", "FSIGMA 0.2 0.05", "SSIGMA 1", "ENORM sqrt",
      "ENERGY false"});
  CHECK(c.get<double>("ESIGMA") == 0.5);
  CHECK(c.size("FSIGMA") == 2);
  CHECK(c.get<double>("FSIGMA", 1) == 0.05);
  CHECK(c.size("SSIGMA") == 1);
  CHECK(c.get<std::string>("ENORM") == "sqrt");
  CHECK_FALSE(c.get<bool>("ENERGY"));
  REQUIRE_THROWS_WITH(CoreUserConfigParser().parse(std::vector<std::string>{
                          "ENORM TOTAL"}),
                      Contains("ENORM") && Contains("ATOM, SQRT"));
  REQUIRE_THROWS(CoreUserConfigParser().parse(std::vector<std::string>{
      "FSIGMA 0.1 0 0"}));
  /* The defaults: written into every potential a fit writes. */
  Context d;
  CHECK(d.get<double>("ESIGMA") == 1.0);
  CHECK(d("FSIGMA") == (std::vector<std::string>{"0.1", "0"}));
  CHECK(d("SSIGMA") == (std::vector<std::string>{"0.5", "0"}));
  CHECK(d.get<std::string>("ENORM") == "ATOM");
  CHECK(d.get<bool>("ENERGY"));
}

/* Pins: a parse of a file with a KeyLineRule gives the caller a retired
 * key's line that the rule selects, as it gives any other line: listed in
 * skipped (its file, line, key and values), not recorded on the result
 * (Context::retired_keys()) and not in the parse's notice; a retired line
 * the rule does not select is recorded, with the notice.
 * Before the fix: the rule was not asked for a retired key's line, which
 * was recorded and named in the notice. */
TEST_CASE("A retired key's line that the caller's rule selects is the "
          "caller's",
          "[config][retired]") {
  IgnoredKeyNoticeCapture cap;
  const ScratchDir dir;
  const std::string file =
      dir.write_file("rule.cfg", "ATOM Ta\nUNCERTAINTY true\nWCOV 1 0.5\n");
  const KeyLineRule rule = [](const std::set<std::string> &,
                              const std::string &key) {
    return key == "UNCERTAINTY";
  };
  std::vector<SkippedKeyLine> skipped;
  const Context c = CoreUserConfigParser().parse(file, rule, skipped);
  REQUIRE(skipped.size() == 1);
  CHECK(skipped[0].key == "UNCERTAINTY");
  CHECK(skipped[0].source == file);
  CHECK(skipped[0].line == 2);
  CHECK(skipped[0].values == std::vector<std::string>{"true"});
  const std::vector<RetiredKeyLine> r = c.retired_keys();
  REQUIRE(r.size() == 1);
  CHECK(r[0].key == "WCOV");
  REQUIRE(cap.notices().size() == 1);
  CHECK_THAT(cap.notices()[0],
             StartsWith("Retired key WCOV (line 3 of " + file +
                        ") is ignored"));
}

TEST_CASE("retired_keys_refusal names each retired line and its conversion",
          "[retired]") {
  IgnoredKeyNoticeCapture quiet;
  const ScratchDir dir;
  const std::string pot = dir.write_file("old.tadah", kOldPotential);
  const Context c = CoreUserConfigParser().parse(pot);
  const std::string msg = retired_keys_refusal(c.retired_keys(), "tadah train");
  CHECK_THAT(msg, StartsWith("tadah train: "));
  CHECK_THAT(msg, Contains("EWEIGHT 1.0 (line 2 of " + pot + ")") &&
                      Contains("FWEIGHT 100 (line 3 of " + pot + ")") &&
                      Contains("SWEIGHT 0 (line 5 of " + pot + ")") &&
                      Contains("WDBFILE_AUTO 0.0 (line 6 of " + pot +
                               "; 0: converted by deleting the line)"));
  const auto &keys = BlueprintParser::getKeysMapRef();
  for (const std::string key :
       {"EWEIGHT", "FWEIGHT", "SWEIGHT", "WDBFILE_AUTO"}) {
    INFO(key);
    CHECK_THAT(msg, Contains(keys.at(key).retired.conversion));
  }
  /* A line a deletion converts is marked, and the refusal says what to do
   * with both kinds: "With the converted keys in place of these lines, or
   * the marked lines deleted (...)". (Before the fix: "With the converted
   * keys in place of these lines (...)", whatever the marks.) */
  CHECK_THAT(msg, Contains(" With the converted keys in place of these "
                           "lines, or the marked lines deleted (in the file "
                           "that has them, a potential's included"));
  /* Each key's conversion once, however many lines set it. */
  const Context twice = CoreUserConfigParser().parse(
      std::vector<std::string>{"FWEIGHT 1", "FWEIGHT 2"});
  const std::string m2 = retired_keys_refusal(twice.retired_keys(), "A fit");
  const std::string conv = keys.at("FWEIGHT").retired.conversion;
  CHECK(m2.find(conv) != std::string::npos);
  CHECK(m2.find(conv, m2.find(conv) + 1) == std::string::npos);
  /* No line marked: the converted keys in place of every line. */
  CHECK_THAT(m2, Contains(" With the converted keys in place of these lines "
                          "(in the file that has them, a potential's "
                          "included), the configuration trains"));
  CHECK(m2.find("marked") == std::string::npos);

  REQUIRE_THROWS_WITH(require_no_retired_keys(c, "A fit"),
                      StartsWith("A fit: ") && Contains("EWEIGHT 1.0"));
  Context fresh;
  fresh.add("ATOM", "Ta");
  REQUIRE_NOTHROW(require_no_retired_keys(fresh, "A fit"));
}

/* Pins: the refusal names, for each line of a quantity the configuration
 * leaves out, that deleting the line converts it: FWEIGHT with FORCE
 * false, SWEIGHT with STRESS false (a potential of Tadah! 1.0.0 to 1.5.0
 * has both lines whatever the switches, and a fit refuses an FSIGMA or
 * SSIGMA other than the default for a quantity that is off), and
 * WDBFILE_AUTO 0; a line of a quantity that is on is not so marked.
 * Before the fix: the refusal gave FSIGMA = 1/sqrt(FWEIGHT) and SSIGMA =
 * 160.2/sqrt(SWEIGHT) for every line, so the converted configuration of
 * such a potential was refused again (SSIGMA 160.2 with STRESS false). */
TEST_CASE("retired_keys_refusal says which lines convert by deletion",
          "[retired]") {
  IgnoredKeyNoticeCapture quiet;
  const Context off = CoreUserConfigParser().parse(std::vector<std::string>{
      "FORCE false", "STRESS false", "EWEIGHT 1.0", "FWEIGHT 1.0",
      "SWEIGHT 1.0", "WDBFILE_AUTO 0.0"});
  std::string msg;
  try {
    require_no_retired_keys(off, "A fit");
  } catch (const std::exception &e) {
    msg = e.what();
  }
  CHECK_THAT(msg,
             Contains("EWEIGHT 1.0 (line 3)") &&
                 Contains("FWEIGHT 1.0 (line 4; FORCE false here: converted "
                          "by deleting the line)") &&
                 Contains("SWEIGHT 1.0 (line 5; STRESS false here: converted "
                          "by deleting the line)") &&
                 Contains("WDBFILE_AUTO 0.0 (line 6; 0: converted by "
                          "deleting the line)"));
  const Context on = CoreUserConfigParser().parse(std::vector<std::string>{
      "FORCE true", "STRESS true", "FWEIGHT 100", "SWEIGHT 4",
      "WDBFILE_AUTO 0.5"});
  msg.clear();
  try {
    require_no_retired_keys(on, "A fit");
  } catch (const std::exception &e) {
    msg = e.what();
  }
  CHECK_THAT(msg, Contains("FWEIGHT 100 (line 3)") &&
                      Contains("SWEIGHT 4 (line 4)") &&
                      Contains("WDBFILE_AUTO 0.5 (line 5)"));
  CHECK(msg.find("deleting the line)") == std::string::npos);
}

/* Pins: a retrain does not read a potential's WDBFILE line (it is in the
 * training record of the potentials this version writes), so the
 * conversion of a potential's WDBFILE_AUTO line puts the WDBFILE values
 * on the command line or into a file that includes the potential: the
 * conversion says so, and so does the refusal's last sentence when a
 * WDBFILE_AUTO line is among its lines (not otherwise).
 * Before the fix: the refusal said to put every converted key in place of
 * its line, a potential's included, so the WDBFILE line of a converted
 * potential of Tadah! 1.5.0 was ignored and the retrain fitted another
 * fit. */
TEST_CASE("retired_keys_refusal puts the WDBFILE values of a potential "
          "where a retrain reads them",
          "[retired]") {
  IgnoredKeyNoticeCapture quiet;
  const ScratchDir dir;
  const std::string pot = dir.write_file(
      "auto.tadah", "ATOM Ta\nEWEIGHT 2.5\nWDBFILE 1 3\nWDBFILE_AUTO 0.7\n"
                    "WEIGHTS 1 2\n");
  const Context c = CoreUserConfigParser().parse(pot);
  std::string msg;
  try {
    require_no_retired_keys(c, "tadah train");
  } catch (const std::exception &e) {
    msg = e.what();
  }
  CHECK_THAT(msg, Contains("WDBFILE_AUTO 0.7 (line 4 of " + pot + ")") &&
                      Contains("A retrain does not read a potential's "
                               "WDBFILE line") &&
                      Contains("the WDBFILE values of a potential on the "
                               "command line or in a file that includes it"));
  const Context no_auto = CoreUserConfigParser().parse(
      std::vector<std::string>{"EWEIGHT 2.5", "FWEIGHT 50"});
  msg.clear();
  try {
    require_no_retired_keys(no_auto, "tadah train");
  } catch (const std::exception &e) {
    msg = e.what();
  }
  CHECK_THAT(msg, Contains("EWEIGHT 2.5 (line 1)"));
  CHECK(msg.find("WDBFILE") == std::string::npos);
}

/* Pins: M_KRR implementation 2 (MODEL M_KRR <kernel> 2) of Tadah! 1.0.0
 * to 1.5.0 solved (K + LAMBDA I) w = t on the energies per atom in eV and
 * read none of the retired keys (no weight, no priority), so under it each
 * retired line converts by its deletion, and ESIGMA 1000 (sigma = 1 eV),
 * or LAMBDA times 10^6 with ESIGMA 1, gives that solve: the refusal marks
 * every line so and gives that conversion instead of the keys' own (which
 * would change the fit: ESIGMA = 1000/(N sqrt(EWEIGHT)) is sigma = 18.5
 * meV/atom for EWEIGHT 1 and 54-atom cells). Implementation 1, given or
 * by default, keeps the keys' conversions.
 * Before the fix: the refusal gave the conversions of implementation 1
 * under implementation 2 too. */
TEST_CASE("retired_keys_refusal converts every retired line of M_KRR "
          "implementation 2 by its deletion, with ESIGMA 1000",
          "[retired]") {
  IgnoredKeyNoticeCapture quiet;
  auto refusal = [](const std::vector<std::string> &lines) {
    const Context c = CoreUserConfigParser().parse(lines);
    try {
      require_no_retired_keys(c, "tadah train");
    } catch (const std::exception &e) {
      return std::string(e.what());
    }
    return std::string();
  };
  const std::string mark =
      "; M_KRR implementation 2 here: converted by deleting the line)";
  const std::string msg = refusal(
      {"MODEL M_KRR Kern_RBF 2", "FORCE false", "STRESS false",
       "EWEIGHT 1.0", "FWEIGHT 1.0", "SWEIGHT 1.0", "WDBFILE_AUTO 0.5",
       "EWEIGHT_TEMP 300"});
  CHECK_THAT(msg, Contains("EWEIGHT 1.0 (line 4" + mark) &&
                      Contains("FWEIGHT 1.0 (line 5" + mark) &&
                      Contains("SWEIGHT 1.0 (line 6" + mark) &&
                      Contains("WDBFILE_AUTO 0.5 (line 7" + mark) &&
                      Contains("EWEIGHT_TEMP 300 (line 8" + mark) &&
                      Contains("M_KRR implementation 2 of Tadah! 1.0.0 to "
                               "1.5.0 read none of them") &&
                      Contains("(K + LAMBDA I) w = t") &&
                      Contains("With these lines deleted") &&
                      Contains("ESIGMA 1000, or LAMBDA times 10^6 with "
                               "ESIGMA 1,"));
  CHECK(msg.find("ESIGMA = 1000/(N sqrt(EWEIGHT))") == std::string::npos);
  CHECK(msg.find("n_d^-a") == std::string::npos);
  const std::string one =
      refusal({"MODEL M_KRR Kern_Linear 2", "EWEIGHT 1"});
  CHECK_THAT(one, Contains("EWEIGHT 1 (line 2" + mark) &&
                      Contains("did not read it") &&
                      Contains("With this line deleted"));
  for (const std::string model :
       {"MODEL M_KRR Kern_RBF 1", "MODEL M_KRR Kern_RBF",
        "MODEL M_BLR BF_Linear"}) {
    INFO(model);
    const std::string m = refusal({model, "EWEIGHT 1"});
    CHECK_THAT(m, Contains("EWEIGHT 1 (line 2)") &&
                      Contains("ESIGMA = 1000/(N sqrt(EWEIGHT))"));
    CHECK(m.find("implementation 2 here") == std::string::npos);
  }
}

/* Pins: a line of a key that no key converts (the field unconverted of
 * `retired`: EWEIGHT_TEMP, whose Boltzmann factor no key reproduces) is
 * marked "no key converts it", counts neither among the lines a converted
 * key replaces nor among those a deletion converts, and the refusal gives
 * the key's reason and closes with the line deleted and that no key
 * reproduces it:
 *  - alone: "EWEIGHT_TEMP 300 (line 1; no key converts it)", "With this
 *    line deleted (in the file that has it, a potential's included), the
 *    configuration trains, and no key reproduces the EWEIGHT_TEMP line; no
 *    coefficients were fitted.";
 *  - with EWEIGHT: "With the converted keys in place of the others and the
 *    EWEIGHT_TEMP line deleted (...)";
 *  - with FWEIGHT under FORCE false (a line a deletion converts): "With
 *    these lines deleted (...)";
 *  - with both: "With the converted keys in place of the others, or the
 *    marked lines deleted, and the EWEIGHT_TEMP line deleted (...)".
 * Under M_KRR implementation 2 every line converts by its deletion (the
 * case above).
 * Before the fix: the EWEIGHT_TEMP line was not marked, and the refusal
 * closed "With the converted keys in place of this line (...)" (with
 * EWEIGHT: "of these lines"), while the conversion it gave said "No key
 * converts it": every check of the closing fails. */
TEST_CASE("retired_keys_refusal deletes the line of a key that no key "
          "converts",
          "[retired]") {
  IgnoredKeyNoticeCapture quiet;
  auto refusal = [](const std::vector<std::string> &lines) {
    const Context c = CoreUserConfigParser().parse(lines);
    try {
      require_no_retired_keys(c, "tadah train");
    } catch (const std::exception &e) {
      return std::string(e.what());
    }
    return std::string();
  };
  const std::string reproduces =
      "), the configuration trains, and no key reproduces the EWEIGHT_TEMP "
      "line; no coefficients were fitted.";
  const std::string reason =
      "EWEIGHT_TEMP: EWEIGHT_TEMP T multiplied the energy priority of each "
      "structure by exp(-(E/N - E_min)/(kB T))";

  const std::string alone = refusal({"EWEIGHT_TEMP 300"});
  INFO(alone);
  CHECK_THAT(alone, Contains("EWEIGHT_TEMP 300 (line 1; no key converts it)") &&
                        Contains(reason) && Contains("No key converts it.") &&
                        Contains(" With this line deleted (in the file that "
                                 "has it, a potential's included" +
                                 reproduces));
  CHECK(alone.find("converted keys") == std::string::npos);

  const std::string with_eweight =
      refusal({"EWEIGHT 1.0", "EWEIGHT_TEMP 300"});
  INFO(with_eweight);
  CHECK_THAT(with_eweight,
             Contains("EWEIGHT 1.0 (line 1)") &&
                 Contains("EWEIGHT_TEMP 300 (line 2; no key converts it)") &&
                 Contains(" With the converted keys in place of the others "
                          "and the EWEIGHT_TEMP line deleted (in the file "
                          "that has them, a potential's included" +
                          reproduces));

  const std::string with_marked =
      refusal({"FORCE false", "FWEIGHT 1.0", "EWEIGHT_TEMP 300"});
  INFO(with_marked);
  CHECK_THAT(with_marked,
             Contains("FWEIGHT 1.0 (line 2; FORCE false here: converted by "
                      "deleting the line)") &&
                 Contains("EWEIGHT_TEMP 300 (line 3; no key converts it)") &&
                 Contains(" With these lines deleted (in the file that has "
                          "them, a potential's included" +
                          reproduces));

  const std::string all = refusal(
      {"FORCE false", "EWEIGHT 1.0", "FWEIGHT 1.0", "EWEIGHT_TEMP 300"});
  INFO(all);
  CHECK_THAT(all, Contains(" With the converted keys in place of the others, "
                           "or the marked lines deleted, and the EWEIGHT_TEMP "
                           "line deleted (in the file that has them, a "
                           "potential's included" +
                           reproduces));
}

/* A QUANTITY line, which tadah train of Tadah! 1.5.0 wrote into a potential
 * when its configuration set the key (no command read it), is a retired
 * key that a deletion converts: the potential loads, the line recorded and
 * named in one notice and not stored, and a fit refuses it, marking the
 * line converted by deleting it and giving the reason. The key is no key
 * of a command (command_only false) and has no command-line option.
 * Before the fix: the blueprint had no QUANTITY, so the parse refused the
 * potential ("Invalid key QUANTITY on line 2 (closest key: ...)"): the
 * load fails, and so does the blueprint check. */
TEST_CASE("A QUANTITY line of a 1.5.0 potential loads with one notice, and "
          "a fit refuses it",
          "[config][retired]") {
  const auto &keys = BlueprintParser::getKeysMapRef();
  REQUIRE(keys.count("QUANTITY") == 1);
  const BlueprintKey &k = keys.at("QUANTITY");
  CHECK(k.isRetired());
  CHECK(k.retired.by_deletion);
  CHECK_FALSE(k.command_only);
  CHECK(k.cli_long_flag.empty());

  IgnoredKeyNoticeCapture cap;
  Context c;
  REQUIRE_NOTHROW(c = CoreUserConfigParser().parse(std::vector<std::string>{
                      "ATOM Ta", "QUANTITY          volume"}));
  CHECK_FALSE(c.has_stored("QUANTITY"));
  const std::vector<RetiredKeyLine> r = c.retired_keys();
  REQUIRE(r.size() == 1);
  CHECK(r[0].key == "QUANTITY");
  CHECK(r[0].line == 2);
  REQUIRE(cap.notices().size() == 1);
  CHECK_THAT(cap.notices()[0],
             StartsWith("Retired key QUANTITY (line 2) is ignored"));

  std::string msg;
  try {
    require_no_retired_keys(c, "tadah train");
  } catch (const std::exception &e) {
    msg = e.what();
  }
  INFO(msg);
  CHECK_THAT(msg, Contains("QUANTITY volume (line 2; converted by deleting "
                           "the line)") &&
                      Contains("QUANTITY: QUANTITY was read by no command"));
}
