// pair_style tadah (tadah::LammpsTadah, the object pair_coeff builds from
// the potential file) and the keys Tadah! 1.6.0 retired: EWEIGHT, FWEIGHT,
// SWEIGHT, WDBFILE_AUTO and EWEIGHT_TEMP weighted the training data of a fit
// before 1.6.0, and UNCERTAINTY asked tadah train for weights.tadah: a
// potential that tadah train or tadah hpo of 1.0.0 to 1.5.0 wrote has
// EWEIGHT, FWEIGHT and SWEIGHT lines, from 1.3.0 a WDBFILE_AUTO line, and
// a potential of tadah train an UNCERTAINTY line. Such a potential loads: the keys change nothing a
// prediction gives, so they are not stored, and each load gives one notice
// that lists them with their lines (pair_style tadah writes it as a LAMMPS
// warning on rank 0 for each pair_coeff; Tadah.MLIP's test_pair_coeff_reload
// runs that in LAMMPS).
//
// tests_data/pots/ta_blip_1.5.0.tadah is a potential of Tadah! 1.5.0, byte
// for byte as `tadah train -c config.train` of tadah 1.5.0 wrote it (D2_Blip
// 3 3 Ta Ta, Cut_Cos at 5 A, M_BLR BF_Linear, FORCE true, OALGO 3, EWEIGHT
// 1.0 and FWEIGHT 100, on three 54-atom bcc Ta cells): its EWEIGHT, FWEIGHT,
// SWEIGHT, UNCERTAINTY and WDBFILE_AUTO lines are lines 8, 10, 23, 25 and
// 27.
// tests_data/pots/tadah.n2 is a verbatim copy of the published Tadah!N2
// potential (Tadah.MLIP docs/source/mlips/tadah.n2): the EWEIGHT, FWEIGHT
// and SWEIGHT of its fit are in its training record, the comment block
// above its key lines, which no loader reads.

#include "catch2/catch.hpp"

#include <tadah/core/key_notice.h>
#include <tadah/core/periodic_table.h>
#include <tadah/tadah.lammps/lammps_tadah.h>

#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <unistd.h>
#include <utility>
#include <vector>

namespace {

/* pair_coeff * * <file> <species>: LammpsTadah from the file, then the type
 * map. */
std::unique_ptr<tadah::LammpsTadah> load(const std::string &path,
                                         const std::string &species) {
  std::vector<std::string> args = {"*", "*", path, species};
  std::vector<char *> argv;
  for (auto &a : args) argv.push_back(&a[0]);
  std::unique_ptr<tadah::LammpsTadah> lt(
      new tadah::LammpsTadah(static_cast<int>(argv.size()), argv.data()));
  lt->initialize(static_cast<int>(argv.size()), argv.data());
  return lt;
}

/* The text of @p path without its lines whose first word is one of
 * @p keys, written to a file unique to this process, removed with the
 * object. */
struct WithoutKeys {
  std::string path;
  WithoutKeys(const std::string &from, const std::vector<std::string> &keys,
              const std::string &tag)
      : path("retired_keys_" + std::to_string(::getpid()) + "_" + tag +
             ".tadah") {
    std::ifstream in(from);
    std::ofstream out(path);
    for (std::string line; std::getline(in, line);) {
      std::istringstream ls(line);
      std::string first;
      ls >> first;
      bool drop = false;
      for (const auto &k : keys) drop = drop || first == k;
      if (!drop) out << line << "\n";
    }
  }
  ~WithoutKeys() { std::remove(path.c_str()); }
};

std::size_t retired_notices(const std::vector<std::string> &notices) {
  std::size_t n = 0;
  for (const auto &s : notices)
    if (s.find("Retired key") != std::string::npos) ++n;
  return n;
}

struct Case {
  std::string file, species;
  std::vector<std::pair<std::string, int>> retired;  // key, line
};

}  // namespace

