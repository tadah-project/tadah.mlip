#include "catch2/catch.hpp"
#include <tadah/core/utils.h>
#include <tadah/core/core_types.h>
#include <iomanip>

using namespace tadah::core;

TEST_CASE( "Testing geomspace() 1", "[utils]" ) {
    REQUIRE_THROWS(geomspace(0,2,10,10));
}
TEST_CASE( "Testing geomspace() 2", "[utils]" ) {
    v_type v = geomspace(1,10,4, 10);
    REQUIRE_THAT( v[0], Catch::Matchers::WithinAbs(1 ,1e-12) );
    REQUIRE_THAT( v[1], Catch::Matchers::WithinAbs(5.8550197495911 ,1e-12) );
    REQUIRE_THAT( v[2], Catch::Matchers::WithinAbs(34.2812562681019 ,1e-12) );
    REQUIRE_THAT( v[3], Catch::Matchers::WithinAbs(200.71743249053 ,1e-12) );
}
TEST_CASE( "Testing geomspace() 3", "[utils]" ) {
    v_type v = geomspace(1,4,4, log(10));
    REQUIRE_THAT( v[0], Catch::Matchers::WithinAbs(1 ,1e-12) );
    REQUIRE_THAT( v[1], Catch::Matchers::WithinAbs(1.4702093789085, 1e-12) );
    REQUIRE_THAT( v[2], Catch::Matchers::WithinAbs(2.1615156178305, 1e-12) );
    REQUIRE_THAT( v[3], Catch::Matchers::WithinAbs(3.1778805339916, 1e-12) );
}
TEST_CASE( "Testing number_of_digits()", "[utils]" ) {
    REQUIRE( number_of_digits(123456789)==9);
    REQUIRE( number_of_digits(9)==1);
    REQUIRE( number_of_digits(0)==1);
    REQUIRE( number_of_digits(10)==2);
}
TEST_CASE( "Testing max_number()", "[utils]" ) {
    REQUIRE( max_number(0)==0);
    REQUIRE( max_number(1)==9);
    REQUIRE( max_number(2)==99);
    REQUIRE( max_number(9)==999999999);
}
TEST_CASE( "Testing vec_tp_string()", "[utils]" ) {
    v_type v = {1.12, 1.13, 1.145};
    REQUIRE_THAT(vec_to_string(v), Catch::Matchers::Equals("1.12 1.13 1.145"));
    REQUIRE_THAT(vec_to_string(v,","), Catch::Matchers::Equals("1.12,1.13,1.145"));
}
TEST_CASE( "Testing vec_to_upper()", "[utils]" ) {
    REQUIRE_THAT(to_upper("this is test"), Catch::Matchers::Equals("THIS IS TEST"));
    //REQUIRE_THAT(to_upper("This IS tEst"), Catch::Matchers::Equals("THIS IS TEST"));
}
TEST_CASE( "Testing operator<<(vec<T>)", "[utils]" ) {
    v_type v = {1.12, 1.13, 1.145};
    std::stringstream ss;
    ss << v;
    REQUIRE_THAT(ss.str(), Catch::Matchers::Equals("1.12 1.13 1.145 "));
}
TEST_CASE( "Testing linspace", "[utils]" ) {
    v_type v = linspace(0, 10, 11);
    REQUIRE(v[0]==0);
    REQUIRE(v[1]==1);
    REQUIRE(v[9]==9);
    REQUIRE(v[10]==10);
    REQUIRE(linspace(0, 10, 0).size()==0);
    REQUIRE(linspace(1.1, 3.0, 1.0).size()==1);
    REQUIRE(linspace(1.1, 3.0, 1.0)[0]==1.1);
}
TEST_CASE( "Testing to_string<T>(v,n)", "[utils]" ) {
    REQUIRE_THAT(to_string(1.123456789,2), Catch::Matchers::Equals("1.12"));
    REQUIRE_THAT(to_string(1.123456789,4), Catch::Matchers::Equals("1.1235"));
    REQUIRE_THAT(to_string(1.123456789,8), Catch::Matchers::Equals("1.12345679"));
    REQUIRE_THAT(to_string(1.123456789,9), Catch::Matchers::Equals("1.123456789"));
}
TEST_CASE( "Testing to_string<T>(v)", "[utils]" ) {
    REQUIRE_THAT(to_string(1.123456789), Catch::Matchers::Equals("1.12346"));
}

