// D2_EAM and DM_EAM read their tabulated functions from one setfl file, named
// by the SETFL key (D2_EAM the pair function, DM_EAM the density and the
// embedding function). Each descriptor takes one TYPE2B / TYPEMB parameter,
// which the key formats require (TYPEMB admits only an unsigned integer
// there): it is a placeholder, written 0. Any other value has no effect and
// is reported on standard error once per process
// (core::report_ignored_key_once, the notice of every key without effect).
//
// The contexts below are the ones a configuration file gives, e.g.
//   TYPE2B D2_EAM 0 Ta Ta
//   TYPEMB DM_EAM 0 Ta Ta
//   SETFL  tests_data/eam.alloy
// with RCUT2B / RCUTMB the file's cutoff: the descriptors refuse another.

#include "catch2/catch.hpp"

#include <tadah/core/context.h>
#include <tadah/core/key_notice.h>
#include <tadah/core/periodic_table.h>
#include <tadah/models/cutoffs.h>
#include <tadah/models/descriptors/d2/d2_bp.h>
#include <tadah/models/descriptors/d2/d2_eam.h>
#include <tadah/models/descriptors/d2/d2_lj.h>
#include <tadah/models/descriptors/d2/d2_mjoin.h>
#include <tadah/models/descriptors/dm/dm_eam.h>
#include <tadah/models/descriptors/dm/dm_mjoin.h>
#include <tadah/models/dc_selector.h>
#include <tadah/models/eam/eam_setfl_io.h>

#include <cstdio>
#include <optional>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

using tadah::core::Context;
using tadah::models::Cut_Dummy;
using tadah::models::D2_EAM;
using tadah::models::D2_mJoin;
using tadah::models::DM_EAM;
using tadah::models::DM_mJoin;
using tadah::models::EAMSetflIO;

namespace {

const char *const kSetfl = "tests_data/eam.alloy";  // Ta, cutoff 5.58190742024
const double kSetflRcut = 5.58190742024;
const int kTa = 73;

/* Captures std::cerr (the notice) while it lives. */
class CerrCapture {
public:
  CerrCapture() : old_(std::cerr.rdbuf(err_.rdbuf())) {}
  ~CerrCapture() { std::cerr.rdbuf(old_); }
  CerrCapture(const CerrCapture &) = delete;
  CerrCapture &operator=(const CerrCapture &) = delete;
  std::string str() const { return err_.str(); }

private:
  std::ostringstream err_;
  std::streambuf *old_;
};

std::size_t count_of(const std::string &text, const std::string &needle) {
  std::size_t n = 0;
  for (std::size_t p = text.find(needle); p != std::string::npos;
       p = text.find(needle, p + needle.size()))
    ++n;
  return n;
}

/* A context with the D2_EAM pair term (`param` its TYPE2B parameter). */
Context d2_context(const std::string &param) {
  Context c;
  c.add("ATOM", "Ta");
  c.remove("INIT2B");
  c.add("INIT2B", "true");
  c.add("TYPE2B", "D2_EAM");
  c.add("TYPE2B", param);
  c.add("TYPE2B", "Ta");
  c.add("TYPE2B", "Ta");
  c.add("RCUT2B", kSetflRcut);
  c.add("SETFL", kSetfl);
  return c;
}

/* A context with the DM_EAM many-body term (`param` its TYPEMB parameter). */
Context dm_context(const std::string &param) {
  Context c;
  c.add("ATOM", "Ta");
  c.remove("INITMB");
  c.add("INITMB", "true");
  c.add("TYPEMB", "DM_EAM");
  c.add("TYPEMB", param);
  c.add("TYPEMB", "Ta");
  c.add("TYPEMB", "Ta");
  c.add("RCUTMB", kSetflRcut);
  c.add("SETFL", kSetfl);
  return c;
}

}  // namespace

