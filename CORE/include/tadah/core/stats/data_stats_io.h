#ifndef TADAH_CORE_STATS_DATA_STATS_IO_H
#define TADAH_CORE_STATS_DATA_STATS_IO_H

// Header-only text-format save/load for DataStats. Versioned with a
// leading magic line so the schema can evolve without silently
// corrupting the saved statistics that a later load() reads.

#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <tadah/core/stats/data_stats.h>
#include <tadah/core/stats/histogram.h>
#include <tadah/core/stats/kde.h>

namespace tadah {
namespace core {
namespace stats {

constexpr const char* DATASTATS_MAGIC   = "TADAH_DATASTATS_V1";

inline void save(const DataStats& d, std::ostream& os) {
  // 17 significant digits is the minimum for lossless round-trip of
  // IEEE-754 double precision (max_digits10).
  os << std::scientific << std::setprecision(17);
  os << DATASTATS_MAGIC << "\n";
  os << "n_atoms "      << d.n_atoms      << "\n";
  os << "n_structures " << d.n_structures << "\n";
  os << "elements " << d.elements.size();
  for (const auto& e : d.elements) os << " " << e;
  os << "\n";
  os << "class_label " << (d.class_label.empty() ? "-" : d.class_label) << "\n";

  os << "e_per_atom_percentiles";
  for (double v : d.e_per_atom_percentiles) os << " " << v;
  os << "\n";

  os << "f_norm_percentiles";
  for (double v : d.f_norm_percentiles) os << " " << v;
  os << "\n";

  os << "nn_count_percentiles_per_element " << d.nn_count_percentiles_per_element.size() << "\n";
  for (const auto& kv : d.nn_count_percentiles_per_element) {
    os << kv.first;
    for (double v : kv.second) os << " " << v;
    os << "\n";
  }

  os << "rho_percentiles_per_descriptor " << d.rho_percentiles_per_descriptor.size() << "\n";
  for (const auto& kv : d.rho_percentiles_per_descriptor) {
    os << kv.first;
    for (double v : kv.second) os << " " << v;
    os << "\n";
  }

  os << "per_pair " << d.per_pair.size() << "\n";
  for (const auto& pp : d.per_pair) {
    os << pp.elements.first << " " << pp.elements.second
       << " n_pairs " << pp.n_pairs
       << " r_min " << pp.r_min
       << " r_max " << pp.r_max
       << "\n";
    os << "  r_percentiles";
    for (double v : pp.r_percentiles) os << " " << v;
    os << "\n";
    os << "  histogram " << pp.r_histogram.low << " " << pp.r_histogram.high
       << " " << pp.r_histogram.n_bins;
    for (auto c : pp.r_histogram.counts) os << " " << c;
    os << "\n";
    os << "  peaks " << pp.r_peaks.size();
    for (const auto& pk : pp.r_peaks) {
      os << " " << pk.position << " " << pk.density << " " << pk.fwhm;
    }
    os << "\n";
  }
}

inline DataStats load(std::istream& is) {
  std::string magic;
  if (!(is >> magic) || magic != DATASTATS_MAGIC) {
    throw std::runtime_error("data_stats load: bad magic '" + magic + "'");
  }
  DataStats d;
  std::string tag;
  while (is >> tag) {
    if (tag == "n_atoms") {
      is >> d.n_atoms;
    } else if (tag == "n_structures") {
      is >> d.n_structures;
    } else if (tag == "elements") {
      std::size_t n; is >> n;
      d.elements.resize(n);
      for (std::size_t i = 0; i < n; ++i) is >> d.elements[i];
    } else if (tag == "class_label") {
      is >> d.class_label;
      if (d.class_label == "-") d.class_label.clear();
    } else if (tag == "e_per_atom_percentiles") {
      for (double& v : d.e_per_atom_percentiles) is >> v;
    } else if (tag == "f_norm_percentiles") {
      for (double& v : d.f_norm_percentiles) is >> v;
    } else if (tag == "nn_count_percentiles_per_element") {
      std::size_t n; is >> n;
      for (std::size_t i = 0; i < n; ++i) {
        std::string sym; is >> sym;
        std::array<double, 11> arr{};
        for (double& v : arr) is >> v;
        d.nn_count_percentiles_per_element[sym] = arr;
      }
    } else if (tag == "rho_percentiles_per_descriptor") {
      std::size_t n; is >> n;
      for (std::size_t i = 0; i < n; ++i) {
        std::string lbl; is >> lbl;
        std::array<double, 11> arr{};
        for (double& v : arr) is >> v;
        d.rho_percentiles_per_descriptor[lbl] = arr;
      }
    } else if (tag == "per_pair") {
      std::size_t n; is >> n;
      d.per_pair.reserve(n);
      for (std::size_t i = 0; i < n; ++i) {
        PerPairStats pp;
        is >> pp.elements.first >> pp.elements.second;
        std::string sub;
        is >> sub; if (sub != "n_pairs") throw std::runtime_error("data_stats load: expected 'n_pairs'");
        is >> pp.n_pairs;
        is >> sub; if (sub != "r_min") throw std::runtime_error("data_stats load: expected 'r_min'");
        is >> pp.r_min;
        is >> sub; if (sub != "r_max") throw std::runtime_error("data_stats load: expected 'r_max'");
        is >> pp.r_max;
        is >> sub; if (sub != "r_percentiles") throw std::runtime_error("data_stats load: expected 'r_percentiles'");
        for (double& v : pp.r_percentiles) is >> v;
        is >> sub; if (sub != "histogram") throw std::runtime_error("data_stats load: expected 'histogram'");
        std::size_t nbins;
        is >> pp.r_histogram.low >> pp.r_histogram.high >> nbins;
        pp.r_histogram.n_bins = nbins;
        pp.r_histogram.counts.resize(nbins);
        for (auto& c : pp.r_histogram.counts) is >> c;
        is >> sub; if (sub != "peaks") throw std::runtime_error("data_stats load: expected 'peaks'");
        std::size_t npk; is >> npk;
        pp.r_peaks.resize(npk);
        for (auto& pk : pp.r_peaks) is >> pk.position >> pk.density >> pk.fwhm;
        d.per_pair.push_back(std::move(pp));
      }
    } else {
      throw std::runtime_error("data_stats load: unknown tag '" + tag + "'");
    }
  }
  return d;
}

}  // namespace stats
}  // namespace core
}  // namespace tadah

#endif  // TADAH_CORE_STATS_DATA_STATS_IO_H
