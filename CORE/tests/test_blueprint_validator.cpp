#include "catch2/catch.hpp"
#include <chrono>
#include <vector>
#include <sstream>
#include <regex>
#include <random>
#include <string>
#include <iostream> // For debug if needed

// Assume "blueprint_validator.h" provides tadah::core::BlueprintValidator,
// or adapt the path as necessary.
#include <tadah/core/blueprint_validator.h>

/**
 * @brief Helper that returns true if all provided examples pass validation
 *        for a given blueprint. This approach is used for brevity.
 */
static bool allExamplesValidate(const std::string &blueprint,
                                const std::vector<std::string> &examples) {
  tadah::core::BlueprintValidator validator(blueprint);
  for (auto &example : examples) {
    if (!validator.validate(example)) {
      return false;
    }
  }
  return true;
}

/**
 * @brief Helper that returns true if any of the provided strings pass validation.
 *        This checks negative/edge cases that are expected to fail.
 */
static bool anyStringValidates(const std::string &blueprint,
                               const std::vector<std::string> &negatives) {
  tadah::core::BlueprintValidator validator(blueprint);
  for (auto &neg : negatives) {
    if (validator.validate(neg)) {
      return true;
    }
  }
  return false;
}

TEST_CASE("TYPES.BOOL: value_type = [\"BOOL\"]", "[blueprint]") {
  // Single bool: "true" or "false"
  std::string blueprint = "BOOL";
  std::vector<std::string> validCases = {"true", "false"};
  std::vector<std::string> invalidCases = {"True", "False", "TRUE", "maybe", ""};

  SECTION("Valid examples must pass") {
    REQUIRE(allExamplesValidate(blueprint, validCases));
  }
  SECTION("Invalid examples must fail") {
    REQUIRE_FALSE(anyStringValidates(blueprint, invalidCases));
  }
}

TEST_CASE("TYPES.BOOLS: value_type = [\"{BOOL}\"]", "[blueprint]") {
  // Repeated bool tokens
  std::string blueprint = "{BOOL}";
  // Some valid sequences: "true", "false", "true true false"
  std::vector<std::string> validCases = {"true", "false", "true true false", "false true"};
  // Some invalid sequences
  std::vector<std::string> invalidCases = {"true false maybe", "maybe", "TRUE"};

  SECTION("Valid examples must pass") {
    REQUIRE(allExamplesValidate(blueprint, validCases));
  }
  SECTION("Invalid examples must fail") {
    REQUIRE_FALSE(anyStringValidates(blueprint, invalidCases));
  }
}

TEST_CASE("TYPES.INT: value_type = [\"INT\"]", "[blueprint]") {
  std::string blueprint = "INT";
  std::vector<std::string> validCases = {"20", "-10", "0", "+5"};
  std::vector<std::string> invalidCases = {"1.0", "abc", "--10","0 +42","20.5", "abc", "2 x 3", ""};

  SECTION("Valid repeated integer examples must pass") {
    REQUIRE(allExamplesValidate(blueprint, validCases));
  }
  SECTION("Invalid sequences must fail") {
    REQUIRE_FALSE(anyStringValidates(blueprint, invalidCases));
  }
}

TEST_CASE("TYPES.UINT: value_type = [\"UINT\"]", "[blueprint]") {
  std::string blueprint = "UINT";
  // "UINT" is 0 or positive integer
  std::vector<std::string> validCases = {"0", "20", "9999"};
  std::vector<std::string> invalidCases = {"-1", "+1", "abc", "", "03"};

  SECTION("Valid unsigned examples must pass") {
    REQUIRE(allExamplesValidate(blueprint, validCases));
  }
  SECTION("Invalid unsigned examples must fail") {
    REQUIRE_FALSE(anyStringValidates(blueprint, invalidCases));
  }
}

TEST_CASE("TYPES.UINTS: value_type = [\"{UINT}\"]", "[blueprint]") {
  std::string blueprint = "{UINT}";
  std::vector<std::string> validCases = {"0", "20 5 3", "10", "1 2 3 9999"};
  std::vector<std::string> invalidCases = {"-1", "abc", "5 -1"};

  SECTION("Valid repeated uint examples must pass") {
    REQUIRE(allExamplesValidate(blueprint, validCases));
  }
  SECTION("Invalid repeated uint examples must fail") {
    REQUIRE_FALSE(anyStringValidates(blueprint, invalidCases));
  }
}

TEST_CASE("TYPES.DOUBLE: value_type = [\"DOUBLE\"]", "[blueprint]") {
  std::string blueprint = "DOUBLE";
  // e.g., "2.0", "-1.0", "3.5e+2"
  std::vector<std::string> validCases = {"2.0", "-1.0", "3.14", "3.5e+2", "0", "-10"};
  std::vector<std::string> invalidCases = {"abc", ".", "--3.0", ""};

  SECTION("Valid floating examples must pass") {
    REQUIRE(allExamplesValidate(blueprint, validCases));
  }
  SECTION("Invalid floating examples must fail") {
    REQUIRE_FALSE(anyStringValidates(blueprint, invalidCases));
  }
}