/* Pins: one context with `TYPE2B D2_EAM 0 Ta Ta`, `TYPEMB DM_EAM 0 Ta Ta`
 * and one SETFL builds both descriptors (size 1 each), and both evaluate the
 * functions of that file: at r = 2.86 A (the bcc Ta nearest neighbour) for a
 * Ta-Ta pair, D2_EAM's descriptor is EAMSetflIO's phi(r) and DM_EAM's pair
 * density is EAMSetflIO's rho(r), for an EAMSetflIO that read the same file
 * (the same splines; 1e-12 relative).
 * Before the fix: DM_EAM read its setfl path from TYPEMB parameter 1, the
 * placeholder "0": "EAMSetflIO: setfl file does not exist: 0", so the
 * REQUIRE_NOTHROW for DM_EAM failed. */
TEST_CASE("DM_EAM reads its setfl file from SETFL, the file D2_EAM reads",
          "[DM][D2][eam]") {
  tadah::core::PeriodicTable::initialize();
  Context c = d2_context("0");
  c.remove("INITMB");
  c.add("INITMB", "true");
  c.add("TYPEMB", "DM_EAM");
  c.add("TYPEMB", "0");
  c.add("TYPEMB", "Ta");
  c.add("TYPEMB", "Ta");
  c.add("RCUTMB", kSetflRcut);

  EAMSetflIO eam;
  eam.read_setfl(kSetfl);
  eam.build_splines();
  REQUIRE(eam.rcut() == Approx(kSetflRcut).epsilon(1e-15));
  const double r = 2.86;

  std::unique_ptr<D2_EAM> d2;
  REQUIRE_NOTHROW(d2 = std::make_unique<D2_EAM>(c));
  Cut_Dummy fc2(kSetflRcut);
  d2->set_fcut(&fc2, false);
  CHECK(d2->size() == 1);
  tadah::core::aed_type aed(d2->size());
  aed.set_zero();
  d2->calc_aed(kTa, kTa, r, r * r, aed);
  CHECK(aed(0) == Approx(eam.phi_tab(r)).epsilon(1e-12));

  std::unique_ptr<DM_EAM> dm;
  REQUIRE_NOTHROW(dm = std::make_unique<DM_EAM>(c));
  Cut_Dummy fcm(kSetflRcut);
  dm->set_fcut(&fcm, false);
  CHECK(dm->size() == 1);
  tadah::core::rho_type rho;
  dm->init_rhoi(rho);
  dm->calc_rho(kTa, kTa, r, r * r, tadah::core::Vec3d(r, 0.0, 0.0), rho);
  CHECK(rho(0) != 0.0);
  CHECK(rho(0) == Approx(eam.rho_tab(r)).epsilon(1e-12));
}

/* Pins: a setfl path in DM_EAM's TYPEMB parameter, the form the old tests
 * used (`TYPEMB DM_EAM tests_data/eam.alloy Ta Ta`, which no configuration
 * file can hold: the TYPEMB format takes an unsigned integer there), is not
 * read: without SETFL the descriptor fails naming SETFL. A SETFL naming no
 * file fails naming that path.
 * Before the fix: the first context built a DM_EAM from the TYPEMB path
 * (no throw, so CHECK_THROWS_WITH failed), and the second failed naming the
 * placeholder ("setfl file does not exist: 0"), not path/to/nowhere. */
TEST_CASE("DM_EAM: a setfl path in the TYPEMB parameter is not read",
          "[DM][eam]") {
  tadah::core::PeriodicTable::initialize();
  Context no_setfl = dm_context(kSetfl);
  no_setfl.remove("SETFL");
  {
    CerrCapture cap;  // the placeholder notice for the path, if not yet given
    CHECK_THROWS_WITH(DM_EAM(no_setfl), Catch::Contains("SETFL"));
  }

  Context nowhere = dm_context("0");
  nowhere.remove("SETFL");
  nowhere.add("SETFL", "path/to/nowhere");
  CHECK_THROWS_WITH(DM_EAM(nowhere), Catch::Contains("path/to/nowhere"));
}

