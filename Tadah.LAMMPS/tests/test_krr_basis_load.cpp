// pair_style tadah and the shape of a non-linear potential: every K(b, x)
// of a KRR potential (M_KRR with Kern_RBF, Kern_Polynomial, ...) compares
// a descriptor x with a basis vector b of the potential's BASIS (SBASIS
// vectors stored one after another), so the vectors must be there, one per
// weight, and have the length of the descriptor; BF_Polynomial2 (M_BLR)
// has one weight per product x_i x_j (i <= j) of the descriptor's
// components. Only the pair style knows that length
// (DC_Selector::descriptor_size()); M_KRR_Core checks the rest. With BIAS
// true the descriptor has one intercept (bias) component per ATOM species,
// as tadah trains it; a potential with one bias component shared by the
// species (how tadah trained BIAS true with several species before 1.6.0)
// is refused with the hint to retrain it (models::require_weights_fit).
//
// Before the fix: a potential with basis vectors of another length
// loaded, and every K(b, x) read past the shorter of the two; one without
// BASIS loaded with an empty basis and predicted E = 0 for every atom.

#include "catch2/catch.hpp"

#include <tadah/core/context.h>
#include <tadah/core/core_user_config_parser.h>
#include <tadah/core/periodic_table.h>
#include <tadah/tadah.lammps/lammps_tadah.h>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

using namespace tadah::core;
using tadah::LammpsTadah;

namespace {

/* An M_KRR Kern_RBF potential on D2_Blip with 3 functions (descriptor
 * length 3, BIAS false), SBASIS 2, with the given BASIS and WEIGHTS lines
 * ("" leaves the line out). */
std::vector<std::string> rbf_potential(const std::string& basis,
                                       const std::string& weights) {
    std::vector<std::string> lines = {
        "ATOM Ta",
        "WATOM 1",
        "INIT2B true",
        "INITMB false",
        "TYPE2B D2_Blip 3 3 Ta Ta",
        "CGRID2B 2.8 3.5 4.2",
        "SGRID2B 2.0 2.0 2.0",
        "RCTYPE2B Cut_Cos",
        "RCUT2B 5.0",
        "BIAS false",
        "NORM false",
        "VERBOSE 0",
        "MODEL M_KRR Kern_RBF 1",
        "MPARAMS 1.0",
        "SBASIS 2",
    };
    if (!basis.empty()) lines.push_back("BASIS " + basis);
    if (!weights.empty()) lines.push_back("WEIGHTS " + weights);
    return lines;
}

/* The message of the exception loading `lines` throws, "" when none: the
 * potential is built and initialised as `pair_coeff * * pot.tadah <its
 * ATOM species>` does (LammpsTadah::initialize checks that the potential
 * fits the descriptor, models::require_weights_fit). */
std::string load_error(const std::vector<std::string>& lines) {
    PeriodicTable::initialize();
    try {
        LammpsTadah lt(CoreUserConfigParser().parse(lines));
        std::vector<std::string> args = {"*", "*", "pot.tadah"};
        for (std::size_t i = 0; i < lt.c.size("ATOM"); ++i)
            args.push_back(lt.c.get<std::string>("ATOM", i));
        std::vector<char*> argv;
        for (auto& a : args) argv.push_back(&a[0]);
        lt.initialize(static_cast<int>(argv.size()), argv.data());
    } catch (const std::exception& e) {
        return e.what();
    }
    return "";
}

bool contains(const std::string& s, const std::string& part) {
    return s.find(part) != std::string::npos;
}

}  // namespace

/* Pins: the potential with 2 basis vectors of 3 values and 2 weights
 * loads (a pin); with 2 vectors of 4 values (BASIS of 8 values, a multiple
 * of SBASIS 2) it is refused, naming BASIS, the vector length and the
 * descriptor's ("SBASIS 2 basis vectors of 4 values each", "has 3
 * components").
 * Before the fix: the potential with 4-value vectors loaded (no throw). */