TEST_CASE("TYPES.DOUBLES: value_type = [\"{DOUBLE}\"]", "[blueprint]") {
  std::string blueprint = "{DOUBLE}";
  // Repeated floating
  std::vector<std::string> validCases = {"2.0 -4.65 0.4", "-1.0", "1.0 2.0 3.0 4.0"};
  std::vector<std::string> invalidCases = {"abc", "1. 2. 3..", ""};

  SECTION("Valid repeated float examples must pass") {
    REQUIRE(allExamplesValidate(blueprint, validCases));
  }
  SECTION("Invalid repeated float examples must fail") {
    REQUIRE_FALSE(anyStringValidates(blueprint, invalidCases));
  }
}

TEST_CASE("TYPES.STRING: value_type = [\"STRING\"]", "[blueprint]") {
  std::string blueprint = "STRING";
  // Single string with at least one letter
  std::vector<std::string> validCases = {"validString", "/path/to/file", "H", "*hello", "XYZ123"};
  std::vector<std::string> invalidCases = {"", "1234", "   ", "/"};

  SECTION("Valid single string examples must pass") {
    REQUIRE(allExamplesValidate(blueprint, validCases));
  }
  SECTION("Invalid single string examples must fail") {
    REQUIRE_FALSE(anyStringValidates(blueprint, invalidCases));
  }
}

TEST_CASE("TYPES.STRINGS: value_type = [\"{STRING}\"]", "[blueprint]") {
  std::string blueprint = "{STRING}";
  // Repeated sets of valid strings
  std::vector<std::string> validCases = {
    "string1 string2",
    "path1 /path/to/file2",
    "alpha12 Beta13 gamma14",
    "a b c"
  };
  std::vector<std::string> invalidCases = {"", "123 456", "   "};

  SECTION("Valid repeated string examples must pass") {
    REQUIRE(allExamplesValidate(blueprint, validCases));
  }
  SECTION("Invalid repeated string examples must fail") {
    REQUIRE_FALSE(anyStringValidates(blueprint, invalidCases));
  }
}

TEST_CASE("TYPES.STRINGS handles long path lists", "[blueprint][long]") {
  tadah::core::BlueprintValidator validator("{STRING}");

  auto join = [](const std::vector<std::string> &tokens) {
    std::string out;
    for (std::size_t i = 0; i < tokens.size(); ++i) {
      if (i > 0) {
        out += ' ';
      }
      out += tokens[i];
    }
    return out;
  };

  std::vector<std::string> validTokens;
  validTokens.reserve(90);
  for (int i = 0; i < 90; ++i) {
    validTokens.push_back("/tmp/HS_MD_GAMMA_ONLY_" + std::to_string(i) + "/OUTCAR");
  }

  const std::string validLongLine = join(validTokens);
  REQUIRE(validLongLine.size() > 2048);
  REQUIRE(validator.validate(validLongLine));

  validTokens[45] = "123456";
  const std::string invalidLongLine = join(validTokens);
  REQUIRE(invalidLongLine.size() > 2048);
  REQUIRE_FALSE(validator.validate(invalidLongLine));
}

/**
 * @brief Test the more complicated pattern:
 *        "STRING {[INT | DOUBLE]} {STRING STRING}"
 */
TEST_CASE("TYPE2B: value_type = [\"STRING {[INT | DOUBLE]} {STRING STRING}\"]", "[blueprint]") {
  std::string blueprint = "STRING {[INT | DOUBLE]} {STRING STRING}";
  tadah::core::BlueprintValidator validator(blueprint);

  SECTION("Valid: minimal form with INT variant") {
    REQUIRE(validator.validate("D2_LJ 11 Ti Ti"));
  }
  SECTION("Valid: minimal form with DOUBLE variant") {
    REQUIRE(validator.validate("D2_LJ 3.0 Ti Ti"));
  }
  SECTION("Valid: repeated block with INT") {
    // Repeated block means it can appear multiple times: "11 12 13" is valid for the second slot
    REQUIRE(validator.validate("D2_MIE 11 12.5 Ti Nb Nb Nb"));
  }
  SECTION("Invalid: missing second {STRING STRING}") {
    REQUIRE_FALSE(validator.validate("D2_LJ 11 Ti"));
  }
  SECTION("Invalid: second block has only one string") {
    REQUIRE_FALSE(validator.validate("D2_LJ 11 12 Ti"));
  }
  SECTION("Invalid: no letter in 'STRING' slot") {
    REQUIRE_FALSE(validator.validate("777 11 12 x y")); // first token must be a valid 'STRING'
  }
}

/**
 * @brief Additional edge checks for partial matches, empties, etc.
 */
TEST_CASE("Edge Cases and Partial Matches", "[blueprint]") {
  SECTION("Empty blueprint must fail everything") {
    tadah::core::BlueprintValidator emptyValidator("");
    REQUIRE_FALSE(emptyValidator.validate("anything"));
    REQUIRE_FALSE(emptyValidator.validate(""));
  }
  SECTION("Partial match does not pass full validation") {
    std::string blueprint = "INT INT";
    tadah::core::BlueprintValidator val(blueprint);
    // "10" alone is only partially matching (needs two INT).
    // The new code might detect partial match, but .validate() should fail
    REQUIRE_FALSE(val.validate("10"));
    // Full match
    REQUIRE(val.validate("10 20"));
  }
  SECTION("getCaptures for partial match fallback") {
    // Two patterns: second one might partially match better
    std::vector<std::string> bps = {"INT INT DOUBLE", "INT DOUBLE"};
    tadah::core::BlueprintValidator multiVal(bps);

    std::string textPartial = "10 3.14";
    auto captures = multiVal.getCaptures(textPartial);
    REQUIRE(captures.size() == 2); 
    // For "INT DOUBLE": captures[0]="10", captures[1]="3.14", best partial
  }
}

