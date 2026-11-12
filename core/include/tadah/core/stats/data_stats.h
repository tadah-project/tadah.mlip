#ifndef TADAH_CORE_STATS_DATA_STATS_H
#define TADAH_CORE_STATS_DATA_STATS_H

// DataStats — canonical snapshot of dataset statistics consumed by the
// HPO bound-validation pipeline. The producer engine is `tadah analysis
// dataset_stats`; the SCHEMA itself is defined here so the descriptors'
// metadata closures, the bound suggester and the pre-flight audit can
// all compile against a stable type.

#include <array>
#include <cstddef>
#include <iosfwd>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <tadah/core/stats/histogram.h>
#include <tadah/core/stats/kde.h>

namespace tadah {
namespace core {
namespace stats {

/** \brief Index conventions for DataStats `..._percentiles[11]`:
 *  [0] p1, [1] p5, [2] p10, [3] p25, [4] p50, [5] p75,
 *  [6] p90, [7] p95, [8] p99, [9] mean, [10] std
 *
 *  These indices match the comments in `summary_percentiles` (see
 *  CORE/include/tadah/core/stats/percentiles.h). The descriptors'
 *  metadata (MODELS, `DescriptorMeta`) refer to them by index in their
 *  `BoundSuggester` closures, so DO NOT renumber.
 */
constexpr std::size_t IDX_P1   = 0;
constexpr std::size_t IDX_P5   = 1;
constexpr std::size_t IDX_P10  = 2;
constexpr std::size_t IDX_P25  = 3;
constexpr std::size_t IDX_P50  = 4;
constexpr std::size_t IDX_P75  = 5;
constexpr std::size_t IDX_P90  = 6;
constexpr std::size_t IDX_P95  = 7;
constexpr std::size_t IDX_P99  = 8;
constexpr std::size_t IDX_MEAN = 9;
constexpr std::size_t IDX_STD  = 10;

/** \brief Per-element-pair pair-distance statistics.
 *
 *  Populated by the `analysis dataset_stats` engine over all
 *  i,j atom pairs (i != j) within the audit cutoff RCUT_AUDIT (also
 *  determined by that engine, currently the max of all configured
 *  RCUTxB).
 */
struct PerPairStats {
  /** Element symbols, lex-ordered: elements.first <= elements.second. */
  std::pair<std::string, std::string> elements{};
  std::size_t   n_pairs{0};
  double        r_min{0.0};
  double        r_max{0.0};
  /** 11-tuple per IDX_* convention above. */
  std::array<double, 11> r_percentiles{};
  /** Optional FD-binned histogram (n_bins == 0 if not computed). */
  Histogram     r_histogram{};
  /** KDE peaks of the per-pair r_ij distribution; empty if peak
   *  detection was skipped. Sorted by position ascending. */
  std::vector<KdePeak> r_peaks{};
};

/** \brief Whole-dataset statistics consumed by the metadata pipeline.
 *
 *  All fields are POD-like and trivially copyable / serializable. The
 *  serializer pair save/load is in `data_stats_io.h`.
 */
struct DataStats {
  std::size_t n_atoms{0};
  std::size_t n_structures{0};
  std::vector<std::string> elements{};                    // unique, lex-sorted
  std::vector<PerPairStats> per_pair{};                   // size N(N+1)/2
  std::map<std::string, std::array<double, 11>>
      nn_count_percentiles_per_element{};
  std::array<double, 11> e_per_atom_percentiles{};
  std::array<double, 11> f_norm_percentiles{};

  /** Material-class label produced by the dataset_stats classifier
   *  ("bcc_metal", "covalent", "molecular_crystal", "dimer", ...).
   *  Empty string when not classified. */
  std::string class_label{};

  /** Per-descriptor ρ percentiles (left to a caller of MLIP's
   *  `compute_data_stats` with `with_descriptor_stats` true; default
   *  empty). Map key is the descriptor's registered label, e.g.
   *  "DM_mBlipEnv". */
  std::map<std::string, std::array<double, 11>>
      rho_percentiles_per_descriptor{};
};

// NOTE: serialisation (save/load) is in a dedicated header
// (`data_stats_io.h`) so the schema stays dependency-free for the
// descriptors' metadata closures.

}  // namespace stats
}  // namespace core
}  // namespace tadah

#endif  // TADAH_CORE_STATS_DATA_STATS_H