/* Pins: `TYPE2B D2_EAM 12345 Ta Ta` builds the descriptor and is reported
 * once per process on standard error, with the exact line
 *   WARNING: TYPE2B D2_EAM parameter '12345' is ignored: it is a
 *   placeholder (write 0); D2_EAM reads its setfl file from SETFL.
 * (one line), however often the descriptor is built; the placeholder 0 is
 * not reported. When an earlier test case in this process reported the
 * parameter already, nothing more may be written (as for EMBF0).
 * Before the fix: nothing was written (the parameter was dropped without a
 * word), so the count CHECK and ignored_key_reported failed. */
TEST_CASE("D2_EAM reports a TYPE2B parameter other than the placeholder 0 "
          "once",
          "[D2][eam][key_notice]") {
  tadah::core::PeriodicTable::initialize();
  const std::string key = "TYPE2B D2_EAM parameter";
  const bool before = tadah::core::ignored_key_reported(key);
  const std::string line =
      "WARNING: TYPE2B D2_EAM parameter '12345' is ignored: it is a "
      "placeholder (write 0); D2_EAM reads its setfl file from SETFL.\n";

  std::string zero, value;
  {
    CerrCapture cap;
    REQUIRE_NOTHROW(D2_EAM(d2_context("0")));
    zero = cap.str();
  }
  INFO("stderr with 0:\n" << zero);
  CHECK(count_of(zero, "is ignored") == 0);
  CHECK(tadah::core::ignored_key_reported(key) == before);
  {
    CerrCapture cap;
    REQUIRE_NOTHROW(D2_EAM(d2_context("12345")));
    REQUIRE_NOTHROW(D2_EAM(d2_context("12345")));
    value = cap.str();
  }
  INFO("stderr with 12345:\n" << value);
  CHECK(count_of(value, line) == (before ? 0u : 1u));
  CHECK(count_of(value, "is ignored") == (before ? 0u : 1u));
  CHECK(tadah::core::ignored_key_reported(key));
}

/* Pins: the same for DM_EAM: `TYPEMB DM_EAM 7 Ta Ta` gives, once per
 * process,
 *   WARNING: TYPEMB DM_EAM parameter '7' is ignored: it is a placeholder
 *   (write 0); DM_EAM reads its setfl file from SETFL.
 * and the placeholder 0 gives nothing.
 * Before the fix: DM_EAM read its setfl path from the parameter, so both
 * contexts failed ("setfl file does not exist: 0", then ": 7") and the
 * REQUIRE_NOTHROW failed. */
TEST_CASE("DM_EAM reports a TYPEMB parameter other than the placeholder 0 "
          "once",
          "[DM][eam][key_notice]") {
  tadah::core::PeriodicTable::initialize();
  const std::string key = "TYPEMB DM_EAM parameter";
  const bool before = tadah::core::ignored_key_reported(key);
  const std::string line =
      "WARNING: TYPEMB DM_EAM parameter '7' is ignored: it is a "
      "placeholder (write 0); DM_EAM reads its setfl file from SETFL.\n";

  std::string zero, value;
  {
    CerrCapture cap;
    REQUIRE_NOTHROW(DM_EAM(dm_context("0")));
    zero = cap.str();
  }
  INFO("stderr with 0:\n" << zero);
  CHECK(count_of(zero, "is ignored") == 0);
  CHECK(tadah::core::ignored_key_reported(key) == before);
  {
    CerrCapture cap;
    REQUIRE_NOTHROW(DM_EAM(dm_context("7")));
    REQUIRE_NOTHROW(DM_EAM(dm_context("7")));
    value = cap.str();
  }
  INFO("stderr with 7:\n" << value);
  CHECK(count_of(value, line) == (before ? 0u : 1u));
  CHECK(count_of(value, "is ignored") == (before ? 0u : 1u));
  CHECK(tadah::core::ignored_key_reported(key));
}

