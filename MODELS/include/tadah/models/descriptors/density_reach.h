#ifndef TADAH_MODELS_DESCRIPTORS_DENSITY_REACH_H
#define TADAH_MODELS_DESCRIPTORS_DENSITY_REACH_H

// The reach of a cutoff and of a radial density channel, and the OPTIM
// ranges of a cutoff and of a channel's centre and width that the seed and
// the data suggest.
//
// A descriptor sums over the neighbours within its cutoff: a cutoff below
// every neighbour distance of the data leaves it nothing to sum
// (cutoff_reach_check). A many-body density sums a radial function g(r) of
// each channel over those neighbours, so a channel is judged by what g gives
// at the neighbour distances of the data, not by where its centre lies: a
// Gaussian exp(-SGRIDMB r^2) centred at the origin reaches the first
// neighbour shell when its width SGRIDMB is small enough, and a blip centred
// below the nearest neighbour reaches it when its support does.
//
// One constant, kDensityReachFloor, is the value below which a radial
// function or a density counts as reaching nothing: the CGRIDMB checks of the
// densities (radial_reach_check), the rho-collapse checks of DM_mBlipEnv and
// DM_Blip, and the widths tadah analysis suggest_bounds proposes for a
// Gaussian or a blip at the origin (gaussian_width_suggestion,
// blip_width_suggestion) use it.
//
// tadah analysis suggest_bounds takes the neighbour statistics up to
// kCutoffSearchReach times the largest cutoff of the seed, so that the range
// of a cutoff (cutoff_suggestion) can reach past the seed's.
//
// A descriptor sums over the neighbours of the element pairs its TYPE2B or
// TYPEMB line names (LinePairs): its checks are given the statistics of
// those pairs (the pre-flight audit, tadah analysis check_bounds) and judge
// each element pair the line stands for on its own, a pair with "*" each
// element pair it matches, since a pair that the cutoff, or every channel
// of a density, does not reach adds nothing to the descriptor whatever the
// other pairs give (the line Ta Ta W W with a cutoff below every (Ta, Ta)
// distance is 0 for every Ta atom); its suggestions are given those of one
// of them at a time (tadah analysis suggest_bounds).
// Their messages name the pairs of the line when the view of the seed has
// them (SeedView::line_pairs()): "the smallest (Ta, Ta) neighbour distance
// of the dataset".

#include <string>

#include <tadah/core/stats/data_stats.h>
#include <tadah/models/descriptors/basis_kind.h>
#include <tadah/models/descriptors/param_meta.h>

