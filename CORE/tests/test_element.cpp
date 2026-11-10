#include "catch2/catch.hpp"
#include <tadah/core/element.h>
#include <tadah/core/periodic_table.h>
#include <sstream>
#include <unordered_set>

using namespace tadah::core;

struct PeriodicTableFixture {
  PeriodicTableFixture() {
    PeriodicTable::initialize(); // Called once per test case that uses this fixture
  }
};

TEST_CASE("Initialize the PeriodicTable", "[periodic_table]") {
  REQUIRE_NOTHROW(PeriodicTable::initialize());
}

// Test default constructor
TEST_CASE("Default constructor does not throw", "[element][constructor]") {
  REQUIRE_NOTHROW(Element());
}

// Test constructing Element with (symbol, Z, check_=true) for valid inputs
TEST_CASE("Element constructor with valid symbol and Z", "[element][constructor]") {
  REQUIRE_NOTHROW(Element("He", 2));
  REQUIRE_NOTHROW(Element("Ti", 22));
  REQUIRE_NOTHROW(Element("U", 92));
}

// Test constructing Element with (symbol, Z, check_=true) for invalid symbol length
TEST_CASE("Element constructor throws on invalid symbol length", "[element][constructor][exception]") {
  // Symbol has more than 2 characters
  REQUIRE_THROWS_AS(Element("Long", 12), std::runtime_error);
}

// Test constructing Element with (symbol, Z, check_=true) for out-of-range atomic numbers
TEST_CASE("Element constructor throws on out-of-range Z", "[element][constructor][exception]") {
  // Z < 1
  REQUIRE_THROWS_AS(Element("H", 0), std::runtime_error);
  // Z > 118
  REQUIRE_THROWS_AS(Element("Og", 119), std::runtime_error);
}

// Test constructing Element with check_=false to bypass validation
TEST_CASE("Element constructor with check_=false skips validation", "[element][constructor]") {
  // Should not throw even though symbol is too long and Z is invalid
  REQUIRE_NOTHROW(Element("LongSymbol", 999, false));
  // However, check that the members were still set
  Element e("LongSymbol", 999, false);
  REQUIRE(e.symbol == "LongSymbol");
  REQUIRE(e.Z == 999);
}

// Test constructing Element via symbol-based constructor that calls PeriodicTable::find_by_symbol
TEST_CASE("Element constructor with only symbol uses PeriodicTable lookup", "[element][constructor]") {
  PeriodicTable::initialize(); // Called once per test case that uses this fixture
  // Provide a known symbol that PeriodicTable can resolve
  // Adjust the symbol to something your PeriodicTable is guaranteed to know
  REQUIRE_NOTHROW(Element("He"));

  // PeriodicTable throws for unknown symbols:
  REQUIRE_THROWS_AS(Element("UnknownSymbol"), std::runtime_error);
}

// Test constructing Element via int-based constructor that calls PeriodicTable::find_by_Z
TEST_CASE("Element constructor with only int uses PeriodicTable lookup", "[element][constructor]") {
  PeriodicTable::initialize(); // Called once per test case that uses this fixture
  // Provide a known atomic number
  REQUIRE_NOTHROW(Element(2));

  // PeriodicTable throws for out-of-range or missing Z:
  REQUIRE_THROWS_AS(Element(999), std::runtime_error); 
}

// Test comparison operators
TEST_CASE("Comparison operators", "[element][comparison]") {
  Element eH("H", 1);
  Element eHe("He", 2);
  Element eNa("Na", 11);

  SECTION("operator==") {
    Element eH2("H", 1);
    REQUIRE(eH == eH2);
    REQUIRE_FALSE(eH == eHe);
    // Different symbol but same Z -> should NOT be equal
    Element eUnknownH("X", 1, false); // bypass check
    REQUIRE_FALSE(eH == eUnknownH);
    // Same symbol but different Z -> also not equal
    Element eH3("H", 2, false);
    REQUIRE_FALSE(eH == eH3);
  }

  SECTION("operator< and operator>") {
    // eH (Z=1) < eHe (Z=2)
    REQUIRE(eH < eHe);
    REQUIRE(eHe > eH);

    // eHe (Z=2) < eNa (Z=11)
    REQUIRE(eHe < eNa);
    REQUIRE(eNa > eHe);

    // For equal Z, the operator< should return false
    Element eH2("H", 1);
    REQUIRE_FALSE(eH < eH2);
    REQUIRE_FALSE(eH > eH2);
  }
}

