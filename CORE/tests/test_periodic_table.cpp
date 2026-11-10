#include "catch2/catch.hpp"
#include <tadah/core/periodic_table.h>

using namespace tadah::core;


TEST_CASE("Find Element by Symbol", "[find_by_symbol]") {
    PeriodicTable::initialize();

    SECTION("Known symbol") {
        const Element oxygen = PeriodicTable::find_by_symbol("O");
        REQUIRE(oxygen.symbol == "O");
        REQUIRE(oxygen.Z == 8);
    }

    SECTION("Unknown symbol") {
        REQUIRE_THROWS(
            PeriodicTable::find_by_symbol("Xx")
        );
    }
}

TEST_CASE("Find Element by Name", "[find_by_name]") {
    PeriodicTable::initialize();

    SECTION("Known name") {
        const Element carbon = PeriodicTable::find_by_name("Carbon");
        REQUIRE(carbon.symbol == "C");
        REQUIRE(carbon.Z == 6);
    }

    SECTION("Unknown name") {
        REQUIRE_THROWS(
            PeriodicTable::find_by_name("Unobtainium")
        );
    }
}

TEST_CASE("Find Element by Atomic Number (Z)", "[find_by_Z]") {
    PeriodicTable::initialize();

    SECTION("Known atomic number") {
        const Element& iron = PeriodicTable::find_by_Z(26);
        REQUIRE(iron.symbol == "Fe");
        REQUIRE(iron.Z == 26);
    }

    SECTION("Unknown atomic number") {
        REQUIRE_THROWS(
            PeriodicTable::find_by_Z(999)
        );
    }
}

TEST_CASE("Get Atomic Mass", "[get_mass]") {
    PeriodicTable::initialize();

    SECTION("By atomic number") {
        double mass_O = PeriodicTable::get_mass(8);
        REQUIRE(mass_O == Approx(15.9990).epsilon(0.0001));
    }

    SECTION("By Element object") {
        const Element gold = PeriodicTable::find_by_symbol("Au");
        double mass_Au = PeriodicTable::get_mass(gold);
        REQUIRE(mass_Au == Approx(196.967).epsilon(0.0001));
    }

    SECTION("Unknown atomic number") {
        REQUIRE_THROWS(
            PeriodicTable::get_mass(999)
        );
    }
}

TEST_CASE("PeriodicTable::initialize() coverage") {
    // The first call should fill symbol, name, Z, masses
    // if they are empty, hitting lines 27–29.
    // If they've already been filled by a previous test, it won't refill,
    // but the code path is still visited at least once per suite.

    // We manually clear the static maps to ensure a fresh state for coverage.
    // This is optional when the test harness is re-run from scratch each time.
    PeriodicTable::symbol.clear();
    PeriodicTable::name.clear();
    PeriodicTable::Z.clear();
    PeriodicTable::masses.clear();

    REQUIRE(PeriodicTable::symbol.empty());
    REQUIRE(PeriodicTable::masses.empty());

    // Now call initialize()
    REQUIRE_NOTHROW(PeriodicTable::initialize());

    // Should have some entries now
    REQUIRE_FALSE(PeriodicTable::symbol.empty());
    REQUIRE_FALSE(PeriodicTable::masses.empty());
}

// 2) Testing find_by_name() with a name that doesn't exist (lines ~56–58).
//    We expect a thrown exception from name.at(s).
TEST_CASE("PeriodicTable::find_by_name() unknown name") {
    // Ensure the table is initialized
    PeriodicTable::initialize();

    // Some obviously invalid name
    std::string nonexistent = "UnobtainiumXYZ";
    REQUIRE_THROWS_AS(
        PeriodicTable::find_by_name(nonexistent),
        std::out_of_range
    );
}

// 3) Testing find_by_symbol() with an unknown symbol (lines ~64–72).
//    This re-throws a std::runtime_error if symbol not found.
TEST_CASE("PeriodicTable::find_by_symbol() unknown symbol") {
    PeriodicTable::initialize();

    std::string unknownSym = "Xx";
    // code re-throws as std::runtime_error("Unknown symbol: " + s)
    REQUIRE_THROWS_WITH(
        PeriodicTable::find_by_symbol(unknownSym),
        Catch::Matchers::Contains("Unknown symbol: Xx")
    );
}

// 4) Testing find_by_Z() with an invalid atomic number (lines ~75–88).
//    Should re-throw a std::runtime_error if not found.
TEST_CASE("PeriodicTable::find_by_Z() unknown Z") {
    PeriodicTable::initialize();

    int unknownZ = 9999; // definitely not in the table
    REQUIRE_THROWS_WITH(
        PeriodicTable::find_by_Z(unknownZ),
        Catch::Matchers::Contains("Unknown atomic number: 9999")
    );
}

TEST_CASE("PeriodicTable::get_mass() unknown atomic number") {
    PeriodicTable::initialize();

    int invalidZ = 9999;
    REQUIRE_THROWS_AS(
        PeriodicTable::get_mass(invalidZ),
        std::runtime_error
    );
}

TEST_CASE("PeriodicTable::get_mass(Element) coverage with large Z in test", "[PeriodicTable]") {
    PeriodicTable::initialize();

    // Suppose we forcibly patch an Element’s Z after creation:
    Element e("U", 92); // valid Uranium
    // Then forcibly set e.Z = 9999 (Z is a public, non-const member)
    e.Z = 9999;

    REQUIRE_THROWS_AS(
        PeriodicTable::get_mass(e),
        std::runtime_error
    );
}