/* Pins: a D2_EAM built from a context with INIT2B false is empty (size 0)
 * and still takes its one TYPE2B parameter (nparams == 1), as DM_EAM with
 * INITMB false does.
 * Before the fix: D2_EAM(Context &) returned before init(), so nparams was
 * never set (an indeterminate value; the CHECK failed). */
TEST_CASE("D2_EAM built with INIT2B false takes its one parameter",
          "[D2][eam]") {
  tadah::core::PeriodicTable::initialize();
  Context off = d2_context("0");
  off.remove("INIT2B");
  off.add("INIT2B", "false");
  auto d2 = std::make_unique<D2_EAM>(off);
  CHECK(d2->size() == 0);
  CHECK(d2->nparams == 1);

  Context off_mb = dm_context("0");
  off_mb.remove("INITMB");
  off_mb.add("INITMB", "false");
  auto dm = std::make_unique<DM_EAM>(off_mb);
  CHECK(dm->size() == 0);
  CHECK(dm->nparams == 1);
}

/* Pins: D2_EAM refuses an RCUT2B other than the cutoff of its setfl file,
 * smaller (3) or larger (7), with an error that names the descriptor, the
 * key and both values, and the file:
 *   D2_EAM: RCUT2B 3 differs from the cutoff 5.58190742024 of the setfl file
 *   'tests_data/eam.alloy' (SETFL); the RCUT2B of D2_EAM is the file's
 *   cutoff, 5.58190742024.
 * The context keeps RCUT2B as it was (3). With the file's cutoff and
 * RCTYPE2B Cut_Dummy the descriptor builds, evaluates with that cutoff
 * (get_rcut()) and leaves RCUT2B as given. Inside a D2_mJoin the D2_EAM
 * child's own RCUT2B is checked (RCUT2B 5.58190742024 3 refused naming 3).
 * Before the fix: none was refused. D2_EAM replaced RCUT2B in the context
 * with the file's cutoff and logged a WARNING ("Enforcing SETFL file
 * cutoff"), after its cutoff function had been built from the configured
 * value: get_rcut() was 3 while RCUT2B read 5.58190742024, the value a
 * potential file records, so the saved potential was not the fitted model.
 * CHECK_THROWS_WITH failed and RCUT2B read 5.58190742024, not 3.
 * Before the fix that states the rule: the refusal ended "(SETFL); set RCUT2B
 * to the file's cutoff, 5.58190742024.", an instruction: the checks fail. */
TEST_CASE("D2_EAM refuses an RCUT2B other than its setfl file's cutoff",
          "[D2][eam][cutoff]") {
  tadah::core::PeriodicTable::initialize();
  for (const double rc : {3.0, 7.0}) {
    INFO("RCUT2B " << rc);
    Context c = d2_context("0");
    c.remove("RCUT2B");
    c.add("RCUT2B", rc);
    c.remove("RCTYPE2B");
    c.add("RCTYPE2B", "Cut_Dummy");
    const std::string v = rc == 3.0 ? "3" : "7";
    CHECK_THROWS_WITH(
        D2_EAM(c),
        "D2_EAM: RCUT2B " + v +
            " differs from the cutoff 5.58190742024 of the setfl file "
            "'tests_data/eam.alloy' (SETFL); the RCUT2B of D2_EAM is the "
            "file's cutoff, 5.58190742024.");
    CHECK(c.get<double>("RCUT2B") == rc);
  }

  Context ok = d2_context("0");
  ok.remove("RCTYPE2B");
  ok.add("RCTYPE2B", "Cut_Dummy");
  std::unique_ptr<D2_EAM> d2;
  REQUIRE_NOTHROW(d2 = std::make_unique<D2_EAM>(ok));
  CHECK(d2->get_rcut() == kSetflRcut);
  CHECK(ok.get<double>("RCUT2B") == kSetflRcut);

  // D2_mJoin of two D2_EAM terms: the second one's RCUT2B is 3
  Context join;
  join.add("ATOM", "Ta");
  join.remove("INIT2B");
  join.add("INIT2B", "true");
  for (const char *t : {"D2_mJoin", "D2_EAM", "0", "Ta", "Ta", "D2_EAM", "0",
                        "Ta", "Ta"})
    join.add("TYPE2B", t);
  join.add("RCTYPE2B", "Cut_Dummy");
  join.add("RCTYPE2B", "Cut_Dummy");
  join.add("RCUT2B", kSetflRcut);
  join.add("RCUT2B", 3.0);
  join.add("SETFL", kSetfl);
  CHECK_THROWS_WITH(D2_mJoin(join),
                    Catch::StartsWith("D2_EAM: RCUT2B 3 differs from the "
                                      "cutoff 5.58190742024 of the setfl "
                                      "file"));
}

