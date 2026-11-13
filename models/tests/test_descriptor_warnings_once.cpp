// test_descriptor_warnings_once.cpp
//
// The warnings a descriptor gives about its configuration when it is built
// (a knot past the cutoff, a bump whose support reaches past a Cut_Dummy
// cutoff, a non-compact basis with a Cut_Dummy cutoff, a channel function
// that is not rotationally invariant for L > 0) are logged once per
// process, as the notice of a key without effect is: tadah hpo builds the
// descriptors of every evaluation anew, and a warning per descriptor object
// was repeated at every evaluation.
//
// Each case records whether its warning was already logged in this process
// (warning_logged_once) before it builds anything: in a CTest entry, a
// process of its own, it was not, and the case requires the warning exactly
// once; run in one process with other cases that build the same
// configuration, it requires none. Either way the warning is logged in the
// process when the case ends, so a warning that is never logged fails.
#include "catch2/catch.hpp"
#include <tadah/core/context.h>
#include <tadah/core/logger.h>
#include <tadah/core/periodic_table.h>
#include <tadah/models/dc_selector.h>
#include <tadah/models/descriptors/basis_kind.h>
#include <tadah/models/descriptors/d2/d2_blip.h>
#include <tadah/models/descriptors/d2/d2_knot5.h>
#include <tadah/models/descriptors/dm/dm_ream.h>
#include <tadah/models/descriptors/warn_once.h>

#include <iostream>
#include <sstream>
#include <string>

using tadah::core::Context;

namespace {

/* Captures std::cout, where the Logger writes, while it lives, with the
 * Logger at @p level (WARNING: the level of the tadah CLI). */
class CoutCapture {
public:
  explicit CoutCapture(
      tadah::core::LogLevel level = tadah::core::LogLevel::WARNING)
      : old_(std::cout.rdbuf(out_.rdbuf())),
        level_(tadah::core::Logger::instance().getLogLevel()) {
    tadah::core::Logger::instance().setLogLevel(level);
  }
  ~CoutCapture() {
    tadah::core::Logger::instance().setLogLevel(level_);
    std::cout.rdbuf(old_);
  }
  CoutCapture(const CoutCapture &) = delete;
  CoutCapture &operator=(const CoutCapture &) = delete;
  std::string str() const { return out_.str(); }

private:
  std::ostringstream out_;
  std::streambuf *old_;
  tadah::core::LogLevel level_;
};

std::size_t occurrences(const std::string &text, const std::string &needle) {
  std::size_t n = 0;
  for (std::size_t p = text.find(needle); p != std::string::npos;
       p = text.find(needle, p + needle.size()))
    ++n;
  return n;
}

/* The warning of @p key, whose text contains @p needle, appears in @p out
 * once when it had not been logged in this process before the case built
 * anything (@p before false), else not at all; and it is logged in the
 * process now. */
void check_logged_once(const std::string &out, const std::string &needle,
                       const std::string &key, bool before) {
  INFO(out);
  INFO("logged before this case: " << (before ? "yes" : "no"));
  CHECK(occurrences(out, needle) == (before ? 0u : 1u));
  CHECK(tadah::models::warning_logged_once(key));
}

/* A two-body H-H context: TYPE2B <label> <sizes...> H H, the grids given,
 * RCTYPE2B and RCUT2B. */
Context two_body(const std::string &label, const std::string &sizes,
                 const std::string &cgrid, const std::string &sgrid,
                 const std::string &rctype, double rcut) {
  Context c;
  c.add("ATOM", "H");
  c.add("WATOM", 1);
  c.remove("INIT2B");
  c.add("INIT2B", "true");
  c.add("VERBOSE", 0);
  c.add("CGRID2B", cgrid);
  if (!sgrid.empty()) c.add("SGRID2B", sgrid);
  c.remove("RCTYPE2B");
  c.add("RCTYPE2B", rctype);
  c.add("RCUT2B", rcut);
  c.add("TYPE2B", label + " " + sizes + " H H");
  return c;
}

/* A many-body Ta context, no two-body term: TYPEMB @p typemb, RCUTMB 5.0
 * with Cut_Cos. */
Context many_body(const std::string &typemb) {
  Context c;
  c.add("ATOM", "Ta");
  c.add("WATOM", 1);
  c.remove("INIT2B");
  c.add("INIT2B", "false");
  c.remove("INITMB");
  c.add("INITMB", "true");
  c.add("VERBOSE", 0);
  c.remove("RCTYPEMB");
  c.add("RCTYPEMB", "Cut_Cos");
  c.add("RCUTMB", 5.0);
  c.add("TYPEMB", typemb);
  return c;
}

}  // namespace

