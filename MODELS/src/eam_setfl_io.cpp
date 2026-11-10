#include <tadah/models/eam/eam_setfl_io.h>

#include <tadah/core/number_format.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <map>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <vector>

namespace tadah {
namespace models {

namespace {

/* The bytes of the file @p path; throws the reader's message when it
 * cannot be opened. */
std::string setfl_bytes(const std::string &path) {
  std::ifstream in(path, std::ios::binary);
  if (!in.good())
    throw std::runtime_error("EAMSetflIO: setfl file does not exist: " + path);
  std::ostringstream bytes;
  bytes << in.rdbuf();
  if (in.bad())
    throw std::runtime_error("EAMSetflIO: cannot read setfl file: " + path);
  return bytes.str();
}

}  // namespace

void EAMSetflIO::read_setfl(const std::string &path) {
  read_setfl_text(setfl_bytes(path), path);
}

namespace {

// The smallest table a setfl file may have: the cubic spline that LAMMPS
// (PairEAM::interpolate) and tadah (gen_spline) fit through a table reads
// its third point. scaled_grid() keeps the same floor.
constexpr int kMinSetflPoints = 3;
const char *const kSplineReason =
    " (the cubic spline that LAMMPS and tadah fit through a table reads its "
    "third point)";

// LAMMPS (pair_style eam/alloy, PairEAM::compute) and tadah (eval_value,
// D2_EAM, DM_EAM) evaluate a distance r below rcut at the spline index
// p = r / dr + 1, converted to int and clamped to the last interval of the
// table, whose points are r = 0, dr, ..., (nr - 1) dr: every r past the
// last point takes the table's last value. Setfl files put the cutoff at
// the last point, before it (the tables reach past rcut) or one step past
// it (rcut = nr x dr, the functions zero there); a cutoff further past the
// last point leaves the tables short of it. nr x dr may fall below rcut by
// this fraction of rcut: the rounding of decimals printed to 6 significant
// digits (C's %g rounds dr and rcut by up to 5e-6 each, relative) and more.
constexpr double kSetflCutoffTolerance = 1e-4;

/* The lines of a setfl text, read as LAMMPS pair eam/alloy reads them
 * (TextFileReader): lines 1-3 whole, every later line with its '#' comment
 * cut and split into words at whitespace ('\r' included), a line without
 * words skipped. Errors name the file and the line. */
class SetflLines {
public:
  SetflLines(const std::string &text, const std::string &path)
      : text_(text), path_(path) {}

  /* The next line, whole; false at the end of the text. */
  bool raw_line(std::string_view &out) {
    if (pos_ >= text_.size()) return false;
    const std::size_t nl = text_.find('\n', pos_);
    const std::size_t stop = nl == std::string::npos ? text_.size() : nl;
    out = std::string_view(text_).substr(pos_, stop - pos_);
    pos_ = nl == std::string::npos ? text_.size() : nl + 1;
    ++line_;
    return true;
  }

  /* The words of the next line that has any; false at the end. */
  bool next_words(std::vector<std::string_view> &words) {
    std::string_view l;
    while (raw_line(l)) {
      const std::size_t hash = l.find('#');
      if (hash != std::string_view::npos) l = l.substr(0, hash);
      words.clear();
      std::size_t i = 0;
      while (i < l.size()) {
        while (i < l.size() && is_blank(l[i])) ++i;
        const std::size_t b = i;
        while (i < l.size() && !is_blank(l[i])) ++i;
        if (i > b) words.push_back(l.substr(b, i - b));
      }
      if (!words.empty()) return true;
    }
    return false;
  }

