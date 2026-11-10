#ifndef TADAH_MODELS_EAM_LATTICE_H
#define TADAH_MODELS_EAM_LATTICE_H

#include <tadah/core/number_format.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <functional>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace tadah {
namespace models {
namespace eam_lattice {

/** \brief Ideal-lattice neighbour distances for monatomic bcc/fcc/hcp crystals.
 *
 * Shared by the refit reachability analysis (which embedding densities / pair
 * distances a compressed crystal can actually visit) and the refit crystal
 * energy self-test. All atoms in these lattices are symmetry-equivalent, so
 * distances from a single basis atom describe every atom. hcp uses the ideal
 * c/a = sqrt(8/3); for energy *differences* between two potentials evaluated
 * on the same geometry this is exact, and for reachability it is a perfectly
 * adequate probe geometry.
 */

/// true if \p lattice is one of the supported monatomic lattices. Expects the
/// canonical lower-case form (EAMSetflIO normalises the header label on read).
inline bool supported(const std::string &lattice) {
  return lattice == "bcc" || lattice == "fcc" || lattice == "hcp";
}

/** Require a crystal the refit self-test can build: one of fcc/bcc/hcp and a
 *  positive lattice constant. Throws std::runtime_error with a fix hint
 *  otherwise. Case-insensitive (normalises \p lattice internally); \p
 *  lattice_for_msg is shown verbatim so the message names the user's own token. */
inline void require_crystal(const std::string &lattice, double a0) {
  std::string l = lattice;
  for (auto &ch : l) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  if (supported(l) && a0 > 0.0) return;
  throw std::runtime_error(
      "tadah refit: the EAM header does not specify a crystal usable for the "
      "faithfulness self-test (lattice='" + (lattice.empty() ? "<missing>" : lattice) +
      "', a0=" + tadah::core::format_roundtrip(a0) + "). The self-test needs "
      "one of fcc, bcc and hcp and a positive lattice constant, which the "
      "element line of the setfl file (`<Z> <mass> <a0> <lattice>`) gives: "
      "the ground-state lattice, fcc, bcc or hcp in place of a placeholder "
      "such as NONE, and the equilibrium lattice constant.");
}

/// nearest-neighbour distance for lattice constant \p a
inline double nn_distance(const std::string &lattice, double a) {
  if (lattice == "fcc") return a / std::sqrt(2.0);
  if (lattice == "hcp") return a;
  return a * std::sqrt(3.0) / 2.0;  // bcc (and fallback)
}

/** The nearest-neighbour distance of the crystal of lattice constant \p a0
 *  compressed to a/a0 = \p reach_smin (REFIT_REACH_SMIN): the shortest
 *  pair distance the refit's crystal sums visit, EAMRefit::r_reach(), which
 *  is computed by this function. \p lattice in the canonical lower case
 *  (as nn_distance). */
inline double reach_distance(const std::string &lattice, double a0,
                             double reach_smin) {
  return reach_smin * nn_distance(lattice, a0);
}

/** The crystal gate of `tadah refit`: require_crystal(); the crystal of
 *  the header, at a0, must have its nearest neighbours below the table's
 *  cutoff \p rcut (nn_distance() < rcut: LAMMPS, pair_style eam/alloy,
 *  counts a pair only below rcut, so in LAMMPS a crystal whose nearest
 *  neighbours are at or beyond it has no interaction -- a lattice constant
 *  written in picometres rather than Angstrom, for one; tadah's own
 *  neighbour_distances() and D2_EAM/DM_EAM keep a pair at exactly rcut);
 *  and the crystal at the strongest
 *  compression the refit tests, a/a0 = \p reach_smin (REFIT_REACH_SMIN:
 *  the self-test's E-V scan and the reachability sweep start there), must
 *  have its nearest neighbours at least the step \p dr of the table's
 *  radial grid apart (reach_distance() >= dr). Closer, the table,
 *  tabulated at r = 0, dr, 2 dr, ..., describes no pair distance of that
 *  crystal: a lattice constant written in metres rather than Angstrom, or
 *  a subnormal one (whose distance underflows to 0). So dr <=
 *  EAMRefit::r_reach() < rcut for every table it passes. Case-insensitive
 *  like require_crystal; throws std::runtime_error naming a0, the lattice,
 *  the distance and rcut, or REFIT_REACH_SMIN and dr (each as the shortest
 *  text that reads back to it). */
inline void require_crystal_on_grid(const std::string &lattice, double a0,
                                    double reach_smin, double dr,
                                    double rcut) {
  require_crystal(lattice, a0);
  std::string l = lattice;
  for (auto &ch : l) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  const double nn = nn_distance(l, a0);
  if (!(nn < rcut))
    throw std::runtime_error(
        "tadah refit: the lattice constant of the EAM header, a0 = " +
        core::format_roundtrip(a0) + " A, puts the nearest neighbours of the " +
        l + " crystal " + core::format_roundtrip(nn) + " A apart, at or "
        "beyond the table's cutoff, rcut = " + core::format_roundtrip(rcut) +
        " A, and LAMMPS (pair_style eam/alloy) counts a pair only below "
        "rcut: the crystal has no interaction in LAMMPS. The a0 of the "
        "element line of the setfl file (`<Z> <mass> <a0> <lattice>`) is "
        "the equilibrium lattice constant in Angstrom (a value in "
        "picometres is 100 times the value in Angstrom).");
  const double r = reach_distance(l, a0, reach_smin);
  if (r >= dr) return;
  throw std::runtime_error(
      "tadah refit: the lattice constant of the EAM header, a0 = " +
      core::format_roundtrip(a0) + " A, puts the nearest neighbours of the " +
      l + " crystal " + core::format_roundtrip(r) + " A apart at the "
      "strongest compression the refit tests (a/a0 = REFIT_REACH_SMIN " +
      core::format_roundtrip(reach_smin) + "), less than the step of the "
      "table's radial grid, dr = " + core::format_roundtrip(dr) + " A: the "
      "table describes no pair distance of that crystal. The a0 of the "
      "element line of the setfl file (`<Z> <mass> <a0> <lattice>`) is the "
      "equilibrium lattice constant in Angstrom.");
}

/** All neighbour distances (one central atom) up to \p rcut, d <= rcut (a
 *  shell at exactly rcut included, as D2_EAM and DM_EAM count it), for
 *  lattice constant \p a. Unknown lattice falls back to bcc (caller should
 *  report).
 *  The lattice points of a cube of ceil(rcut / e) + 1 cells each way (e the
 *  cell's shortest edge: a for bcc and fcc) are visited with an int index:
 *  rcut / e must be finite and at most 2147483645 (INT_MAX - 2), else
 *  std::invalid_argument naming the lattice, a, rcut and rcut / e (the cost
 *  grows as (rcut / e)^3: the crystal gate of `tadah refit`,
 *  require_crystal_on_grid, and the setfl reader's grid rules bound it for
 *  the refit's crystals). */
inline std::vector<double> neighbour_distances(const std::string &lattice,
                                               double a, double rcut) {
  // lattice vectors (rows) and fractional basis
  double A[3][3];
  std::vector<std::array<double, 3>> basis;
  if (lattice == "fcc") {
    A[0][0] = a; A[0][1] = 0; A[0][2] = 0;
    A[1][0] = 0; A[1][1] = a; A[1][2] = 0;
    A[2][0] = 0; A[2][1] = 0; A[2][2] = a;
    basis = {{0, 0, 0}, {0.5, 0.5, 0}, {0.5, 0, 0.5}, {0, 0.5, 0.5}};
  } else if (lattice == "hcp") {
    const double c = a * std::sqrt(8.0 / 3.0);
    A[0][0] = a;       A[0][1] = 0;                  A[0][2] = 0;
    A[1][0] = -0.5 * a; A[1][1] = a * std::sqrt(3.0) / 2.0; A[1][2] = 0;
    A[2][0] = 0;       A[2][1] = 0;                  A[2][2] = c;
    basis = {{0, 0, 0}, {1.0 / 3.0, 2.0 / 3.0, 0.5}};
  } else {  // bcc + fallback
    A[0][0] = a; A[0][1] = 0; A[0][2] = 0;
    A[1][0] = 0; A[1][1] = a; A[1][2] = 0;
    A[2][0] = 0; A[2][1] = 0; A[2][2] = a;
    basis = {{0, 0, 0}, {0.5, 0.5, 0.5}};
  }
  const double amin = std::min({std::fabs(A[0][0]), std::fabs(A[1][1]),
                                std::fabs(A[2][2])});
  // The cube's half-width, ceil(rcut / amin) + 1, and the loops' ++i past
  // it must fit an int.
  constexpr double kMaxSpan = std::numeric_limits<int>::max() - 2.0;
  const double span = rcut / amin;
  if (!(span <= kMaxSpan))
    throw std::invalid_argument(
        "eam_lattice::neighbour_distances: the neighbours of the " + lattice +
        " lattice of lattice constant a = " + core::format_roundtrip(a) +
        " within r = " + core::format_roundtrip(rcut) + " lie up to " +
        core::format_roundtrip(span) + " lattice vectors away, more than "
        "the enumeration's int index reaches (" +
        core::format_roundtrip(kMaxSpan) + ")");
  const int N = static_cast<int>(std::ceil(span)) + 1;
  std::vector<double> out;
  for (int i = -N; i <= N; ++i)
    for (int j = -N; j <= N; ++j)
      for (int k = -N; k <= N; ++k)
        for (const auto &b : basis) {
          const double fx = i + b[0], fy = j + b[1], fz = k + b[2];
          const double x = fx * A[0][0] + fy * A[1][0] + fz * A[2][0];
          const double y = fx * A[0][1] + fy * A[1][1] + fz * A[2][1];
          const double z = fx * A[0][2] + fy * A[1][2] + fz * A[2][2];
          const double d = std::sqrt(x * x + y * y + z * z);
          if (d > 1e-9 && d <= rcut) out.push_back(d);
        }
  return out;
}

/** \brief One neighbour shell of a monatomic lattice: distance in units of
 *  the lattice constant, and the number of neighbours at that distance. */
struct Shell {
  double d;  ///< shell radius / a
  int m;     ///< multiplicity
};

/** Neighbour shells (d, m) of a monatomic bcc/fcc/hcp crystal with
 *  d <= @p dmax (d in units of the lattice constant a; hcp: a is the
 *  in-plane constant, ideal c/a = sqrt(8/3)), sorted by ascending d.
 *
 *  bcc and fcc are grouped by the exact integer n4 = |2R/a|^2, with
 *  d = sqrt(n4)/2 and n4 <= 4 dmax^2, so every shell radius is one exactly
 *  rounded square root. hcp groups the sorted neighbour_distances(hcp, 1,
 *  dmax) at a relative tolerance of 1e-12 and represents a shell by its
 *  smallest member (equivalent neighbours of the hcp lattice differ by an
 *  ulp). The lattice name is case-insensitive; anything other than
 *  bcc/fcc/hcp, or a non-finite dmax, throws.
 *
 *  Cost: the lattice points of the cube |i|, |j|, |k| <= dmax + 2 are
 *  visited, O(dmax^3) time. bcc/fcc count the multiplicities per n4
 *  (memory O(dmax^2)); hcp stores every distance within dmax (O(dmax^3)).
 *  Callers bound dmax (HPO RHOBARGATE: at most 20 lattice constants). */
inline std::vector<Shell> shells(const std::string &lattice, double dmax) {
  std::string lat = lattice;
  for (auto &ch : lat)
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  if (!supported(lat) || !std::isfinite(dmax))
    throw std::runtime_error(
        "eam_lattice::shells: needs lattice bcc, fcc or hcp and a finite "
        "radius (got lattice '" + lattice + "', dmax " +
        std::to_string(dmax) + ").");
  std::vector<Shell> out;
  if (!(dmax > 0.0)) return out;
  if (lat == "hcp") {
    std::vector<double> d = neighbour_distances(lat, 1.0, dmax);
    std::sort(d.begin(), d.end());
    for (const double x : d) {
      if (!out.empty() && x - out.back().d <= 1e-12 * out.back().d)
        ++out.back().m;
      else
        out.push_back({x, 1});
    }
    return out;
  }
  // bcc/fcc: 2R/a = (2i + bx, 2j + by, 2k + bz) with the doubled basis
  // offsets, so n4 = |2R/a|^2 is an exact integer label of the shell; the
  // points are counted per n4, which also sorts the shells.
  const std::vector<std::array<int, 3>> basis =
      lat == "fcc"
          ? std::vector<std::array<int, 3>>{{0, 0, 0}, {1, 1, 0},
                                            {1, 0, 1}, {0, 1, 1}}
          : std::vector<std::array<int, 3>>{{0, 0, 0}, {1, 1, 1}};
  const long N = static_cast<long>(std::ceil(dmax)) + 1;
  const double n4max = 4.0 * dmax * dmax;
  std::vector<int> count(static_cast<std::size_t>(n4max) + 1, 0);
  for (long i = -N; i <= N; ++i)
    for (long j = -N; j <= N; ++j)
      for (long k = -N; k <= N; ++k)
        for (const auto &b : basis) {
          const long x = 2 * i + b[0], y = 2 * j + b[1], z = 2 * k + b[2];
          const long n4 = x * x + y * y + z * z;
          if (n4 == 0 || static_cast<double>(n4) > n4max) continue;
          ++count[static_cast<std::size_t>(n4)];
        }
  for (std::size_t n4 = 1; n4 < count.size(); ++n4)
    if (count[n4] > 0)
      out.push_back({std::sqrt(static_cast<double>(n4)) / 2.0, count[n4]});
  return out;
}

/** Energy per atom of a monatomic bcc/fcc/hcp crystal at lattice constant
 *  \p a by a direct pair/density sum within \p rcut. NaN for an unsupported
 *  lattice. `phi`,`rho`,`F` may be tabulated originals or fitted functions —
 *  shared by the refit self-test and the compact-mode energy metric. */
inline double crystal_epa(const std::string &lat, double a, double rcut,
                          const std::function<double(double)> &phi,
                          const std::function<double(double)> &rho,
                          const std::function<double(double)> &F) {
  if (!supported(lat)) return std::nan("");
  const auto dists = neighbour_distances(lat, a, rcut);
  double half_pair = 0.0, dens = 0.0;  // sums seen by one (any) atom
  for (double r : dists) {
    half_pair += 0.5 * phi(r);
    dens += rho(r);
  }
  return half_pair + F(dens);  // monatomic: every atom is equivalent
}

}  // namespace eam_lattice
}  // namespace models
}  // namespace tadah
#endif  // TADAH_MODELS_EAM_LATTICE_H