/* Pins: the same for DM_EAM and RCUTMB:
 *   DM_EAM: RCUTMB 3 differs from the cutoff 5.58190742024 of the setfl file
 *   'tests_data/eam.alloy' (SETFL); the RCUTMB of DM_EAM is the file's
 *   cutoff, 5.58190742024.
 * for 3 and 7; RCUTMB is left as it was; the file's cutoff with RCTYPEMB
 * Cut_Dummy builds and evaluates with that cutoff.
 * Before the fix: none was refused; RCUTMB was replaced in the context with
 * the file's cutoff while get_rcut() kept the configured one, so
 * CHECK_THROWS_WITH failed and RCUTMB read 5.58190742024.
 * Before the fix that states the rule: the refusal ended "(SETFL); set RCUTMB
 * to the file's cutoff, 5.58190742024.", an instruction: the checks fail. */
TEST_CASE("DM_EAM refuses an RCUTMB other than its setfl file's cutoff",
          "[DM][eam][cutoff]") {
  tadah::core::PeriodicTable::initialize();
  for (const double rc : {3.0, 7.0}) {
    INFO("RCUTMB " << rc);
    Context c = dm_context("0");
    c.remove("RCUTMB");
    c.add("RCUTMB", rc);
    c.remove("RCTYPEMB");
    c.add("RCTYPEMB", "Cut_Dummy");
    const std::string v = rc == 3.0 ? "3" : "7";
    CHECK_THROWS_WITH(
        DM_EAM(c),
        "DM_EAM: RCUTMB " + v +
            " differs from the cutoff 5.58190742024 of the setfl file "
            "'tests_data/eam.alloy' (SETFL); the RCUTMB of DM_EAM is the "
            "file's cutoff, 5.58190742024.");
    CHECK(c.get<double>("RCUTMB") == rc);
  }

  Context ok = dm_context("0");
  ok.remove("RCTYPEMB");
  ok.add("RCTYPEMB", "Cut_Dummy");
  std::unique_ptr<DM_EAM> dm;
  REQUIRE_NOTHROW(dm = std::make_unique<DM_EAM>(ok));
  CHECK(dm->get_rcut() == kSetflRcut);
  CHECK(ok.get<double>("RCUTMB") == kSetflRcut);
}

/* Pins: D_Base::cutoff_fixed_by(i) names what fixes value i of a
 * descriptor's cutoff key, so that tadah hpo can refuse a search over it
 * at startup: a D2_EAM and a DM_EAM built from tests_data/eam.alloy answer
 *   D2_EAM takes the cutoff 5.58190742024 of its setfl file
 *   'tests_data/eam.alloy' (SETFL)
 * (DM_EAM the same with its name) for value 1; a D2_EAM built with INIT2B
 * false, which reads no file, and a D2_BP answer "" (a free cutoff); a
 * D2_mJoin of D2_BP and D2_EAM answers "" for RCUT2B value 1 (D2_BP's),
 * D2_EAM's text for value 2 and "" for value 3, which no joined descriptor
 * takes; a DM_mJoin of DM_Blip and DM_EAM answers likewise for RCUTMB.
 * Before the fix: D_Base has no cutoff_fixed_by (does not compile); tadah
 * hpo accepted OPTIM RCUT2B with D2_EAM, and every candidate other than
 * the seed failed in the D2_EAM constructor (FAILSCORE). */