//------------------------------------------------------------------------------
// Additional Tests: BOOL, BOOLS
//------------------------------------------------------------------------------

TEST_CASE("Extended TYPES.BOOL checks", "[blueprint][extra]") {
  std::string blueprint = "BOOL";
  // Already tested simple "true"/"false" in older suite.
  // Now add more invalid attempts, plus stress test with whitespace or partial matches.
  std::vector<std::string> validCases = {
    "true",
    "false"
  };
  std::vector<std::string> invalidCases = {
    "0",
    "1",
    "TRUE",
    "False",
    "maybe",
    ""
  };

  SECTION("Extended valid checks") {
    REQUIRE(allExamplesValidate(blueprint, validCases));
  }
  SECTION("Extended invalid checks") {
    REQUIRE_FALSE(anyStringValidates(blueprint, invalidCases));
  }
}

TEST_CASE("Extended TYPES.BOOLS checks", "[blueprint][extra]") {
  std::string blueprint = "{BOOL}";
  // Should allow repeated bools up to an arbitrary count, but we limit to 5 reps max in our test strings
  std::vector<std::string> validCases = {
    "true", 
    "false", 
    "true false true",
    "false true false true",
    "true true true true true"
  };
  std::vector<std::string> invalidCases = {
    "true  false   maybe", 
    "maybe", 
    "TRUE false", 
    "" 
  };

  SECTION("Extended valid checks") {
    REQUIRE(allExamplesValidate(blueprint, validCases));
  }
  SECTION("Extended invalid checks") {
    REQUIRE_FALSE(anyStringValidates(blueprint, invalidCases));
  }
}

//------------------------------------------------------------------------------
// Additional Tests: INT, INTS
//------------------------------------------------------------------------------

TEST_CASE("Extended TYPES.INT checks", "[blueprint][extra]") {
  std::string blueprint = "INT";
  std::vector<std::string> validCases = {
    "0",
    "-0",    // same as "0" in many contexts
    "+42",
    "12345",
    "-9999"
  };
  // We expand on possible invalid ones:
  std::vector<std::string> invalidCases = {
    "+",         // sign only
    "42.0",      // decimal
    "--10",      // double negatives
    "abc",
    "",
    "+-10"
  };

  SECTION("Extended valid INT checks") {
    REQUIRE(allExamplesValidate(blueprint, validCases));
  }
  SECTION("Extended invalid INT checks") {
    REQUIRE_FALSE(anyStringValidates(blueprint, invalidCases));
  }
}

TEST_CASE("Extended TYPES.INTS checks", "[blueprint][extra]") {
  std::string blueprint = "{INT}";
  // Up to 5 reps
  std::vector<std::string> validCases = {
    "10",
    "0 1 2 3 4",
    "-1 +2 -3 +4 -5",
    "999 123 0 -999 +20",
    "+42 +43 +44 +45 +46" // 5 repeated
  };
  std::vector<std::string> invalidCases = {
    "10.0",     // decimal
    "--5 6",    // invalid sign
    "1 x 3",    // letter
    ""          // empty
  };

  SECTION("Extended valid repeated INT checks") {
    REQUIRE(allExamplesValidate(blueprint, validCases));
  }
  SECTION("Extended invalid repeated INT checks") {
    REQUIRE_FALSE(anyStringValidates(blueprint, invalidCases));
  }
}

//------------------------------------------------------------------------------
// Additional Tests: UINT, UINTS
//------------------------------------------------------------------------------

TEST_CASE("Extended TYPES.UINT checks", "[blueprint][extra]") {
  std::string blueprint = "UINT";
  std::vector<std::string> validCases = {
    "0",
    "1",
    "999999",
    "1234"
  };
  std::vector<std::string> invalidCases = {
    "-1",
    "+1",   // sign disallowed
    "01",   // leading zero is suspicious, but your pattern "(?:0|[1-9]\\d*)" allows "0" or "nonzeroDigit plus any digits". So "01" is invalid in the blueprint.
    "abc",
    ""
  };

  SECTION("Valid unsigned tests") {
    REQUIRE(allExamplesValidate(blueprint, validCases));
  }
  SECTION("Invalid unsigned tests") {
    REQUIRE_FALSE(anyStringValidates(blueprint, invalidCases));
  }
}

TEST_CASE("Extended TYPES.UINTS checks", "[blueprint][extra]") {
  std::string blueprint = "{UINT}";
  // Up to 5 reps
  std::vector<std::string> validCases = {
    "0",
    "7 20 999 1234 19999",
    "1 2 3 4 5",
    "10"
  };
  std::vector<std::string> invalidCases = {
    "-1 2", 
    "01 02",   // each token invalid 
    "5 6 07",  // "07" invalid  
    ""
  };

  SECTION("Valid repeated unsigned tests") {
    REQUIRE(allExamplesValidate(blueprint, validCases));
  }
  SECTION("Invalid repeated unsigned tests") {
    REQUIRE_FALSE(anyStringValidates(blueprint, invalidCases));
  }
}

//------------------------------------------------------------------------------
// Additional Tests: DOUBLE, DOUBLES
//------------------------------------------------------------------------------

