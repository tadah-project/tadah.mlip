// pair_coeff loads a potential whose weights fit pair_style tadah's
// descriptor, and refuses one whose weights do not.
//
// pair_style tadah builds the descriptor with DC_Selector: with BIAS true
// one one-hot intercept column per ATOM species, then the two-body and the
// many-body descriptors. Its predictors' dot products run over the weights,
// so a potential whose WEIGHTS were written for another layout gave
// silently wrong energies and forces. A multi-species BIAS potential trained
// by tadah before 1.6.0 has such weights: one intercept column shared by all
// species (Ta-Nb D2_LJ: -622 against -7.65 eV/atom).

#include "catch2/catch.hpp"

#include <tadah/core/context.h>
#include <tadah/core/core_user_config_parser.h>
#include <tadah/core/periodic_table.h>
#include <tadah/tadah.lammps/lammps_tadah.h>

#include <memory>
#include <string>
#include <vector>

using namespace tadah::core;
using tadah::LammpsTadah;

namespace {

/* ATOM Nb Ta, BIAS true, D2_BP (3 functions) for every pair: DSIZE 5 for
 * pair_style tadah (2 intercepts + 3). `model` lines, then `extra`. */
std::vector<std::string> nbta(const std::vector<std::string> &extra,
                              const std::string &model =
                                  "MODEL M_BLR BF_Linear") {
  std::vector<std::string> lines = {
      "ATOM Nb Ta",      "BIAS true",           "NORM false",
      "DIMER false 0 false", "EAMFAST false",   "INIT2B true",
      "TYPE2B D2_BP 3 3 * *", "CGRID2B 2.4 3.1 3.9", "SGRID2B 1.0 1.2 0.9",
      "RCTYPE2B Cut_Cos", "RCUT2B 5.0",         "INITMB false",
      model};
  lines.insert(lines.end(), extra.begin(), extra.end());
  return lines;
}

/* Load a potential the way pair_coeff * * <file> Ta Nb does (LAMMPS type 1
 * Ta, type 2 Nb). */
std::unique_ptr<LammpsTadah> load(const std::vector<std::string> &lines) {
  PeriodicTable::initialize();
  std::unique_ptr<LammpsTadah> lt(
      new LammpsTadah(CoreUserConfigParser().parse(lines)));
  std::vector<std::string> args = {"*", "*", "pot.tadah", "Ta", "Nb"};
  std::vector<char *> argv;
  for (auto &a : args) argv.push_back(&a[0]);
  lt->initialize(static_cast<int>(argv.size()), argv.data());
  return lt;
}

std::string refusal(const std::vector<std::string> &lines) {
  try {
    load(lines);
  } catch (const std::exception &e) {
    return e.what();
  }
  return "<accepted>";
}

const char *kLegacy = "one intercept column shared by all species";

}  // namespace

// Pins: pair_coeff refuses, with a message naming WEIGHTS, DSIZE, BIAS and
// ATOM and the retrain hint ("since 1.3.0-beta.1": the versions whose
// pair_style tadah used one intercept column per species with such a
// potential), an ATOM Nb Ta BIAS true potential with 4 WEIGHTS (one
// intercept shared by the two species, the layout tadah wrote before 1.6.0)
// for its 5-entry descriptor ("has 4 WEIGHTS", "5 components (DSIZE)"); a
// 6-weight one without the hint; an M_KRR Kern_RBF potential whose basis
// vectors have 4 entries (the one-intercept layout: "BASIS holds SBASIS 3
// basis vectors of 4 values each"), with the hint; and a NORM true Kern_RBF
// potential (a non-linear model normalises its descriptors with NMEAN and
// NSTDEV) whose basis fits but whose NMEAN and NSTDEV have 4 values, naming
// NMEAN, with the hint.
// Before the fix: every one of them was accepted (the predictors ran over
// the weights: wrong energies and forces). Before the one-guard rewording:
// the messages said "WEIGHTS has 4 values", "DSIZE 5" and "4
// entries", the hint named no version, the Kern_RBF potential was refused
// by build_sub_objects without the hint ("cannot evaluate a non-linear
// potential trained with BIAS true on several species"), and the NMEAN case
// used BF_Linear, which reads no NMEAN.
TEST_CASE("pair_coeff refuses a potential whose weights do not fit the "
          "descriptor of pair_style tadah",
          "[lammps_tadah][weights_layout]") {
  SECTION("4 WEIGHTS: one shared intercept") {
    const std::string m = refusal(nbta({"WEIGHTS -8.2 0.1 -0.05 0.02"}));
    INFO(m);
    CHECK(m.find("has 4 WEIGHTS") != std::string::npos);
    CHECK(m.find("5 components (DSIZE)") != std::string::npos);
    CHECK(m.find("BIAS true") != std::string::npos);
    CHECK(m.find("ATOM Nb Ta") != std::string::npos);
    CHECK(m.find(kLegacy) != std::string::npos);
    CHECK(m.find("since 1.3.0-beta.1") != std::string::npos);
    CHECK(m.find("retrain") != std::string::npos);
  }
  SECTION("6 WEIGHTS") {
    const std::string m =
        refusal(nbta({"WEIGHTS -8.2 -7.1 0.1 -0.05 0.02 0.3"}));
    INFO(m);
    CHECK(m.find("has 6 WEIGHTS") != std::string::npos);
    CHECK(m.find(kLegacy) == std::string::npos);
  }
  SECTION("Kern_RBF with basis vectors of 4 entries") {
    const std::string m = refusal(nbta(
        {"MPARAMS 1.5", "SBASIS 3",
         "BASIS 1 0.1 0.2 0.3 1 0.4 0.5 0.6 1 0.7 0.8 0.9",
         "WEIGHTS 0.5 -0.2 0.1"},
        "MODEL M_KRR Kern_RBF 1"));
    INFO(m);
    CHECK(m.find("BASIS holds SBASIS 3 basis vectors of 4 values each") !=
          std::string::npos);
    CHECK(m.find(kLegacy) != std::string::npos);
  }
  SECTION("Kern_RBF, NORM true with 4 NMEAN and NSTDEV values") {
    std::vector<std::string> lines = nbta(
        {"MPARAMS 1.5", "SBASIS 3",
         "BASIS 1 0 0.1 0.2 0.3 0 1 0.4 0.5 0.6 1 0 0.7 0.8 0.9",
         "WEIGHTS 0.5 -0.2 0.1", "NMEAN 1 0.5 0.4 0.3",
         "NSTDEV 0 0.2 0.3 0.4"},
        "MODEL M_KRR Kern_RBF 1");
    for (auto &l : lines)
      if (l == "NORM false") l = "NORM true";
    const std::string m = refusal(lines);
    INFO(m);
    CHECK(m.find("NMEAN has 4 values") != std::string::npos);
    CHECK(m.find(kLegacy) != std::string::npos);
  }
}

