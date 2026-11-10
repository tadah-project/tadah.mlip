// The weights of a model and the descriptors it predicts from.
//
// Function_Base::weights_count(dsize) is the number of model weights (the
// design-matrix width) of each basis function and kernel for a descriptor
// of dsize entries, and basis_vector_size() the length of the basis
// vectors of a kernel with a basis. require_weights_fit() refuses a
// potential whose WEIGHTS, NMEAN/NSTDEV (NORM true) or basis vectors do not
// fit the descriptor of its configuration, before anything is predicted:
// the dot products of the predictors run over the weights, so a potential
// with weights for another descriptor layout predicted silently wrong
// energies and forces (a potential trained by tadah before 1.6.0 with
// BIAS true and two or more species has one intercept column shared by all
// species, while pair_style tadah builds one per species). It is the one
// check of a potential's fit: tadah predict (MLIP) and pair_style tadah
// both call it, and both give its message.

#include "catch2/catch.hpp"

#include <tadah/core/context.h>
#include <tadah/core/core_types.h>
#include <tadah/core/periodic_table.h>
#include <tadah/models/dc_selector.h>
#include <tadah/models/functions/f_all.h>
#include <tadah/models/functions/weights_fit.h>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

using namespace tadah::core;
using namespace tadah::models;

namespace {

/* BIAS and ATOM, NORM, and n WEIGHTS / NMEAN / NSTDEV values. */
Context pot(bool bias, const std::vector<std::string> &atoms, bool norm,
            std::size_t n_weights, std::size_t n_mean = 0,
            std::size_t n_stdev = 0) {
  Context c;
  c.add("BIAS", bias ? "true" : "false");
  for (const auto &a : atoms) c.add("ATOM", a);
  c.add("NORM", norm ? "true" : "false");
  for (std::size_t i = 0; i < n_weights; ++i) c.add("WEIGHTS", 0.1 * i);
  for (std::size_t i = 0; i < n_mean; ++i) c.add("NMEAN", 0.2 * i);
  for (std::size_t i = 0; i < n_stdev; ++i) c.add("NSTDEV", 1.0 + i);
  return c;
}

Matrix basis_of(std::size_t length, std::size_t count) {
  Matrix b(length, count);
  b.set_zero();
  for (std::size_t j = 0; j < count; ++j) b(j % length, j) = 1.0;
  return b;
}

std::string refusal(const Context &c, std::size_t dsize,
                    const Function_Base &f) {
  try {
    require_weights_fit(c, dsize, f);
  } catch (const std::runtime_error &e) {
    return e.what();
  }
  return "<accepted>";
}

const char *kLegacy = "one intercept column shared by all species";
/* The versions the legacy hint names: pair_style tadah has used one
 * intercept column per species since 1.3.0-beta.1. */
const char *kLegacyWindow = "since 1.3.0-beta.1";

}  // namespace

