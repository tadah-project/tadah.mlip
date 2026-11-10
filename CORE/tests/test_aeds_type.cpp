#define CATCH_AEDS_TYPE_MAIN
#include "catch2/catch.hpp"
#include <tadah/core/aeds_type.h>
#include <sstream>
#include <stdexcept>

using namespace tadah::core;

TEST_CASE("aeds_type2 default constructor") {
    aeds_type2 array;
    REQUIRE(array.size() == 0);
    REQUIRE(array.cols() == 0);
    REQUIRE(array.rows() == 0);
}

TEST_CASE("aeds_type2 parameterized constructor") {
    size_t dim = 4;
    size_t natoms = 3;
    aeds_type2 array(dim, natoms);
    REQUIRE(array.size() == dim * natoms);
    REQUIRE(array.cols() == natoms);
    REQUIRE(array.rows() == dim);
    
    for (size_t i = 0; i < dim; ++i) {
        for (size_t j = 0; j < natoms; ++j) {
            REQUIRE(array(i, j) == 0.0);
        }
    }
}

TEST_CASE("aeds_type2 copy constructor") {
    size_t dim = 4;
    size_t natoms = 3;
    aeds_type2 array1(dim, natoms);
    array1(1, 1) = 5.0;
    
    aeds_type2 array2(array1);
    REQUIRE(array2.size() == array1.size());
    REQUIRE(array2.cols() == array1.cols());
    REQUIRE(array2.rows() == array1.rows());
    REQUIRE(array2(1, 1) == 5.0);
}

TEST_CASE("aeds_type2 move constructor") {
    size_t dim = 4;
    size_t natoms = 3;
    aeds_type2 array1(dim, natoms);
    array1(1, 1) = 5.0;
    
    aeds_type2 array2(std::move(array1));
    REQUIRE(array2.size() == dim * natoms);
    REQUIRE(array2(1, 1) == 5.0);
}

TEST_CASE("aeds_type2 copy assignment operator") {
    size_t dim = 4;
    size_t natoms = 3;
    aeds_type2 array1(dim, natoms);
    aeds_type2 array2;
    array1(1, 1) = 5.0;
    
    array2 = array1;
    REQUIRE(array2.size() == array1.size());
    REQUIRE(array2(1, 1) == 5.0);
}

TEST_CASE("aeds_type2 move assignment operator") {
    size_t dim = 4;
    size_t natoms = 3;
    aeds_type2 array1(dim, natoms);
    aeds_type2 array2;
    array1(1, 1) = 5.0;
    
    array2 = std::move(array1);
    REQUIRE(array2.size() == dim * natoms);
    REQUIRE(array2(1, 1) == 5.0);
}

TEST_CASE("aeds_type2 resize and set_zero") {
    size_t dim = 4;
    size_t natoms = 3;
    aeds_type2 array(dim, natoms);
    array(0, 0) = 3.5;
    
    array.resize(2, 2);
    REQUIRE(array.size() == 4);
    REQUIRE(array.cols() == 2);
    REQUIRE(array.rows() == 2);

    array.set_zero();
    for (size_t i = 0; i < array.rows(); ++i) {
        for (size_t j = 0; j < array.cols(); ++j) {
            REQUIRE(array(i, j) == 0.0);
        }
    }
}

TEST_CASE("aeds_type2 self-assignment copy operator=", "[aeds_type2][new]") {
    aeds_type2 arr(3, 2);
    // Fill some data
    arr(0,0) = 1.1;
    arr(2,1) = 9.9;

    // Self assignment
    aeds_type2 &ref = arr;
    REQUIRE_NOTHROW(arr = ref);
    REQUIRE(arr.size() == 3*2);
    REQUIRE(arr(2,1) == 9.9);
}

