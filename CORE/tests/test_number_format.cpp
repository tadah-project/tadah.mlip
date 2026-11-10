#include "catch2/catch.hpp"
#include <tadah/core/context.h>
#include <tadah/core/core_user_config_parser.h>
#include <tadah/core/utils.h>
#include <tadah/core/number_format.h>

#include <cerrno>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <random>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace tadah::core;

namespace {

std::uint64_t bits_of(double v) {
  std::uint64_t u;
  std::memcpy(&u, &v, sizeof u);
  return u;
}

double from_bits(std::uint64_t u) {
  double v;
  std::memcpy(&v, &u, sizeof v);
  return v;
}

// The two parsers text in Context meets: istringstream (Context::get<double>)
// and strtod (std::stod in the descriptor grid readers).
double parse_iss(const std::string &s) {
  std::istringstream iss(s);
  double v = std::numeric_limits<double>::quiet_NaN();
  iss >> v;
  return v;
}

double parse_strtod(const std::string &s) { return std::strtod(s.c_str(), nullptr); }

// Blueprint DOUBLE token grammar (blueprint_validator.cpp g_primitiveMap).
const std::regex &double_token_re() {
  static const std::regex re("[-+]?\\d+(?:\\.\\d+)?(?:[eE][-+]?\\d+)?");
  return re;
}

std::string printf_g(double v, int digits) {
  char buf[256];
  std::snprintf(buf, sizeof buf, "%.*g", digits, v);
  return buf;
}

} // namespace

TEST_CASE("format_roundtrip: special values round-trip bit-exactly",
          "[number_format]") {
  const std::vector<double> values = {
      0.0,       -0.0,     0.1,          0.1 + 0.2,   5.3,
      1e-20,     1e-13,    3e-13,        1e5,         100000.0,
      -7.0,      1.0 / 3,  DBL_MAX,      -DBL_MAX,    DBL_MIN,
      DBL_EPSILON,         std::numeric_limits<double>::denorm_min(),
      4.9e-310,  9.612509619274691e+19,  9007199254740991.0,
      9007199254740992.0,  1e300,        0.962983118529000759,
      0.962983118528700777};
  for (double v : values) {
    const std::string s = format_roundtrip(v);
    INFO("value text: " << s);
    CHECK(bits_of(parse_iss(s)) == bits_of(v));
    CHECK(bits_of(parse_strtod(s)) == bits_of(v));
    CHECK(std::regex_match(s, double_token_re()));
  }
}

TEST_CASE("format_roundtrip: textual forms", "[number_format]") {
  CHECK(format_roundtrip(0.1) == "0.1");
  CHECK(format_roundtrip(0.1 + 0.2) == "0.30000000000000004");
  CHECK(format_roundtrip(5.0) == "5");
  CHECK(format_roundtrip(-0.0) == "-0");
  CHECK(format_roundtrip(1e-20) == "1e-20");
  CHECK(format_roundtrip(9.612509619274691e+19) == "9.612509619274691e+19");
  // Integral values below 2^53 stay plain integers: integer readers work.
  CHECK(format_roundtrip(100000.0) == "100000");
  CHECK(format_roundtrip(1e15) == "1000000000000000");
  CHECK(format_roundtrip(-42.0) == "-42");
  std::istringstream iss(format_roundtrip(1e6));
  int k = 0;
  iss >> k;
  CHECK(k == 1000000);
  CHECK(format_roundtrip(std::numeric_limits<double>::infinity()) == "inf");
  CHECK(format_roundtrip(-std::numeric_limits<double>::infinity()) == "-inf");

  std::string out = "x=";
  append_roundtrip(out, 0.25);
  CHECK(out == "x=0.25");
}

TEST_CASE("format_roundtrip: 1e5 random bit patterns round-trip",
          "[number_format]") {
  std::mt19937_64 rng(20260924);
  std::size_t checked = 0;
  for (int i = 0; i < 100000; ++i) {
    const double v = from_bits(rng());
    if (std::isnan(v) || std::isinf(v)) continue;
    const std::string s = format_roundtrip(v);
    if (bits_of(parse_iss(s)) != bits_of(v) ||
        bits_of(parse_strtod(s)) != bits_of(v)) {
      FAIL("round trip failed for " << s);
    }
    ++checked;
  }
  CHECK(checked > 99000);
}