  /* A header record of exactly n words, read as LAMMPS' next_values(m)
   * reads one: lines joined until they hold m words or more, the words
   * LAMMPS reads; the record ends with the line that completes them (LAMMPS
   * drops the rest of that line, and its next read starts on the next
   * line). The grid line: m = n = 5. The element record 'Z mass a lattice':
   * m = 2 (Z and the mass), so a and lattice must be on the mass's line;
   * `short_reason` says so when the record that ends there is short. */
  void record(std::size_t n, std::size_t m, const std::string &what,
              const std::string &fields,
              std::vector<std::string_view> &words,
              const std::string &short_reason = std::string()) {
    words.clear();
    std::vector<std::string_view> more;
    std::size_t first = 0;
    while (words.size() < m) {
      if (!next_words(more)) {
        if (words.empty())
          throw std::runtime_error(file() + ": the file ends before the " +
                                   what + " ('" + fields + "')");
        throw std::runtime_error(
            file() + ": the file ends inside the " + what + " ('" + fields +
            "'), which has " + std::to_string(words.size()) + " of its " +
            std::to_string(n) + " words");
      }
      if (words.empty()) first = line_;
      words.insert(words.end(), more.begin(), more.end());
    }
    if (words.size() == n) return;
    if (words.size() < n && !more_words_follow())
      throw std::runtime_error(
          file() + ": the file ends inside the " + what + " ('" + fields +
          "'), which has " + std::to_string(words.size()) + " of its " +
          std::to_string(n) + " words");
    throw std::runtime_error(
        at(first) + "the " + what + " is '" + fields + "', it has " +
        std::to_string(words.size()) + " words" +
        (line_ == first ? std::string()
                        : " on lines " + std::to_string(first) + "-" +
                              std::to_string(line_)) +
        (words.size() < n ? short_reason : std::string()));
  }

  /* Whether a line with words follows (the position is kept). */
  bool more_words_follow() {
    const std::size_t pos = pos_, line = line_;
    std::vector<std::string_view> rest;
    const bool more = next_words(rest);
    pos_ = pos;
    line_ = line;
    return more;
  }

  /* A table of n finite values, `name` (`count` = n), from the next line
   * with words on: it ends at the end of a line (LAMMPS' next_dvector reads
   * a table's last line and drops the words after its last value, so a
   * table that ends inside a line would be another potential in LAMMPS). */
  std::vector<double> table(int n, const std::string &name,
                            const std::string &count) {
    std::vector<double> values;
    // Each value takes two characters or more: an absurd n is not
    // allocated before the text runs out.
    values.reserve(std::min<std::size_t>(static_cast<std::size_t>(n),
                                         text_.size() / 2 + 1));
    std::vector<std::string_view> words;
    while (values.size() < static_cast<std::size_t>(n)) {
      if (!next_words(words))
        throw std::runtime_error(
            file() + ": the file ends inside the " + name + " table, after " +
            std::to_string(values.size()) + " of its " + count + " = " +
            std::to_string(n) + " values");
      for (std::size_t k = 0; k < words.size(); ++k) {
        if (values.size() == static_cast<std::size_t>(n))
          throw std::runtime_error(
              at(line_) + "the " + name + " table of " + count + " = " +
              std::to_string(n) + " values ends with word " +
              std::to_string(k) + " of this line, which has " +
              std::to_string(words.size()) +
              ": each table starts on a new line (LAMMPS drops the rest of "
              "a table's last line)");
        double v = 0.0;
        if (!core::parse_decimal_finite(words[k], v))
          throw std::runtime_error(
              at(line_) + "value " + std::to_string(values.size() + 1) +
              " of the " + name + " table, '" + std::string(words[k]) +
              "', is not a finite number");
        values.push_back(v);
      }
    }
    return values;
  }

  std::size_t line() const { return line_; }
  std::string file() const {
    return "EAMSetflIO: setfl file '" + path_ + "'";
  }
  std::string at(std::size_t line) const {
    return file() + ", line " + std::to_string(line) + ": ";
  }

private:
  static bool is_blank(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\v' || c == '\f';
  }

