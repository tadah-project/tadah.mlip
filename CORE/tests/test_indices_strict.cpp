#include "catch2/catch.hpp"
#include <tadah/core/utils.h>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

TEST_CASE("Strict indices expand lists ranges and strides", "[utils][pc_dataset]") {
    CHECK(tadah::core::parse_indices_strict("1, 3-7:2, 9-10")
          == std::vector<std::size_t>{1,3,5,7,9,10});
    CHECK(tadah::core::parse_indices_strict("3, 1, 0") == std::vector<std::size_t>{3,1,0});
    const auto max = std::numeric_limits<std::size_t>::max();
    const auto s = std::to_string(max);
    CHECK(tadah::core::parse_indices_strict(s + "-" + s) == std::vector<std::size_t>{max});
}

TEST_CASE("Strict indices reject partial input and unsafe expansion", "[utils][pc_dataset]") {
    for (const auto* input : {"", " ", ",", "1,", ",1", "1,,2", "x,1", "1x", "-1",
                             "1-", "3-1", "1-3:0", "1-3:", "1-3:x", "1:2", "1-2-3",
                             "1-3:2:1", "1,1", "1-3,2", "184467440737095516160"}) {
        INFO(input);
        CHECK_THROWS(tadah::core::parse_indices_strict(input));
    }
}

/* Pins: parse_dataset_indices, the 1-based index list of the dataset
 * commands (tadah data sample and data balance -i), on a dataset of 10:
 *  - the values form one list, as the command line gives them: '1, 3'
 *    arrives as the values '1,' and '3'; spaces around '-' and
 *    ':' and between indices are allowed; repeated indices are removed;
 *  - an empty segment, a descending range, a step of 0, a token that is not
 *    an index, index 0 and an index above 10 are refused, naming the
 *    offending text, and a range is checked against the bounds before it is
 *    expanded (1-1000000000 is refused at once). */
TEST_CASE("parse_dataset_indices: one 1-based list, checked before expansion",
          "[utils][indices]") {
    using tadah::core::parse_dataset_indices;
    using V = std::vector<std::size_t>;
    using S = std::vector<std::string>;
    CHECK(parse_dataset_indices(S{"1,", "3"}, 10, "t") == V{1, 3});
    CHECK(parse_dataset_indices(S{"1, 3"}, 10, "t") == V{1, 3});
    CHECK(parse_dataset_indices(S{"1 - 3"}, 10, "t") == V{1, 2, 3});
    CHECK(parse_dataset_indices(S{"1-5 : 2"}, 10, "t") == V{1, 3, 5});
    CHECK(parse_dataset_indices(S{"1,3", "5"}, 10, "t") == V{1, 3, 5});
    CHECK(parse_dataset_indices(S{"1,3 5"}, 10, "t") == V{1, 3, 5});
    CHECK(parse_dataset_indices(S{"1,1,2"}, 10, "t") == V{1, 2});
    CHECK(parse_dataset_indices(S{"10", "1-10:9"}, 10, "t") == V{10, 1});
    const std::vector<std::pair<S, std::string>> bad = {
        {S{"1,,2"}, "1,,2"}, {S{",1"}, ",1"}, {S{"1,"}, "1,"}, {S{"5-3"}, "'5-3'"},
        {S{"1-3:0"}, "'1-3:0'"}, {S{"x"}, "'x'"}, {S{"0"}, "index 0"},
        {S{"11"}, "index 11"}, {S{"1-1000000000"}, "index 1000000000"},
        {S{"184467440737095516160"}, "'184467440737095516160'"}};
    for (const auto &b : bad) {
        INFO(b.first[0]);
        try {
            (void)parse_dataset_indices(b.first, 10, "tadah data sample");
            FAIL("no exception");
        } catch (const std::runtime_error &e) {
            CHECK_THAT(e.what(), Catch::Contains(b.second));
            CHECK_THAT(e.what(), Catch::Contains("tadah data sample"));
        }
    }
}