double f(double x) {
    return 3*x*x;
}

TEST_CASE( "Testing central_difference", "[utils]" ) {
    REQUIRE_THAT(central_difference(f,0,0.01),
            Catch::Matchers::WithinAbs(0,1e-12) );
    REQUIRE_THAT(central_difference(f,1,0.01),
            Catch::Matchers::WithinAbs(6,1e-12) );
    REQUIRE_THAT(central_difference(f,-1,0.01),
            Catch::Matchers::WithinAbs(-6 ,1e-12) );
}
TEST_CASE( "Testing random", "[utils]" ) {
    double d1 = random(0.0, 1.0);
    double d2 = random(0.0, 1.0);
    REQUIRE_THAT(d1, !Catch::Matchers::WithinAbs(d2 ,1e-12));
}
TEST_CASE( "Testing get_index", "[utils]" ) {
    v_type v = {1.12, 1.13, 1.55};
    REQUIRE(get_index(v, 1.12)==0);
    REQUIRE(get_index(v, 1.13)==1);
    REQUIRE(get_index(v, 1.55)==2);
}

TEST_CASE("Testing is_blank_line()", "[utils][new]") {
    REQUIRE(is_blank_line("") == true);
    REQUIRE(is_blank_line("   ") == true);
    REQUIRE(is_blank_line("\t\n") == true);
    REQUIRE(is_blank_line("  a  ") == false);
}

TEST_CASE("Testing is_integer(double) and is_integer(const std::string&)", "[utils][new]") {
    // double overload
    REQUIRE(is_integer(4.0) == true);
    REQUIRE(is_integer(4.5) == false);

    // string overload
    REQUIRE(is_integer("123456") == true);
    REQUIRE(is_integer("abc123") == false);
    REQUIRE(is_integer("") == false);
    REQUIRE(is_integer("0001") == true); // leading zeros
}

TEST_CASE("Testing is_number(const std::string&)", "[utils][new]") {
    REQUIRE(is_number("123") == true);
    REQUIRE(is_number("123.456") == true);
    REQUIRE(is_number("-42") == true);      // Negative sign in front
    REQUIRE(is_number("3.14e-2") == true);
    REQUIRE(is_number("abc") == false);
    REQUIRE(is_number("") == false);
    // std::stod acceptance is kept (leading whitespace, inf/nan, hex) ...
    REQUIRE(is_number(" 1.5") == true);
    REQUIRE(is_number("inf") == true);
    REQUIRE(is_number("nan") == true);
    REQUIRE(is_number("0x1p3") == true);
    REQUIRE(is_number("1.5 ") == false);    // whole token
    REQUIRE(is_number("1e400") == false);   // overflow
    // ... but a subnormal is a number (the lossless writers emit them).
    REQUIRE(is_number("1e-310") == true);
    REQUIRE(is_number("-4.9406564584124654e-324") == true);
}

TEST_CASE("Testing trim(const std::string&)", "[utils][new]") {
    REQUIRE(trim("  hello  ") == "hello");
    REQUIRE(trim("\t\t line \n") == "line");
    REQUIRE(trim("") == "");
    REQUIRE(trim("  ") == "");
}

TEST_CASE("Testing join(const std::vector<std::string>&)", "[utils][new]") {
    std::vector<std::string> words = {"hello", "world"};
    REQUIRE(join(words) == "hello world");

    std::vector<std::string> single = {"one"};
    REQUIRE(join(single) == "one");

    std::vector<std::string> empty = {};
    REQUIRE(join(empty) == "");
}

TEST_CASE("Testing divide_into_parts(size_t, size_t)", "[utils][new]") {
    // Perfectly divisible
    auto parts1 = divide_into_parts(10, 5);
    REQUIRE(parts1.size() == 5);
    REQUIRE(parts1[0] == 2);
    REQUIRE(parts1[4] == 2);

    // Not perfectly divisible
    auto parts2 = divide_into_parts(11, 5);
    REQUIRE(parts2.size() == 5);
    // Sum must be 11
    size_t sum2 = 0; 
    for (auto& p : parts2) sum2 += p;
    REQUIRE(sum2 == 11);

    // Edge cases
    auto parts3 = divide_into_parts(0, 5);
    REQUIRE(parts3.size() == 5);
    for (auto p : parts3) {
        REQUIRE(p == 0);
    }
}