TEST_CASE("Extended TYPES.DOUBLE checks", "[blueprint][extra]") {
  std::string blueprint = "DOUBLE";
  std::vector<std::string> validCases = {
    "0",
    "3.14",
    "-2.0",
    "2.0e+1",
    "+2.0e-3",
    "5e7",
    "-9.9999e20"
  };
  std::vector<std::string> invalidCases = {
    ".",         // no digits
    "2.",        // might be valid for some double formats, but your pattern requires at least one digit after decimal
    "e3",        // missing leading digits
    " --2.0",    // double negative sign
    "abc", 
    ""
  };

  SECTION("Extended valid float checks") {
    REQUIRE(allExamplesValidate(blueprint, validCases));
  }
  SECTION("Extended invalid float checks") {
    REQUIRE_FALSE(anyStringValidates(blueprint, invalidCases));
  }
}

TEST_CASE("Extended TYPES.DOUBLES checks", "[blueprint][extra]") {
  std::string blueprint = "{DOUBLE}";
  // Up to 5 reps
  std::vector<std::string> validCases = {
    "2.0",
    "2.0 -4.65 0.4 3.5e2 -1e-5",
    "1 2 3 4 5",        // all valid doubles because ints also match double pattern
    "0.0 -1.0 +2.0"
  };
  std::vector<std::string> invalidCases = {
    ".",          // no leading digit
    "1. 2. 3..",  // malformed
    "abc", 
    "", 
    "2.0 .5"      // pattern doesn't allow ".5" alone; needs leading digit
  };

  SECTION("Extended valid repeated double checks") {
    REQUIRE(allExamplesValidate(blueprint, validCases));
  }
  SECTION("Extended invalid repeated double checks") {
    REQUIRE_FALSE(anyStringValidates(blueprint, invalidCases));
  }
}

//------------------------------------------------------------------------------
// Additional Tests: STRING, STRINGS
//------------------------------------------------------------------------------

TEST_CASE("Extended TYPES.STRING checks", "[blueprint][extra]") {
  // Each token must contain at least one letter or '*'
  std::string blueprint = "STRING";
  std::vector<std::string> validCases = {
    "H",       // single char
    "*",       // single star
    "Hello", 
    "D2_LJ", 
    "abc123", 
    "some_string*ext", 
    "XYZ" 
  };
  std::vector<std::string> invalidCases = {
    "", 
    "12345",        // does not contain letter or star
    "     ",        // whitespace only
    "?!.#"         // punctuation with no letters
  };

  SECTION("Extended valid single string checks") {
    REQUIRE(allExamplesValidate(blueprint, validCases));
  }
  SECTION("Extended invalid single string checks") {
    REQUIRE_FALSE(anyStringValidates(blueprint, invalidCases));
  }
}

TEST_CASE("Extended TYPES.STRINGS checks", "[blueprint][extra]") {
  // repeated "STRING"
  std::string blueprint = "{STRING}";
  std::vector<std::string> validCases = {
    "alpha bravo charlie",
    "* * *",
    "ABC abc 123d *", 
    "X Y Z", 
    "OneString AnotherString ThirdString FourthString FifthString"
  };
  std::vector<std::string> invalidCases = {
    "",                 // empty
    "1234 5678",        // no letter or star in either token
    "abc ??? 123"      // "???" doesn't contain a letter or star
  };

  SECTION("Extended valid repeated string checks") {
    REQUIRE(allExamplesValidate(blueprint, validCases));
  }
  SECTION("Extended invalid repeated string checks") {
    REQUIRE_FALSE(anyStringValidates(blueprint, invalidCases));
  }
}

//------------------------------------------------------------------------------
// Additional Tests: TYPE2B, focusing on the pattern: STRING {[INT | DOUBLE]} {STRING STRING}
//------------------------------------------------------------------------------

TEST_CASE("Extended TYPE2B checks", "[blueprint][extra]") {
  // The previously tested blueprint:
  std::string blueprint = "STRING {[INT | DOUBLE]} {STRING STRING}";
  tadah::core::BlueprintValidator val(blueprint);

  SECTION("Valid: multiple tokens in repeated numeric block") {
    // Repeated numeric block can combine INT/DOUBLE in any repeated order
    // e.g., "11 3.5 0 10 -1 2.0"
    REQUIRE(val.validate("D2_MIE 11 3.5 2.0 6 Ti Nb Nb Nb"));
    REQUIRE(val.validate("Descriptor 1 2 3.0 4.0 5.0 X Y"));  
  }

  SECTION("Invalid: numeric block lacking digits") {
    // The second slot is repeated [INT|DOUBLE], so "abc" is not valid
    REQUIRE_FALSE(val.validate("D2_LJ abc Ti Ti"));
    REQUIRE_FALSE(val.validate("SomeDesc 2.0 -3.0 ? Ti Ti"));
  }

  SECTION("Invalid: second repeated block (STRING STRING) has not enough tokens") {
    // We need at least 2 tokens in that repeated block. "Ti" alone is not enough.
    REQUIRE_FALSE(val.validate("D2_LJ 11 Ti"));
    REQUIRE_FALSE(val.validate("D2_LJ 99 1.5 X"));
  }

  SECTION("Edge: second repeated block with 5 pairs of strings") {
    // The blueprint pattern has “{STRING STRING},” meaning 1 block is 2 strings, can be repeated:
    // repeated block => (STRING STRING)( (STRING STRING))* 
    // e.g. "Ti Nb Al Ni Cu Fe", that’s 3 pairs => 6 tokens
    // Valid, as the blueprint allows indefinite repetition
    REQUIRE(val.validate("MyDescriptor 1 -1 Ti Nb Al Cu X Y Qp Qm XEnd YEnd"));
  }
}