TEST_CASE("format_sig matches printf %.*g", "[number_format]") {
  std::mt19937_64 rng(7);
  const int digits[] = {1, 6, 12, 15, 17, 18, 25};
  for (int i = 0; i < 20000; ++i) {
    const double v = from_bits(rng());
    if (std::isnan(v) || std::isinf(v)) continue;
    for (int d : digits) {
      if (format_sig(v, d) != printf_g(v, d)) {
        FAIL("format_sig(" << printf_g(v, 17) << ", " << d
                           << ") = " << format_sig(v, d)
                           << " vs printf " << printf_g(v, d));
      }
    }
  }
  CHECK(format_sig(0.1, 18) == "0.100000000000000006");
  CHECK(format_sig(5.3, 18) == "5.29999999999999982");
  CHECK(format_sig(3.0, 18) == "3");
  CHECK(format_sig(0.1, 12) == "0.1");
  CHECK(format_sig(0.123456789012345678, 12) == "0.123456789012");
  // Clamped to [1, kMaxSigDigits].
  CHECK(format_sig(0.123, 0) == printf_g(0.123, 1));
  CHECK(format_sig(0.1, 500) == printf_g(0.1, kMaxSigDigits));
  // 17 and 18 significant digits round-trip every double.
  std::mt19937_64 rng2(11);
  for (int i = 0; i < 20000; ++i) {
    const double v = from_bits(rng2());
    if (std::isnan(v) || std::isinf(v)) continue;
    REQUIRE(bits_of(parse_iss(format_sig(v, 17))) == bits_of(v));
    REQUIRE(bits_of(parse_iss(format_sig(v, 18))) == bits_of(v));
  }
}

TEST_CASE("is_integer_literal", "[number_format]") {
  for (const char *t : {"0", "7", "-3", "+12", "0012", "12345678901234567890"})
    CHECK(is_integer_literal(t));
  for (const char *t : {"", "+", "-", "3.0", "3.", "1e5", "1-58", "LIN", " 3",
                        "3 ", "0x10", "true"})
    CHECK_FALSE(is_integer_literal(t));
}

TEST_CASE("parse_decimal_finite", "[number_format]") {
  double v = -1.0;
  CHECK(parse_decimal_finite("1.5", v));
  CHECK(v == 1.5);
  CHECK(parse_decimal_finite("+3.25", v));
  CHECK(v == 3.25);
  CHECK(parse_decimal_finite("-2", v));
  CHECK(v == -2.0);
  CHECK(parse_decimal_finite("5.", v));
  CHECK(v == 5.0);
  CHECK(parse_decimal_finite(".5", v));
  CHECK(v == 0.5);
  CHECK(parse_decimal_finite("1E5", v));
  CHECK(v == 1e5);
  CHECK(parse_decimal_finite("-0", v));
  CHECK((v == 0.0 && std::signbit(v)));
  // Subnormals are representable and accepted.
  CHECK(parse_decimal_finite("4.9e-324", v));
  CHECK(v == std::numeric_limits<double>::denorm_min());
  CHECK(parse_decimal_finite("1e-310", v));
  CHECK(v == parse_strtod("1e-310"));
  // Underflow to zero reads as +-0 (what istringstream gives), not a failure.
  CHECK(parse_decimal_finite("1e-400", v));
  CHECK((v == 0.0 && !std::signbit(v)));
  CHECK(parse_decimal_finite("-1e-400", v));
  CHECK((v == 0.0 && std::signbit(v)));
  CHECK(parse_iss("1e-400") == 0.0);

  v = 42.0;
  for (const char *t : {"", "+", "-", ".", "e5", "1e", "1e+", "1.5.2", "LIN",
                        "GEOM", "1-58", "0x1p3", "inf", "-inf", "nan", "true",
                        " 1.5", "1.5 ", "1,5", "1e400", "-1e400"}) {
    INFO("token: '" << t << "'");
    CHECK_FALSE(parse_decimal_finite(t, v));
  }
  CHECK(v == 42.0); // untouched on failure
}