TEST_CASE("D2_EAM and DM_EAM name the setfl file that fixes their cutoff",
          "[D2][DM][eam][cutoff]") {
  tadah::core::PeriodicTable::initialize();
  const std::string d2_text =
      "D2_EAM takes the cutoff 5.58190742024 of its setfl file "
      "'tests_data/eam.alloy' (SETFL)";
  const std::string dm_text =
      "DM_EAM takes the cutoff 5.58190742024 of its setfl file "
      "'tests_data/eam.alloy' (SETFL)";

  Context c2 = d2_context("0");
  c2.add("RCTYPE2B", "Cut_Cos");
  CHECK(D2_EAM(c2).cutoff_fixed_by(0) == d2_text);
  Context cm = dm_context("0");
  cm.add("RCTYPEMB", "Cut_Cos");
  CHECK(DM_EAM(cm).cutoff_fixed_by(0) == dm_text);

  Context off = d2_context("0");
  off.remove("INIT2B");
  off.add("INIT2B", "false");
  CHECK(D2_EAM(off).cutoff_fixed_by(0).empty());

  Context bp;
  bp.add("ATOM", "Ta");
  bp.remove("INIT2B");
  bp.add("INIT2B", "true");
  bp.add("VERBOSE", 0);
  bp.add("TYPE2B", "D2_BP 2 2 Ta Ta");
  bp.add("CGRID2B", "2.6 3.3");
  bp.add("SGRID2B", "1.0 1.0");
  bp.add("RCUT2B", 4.5);
  bp.add("RCTYPE2B", "Cut_Cos");
  CHECK(tadah::models::D2_BP(bp).cutoff_fixed_by(0).empty());

  Context j2;
  j2.add("ATOM", "Ta");
  j2.remove("INIT2B");
  j2.add("INIT2B", "true");
  j2.add("VERBOSE", 0);
  j2.add("TYPE2B", "D2_mJoin D2_BP 2 2 Ta Ta D2_EAM 0 Ta Ta");
  j2.add("CGRID2B", "2.6 3.3");
  j2.add("SGRID2B", "1.0 1.0");
  j2.add("RCTYPE2B", "Cut_Cos Cut_Cos");
  j2.add("RCUT2B", 4.5);
  j2.add("RCUT2B", kSetflRcut);
  j2.add("SETFL", kSetfl);
  D2_mJoin d2j(j2);
  CHECK(d2j.cutoff_fixed_by(0).empty());
  CHECK(d2j.cutoff_fixed_by(1) == d2_text);
  CHECK(d2j.cutoff_fixed_by(2).empty());

  Context jm;
  jm.add("ATOM", "Ta");
  jm.remove("INITMB");
  jm.add("INITMB", "true");
  jm.add("VERBOSE", 0);
  jm.add("TYPEMB", "DM_mJoin DM_Blip 0 1 1 Ta Ta DM_EAM 0 Ta Ta");
  jm.add("CGRIDMB", "1.0");
  jm.add("SGRIDMB", "0.8");
  jm.add("RCTYPEMB", "Cut_Cos Cut_Cos");
  jm.add("RCUTMB", 5.0);
  jm.add("RCUTMB", kSetflRcut);
  jm.add("SETFL", kSetfl);
  DM_mJoin dmj(jm);
  CHECK(dmj.cutoff_fixed_by(0).empty());
  CHECK(dmj.cutoff_fixed_by(1) == dm_text);
  CHECK(dmj.cutoff_fixed_by(2).empty());
}