TEST_CASE("pair_style tadah refuses KRR basis vectors that do not have the "
          "descriptor's length",
          "[lammps_tadah][krr_basis]") {
    CHECK(load_error(rbf_potential("1 0 0 0.75 1.06 0.49", "0.5 -0.25")) == "");
    const std::string msg =
        load_error(rbf_potential("1 0 0 0 0.75 1.06 0.49 0", "0.5 -0.25"));
    INFO("message: " << msg);
    CHECK(contains(msg, "BASIS holds SBASIS 2 basis vectors of 4 values each"));
    CHECK(contains(msg, "has 3 components"));
}

/* Pins: a KRR potential of a non-linear kernel with WEIGHTS and no BASIS
 * is refused, naming SBASIS and BASIS; one with 3 WEIGHTS for 2 basis
 * vectors is refused, naming WEIGHTS and SBASIS.
 * Before the fix: both loaded; the first predicted E = 0 for every atom
 * (no basis vector, nothing to sum); the second ignored its third weight
 * (the kernel sums over its 2 basis vectors, weights 1 and 2). */
TEST_CASE("pair_style tadah refuses a KRR potential whose basis does not "
          "match its WEIGHTS",
          "[lammps_tadah][krr_basis]") {
    {
        const std::string msg = load_error(rbf_potential("", "0.5 -0.25"));
        INFO("message: " << msg);
        CHECK(contains(msg, "no basis vectors (SBASIS and BASIS)"));
    }
    {
        const std::string msg =
            load_error(rbf_potential("1 0 0 0.75 1.06 0.49", "0.5 -0.25 0.1"));
        INFO("message: " << msg);
        CHECK(contains(msg, "3 WEIGHTS but SBASIS 2"));
    }
}

namespace {

/* A potential on ATOM `atoms` (WATOM 1 each) with D2_Blip of 3 functions
 * for every pair of them, BIAS `bias`, and the MODEL, MPARAMS, SBASIS,
 * BASIS and WEIGHTS lines given ("" leaves a line out). With BIAS true its
 * descriptor has 3 + one intercept component per species: 4 on ATOM Ta, 5
 * on ATOM Nb Ta. */
std::vector<std::string> potential(const std::string& atoms, bool bias,
                                   const std::vector<std::string>& model) {
    const bool two = atoms.find(' ') != std::string::npos;
    std::vector<std::string> lines = {
        "ATOM " + atoms,
        two ? "WATOM 1 1" : "WATOM 1",
        "INIT2B true",
        "INITMB false",
        two ? "TYPE2B D2_Blip 3 3 Ta Ta Ta Nb Nb Nb"
            : "TYPE2B D2_Blip 3 3 Ta Ta",
        "CGRID2B 2.8 3.5 4.2",
        "SGRID2B 2.0 2.0 2.0",
        "RCTYPE2B Cut_Cos",
        "RCUT2B 5.0",
        bias ? "BIAS true" : "BIAS false",
        "NORM false",
        "VERBOSE 0",
    };
    for (const std::string& l : model)
        if (!l.empty()) lines.push_back(l);
    return lines;
}

/* "KEY v v ..." with the n values 0.1, 0.2, ... */
std::string values(const std::string& key, std::size_t n) {
    std::string s = key;
    for (std::size_t v = 0; v < n; ++v) s += " " + std::to_string(0.1 * (v + 1));
    return s;
}

}  // namespace

/* Pins: a non-linear potential with BIAS true on two species (ATOM Nb Ta)
 * laid out with one bias component shared by the species (a descriptor of
 * 4 components: 3 D2_Blip and the shared one), the layout in which tadah
 * trained BIAS true with several species before 1.6.0, is refused by
 * pair_style tadah, whose descriptor has one intercept component per
 * species (5 components), with the message of models::require_weights_fit:
 * the potential's sizes ("BASIS holds SBASIS 2 basis vectors of 4 values
 * each, but the descriptor of the potential has 5 components (DSIZE)"; "has
 * 10 WEIGHTS, but BF_Polynomial2 needs 15 weights"), BIAS true, ATOM Nb Ta,
 * the shared intercept column, that pair_style tadah has used one per
 * species since 1.3.0-beta.1, and to retrain it. Checked for M_KRR Kern_RBF
 * (SBASIS 2, BASIS of 2 x 4 values, 2 WEIGHTS) and M_BLR BF_Polynomial2 (10
 * WEIGHTS, 4 x 5 / 2). The potentials laid out as tadah trains them now, one
 * intercept component per species (BASIS of 2 x 5 values; 15 WEIGHTS), load
 * on ATOM Nb Ta, and the 4-component ones load on ATOM Ta (pins).
 * Before the fix: both 4-component potentials on ATOM Nb Ta were refused
 * by LammpsTadah's own check with "... It is laid out for a descriptor of 4
 * components, with one bias component shared by the species, as tadah
 * trains it ... pair_style tadah cannot evaluate a non-linear potential
 * trained with BIAS true on several species; one trained with BIAS false
 * runs" (since the per-species training of this branch, tadah trains 5
 * components, and pair_style tadah evaluates them), without the sizes or
 * the retrain hint. */