TEST_CASE("parse_decimal_normal: the hpotarget number grammar",
          "[number_format]") {
  // parse_decimal_finite, and the value must be zero or a normal double:
  // the grammar of tadah hpo's hpotarget numbers (HPO's parse_number), so
  // the numbers of tadah refit --retarget, which it writes into an
  // hpotarget, are the numbers tadah hpo reads back.
  double v = -1.0;
  for (const auto &ok : std::vector<std::pair<const char *, double>>{
           {"10", 10.0},       {"+7.5", 7.5},     {"-0.25", -0.25},
           {"5.", 5.0},        {".5", 0.5},       {"2.5E+07", 2.5e7},
           {"0", 0.0},         {"-0.0", -0.0},    {"0e-400", 0.0},
           {"2.2250738585072014e-308", DBL_MIN},
           {"1.7976931348623157e308", DBL_MAX}}) {
    INFO("token: '" << ok.first << "'");
    CHECK(parse_decimal_normal(ok.first, v));
    CHECK(v == ok.second);
  }
  v = 42.0;
  for (const char *t :
       {"", "+", ".", "5x", "0x10", "0x1p-1", "inf", "-inf", "nan", "NaN",
        " 1", "1 ", "1,5", "1e400", "-1e400",
        // below the smallest normal double: a subnormal, or an underflow to 0
        "1e-310", "4.9e-324", "-2.2e-308", "1e-400", "-1e-400"}) {
    INFO("token: '" << t << "'");
    CHECK_FALSE(parse_decimal_normal(t, v));
  }
  CHECK(v == 42.0);  // untouched on failure
}

TEST_CASE("reformat_double_token", "[number_format]") {
  CHECK(reformat_double_token("0.1", 18) == "0.100000000000000006");
  CHECK(reformat_double_token("0.1", 17) == "0.10000000000000001");
  CHECK(reformat_double_token("0.1", 12) == "0.1");
  CHECK(reformat_double_token("3.0", 18) == "3");
  CHECK(reformat_double_token("1e-13", 18) == "1.00000000000000003e-13");
  CHECK(reformat_double_token("+0.5", 18) == "0.5");
  CHECK(reformat_double_token("4.9e-324", 18) == "4.94065645841246544e-324");
  CHECK(reformat_double_token("1e-400", 18) == "0");
  // Verbatim tokens.
  for (const char *t : {"7", "-3", "20", "LIN", "GEOM", "D2_Blip5", "Ta",
                        "true", "false", "1-58", "1-10:2", "inf", "nan",
                        "1e400", "Cut_Cos", "M_BLR"})
    CHECK(reformat_double_token(t, 18) == t);

  char buf[kReformatBufferSize];
  CHECK(reformat_double_token("LIN", 18, buf, sizeof buf) == 0);
  CHECK(reformat_double_token("12", 18, buf, sizeof buf) == 0);
  const std::size_t n = reformat_double_token("5.3", 18, buf, sizeof buf);
  CHECK(std::string(buf, n) == "5.29999999999999982");
  char small[8];
  CHECK_THROWS(reformat_double_token("5.3", 18, small, sizeof small));
}