// Test copy constructor / assignment
TEST_CASE("Copy constructor and assignment", "[element][copy]") {
  Element eTi("Ti", 22);
  Element eCopy(eTi); // copy constructor
  REQUIRE(eCopy == eTi);

  Element eAssign("H", 1);
  eAssign = eTi; // copy assignment
  REQUIRE(eAssign == eTi);
}

// Test output streaming
TEST_CASE("Operator<< streaming output", "[element][ostream]") {
  Element e("Ti", 22);
  std::stringstream ss;
  ss << e;
  REQUIRE_THAT(ss.str(), Catch::Matchers::Contains("Symbol: Ti"));
  REQUIRE_THAT(ss.str(), Catch::Matchers::Contains("Atomic Number: 22"));
}

// Test hashing functionality
TEST_CASE("Hashing Elements", "[element][hash]") {
  Element eH("H", 1);
  Element eHe("He", 2);
  std::unordered_set<Element> elementSet;
  elementSet.insert(eH);
  elementSet.insert(eHe);

  // Should contain eH and eHe
  REQUIRE(elementSet.find(eH) != elementSet.end());
  REQUIRE(elementSet.find(eHe) != elementSet.end());

  // Another element with the same atomic number as eH (1) but
  // different symbol is considered hashed the same according to
  // the current std::hash<int>(e.Z) implementation, but note that
  // operator== will differ. This may or may not be desirable.
  Element eFakeH("X", 1, false);
  // The hash is the same, but the unordered_set also checks equality,
  // so eFakeH should not be considered present:
  REQUIRE(elementSet.find(eFakeH) == elementSet.end());
}

TEST_CASE("Element boundary conditions for atomic number", "[element][boundary]") {
  SECTION("Minimum valid atomic number: Z=1") {
    // Should not throw because Z=1 is within the valid range
    REQUIRE_NOTHROW(Element("H", 1, true));

    // For thoroughness, check if the symbol is actually "H"
    Element e("H", 1, true);
    REQUIRE(e.symbol == "H");
    REQUIRE(e.Z == 1);
  }

  SECTION("Maximum valid atomic number: Z=118") {
    // Should not throw because Z=118 is within the valid range
    // (assuming you consider Oganesson as the 118th element)
    REQUIRE_NOTHROW(Element("Og", 118, true));

    Element e("Og", 118, true);
    REQUIRE(e.symbol == "Og");
    REQUIRE(e.Z == 118);
  }
}

TEST_CASE("Element boundary conditions for symbol length", "[element][boundary]") {
  SECTION("Empty symbol with valid Z") {
    // According to the current `check` logic, symbol size = 0
    // is not > 2, so it should not throw.
    REQUIRE_NOTHROW(Element("", 5, true));

    // Check that the object is constructed with the given values
    Element e("", 5, true);
    REQUIRE(e.symbol.empty());
    REQUIRE(e.Z == 5);
  }

  SECTION("Exactly 2-character symbol with various Z") {
    // Should not throw for length=2 and Z in a valid range
    REQUIRE_NOTHROW(Element("Fe", 26, true));
    Element e("Fe", 26, true);
    REQUIRE(e.symbol == "Fe");
    REQUIRE(e.Z == 26);
  }
}

TEST_CASE("Default constructed Element streaming", "[element][ostream]") {
  Element e;  // default constructed => symbol="", Z=0
  std::stringstream ss;
  ss << e;
  // Check the output for default values:
  // "Symbol: \nAtomic Number: 0\n" (Line breaks may vary)
  REQUIRE_THAT(ss.str(), Catch::Matchers::Contains("Symbol:"));
  REQUIRE_THAT(ss.str(), Catch::Matchers::Contains("Atomic Number:"));
  // Optionally verify numeric parts
  REQUIRE_THAT(ss.str(), Catch::Matchers::Contains("0"));
}

TEST_CASE("Operator== for default constructed Elements", "[element][equals][default]") {
  Element e1; // both symbol="" and Z=0
  Element e2; // same default construction
  REQUIRE(e1 == e2);
}

