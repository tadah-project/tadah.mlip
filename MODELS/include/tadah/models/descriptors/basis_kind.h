#ifndef TADAH_MODELS_DESCRIPTORS_BASIS_KIND_H
#define TADAH_MODELS_DESCRIPTORS_BASIS_KIND_H

// Single source of truth for the descriptor RADIAL/EMBEDDING basis family.
//
// A descriptor's basis is one of a small, fixed set of shapes. Historically the
// choice was encoded three incompatible ways: (1) a per-class name (D2_Blip vs
// D2_BP vs D2_Blip5 vs D2_Knot5), (2) a compile-time template policy
// (radial_policies.h BlipRadial/GaussianRadial), and (3) a runtime integer code
// baked into DM_REAM's TYPEMB token with an ad-hoc private enum + scattered
// if-chains. `BasisKind` unifies the {enum <-> wire-code <-> name <-> traits}
// relationship in ONE place so every consumer (descriptors, refit, the basis
// analysis engine, HPO capability queries) shares it.
//
// The actual per-basis math lives in d_basis_functions.h; the runtime dispatch
// that maps a BasisKind to those primitives lives in basis_dispatch.h.

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>

namespace tadah {
namespace models {

/** \brief The radial/embedding basis family of a descriptor.
 *
 * The underlying integer values are the FROZEN wire codes written into the
 * TYPEMB/TYPE2B `<basis>` token (they must match DM_REAM potential files and
 * `tadah refit` output). Do not renumber.
 */
enum class BasisKind : std::uint8_t {
  Blip     = 0,  ///< cubic B-spline (compact support, C^2)
  Gaussian = 1,  ///< GTO exp(-eta (r-rs)^2) (NON-compact)
  Knot5    = 2,  ///< one-sided quintic/cubic truncated power (compact; eta gauge)
  Blip5    = 3,  ///< quintic M6 B-spline bump (compact, C^4)
};

/** \brief Static properties of a basis family used across the codebase. */
struct BasisTraits {
  /** Basis self-terminates (finite support) so a hard Cut_Dummy cutoff is
   *  sound: the density/pair function vanishes smoothly at the cutoff BY
   *  CONSTRUCTION. False for Gaussian (only a real, smooth cutoff avoids a
   *  force discontinuity — the "no-cutoff smell"). */
  bool compact_support;
  /** The width channel (SGRIDMB / SEMBFUNC) is a pure gauge and is ABSENT
   *  from the token stream (knot family): (c, eta) -> (c s^k, eta/s) leaves
   *  the function invariant, so fitters pin eta = 1. */
  bool eta_is_gauge;
  /** Human-readable name (matches the canonical `tadah refit` REFIT_BASIS). */
  const char *name;
  /** Support half-width of a symmetric BUMP basis function in units of 1/eta:
   *  the support of bump k is [c_k - w/eta_k, c_k + w/eta_k] (w = 2 for the
   *  cubic Blip, 3 for the quintic Blip5). 0 for non-bump shapes (Gaussian is
   *  non-compact; Knot5 is one-sided — its rightmost extent is the knot
   *  itself, see potedit::derived_knot_rcut). */
  double bump_halfwidth;
};

/** \brief Compile-time trait lookup (no runtime cost). */
constexpr BasisTraits basis_traits(BasisKind k) {
  switch (k) {
    case BasisKind::Blip:     return {true,  false, "Blip",     2.0};
    case BasisKind::Gaussian: return {false, false, "Gaussian", 0.0};
    case BasisKind::Knot5:    return {true,  true,  "Knot5",    0.0};
    case BasisKind::Blip5:    return {true,  false, "Blip5",    3.0};
  }
  return {true, false, "Blip", 2.0};  // unreachable (enum is exhaustive above)
}

inline int          basis_code(BasisKind k) { return static_cast<int>(k); }
inline const char  *basis_name(BasisKind k) { return basis_traits(k).name; }
constexpr double    bump_halfwidth(BasisKind k) {
  return basis_traits(k).bump_halfwidth;
}

/** \brief Tolerance for comparing a bump's support reach against a hard
 *  cutoff: `c + w/eta <= rcut + bump_rcut_tol(rcut)` passes.
 *
 * A support edge EXACTLY at rcut is fine (B/B5 vanish C^2/C^4-smoothly
 * there); the tolerance only absorbs round-off. It is deliberately ~1e-6
 * RELATIVE — no tighter — so bounds produced by the hpotarget emitter's
 * decimal quantization (optim_bound_quantum, ~1e-6 relative) can sit exactly
 * on the cap and still pass the `tadah hpo` startup gate. 5e-6 A of spill
 * contributes O((eta*5e-6)^w) — physically nil. */
inline double bump_rcut_tol(double rcut) {
  return 1e-6 * (rcut > 1.0 ? rcut : 1.0);
}

/** \brief Map the FROZEN TYPExB integer `<basis>` code to a BasisKind.
 *
 * Throws on an unknown code rather than silently degrading to Blip — a pot
 * written by a newer Tadah must not be evaluated with the wrong basis without
 * any warning. Replaces DM_REAM's private `basis_from_bcode`.
 */
inline BasisKind basis_kind_from_code(int code) {
  switch (code) {
    case 0: return BasisKind::Blip;
    case 1: return BasisKind::Gaussian;
    case 2: return BasisKind::Knot5;
    case 3: return BasisKind::Blip5;
    default:
      throw std::runtime_error(
          "BasisKind: unknown basis code " + std::to_string(code) +
          " (known: 0=Blip, 1=Gaussian, 2=Knot5, 3=Blip5).");
  }
}

/** \brief The BasisKind a user-facing basis string names (REFIT_BASIS and its
 *  aliases), or none: the one spelling table of `tadah refit` (MLIP's
 *  refit_basis_is_* read it). Note the Knot3 / KnotMix / Mixed aliases all
 *  map to the Knot5 family — the per-atom KDEG* degree arrays (3|5) then
 *  select cubic vs quintic knot atoms. The exact wire contract is
 *  `basis_kind_from_code`. */
inline std::optional<BasisKind> basis_kind_from_name(const std::string &s) {
  if (s == "Gaussian" || s == "gaussian" || s == "BP" || s == "D2_BP")
    return BasisKind::Gaussian;
  if (s == "Knot5" || s == "KNOT5" || s == "knot5" ||
      s == "Knot3" || s == "KNOT3" || s == "knot3" ||
      s == "KnotMix" || s == "KNOTMIX" || s == "knotmix" ||
      s == "Mixed" || s == "MIXED")
    return BasisKind::Knot5;
  if (s == "Blip5" || s == "BLIP5" || s == "blip5" || s == "M6")
    return BasisKind::Blip5;
  if (s == "Blip" || s == "BLIP" || s == "blip")
    return BasisKind::Blip;
  return std::nullopt;
}

/** \brief Map a user-facing basis string (REFIT_BASIS and its aliases) to a
 *  BasisKind (basis_kind_from_name). A string that names no basis throws
 *  std::runtime_error naming it: it was Blip ("lenient by design"), so a
 *  typo fitted another basis than the one asked for. */
inline BasisKind basis_kind_from_string(const std::string &s) {
  if (const auto k = basis_kind_from_name(s)) return *k;
  throw std::runtime_error("BasisKind: '" + s + "' names no basis (Blip, "
                           "Blip5, Knot5 and its aliases, Gaussian).");
}

}  // namespace models
}  // namespace tadah

#endif  // TADAH_MODELS_DESCRIPTORS_BASIS_KIND_H