/* Pins: the parameter metadata of D2_EAM and DM_EAM (meta(), read by the
 * pre-flight audit and `tadah analysis suggest_bounds`) lists the cutoff key
 * (RCUT2B, RCUTMB) as Optional, with no bound suggester: the setfl file
 * fixes it, so it is not a search target. Its health check (a cutoff that
 * finds no neighbours) stays. D2_LJ, whose cutoff is free, keeps RCUT2B
 * Critical with a suggester.
 * Before the fix: D2_EAM and DM_EAM listed their cutoff as Critical with a
 * suggester, so the audit told a user to add `OPTIM RCUT2B` from
 * suggest_bounds, a search whose every candidate but the seed D2_EAM
 * refuses. */
TEST_CASE("D2_EAM and DM_EAM do not offer their cutoff as a search target",
          "[D2][DM][eam][cutoff][meta]") {
  using tadah::models::Criticality;
  struct Case { std::optional<tadah::models::DescriptorMeta> meta;
                const char *key; };
  for (const auto &c : {Case{D2_EAM().meta(), "RCUT2B"},
                        Case{DM_EAM().meta(), "RCUTMB"}}) {
    INFO(c.key);
    REQUIRE(c.meta.has_value());
    REQUIRE(c.meta->params.size() == 1);
    const auto &pm = c.meta->params[0];
    CHECK(pm.key == c.key);
    CHECK(pm.crit == Criticality::Optional);
    CHECK_FALSE(static_cast<bool>(pm.suggest));
    CHECK(static_cast<bool>(pm.check));
  }
  const auto lj = tadah::models::D2_LJ().meta();
  REQUIRE(lj.has_value());
  REQUIRE(lj->params.size() == 1);
  CHECK(lj->params[0].crit == Criticality::Critical);
  CHECK(static_cast<bool>(lj->params[0].suggest));
}

namespace {

/* tests_data/eam.alloy written to @p path with its pair function z2r
 * multiplied by @p z2r_scale (the other functions and the grid as read). */
void write_scaled_setfl(const std::string &path, double z2r_scale) {
  EAMSetflIO src;
  src.read_setfl(kSetfl);
  src.build_splines();
  src.write_setfl(path, src.input_grid(),
                  [&](double rho) { return src.F_tab(rho); },
                  [&](double r) { return src.rho_tab(r); },
                  [&](double r) { return z2r_scale * src.z2_tab(r); });
}

/* D2_EAM's descriptor of a Ta-Ta pair at r for the context c. */
double d2_eam_aed(const Context &c, double r) {
  D2_EAM d(c);
  Cut_Dummy fc(kSetflRcut);
  d.set_fcut(&fc, false);
  tadah::core::aed_type aed(d.size());
  aed.set_zero();
  d.calc_aed(kTa, kTa, r, r * r, aed);
  return aed(0);
}

}  // namespace

/* Pins: a D2_EAM built after its setfl file was rewritten evaluates the new
 * file: with SETFL naming one path, rewritten from z2r x 1 to z2r x 1.5,
 * the descriptor at r = 2.86 A changes, and equals (bit for bit) that of a
 * D2_EAM built from a second path holding the second file; built again
 * after the first contents are written back, it equals the first value.
 * (The parsed setfl tables are shared between descriptors that read the
 * same file; they must follow its contents.)
 * Before the fix: passes as well (a pin: every D2_EAM read its file in its
 * constructor). */