/* Pins: three D2_Knot5 descriptors built from one configuration whose
 * largest CGRID2B knot (4.5) lies past RCUT2B (4.0) log the warning
 * "D2_Knot5: the largest CGRID2B knot, 4.5, lies beyond RCUT2B, 4, so the
 * knot functions are cut off at RCUT2B; an RCUT2B at the largest knot
 * keeps them whole." exactly once between them (none when this process
 * logged it before the case), and the warning is logged in the process
 * afterwards (check_logged_once). The warning comes only where tadah keeps
 * the stated RCUT2B (a D2_mJoin line, or a program or pair_style tadah that
 * reads a potential file), so it says what the cutoff does and gives the
 * numbers as written.
 * Before the fix: once per descriptor object, three times (and once per
 * evaluation of a tadah hpo run that builds a model per evaluation). A
 * log_warning_once that logs nothing fails it as well: 0 occurrences and
 * warning_logged_once false. The text read "... knot (4.500000) lies beyond
 * RCUT2B (4.000000) — its contribution is silently hard-truncated at the
 * cutoff. ...: set RCUT2B to the largest knot (refit/edit/hpo tooling
 * derives it automatically).", which a D2_mJoin line, where the warning
 * comes, does not do. */
TEST_CASE("D2_Knot5 warns once per process of a knot past RCUT2B",
          "[descriptor][warn_once]") {
  tadah::core::PeriodicTable::initialize();
  const Context c =
      two_body("D2_Knot5", "3 0", "1.0 2.0 4.5", "", "Cut_Cos", 4.0);
  const std::string key = "D2_Knot5: knot past RCUT2B";
  const bool before = tadah::models::warning_logged_once(key);
  CoutCapture out;
  for (int k = 0; k < 3; ++k) tadah::models::D2_Knot5 d(c);
  check_logged_once(out.str(),
                    "D2_Knot5: the largest CGRID2B knot, 4.5, lies beyond "
                    "RCUT2B, 4, so the knot functions are cut off at RCUT2B; "
                    "an RCUT2B at the largest knot keeps them whole.",
                    key, before);
}

/* Pins: three D2_Blip descriptors built from one configuration with a
 * Cut_Dummy cutoff and a bump whose support (centre 3.5 + 2/eta = 4.5)
 * reaches past RCUT2B (4.0) log the warning "has support reaching ...,
 * past RCUT2B" exactly once between them (check_logged_once).
 * Before the fix: three times, once per descriptor object; a
 * log_warning_once that logs nothing fails it too (0 occurrences). */
TEST_CASE("D2_Blip warns once per process of a bump past a Cut_Dummy "
          "RCUT2B",
          "[descriptor][warn_once]") {
  tadah::core::PeriodicTable::initialize();
  const Context c =
      two_body("D2_Blip", "2 2", "1.0 3.5", "2.0 2.0", "Cut_Dummy", 4.0);
  const std::string key = "D2_Blip: bump support past a Cut_Dummy RCUT2B";
  const bool before = tadah::models::warning_logged_once(key);
  CoutCapture out;
  for (int k = 0; k < 3; ++k) tadah::models::D2_Blip d(c);
  check_logged_once(out.str(), "has support reaching", key, before);
}

/* Pins: three DC_Selectors (the descriptors of three models) built from one
 * configuration with the Gaussian basis of D2_BP and a Cut_Dummy cutoff
 * log the warning "uses a non-compact Gaussian basis with a discontinuous
 * (Cut_Dummy) cutoff" exactly once between them (check_logged_once).
 * Before the fix: three times, once per DC_Selector; a log_warning_once
 * that logs nothing fails it too (0 occurrences). */
