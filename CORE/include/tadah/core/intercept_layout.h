#ifndef TADAH_CORE_INTERCEPT_LAYOUT_H
#define TADAH_CORE_INTERCEPT_LAYOUT_H

#include <tadah/core/context.h>
#include <tadah/core/periodic_table.h>

#include <array>
#include <cstddef>
#include <stdexcept>
#include <string>

namespace tadah {
namespace core {

/** \brief Number of intercept columns of the descriptor layout.
 *
 *  BIAS true gives every atom a one-hot intercept column for its species:
 *  one column per ATOM species, in ATOM order, before the descriptor
 *  columns. BIAS false gives none. The descriptors (MLIP DescriptorsCalc,
 *  MODELS DC_Selector), the ridge, the normaliser, the NORM weight
 *  conversion and pair_style tadah all use this layout.
 *
 *  @throws std::runtime_error BIAS true without ATOM: no species to give
 *          an intercept column to. */
inline size_t n_intercept_columns(const Context &c) {
  if (!c.get<bool>("BIAS")) return 0;
  if (!c.contains("ATOM") || c.size("ATOM") == 0)
    throw std::runtime_error(
        "BIAS true needs ATOM: BIAS gives one intercept column per ATOM "
        "species, and the configuration lists no ATOM species");
  return c.size("ATOM");
}

/** \brief The intercept column of the species with atomic number Z
 *  (its index in ATOM), or -1 when ATOM does not list it. */
inline int species_intercept_idx(int Z, const Context &c) {
  if (!c.contains("ATOM")) return -1;
  const size_t n = c.size("ATOM");
  for (size_t i = 0; i < n; ++i) {
    const std::string sym = c.get<std::string>("ATOM", i);
    if (PeriodicTable::find_by_symbol(sym).Z == Z)
      return static_cast<int>(i);
  }
  return -1;
}

/** \brief species_intercept_idx for every atomic number 0..118: entry Z is
 *  the intercept column of element Z (its index in ATOM), -1 for an
 *  element ATOM does not list. A table for per-atom lookups. */
inline std::array<int, 119> intercept_columns_by_Z(const Context &c) {
  std::array<int, 119> col;
  col.fill(-1);
  if (!c.contains("ATOM")) return col;
  const size_t n = c.size("ATOM");
  for (size_t i = n; i-- > 0;) {   // the first entry of a repeated symbol wins
    const int Z = PeriodicTable::find_by_symbol(c.get<std::string>("ATOM", i)).Z;
    if (Z >= 0 && Z < static_cast<int>(col.size())) col[Z] = static_cast<int>(i);
  }
  return col;
}

} // namespace core
} // namespace tadah

#endif