//------------------------------------------------------------------------------
// Additional edge coverage
//------------------------------------------------------------------------------

TEST_CASE("Additional edge coverage for partial matches", "[blueprint][extra]") {
  // Combining multiple patterns to confirm partial match logic is robust
  std::vector<std::string> bps = {"INT INT", "INT INT INT"};
  tadah::core::BlueprintValidator multiple(bps);

  SECTION("Partial match with two INT but the text has 3rd token") {
    // "10 20 30" fully matches the second pattern "INT INT INT", so .validate() => true
    REQUIRE(multiple.validate("10 20 30"));
    // "10 20 30 40" not fully matched by either pattern
    // But partial match might catch "10 20 30"
    REQUIRE_FALSE(multiple.validate("10 20 30 40"));
  }
}

// More corner cases can go here if needed, especially focusing on invalid user input.

/* Pins: a potential written losslessly can carry subnormal values: a
 * {DOUBLE} line of 2048 characters or more whose first value is subnormal
 * or underflows ('4.94065645841246544e-324', '1e-310', '-2.5e-320',
 * '1e-400') is valid. A {INT} line of such values ('1e-310 0.1234...') is
 * refused, long or short, and so is one starting with '1e400' or
 * '-1e400': they are not integers.
 * Before the fix: a line of 2048 characters or more of words strtod reads
 * whole was valid whatever the blueprint, so the long {INT} '1e-310' line
 * was valid (its section pinned that); the other checks passed as well. */
TEST_CASE("Long numeric lines: fast path accepts underflow, rejects overflow",
          "[blueprint][long]") {
  auto long_line = [](const std::string &first) {
    std::string out = first;
    while (out.size() < 4096) out += " 0.123456789012345677";
    return out;
  };

  SECTION("{DOUBLE} line with subnormal and underflowing values") {
    tadah::core::BlueprintValidator validator("{DOUBLE}");
    for (const char *t : {"4.94065645841246544e-324", "1e-310", "-2.5e-320",
                          "1e-400"}) {
      INFO("first token: " << t);
      const std::string line = long_line(t);
      REQUIRE(line.size() > 2048);
      REQUIRE(validator.validate(line));
    }
  }
  SECTION("a {INT} blueprint refuses a line of non-integers, long as "
          "short") {
    tadah::core::BlueprintValidator validator("{INT}");
    CHECK_FALSE(validator.validate("1e-310 0.123456789012345677"));
    CHECK_FALSE(validator.validate(long_line("1e-310")));
  }
  SECTION("overflow is not a valid number") {
    tadah::core::BlueprintValidator validator("{INT}");
    REQUIRE_FALSE(validator.validate(long_line("1e400")));
    REQUIRE_FALSE(validator.validate(long_line("-1e400")));
  }
}

/* Pins: the values of a list (a repeated group {...}) are separated by
 * blanks, as CoreUserConfigParser splits a configuration line into values
 * at blanks only. A comma or a '+' between two numbers is refused, with or
 * without blanks around the comma: {DOUBLE} '0.1,0.2', '0.1, 0.2',
 * '0.1 ,0.2', '0.1+0.2'; {INT} and {UINT} '1,2', '1+2'; {BOOL}
 * 'true,false'; the group {STRING INT DOUBLE DOUBLE} 'LIN 7,0.1 3' and
 * 'LIN,7 0.1 3'; 'INT {DOUBLE}' '3 1.0,2.0'. Blanks (spaces, tabs, several)
 * separate them, a '+' sign after a blank and an exponent's sign are part
 * of a number ('0.1 +0.2', '1e+5 2'). A STRING value holds a comma or a
 * '+' as part of its word ({STRING} 'a,b.tadah', 'Ti+Nb'), as the parser
 * keeps the word whole. An index list {INDEX_PATTERN} (INDEX, FIXINDEX,
 * whose readers split at commas and blanks) takes commas and blanks between
 * entries ('1,3,5', '13 14', '13, 14', '13 ,14', '1 - 4, 7', '1-10:2,12')
 * and refuses a '+' between two entries ('13+14'), an empty entry ('1,,2')
 * and a leading or trailing comma.
 * Before the fix: the elements of a repeated group were joined by the class
 * [,\s+], so these were accepted: every {DOUBLE}, {INT}, {UINT} and {BOOL}
 * comma and '+' form above, the three {STRING INT DOUBLE DOUBLE} forms,
 * 'INT {DOUBLE}' '3 1.0,2.0' and {INDEX_PATTERN} '13+14' (19 checks), and
 * the parser stored '0.1,0.2' as one value, refused when read
 * ('Conversion failed for key=..., value="0.1,0.2"'). The other refusals
 * ({STRING} '1,2' and '1+2', 'INT {DOUBLE}' '3,1.0 2.0', {INDEX_PATTERN}
 * '1,,2', '1,3,', ',1' and '1, ,2') and every valid form passed as well (a
 * pin). */
