// pair_style tadah evaluates a DIMER potential (compute_dimers) with every
// atom as Z = 1, whatever its element: a two-body or many-body descriptor
// declared for specific element pairs (`TYPE2B D2_Blip 4 4 N N`) applies to
// no pair of Z = 1 atoms, and the model predicted from all-zero
// descriptors without a word. Such a potential is refused when it is
// loaded; `* *` element pairs are required.
//
// tests_data/pots/tadah.n2 is a verbatim copy of the published Tadah!N2
// potential (Tadah.MLIP docs/source/mlips/tadah.n2), a DIMER model whose
// descriptors are declared for `* *`.

#include "catch2/catch.hpp"

#include <tadah/core/context.h>
#include <tadah/core/core_user_config_parser.h>
#include <tadah/core/periodic_table.h>
#include <tadah/tadah.lammps/lammps_tadah.h>

#include <fstream>
#include <memory>
#include <string>
#include <vector>

using namespace tadah::core;
using tadah::LammpsTadah;

namespace {

const char* kN2 = "tests_data/pots/tadah.n2";
const char* kDimerError =
    "DIMER evaluates every atom as Z=1; declare TYPE2B/TYPEMB with * * "
    "element pairs";

/* The lines of the N2 potential, with the TYPE2B and TYPEMB lines replaced
 * by `type2b` and `typemb` when they are not empty. */
std::vector<std::string> n2_lines(const std::string& type2b,
                                  const std::string& typemb) {
    std::ifstream in(kN2);
    REQUIRE(in.is_open());
    std::vector<std::string> lines;
    for (std::string line; std::getline(in, line);) {
        if (!type2b.empty() && line.rfind("TYPE2B", 0) == 0) line = type2b;
        if (!typemb.empty() && line.rfind("TYPEMB", 0) == 0) line = typemb;
        lines.push_back(line);
    }
    return lines;
}

/* Load a potential the way pair_coeff * * <file> N does. */
std::unique_ptr<LammpsTadah> load(const std::vector<std::string>& lines) {
    PeriodicTable::initialize();
    std::unique_ptr<LammpsTadah> lt(
        new LammpsTadah(CoreUserConfigParser().parse(lines)));
    std::vector<std::string> args = {"*", "*", kN2, "N"};
    std::vector<char*> argv;
    for (auto& a : args) argv.push_back(&a[0]);
    lt->initialize(static_cast<int>(argv.size()), argv.data());
    return lt;
}

}  // namespace

TEST_CASE("pair_style tadah loads the published N2 DIMER potential",
          "[dimer][lammps_tadah]") {
    std::unique_ptr<LammpsTadah> lt;
    REQUIRE_NOTHROW(lt = load(n2_lines("", "")));
    CHECK(lt->dimer);
    CHECK(lt->init2b);
    CHECK(lt->initmb);
}

TEST_CASE("pair_style tadah refuses a DIMER potential whose descriptors "
          "name specific element pairs", "[dimer][lammps_tadah]") {
    const std::string b2_all = "TYPE2B D2_Blip 4 4 * *";
    const std::string mb_all = "TYPEMB DM_Blip 1 4 4 * *";
    for (const auto& lines :
         {n2_lines("TYPE2B D2_Blip 4 4 N N", "TYPEMB DM_Blip 1 4 4 N N"),
          n2_lines("TYPE2B D2_Blip 4 4 N N", mb_all),
          n2_lines(b2_all, "TYPEMB DM_Blip 1 4 4 N N"),
          n2_lines("TYPE2B D2_Blip 4 4 * N", mb_all)}) {
        std::string type_lines;
        for (const auto& l : lines)
            if (l.rfind("TYPE", 0) == 0) type_lines += l + "; ";
        INFO(type_lines);
        CHECK_THROWS_WITH(load(lines), Catch::Contains(kDimerError));
    }
}