/* Pins: parse_one_based_indices(values, total, who, counted), the strict
 * 1-based index list of the commands that select structures or basis
 * functions (tadah data print, analysis descriptor, analysis bfunc -i):
 *  - the grammar of parse_dataset_indices: several values form one list
 *    ('1 3' and the values '1', '3'), spaces around '-' and ':', repeated
 *    indices removed, the given order kept;
 *  - an index outside 1 ... total is refused naming the command, the index,
 *    its entry and `counted`, what the indices count:
 *    "tadah analysis bfunc: index 0 is outside 1-4 (the D2_Blip basis has 4
 *    functions)", "... index 9 (in '2-9') is outside 1-4 (...)";
 *  - a descending range and a step of 0 are refused naming the entry; a
 *    range is checked against the bounds before it is expanded
 *    (1-10000000000 at once).
 * Before the fix: does not compile (there was no parse_one_based_indices;
 * the commands used the lenient parse_indices). */
TEST_CASE("parse_one_based_indices: the bound message names what the indices count",
          "[utils][indices]") {
    using tadah::core::parse_one_based_indices;
    using V = std::vector<std::size_t>;
    using S = std::vector<std::string>;
    const std::string who = "tadah analysis bfunc";
    const std::string counted = "the D2_Blip basis has 4 functions";
    CHECK(parse_one_based_indices(S{"1 3"}, 4, who, counted) == V{1, 3});
    CHECK(parse_one_based_indices(S{"1", "3"}, 4, who, counted) == V{1, 3});
    CHECK(parse_one_based_indices(S{"3,1-2"}, 4, who, counted) == V{3, 1, 2});
    CHECK(parse_one_based_indices(S{"1 - 4 : 2", "4"}, 4, who, counted) == V{1, 3, 4});
    CHECK(parse_one_based_indices(S{"2,2"}, 4, who, counted) == V{2});
    const std::vector<std::pair<S, std::string>> bad = {
        {S{"0"}, "tadah analysis bfunc: index 0 is outside 1-4 (the D2_Blip basis has 4 "
                 "functions)"},
        {S{"2-9"}, "tadah analysis bfunc: index 9 (in '2-9') is outside 1-4 (the D2_Blip "
                   "basis has 4 functions)"},
        {S{"1-10000000000"}, "tadah analysis bfunc: index 10000000000 (in '1-10000000000') "
                             "is outside 1-4 (the D2_Blip basis has 4 functions)"},
        {S{"5-2"}, "tadah analysis bfunc: index '5-2' is a descending range (use i, a-b or "
                   "a-b:step, 1-based, separated by commas)"},
        {S{"1-3:0"}, "tadah analysis bfunc: index '1-3:0' has a step of 0 (use i, a-b or "
                     "a-b:step, 1-based, separated by commas)"}};
    for (const auto &b : bad) {
        INFO(b.first[0]);
        try {
            (void)parse_one_based_indices(b.first, 4, who, counted);
            FAIL("no exception");
        } catch (const std::runtime_error &e) {
            CHECK(std::string(e.what()) == b.second);
        }
    }
}

/* Pins: parse_dataset_indices (tadah data sample, balance and write -i)
 * gives exactly the messages it gave before it was written with
 * parse_one_based_indices: on a dataset of 10, for '11', '1-1000000000',
 * '0', '5-3', '1-3:0', 'x' and '1,,2'.
 * Before the fix: passes as well (a pin of the messages the data sample,
 * balance and write tests match). */
TEST_CASE("parse_dataset_indices: its messages are unchanged", "[utils][indices]") {
    using S = std::vector<std::string>;
    const std::string usage = " (use i, a-b or a-b:step, 1-based, separated by commas)";
    const std::vector<std::pair<S, std::string>> bad = {
        {S{"11"}, "tadah data sample: index 11 is outside 1-10 (the dataset has 10 "
                  "structures)"},
        {S{"1-1000000000"}, "tadah data sample: index 1000000000 (in '1-1000000000') is "
                            "outside 1-10 (the dataset has 10 structures)"},
        {S{"0"}, "tadah data sample: index 0 is outside 1-10 (the dataset has 10 "
                 "structures)"},
        {S{"5-3"}, "tadah data sample: index '5-3' is a descending range" + usage},
        {S{"1-3:0"}, "tadah data sample: index '1-3:0' has a step of 0" + usage},
        {S{"x"}, "tadah data sample: index 'x' is not an index" + usage},
        {S{"1,,2"}, "tadah data sample: the index list '1,,2' has an empty entry" + usage}};
    for (const auto &b : bad) {
        INFO(b.first[0]);
        try {
            (void)tadah::core::parse_dataset_indices(b.first, 10, "tadah data sample");
            FAIL("no exception");
        } catch (const std::runtime_error &e) {
            CHECK(std::string(e.what()) == b.second);
        }
    }
}