TEST_CASE("reformat_double_token / format_sig_chars: a finite value never "
          "renders to text that overflows", "[number_format][lossless]") {
  // NSTDEV stores DBL_MAX for every constant descriptor column. Plain %.{N}g
  // rounds it past DBL_MAX for N in {1-5, 10, 11, 15, 16} ("1.8e+308"),
  // which made the written potential unloadable. The guard truncates the
  // mantissa toward zero instead, keeping exactly N significant digits.
  const double big[] = {DBL_MAX, -DBL_MAX, std::nextafter(DBL_MAX, 0.0),
                        1.7976931348623155e308, 1e308, -1.0000000000000002e308};
  char buf[kReformatBufferSize];
  for (double v : big) {
    const std::string tok = format_roundtrip(v);
    for (int n = 1; n <= 18; ++n) {
      INFO("v=" << tok << " NUMERIC " << n);
      const std::string out = reformat_double_token(tok, n);
      const std::size_t len = format_sig_chars(v, n, buf, sizeof buf);
      CHECK(std::string(buf, len) == out);
      errno = 0;
      char *end = nullptr;
      const double back = std::strtod(out.c_str(), &end);
      CHECK(end == out.c_str() + out.size());
      CHECK(errno == 0);
      CHECK(std::isfinite(back));
      CHECK(std::isfinite(parse_iss(out)));
      CHECK(std::signbit(back) == std::signbit(v));
      // at most n significant digits in the mantissa
      const std::string mant = out.substr(0, out.find('e'));
      int digits = 0;
      for (char c : mant) digits += (c >= '0' && c <= '9');
      CHECK(digits <= n);
      // plain %.{N}g wherever that already reads back finite
      if (std::isfinite(parse_strtod(printf_g(v, n))))
        CHECK(out == printf_g(v, n));
      else
        CHECK(std::fabs(back) <= std::fabs(v));   // truncated toward zero
    }
  }
  CHECK(reformat_double_token(format_roundtrip(DBL_MAX), 3) == "1.79e+308");
  CHECK(reformat_double_token(format_roundtrip(-DBL_MAX), 1) == "-1e+308");
  CHECK(reformat_double_token(format_roundtrip(DBL_MAX), 16) ==
        "1.797693134862315e+308");
  // everything below 1e308 is plain %.{N}g
  CHECK(std::string(buf, format_sig_chars(9.99e307, 2, buf, sizeof buf)) ==
        printf_g(9.99e307, 2));
  CHECK(std::string(buf, format_sig_chars(std::numeric_limits<double>::infinity(),
                                          5, buf, sizeof buf)) == "inf");
  char small[8];
  CHECK_THROWS(format_sig_chars(1.0, 18, small, sizeof small));
}

TEST_CASE("Context round trip of a DBL_MAX NSTDEV at every NUMERIC",
          "[number_format][context][lossless]") {
  for (int n : {1, 5, 10, 11, 15, 16, 17, 18}) {
    INFO("NUMERIC " << n);
    Context c;
    c.add("NSTDEV", std::vector<double>{0.0, DBL_MAX, 1.25, -DBL_MAX});
    std::ostringstream os;
    c.write_potential(os, n);
    std::istringstream is(os.str());
    const Context back = CoreUserConfigParser().parse(is);
    REQUIRE(back.size("NSTDEV") == 4);
    double v1 = 0.0, v3 = 0.0;
    REQUIRE_NOTHROW(v1 = back.get<double>("NSTDEV", 1));
    REQUIRE_NOTHROW(v3 = back.get<double>("NSTDEV", 3));
    CHECK(std::isfinite(v1));
    CHECK(std::isfinite(v3));
    CHECK(v1 >= 1e308);
    CHECK(v3 <= -1e308);
    if (n >= 17) {
      CHECK(v1 == DBL_MAX);
      CHECK(v3 == -DBL_MAX);
    }
  }
}

/* Pins: parse_number, to_double and is_number accept every literal that
 * underflows, with the value istringstream reads (the subnormal, or +-0),
 * whatever the C library's strtod reports for it. std::stod, which the
 * readers used before, keeps its own contract on the platform: it throws
 * out_of_range exactly for the literals whose strtod sets ERANGE, and
 * otherwise returns the value parse_number gives. Whether strtod sets ERANGE
 * on an underflow is the C library's choice (C11 7.22.1.3): glibc sets it
 * for each literal here, musl not for 4.9406564584124654e-324 (it returns
 * denorm_min without ERANGE).
 * Before the fix: passes as well (a pin of the readers; the defect was in
 * this test, which asserted that std::stod throws for every literal here
 * and failed with musl (Alpine 3.19): 'no exception was thrown where one
 * was expected' for 4.9406564584124654e-324). */
