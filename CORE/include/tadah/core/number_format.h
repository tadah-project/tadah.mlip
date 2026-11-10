#ifndef TADAH_CORE_NUMBER_FORMAT_H
#define TADAH_CORE_NUMBER_FORMAT_H

/*
 *  Number <-> text conversions with an explicit precision contract.
 *
 *  Two rules, kept apart on purpose:
 *
 *  1. In-memory text is LOSSLESS. Context stores every value as text, so a
 *     double that goes through Context::add() and back through get<double>()
 *     must come back bit-identical. format_roundtrip() gives the shortest
 *     decimal that parses back to the same double (std::to_chars); nothing
 *     that feeds a calculation may use a fixed-precision rendering.
 *
 *  2. Potential and dataset files follow NUMERIC as SIGNIFICANT digits
 *     (printf %.{N}g). The default (kPotDigitsDefault = 18) is bit-exact
 *     for every double (17 significant digits always suffice). A smaller
 *     explicit NUMERIC is honoured, with a warning (pot_digits_warning()),
 *     except for the keys a potential's load checks exactly (the blueprint's
 *     exact_in_potential: the cutoffs and radial grids), whose numbers a
 *     potential records exactly whatever NUMERIC is
 *     (reformat_double_token_exact()).
 *     Loaded values are never limited by NUMERIC, and every TADAH reader
 *     of a file TADAH writes -- potential, configuration, checkpoint,
 *     dataset, VASP POSCAR -- accepts what the writers produce
 *     (to_double(): exponent forms and subnormals included).
 *
 *  Formatting is locale-independent (std::to_chars) and allocation-free
 *  apart from the returned std::string.
 */

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace tadah {
namespace core {

class Context;

/// Significant digits that round-trip every IEEE-754 double (max_digits10).
constexpr int kRoundTripDigits = 17;

/// Default NUMERIC: significant digits used for numbers in potential files.
constexpr int kPotDigitsDefault = 18;

/// Upper clamp for a significant-digits request (format_sig et al.).
constexpr int kMaxSigDigits = 99;

/**
 * @brief Shortest decimal text that parses back to exactly @p v.
 *
 * Integral values with |v| < 2^53 are written as plain integers
 * ("5", "100000", "-0") so integer readers (get<int>, std::stoi) keep
 * working on them; everything else uses the shortest %g-style form
 * ("0.1", "1e-20", "9.612509619274691e+19"). Every finite result matches
 * the blueprint DOUBLE token grammar. inf/nan are written as "inf",
 * "-inf", "nan".
 */
std::string format_roundtrip(double v);

/// Appends format_roundtrip(v) to @p out (no temporary string).
void append_roundtrip(std::string &out, double v);

/**
 * @brief printf("%.*g", digits, v) in the "C" locale.
 *
 * @p digits is clamped to [1, kMaxSigDigits].
 */
std::string format_sig(double v, int digits);

/// Minimum buffer size accepted by the buffer forms below.
constexpr std::size_t kReformatBufferSize = 128;

/**
 * @brief Buffer form of format_sig() for files that are read back
 * (datasets): writes @p v with @p digits significant digits into @p buf
 * and returns the length (no allocation).
 *
 * Unlike plain %.{N}g, the text of a finite @p v always reads back as a
 * finite number: a value that would round past DBL_MAX (e.g. DBL_MAX at 3
 * digits, "1.8e+308") has its mantissa truncated toward zero instead
 * ("1.79e+308"). @p cap must be at least kReformatBufferSize.
 */
std::size_t format_sig_chars(double v, int digits, char *buf,
                             std::size_t cap);

/// True iff @p tok is [+-]?[0-9]+ (an integer literal, no '.', no exponent).
bool is_integer_literal(std::string_view tok);

/**
 * @brief True iff the WHOLE token @p tok is a decimal floating literal:
 * [+-]? digits [. digits] [e|E [+-] digits], with at least one mantissa
 * digit ("5", "5.", ".5", "-1e-3"); no hex literals, inf or nan.
 */
bool is_decimal_literal(std::string_view tok);

/**
 * @brief Parses @p tok iff the WHOLE token is a decimal floating literal
 * ([+-]? digits [. digits] [e|E [+-] digits], also "5." and ".5") whose
 * value is finite (strtod in the "C" locale).
 *
 * Returns false, leaving @p out untouched, for anything else: names,
 * index patterns, hex literals, inf/nan, and literals that overflow.
 * A literal that underflows is accepted with the value istringstream reads
 * for it (the subnormal, or +-0).
 */
bool parse_decimal_finite(std::string_view tok, double &out);

/**
 * @brief parse_decimal_finite() into a float (strtof) or a long double
 * (strtold): the same grammar (is_decimal_literal()), a value that
 * overflows the type is refused, one that underflows it is accepted with
 * the value the conversion gives (the subnormal, or +-0). @p out is left
 * untouched on false.
 */
bool parse_decimal_finite(std::string_view tok, float &out);
/// @copydoc parse_decimal_finite(std::string_view, float &)
bool parse_decimal_finite(std::string_view tok, long double &out);

/**
 * @brief parse_decimal_finite() whose value is also zero or a normal
 * double: the number grammar of the hpotarget file (tadah hpo's
 * parse_number, the syntax LAMMPS' utils::is_double accepts).
 *
 * A literal whose magnitude is below the smallest normal double is refused
 * (false, @p out untouched): a subnormal ("1e-310") and a nonzero literal
 * that underflows to 0 ("1e-400"). An exact zero ("0", "-0.0", "0e-400")
 * is a value. The tests are on the bits, which neither -ffast-math nor a
 * denormals-are-zero mode can defeat. For numbers that end up in an
 * hpotarget, such as those of `tadah refit --retarget` (--pct, --set).
 */
bool parse_decimal_normal(std::string_view tok, double &out);

/**
 * @brief std::stod without the underflow error: the numeric parser of
 * every reader of Context, potential, configuration, checkpoint and
 * dataset tokens.
 *
 * Same acceptance as std::stod (strtod in the current C locale: leading
 * whitespace, decimal and hex literals, inf/nan; @p pos receives the
 * number of characters consumed), except that a literal that UNDERFLOWS
 * is accepted with the value istringstream (and so Context::get<double>)
 * reads for it: the subnormal, or +-0. std::stod throws out_of_range for
 * such a literal whenever strtod sets ERANGE for it, which the C standard
 * leaves to the C library: glibc sets it for every one whose value it
 * rounds ("1e-310") and not for an exact subnormal ("0x1p-1074"); musl
 * not for every rounded one. format_roundtrip() and write_potential()
 * write one for every subnormal double (e.g. "1e-310"). to_double accepts
 * it whatever strtod reports.
 *
 * @throws std::invalid_argument if no conversion can be performed.
 * @throws std::out_of_range if the value overflows.
 */
double to_double(const std::string &s, std::size_t *pos = nullptr);

/**
 * @brief Whole-token form of to_double(), the semantics of is_number():
 * true (value in @p out) iff the ENTIRE token converts and does not
 * overflow; false, leaving @p out untouched, otherwise.
 */
bool parse_number(const std::string &tok, double &out) noexcept;

/**
 * @brief Re-renders a potential-file token with @p digits significant
 * digits when it is a non-integer decimal literal; any other token is
 * returned verbatim.
 *
 * Integer literals are exempt (they are exact as written and may sit in
 * INT/UINT positions); so are strings, grid generator names (LIN/GEOM),
 * bools, index patterns, inf/nan and overflowing literals. The result
 * always reads back finite (see format_sig_chars()).
 */
std::string reformat_double_token(std::string_view tok, int digits);

/**
 * @brief Buffer form of reformat_double_token(): writes the re-rendered
 * token into @p buf and returns its length, or returns 0 when the token
 * must be written verbatim. @p cap must be at least kReformatBufferSize.
 */
std::size_t reformat_double_token(std::string_view tok, int digits,
                                  char *buf, std::size_t cap);

/**
 * @brief The exact counterpart of reformat_double_token(): writes a
 * non-integer decimal literal @p tok as the shortest text that reads back
 * as the same double (format_roundtrip()) into @p buf and returns its
 * length, or returns 0 when the token must be written verbatim (the tokens
 * reformat_double_token() leaves verbatim). @p cap must be at least
 * kReformatBufferSize. Context::write_potential writes the keys the
 * blueprint marks `exact_in_potential` with it below kRoundTripDigits.
 */
std::size_t reformat_double_token_exact(std::string_view tok, char *buf,
                                        std::size_t cap);

/**
 * @brief The ALLOWED_KEYS keys whose numbers a potential file records
 * exactly whatever NUMERIC is (BlueprintKey::exact_in_potential), in
 * alphabetical order: CGRID2B, CGRIDMB, RCUT2B, RCUTMB, SGRID2B, SGRIDMB.
 */
std::vector<std::string> exact_in_potential_keys();

/**
 * @brief Precision for numbers written into a potential file.
 *
 * `explicit_` is true iff the Context STORES a NUMERIC value (user config,
 * a potential file that carried NUMERIC, or a value inserted by a caller).
 */
struct PotDigits {
  int digits = kPotDigitsDefault;
  bool explicit_ = false;
};

/**
 * @brief Resolves the potential-file precision from @p ctx without
 * inserting any default (non-inserting lookup): NUMERIC absent ->
 * {kPotDigitsDefault, false}; present -> {value clamped to
 * [1, kMaxSigDigits], true}.
 *
 * @throws std::runtime_error if the stored NUMERIC is not an integer.
 */
PotDigits resolve_pot_digits(const Context &ctx);

/**
 * @brief Warning text for a precision that cannot round-trip doubles:
 * empty unless `pd.explicit_ && pd.digits < kRoundTripDigits`.
 *
 * Covers the potential's key/value lines (integer literals are written
 * exactly, and so are the numbers of the keys exact_in_potential_keys()
 * names, which the text lists; '#' comment headers are annotations NUMERIC
 * does not govern).
 */
std::string pot_digits_warning(const PotDigits &pd);

} // namespace core
} // namespace tadah

#endif // TADAH_CORE_NUMBER_FORMAT_H