TEST_CASE("aeds_type2 self-assignment move operator=", "[aeds_type2][new]") {
    aeds_type2 arr(3, 2);
    arr(0,0) = 2.2;
    arr(1,1) = 7.7;

    // Self move assignment is generally ill-defined, 
    // but it must not crash.
    aeds_type2 &ref = arr;
    REQUIRE_NOTHROW(arr = std::move(ref));
    REQUIRE(arr.size() == 6);
    // no guarantee about content after self move
}

TEST_CASE("aeds_type2 operator()(r,c) potential out-of-bounds", "[aeds_type2][new]") {
    // There's no built-in boundary check => can cause undefined behavior 
    // for arr(rows(), anyCol). The test only shows that it doesn't throw
    // because there's no check. 
    aeds_type2 arr(2, 2);
    REQUIRE_NOTHROW(arr(2, 0));
    REQUIRE_NOTHROW(arr(0, 2));
    // This is *undefined behavior*, not recommended in production. 
    // Typically you'd want an assert or throw, but the code lacks it.
}

TEST_CASE("aeds_type2 operator[] access columns", "[aeds_type2][new]") {
    // Retrieves columns as aed_type references
    size_t dim = 3;
    size_t natoms = 2;
    aeds_type2 arr(dim, natoms);

    // Write some data
    arr(0,0) = 10;
    arr(1,0) = 11;
    arr(2,0) = 12;
    arr(0,1) = 20;
    arr(1,1) = 21;

    // operator[](col) => an aed_type for that column
    aed_type &col0 = arr[0];
    REQUIRE(col0.size() == dim);
    REQUIRE(col0[0] == 10.0);
    REQUIRE(col0[1] == 11.0);
    REQUIRE(col0[2] == 12.0);

    // similarly for col1
    aed_type &col1 = arr[1];
    REQUIRE(col1.size() == dim);
    REQUIRE(col1[0] == 20.0);
    REQUIRE(col1[1] == 21.0);
    REQUIRE(col1[2] == 0.0); // default 0
}

TEST_CASE("aeds_type2 operator<<(std::ostream&)", "[aeds_type2][new]") {
    aeds_type2 arr(2, 2);
    arr(0,0) = 1.1;
    arr(1,0) = 2.2;
    arr(0,1) = 3.3;
    arr(1,1) = 4.4;
    
    // Print:
    // Typically prints row i across columns j => "1.1 3.3" in first line, "2.2 4.4" in second line
    std::stringstream ss;
    ss << arr;
    // Verify the stream is non-empty and has our numbers
    REQUIRE_FALSE(ss.str().empty());
    REQUIRE(ss.str().find("1.1") != std::string::npos);
    REQUIRE(ss.str().find("4.4") != std::string::npos);
    // The lines are typically row-based
}

/* Pins: a column of an aeds_type2 is a view of its values, and an
 * assignment to it writes through: aeds[c] = a and aeds.col(c) = a * 2.0
 * set aeds(r, c) for every r, and aeds[c].data() stays &aeds(0, c); a
 * vector of another size is refused (std::invalid_argument) and changes
 * nothing.
 * Before the fix: the column took a buffer of its own, so aeds(r, c) kept
 * 0, and a vector of another size replaced the column. */
TEST_CASE("aeds_type2: an assignment to a column writes into the aeds_type2",
          "[aeds_type2][column_view]") {
  aeds_type2 aeds(3, 2);
  const aed_type a({1.0, 2.0, 3.0});
  aeds[1] = a;
  aeds.col(0) = a * 2.0;
  for (size_t r = 0; r < 3; ++r) {
    INFO("row " << r);
    CHECK(aeds(r, 1) == a[r]);
    CHECK(aeds(r, 0) == 2.0 * a[r]);
  }
  CHECK(aeds[0].data() == &aeds(0, 0));
  CHECK(aeds[1].data() == &aeds(0, 1));
  CHECK_THROWS_AS(aeds[1] = aed_type(2), std::invalid_argument);
  CHECK(aeds(2, 1) == 3.0);
  CHECK(aeds[1].size() == 3);
}