namespace tadah {
namespace models {

/** \brief The value below which a radial function at a neighbour distance,
 *  or the density of an atom, counts as zero: 1e-6. */
inline constexpr double kDensityReachFloor = 1.0e-6;

/** \brief The factor by which tadah analysis suggest_bounds takes the
 *  neighbour statistics past the largest cutoff of the seed: 1.25. The
 *  suggested ranges come from the neighbour distances up to that radius, so
 *  the range of a cutoff can end past the seed's cutoff. */
inline constexpr double kCutoffSearchReach = 1.25;

/** \brief The suggestion of a parameter whose range the neighbour
 *  distances give, for data with no pair of atoms within the reach of the
 *  statistics: no OPTIM line (BoundSuggestion::no_row), its rationale
 *  saying so and naming the element pairs of the line of @p s ("no (W, W)
 *  pair of atoms of the data ..."; "no pair of atoms ..." for a view
 *  without them). */
BoundSuggestion no_pair_suggestion(const SeedView &s);

/** \brief The OPTIM range of a cutoff (RCUT2B, RCUTMB): from 1.05 times the
 *  second peak of the neighbour distances of the first element pair of
 *  @p d (their median without a second peak, or when that lies at or above
 *  their 99th percentile) to their 99th percentile, the distances taken up
 *  to the reach of the statistics (kCutoffSearchReach times the largest
 *  cutoff of the seed); no_pair_suggestion() without a pair. The rationale
 *  names the form, the element pair ("(Ta, Ta)") and the statistics. tadah
 *  analysis suggest_bounds gives it the statistics of one element pair of
 *  the descriptor's line at a time. */
BoundSuggestion cutoff_suggestion(const tadah::core::stats::DataStats &d,
                                  const SeedView &s, int arg_pos);

/** \brief The check of the cutoff @p key (RCUT2B, RCUTMB) of @p s, each
 *  element pair the line of @p s (SeedView::line_pairs()) stands for judged
 *  on its own by the statistics of @p d of that pair: a pair of two
 *  elements as the line writes it, a pair with "*" each element pair it
 *  matches, of the elements of the data (those @p d lists and those of its
 *  pairs) and those the line names: "Ta *" on Ta-W data stands for (Ta,
 *  Ta) and (Ta, W), "* *" for (Ta, Ta), (Ta, W) and (W, W), "Mo *" for
 *  (Mo, Mo), (Mo, Ta) and (Mo, W); so that two lines that give the
 *  descriptor the same pairs get the same verdict. It fails when the data have atoms and a
 *  pair has no neighbour distance within the cutoff (the cutoff is below
 *  its smallest one, or @p d has none of it). When no pair of the line has
 *  one, the descriptor sums over no neighbour, and the message names the
 *  key, its value, the smallest neighbour distance of the pairs of the
 *  line, and the pairs ("RCUT2B = 2.45 is below the smallest (Ta, Ta) or
 *  (W, W) neighbour distance of the dataset (2.5 Å): the descriptor sums
 *  over no neighbour and is 0 for every atom."); when some do, it names
 *  each pair that has none and says that these add nothing to the
 *  descriptor ("RCUT2B = 2.6 is below the smallest (Ta, Ta) neighbour
 *  distance of the dataset (2.86 Å): the (Ta, Ta) pair adds nothing to the
 *  descriptor, which sums over its other element pairs alone."; "RCUT2B =
 *  2.45 is below the smallest (Ta, Ta) neighbour distance of the dataset
 *  (2.86 Å) and the smallest (W, W) one (2.5 Å): the (Ta, Ta) and (W, W)
 *  pairs add nothing ..."; "RCUT2B = 2.6 holds no (Ta, Ta) pair of atoms
 *  of the dataset: ..." for a pair @p d has none of, as statistics taken
 *  up to the cutoff have none beyond it). A view without the pairs of its
 *  line judges @p d as a whole and names no pair. HealthOk without the key
 *  or without atoms. */
HealthVerdict cutoff_reach_check(const tadah::core::stats::DataStats &d,
                                 const SeedView &s, const std::string &key);

/** \brief The neighbour distances a density with cutoff @p rcut sums over:
 *  from the smallest r_min of the element pairs of @p d to the smaller of
 *  their largest r_max and @p rcut (@p rcut <= 0: no cutoff given, r_max).
 *  `empty` when the data have no pair or none closer than @p rcut. */
struct NeighbourRange {
  double lo = 0.0;
  double hi = 0.0;
  bool empty = true;
};
NeighbourRange neighbour_range(const tadah::core::stats::DataStats &d,
                               double rcut);

/** \brief The width eta above which a Gaussian exp(-eta (r - c)^2) is
 *  below kDensityReachFloor at the distance @p dist from its centre:
 *  ln(1 / kDensityReachFloor) / dist^2 (infinite for @p dist 0). */
double gaussian_reach_width(double dist);

/** \brief The width eta above which a blip b(eta (r - c)) is below
 *  kDensityReachFloor at the distance @p dist from its centre: x_f / dist,
 *  with x_f = 2 - (4 kDensityReachFloor)^(1/3), where the tail 0.25 (2 -
 *  x)^3 of the cubic B-spline is kDensityReachFloor (infinite for @p dist
 *  0). */
double blip_reach_width(double dist);

/** \brief The reach check of the radial channels of a many-body density:
 *  each centre CGRIDMB(i) with its width SGRIDMB(i) and the cutoff RCUTMB
 *  of @p s, against the neighbour distances (neighbour_range) of each
 *  element pair the line of @p s stands for, judged on its own as by
 *  cutoff_reach_check() (a pair with "*" each element pair it matches),
 *  which its messages name ("every (W, W) neighbour distance
 *  within RCUTMB = 3.5").
 *
 *  A Gaussian channel (@p kind Gaussian, exp(-SGRIDMB (r - CGRIDMB)^2))
 *  reaches a pair unless it is below kDensityReachFloor at every distance
 *  of the pair's range; a blip channel (Blip, b(SGRIDMB (r - CGRIDMB)),
 *  support |r - CGRIDMB| < 2/SGRIDMB) when its support holds one of them. A
 *  width of 0 or less is no reach failure (other rules refuse it). The
 *  channels are judged together: a channel that reaches no pair of the
 *  line fails (its channel of ρ is 0 for every atom), and so does a pair
 *  that no channel reaches (ρ gets nothing from it); a channel that misses
 *  a pair that another channel reaches is no finding. HealthFail names the
 *  first channel that reaches no pair by its entry of the configuration
 *  (SeedView::entry_number(): CGRIDMB(3) for the second channel of a
 *  joined line's descriptor after one of another), its centre and width,
 *  the form, and each pair with its range ("so its channel of ρ is ~0 for
 *  every atom", 0 for a blip); else the pairs that no channel reaches, with
 *  the channel and its width for a density of one channel ("CGRIDMB(1) =
 *  3.1 with SGRIDMB(1) = 100: the Gaussian ... is below 1e-6 at every
 *  (W, W) neighbour distance within RCUTMB = 3.5 (2.5 to 2.7 Å), so ρ gets
 *  nothing from the (W, W) pairs of atoms ..."), with the entries of the
 *  channels for several ("the channels CGRIDMB(1) to CGRIDMB(4) with their
 *  widths SGRIDMB(1) to SGRIDMB(4): each Gaussian ..."). Also a centre
 *  without a width, and, when @p s has no RCUTMB, data of one atom or more
 *  without a pair of the line ("the dataset has no (W, W) pair of atoms").
 *  A pair without a neighbour distance within RCUTMB is left to the check
 *  of the cutoff (cutoff_reach_check), which gives that finding: HealthOk
 *  when no pair has one. HealthOk without CGRIDMB, or for data without
 *  atoms. @throws std::logic_error for a @p kind other than Gaussian and
 *  Blip. */
HealthVerdict radial_reach_check(const tadah::core::stats::DataStats &d,
                                 const SeedView &s, BasisKind kind);

/** \brief The OPTIM range of the centre CGRIDMB(@p arg_pos) (-1: the
 *  first) of a Gaussian density channel, by the seed's centre c:
 *  - at the origin (CGRIDMB 0): no row, the width alone sets the reach;
 *  - elsewhere: c +- the FWHM of the neighbour shell nearest to c (the
 *    peak of the neighbour distances nearest to it; 0.2 times its position
 *    for a peak without a FWHM), its low end not below r_min and its high
 *    end not above the neighbour distances within RCUTMB
 *    (neighbour_range()) unless c is (then the range ends at c): the range
 *    holds c;
 *  - without a seed centre: the first peak of the neighbour distances +-
 *    its FWHM, from r_min;
 *  - without peaks: the 10th to 90th percentiles of the distances;
 *  - without a pair: no_pair_suggestion(@p s). */
BoundSuggestion gaussian_centre_suggestion(
    const tadah::core::stats::DataStats &d, const SeedView &s, int arg_pos);

/** \brief The OPTIM range of the width SGRIDMB(@p arg_pos) (-1: the first)
 *  of a Gaussian density channel, by the seed's centre CGRIDMB c of the
 *  same entry:
 *  - at the origin: the widths at which exp(-SGRIDMB r^2) drops by a
 *    factor of 0.9 to 1/20 from the first neighbour shell r1 to the second
 *    r2, ln(1/0.9) / (r2^2 - r1^2) to ln(20) / (r2^2 - r1^2), the upper end
 *    at most gaussian_reach_width(r_min) (the shells: the first two peaks
 *    of the neighbour distances; without them, their 10th percentile and
 *    median);
 *  - elsewhere: 0.25 / w^2 to 9 / w^2 with w the FWHM of the neighbour
 *    shell nearest to c (as gaussian_centre_suggestion() takes it);
 *  - without a seed centre, or without peaks: 0.25 / w^2 to 9 / w^2 with
 *    w the FWHM of the first peak (without peaks, or without a FWHM, the
 *    spread from the 10th to the 90th percentile), a fixed range 0.1 to 1
 *    when the distances have no spread;
 *  - without a pair: no_pair_suggestion(@p s). */
BoundSuggestion gaussian_width_suggestion(
    const tadah::core::stats::DataStats &d, const SeedView &s, int arg_pos);

/** \brief The OPTIM range of the centre CGRIDMB(@p arg_pos) (-1: the
 *  first) of a blip density channel b(SGRIDMB (r - CGRIDMB)), by the seed's
 *  centre c, as gaussian_centre_suggestion() reads it:
 *  - at the origin (CGRIDMB 0): no row, the width alone sets the reach;
 *  - elsewhere: c +- the FWHM of the neighbour shell nearest to c, within
 *    the neighbour distances within RCUTMB unless c lies outside them;
 *  - without a seed centre: the first peak of the neighbour distances +-
 *    its FWHM, from 0;
 *  - without peaks: the 10th to 90th percentiles of the distances;
 *  - without a pair: no_pair_suggestion(@p s). */
BoundSuggestion blip_centre_suggestion(
    const tadah::core::stats::DataStats &d, const SeedView &s, int arg_pos);

/** \brief The OPTIM range of the width SGRIDMB(@p arg_pos) (-1: the first)
 *  of a blip density channel, by the seed's centre CGRIDMB c of the same
 *  entry:
 *  - at the origin: the widths whose reach, the distance x_f / SGRIDMB at
 *    which b(SGRIDMB r) falls to kDensityReachFloor, runs from r_min to the
 *    farthest neighbour distance within RCUTMB (neighbour_range()):
 *    blip_reach_width(farthest) to blip_reach_width(r_min); no row when
 *    no neighbour distance lies within RCUTMB, or when the two are one;
 *  - elsewhere: 1 / w to 6 / w with w the FWHM of the neighbour shell
 *    nearest to c (the support half-width 2/SGRIDMB from 2 w down to w/3);
 *  - without a seed centre, or without peaks: 1 / w to 6 / w with w the
 *    FWHM of the first peak (without peaks, the spread from the 10th to the
 *    90th percentile), a fixed range 0.1 to 1 when that is not above 0;
 *  - without a pair: no_pair_suggestion(@p s). */
BoundSuggestion blip_width_suggestion(
    const tadah::core::stats::DataStats &d, const SeedView &s, int arg_pos);

}  // namespace models
}  // namespace tadah

#endif  // TADAH_MODELS_DESCRIPTORS_DENSITY_REACH_H