TEST_CASE("D2_EAM evaluates the setfl file as it is when it is built",
          "[D2][eam][setfl_cache]") {
  tadah::core::PeriodicTable::initialize();
  const std::string one = "tests_data_eam_follow_one.alloy";
  const std::string two = "tests_data_eam_follow_two.alloy";
  const double r = 2.86;
  Context c1 = d2_context("0");
  c1.remove("SETFL");
  c1.add("SETFL", one);
  Context c2 = d2_context("0");
  c2.remove("SETFL");
  c2.add("SETFL", two);

  write_scaled_setfl(one, 1.0);
  const double first = d2_eam_aed(c1, r);
  write_scaled_setfl(one, 1.5);
  const double second = d2_eam_aed(c1, r);
  write_scaled_setfl(two, 1.5);
  const double other_path = d2_eam_aed(c2, r);
  write_scaled_setfl(one, 1.0);
  const double first_again = d2_eam_aed(c1, r);
  std::remove(one.c_str());
  std::remove(two.c_str());

  CHECK(second == Approx(1.5 * first).epsilon(1e-9));
  CHECK(second != first);
  CHECK(second == other_path);
  CHECK(first_again == first);
}

/* Pins: a model built from a potential with D2_EAM and DM_EAM (one SETFL)
 * and WATOM 2.5 -- a pair weight that no descriptor of it reads -- gives
 * one notice (core::report_ignored_key_once, captured here as pair_style
 * tadah captures it): "Key WATOM (2.5) is ignored: no descriptor of the
 * potential reads it (D2_EAM and DM_EAM take their pair and density
 * functions from the SETFL file, unweighted); ...", and its descriptors
 * evaluate as with WATOM 1 (the pair term at 2.86 A, bit for bit). WATOM 1
 * and a D2_LJ potential with WATOM 2.5 give no notice of WATOM.
 * Before the fix: no notice: the WATOM line was accepted without a word. */
TEST_CASE("A D2_EAM and DM_EAM potential with a WATOM other than 1 loads "
          "with a notice that no descriptor reads it",
          "[D2][DM][eam][watom]") {
  tadah::core::PeriodicTable::initialize();
  auto eam = [](const std::string &watom) {
    Context c = d2_context("0");
    c.remove("INITMB");
    c.add("INITMB", "true");
    c.add("TYPEMB", "DM_EAM");
    c.add("TYPEMB", "0");
    c.add("TYPEMB", "Ta");
    c.add("TYPEMB", "Ta");
    c.add("RCUTMB", kSetflRcut);
    c.add("RCTYPE2B", "Cut_Dummy");
    c.add("RCTYPEMB", "Cut_Dummy");
    c.add("WATOM", watom);
    return c;
  };
  auto watom_notices = [](const tadah::core::IgnoredKeyNoticeCapture &cap) {
    std::size_t n = 0;
    for (const auto &s : cap.notices())
      if (s.find("WATOM") != std::string::npos) ++n;
    return n;
  };
  double aed_w = 0, aed_1 = 0;
  {
    tadah::core::IgnoredKeyNoticeCapture cap;
    Context c = eam("2.5");
    tadah::models::DC_Selector s(c);
    INFO("notices: " << cap.notices().size());
    CHECK(watom_notices(cap) == 1u);
    bool named = false;
    for (const auto &n : cap.notices())
      named = named || (n.find("Key WATOM (2.5) is ignored: no descriptor "
                               "of the potential reads it") == 0 &&
                        n.find("D2_EAM and DM_EAM") != std::string::npos);
    CHECK(named);
    aed_w = d2_eam_aed(c, 2.86);
  }
  {
    tadah::core::IgnoredKeyNoticeCapture cap;
    Context c = eam("1");
    tadah::models::DC_Selector s(c);
    CHECK(watom_notices(cap) == 0u);
    aed_1 = d2_eam_aed(c, 2.86);
  }
  CHECK(aed_w == aed_1);
  {
    tadah::core::IgnoredKeyNoticeCapture cap;
    Context c;
    c.add("ATOM", "Ta");
    c.remove("INIT2B");
    c.add("INIT2B", "true");
    c.add("TYPE2B", "D2_LJ Ta Ta");
    c.add("RCTYPE2B", "Cut_Cos");
    c.add("RCUT2B", 5.0);
    c.add("WATOM", "2.5");
    tadah::models::DC_Selector s(c);
    CHECK(watom_notices(cap) == 0u);
  }
}