TEST_CASE("Blueprint lists are separated by blanks, as the parser splits a "
          "line",
          "[blueprint][separators]") {
  struct Bp {
    std::string blueprint;
    std::vector<std::string> valid, invalid;
  };
  const std::vector<Bp> cases = {
      {"{DOUBLE}",
       {"0.1 0.2", "0.1  0.2", "0.1\t0.2", "0.1 +0.2", "1e+5 2", "-1.5e-3"},
       {"0.1,0.2", "0.1, 0.2", "0.1 ,0.2", "0.1+0.2", "0.1 0.2,0.3",
        "1,2,3"}},
      {"{INT}", {"1 2", "-1 +2"}, {"1,2", "1+2", "1, 2"}},
      {"{UINT}", {"1 2", "80 20"}, {"1,2", "1+2", "80,20"}},
      {"{BOOL}", {"true false"}, {"true,false", "true+false"}},
      {"{STRING INT DOUBLE DOUBLE}",
       {"LIN 7 0.1 3", "LIN 7 0.1 3 LOG 5 0.2 4"},
       {"LIN 7,0.1 3", "LIN,7 0.1 3", "LIN 7 0.1+3"}},
      {"INT {DOUBLE}", {"3 1.0 2.0"}, {"3 1.0,2.0", "3,1.0 2.0"}},
      {"{STRING}", {"a,b.tadah", "Ti+Nb", "x1,2 y", "a, b"}, {"1,2", "1+2"}},
      {"{INDEX_PATTERN}",
       {"1,3,5", "1-4,7,9", "1-10:2", "13 14", "13, 14", "13 ,14",
        "1 - 4, 7", "1-10:2,12", "1 , 2 3"},
       {"13+14", "1,,2", "1,3,", ",1", "1, ,2"}},
  };
  for (const Bp &bp : cases) {
    tadah::core::BlueprintValidator v(bp.blueprint);
    for (const std::string &text : bp.valid) {
      INFO(bp.blueprint << " should take '" << text << "'");
      CHECK(v.validate(text));
    }
    for (const std::string &text : bp.invalid) {
      INFO(bp.blueprint << " should refuse '" << text << "'");
      CHECK_FALSE(v.validate(text));
    }
  }
}

/* Pins: a line of 2048 characters or more (checked without std::regex,
 * which overflows the stack on long input) is split into values at blanks
 * as a short line is: a {DOUBLE} line of numbers joined by commas
 * ('1.0,2.0,...'), by '+' or by '-' ('1.0+2.0', '1.0-2.0'), or with one
 * comma among blank-separated numbers, also at about 40000 characters, is
 * refused; one of blank-separated numbers is valid. A {STRING} line of
 * words that hold commas ('x1,2') is valid, as each word is one STRING
 * value. An {INDEX_PATTERN} line of comma-separated entries, ranges and
 * steps ('1,2,...', '1-10:2, 12, ...') is valid; one with a '+' between two
 * entries or an empty entry ('1,,2') is refused.
 * Before the fix: the numeric shortcut skipped commas and read numbers
 * glued by a sign, so the comma, '+' and '-' {DOUBLE} lines and the '+' and
 * empty-entry index lines were valid; the {STRING} shortcut split at
 * commas, so 'x1,2' was refused for its word '2'. */
TEST_CASE("Long lines: values are separated by blanks, as on a short line",
          "[blueprint][long][separators]") {
  auto repeat = [](const std::string &first, const std::string &next,
                   std::size_t size) {
    std::string out = first;
    while (out.size() < size) out += next;
    return out;
  };
  tadah::core::BlueprintValidator doubles("{DOUBLE}");
  CHECK(doubles.validate(repeat("1.0", " 2.0", 2100)));
  for (const std::string &line :
       {repeat("1.0", ",2.0", 2100), repeat("1.0", ", 2.0", 2100),
        repeat("1.0", "+2.0", 2100), repeat("1.0", "-2.0", 2100),
        repeat("1.0", " 2.0", 2100) + ",3.0",
        repeat("1.0", " 2.0", 40000) + ",3.0"}) {
    INFO("{DOUBLE} line of " << line.size() << " characters ending '"
                             << line.substr(line.size() - 12) << "'");
    REQUIRE(line.size() >= 2048);
    CHECK_FALSE(doubles.validate(line));
  }

  tadah::core::BlueprintValidator strings("{STRING}");
  const std::string comma_words = repeat("x1,2", " x1,2", 2100);
  REQUIRE(comma_words.size() >= 2048);
  CHECK(strings.validate(comma_words));

  tadah::core::BlueprintValidator indices("{INDEX_PATTERN}");
  for (const std::string &line :
       {repeat("1", ",2", 2100), repeat("1-10:2", ", 12", 2100),
        repeat("1 - 4", " 7,9", 2100), repeat("13", " 14", 2100)}) {
    INFO("{INDEX_PATTERN} line starting '" << line.substr(0, 12) << "'");
    REQUIRE(line.size() >= 2048);
    CHECK(indices.validate(line));
  }
  for (const std::string &line :
       {repeat("1", "+2", 2100), repeat("1", ",2", 2100) + ",,3",
        repeat("1", ",2", 2100) + ","}) {
    INFO("{INDEX_PATTERN} line ending '" << line.substr(line.size() - 6)
                                         << "'");
    REQUIRE(line.size() >= 2048);
    CHECK_FALSE(indices.validate(line));
  }
}