/* Pins: for the potential of Tadah! 1.5.0, a load as pair_coeff loads it
 * succeeds and gives one notice, "Retired keys EWEIGHT (line 8 of <file>),
 * FWEIGHT (line 10 of <file>), SWEIGHT (line 23 of <file>), UNCERTAINTY
 * (line 25 of <file>) and WDBFILE_AUTO (line 27 of <file>) are ignored:
 * ..."; a second load gives it again (one notice per load);
 * none of the keys is stored; and the loaded context writes the key lines
 * of the same file without those lines (write_potential, which writes no
 * retired line).
 * Before the fix: the keys were stored as keys of the model (EWEIGHT 1.0,
 * FWEIGHT 100 ...), with no notice. Before UNCERTAINTY was retired: the
 * notice listed four keys, and the context stored UNCERTAINTY false. (The
 * published Tadah!N2 was a second case, with EWEIGHT, FWEIGHT and SWEIGHT
 * on lines 8, 9 and 22, until those moved into its training record: the
 * case below.) */
TEST_CASE("pair_style tadah loads a potential with retired keys, with one "
          "notice per load that lists them",
          "[lammps_tadah][retired]") {
  tadah::core::PeriodicTable::initialize();
  const std::vector<Case> cases = {
      {"tests_data/pots/ta_blip_1.5.0.tadah", "Ta",
       {{"EWEIGHT", 8}, {"FWEIGHT", 10}, {"SWEIGHT", 23},
        {"UNCERTAINTY", 25}, {"WDBFILE_AUTO", 27}}}};
  for (const Case &c : cases) {
    INFO(c.file);
    std::vector<std::string> keys;
    std::string listed;
    for (std::size_t k = 0; k < c.retired.size(); ++k) {
      keys.push_back(c.retired[k].first);
      listed += (k == 0 ? "" : k + 1 == c.retired.size() ? " and " : ", ") +
                c.retired[k].first + " (line " +
                std::to_string(c.retired[k].second) + " of " + c.file + ")";
    }
    for (int load_no = 1; load_no <= 2; ++load_no) {
      INFO("load " << load_no);
      tadah::core::IgnoredKeyNoticeCapture notices;
      std::unique_ptr<tadah::LammpsTadah> lt;
      std::string msg;
      try {
        lt = load(c.file, c.species);
      } catch (const std::exception &e) {
        msg = e.what();
      }
      INFO("load: " << msg);
      REQUIRE(lt != nullptr);
      CHECK(retired_notices(notices.notices()) == 1);
      bool listed_all = false;
      for (const auto &n : notices.notices())
        listed_all = listed_all ||
                     n.find("Retired keys " + listed + " are ignored") !=
                         std::string::npos;
      CHECK(listed_all);
      for (const auto &k : keys) {
        INFO("key " << k);
        CHECK_FALSE(lt->c.has_stored(k));
      }
      const WithoutKeys bare(c.file, keys, "bare");
      tadah::core::IgnoredKeyNoticeCapture quiet;
      std::unique_ptr<tadah::LammpsTadah> lt_bare = load(bare.path, c.species);
      std::ostringstream with_lines, without_lines;
      lt->c.write_potential(with_lines, 18);
      lt_bare->c.write_potential(without_lines, 18);
      CHECK(with_lines.str() == without_lines.str());
    }
  }
}

/* Pins: the published Tadah!N2 (tests_data/pots/tadah.n2) loads as
 * pair_coeff loads it with no notice of retired keys, twice: the EWEIGHT,
 * FWEIGHT and SWEIGHT of its fit are in its training record, and none of
 * them is stored.
 * Before the fix: each load gave "Retired keys EWEIGHT (line 8 of <file>),
 * FWEIGHT (line 9 of <file>) and SWEIGHT (line 22 of <file>) are ignored:
 * ...". */
TEST_CASE("pair_style tadah loads the published Tadah!N2 without a notice "
          "of retired keys",
          "[lammps_tadah][retired][published]") {
  tadah::core::PeriodicTable::initialize();
  for (int load_no = 1; load_no <= 2; ++load_no) {
    INFO("load " << load_no);
    tadah::core::IgnoredKeyNoticeCapture notices;
    std::unique_ptr<tadah::LammpsTadah> lt;
    std::string msg;
    try {
      lt = load("tests_data/pots/tadah.n2", "N");
    } catch (const std::exception &e) {
      msg = e.what();
    }
    INFO("load: " << msg);
    REQUIRE(lt != nullptr);
    CHECK(retired_notices(notices.notices()) == 0);
    for (const char *k : {"EWEIGHT", "FWEIGHT", "SWEIGHT"}) {
      INFO("key " << k);
      CHECK_FALSE(lt->c.has_stored(k));
    }
  }
}