// Pins: a linear potential (M_BLR BF_Linear, ATOM Nb Ta, BIAS true, NORM
// true, 5 WEIGHTS) loads whatever NMEAN and NSTDEV hold: pair_style tadah
// predicts a linear model with its converted weights and reads neither.
// Here they hold 10 values each, twice DSIZE: what `tadah train -c
// <potential>` wrote before 1.6.0 (the potential's 5 values, then the new
// fit's 5).
// Before the fix: refused, "NORM true: NMEAN has 10 values, but the
// descriptor has DSIZE 5 entries" (a regression of the first per-species
// fix: the potential loaded and predicted before it).
TEST_CASE("pair_coeff loads a linear NORM potential whatever its NMEAN and "
          "NSTDEV",
          "[lammps_tadah][weights_layout][norm]") {
  std::vector<std::string> lines =
      nbta({"WEIGHTS -8.2 -7.1 0.1 -0.05 0.02",
            "NMEAN 0 0 0.4 0.3 0.2 0 0 0.41 0.31 0.21",
            "NSTDEV 1 1 0.2 0.3 0.4 1 1 0.21 0.31 0.41"});
  for (auto &l : lines)
    if (l == "NORM false") l = "NORM true";
  CHECK(refusal(lines) == "<accepted>");
}

// Pins: pair_coeff refuses a linear potential with NORM true and BIAS false
// (ATOM Nb Ta, D2_BP 3: DSIZE 3, 3 WEIGHTS, 3 NMEAN and NSTDEV values), M_BLR
// BF_Linear and M_KRR Kern_Linear, naming NORM true and BIAS false and
// saying to retrain: an earlier version's weight conversion took
// descriptor component 0 for an intercept, so its energies are wrong. With
// NORM false the same potential loads (a pin).
// Before the fix: both NORM true potentials loaded (every size fits) and
// pair_style tadah computed their wrong energies and forces.
TEST_CASE("pair_coeff refuses a linear potential with NORM true and BIAS "
          "false",
          "[lammps_tadah][weights_layout][norm]") {
  for (const std::string model :
       {"MODEL M_BLR BF_Linear", "MODEL M_KRR Kern_Linear 1"}) {
    CAPTURE(model);
    std::vector<std::string> lines = nbta(
        {"WEIGHTS 0.1 -0.05 0.02", "NMEAN 0.4 0.3 0.2", "NSTDEV 0.2 0.3 0.4"},
        model);
    for (auto &l : lines)
      if (l == "BIAS true") l = "BIAS false";
    CHECK(refusal(lines) == "<accepted>");
    for (auto &l : lines)
      if (l == "NORM false") l = "NORM true";
    const std::string m = refusal(lines);
    INFO(m);
    CHECK(m.find("NORM true") != std::string::npos);
    CHECK(m.find("BIAS false") != std::string::npos);
    CHECK(m.find("retrain") != std::string::npos);
  }
}

// Pins: a potential whose weights fit loads: ATOM Nb Ta, BIAS true with 5
// WEIGHTS (DSIZE 5 = the WEIGHTS count; LAMMPS type 1 = Ta has intercept
// column 1, type 2 = Nb column 0, and their per-atom constants are WEIGHTS
// 2 and 1), and an M_KRR Kern_RBF potential with 3 basis vectors of 5
// entries.
// Before the fix: passes as well (a pin: the refusal above must not refuse
// a potential in the per-species layout).
TEST_CASE("pair_coeff loads a potential whose weights fit the descriptor",
          "[lammps_tadah][weights_layout]") {
  std::unique_ptr<LammpsTadah> lt;
  REQUIRE_NOTHROW(lt = load(nbta({"WEIGHTS -8.2 -7.1 0.1 -0.05 0.02"})));
  CHECK(lt->S.descriptor_size() == 5);
  CHECK(lt->S.descriptor_size() == lt->c.size("WEIGHTS"));
  REQUIRE(lt->species_intercept_idx_per_type.size() == 2);
  CHECK(lt->species_intercept_idx_per_type[0] == 1);   // Ta
  CHECK(lt->species_intercept_idx_per_type[1] == 0);   // Nb
  CHECK(lt->bias_per_atom_type[0] == -7.1);
  CHECK(lt->bias_per_atom_type[1] == -8.2);
  CHECK_NOTHROW(load(nbta(
      {"MPARAMS 1.5", "SBASIS 3",
       "BASIS 1 0 0.1 0.2 0.3 0 1 0.4 0.5 0.6 1 0 0.7 0.8 0.9",
       "WEIGHTS 0.5 -0.2 0.1"},
      "MODEL M_KRR Kern_RBF 1")));
}