// Pins: weights_count(dsize) is dsize for BF_Linear and Kern_Linear,
// dsize (dsize + 1) / 2 for BF_Polynomial2, and the number of basis vectors
// for the kernels with a basis (Kern_RBF, Kern_Quadratic, Kern_Polynomial,
// Kern_LQ, Kern_Sigmoid), whatever dsize is; basis_vector_size() is the
// length of those basis vectors, and 0 for BF_Linear, BF_Polynomial2 and
// Kern_Linear (no basis).
// Before the fix: weights_count and basis_vector_size did not exist (run
// with stubs that throw std::logic_error: every section fails on that
// exception).
TEST_CASE("Function_Base::weights_count: the model weights of each basis "
          "function and kernel for a descriptor of dsize entries",
          "[weights_fit]") {
  SECTION("basis functions and the linear kernel") {
    CHECK(BF_Linear().weights_count(4) == 4);
    CHECK(BF_Linear().weights_count(1) == 1);
    CHECK(BF_Polynomial2().weights_count(4) == 10);
    CHECK(BF_Polynomial2().weights_count(3) == 6);
    CHECK(Kern_Linear().weights_count(4) == 4);
    CHECK(BF_Linear().basis_vector_size() == 0);
    CHECK(BF_Polynomial2().basis_vector_size() == 0);
    Kern_Linear kl;
    kl.set_basis(basis_of(4, 5));   // Kern_Linear has no basis
    CHECK(kl.basis_vector_size() == 0);
    CHECK(kl.weights_count(4) == 4);
  }
  SECTION("kernels with a basis") {
    std::vector<std::unique_ptr<Kern_Base>> kernels;
    kernels.emplace_back(new Kern_RBF());
    kernels.emplace_back(new Kern_Quadratic());
    kernels.emplace_back(new Kern_Polynomial());
    kernels.emplace_back(new Kern_LQ());
    kernels.emplace_back(new Kern_Sigmoid());
    for (auto &k : kernels) {
      CAPTURE(k->get_label());
      CHECK(k->weights_count(4) == 0);   // no basis yet
      CHECK(k->basis_vector_size() == 0);
      k->set_basis(basis_of(4, 5));
      CHECK(k->weights_count(4) == 5);
      CHECK(k->weights_count(9) == 5);
      CHECK(k->basis_vector_size() == 4);
    }
  }
}

// Pins: require_weights_fit accepts potentials whose weights fit the
// descriptor: BF_Linear with ATOM Nb Ta and BIAS true on DSIZE 4 with 4
// WEIGHTS (and with NORM true, 4 NMEAN and 4 NSTDEV); BIAS false on DSIZE 2
// with 2 WEIGHTS; BF_Polynomial2 on DSIZE 4 with 10 WEIGHTS; Kern_RBF with
// 5 basis vectors of length 4 and 5 WEIGHTS.
// Before the fix: require_weights_fit did not exist (run with a stub that
// throws std::logic_error: every check fails on that exception).
TEST_CASE("require_weights_fit accepts weights that fit the descriptor",
          "[weights_fit]") {
  PeriodicTable::initialize();
  CHECK(refusal(pot(true, {"Nb", "Ta"}, false, 4), 4, BF_Linear()) ==
        "<accepted>");
  CHECK(refusal(pot(true, {"Nb", "Ta"}, true, 4, 4, 4), 4, BF_Linear()) ==
        "<accepted>");
  CHECK(refusal(pot(false, {"Nb", "Ta"}, false, 2), 2, BF_Linear()) ==
        "<accepted>");
  CHECK(refusal(pot(true, {"Nb", "Ta"}, false, 10), 4, BF_Polynomial2()) ==
        "<accepted>");
  Kern_RBF rbf;
  rbf.set_basis(basis_of(4, 5));
  CHECK(refusal(pot(true, {"Nb", "Ta"}, false, 5), 4, rbf) == "<accepted>");
  // The overload with explicit sizes, as a trained model calls it.
  CHECK_NOTHROW(require_weights_fit(pot(true, {"Nb", "Ta"}, true, 0), 4,
                                    BF_Linear(), TrainedSizes{4, 4, 4}));
}

