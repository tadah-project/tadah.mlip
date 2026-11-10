// pair_style tadah (tadah::LammpsTadah, the object pair_coeff builds from
// the potential file) and the training record of a potential: the comment
// block that tadah train, tadah hpo and tadah refit write above the key
// lines, naming the run's DBFILE, the keys that prepared its training data
// (EFILTER, FFILTER, ESHIFT_ATOM, ESHIFT_DBATOM, ZERO_COM_FORCE, WDBFILE),
// VALIDATION, HPOTARGET, VERBOSE, NUMERIC, OUTFILE, AUDIT, AUDIT_SAMPLE
// and HEALTH_LOG for the user's record. The block is
// comments only: pair_coeff reads neither its keys nor its values.

#include "catch2/catch.hpp"

#include <tadah/core/context.h>
#include <tadah/core/key_notice.h>
#include <tadah/core/logger.h>
#include <tadah/core/periodic_table.h>
#include <tadah/tadah.lammps/lammps_tadah.h>

#include <cstdio>
#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

namespace {

/* A file unique to this process, removed with the object. */
struct TempFile {
  std::string path;
  TempFile(const std::string &stem, const std::string &body)
      : path(stem + "_" + std::to_string(::getpid())) {
    std::ofstream(path) << body;
  }
  ~TempFile() { std::remove(path.c_str()); }
};

/* A Ta pair potential (D2_BP, two functions) as tadah train writes it. */
std::string pair_potential() {
  tadah::core::Context c;
  c.add("ATOM", "Ta");
  c.add("WATOM", 73);
  c.add("BIAS", "false");
  c.add("NORM", "false");
  c.add("INIT2B", "true");
  c.add("INITMB", "false");
  c.add("TYPE2B", "D2_BP 2 2 Ta Ta");
  c.add("CGRID2B", "2.6 3.3");
  c.add("SGRID2B", "1.0 1.0");
  c.add("RCTYPE2B", "Cut_Cos");
  c.add("RCUT2B", 5.0);
  c.add("MODEL", "M_BLR BF_Linear");
  c.add("WEIGHTS", std::vector<double>{0.25, -0.5});
  std::ostringstream os;
  c.write_potential(os, 18);
  return os.str();
}

/* The training record of a run whose configuration set every key a
 * potential keeps only there, VERBOSE 2 among them (a key line VERBOSE 2
 * would set the Tadah log level of the process), with a DBFILE that does
 * not exist, as tadah writes it (its opening line once said "(not read
 * by tadah)"). */
const char *const kRecord =
    "# --- training record (no key is read from it) ---\n"
    "# DBFILE no_such_dataset.db other.db\n"
    "# EFILTER -12 -2\n"
    "# FFILTER 20\n"
    "# ESHIFT_ATOM true\n"
    "# ESHIFT_DBATOM true\n"
    "# ZERO_COM_FORCE true\n"
    "# WDBFILE 1 0\n"
    "# VALIDATION config.val\n"
    "# HPOTARGET config.hpo\n"
    "# VERBOSE 2\n"
    "# NUMERIC 9\n"
    "# OUTFILE pot.tadah\n"
    "# AUDIT error\n"
    "# AUDIT_SAMPLE 64\n"
    "# HEALTH_LOG full\n"
    "# --- end of training record ---\n";

/* pair_coeff * * <file> Ta: LammpsTadah from the file, then the type map. */
std::unique_ptr<tadah::LammpsTadah> load(const std::string &path) {
  std::vector<std::string> args = {"*", "*", path, "Ta"};
  std::vector<char *> argv;
  for (auto &a : args) argv.push_back(&a[0]);
  std::unique_ptr<tadah::LammpsTadah> lt(
      new tadah::LammpsTadah(static_cast<int>(argv.size()), argv.data()));
  lt->initialize(static_cast<int>(argv.size()), argv.data());
  return lt;
}

}  // namespace

/* Pins: pair_coeff loads a potential whose key lines follow its training
 * record (DBFILE of a dataset that does not exist, the six keys that
 * prepared the training data, VALIDATION, HPOTARGET, VERBOSE 2, NUMERIC
 * 9, OUTFILE, AUDIT error, AUDIT_SAMPLE 64, HEALTH_LOG full) to the same
 * context, key by key, as the same key lines without the block (a
 * potential without a record, as every earlier version wrote, loads as
 * before), with none of the fifteen keys but VERBOSE, which the
 * model's construction stores at its default as for the potential without
 * the block (not the record's 2), and leaves the Tadah log level where the
 * potential without the block leaves it.
 * Before the fix: passes as well (a pin: core::CoreUserConfigParser, the
 * reader of pair_coeff, skips every line whose first non-blank character
 * is '#', and the training record is a block of such lines, whatever its
 * opening line says). */
