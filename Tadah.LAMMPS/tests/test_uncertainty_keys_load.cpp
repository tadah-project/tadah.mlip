// pair_style tadah loads (tadah::LammpsTadah, the object pair_coeff builds
// from the potential file) a potential with the keys of uncertainty output,
// which is removed: WCOV and WNOISE, the covariance of the
// WEIGHTS and the noise variance of each quantity that a development build
// of Tadah! 1.6.0 wrote for an OALGO 3 fit, are retired keys (one notice
// per load, not stored), and SIGMA, which Tadah! 1.0.0 to 1.5.0 wrote, is a
// deprecated key (one notice, not stored).
//
// The potential is the D2_LJ pair potential of
// tests_data/pots/ta_lj_cutcos.tadah, with the error scale lines a fit
// writes (the defaults).

#include "catch2/catch.hpp"

#include <tadah/core/key_notice.h>
#include <tadah/core/periodic_table.h>
#include <tadah/tadah.lammps/lammps_tadah.h>

#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

namespace {

const char *kPairPotential =
    "ATOM     Ta\n"
    "BIAS     false\n"
    "DIMER     false     0     false\n"
    "ENERGY     true\n"
    "ENORM     ATOM\n"
    "ESIGMA     1\n"
    "FSIGMA     0.1     0\n"
    "INIT2B     true\n"
    "INITMB     false\n"
    "LAMBDA     0\n"
    "MODEL     M_KRR     Kern_Linear     1\n"
    "NORM     false\n"
    "OALGO     3\n"
    "RCTYPE2B     Cut_Cos\n"
    "RCUT2B     6.5\n"
    "SSIGMA     0.5     0\n"
    "TYPE2B     D2_LJ     Ta     Ta\n"
    "WATOM     73\n"
    "WEIGHTS     14.204213349318765225     2604.396965191186154698\n";

/* A potential file unique to this process, removed with the object. */
struct PotFile {
  std::string path;
  explicit PotFile(const std::string &extra) {
    path = "uncertainty_keys_" + std::to_string(::getpid()) + ".tadah";
    std::ofstream(path) << kPairPotential << extra;
  }
  ~PotFile() { std::remove(path.c_str()); }
};

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

std::string load_error(const std::string &path) {
  try {
    load(path);
  } catch (const std::exception &e) {
    return e.what();
  }
  return "";
}

}  // namespace

/* Pins: a potential with WCOV (N = 2, then the 2 x 2 matrix) and WNOISE (E
 * and F) beside its error scales loads as pair_coeff loads it, with one
 * notice per load, "Retired keys WCOV (line 20 of <file>) and WNOISE (line
 * 21 of <file>) are ignored: ...", and its Context has neither key and keeps
 * the error scales (ESIGMA 1, FSIGMA 0.1 0).
 * Before the fix: the Context kept both keys (5 and 4 values), with no
 * notice. */
TEST_CASE("pair_style tadah loads a potential with WCOV and WNOISE, with one "
          "notice",
          "[lammps_tadah][retired][uncertainty]") {
  tadah::core::PeriodicTable::initialize();
  const PotFile pot("WCOV     2     0.5 0.1 0.1 0.25\n"
                    "WNOISE     E 0.04     F 0.0025\n");
  for (int load_no = 1; load_no <= 2; ++load_no) {
    INFO("load " << load_no);
    tadah::core::IgnoredKeyNoticeCapture notices;
    std::unique_ptr<tadah::LammpsTadah> lt;
    std::string msg;
    try {
      lt = load(pot.path);
    } catch (const std::exception &e) {
      msg = e.what();
    }
    INFO("load: " << msg);
    REQUIRE(lt != nullptr);
    REQUIRE(notices.notices().size() == 1);
    CHECK(notices.notices()[0].rfind(
              "Retired keys WCOV (line 20 of " + pot.path +
                  ") and WNOISE (line 21 of " + pot.path + ") are ignored",
              0) == 0);
    CHECK_FALSE(lt->c.has_stored("WCOV"));
    CHECK_FALSE(lt->c.has_stored("WNOISE"));
    CHECK(lt->c.stored_value("ESIGMA") == std::optional<std::string>("1"));
    CHECK(lt->c.size("FSIGMA") == 2);
  }
}

/* Pins: a potential with a SIGMA line (an OALGO 3 potential of Tadah!
 * 1.0.0 to 1.5.0) loads as pair_coeff loads it, writes the deprecation
 * notice of SIGMA ("WARNING: Deprecated key SIGMA is ignored: ...") once,
 * on standard error (pair_style tadah routes it to LAMMPS' warning, see
 * HPO's test_pair_tadah_notices.cpp), and its Context has no SIGMA.
 * Before the fix: SIGMA was stored, with no notice. */
TEST_CASE("pair_style tadah loads a potential with SIGMA and ignores it",
          "[lammps_tadah][uncertainty]") {
  tadah::core::PeriodicTable::initialize();
  const PotFile pot("SIGMA     2     1 0 0 1\n");
  const std::size_t expected =
      tadah::core::ignored_key_reported("SIGMA") ? 0 : 1;
  std::ostringstream err;
  std::streambuf *old = std::cerr.rdbuf(err.rdbuf());
  std::unique_ptr<tadah::LammpsTadah> lt;
  std::string msg;
  try {
    lt = load(pot.path);
  } catch (const std::exception &e) {
    msg = e.what();
  }
  std::cerr.rdbuf(old);
  INFO("load: " << msg << "\nstderr: " << err.str());
  REQUIRE(lt != nullptr);
  CHECK_FALSE(lt->c.has_stored("SIGMA"));
  std::size_t n = 0;
  const std::string text = err.str();
  for (std::size_t p = text.find("Deprecated key SIGMA is ignored");
       p != std::string::npos;
       p = text.find("Deprecated key SIGMA is ignored", p + 1))
    ++n;
  CHECK(n == expected);
}