// Pins: require_weights_fit refuses, with std::runtime_error naming
// WEIGHTS, DSIZE, BIAS and ATOM:
//   - BF_Linear, ATOM Nb Ta, BIAS true, DSIZE 4 with 3 WEIGHTS (the layout
//     of a potential trained before 1.6.0: one intercept column shared by
//     the two species), with the hint that names that layout and says to
//     retrain; with 5 WEIGHTS without the hint;
//   - BIAS false, DSIZE 2 with 3 WEIGHTS ("BIAS false"), no hint;
//   - ATOM Ta (one species), DSIZE 3 with 2 WEIGHTS, no hint;
//   - BF_Polynomial2, ATOM Nb Ta, DSIZE 4: 6 WEIGHTS (the count for the
//     one-intercept layout, DSIZE 3) with the hint, 9 without it;
//   - the explicit-size overload with sizes that do not fit.
// The message says "the potential has <n> WEIGHTS, but <function> needs
// <m> weights for a descriptor of <d> components (DSIZE)"; the hint names
// the versions whose pair_style tadah used one intercept column per
// species ("since 1.3.0-beta.1").
// Before the fix: require_weights_fit did not exist (run with a stub that
// throws std::logic_error: every check fails on the stub's message). Before
// the one-guard rewording: it said "WEIGHTS has <n> values" and
// "DSIZE <d>", and the hint said that pair_style tadah used one column per
// species with every earlier version, so those CHECKs failed.
TEST_CASE("require_weights_fit refuses WEIGHTS that do not fit the "
          "descriptor, naming WEIGHTS, DSIZE, BIAS and ATOM",
          "[weights_fit]") {
  PeriodicTable::initialize();
  SECTION("BF_Linear, two species, the one-intercept layout") {
    const std::string m = refusal(pot(true, {"Nb", "Ta"}, false, 3), 4,
                                  BF_Linear());
    INFO(m);
    CHECK(m.find("has 3 WEIGHTS") != std::string::npos);
    CHECK(m.find("BF_Linear needs 4 weights") != std::string::npos);
    CHECK(m.find("4 components (DSIZE)") != std::string::npos);
    CHECK(m.find("BIAS true") != std::string::npos);
    CHECK(m.find("ATOM Nb Ta") != std::string::npos);
    CHECK(m.find(kLegacy) != std::string::npos);
    CHECK(m.find(kLegacyWindow) != std::string::npos);
    CHECK(m.find("retrain") != std::string::npos);
  }
  SECTION("BF_Linear, two species, another count") {
    const std::string m = refusal(pot(true, {"Nb", "Ta"}, false, 5), 4,
                                  BF_Linear());
    INFO(m);
    CHECK(m.find("has 5 WEIGHTS") != std::string::npos);
    CHECK(m.find("4 components (DSIZE)") != std::string::npos);
    CHECK(m.find(kLegacy) == std::string::npos);
  }
  SECTION("BIAS false") {
    const std::string m = refusal(pot(false, {"Nb", "Ta"}, false, 3), 2,
                                  BF_Linear());
    INFO(m);
    CHECK(m.find("has 3 WEIGHTS") != std::string::npos);
    CHECK(m.find("BIAS false") != std::string::npos);
    CHECK(m.find(kLegacy) == std::string::npos);
  }
  SECTION("one species") {
    const std::string m = refusal(pot(true, {"Ta"}, false, 2), 3,
                                  BF_Linear());
    INFO(m);
    CHECK(m.find("has 2 WEIGHTS") != std::string::npos);
    CHECK(m.find("ATOM Ta") != std::string::npos);
    CHECK(m.find(kLegacy) == std::string::npos);
  }
  SECTION("BF_Polynomial2") {
    const std::string legacy = refusal(pot(true, {"Nb", "Ta"}, false, 6), 4,
                                       BF_Polynomial2());
    INFO(legacy);
    CHECK(legacy.find("has 6 WEIGHTS") != std::string::npos);
    CHECK(legacy.find("needs 10 weights") != std::string::npos);
    CHECK(legacy.find(kLegacy) != std::string::npos);
    const std::string other = refusal(pot(true, {"Nb", "Ta"}, false, 9), 4,
                                      BF_Polynomial2());
    INFO(other);
    CHECK(other.find("has 9 WEIGHTS") != std::string::npos);
    CHECK(other.find(kLegacy) == std::string::npos);
  }
  SECTION("explicit sizes") {
    CHECK_THROWS_WITH(require_weights_fit(pot(true, {"Nb", "Ta"}, false, 4),
                                          4, BF_Linear(),
                                          TrainedSizes{3, 0, 0}),
                      Catch::Contains("has 3 WEIGHTS") &&
                          Catch::Contains(kLegacy));
  }
}