TEST_CASE("Testing divide_by_percentage(size_t, const std::vector<size_t>&)", "[utils][new]") {
    // 100% partition
    std::vector<size_t> perc1 = {50, 50};
    auto parts1 = divide_by_percentage(100, perc1);
    REQUIRE(parts1.size() == 2);
    REQUIRE(parts1[0] == 50);
    REQUIRE(parts1[1] == 50);

    // Summation mismatch scenario
    std::vector<size_t> perc2 = {33, 33, 34}; // sum=100
    auto parts2 = divide_by_percentage(99, perc2);
    REQUIRE(parts2.size() == 3);
    // Must total back to 99
    size_t total2 = parts2[0] + parts2[1] + parts2[2];
    REQUIRE(total2 == 99);

    // Zero distribution
    std::vector<size_t> perc3 = {0, 100};
    auto parts3 = divide_by_percentage(50, perc3);
    REQUIRE(parts3.size() == 2);
    REQUIRE(parts3[0] == 0);
    REQUIRE(parts3[1] == 50);
}

/* Pins: the strict index-list parsers on the inputs this case gave the
 * lenient core::parse_indices, removed once FIXINDEX had its own reader
 * (nothing called it any more): parse_indices_strict gives "0, 2, 5" and
 * "2-4, 6-10:2" in order, and refuses a repeat ("1,1,1-3") and a malformed
 * segment ("abc, 1-2:xyz"); parse_one_based_indices removes the repeats of
 * "1,1,1-3" without a word.
 * Before the fix: passes as well (a pin of the strict parsers). The case
 * tested parse_indices, which removed the repeats with "Warning: Duplicate
 * indices found and removed." on stderr and dropped both segments of
 * "abc, 1-2:xyz" with an "Error:" line, returning nothing. */
TEST_CASE("Testing parse_indices_strict(const std::string&)", "[utils][new]") {
    CHECK(parse_indices_strict("0, 2, 5") == std::vector<size_t>{0, 2, 5});
    CHECK(parse_indices_strict("2-4, 6-10:2") ==
          std::vector<size_t>{2, 3, 4, 6, 8, 10});
    CHECK_THROWS_WITH(parse_indices_strict("1,1,1-3"),
                      "Duplicate index in '1,1,1-3'");
    CHECK_THROWS_WITH(parse_indices_strict("abc, 1-2:xyz"),
                      "Invalid index number 'abc'");
    CHECK(parse_one_based_indices({"1,1,1-3"}, 3, "t", "3 items") ==
          std::vector<size_t>{1, 2, 3});
}

TEST_CASE("Testing generate_2d_histogram()", "[utils][new]") {
    // Simple uniform distribution
    std::vector<std::pair<double,double>> data = {
        {1.0, 1.0}, {2.0, 2.0}, {2.1, 1.9}, {3.0, 3.0},
        {3.0, 2.8}, {3.5, 2.9}, {1.2, 1.1}
    };
    auto hist = generate_2d_histogram(data, 2, 2);
    // hist is a flattened bin structure: 2 bins along x * 2 bins along y = 4 bins total
    REQUIRE(hist.size() == 4);

    // Check if total counts match the data size
    size_t total_count = 0;
    for (auto& bin : hist) {
        total_count += std::get<2>(bin);
    }
    REQUIRE(total_count == data.size());

    // Edge case: empty data
    auto empty_hist = generate_2d_histogram({}, 2, 2);
    REQUIRE(empty_hist.empty() == true);
}