TEST_CASE("pair_style tadah ignores the training record of a potential",
          "[lammps_tadah][training_record]") {
  tadah::core::PeriodicTable::initialize();
  auto &log = tadah::core::Logger::instance();
  const auto old_level = log.getLogLevel();
  const std::string body = pair_potential();
  const TempFile bare("training_record_bare.tadah", body);
  const TempFile with("training_record_with.tadah", kRecord + body);

  log.setLogLevel(tadah::core::LogLevel::WARNING);
  std::unique_ptr<tadah::LammpsTadah> lt_bare = load(bare.path);
  const auto level_bare = log.getLogLevel();
  log.setLogLevel(tadah::core::LogLevel::WARNING);
  std::unique_ptr<tadah::LammpsTadah> lt_with = load(with.path);
  const auto level_with = log.getLogLevel();
  log.setLogLevel(old_level);

  REQUIRE(lt_bare != nullptr);
  REQUIRE(lt_with != nullptr);
  CHECK(lt_with->c.storage_snapshot() == lt_bare->c.storage_snapshot());
  for (const char *key :
       {"DBFILE", "EFILTER", "FFILTER", "ESHIFT_ATOM", "ESHIFT_DBATOM",
        "ZERO_COM_FORCE", "WDBFILE", "VALIDATION", "HPOTARGET", "NUMERIC",
        "OUTFILE", "AUDIT", "AUDIT_SAMPLE", "HEALTH_LOG"}) {
    INFO("key " << key);
    CHECK_FALSE(lt_with->c.has_stored(key));
  }
  // The model's construction stores VERBOSE at its blueprint default, as
  // for the potential without the block: not the record's 2.
  CHECK(lt_with->c.stored_value("VERBOSE") ==
        lt_bare->c.stored_value("VERBOSE"));
  CHECK(lt_with->c.stored_value("VERBOSE") != std::optional<std::string>("2"));
  CHECK(level_with == level_bare);
}

/* The key lines of the run settings a potential of an earlier version can
 * have (every 1.5.x best_pot.tadah has HPOTARGET, VERBOSE and NUMERIC, and
 * VALIDATION when its run had a validation file), VERBOSE 2 among them. */
static const char *const kOldRunSettings = "VALIDATION config.val\n"
                                           "HPOTARGET config.hpo\n"
                                           "VERBOSE 2\n"
                                           "NUMERIC 12\n"
                                           "OUTFILE pot.tadah\n"
                                           "AUDIT warn\n"
                                           "AUDIT_SAMPLE 2\n"
                                           "HEALTH_LOG full\n";

/* Pins: pair_coeff loads a potential of an earlier version whose key lines
 * include the run settings of the run that wrote it -- VALIDATION,
 * HPOTARGET, VERBOSE 2, NUMERIC 12, OUTFILE, AUDIT, AUDIT_SAMPLE and
 * HEALTH_LOG -- to the same context, key by key, as the potential without
 * them, so the model's VERBOSE is the one its construction stores (not
 * 2), and the Tadah log level stays where LAMMPS has it (ERROR, the
 * Logger's own; pair_style tadah sets none), with no notice: the lines are
 * not read (core::run_setting_keys()).
 * Before the fix: core::CoreUserConfigParser stored the eight, and adding
 * VERBOSE 2 to the context set the Tadah log level of the LAMMPS process to
 * INFO (Tadah's INFO lines on every rank's standard output for the rest of
 * the run) and the model's verbosity to 2: the context, VERBOSE and level
 * checks fail. */
TEST_CASE("pair_style tadah reads no run setting of an older potential, and "
          "its VERBOSE sets no log level",
          "[lammps_tadah][training_record][run_settings]") {
  tadah::core::PeriodicTable::initialize();
  auto &log = tadah::core::Logger::instance();
  const auto old_level = log.getLogLevel();
  const std::string body = pair_potential();
  const TempFile bare("run_settings_bare.tadah", body);
  const TempFile older("run_settings_older.tadah", body + kOldRunSettings);

  log.setLogLevel(tadah::core::LogLevel::ERROR);
  std::unique_ptr<tadah::LammpsTadah> lt_bare = load(bare.path);
  std::vector<std::string> notices;
  std::unique_ptr<tadah::LammpsTadah> lt_older;
  {
    tadah::core::IgnoredKeyNoticeCapture capture;  // as pair_coeff loads
    lt_older = load(older.path);
    notices = capture.notices();
  }
  const auto level = log.getLogLevel();
  log.setLogLevel(old_level);

  REQUIRE(lt_bare != nullptr);
  REQUIRE(lt_older != nullptr);
  CHECK(level == tadah::core::LogLevel::ERROR);
  CHECK(notices.empty());
  CHECK(lt_older->c.storage_snapshot() == lt_bare->c.storage_snapshot());
  for (const char *key : {"VALIDATION", "HPOTARGET", "NUMERIC", "OUTFILE",
                          "AUDIT", "AUDIT_SAMPLE", "HEALTH_LOG"}) {
    INFO("key " << key);
    CHECK_FALSE(lt_older->c.has_stored(key));
  }
  CHECK(lt_older->c.stored_value("VERBOSE") !=
        std::optional<std::string>("2"));
}