  const std::string &text_;
  const std::string &path_;
  std::size_t pos_ = 0;
  std::size_t line_ = 0;
};

/* The whole token as an int ([+-]digits in the range of int). */
bool parse_int_token(std::string_view tok, int &out) {
  if (!core::is_integer_literal(tok)) return false;
  if (!tok.empty() && tok.front() == '+') tok.remove_prefix(1);
  int v = 0;
  const auto r = std::from_chars(tok.data(), tok.data() + tok.size(), v);
  if (r.ec != std::errc() || r.ptr != tok.data() + tok.size()) return false;
  out = v;
  return true;
}

}  // namespace

void EAMSetflIO::read_setfl_text(const std::string &text,
                                 const std::string &path) {
  // Parsed into a fresh table: a refused file leaves this one as it was.
  EAMSetflIO t;
  t.file_path_ = path;
  SetflLines in(text, path);
  std::vector<std::string_view> w;

  // Lines 1-3: comments, whatever they hold.
  std::string_view line3;
  for (int i = 0; i < 3; ++i)
    if (!in.raw_line(line3)) line3 = std::string_view();

  // The element line: <ntypes> <El1> [<El2> ...].
  if (!in.next_words(w))
    throw std::runtime_error(in.file() +
                             ": the file ends before the element line (the "
                             "number of element types and their names)");
  const std::size_t element_line = in.line();
  int ntypes = 0;
  if (!parse_int_token(w[0], ntypes) || ntypes <= 0) {
    // DYNAMO funcfl heuristic: its line 3 is "nrho drho nr dr rcut"
    // (int double int double double) where setfl has a comment.
    std::istringstream f3{std::string(line3)};
    int fn1 = 0, fn2 = 0; double fd1 = 0, fd2 = 0, fd3 = 0;
    if (f3 >> fn1 >> fd1 >> fn2 >> fd2 >> fd3)
      throw std::runtime_error(
          "EAMSetflIO: '" + path + "' looks like a DYNAMO funcfl file "
          "(pair_style eam). Only setfl files (eam/alloy or eam/fs) are "
          "supported in this version; convert the potential to setfl or "
          "obtain the setfl distribution.");
    throw std::runtime_error(in.at(element_line) +
                             "the element line must start with the number "
                             "of element types, got '" +
                             std::string(w[0]) + "'");
  }
  if (ntypes != 1)
    throw std::runtime_error(
        "EAMSetflIO: only single-element setfl (eam/alloy or eam/fs) is "
        "supported in this version; '" + path + "' declares " +
        std::to_string(ntypes) + " element types. Multi-element refit is a "
        "planned extension.");
  if (w.size() != 2)
    throw std::runtime_error(
        in.at(element_line) +
        "the element line of a file of 1 element type is '1 <symbol>', it "
        "has " + std::to_string(w.size()) +
        (w.size() == 1 ? " word" : " words"));
  t.element_ = std::string(w[1]);

  // The grid: nrho drho nr dr rcut.
  in.record(5, 5, "grid line", "nrho drho nr dr rcut", w);
  const std::string grid_at = in.at(in.line());
  auto count = [&](std::string_view tok, const char *name) {
    int n = 0;
    if (!parse_int_token(tok, n) || n < kMinSetflPoints)
      throw std::runtime_error(grid_at + name +
                               " must be a whole number from 3 to "
                               "2147483647" + kSplineReason + ", got '" +
                               std::string(tok) + "'");
    return n;
  };
  auto positive = [&](std::string_view tok, const char *name) {
    double v = 0.0;
    if (!core::parse_decimal_finite(tok, v) || !(v > 0.0))
      throw std::runtime_error(grid_at + name +
                               " must be a positive finite number, got '" +
                               std::string(tok) + "'");
    return v;
  };
  t.nrho_ = count(w[0], "nrho");
  t.drho_ = positive(w[1], "drho");
  t.nr_   = count(w[2], "nr");
  t.dr_   = positive(w[3], "dr");
  t.rcut_ = positive(w[4], "rcut");

  // The r tables must reach the cutoff (kSetflCutoffTolerance), and the
  // spline index of a distance below rcut must be an int: what LAMMPS reads
  // otherwise is not the potential (the tables' last values up to rcut; an
  // undefined conversion).
  using core::format_roundtrip;
  const std::string fix =
      " Correct nr, dr or rcut on the grid line ('nrho drho nr dr rcut').";
  const double last = static_cast<double>(t.nr_ - 1) * t.dr_;
  const double reach = static_cast<double>(t.nr_) * t.dr_;
  if (!(reach >= t.rcut_ * (1.0 - kSetflCutoffTolerance)))
    throw std::runtime_error(
        grid_at + "the r tables do not reach the cutoff: nr = " +
        std::to_string(t.nr_) + " points dr = " + format_roundtrip(t.dr_) +
        " apart end at r = (nr - 1) x dr = " + format_roundtrip(last) +
        ", and rcut = " + format_roundtrip(t.rcut_) + " lies more than one "
        "step past their last point (nr x dr = " + format_roundtrip(reach) +
        "); LAMMPS (pair_style eam/alloy) and tadah would take each table's "
        "last value for every r from " + format_roundtrip(last) + " to " +
        format_roundtrip(t.rcut_) + "." + fix);
  const double index = t.rcut_ / t.dr_;
  if (!(index <= static_cast<double>(std::numeric_limits<int>::max())))
    throw std::runtime_error(
        grid_at + "rcut / dr = " + format_roundtrip(index) + " is above " +
        std::to_string(std::numeric_limits<int>::max()) + ": the spline index "
        "of a distance r below rcut, r / dr + 1, which LAMMPS (pair_style "
        "eam/alloy) and tadah compute as an int, would not fit one." + fix);

  // The element record: Z mass a lattice, read as LAMMPS' pair eam/alloy
  // reads it (next_values(2): Z and the mass, then the F(rho) table from
  // the next line), so a and lattice are on the line of the mass.
  in.record(4, 2, "element record", "Z mass a lattice", w,
            ": LAMMPS (pair eam/alloy) reads the record to the end of the "
            "line of its mass, and the F(rho) table from the next line");
  const std::string record_at = in.at(in.line());
  if (!parse_int_token(w[0], t.atomic_number_))
    throw std::runtime_error(record_at +
                             "the atomic number must be a whole number, "
                             "got '" + std::string(w[0]) + "'");
  auto finite = [&](std::string_view tok, const char *name) {
    double v = 0.0;
    if (!core::parse_decimal_finite(tok, v))
      throw std::runtime_error(record_at + name +
                               " must be a finite number, got '" +
                               std::string(tok) + "'");
    return v;
  };
  t.atomic_mass_   = finite(w[1], "the mass");
  t.lattice_param_ = finite(w[2], "the lattice constant");
  t.lattice_       = std::string(w[3]);

  // The setfl lattice label is an informational field with no fixed case
  // convention — real files use both "fcc" and "FCC". Keep the raw token for
  // faithful messages / round-trip output, and normalise to lower case so the
  // crystal routines (eam_lattice::supported / crystal_epa) recognise it.
  t.lattice_raw_ = t.lattice_;
  for (auto &ch : t.lattice_)
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));