TEST_CASE("A non-compact basis with Cut_Dummy is warned of once per "
          "process",
          "[descriptor][warn_once]") {
  tadah::core::PeriodicTable::initialize();
  Context c =
      two_body("D2_BP", "2 2", "1.0 2.0", "1.0 1.0", "Cut_Dummy", 4.0);
  c.remove("INITMB");
  c.add("INITMB", "false");
  const std::string key = "D2_BP: non-compact basis with a Cut_Dummy cutoff";
  const bool before = tadah::models::warning_logged_once(key);
  CoutCapture out;
  for (int k = 0; k < 3; ++k) tadah::models::DC_Selector s(c);
  check_logged_once(out.str(), "uses a non-compact", key, before);
}

/* Pins: three DM_REAM descriptors built from one configuration with a
 * knot density (Knot5 family) whose largest CGRIDMB knot (5.5) lies past
 * RCUTMB (5.0) log the warning "the largest CGRIDMB density knot ... lies
 * beyond RCUTMB" exactly once between them (check_logged_once).
 * Before the fix: three times, once per descriptor object (main's
 * DM_REAM::warn_if_knots_beyond_rcut logged on every call). */
TEST_CASE("DM_REAM warns once per process of a density knot past RCUTMB",
          "[descriptor][warn_once]") {
  tadah::core::PeriodicTable::initialize();
  using tadah::models::BasisKind;
  // TYPEMB DM_REAM <basis> nC nS nA nCE nSE nKD nKDE Ta Ta: six density
  // knots of degrees KDEGMB, three embedding knots.
  Context c = many_body("DM_REAM");
  c.add("TYPEMB", tadah::models::basis_code(BasisKind::Knot5));
  for (int n : {6, 0, 6, 3, 0, 6, 0}) c.add("TYPEMB", n);
  c.add("TYPEMB", "Ta");
  c.add("TYPEMB", "Ta");
  c.add("CGRIDMB", "2.5 3.1 3.7 4.2 4.9 5.5");
  c.add("AMPGRIDMB", "0.09 -0.035 0.06 -0.02 0.015 -0.005");
  c.add("KDEGMB", "5 3 5 3 5 5");
  c.add("CEMBFUNC", "0.0 0.5 1.0");
  c.add("RHOFORM", "SIGNED");
  const std::string key = "DM_REAM: density knot past RCUTMB";
  const bool before = tadah::models::warning_logged_once(key);
  CoutCapture out;
  for (int k = 0; k < 3; ++k) tadah::models::DM_REAM d(c);
  check_logged_once(out.str(), "lies beyond RCUTMB", key, before);
}

/* Pins: three DC_Selectors built from a DM_mBlipEnv configuration with
 * Lmax 1 (a DM_mEAD descriptor whose channel function is not F_SQ) log the
 * warning "DM_mBlipEnv: the channel function is not F_SQ, so the
 * descriptor is not rotationally invariant for L>0 (the DM_PS_ descriptors
 * are). Its stress is not the strain derivative of the energy (the virial
 * is not symmetric)." exactly once between them (check_logged_once): the
 * warning names the descriptor by its registered name, the TYPEMB value.
 * Before the fix: "channel function is not F_SQ — descriptor
 * is NOT rotationally invariant for L>0. Use DM_PS_* descriptors instead."
 * (no occurrence of the pinned text).
 * Before the fix: three times, once per descriptor object (a tadah hpo
 * run printed it once per evaluation). Before the stress clause was added:
 * the warning did not mention the stress. Before the warning was
 * reordered: "... NOT rotationally invariant for L>0. Use DM_PS_*
 * descriptors instead. The stress is then not the strain derivative ...",
 * whose "then" read as if it followed from the remedy. Before the label
 * was the registered name: "DM_mEAD: the channel function ...", the
 * family name, for every DM_mEAD descriptor (key "DM_mEAD: channel ..."). */
TEST_CASE("DM_mEAD warns once per process of a channel that is not "
          "rotationally invariant",
          "[descriptor][warn_once]") {
  tadah::core::PeriodicTable::initialize();
  Context c = many_body("DM_mBlipEnv 1 1 1 5 5 Ta Ta");
  c.add("CGRIDMB", "2.88");
  c.add("SGRIDMB", "8.0");
  c.add("CEMBFUNC", "0.5 0.75 1.0 1.25 1.5");
  c.add("SEMBFUNC", "2.0 2.0 2.0 2.0 2.0");
  c.add("RCUTENV", 0.6);
  const std::string key =
      "DM_mBlipEnv: channel not rotationally invariant for L>0";
  const bool before = tadah::models::warning_logged_once(key);
  CoutCapture out;
  for (int k = 0; k < 3; ++k) tadah::models::DC_Selector s(c);
  check_logged_once(out.str(),
                    "DM_mBlipEnv: the channel function is not F_SQ, so the "
                    "descriptor is not rotationally invariant for L>0 (the "
                    "DM_PS_ descriptors are). Its stress is not the strain "
                    "derivative of the energy (the virial is not "
                    "symmetric).",
                    key, before);
}