// Pins: with NORM true, a non-linear model (here BF_Polynomial2, 10
// WEIGHTS on DSIZE 4) normalises its descriptors with NMEAN and NSTDEV when
// it predicts, so they must have one value per descriptor entry: NMEAN 3
// (the one-intercept layout of DSIZE 4 with two species) is refused naming
// NMEAN, with the legacy hint; NSTDEV 5 naming NSTDEV, without it; NMEAN
// and NSTDEV of 8 values (twice DSIZE: what `tadah train -c <potential>`
// wrote before 1.6.0, the new values after the potential's) naming both,
// with the hint that they were appended and the model read the first 4, the
// potential's; with NORM false they are not read. A kernel whose basis
// vectors have 3 entries on DSIZE 4 (5 basis vectors, 5 WEIGHTS) is
// refused with "BASIS holds SBASIS 5 basis vectors of 3 values each, but
// the descriptor of the potential has 4 components (DSIZE)", with the
// legacy hint; a kernel with 4 WEIGHTS for 5 basis vectors with "has 4
// WEIGHTS" and "needs 5 weights, one per basis vector".
// Before the fix: require_weights_fit did not exist (run with a stub that
// throws std::logic_error: every check fails on the stub's message). Before
// the one-guard rewording: the NMEAN and NSTDEV checks were run
// with BF_Linear (the 8-value section was refused without the appended
// hint), and the messages said "DSIZE 4" and "the BASIS vectors (SBASIS 5)
// have 3 entries each" and "WEIGHTS has 4 values", so those CHECKs failed.
TEST_CASE("require_weights_fit refuses NMEAN, NSTDEV and basis vectors "
          "that do not fit the descriptor",
          "[weights_fit]") {
  PeriodicTable::initialize();
  SECTION("NMEAN") {
    const std::string m = refusal(pot(true, {"Nb", "Ta"}, true, 10, 3, 4), 4,
                                  BF_Polynomial2());
    INFO(m);
    CHECK(m.find("NMEAN has 3 values") != std::string::npos);
    CHECK(m.find("4 components (DSIZE)") != std::string::npos);
    CHECK(m.find(kLegacy) != std::string::npos);
  }
  SECTION("NSTDEV") {
    const std::string m = refusal(pot(true, {"Nb", "Ta"}, true, 10, 4, 5), 4,
                                  BF_Polynomial2());
    INFO(m);
    CHECK(m.find("NSTDEV has 5 values") != std::string::npos);
    CHECK(m.find(kLegacy) == std::string::npos);
  }
  SECTION("NMEAN and NSTDEV appended to a potential's") {
    const std::string m = refusal(pot(true, {"Nb", "Ta"}, true, 10, 8, 8), 4,
                                  BF_Polynomial2());
    INFO(m);
    CHECK(m.find("NMEAN has 8 values") != std::string::npos);
    CHECK(m.find("NSTDEV has 8 values") != std::string::npos);
    CHECK(m.find("appended") != std::string::npos);
    CHECK(m.find("retrain") != std::string::npos);
    CHECK(m.find(kLegacy) == std::string::npos);
  }
  SECTION("NORM false: NMEAN and NSTDEV are not read") {
    CHECK(refusal(pot(true, {"Nb", "Ta"}, false, 10, 3, 3), 4,
                  BF_Polynomial2()) == "<accepted>");
  }
  SECTION("basis vectors") {
    Kern_RBF rbf;
    rbf.set_basis(basis_of(3, 5));
    const std::string m = refusal(pot(true, {"Nb", "Ta"}, false, 5), 4, rbf);
    INFO(m);
    CHECK(m.find("BASIS holds SBASIS 5 basis vectors of 3 values each") !=
          std::string::npos);
    CHECK(m.find("the descriptor of the potential has 4 components (DSIZE)") !=
          std::string::npos);
    CHECK(m.find(kLegacy) != std::string::npos);
  }
  SECTION("WEIGHTS of a kernel: one per basis vector") {
    Kern_RBF rbf;
    rbf.set_basis(basis_of(4, 5));
    const std::string m = refusal(pot(true, {"Nb", "Ta"}, false, 4), 4, rbf);
    INFO(m);
    CHECK(m.find("has 4 WEIGHTS") != std::string::npos);
    CHECK(m.find("needs 5 weights, one per basis vector") != std::string::npos);
  }
}