  // The tables; the lines after the z2r table are not read (nor are they
  // by LAMMPS).
  t.frho_ = in.table(t.nrho_, "F(rho)", "nrho");
  t.rhor_ = in.table(t.nr_, "rho(r)", "nr");
  t.z2r_  = in.table(t.nr_, "z2r(r)", "nr");

  t.rdr_   = 1.0 / t.dr_;
  t.rdrho_ = 1.0 / t.drho_;
  // LAMMPS convention: the embedding spline is valid up to (nrho-1)*drho.
  t.rhomax_ = (t.nrho_ - 1) * t.drho_;
  *this = std::move(t);
}

// Natural cubic spline, identical to the former D2_EAM/DM_EAM gen_splines.
void EAMSetflIO::gen_spline(int n, double delta,
                            const std::vector<double> &f,
                            std::vector<std::vector<double>> &spline) {
  // in lammps f is n+1, here is size n
  for (int m = 1; m <= n; m++) spline[m][6] = f[m - 1];

  spline[1][5]   = spline[2][6] - spline[1][6];
  spline[2][5]   = 0.5 * (spline[3][6] - spline[1][6]);
  spline[n - 1][5] = 0.5 * (spline[n][6] - spline[n - 2][6]);
  spline[n][5]   = spline[n][6] - spline[n - 1][6];

  for (int m = 3; m <= n - 2; m++)
    spline[m][5] = ((spline[m - 2][6] - spline[m + 2][6]) +
                    8.0 * (spline[m + 1][6] - spline[m - 1][6])) / 12.0;

  for (int m = 1; m <= n - 1; m++) {
    spline[m][4] = 3.0 * (spline[m + 1][6] - spline[m][6]) - 2.0 * spline[m][5] - spline[m + 1][5];
    spline[m][3] = spline[m][5] + spline[m + 1][5] - 2.0 * (spline[m + 1][6] - spline[m][6]);
  }

  spline[n][4] = 0.0;
  spline[n][3] = 0.0;

  for (int m = 1; m <= n; m++) {
    spline[m][2] = spline[m][5] / delta;
    spline[m][1] = 2.0 * spline[m][4] / delta;
    spline[m][0] = 3.0 * spline[m][3] / delta;
  }
}