/* Pins: three DC_Selectors built from a DM_OW_SQRT configuration with
 * Lmax 1 (a DM_Poly descriptor, OrbitalWise, whose channel function is not
 * F_SQ) log the warning "DM_OW_SQRT (OrbitalWise): the channel function is
 * not F_SQ, so the descriptor is not rotationally invariant for L>0 (the
 * PowerSpectrum mode is). Its stress is not the strain derivative of the
 * energy (the virial is not symmetric)." exactly once between them
 * (check_logged_once): the warning names the descriptor by its registered
 * name, the TYPEMB value.
 * Before the fix: "DM_Poly (OrbitalWise): channel function is
 * not F_SQ — descriptor is NOT rotationally invariant for L>0. Use
 * PowerSpectrum mode." (no occurrence of the pinned text).
 * Before the fix: three times, once per descriptor object. Before the
 * stress clause was added: the warning did not mention the stress. Before
 * the warning was reordered: "... Use PowerSpectrum mode. The stress is
 * then not the strain derivative ...". Before the label was the
 * registered name: "DM_Poly (OrbitalWise): the channel function ...", the
 * family name, for every DM_Poly descriptor (key "DM_Poly (OrbitalWise):
 * channel ..."). */
TEST_CASE("DM_Poly OrbitalWise warns once per process of a channel that is "
          "not rotationally invariant",
          "[descriptor][warn_once]") {
  tadah::core::PeriodicTable::initialize();
  Context c = many_body("DM_OW_SQRT 1 2 2 Ta Ta");
  c.add("CGRIDMB", "1 2");
  c.add("SGRIDMB", "0.1 0.2");
  c.add("SEMBFUNC", "1 1");
  const std::string key =
      "DM_OW_SQRT (OrbitalWise): channel not rotationally invariant for L>0";
  const bool before = tadah::models::warning_logged_once(key);
  CoutCapture out;
  for (int k = 0; k < 3; ++k) tadah::models::DC_Selector s(c);
  check_logged_once(out.str(),
                    "DM_OW_SQRT (OrbitalWise): the channel function is not "
                    "F_SQ, "
                    "so the descriptor is not rotationally invariant for L>0 "
                    "(the PowerSpectrum mode is). Its stress is not the "
                    "strain derivative of the energy (the virial is not "
                    "symmetric).",
                    key, before);
}

/* Pins: log_warning_once called while the Logger hides WARNINGs (level
 * ERROR) logs nothing, returns false and does not use up its key
 * (warning_logged_once stays false); the first call once WARNINGs show
 * logs the message and returns true, a second call for the key logs
 * nothing and returns false. The key is this case's own.
 * Before the fix: passes as well (a pin of the rule log_warning_once
 * documents, warn_once.h); a log_warning_once that takes the key while
 * WARNINGs are hidden fails it (the key used up, the later message never
 * logged). */
TEST_CASE("log_warning_once: a call while WARNINGs are hidden does not use "
          "up its key",
          "[warn_once]") {
  using tadah::models::log_warning_once;
  using tadah::models::warning_logged_once;
  const std::string key = "test_descriptor_warnings_once: hidden level";
  const std::string text = "warn_once hidden-level probe message";
  REQUIRE_FALSE(warning_logged_once(key));
  {
    CoutCapture hidden(tadah::core::LogLevel::ERROR);
    CHECK_FALSE(log_warning_once(key, text));
    CHECK(occurrences(hidden.str(), text) == 0);
  }
  CHECK_FALSE(warning_logged_once(key));
  CoutCapture out;
  CHECK(log_warning_once(key, text));
  CHECK_FALSE(log_warning_once(key, text));
  INFO(out.str());
  CHECK(occurrences(out.str(), text) == 1);
  CHECK(warning_logged_once(key));
}
