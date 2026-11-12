// pair_style tadah loads (tadah::LammpsTadah, the object pair_coeff builds
// from the potential file) a D2_EAM + DM_EAM potential that tadah wrote with
// NUMERIC below 17. D2_EAM and DM_EAM refuse an RCUT2B / RCUTMB other than
// the cutoff of their setfl file, compared exactly, so the potential writer
// (core::Context::write_potential, what tadah train, refit and hpo write
// through) records the cutoff and grid keys exactly whatever NUMERIC is.
//
// The setfl file is synthetic (analytic tables for Ta) with the cutoff
// 4.51234567891, which 6 or 3 significant digits do not represent.

#include "catch2/catch.hpp"

#include <tadah/core/context.h>
#include <tadah/core/periodic_table.h>
#include <tadah/tadah.lammps/lammps_tadah.h>

#include <cmath>
#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

namespace {

const char *kCutoff = "4.51234567891";

/* A file unique to this process, removed with the object. */
struct TempFile {
  std::string path;
  TempFile(const std::string &stem, const std::string &body)
      : path(stem + "_" + std::to_string(::getpid())) {
    std::ofstream(path) << body;
  }
  ~TempFile() { std::remove(path.c_str()); }
};

/* A single-element (Ta) setfl file, cutoff kCutoff: F(rho) = -sqrt(rho),
 * rho(r) = (rc - r)^2 and r phi(r) = 0.5 r (rc - r)^3 below the cutoff. */
std::string ta_setfl() {
  const int nrho = 501, nr = 501;
  const double drho = 0.1, dr = 0.01, rc = std::stod(kCutoff);
  std::ostringstream os;
  os.precision(17);
  os << "Synthetic Ta setfl\nanalytic tables\n\n1 Ta\n"
     << nrho << " " << drho << " " << nr << " " << dr << " " << kCutoff
     << "\n73 180.95 3.304 bcc\n";
  for (int i = 0; i < nrho; ++i) os << -std::sqrt(i * drho) << "\n";
  for (int i = 0; i < nr; ++i) {
    const double r = i * dr;
    os << (r < rc ? (rc - r) * (rc - r) : 0.0) << "\n";
  }
  for (int i = 0; i < nr; ++i) {
    const double r = i * dr;
    os << (r < rc ? r * 0.5 * (rc - r) * (rc - r) * (rc - r) : 0.0) << "\n";
  }
  return os.str();
}

/* The model tadah train writes for a D2_EAM + DM_EAM fit of @p setfl. */
tadah::core::Context eam_potential(const std::string &setfl) {
  tadah::core::Context c;
  c.add("ATOM", "Ta");
  c.add("WATOM", 73);
  c.add("BIAS", false);
  c.add("NORM", false);
  c.add("INIT2B", true);
  c.add("TYPE2B", "D2_EAM 0 Ta Ta");
  c.add("RCTYPE2B", "Cut_Dummy");
  c.add("RCUT2B", kCutoff);
  c.add("INITMB", true);
  c.add("TYPEMB", "DM_EAM 0 Ta Ta");
  c.add("RCTYPEMB", "Cut_Dummy");
  c.add("RCUTMB", kCutoff);
  c.add("SETFL", setfl);
  c.add("MODEL", "M_BLR BF_Linear");
  c.add("WEIGHTS", std::vector<double>{1.0 / 3, 2.0 / 3});
  return c;
}

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

/* The tokens after `key` on its line of @p text. */
std::vector<std::string> line_values(const std::string &text,
                                     const std::string &key) {
  std::istringstream in(text);
  for (std::string line; std::getline(in, line);) {
    std::istringstream ls(line);
    std::string k;
    if (!(ls >> k) || k != key) continue;
    std::vector<std::string> v;
    for (std::string t; ls >> t;) v.push_back(t);
    return v;
  }
  return {};
}

}  // namespace

/* Pins: a D2_EAM + DM_EAM potential of the setfl cutoff 4.51234567891,
 * written by core::Context::write_potential with 6 and with 3 significant
 * digits (NUMERIC 6, NUMERIC 3), records 'RCUT2B 4.51234567891' and
 * 'RCUTMB 4.51234567891', its WEIGHTS rounded (0.333333 0.666667 at 6
 * digits, 0.333 0.667 at 3), and loads as pair_coeff loads it, with both
 * cutoffs the file's.
 * Before the fix: the cutoffs were rounded with the weights ('RCUT2B
 * 4.51235', 'RCUT2B 4.51'), and the load threw 'D2_EAM: RCUT2B 4.51235
 * differs from the cutoff 4.51234567891 of the setfl file ...' (pair_coeff
 * failed). */
TEST_CASE("pair_style tadah loads a D2_EAM and DM_EAM potential written "
          "with NUMERIC below 17",
          "[lammps_tadah][numeric][exact_in_potential]") {
  tadah::core::PeriodicTable::initialize();
  const TempFile setfl("numeric_exact_keys.eam.alloy", ta_setfl());
  const tadah::core::Context pot = eam_potential(setfl.path);
  for (const int digits : {6, 3}) {
    INFO("digits " << digits);
    std::ostringstream os;
    pot.write_potential(os, digits);
    const std::string text = os.str();
    INFO(text);
    CHECK(line_values(text, "RCUT2B") == std::vector<std::string>{kCutoff});
    CHECK(line_values(text, "RCUTMB") == std::vector<std::string>{kCutoff});
    CHECK(line_values(text, "WEIGHTS") ==
          (digits == 6 ? std::vector<std::string>{"0.333333", "0.666667"}
                       : std::vector<std::string>{"0.333", "0.667"}));
    const TempFile file("numeric_exact_keys.tadah", text);
    std::unique_ptr<tadah::LammpsTadah> lt;
    std::string msg;
    try {
      lt = load(file.path);
    } catch (const std::exception &e) {
      msg = e.what();
    }
    INFO("load: " << msg);
    REQUIRE(lt != nullptr);
    CHECK(lt->c.get<double>("RCUT2B") == std::stod(kCutoff));
    CHECK(lt->c.get<double>("RCUTMB") == std::stod(kCutoff));
  }
}