TEST_CASE("Testing move_columns_in_place() and reverse_columns()", "[utils][new]") {
    // A 2x4 matrix, stored in col-major order for Matrix_Base.
    // col0 => [1, 3], col1 => [2, 4], col2 => [5, 7], col3 => [6, 8].
    // So m_[0] = 1, m_[1] = 3, m_[2] = 2, ...
    std::vector<double> matData = {
        1,  3,  // Column 0
        2,  4,  // Column 1
        5,  7,  // Column 2
        6,  8   // Column 3
    };

    // Construct a 2-row x 4-column matrix in col-major form.
    Matrix_Base<std::vector<double>> mat(matData, 2, 4);

    // Confirm the original order:
    //   mat(0,0) = 1,  mat(1,0) = 3
    //   mat(0,1) = 2,  mat(1,1) = 4
    //   mat(0,2) = 5,  mat(1,2) = 7
    //   mat(0,3) = 6,  mat(1,3) = 8
    REQUIRE(mat(0,0) == 1);
    REQUIRE(mat(1,0) == 3);
    REQUIRE(mat(0,1) == 2);
    REQUIRE(mat(1,1) == 4);
    REQUIRE(mat(0,2) == 5);
    REQUIRE(mat(1,2) == 7);
    REQUIRE(mat(0,3) == 6);
    REQUIRE(mat(1,3) == 8);

    // Move columns {1, 3} to the right in-place
    // This should reorder columns as [0,2,1,3].
    std::vector<size_t> col_map = move_columns_in_place(mat, {1, 3});

    // After reordering:
    //   new col0 => original col0 -> [1, 3]
    //   new col1 => original col2 -> [5, 7]
    //   new col2 => original col1 -> [2, 4]
    //   new col3 => original col3 -> [6, 8]
    REQUIRE(mat(0,0) == 1); // still from original col0
    REQUIRE(mat(1,0) == 3);
    REQUIRE(mat(0,1) == 5); // from original col2
    REQUIRE(mat(1,1) == 7);
    REQUIRE(mat(0,2) == 2); // from original col1
    REQUIRE(mat(1,2) == 4);
    REQUIRE(mat(0,3) == 6); // from original col3
    REQUIRE(mat(1,3) == 8);

    // Reverse columns to restore original order
    reverse_columns(mat, col_map);

    // Check that all columns are back to the original layout
    REQUIRE(mat(0,0) == 1);
    REQUIRE(mat(1,0) == 3);
    REQUIRE(mat(0,1) == 2);
    REQUIRE(mat(1,1) == 4);
    REQUIRE(mat(0,2) == 5);
    REQUIRE(mat(1,2) == 7);
    REQUIRE(mat(0,3) == 6);
    REQUIRE(mat(1,3) == 8);
}

TEST_CASE("Testing reverse_vector()", "[utils][new]") {
    t_type originalVec = {10, 20, 30, 40};
    // Suppose we rearranged it to {30, 10, 40, 20},
    // and the "original_map" tells us how to revert to original.
    // If index 0 -> index 1, index 1 -> index 3, index 2 -> index 0, index 3 -> index 2, etc.
    // A plausible mapping:
    t_type rearranged = {30, 10, 40, 20};
    std::vector<size_t> map = {2, 0, 3, 1}; 
    // means rearranged[0] should go to originalVec[2], rearranged[1] -> originalVec[0], etc.
    // reverse_vector re-orders "rearranged" back.
    
    reverse_vector(rearranged, map);

    // Check we get back the original
    REQUIRE(rearranged.size() == originalVec.size());
    for (size_t i = 0; i < rearranged.size(); ++i) {
        REQUIRE(rearranged[i] == originalVec[i]);
    }
}
// 1) Testing geomspace with stop < start and base < 1, etc.
TEST_CASE("Testing geomspace() - stop < start and base < 1", "[utils][improvements]") {
    // Expect a possibly incorrect or unexpected result if we do not handle "stop < start" in geomspace.
    // If the underlying implementation doesn't handle it, it may not throw. The test only confirms it doesn't crash.
    // The user can decide if a throw or normal return is desired.
    REQUIRE_NOTHROW(geomspace(10, 1, 5, 10)); // Larger start, smaller stop

    // Test with base < 1
    // geomspace might produce decreasing values. No explicit error is in the code, so we only check for exceptions.
    REQUIRE_NOTHROW(geomspace(1, 10, 5, 0.5));

    // Check negative start as well
    // The function uses log(start) internally; negative start can lead to domain error in std::log
    // This might throw a runtime_error or produce NaN. We can see if it throws.
    REQUIRE_THROWS(geomspace(-1, 10, 5, 10));
}