void EAMSetflIO::build_splines() {
  frho_spline_.assign(nrho_ + 1, std::vector<double>(7, 0.0));
  rhor_spline_.assign(nr_ + 1, std::vector<double>(7, 0.0));
  z2r_spline_.assign(nr_ + 1, std::vector<double>(7, 0.0));
  gen_spline(nrho_, drho_, frho_, frho_spline_);
  gen_spline(nr_,   dr_,   rhor_, rhor_spline_);
  gen_spline(nr_,   dr_,   z2r_,  z2r_spline_);
}

double EAMSetflIO::eval_value(const std::vector<std::vector<double>> &spline,
                              double x, double rdx, int n) {
  double p = x * rdx + 1.0;
  int m = static_cast<int>(p);
  if (m < 1) m = 1;
  if (m > n - 1) m = n - 1;
  p -= m;
  if (p > 1.0) p = 1.0;
  const std::vector<double> &c = spline[m];
  return ((c[3] * p + c[4]) * p + c[5]) * p + c[6];
}

// Analytic d(value)/dx: value = c3 p^3 + c4 p^2 + c5 p + c6 with p = x*rdx - m,
// so d/dx = rdx*(3 c3 p^2 + 2 c4 p + c5) = (c[0] p + c[1]) p + c[2] using the
// stored deriv coeffs (c[0]=3c3/dx, c[1]=2c4/dx, c[2]=c5/dx). d^2/dx^2 =
// rdx*(2 c[0] p + c[1]).
double EAMSetflIO::eval_deriv(const std::vector<std::vector<double>> &spline,
                             double x, double rdx, int n) {
  double p = x * rdx + 1.0;
  int m = static_cast<int>(p);
  if (m < 1) m = 1;
  if (m > n - 1) m = n - 1;
  p -= m;
  if (p > 1.0) p = 1.0;
  const std::vector<double> &c = spline[m];
  return (c[0] * p + c[1]) * p + c[2];
}

double EAMSetflIO::eval_deriv2(const std::vector<std::vector<double>> &spline,
                              double x, double rdx, int n) {
  double p = x * rdx + 1.0;
  int m = static_cast<int>(p);
  if (m < 1) m = 1;
  if (m > n - 1) m = n - 1;
  p -= m;
  if (p > 1.0) p = 1.0;
  const std::vector<double> &c = spline[m];
  return (2.0 * c[0] * p + c[1]) * rdx;
}

