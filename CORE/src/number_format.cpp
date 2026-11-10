#include <tadah/core/number_format.h>
#include <tadah/core/context.h>

#include <algorithm>
#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <stdexcept>
#include <system_error>

namespace tadah {
namespace core {

namespace {

// Every integer of smaller magnitude is exactly representable (2^53).
constexpr double kExactIntLimit = 9007199254740992.0;

// Shortest round-trip text of v into buf; returns its length.
std::size_t roundtrip_chars(double v, char *buf, std::size_t cap) {
  // NaN fails the equality, +-inf fails the magnitude test.
  const bool plain_int = v == std::trunc(v) && std::fabs(v) < kExactIntLimit;
  const std::to_chars_result r =
      plain_int ? std::to_chars(buf, buf + cap, v, std::chars_format::fixed)
                : std::to_chars(buf, buf + cap, v, std::chars_format::general);
  if (r.ec != std::errc())
    throw std::logic_error("format_roundtrip: output buffer too small");
  return static_cast<std::size_t>(r.ptr - buf);
}

// "%.*g" text of v into buf; returns its length. cap >= kReformatBufferSize.
std::size_t sig_chars(double v, int digits, char *buf, std::size_t cap) {
  digits = std::clamp(digits, 1, kMaxSigDigits);
  const std::to_chars_result r =
      std::to_chars(buf, buf + cap, v, std::chars_format::general, digits);
  if (r.ec != std::errc())
    throw std::logic_error("format_sig: output buffer too small");
  return static_cast<std::size_t>(r.ptr - buf);
}

inline bool is_digit(char c) { return c >= '0' && c <= '9'; }

// sig_chars() for text that is read back: a finite v never renders to a
// literal that overflows. Only |v| >= 1e308 can round past DBL_MAX (e.g.
// DBL_MAX at 3 digits is "1.8e+308", at 16 "1.797693134862316e+308"), so
// every other value pays one comparison. Such a rendering is replaced by
// the 17-digit scientific text (which reads back as v) with its mantissa
// TRUNCATED to `digits` significant digits: truncation only shrinks the
// magnitude, so the result stays finite and keeps exactly the requested
// precision ("1.79e+308").
std::size_t sig_chars_finite(double v, int digits, char *buf,
                             std::size_t cap) {
  const std::size_t n = sig_chars(v, digits, buf, cap);
  if (!(std::fabs(v) >= 1e308) || !std::isfinite(v)) return n;
  double back = 0.0;
  if (parse_decimal_finite(std::string_view(buf, n), back)) return n;

  char sci[64];
  const std::to_chars_result r =
      std::to_chars(sci, sci + sizeof sci, v, std::chars_format::scientific,
                    kRoundTripDigits - 1);
  if (r.ec != std::errc())
    throw std::logic_error("format_sig: output buffer too small");
  const char *const exp = std::find(sci, r.ptr, 'e');
  const char *p = sci;
  std::size_t len = 0;
  if (*p == '-') buf[len++] = *p++;
  buf[len++] = *p++;                                 // leading digit
  const std::size_t keep = static_cast<std::size_t>(
      std::clamp(digits, 1, kMaxSigDigits) - 1);    // fractional digits
  if (p != exp && *p == '.') {
    const char *const frac = p + 1;
    const std::size_t avail = static_cast<std::size_t>(exp - frac);
    std::size_t k = std::min(keep, avail);
    while (k > 0 && frac[k - 1] == '0') --k;          // %g drops zeros
    if (k > 0) {
      buf[len++] = '.';
      std::memcpy(buf + len, frac, k);
      len += k;
    }
  }
  const std::size_t elen = static_cast<std::size_t>(r.ptr - exp);
  std::memcpy(buf + len, exp, elen);
  return len + elen;
}

} // namespace

std::string format_roundtrip(double v) {
  char buf[64];
  return std::string(buf, roundtrip_chars(v, buf, sizeof buf));
}

void append_roundtrip(std::string &out, double v) {
  char buf[64];
  out.append(buf, roundtrip_chars(v, buf, sizeof buf));
}

std::string format_sig(double v, int digits) {
  char buf[kReformatBufferSize];
  return std::string(buf, sig_chars(v, digits, buf, sizeof buf));
}

std::size_t format_sig_chars(double v, int digits, char *buf,
                             std::size_t cap) {
  if (cap < kReformatBufferSize)
    throw std::invalid_argument("format_sig_chars: buffer too small");
  return sig_chars_finite(v, digits, buf, cap);
}

bool is_integer_literal(std::string_view tok) {
  std::size_t i = 0;
  if (i < tok.size() && (tok[i] == '+' || tok[i] == '-')) ++i;
  if (i == tok.size()) return false;
  for (; i < tok.size(); ++i)
    if (!is_digit(tok[i])) return false;
  return true;
}

bool is_decimal_literal(std::string_view tok) {
  const char *const b = tok.data();
  const char *const e = b + tok.size();
  const char *p = b;
  if (p != e && (*p == '+' || *p == '-')) ++p;
  std::size_t mantissa_digits = 0;
  while (p != e && is_digit(*p)) { ++p; ++mantissa_digits; }
  if (p != e && *p == '.') {
    ++p;
    while (p != e && is_digit(*p)) { ++p; ++mantissa_digits; }
  }
  if (mantissa_digits == 0) return false;
  if (p != e && (*p == 'e' || *p == 'E')) {
    ++p;
    if (p != e && (*p == '+' || *p == '-')) ++p;
    std::size_t exp_digits = 0;
    while (p != e && is_digit(*p)) { ++p; ++exp_digits; }
    if (exp_digits == 0) return false;
  }
  return p == e;
}

namespace {

// conv (strtof, strtold) of the decimal literal `tok`, finite: an overflow
// is refused, an underflow (subnormal or +-0, |v| < 1) is the value, as
// for parse_decimal_finite(double). errno is left as it was.
template <class T, class Conv>
bool convert_decimal_finite(std::string_view tok, T &out, Conv conv) {
  char small[kReformatBufferSize];
  std::string large;
  const char *s = small;
  if (tok.size() < sizeof small) {
    std::memcpy(small, tok.data(), tok.size());
    small[tok.size()] = '\0';
  } else {
    large.assign(tok.data(), tok.size());
    s = large.c_str();
  }
  const int saved_errno = errno;
  errno = 0;
  char *end = nullptr;
  const T v = conv(s, &end);
  const bool range = errno == ERANGE;
  errno = saved_errno;
  if (end != s + tok.size()) return false;  // e.g. a non-"C" LC_NUMERIC
  if (range && !(std::fabs(v) < T(1))) return false;
  out = v;
  return true;
}

}  // namespace

bool parse_decimal_finite(std::string_view tok, float &out) {
  return is_decimal_literal(tok) &&
         convert_decimal_finite(tok, out, [](const char *s, char **end) {
           return std::strtof(s, end);
         });
}

bool parse_decimal_finite(std::string_view tok, long double &out) {
  return is_decimal_literal(tok) &&
         convert_decimal_finite(tok, out, [](const char *s, char **end) {
           return std::strtold(s, end);
         });
}

bool parse_decimal_finite(std::string_view tok, double &out) {
  if (!is_decimal_literal(tok)) return false;
  const char *const b = tok.data();
  const char *const e = b + tok.size();

#if defined(__cpp_lib_to_chars) && __cpp_lib_to_chars >= 201611L
  // Fast path (about 4x cheaper than strtod, locale-independent). It
  // reports under- and overflow as result_out_of_range without a value, so
  // those fall through to strtod below. from_chars rejects a leading '+'.
  {
    double v = 0.0;
    const std::from_chars_result r = std::from_chars(
        *b == '+' ? b + 1 : b, e, v, std::chars_format::general);
    if (r.ec == std::errc() && r.ptr == e) {
      out = v;
      return true;
    }
  }
#endif

  // strtod needs a NUL-terminated copy; tokens are short, so the stack
  // buffer covers them (a longer literal falls back to a heap string).
  // Also the portable path: floating-point std::from_chars is missing from
  // older standard libraries (libc++ < 20).
  char small[kReformatBufferSize];
  std::string large;
  const char *s = small;
  if (tok.size() < sizeof small) {
    std::memcpy(small, b, tok.size());
    small[tok.size()] = '\0';
  } else {
    large.assign(b, tok.size());
    s = large.c_str();
  }
  errno = 0;
  char *end = nullptr;
  const double v = std::strtod(s, &end);
  if (end != s + tok.size()) return false;  // e.g. a non-"C" LC_NUMERIC
  // ERANGE: an underflow (subnormal, or flushed to +-0; |v| < 1) is the
  // value the Context/config parsers (istringstream) read for this token;
  // an overflow (+-HUGE_VAL) is not a number they accept.
  if (errno == ERANGE && !(std::fabs(v) < 1.0)) return false;
  out = v;
  return true;
}

bool parse_decimal_normal(std::string_view tok, double &out) {
  double v = 0.0;
  if (!parse_decimal_finite(tok, v)) return false;
  std::uint64_t bits = 0;
  std::memcpy(&bits, &v, sizeof bits);
  const std::uint64_t magnitude = bits & UINT64_C(0x7fffffffffffffff);
  if (magnitude == 0) {
    // zero: exact only when every mantissa digit is 0, else an underflow
    for (const char ch : tok) {
      if (ch == 'e' || ch == 'E') break;
      if (ch >= '1' && ch <= '9') return false;
    }
  } else if ((magnitude >> 52) == 0) {
    return false;  // a subnormal
  }
  out = v;
  return true;
}

namespace {

// strtod over a NUL-terminated string with std::stod's acceptance, minus
// its underflow error. Returns false on no conversion or overflow; end_off
// receives the number of characters consumed. errno is left as it was.
bool strtod_lenient(const std::string &s, double &out,
                    std::size_t &end_off) noexcept {
  const char *const p = s.c_str();
  const int saved_errno = errno;
  errno = 0;
  char *end = nullptr;
  const double v = std::strtod(p, &end);
  const bool range = errno == ERANGE;
  errno = saved_errno;
  if (end == p) return false;
  // ERANGE: an underflow (|v| < 1: the subnormal or +-0, what istringstream
  // reads) is a value; an overflow (+-HUGE_VAL) is not.
  if (range && !(std::fabs(v) < 1.0)) return false;
  out = v;
  end_off = static_cast<std::size_t>(end - p);
  return true;
}

} // namespace

double to_double(const std::string &s, std::size_t *pos) {
  double v = 0.0;
  std::size_t used = 0;
  if (!strtod_lenient(s, v, used)) {
    // Tell the two std::stod failure kinds apart (same exception types).
    const char *const p = s.c_str();
    char *end = nullptr;
    const int saved_errno = errno;
    (void)std::strtod(p, &end);
    errno = saved_errno;
    if (end == p)
      throw std::invalid_argument("not a number: '" + s + "'");
    throw std::out_of_range("number out of range: '" + s + "'");
  }
  if (pos) *pos = used;
  return v;
}

bool parse_number(const std::string &tok, double &out) noexcept {
  double v = 0.0;
  std::size_t used = 0;
  if (!strtod_lenient(tok, v, used) || used != tok.size()) return false;
  out = v;
  return true;
}

std::size_t reformat_double_token(std::string_view tok, int digits,
                                  char *buf, std::size_t cap) {
  if (cap < kReformatBufferSize)
    throw std::invalid_argument("reformat_double_token: buffer too small");
  if (is_integer_literal(tok)) return 0;
  double v = 0.0;
  if (!parse_decimal_finite(tok, v)) return 0;
  return sig_chars_finite(v, digits, buf, cap);
}

std::string reformat_double_token(std::string_view tok, int digits) {
  char buf[kReformatBufferSize];
  const std::size_t n = reformat_double_token(tok, digits, buf, sizeof buf);
  return n ? std::string(buf, n) : std::string(tok);
}

std::size_t reformat_double_token_exact(std::string_view tok, char *buf,
                                        std::size_t cap) {
  if (cap < kReformatBufferSize)
    throw std::invalid_argument(
        "reformat_double_token_exact: buffer too small");
  if (is_integer_literal(tok)) return 0;
  double v = 0.0;
  if (!parse_decimal_finite(tok, v)) return 0;
  return roundtrip_chars(v, buf, cap);
}

std::vector<std::string> exact_in_potential_keys() {
  std::vector<std::string> keys;
  for (const auto &kv : BlueprintParser::getKeysMapRef())
    if (kv.second.exact_in_potential) keys.push_back(kv.first);
  std::sort(keys.begin(), keys.end());
  return keys;
}

PotDigits resolve_pot_digits(const Context &ctx) {
  const std::optional<std::string> s = ctx.stored_value("NUMERIC");
  if (!s) return PotDigits{};

  long long n = 0;
  const char *b = s->data();
  const char *const e = b + s->size();
  if (b != e && *b == '+') ++b;
  const std::from_chars_result r = std::from_chars(b, e, n);
  if (r.ec != std::errc() || r.ptr != e || b == e) {
    // Tolerate an integral decimal ("12.000") and saturate huge values.
    double v = 0.0;
    if (!parse_decimal_finite(*s, v) || v != std::floor(v))
      throw std::runtime_error(
          "NUMERIC must be an integer number of significant digits, got \"" +
          *s + "\"");
    n = v > kMaxSigDigits ? kMaxSigDigits : (v < 1.0 ? 1 : static_cast<long long>(v));
  }
  PotDigits pd;
  pd.digits = static_cast<int>(std::clamp<long long>(n, 1, kMaxSigDigits));
  pd.explicit_ = true;
  return pd;
}

std::string pot_digits_warning(const PotDigits &pd) {
  if (!pd.explicit_ || pd.digits >= kRoundTripDigits) return std::string();
  const std::string n = std::to_string(pd.digits);
  // "CGRID2B, CGRIDMB, ... and SGRIDMB": the keys written exactly anyway.
  const std::vector<std::string> exact = exact_in_potential_keys();
  std::string listed;
  for (std::size_t i = 0; i < exact.size(); ++i)
    listed += (i == 0 ? "" : i + 1 == exact.size() ? " and " : ", ") +
              exact[i];
  const std::string exact_clause =
      exact.empty() ? std::string()
                    : ", and so are the numbers of " + listed +
                          ", which a load checks exactly";
  return "NUMERIC " + n + " limits every non-integer number of the "
         "potential's key/value lines to " + n + " significant digits "
         "(integers are written exactly" + exact_clause + "): the other "
         "values do not round-trip exactly "
         "(a double needs " + std::to_string(kRoundTripDigits) +
         "), so a potential reloaded from this file differs from the one in "
         "memory. NUMERIC is never written into the potential: delete the "
         "NUMERIC line from the configuration that set it "
         "(default " + std::to_string(kPotDigitsDefault) +
         ", exact) or set NUMERIC >= " + std::to_string(kRoundTripDigits) +
         ". Values loaded into memory are never limited by NUMERIC.";
}

} // namespace core
} // namespace tadah