// 2) Testing linspace with reversed range
TEST_CASE("Testing linspace() - reversed range", "[utils][improvements]") {
    auto seq = linspace(5, -5, 11);
    // We expect 11 points from +5 to -5 in steps of -1.0    
    REQUIRE(seq.size() == 11);
    REQUIRE_THAT(seq[0],  Catch::Matchers::WithinAbs(5.0, 1e-12));
    REQUIRE_THAT(seq[5],  Catch::Matchers::WithinAbs(0.0, 1e-12));
    REQUIRE_THAT(seq[10], Catch::Matchers::WithinAbs(-5.0, 1e-12));
}

// 3) Testing random() distribution handle
TEST_CASE("Testing random() in a loop", "[utils][improvements]") {
    // Make sure all values are within [5.0, 10.0]
    bool inRange = true;
    for (int i = 0; i < 100; ++i) {
        double val = random(5.0, 10.0);
        if (val < 5.0 || val > 10.0) {
            inRange = false;
            break;
        }
    }
    REQUIRE(inRange == true);
}

// 4) Testing parse_indices_strict() with step patterns like "2-10:3"
/* Pins: parse_indices_strict expands "2-10:3" to 2, 5, 8 and "0-15:5" to 0,
 * 5, 10, 15 (as the removed lenient parse_indices did).
 * Before the fix: passes as well (a pin; the case tested parse_indices). */
TEST_CASE("Testing parse_indices_strict() - advanced step patterns", "[utils][improvements]") {
    CHECK(parse_indices_strict("2-10:3") == std::vector<size_t>{2, 5, 8});
    CHECK(parse_indices_strict("0-15:5") == std::vector<size_t>{0, 5, 10, 15});
}

// 1) Further testing the strict index parsers on edge cases
/* Pins: on the edge cases this case gave the removed lenient parse_indices,
 * parse_indices_strict refuses an empty list, a list of empty segments, a
 * malformed range start or end and a range without its end ("5-"), and a
 * repeat ("2,2,2-4"); parse_one_based_indices refuses the empty list and
 * "5-" with its messages and removes the repeats of "2,2,2-4".
 * Before the fix: passes as well (a pin of the strict parsers). The lenient
 * parse_indices returned nothing for "", " ,  , ", "abc-5" and "10-abc"
 * (with an "Error:" line for the last two), took "5-" as 5 and removed the
 * repeats of "2,2,2-4" with a stderr warning. */
TEST_CASE("Strict index parsers with edge cases", "[utils][missing_tests]") {
    using S = std::vector<std::string>;
    CHECK_THROWS_WITH(parse_indices_strict(""), "Invalid index number ''");
    CHECK_THROWS_WITH(parse_indices_strict(" ,  , "), "Invalid index number ''");
    CHECK_THROWS_WITH(parse_indices_strict("abc-5"), "Invalid index number 'abc'");
    CHECK_THROWS_WITH(parse_indices_strict("10-abc"), "Invalid index number 'abc'");
    CHECK_THROWS_WITH(parse_indices_strict("5-"), "Invalid index number ''");
    CHECK_THROWS_WITH(parse_indices_strict("2,2,2-4"), "Duplicate index in '2,2,2-4'");
    const std::string usage = " (use i, a-b or a-b:step, 1-based, separated by commas)";
    CHECK_THROWS_WITH(parse_one_based_indices(S{""}, 10, "t", "10 items"),
                      "t: the index list '' has an empty entry" + usage);
    CHECK_THROWS_WITH(parse_one_based_indices(S{"5-"}, 10, "t", "10 items"),
                      "t: index '5-' is not an index range" + usage);
    CHECK(parse_one_based_indices(S{"2,2,2-4"}, 10, "t", "10 items") ==
          std::vector<size_t>{2, 3, 4});
}