/* Pins: a line of 2048 characters or more is checked by the rules of a
 * short line: for each blueprint and value, a line of the value repeated,
 * one blank apart, to more than 2048 characters gets the verdict of the
 * line of the value alone, and that verdict is: {STRING} '1e5,2e5',
 * 'inf-1' and '0x1,0x2' valid (each one STRING word, which holds a letter
 * and keeps its comma or sign); {[DOUBLE | STRING]} '10 1e3,5' valid;
 * {DOUBLE} '1.0' and '-2.5e-3' valid, '.5', '5.', 'inf' and '0x10' refused
 * (a DOUBLE has a digit before and after its point, no hex or inf form);
 * {INT} '7' valid, '1.5' refused; 'INT {DOUBLE}' '3 1.0' valid;
 * {INDEX_PATTERN} '1-4,7' valid.
 * Before the fix: long lines took shortcuts that short lines do not: a
 * line of words strtod reads whole was valid whatever the blueprint
 * ({DOUBLE} '.5', '5.', 'inf' and '0x10', {INT} '1.5'), and one whose words
 * are numbers joined by a comma or a sign was refused whatever the
 * blueprint ({STRING} '1e5,2e5', 'inf-1' and '0x1,0x2', {[DOUBLE | STRING]}
 * '10 1e3,5'). */
TEST_CASE("Long lines are checked by the rules of a short line",
          "[blueprint][long][separators]") {
  struct Case {
    std::string blueprint, value;
    bool valid;
  };
  const std::vector<Case> cases = {
      {"{STRING}", "1e5,2e5", true},
      {"{STRING}", "inf-1", true},
      {"{STRING}", "0x1,0x2", true},
      {"{[DOUBLE | STRING]}", "10 1e3,5", true},
      {"{DOUBLE}", "1.0", true},
      {"{DOUBLE}", "-2.5e-3", true},
      {"{DOUBLE}", ".5", false},
      {"{DOUBLE}", "5.", false},
      {"{DOUBLE}", "inf", false},
      {"{DOUBLE}", "0x10", false},
      {"{INT}", "7", true},
      {"{INT}", "1.5", false},
      {"INT {DOUBLE}", "3 1.0", true},
      {"{INDEX_PATTERN}", "1-4,7", true},
  };
  for (const Case &c : cases) {
    tadah::core::BlueprintValidator v(c.blueprint);
    std::string long_line = c.value;
    // 'INT {DOUBLE}' repeats its last value, the others the whole value.
    const std::string unit =
        c.blueprint == "INT {DOUBLE}" ? c.value.substr(c.value.find(' '))
                                      : " " + c.value;
    while (long_line.size() <= 2048) long_line += unit;
    INFO(c.blueprint << " '" << c.value << "', and a line of "
                     << long_line.size() << " characters of it");
    CHECK(v.validate(c.value) == c.valid);
    CHECK(v.validate(long_line) == c.valid);
  }
}

/* Pins: a line that no blueprint of its key takes is refused in a time
 * that grows with its length only, as one that a blueprint takes (each
 * validation here in well under a second): CGRID2B's blueprints ({DOUBLE},
 * {STRING INT DOUBLE DOUBLE}) and 16 values aligned 3 blanks apart, the
 * last two joined by a comma ('16.0,9.0'); {DOUBLE} and 24 values 2 blanks
 * apart, then 'x'; {STRING} and 12 words 'abcd', then '123'; TYPE2B's
 * blueprints and 'D2_X', 22 numbers and one element ('Ta'); and
 * {[DOUBLE | STRING]} and 24 '1e5' (each a DOUBLE and a STRING), then
 * ','. Each is refused, and the same line with the fault removed is valid.
 * Before the fix: std::regex tried, before refusing, every way of sharing
 * each run of blanks between the primitives' \s* and the separator and
 * every way of taking a word (a STRING at any of its letters, '1e5' as a
 * DOUBLE or a STRING, a number as an INT or a DOUBLE), a number of tries
 * exponential in the number of values: the refusals took 6.5 s (CGRID2B),
 * 5.7 s ({DOUBLE}), 5.4 s ({STRING}), 4.1 s (TYPE2B) and 7.8 s
 * ({[DOUBLE | STRING]}), each about twice as long or more for every
 * further value; v1-dev, whose groups took a comma between values,
 * accepted the CGRID2B line at once and was as slow on the others. */
TEST_CASE("A line no blueprint takes is refused in linear time",
          "[blueprint][separators][performance]") {
  struct Case {
    std::vector<std::string> blueprints;
    std::string refused, valid;
  };
  auto values = [](int n, const std::string &gap) {
    std::string out = "1.0";
    for (int i = 2; i <= n; ++i) out += gap + std::to_string(i) + ".0";
    return out;
  };
  auto repeat = [](const std::string &word, int n) {
    std::string out;
    for (int i = 0; i < n; ++i) out += (i ? " " : "") + word;
    return out;
  };
  std::string numbers;
  for (int i = 1; i <= 22; ++i) numbers += " " + std::to_string(i);
  const std::vector<std::string> type2b = {
      "STRING {[INT | DOUBLE]} {STRING STRING}", "STRING {STRING STRING}",
      "STRING"};
  const std::vector<Case> cases = {
      {{"{DOUBLE}", "{STRING INT DOUBLE DOUBLE}"},
       values(16, "   ") + ",9.0", values(16, "   ") + "   9.0"},
      {{"{DOUBLE}"}, values(24, "  ") + " x", values(24, "  ")},
      {{"{STRING}"}, repeat("abcd", 12) + " 123", repeat("abcd", 12)},
      {type2b, "D2_X" + numbers + " Ta", "D2_X" + numbers + " Ta Ta"},
      {{"{[DOUBLE | STRING]}"}, repeat("1e5", 24) + " ,",
       repeat("1e5", 24)},
  };
  for (const Case &c : cases) {
    tadah::core::BlueprintValidator v(c.blueprints);
    INFO("blueprint " << c.blueprints[0] << ", line '" << c.refused << "'");
    const auto t0 = std::chrono::steady_clock::now();
    const bool refused_ok = !v.validate(c.refused);
    const double seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - t0).count();
    INFO("refused in " << seconds << " s");
    CHECK(refused_ok);
    CHECK(seconds < 1.0);
    CHECK(v.validate(c.valid));
  }
}