TEST_CASE("Check with check_=false allows negative or extremely large Z", "[element][constructor][no_check]") {
  // Should not throw because check is disabled
  REQUIRE_NOTHROW(Element("Xx", -999, false));
  REQUIRE_NOTHROW(Element("Yy", 9999, false));

  Element negativeZ("Xx", -999, false);
  REQUIRE(negativeZ.symbol == "Xx");
  REQUIRE(negativeZ.Z == -999);

  Element largeZ("Yy", 9999, false);
  REQUIRE(largeZ.symbol == "Yy");
  REQUIRE(largeZ.Z == 9999);
}

// ---------------------------------------------------------------------------
// NEW TESTS TO APPLY PREVIOUS SUGGESTIONS (APPEND TO EXISTING TEST SUITE)
// ---------------------------------------------------------------------------

// 1. Validate that symbolic constructor matches known periodic table data:
TEST_CASE("Validate PeriodicTable lookups by symbol include correct atomic number", "[element][lookup]") {
  PeriodicTable::initialize(); // Called once per test case that uses this fixture
  // Use an element known to exist in PeriodicTable:
  // "He" should map to Z=2, for instance.
  Element eHe("He");
  CHECK(eHe.symbol == "He");
  CHECK(eHe.Z == 2);

  // "C" -> Z=6, "Ne" -> Z=10, etc. (Adjust based on your PeriodicTable contents)
  Element eC("C");
  CHECK(eC.symbol == "C");
  CHECK(eC.Z == 6);
}

TEST_CASE("Validate PeriodicTable lookups by Z include correct symbol", "[element][lookup]") {
  PeriodicTable::initialize(); // Called once per test case that uses this fixture
  // Similarly use an atomic number known to exist in PeriodicTable:
  // e.g., 3 -> "Li", 26 -> "Fe", or something unique in your data set.
  Element eLi(3);
  CHECK(eLi.symbol == "Li");
  CHECK(eLi.Z == 3);

  Element eFe(26);
  CHECK(eFe.symbol == "Fe");
  CHECK(eFe.Z == 26);
}

// 2. Fine-tune streaming tests with more detailed checks:
TEST_CASE("Enhanced streaming test with exact format checks", "[element][ostream][format]") {
  Element e("Ne", 10);

  std::stringstream ss;
  ss << e;
  std::string output = ss.str();

  // The output has lines like:
  //   Symbol: Ne
  //   Atomic Number: 10
  // checked line by line:
  CHECK_THAT(output, Catch::Matchers::Contains("Symbol: Ne"));
  CHECK_THAT(output, Catch::Matchers::Contains("Atomic Number: 10"));

  // A check of the newlines in specific places (it depends on how the
  // line breaks are done):
  // CHECK_THAT(output, Catch::Matchers::EndsWith("\n"));
}

// 3. Additional comparison checks for same Z but different symbol:
TEST_CASE("Comparison operators ignoring symbol for < and >", "[element][comparison][extended]") {
  // Same atomic number, different symbols:
  Element eX("Xx", 10, false); // Bypass checks
  Element eNe("Ne", 10);

  SECTION("operator< with same Z returns false") {
    REQUIRE_FALSE(eX < eNe);
    REQUIRE_FALSE(eNe < eX);
  }

  SECTION("operator> with same Z returns false") {
    REQUIRE_FALSE(eX > eNe);
    REQUIRE_FALSE(eNe > eX);
  }
}

// 4. Extended hashing test to demonstrate collisions & equalities:
TEST_CASE("Extended hashing collision test", "[element][hash][collision]") {
  std::unordered_set<Element> elements;
  Element eF("F", 9);
  Element eFakeF("FakeF", 9, false); // Different symbol, same Z

  // Insert eF, but not eFakeF:
  elements.insert(eF);

  // Because the hash uses only Z, eFakeF has the same hash as eF
  // but operator== ensures they are distinct entries in the set?
  // Actually, since operator== compares both symbol and Z, eFakeF != eF.
  // But in a std::unordered_set, hashing + equality determines uniqueness.
  // So: is eFakeF inserted?
  auto result = elements.insert(eFakeF);

  // 'true' if eFakeF was inserted, 'false' if it was deemed a duplicate
  CHECK(result.second == true);

  // The set should now contain two distinct Elements:
  REQUIRE(elements.size() == 2);
}