// 2) Additional test for generate_2d_histogram() with invalid range & empty data
TEST_CASE("generate_2d_histogram() invalid min/max and empty data", "[utils][missing_tests]") {
    SECTION("Empty data -> returns empty histogram") {
        auto hist = generate_2d_histogram({}, 5, 5);
        REQUIRE(hist.empty());
    }
    SECTION("x_min >= x_max or y_min >= y_max -> returns empty") {
        // Data that leads to same min==max
        std::vector<std::pair<double,double>> data = {
            {2.0, 2.0}, {2.0, 2.0}, {2.0, 2.0}
        };
        auto hist2 = generate_2d_histogram(data, 2, 2);
        REQUIRE(hist2.empty());
    }
}

// 3) Testing geomspace() with additional invalid or edge scenarios
TEST_CASE("geomspace() with invalid/edge scenarios", "[utils][missing_tests]") {
    SECTION("start <= 0 => throws") {
        // Already tested for start==0. A negative start throws as well
        REQUIRE_THROWS_WITH(
            geomspace(-1, 10, 5, 10),
            Catch::Matchers::Contains("geomspace() start must be > 0")
        );
    }
    SECTION("num=1 => single value") {
        // If num==1, the function might produce exactly one point at 'start'
        // Just confirm it doesn’t crash and yields a single-element vector
        auto result = geomspace(1.0, 100.0, 1, 10.0);
        REQUIRE(result.size() == 1);
        REQUIRE(result[0] == Approx(1.0));
    }
    SECTION("stop < start => might produce decreasing exponent range") {
        // The function does not throw; it just yields something.
        // The test confirms it doesn't crash
        REQUIRE_NOTHROW(geomspace(10, 1, 5, 10));
    }
}

// 4) Additional tests for random() extremes
TEST_CASE("random() with extreme bounds", "[utils][missing_tests]") {
    SECTION("range_from > range_to => distribution might invert or throw?") {
        // The code does not explicitly handle if range_from > range_to.
        // Typically uniform_real_distribution will allow from>to and produce invalid results.
        // Usually it doesn't throw; the REQUIRE_NOTHROW check is just for
        // coverage.
        REQUIRE_NOTHROW(random(10.0, -5.0));
    }
    SECTION("range_from == range_to => always returns that value") {
        double val = random(5.0, 5.0);
        REQUIRE(val == 5.0);
    }
}
TEST_CASE("Testing divide_into_parts() - new coverage", "[utils]") {
    SECTION("Throws if n <= 0") {
        REQUIRE_THROWS_AS(divide_into_parts(10, 0), std::runtime_error);
        REQUIRE_THROWS_AS(divide_into_parts(10, -5), std::runtime_error);
    }

    SECTION("Perfectly divisible") {
        auto parts = divide_into_parts(10, 5);
        // Expect [2,2,2,2,2]
        REQUIRE(parts.size() == 5);
        REQUIRE(std::accumulate(parts.begin(), parts.end(), 0UL) == 10);
        for (auto p : parts) {
            REQUIRE(p == 2);
        }
    }

    SECTION("Not perfectly divisible") {
        auto parts = divide_into_parts(10, 3);
        // Expect [4,3,3] => sums to 10
        REQUIRE(parts.size() == 3);
        REQUIRE(std::accumulate(parts.begin(), parts.end(), 0UL) == 10);
        REQUIRE(parts[0] == 4);
        REQUIRE(parts[1] == 3);
        REQUIRE(parts[2] == 3);
    }

    SECTION("number = 0") {
        auto parts = divide_into_parts(0, 5);
        // 0/5 => each part = 0, remainder=0 => [0,0,0,0,0]
        REQUIRE(parts.size() == 5);
        REQUIRE(std::accumulate(parts.begin(), parts.end(), 0UL) == 0);
    }

    SECTION("Large remainder scenario") {
        // e.g. number=11, n=4 => remainder=3 => initial [2,2,2,2], 
        // then add 1 to first 3 => [3,3,3,2]
        auto parts = divide_into_parts(11, 4);
        REQUIRE(parts.size() == 4);
        REQUIRE(std::accumulate(parts.begin(), parts.end(), 0UL) == 11);
        // optional checks
        REQUIRE(parts[0] == 3);
        REQUIRE(parts[1] == 3);
        REQUIRE(parts[2] == 3);
        REQUIRE(parts[3] == 2);
    }
}