// Pins: a linear model (BF_Linear, Kern_Linear) predicts with the weights
// converted to the raw descriptors when it was trained (NORM true): it
// never reads NMEAN or NSTDEV, so require_weights_fit accepts a NORM true
// linear potential (ATOM Nb Ta, BIAS true, DSIZE 4, 4 WEIGHTS) whatever
// NMEAN and NSTDEV hold: 8 values each (what `tadah train -c <potential>`
// wrote before 1.6.0: the potential's 4 values, then the new fit's 4), 3
// values, or none.
// Before the fix: each was refused with "NORM true: NMEAN has <n> values,
// but the descriptor has DSIZE 4 entries" (a regression of the first
// per-species fix: before it, tadah predict and pair_style tadah ran such
// potentials, and their predictions do not depend on NMEAN).
TEST_CASE("require_weights_fit does not check NMEAN and NSTDEV of a linear "
          "model, which predicts without them",
          "[weights_fit]") {
  PeriodicTable::initialize();
  for (std::size_t n : {std::size_t(8), std::size_t(3), std::size_t(0)}) {
    CAPTURE(n);
    CHECK(refusal(pot(true, {"Nb", "Ta"}, true, 4, n, n), 4, BF_Linear()) ==
          "<accepted>");
    CHECK(refusal(pot(true, {"Nb", "Ta"}, true, 4, n, n), 4, Kern_Linear()) ==
          "<accepted>");
  }
}

// Pins: a linear potential (BF_Linear, Kern_Linear) with NORM true and
// BIAS false is refused whatever its sizes (ATOM Ta, DSIZE 2, 2 WEIGHTS,
// NMEAN and NSTDEV): tadah has not trained such a potential since this
// change (NORM true with a linear model needs BIAS true), and one written
// by an earlier version is wrong, its weights converted from the
// normalised descriptors with descriptor component 0 taken for an
// intercept. The message names NORM true, BIAS false and the function,
// says the energies are wrong, and says to retrain with NORM false or BIAS
// true. The same potential with NORM false (a pin), and a non-linear one
// (BF_Polynomial2, 3 WEIGHTS) with NORM true and BIAS false (a pin: it
// normalises its descriptors when it predicts and needs no intercept), are
// accepted.
// Before the fix: both linear NORM true potentials were accepted (every
// size fits), and tadah predict and pair_style tadah predicted their wrong
// energies (M_KRR Kern_Linear on the quickstart Ta data: -0.44 against
// -8.04 eV/atom).
TEST_CASE("require_weights_fit refuses a linear potential with NORM true "
          "and BIAS false",
          "[weights_fit][norm]") {
  PeriodicTable::initialize();
  for (const Function_Base *f :
       std::vector<const Function_Base *>{new BF_Linear(), new Kern_Linear()}) {
    std::unique_ptr<const Function_Base> owner(f);
    CAPTURE(f->get_label());
    const std::string m = refusal(pot(false, {"Ta"}, true, 2, 2, 2), 2, *f);
    INFO(m);
    CHECK(m.find("NORM true") != std::string::npos);
    CHECK(m.find("BIAS false") != std::string::npos);
    CHECK(m.find(f->get_label()) != std::string::npos);
    CHECK(m.find("wrong") != std::string::npos);
    CHECK(m.find("retrain") != std::string::npos);
    CHECK(m.find("NORM false") != std::string::npos);
    CHECK(refusal(pot(false, {"Ta"}, false, 2), 2, *f) == "<accepted>");
  }
  CHECK(refusal(pot(false, {"Ta"}, true, 3, 2, 2), 2, BF_Polynomial2()) ==
        "<accepted>");
}