double EAMSetflIO::phi_tab(double r) const {
  return eval_value(z2r_spline_, r, rdr_, nr_) / r;
}

double EAMSetflIO::z2_tab(double r) const {
  return eval_value(z2r_spline_, r, rdr_, nr_);
}

double EAMSetflIO::rho_tab(double r) const {
  return eval_value(rhor_spline_, r, rdr_, nr_);
}

double EAMSetflIO::F_tab(double rho) const {
  return eval_value(frho_spline_, rho, rdrho_, nrho_);
}

// phi(r) = z2r(r)/r. Quotient rule with z = z2r, z' , z'':
//   phi'  = z'/r - z/r^2
//   phi'' = z''/r - 2 z'/r^2 + 2 z/r^3
double EAMSetflIO::dphi_tab(double r) const {
  const double z = eval_value(z2r_spline_, r, rdr_, nr_);
  const double z1 = eval_deriv(z2r_spline_, r, rdr_, nr_);
  return z1 / r - z / (r * r);
}

double EAMSetflIO::d2phi_tab(double r) const {
  const double z = eval_value(z2r_spline_, r, rdr_, nr_);
  const double z1 = eval_deriv(z2r_spline_, r, rdr_, nr_);
  const double z2 = eval_deriv2(z2r_spline_, r, rdr_, nr_);
  return z2 / r - 2.0 * z1 / (r * r) + 2.0 * z / (r * r * r);
}

double EAMSetflIO::drho_tab(double r) const {
  return eval_deriv(rhor_spline_, r, rdr_, nr_);
}

double EAMSetflIO::d2rho_tab(double r) const {
  return eval_deriv2(rhor_spline_, r, rdr_, nr_);
}

double EAMSetflIO::dF_tab(double rho) const {
  return eval_deriv(frho_spline_, rho, rdrho_, nrho_);
}

double EAMSetflIO::d2F_tab(double rho) const {
  return eval_deriv2(frho_spline_, rho, rdrho_, nrho_);
}

EAMGrid EAMSetflIO::scaled_grid(double scale) const {
  if (!(scale > 0.0) || !(scale <= std::numeric_limits<double>::max()))
    throw std::invalid_argument(
        "EAMSetflIO: a grid scale must be a positive finite number, got " +
        core::format_roundtrip(scale));
  // The counts in double: n * scale may exceed an int (and a long).
  const double nr = std::round(nr_ * scale);
  const double nrho = std::round(nrho_ * scale);
  // The spline LAMMPS and tadah fit through a table (gen_spline, PairEAM's
  // interpolate) reads its third point: a table has at least 3.
  constexpr double kMinPoints = kMinSetflPoints;
  constexpr double kMaxPoints = std::numeric_limits<int>::max();
  if (!(nr >= kMinPoints && nr <= kMaxPoints && nrho >= kMinPoints &&
        nrho <= kMaxPoints))
    throw std::invalid_argument(
        "EAMSetflIO: a grid scale of " + core::format_roundtrip(scale) +
        " gives nrho = " + core::format_roundtrip(nrho) +
        " and nr = " + core::format_roundtrip(nr) +
        " points (the input table has " + std::to_string(nrho_) + " and " +
        std::to_string(nr_) +
        "); a setfl table needs 3 to 2147483647 points (the spline that "
        "LAMMPS and tadah fit through it reads its third point)");
  EAMGrid g;
  g.nr   = static_cast<int>(nr);
  g.nrho = static_cast<int>(nrho);
  // Preserve the physical spans (nr-1)*dr and (nrho-1)*drho. A count equal
  // to the input's keeps the input's step itself (d * (n-1) / (n-1) need
  // not be d in double), so a scale whose counts round to the input's gives
  // the input grid. A new r count keeps the span unless that leaves rcut
  // more than one of the new steps past the new last point (an input whose
  // cutoff is one step past its last point, rcut = nr x dr, on a finer
  // grid): then the cutoff is one step past it, as in the input, dr =
  // rcut / nr, so the written table reaches the cutoff as read_setfl_text
  // requires.
  if (g.nr == nr_) {
    g.dr = dr_;
  } else {
    g.dr = dr_ * static_cast<double>(nr_ - 1) / static_cast<double>(g.nr - 1);
    if (static_cast<double>(g.nr) * g.dr < rcut_)
      g.dr = rcut_ / static_cast<double>(g.nr);
  }
  g.drho = g.nrho == nrho_
               ? drho_
               : drho_ * static_cast<double>(nrho_ - 1) /
                     static_cast<double>(g.nrho - 1);
  g.rcut = rcut_;
  return g;
}

