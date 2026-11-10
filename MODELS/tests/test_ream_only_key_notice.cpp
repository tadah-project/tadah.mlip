// EMBF0 and RHOFORM apply only to a DM_REAM many-body block. In a potential
// or configuration without one they have no effect: the key is accepted,
// dropped from every written potential, and reported on standard error once
// per key and process (core::report_ignored_key_once, the notice deprecated
// keys get), whatever the Logger level -- not as a Logger WARNING, which
// pair_style tadah (Logger at ERROR) and VERBOSE 0 hide.

#include "catch2/catch.hpp"

#include <tadah/core/context.h>
#include <tadah/core/core_user_config_parser.h>
#include <tadah/core/key_notice.h>
#include <tadah/core/logger.h>
#include <tadah/core/periodic_table.h>
#include <tadah/models/dc_selector.h>
#include <tadah/models/descriptors/dm/embf0.h>
#include <tadah/models/descriptors/dm/rhoform.h>

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using tadah::core::Context;
using tadah::core::LogLevel;
using tadah::core::Logger;

namespace {

/* Captures std::cerr (the notice) and std::cout (the Logger) and sets the
 * Logger level; restores all three afterwards. */
class StreamCapture {
public:
  explicit StreamCapture(LogLevel level)
      : old_level_(Logger::instance().getLogLevel()),
        old_out_(std::cout.rdbuf(out_.rdbuf())),
        old_err_(std::cerr.rdbuf(err_.rdbuf())) {
    Logger::instance().setLogLevel(level);
  }
  ~StreamCapture() {
    std::cout.rdbuf(old_out_);
    std::cerr.rdbuf(old_err_);
    Logger::instance().setLogLevel(old_level_);
  }
  StreamCapture(const StreamCapture &) = delete;
  StreamCapture &operator=(const StreamCapture &) = delete;

  std::string err() const { return err_.str(); }
  std::string out() const { return out_.str(); }

private:
  std::ostringstream out_;
  std::ostringstream err_;
  LogLevel old_level_;
  std::streambuf *old_out_;
  std::streambuf *old_err_;
};

std::size_t count_of(const std::string &text, const std::string &needle) {
  std::size_t n = 0;
  for (std::size_t p = text.find(needle); p != std::string::npos;
       p = text.find(needle, p + needle.size()))
    ++n;
  return n;
}

/* A D2_Blip potential without a many-body block, as a file would give it. */
Context pair_potential() {
  Context c;
  c.add("ATOM", "Ta");
  c.add("INIT2B", "true");
  c.add("INITMB", "false");
  c.add("TYPE2B", "D2_Blip 3 3 Ta Ta");
  c.add("CGRID2B", "2.2 3.2 4.2");
  c.add("SGRID2B", "1.0 1.0 1.0");
  c.add("RCTYPE2B", "Cut_Cos");
  c.add("RCUT2B", 5.0);
  c.add("VERBOSE", 0);
  return c;
}

const char *kReason =
    " is ignored: it applies only to a DM_REAM many-body block, and the "
    "potential has none";

}  // namespace

TEST_CASE("EMBF0 and RHOFORM without a DM_REAM block: one stderr notice per "
          "key and process",
          "[embf0][rhoform][key_notice]") {
  tadah::core::PeriodicTable::initialize();
  Context pot = pair_potential();
  pot.add("EMBF0", "FREE");
  pot.add("RHOFORM", "NONNEG");
  Context task;
  task.add("EMBF0", "ZERO");
  task.add("RHOFORM", "MONOTONE");

  // Earlier test cases in this process may have reported a key already:
  // then nothing more may be written for it.
  const bool embf0_before = tadah::core::ignored_key_reported("EMBF0");
  const bool rhoform_before = tadah::core::ignored_key_reported("RHOFORM");

  std::string err, out;
  {
    // WARNING: a Logger line would reach standard output
    StreamCapture cap(LogLevel::WARNING);
    // two loads of the potential (every model is built by DC_Selector)
    { tadah::models::DC_Selector first(pot); }
    { tadah::models::DC_Selector second(pot); }
    // the engines that merge a configuration over a potential (refit
    // --edit, analysis bfunc)
    tadah::models::check_embf0_precedence(pot, task, "pot.tadah");
    tadah::models::check_rhoform_precedence(pot, task, "pot.tadah");
    // the writers drop both keys
    Context written = pot;
    tadah::models::normalise_embf0_key(written);
    tadah::models::normalise_rhoform_key(written);
    CHECK_FALSE(written.has_stored("EMBF0"));
    CHECK_FALSE(written.has_stored("RHOFORM"));
    err = cap.err();
    out = cap.out();
  }
  INFO("stderr:\n" << err << "stdout:\n" << out);
  CHECK(count_of(err, std::string("WARNING: Key EMBF0") + kReason) ==
        (embf0_before ? 0u : 1u));
  CHECK(count_of(err, std::string("WARNING: Key RHOFORM") + kReason) ==
        (rhoform_before ? 0u : 1u));
  CHECK(tadah::core::ignored_key_reported("EMBF0"));
  CHECK(tadah::core::ignored_key_reported("RHOFORM"));
  // not a Logger message as well
  CHECK(count_of(out, "EMBF0") == 0);
  CHECK(count_of(out, "RHOFORM") == 0);
}