TEST_CASE("to_double / parse_number: std::stod plus subnormals",
          "[number_format][lossless]") {
  const double sub = 1e-310;
  const double dmin = std::numeric_limits<double>::denorm_min();
  SECTION("underflow is a value (istringstream reads the same)") {
    for (const char *t : {"1e-310", "9.99999999999996945e-311", "5e-324",
                          "4.9406564584124654e-324", "-2.2250738585072009e-308",
                          "1e-400", "-1e-400"}) {
      INFO(t);
      double v = 42.0;
      REQUIRE(parse_number(t, v));
      CHECK(bits_of(v) == bits_of(parse_iss(t)));
      CHECK(bits_of(to_double(t)) == bits_of(v));
      CHECK(is_number(t));
      // What the readers used before: std::stod, which throws out_of_range
      // when strtod sets ERANGE and gives strtod's value otherwise.
      errno = 0;
      const double c = std::strtod(t, nullptr);
      const bool erange = errno == ERANGE;
      INFO("strtod sets ERANGE: " << (erange ? "yes" : "no"));
      CHECK(bits_of(c) == bits_of(v));
      if (erange) {
        CHECK_THROWS_AS(static_cast<void>(std::stod(t)), std::out_of_range);
      } else {
        double s = 42.0;
        CHECK_NOTHROW(s = std::stod(t));
        CHECK(bits_of(s) == bits_of(v));
      }
    }
  }
  SECTION("same acceptance as std::stod otherwise") {
    double v = 0.0;
    CHECK(parse_number("2.2250738585072014e-308", v));
    CHECK(v == DBL_MIN);
    CHECK(parse_number(format_roundtrip(sub), v));
    CHECK(v == sub);
    CHECK(parse_number(format_roundtrip(-dmin), v));
    CHECK(v == -dmin);
    for (const char *t : {"inf", "-inf", "nan", "0x1p-3", " 1.5", "+2", "7"})
      CHECK(parse_number(t, v));
    for (const char *t : {"", "abc", "1.5x", "1.5 ", "1e400", "-1e400", "--1"})
      CHECK_FALSE(parse_number(t, v));
    std::size_t pos = 0;
    CHECK(to_double("1.5 rest", &pos) == 1.5);
    CHECK(pos == 3);
    CHECK_THROWS_AS(to_double("abc"), std::invalid_argument);
    CHECK_THROWS_AS(to_double("1e400"), std::out_of_range);
    CHECK_THROWS_AS(to_double(""), std::invalid_argument);
    errno = EDOM;
    (void)to_double("1e-310");
    CHECK(errno == EDOM);   // errno left as it was, like std::stod
  }
  SECTION("parse_number leaves out untouched on failure") {
    double v = 42.0;
    CHECK_FALSE(parse_number("1.5x", v));
    CHECK(v == 42.0);
  }
}