TEST_CASE("pair_style tadah refuses a non-linear multi-species BIAS "
          "potential with one shared intercept, saying to retrain, and "
          "loads one with an intercept per species",
          "[lammps_tadah][krr_basis][bias]") {
    const std::vector<std::string> rbf_shared = {
        "MODEL M_KRR Kern_RBF 1", "MPARAMS 1.0", "SBASIS 2",
        values("BASIS", 8), values("WEIGHTS", 2)};
    const std::vector<std::string> rbf_per_species = {
        "MODEL M_KRR Kern_RBF 1", "MPARAMS 1.0", "SBASIS 2",
        values("BASIS", 10), values("WEIGHTS", 2)};
    const std::vector<std::string> p2_shared = {"MODEL M_BLR BF_Polynomial2",
                                                values("WEIGHTS", 10)};
    const std::vector<std::string> p2_per_species = {
        "MODEL M_BLR BF_Polynomial2", values("WEIGHTS", 15)};
    {
        const std::string msg = load_error(potential("Nb Ta", true, rbf_shared));
        INFO("Kern_RBF: " << msg);
        CHECK(contains(msg, "BASIS holds SBASIS 2 basis vectors of 4 values "
                            "each, but the descriptor of the potential has 5 "
                            "components (DSIZE)"));
    }
    {
        const std::string msg = load_error(potential("Nb Ta", true, p2_shared));
        INFO("BF_Polynomial2: " << msg);
        CHECK(contains(msg, "has 10 WEIGHTS, but BF_Polynomial2 needs 15 "
                            "weights"));
    }
    for (const auto* m : {&rbf_shared, &p2_shared}) {
        INFO("model: " << m->front());
        CHECK(load_error(potential("Ta", true, *m)) == "");
        const std::string msg = load_error(potential("Nb Ta", true, *m));
        INFO("message: " << msg);
        CHECK(contains(msg, "BIAS true"));
        CHECK(contains(msg, "ATOM Nb Ta"));
        CHECK(contains(msg, "one intercept column shared by all species"));
        CHECK(contains(msg, "since 1.3.0-beta.1"));
        CHECK(contains(msg, "retrain"));
    }
    for (const auto* m : {&rbf_per_species, &p2_per_species}) {
        INFO("model: " << m->front());
        CHECK(load_error(potential("Nb Ta", true, *m)) == "");
    }
}

/* Pins: an M_BLR BF_Polynomial2 potential on a descriptor of 3 components
 * (BIAS false) loads with 6 WEIGHTS (3 x 4 / 2, a pin) and is refused with
 * 7 or 5, naming WEIGHTS, the count, the descriptor's 3 components and the
 * 6 weights it needs ("7 WEIGHTS", "3 components", "6").
 * Before the fix: both loaded (no throw): with 5 WEIGHTS every energy
 * would have read past them, with 7 the last was ignored. */
TEST_CASE("pair_style tadah refuses a BF_Polynomial2 potential whose WEIGHTS "
          "do not match its descriptor",
          "[lammps_tadah][krr_basis]") {
    auto p2 = [](std::size_t n) {
        return potential("Ta", false,
                         {"MODEL M_BLR BF_Polynomial2", values("WEIGHTS", n)});
    };
    CHECK(load_error(p2(6)) == "");
    for (std::size_t n : {std::size_t(7), std::size_t(5)}) {
        const std::string msg = load_error(p2(n));
        INFO("WEIGHTS " << n << ": message: " << msg);
        CHECK(contains(msg, std::to_string(n) + " WEIGHTS"));
        CHECK(contains(msg, "3 components"));
        CHECK(contains(msg, "6 weights"));
    }
}