void EAMSetflIO::write_setfl(const std::string &path,
                             const EAMGrid &grid,
                             const std::function<double(double)> &frho_fn,
                             const std::function<double(double)> &rhor_fn,
                             const std::function<double(double)> &z2r_fn,
                             const std::string &title) const {
  std::ofstream out(path);
  if (!out)
    throw std::runtime_error("EAMSetflIO: cannot open output setfl file: " + path);

  // 3 comment lines (LAMMPS skips exactly three) + element line.
  out << title << "\n";
  out << "Native tadah refit functions (NOT splines of the original table)\n";
  out << "source: " << (file_path_.empty() ? "-" : file_path_) << "\n";
  out << " 1 " << (element_.empty() ? "X" : element_) << "\n";

  out << std::scientific << std::setprecision(16);
  out << grid.nrho << "  " << grid.drho << "  " << grid.nr << "  " << grid.dr
      << "  " << grid.rcut << "\n";
  out << atomic_number_ << "  " << atomic_mass_ << "  " << lattice_param_ << "  "
      << (lattice_raw_.empty() ? "unknown" : lattice_raw_) << "\n";

  auto dump = [&out](int N, double spacing,
                     const std::function<double(double)> &fn) {
    for (int i = 0; i < N; ++i) {
      out << " " << fn(i * spacing);
      if ((i + 1) % 5 == 0) out << "\n";
    }
    if (N % 5 != 0) out << "\n";
  };

  dump(grid.nrho, grid.drho, frho_fn);  // F(rho)
  dump(grid.nr,   grid.dr,   rhor_fn);  // rho(r)
  dump(grid.nr,   grid.dr,   z2r_fn);   // z2r(r) = phi(r)*r
}

namespace {

struct SharedSetfl {
  std::string bytes;                          // the contents it was parsed from
  std::shared_ptr<const EAMSetflIO> table;
};

std::mutex &shared_setfl_mutex() {
  static std::mutex m;
  return m;
}

std::map<std::string, SharedSetfl> &shared_setfl_tables() {
  static std::map<std::string, SharedSetfl> tables;
  return tables;
}

}  // namespace

std::shared_ptr<const EAMSetflIO> shared_setfl(const std::string &path) {
  std::string bytes = setfl_bytes(path);
  // The path as given (path() of the table, and its messages) and the file
  // it names from this working directory: a relative path names another
  // file in another directory.
  std::error_code ec;
  const std::filesystem::path abs = std::filesystem::absolute(path, ec);
  if (ec)
    throw std::runtime_error("EAMSetflIO: cannot resolve the setfl path '" +
                             path + "': " + ec.message());
  const std::string key = abs.lexically_normal().string() + '\n' + path;

  std::lock_guard<std::mutex> lock(shared_setfl_mutex());
  SharedSetfl &entry = shared_setfl_tables()[key];
  if (entry.table && entry.bytes == bytes) return entry.table;
  auto table = std::make_shared<EAMSetflIO>();
  table->read_setfl_text(bytes, path);
  table->build_splines();
  entry.bytes = std::move(bytes);
  entry.table = std::move(table);
  return entry.table;
}

} // namespace models
} // namespace tadah
