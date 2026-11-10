/* Keys that change nothing a fit gives (inert_keys.h). A fit -- tadah
 * train, tadah hpo, tadah refit and a program's own training -- refuses a
 * configuration that has a line of one, naming the key, the line and the
 * file:
 *  - a deprecated key (MPIWPCKG, MBLOCK, NBLOCK, SIGMA), whose lines the
 *    parse records (core::Context::deprecated_keys());
 *  - EMBF0 and RHOFORM when the configuration has no DM_REAM block;
 *  - WATOM with a value other than 1 when no descriptor of the
 *    configuration reads it (D2_EAM and DM_EAM take their functions from
 *    the SETFL file, unweighted).
 * A potential loaded to predict with gives a notice for each instead
 * (embf0.h, the notice of a deprecated key, and DC_Selector for WATOM).
 */

#include "catch2/catch.hpp"

#include <tadah/core/blueprint_parser.h>
#include <tadah/core/context.h>
#include <tadah/core/core_user_config_parser.h>
#include <tadah/core/logger.h>
#include <tadah/core/periodic_table.h>
#include <tadah/models/inert_keys.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>
#include <unistd.h>

using tadah::core::Context;
using tadah::core::CoreUserConfigParser;
using namespace tadah::models;

namespace {

/* A scratch directory unique to this process, removed with its files. */
class ScratchDir {
public:
  ScratchDir()
      : dir_(std::filesystem::temp_directory_path() /
             ("tadah_models_inert_keys_" + std::to_string(::getpid()))) {
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

/* Standard error and output silenced while a parse writes its notices. */
class Quiet {
public:
  Quiet()
      : out_(std::cout.rdbuf(sink_.rdbuf())),
        err_(std::cerr.rdbuf(sink_.rdbuf())) {}
  ~Quiet() {
    std::cout.rdbuf(out_);
    std::cerr.rdbuf(err_);
  }
  Quiet(const Quiet &) = delete;
  Quiet &operator=(const Quiet &) = delete;

private:
  std::ostringstream sink_;
  std::streambuf *out_, *err_;
};

/* A context built as a program builds one (no file, no lines). */
Context ctx_of(const std::vector<std::pair<std::string, std::string>> &kv) {
  Context c;
  for (const auto &p : kv) {
    c.remove(p.first);
    c.add(p.first, p.second);
  }
  return c;
}

const char *const kLj =
    "ATOM Ta\nINIT2B true\nINITMB false\nTYPE2B D2_LJ Ta Ta\n"
    "RCTYPE2B Cut_Cos\nRCUT2B 5.0\nMODEL M_BLR BF_Linear\n";
// D2_EAM and DM_EAM, without ATOM and WATOM (the cases give them).
const char *const kEam =
    "INIT2B true\nTYPE2B D2_EAM 0 Ta Ta\nINITMB true\n"
    "TYPEMB DM_EAM 0 Ta Ta\nSETFL eam.alloy\nRCUT2B 5.58190742024\n"
    "RCUTMB 5.58190742024\nMODEL M_BLR BF_Linear\n";

std::string refusal(const Context &c, const InertKeyRules &rules = {}) {
  try {
    require_no_inert_keys(c, "tadah train", rules);
  } catch (const std::runtime_error &e) {
    return e.what();
  }
  return "";
}

}  // namespace

/* Pins: context_reads_watom is true when an initialised TYPE2B or TYPEMB
 * line names a descriptor that weights its pairs by WATOM, or a component
 * (an @id of a DESCRIPTOR block), and false when every descriptor it names
 * is D2_EAM, DM_EAM or none: D2_EAM + DM_EAM, DM_EAM alone with INIT2B
 * false, INIT2B and INITMB false: false; D2_LJ, D2_EAM with a DM_mJoin of
 * DM_EAM and DM_Blip, a component TYPEMB @ta: true. A D2_LJ line with
 * INIT2B false is not read.
 * Before the fix: does not compile (no inert_keys.h). */
TEST_CASE("context_reads_watom: whether a descriptor of a configuration "
          "weights its pairs by WATOM",
          "[inert_keys][watom]") {
  CHECK_FALSE(context_reads_watom(ctx_of({{"INIT2B", "true"},
                                          {"TYPE2B", "D2_EAM 0 Ta Ta"},
                                          {"INITMB", "true"},
                                          {"TYPEMB", "DM_EAM 0 Ta Ta"}})));
  CHECK_FALSE(context_reads_watom(ctx_of({{"INIT2B", "false"},
                                          {"TYPE2B", "D2_LJ Ta Ta"},
                                          {"INITMB", "true"},
                                          {"TYPEMB", "DM_EAM 0 Ta Ta"}})));
  CHECK_FALSE(context_reads_watom(
      ctx_of({{"INIT2B", "false"}, {"INITMB", "false"}})));
  CHECK(context_reads_watom(
      ctx_of({{"INIT2B", "true"}, {"TYPE2B", "D2_LJ Ta Ta"}})));
  CHECK(context_reads_watom(ctx_of(
      {{"INIT2B", "true"},
       {"TYPE2B", "D2_EAM 0 Ta Ta"},
       {"INITMB", "true"},
       {"TYPEMB", "DM_mJoin DM_EAM 0 Ta Ta DM_Blip 0 1 1 Ta Ta"}})));
  CHECK(context_reads_watom(
      ctx_of({{"INIT2B", "false"}, {"INITMB", "true"}, {"TYPEMB", "@ta"}})));
}

/* Pins: a configuration whose many-body term is DESCRIPTOR blocks without a
 * TYPEMB line (the blocks are its descriptors, DM_Embed components, whose
 * density WATOM weighs) reads WATOM, with INITMB true and without an
 * INITMB line (the blocks switch the term on): context_reads_watom true,
 * and require_no_inert_keys takes a WATOM of 2.0 (no refusal); with INITMB
 * false (which the blocks refuse) and INIT2B false it reads none.
 * Before the fix: context_reads_watom was false for both, and a fit
 * refused WATOM 2.0, "WATOM 2.0 ...: no descriptor of the configuration
 * reads it", although the weight changes the fit. */
TEST_CASE("context_reads_watom: DESCRIPTOR blocks without a TYPEMB line "
          "read WATOM",
          "[inert_keys][watom][embed]") {
  tadah::core::PeriodicTable::initialize();
  const std::string block =
      "DESCRIPTOR ta DM_Embed\nPAIRS Ta Ta\nDENSITY\nBASIS Blip5\n"
      "FORM SIGNED\nCENTRES 2.8 3.6 4.4\nETA 3 3 3\n"
      "AMPLITUDES 0.5 0.3 0.1\nCUTOFF\nTYPE Cut_Cos\nRADIUS 4.5\n"
      "ENDCUTOFF\nENDDENSITY\nEMBEDDING\nBASIS Gaussian\n"
      "CENTRES 0 0.5 1\nETA 2 2 2\nREFERENCE ZERO\nENDEMBEDDING\n"
      "ENDDESCRIPTOR\n";
  for (const std::string initmb : {"INITMB true\n", ""}) {
    INFO("INITMB line: '" << initmb << "'");
    std::istringstream in("ATOM Ta\nWATOM 2.0\nINIT2B false\n" + initmb +
                          "MODEL M_BLR BF_Linear\n" + block);
    const Context c = CoreUserConfigParser().parse(in);
    CHECK(context_reads_watom(c));
    CHECK(refusal(c).empty());
  }
  std::istringstream off("ATOM Ta\nWATOM 2.0\nINIT2B false\nINITMB false\n"
                         "MODEL M_BLR BF_Linear\n" + block);
  CHECK_FALSE(context_reads_watom(CoreUserConfigParser().parse(off)));
}

/* Pins: require_no_inert_keys refuses a configuration with a line that
 * changes nothing a fit gives, naming who refuses, the key with its values
 * and the line and file of the line: MBLOCK 64, NBLOCK 8 and MPIWPCKG 50
 * (deprecated); EMBF0 FREE and RHOFORM SIGNED with no DM_REAM block;
 * WATOM 2.5, and WATOM Ta Ta 2.5 (a pair triple), with D2_EAM and DM_EAM ("WATOM
 * 2.5 (line 2 of eam.train)"). Several such lines give one refusal that
 * lists each. Accepted: the same configurations without those lines, WATOM
 * 1 with D2_EAM and DM_EAM, WATOM 2.5 with D2_LJ, and, with the rules of a
 * fit that reads EMBF0 and RHOFORM itself (InertKeyRules{false}, tadah
 * refit), EMBF0 FREE and RHOFORM SIGNED without a DM_REAM block.
 * Before the fix: does not compile (no inert_keys.h): a fit accepted each
 * line, the deprecated keys and EMBF0 with a WARNING, WATOM without one. */
TEST_CASE("require_no_inert_keys: a fit refuses a line that changes nothing "
          "it gives, naming the key, the line and the file",
          "[inert_keys]") {
  tadah::core::PeriodicTable::initialize();
  const ScratchDir dir;
  Quiet quiet;
  auto parse = [&](const std::string &name, const std::string &body) {
    return CoreUserConfigParser().parse(dir.write_file(name, body));
  };
  struct Case {
    std::string name, extra, named;
  };
  for (const Case &k : std::vector<Case>{
           {"mblock.train", "MBLOCK 64\n", "MBLOCK 64 (line 8 of "},
           {"nblock.train", "NBLOCK 8\n", "NBLOCK 8 (line 8 of "},
           {"mpiwpckg.train", "MPIWPCKG 50\n", "MPIWPCKG 50 (line 8 of "},
           {"embf0.train", "EMBF0 FREE\n", "EMBF0 FREE (line 8 of "},
           {"rhoform.train", "RHOFORM SIGNED\n",
            "RHOFORM SIGNED (line 8 of "}}) {
    INFO(k.name);
    const std::string path = dir.write_file(k.name, std::string(kLj) + k.extra);
    const std::string what = refusal(CoreUserConfigParser().parse(path));
    INFO("refusal: " << what);
    CHECK_THAT(what, Catch::StartsWith("tadah train: "));
    CHECK_THAT(what, Catch::Contains(k.named + path + ")"));
  }
  for (const std::string watom : {"2.5", "Ta Ta 2.5"}) {
    INFO("WATOM " << watom);
    const std::string path = dir.write_file(
        "eam.train", "ATOM Ta\nWATOM " + watom + "\n" + kEam);
    const Context c = CoreUserConfigParser().parse(path);
    CHECK_FALSE(context_reads_watom(c));
    const std::string what = refusal(c);
    INFO("refusal: " << what);
    CHECK_THAT(what, Catch::Contains("WATOM " + watom + " (line 2 of " +
                                     path + ")"));
    CHECK_THAT(what, Catch::Contains("D2_EAM"));
  }
  {
    const std::string path =
        dir.write_file("several.train", std::string(kLj) +
                                            "MBLOCK 64\nEMBF0 FREE\n");
    const std::string what = refusal(CoreUserConfigParser().parse(path));
    INFO("refusal: " << what);
    CHECK_THAT(what, Catch::Contains("MBLOCK 64 (line 8 of " + path + ")"));
    CHECK_THAT(what, Catch::Contains("EMBF0 FREE (line 9 of " + path + ")"));
  }
  CHECK(refusal(parse("lj.train", kLj)).empty());
  CHECK(refusal(parse("eam1.train", std::string("ATOM Ta\nWATOM 1\n") + kEam))
            .empty());
  CHECK(refusal(parse("lj_watom.train", std::string(kLj) + "WATOM 2.5\n"))
            .empty());
  {
    const Context c =
        parse("refit.train", std::string(kLj) + "EMBF0 FREE\nRHOFORM SIGNED\n");
    CHECK(refusal(c, InertKeyRules{false}).empty());
    CHECK_FALSE(refusal(c).empty());
  }
}

/* Pins: a line of more than six values is named by its key and first three
 * values and the number of the others, as a refusal of a retired key names
 * one: the SIGMA line of a potential of Tadah! 1.5.0 (SIGMA 3 and nine
 * values) is "SIGMA 3 5.4833944e-09 3.2e-09 and 7 more values (line 2 of
 * <file>)". inert_keys_text(lines, true), the sentence a fit adds to its
 * refusal of retired keys, says "the configuration also has a line that
 * changes nothing a fit gives, and a fit refuses it: SIGMA 3 ...";
 * inert_keys_refusal is "<who>: " and inert_keys_text(lines, false). The
 * sentence after a refusal of retired keys ends "With this line deleted as
 * well, the configuration trains." (that refusal says the configuration
 * trains with its lines converted); the refusal of these lines alone ends
 * "With this line deleted, the fit is the same."
 * Before the fix: does not compile (no inert_keys_text); the SIGMA line was
 * named with all its values. Before the ending was added: both sentences
 * ended "the fit is the same". */
TEST_CASE("inert keys: a long line is named by its first values, and the "
          "sentence after a refusal of retired keys says 'also'",
          "[inert_keys]") {
  tadah::core::PeriodicTable::initialize();
  const ScratchDir dir;
  Quiet quiet;
  const std::string path = dir.write_file(
      "sigma.tadah",
      std::string("ATOM Ta\nSIGMA 3 5.4833944e-09 3.2e-09 1 2 3 4 5 6 7\n") +
          kLj);
  const Context c = CoreUserConfigParser().parse(path);
  const std::vector<InertKeyLine> lines = inert_key_lines(c);
  REQUIRE(lines.size() == 1u);
  CHECK(lines[0].key == "SIGMA");
  CHECK(lines[0].text == "SIGMA 3 5.4833944e-09 3.2e-09 and 7 more values");
  CHECK(lines[0].where == "line 2 of " + path);
  CHECK_THAT(inert_keys_text(lines, true),
             Catch::StartsWith("the configuration also has a line that "
                               "changes nothing a fit gives, and a fit "
                               "refuses it: SIGMA 3 5.4833944e-09 3.2e-09 "
                               "and 7 more values (line 2 of " +
                               path + "): "));
  CHECK(inert_keys_refusal(lines, "tadah train") ==
        "tadah train: " + inert_keys_text(lines, false));
  // After a refusal of retired keys, which ends "the configuration trains",
  // the sentence says so of these lines too.
  CHECK_THAT(inert_keys_text(lines, true),
             Catch::EndsWith(". With this line deleted as well, the "
                             "configuration trains."));
  CHECK_THAT(inert_keys_text(lines, false),
             Catch::EndsWith(". With this line deleted, the fit is the "
                             "same."));
}

/* Pins: with InertKeyRules::watom_never_read set (a fit that writes its own
 * WATOM, as tadah refit writes WATOM 1), WATOM with a value other than 1 is
 * a line that changes nothing whatever the descriptor lines say, with that
 * reason: WATOM 2.5 beside D2_LJ is named ("WATOM 2.5 (line 8 of <file>):
 * tadah refit writes WATOM 1"); WATOM 1 is not.
 * Before the fix: does not compile (no watom_never_read). */
TEST_CASE("inert keys: a fit that writes its own WATOM refuses another",
          "[inert_keys][watom]") {
  tadah::core::PeriodicTable::initialize();
  const ScratchDir dir;
  Quiet quiet;
  InertKeyRules refit;
  refit.ream_keys = false;
  refit.watom_never_read = "tadah refit writes WATOM 1";
  const std::string path =
      dir.write_file("refit.train", std::string(kLj) + "WATOM 2.5\n");
  const std::string what =
      refusal(CoreUserConfigParser().parse(path), refit);
  INFO("refusal: " << what);
  CHECK_THAT(what, Catch::Contains("WATOM 2.5 (line 8 of " + path +
                                   "): tadah refit writes WATOM 1"));
  const std::string one =
      dir.write_file("refit1.train", std::string(kLj) + "WATOM 1\n");
  CHECK(refusal(CoreUserConfigParser().parse(one), refit).empty());
}

/* Pins: a refusal of several lines separates them with "; ", and the
 * reason of a deprecated key's line (its blueprint text) has no ";", so
 * that each "; " of the refusal begins the next line: a configuration with
 * MBLOCK 64, NBLOCK 8, MPIWPCKG 50, SIGMA 1 0.5 and RHOFORM SIGNED (no
 * DM_REAM block) is refused with a text that "; " splits into five parts,
 * the first naming MBLOCK 64 and each other beginning with the key of its
 * line. The text of every deprecated key of the blueprint has no ";" and
 * says "A fit refuses it.", so that the notice of a potential's line,
 * which quotes the same text, says what a fit does with it.
 * Before the fix: the texts of MPIWPCKG, MBLOCK, NBLOCK and SIGMA said "A
 * potential that has it still loads, its value ignored; a fit refuses it",
 * so the refusal split into nine parts and a line's reason ran into the
 * next line ("... its value ignored; a fit refuses it; NBLOCK 8 ..."). */
TEST_CASE("inert keys: each '; ' of a refusal of several lines begins the "
          "next line",
          "[inert_keys]") {
  tadah::core::PeriodicTable::initialize();
  const ScratchDir dir;
  Quiet quiet;
  const std::string path = dir.write_file(
      "several.train", std::string(kLj) +
                           "MBLOCK 64\nNBLOCK 8\nMPIWPCKG 50\nSIGMA 1 0.5\n"
                           "RHOFORM SIGNED\n");
  const std::string what = refusal(CoreUserConfigParser().parse(path));
  INFO("refusal: " << what);
  std::vector<std::string> parts;
  for (std::size_t from = 0;;) {
    const std::size_t at = what.find("; ", from);
    parts.push_back(what.substr(from, at == std::string::npos
                                          ? std::string::npos
                                          : at - from));
    if (at == std::string::npos) break;
    from = at + 2;
  }
  REQUIRE(parts.size() == 5u);
  CHECK_THAT(parts[0], Catch::Contains("MBLOCK 64 (line 8 of " + path + ")"));
  CHECK_THAT(parts[1], Catch::StartsWith("NBLOCK 8 (line 9 of " + path + ")"));
  CHECK_THAT(parts[2],
             Catch::StartsWith("MPIWPCKG 50 (line 10 of " + path + ")"));
  CHECK_THAT(parts[3],
             Catch::StartsWith("SIGMA 1 0.5 (line 11 of " + path + ")"));
  CHECK_THAT(parts[4],
             Catch::StartsWith("RHOFORM SIGNED (line 12 of " + path + ")"));

  std::size_t deprecated = 0;
  for (const auto &entry : tadah::core::BlueprintParser::getKeysMapRef()) {
    if (!entry.second.isDeprecated()) continue;
    ++deprecated;
    INFO(entry.first << ": " << entry.second.deprecated);
    CHECK(entry.second.deprecated.find(';') == std::string::npos);
    CHECK_THAT(entry.second.deprecated, Catch::Contains("A fit refuses it."));
  }
  CHECK(deprecated >= 4u);
}