/* Pins: an EMBF0 or RHOFORM of a run's configuration over a potential
 * without a DM_REAM block gives the one notice per key and process of a
 * key without effect, as one in the potential does.
 * Before the fix: passes as well (a pin). */
TEST_CASE("An EMBF0 or RHOFORM of a configuration over a potential without "
          "a DM_REAM block is reported like one in the potential",
          "[embf0][rhoform][key_notice]") {
  // The configuration's keys alone (the potential has neither): still the
  // one notice.
  Context pot = pair_potential();
  Context task;
  task.add("EMBF0", "FREE");
  task.add("RHOFORM", "SIGNED");
  const bool embf0_before = tadah::core::ignored_key_reported("EMBF0");
  const bool rhoform_before = tadah::core::ignored_key_reported("RHOFORM");
  std::string err;
  {
    StreamCapture cap(LogLevel::ERROR);  // as in LAMMPS
    REQUIRE_NOTHROW(
        tadah::models::check_embf0_precedence(pot, task, "pot.tadah"));
    REQUIRE_NOTHROW(
        tadah::models::check_rhoform_precedence(pot, task, "pot.tadah"));
    REQUIRE_NOTHROW(
        tadah::models::check_rhoform_precedence(pot, task, "pot.tadah"));
    err = cap.err();
  }
  INFO("stderr:\n" << err);
  CHECK(count_of(err, std::string("WARNING: Key EMBF0") + kReason) ==
        (embf0_before ? 0u : 1u));
  CHECK(count_of(err, std::string("WARNING: Key RHOFORM") + kReason) ==
        (rhoform_before ? 0u : 1u));
  CHECK(tadah::core::ignored_key_reported("EMBF0"));
  CHECK(tadah::core::ignored_key_reported("RHOFORM"));
}

TEST_CASE("A potential with a deprecated key and EMBF0 without DM_REAM, read "
          "twice: each notice once per process",
          "[embf0][key_notice][deprecated]") {
  /* One rule for every ignored key, deprecated
   * (MBLOCK) or without effect (EMBF0 without DM_REAM): one stderr notice
   * per key and process. */
  tadah::core::PeriodicTable::initialize();
  const std::vector<std::string> file = {
      "ATOM Ta",        "INIT2B true",     "INITMB false",
      "TYPE2B D2_Blip 3 3 Ta Ta",          "CGRID2B 2.2 3.2 4.2",
      "SGRID2B 1.0 1.0 1.0",               "RCTYPE2B Cut_Cos",
      "RCUT2B 5.0",     "MBLOCK 64",       "EMBF0 FREE",
      "VERBOSE 0"};
  const bool mblock_before = tadah::core::ignored_key_reported("MBLOCK");
  const bool embf0_before = tadah::core::ignored_key_reported("EMBF0");
  std::string err;
  {
    StreamCapture cap(LogLevel::ERROR);  // as in LAMMPS
    for (int load = 0; load < 2; ++load) {
      Context pot = tadah::core::CoreUserConfigParser().parse(file);
      CHECK_FALSE(pot.has_stored("MBLOCK"));
      tadah::models::DC_Selector model(pot);
    }
    err = cap.err();
  }
  INFO("stderr:\n" << err);
  CHECK(count_of(err, "WARNING: Deprecated key MBLOCK is ignored") ==
        (mblock_before ? 0u : 1u));
  CHECK(count_of(err, std::string("WARNING: Key EMBF0") + kReason) ==
        (embf0_before ? 0u : 1u));
  CHECK(tadah::core::ignored_key_reported("MBLOCK"));
  CHECK(tadah::core::ignored_key_reported("EMBF0"));
}