// Pins: the refusal of a linear potential with NORM true and BIAS false
// says how each model's conversion of earlier versions made it wrong:
// BF_Linear (M_BLR) up to 1.2.0 took descriptor component 0 for an
// intercept (its energies and forces are wrong), and from 1.3.0-beta.1,
// when tadah hpo and the API wrote such a potential (tadah train refused
// the configuration), it dropped the mean correction (its energies are off
// by the same constant for every atom, its forces are right); Kern_Linear
// (M_KRR) took descriptor component 0 for an intercept until this version
// (its energies and forces are wrong).
// Before the fix: both messages said "descriptor component 0 taken for an
// intercept (M_KRR Kern_Linear until this version; M_BLR BF_Linear until
// 1.2.0, after which tadah train refused it), so its energies and forces
// are wrong", which is false for the M_BLR potentials tadah hpo wrote from
// 1.3.0-beta.1.
TEST_CASE("require_weights_fit: the refusal of a linear NORM true BIAS false "
          "potential gives each model's history",
          "[weights_fit][norm]") {
  PeriodicTable::initialize();
  const Context c = pot(false, {"Ta"}, true, 2, 2, 2);
  const std::string blr = refusal(c, 2, BF_Linear());
  INFO(blr);
  CHECK(blr.find("M_BLR BF_Linear") != std::string::npos);
  CHECK(blr.find("up to 1.2.0") != std::string::npos);
  CHECK(blr.find("descriptor component 0") != std::string::npos);
  CHECK(blr.find("from 1.3.0-beta.1") != std::string::npos);
  CHECK(blr.find("tadah hpo") != std::string::npos);
  CHECK(blr.find("dropped the mean correction") != std::string::npos);
  CHECK(blr.find("off by the same constant for every atom") !=
        std::string::npos);
  CHECK(blr.find("forces are right") != std::string::npos);
  const std::string krr = refusal(c, 2, Kern_Linear());
  INFO(krr);
  CHECK(krr.find("M_KRR Kern_Linear") != std::string::npos);
  CHECK(krr.find("descriptor component 0") != std::string::npos);
  CHECK(krr.find("energies and forces are wrong") != std::string::npos);
  CHECK(krr.find("dropped") == std::string::npos);
  CHECK(krr.find("1.2.0") == std::string::npos);
}

// Pins: DC_Selector, the descriptor builder of pair_style tadah, gives
// BIAS true one intercept column per ATOM species (ATOM Nb Ta: bias 2, the
// two-body block from column 2, DSIZE 2 + its size) and refuses BIAS true
// without ATOM, naming ATOM.
// Before the fix: passes as well (a pin: the refusal is CORE's
// n_intercept_columns, CORE ad5b2ec; before that DC_Selector built no
// intercept column for BIAS true without ATOM, while MLIP's DescriptorsCalc
// built one).
TEST_CASE("DC_Selector: one intercept column per ATOM species; BIAS true "
          "without ATOM is refused",
          "[weights_fit][dc_selector]") {
  PeriodicTable::initialize();
  Context c;
  c.add("INIT2B", "true");
  c.add("INITMB", "false");
  c.add("TYPE2B", "D2_LJ");
  c.add("TYPE2B", "*");
  c.add("TYPE2B", "*");
  c.add("RCTYPE2B", "Cut_Cos");
  c.add("RCUT2B", 5.0);
  c.add("BIAS", "true");
  Context no_atom = c;
  c.add("ATOM", "Nb");
  c.add("ATOM", "Ta");
  DC_Selector dcs(c);
  CHECK(dcs.bias == 2);
  CHECK(dcs.d2b->get_fidx() == 2);
  CHECK(dcs.descriptor_size() == 2 + dcs.d2b->size());
  CHECK_THROWS_WITH(DC_Selector(no_atom), Catch::Contains("ATOM"));
}