TEST_CASE("resolve_pot_digits is non-inserting and reads NUMERIC",
          "[number_format]") {
  SECTION("absent -> default, nothing inserted") {
    Context c;
    const PotDigits pd = resolve_pot_digits(c);
    CHECK(pd.digits == kPotDigitsDefault);
    CHECK(pd.digits == 18);
    CHECK_FALSE(pd.explicit_);
    CHECK_FALSE(c.has_stored("NUMERIC"));
    CHECK(c.size() == 0);
    CHECK(pot_digits_warning(pd).empty());
  }
  SECTION("explicit NUMERIC 12 -> 12 digits + warning") {
    Context c;
    c.add("NUMERIC", 12);
    const PotDigits pd = resolve_pot_digits(c);
    CHECK(pd.digits == 12);
    CHECK(pd.explicit_);
    const std::string w = pot_digits_warning(pd);
    CHECK(w.find("NUMERIC 12") != std::string::npos);
    CHECK(w.find("significant digits") != std::string::npos);
  }
  SECTION("explicit NUMERIC >= 17 -> no warning") {
    for (int n : {17, 18, 25}) {
      Context c;
      c.add("NUMERIC", n);
      const PotDigits pd = resolve_pot_digits(c);
      CHECK(pd.digits == n);
      CHECK(pd.explicit_);
      CHECK(pot_digits_warning(pd).empty());
    }
  }
  SECTION("a default-inserted NUMERIC never lowers precision") {
    Context c;
    (void)c.get<size_t>("NUMERIC"); // inserts the blueprint default
    REQUIRE(c.has_stored("NUMERIC"));
    const PotDigits pd = resolve_pot_digits(c);
    CHECK(pd.digits >= kRoundTripDigits);
    CHECK(pot_digits_warning(pd).empty());
  }
  SECTION("integral decimal and clamping") {
    Context c;
    c.add("NUMERIC", "12.000");
    CHECK(resolve_pot_digits(c).digits == 12);
    c.remove("NUMERIC");
    c.add("NUMERIC", "1000");
    CHECK(resolve_pot_digits(c).digits == kMaxSigDigits);
    c.remove("NUMERIC");
    c.add("NUMERIC", "0");
    CHECK(resolve_pot_digits(c).digits == 1);
  }
  SECTION("non-integer NUMERIC throws") {
    Context c;
    c.add("NUMERIC", "twelve");
    CHECK_THROWS_AS(resolve_pot_digits(c), std::runtime_error);
    c.remove("NUMERIC");
    c.add("NUMERIC", "12.5");
    CHECK_THROWS_AS(resolve_pot_digits(c), std::runtime_error);
  }
  SECTION("warning only for explicit values below 17") {
    CHECK(pot_digits_warning(PotDigits{12, false}).empty());
    CHECK_FALSE(pot_digits_warning(PotDigits{16, true}).empty());
    CHECK(pot_digits_warning(PotDigits{17, true}).empty());
  }
}

/* Pins: the NUMERIC warning (NUMERIC 6) names the keys whose numbers are
 * written exactly whatever NUMERIC is, the cutoff and grid keys a load checks
 * exactly, in alphabetical order: '(integers are written exactly, and so are
 * the numbers of CGRID2B, CGRIDMB, RCUT2B, RCUTMB, SGRID2B and SGRIDMB, which
 * a load checks exactly)', and says that the other values do not round-trip.
 * Before the fix: it said that NUMERIC limits every non-integer number
 * ('(integers are written exactly): values do not round-trip exactly'), and
 * the cutoffs and grids were rounded too. */
TEST_CASE("pot_digits_warning names the keys written exactly",
          "[number_format][exact_in_potential]") {
  const std::string w = pot_digits_warning(PotDigits{6, true});
  INFO(w);
  CHECK(w.find("NUMERIC 6 limits every non-integer number of the "
               "potential's key/value lines to 6 significant digits "
               "(integers are written exactly, and so are the numbers of "
               "CGRID2B, CGRIDMB, RCUT2B, RCUTMB, SGRID2B and SGRIDMB, which "
               "a load checks exactly): the other values do not round-trip "
               "exactly") != std::string::npos);
}

/* Pins: the NUMERIC warning's advice names the configuration as the source
 * of an explicit NUMERIC: "NUMERIC is never written into the potential:
 * delete the NUMERIC line from the configuration that set it (default 18,
 * exact) or set NUMERIC >= 17." -- a potential's NUMERIC line (a 1.5.0
 * tadah hpo best_pot.tadah has one) is never read (run_setting_keys()), so
 * no potential sets it.
 * Before the fix: "delete the NUMERIC line from the configuration or
 * potential that set it": the CHECKs fail. */
TEST_CASE("pot_digits_warning names the configuration as the source of "
          "NUMERIC",
          "[number_format][potential_run_settings]") {
  const std::string w = pot_digits_warning(PotDigits{12, true});
  INFO(w);
  CHECK(w.find("NUMERIC is never written into the potential: delete the "
               "NUMERIC line from the configuration that set it (default 18, "
               "exact) or set NUMERIC >= 17.") != std::string::npos);
  CHECK(w.find("potential that set it") == std::string::npos);
}