/* Pins: on short lines (up to 6 words) the validator gives the verdict of
 * the blueprint's anchored regex (getPattern(), one per blueprint, matched
 * with std::regex_match): for the blueprints of TYPE2B, TYPEMB, CGRID2B,
 * MODEL, OALGO, SIGMA, WATOM, DIMER, pct, range, the header's example
 * and of each list type: a line of each that both take, and 3000 lines per
 * set drawn (std::mt19937, seed 20261012) from words that are and are not
 * values of each primitive, joined by one or more blanks or tabs.
 * Before the fix: passes as well (a pin: on lines of fewer than 2048
 * characters validate() was that regex; the fix checks a line word by word
 * and must take the same lines). */
TEST_CASE("The validator takes the lines its blueprint regex takes",
          "[blueprint][separators]") {
  struct Set {
    std::vector<std::string> blueprints;
    std::string example;  // a line both take
  };
  const std::vector<Set> sets = {
      {{"STRING {[INT | DOUBLE]} {STRING STRING}", "STRING {STRING STRING}",
        "STRING"}, "D2_Blip 4 4 Ta Ta"},
      {{"STRING UINT {STRING STRING}", "STRING UINT UINT {STRING STRING}",
        "STRING"}, "DM_EAM 2 Ta Ta"},
      {{"{DOUBLE}", "{STRING INT DOUBLE DOUBLE}"}, "LIN 7 0.1 3"},
      {{"STRING STRING", "STRING STRING STRING", "STRING STRING UINT"},
       "M_KRR Kern_Linear 2"},
      {{"INT", "INT DOUBLE"}, "2 1e-6"},
      {{"INT {DOUBLE}"}, "2 0.5 1.5"},
      {{"{STRING STRING DOUBLE} ", "{DOUBLE}"}, "Ta Ta 1.0"},
      {{"BOOL DOUBLE BOOL"}, "true 1.5 false"},
      {{"{[DOUBLE | STRING]}"}, "10 FIXWEIGHT=2"},
      {{"DOUBLE DOUBLE INT"}, "1.0 2.0 3"},
      {{"INT [DOUBLE] {STRING}"}, "10 3.14 hello"},
      {{"{BOOL}"}, "true false"}, {{"{INT}"}, "-3 4"},
      {{"{UINT}"}, "0 7"}, {{"{STRING}"}, "a,b.tadah Ta"},
      {{"{INDEX_PATTERN}"}, "1 - 4, 7 9"},
  };
  const std::vector<std::string> words = {
      "0", "7", "-3", "+4", "01", "1.5", "-2.5e-3", "1e5", "1E+5", ".5",
      "5.", "inf", "0x10", "true", "false", "True", "Ta", "D2_Blip", "*",
      "x1,2", "1,2", "1+2", "13,14", "1-4", "1-10:2", "1-4,7", ",", "-",
      ":", "1,", ",2", "a,b", "Ti+Nb", "1e5,2e5", "/p/a.tadah", "#"};
  const std::vector<std::string> gaps = {" ", "  ", "\t", " \t "};
  std::mt19937 rng(20261012);
  for (const Set &set : sets) {
    const std::vector<std::string> &bps = set.blueprints;
    tadah::core::BlueprintValidator v(bps);
    std::vector<std::regex> res;
    std::istringstream patterns(v.getPattern());
    for (std::string line; std::getline(patterns, line);)
      if (!line.empty()) res.emplace_back(line, std::regex::ECMAScript);
    REQUIRE(res.size() == bps.size());
    {
      INFO("blueprint " << bps[0] << ", example '" << set.example << "'");
      bool example_by_regex = false;
      for (const auto &re : res)
        example_by_regex =
            example_by_regex || std::regex_match(set.example, re);
      REQUIRE(example_by_regex);
      CHECK(v.validate(set.example));
    }
    for (int k = 0; k < 3000; ++k) {
      const int n = static_cast<int>(rng() % 7);
      std::string text = rng() % 4 == 0 ? " " : "";
      for (int i = 0; i < n; ++i) {
        if (i) text += gaps[rng() % gaps.size()];
        text += words[rng() % words.size()];
      }
      bool by_regex = false;
      for (const auto &re : res) by_regex = by_regex || std::regex_match(text, re);
      INFO("blueprint " << bps[0] << ", line '" << text << "'");
      REQUIRE(v.validate(text) == by_regex);
    }
  }
}
