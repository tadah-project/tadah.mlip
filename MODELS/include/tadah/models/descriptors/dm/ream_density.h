#ifndef TADAH_MODELS_REAM_DENSITY_H
#define TADAH_MODELS_REAM_DENSITY_H

#include <tadah/core/context.h>
#include <tadah/core/core_types.h>
#include <tadah/models/cutoffs.h>
#include <tadah/models/descriptors/basis_kind.h>
#include <tadah/models/descriptors/dm/rhoform.h>
#include <tadah/models/eam/eam_lattice.h>

#include <cstddef>
#include <string>
#include <vector>

namespace tadah {
namespace models {

/** \brief Summary of ReamDensity::scan_grid over its grid. */
struct DensityScan {
  double rho_min = 0.0;   ///< smallest grid value
  double rho_max = 0.0;   ///< largest grid value
  double r_at_min = 0.0;  ///< the FIRST grid point attaining rho_min
  double r_at_max = 0.0;  ///< the FIRST grid point attaining rho_max
  std::size_t i_min = 0;  ///< grid index of r_at_min
  std::size_t i_max = 0;  ///< grid index of r_at_max
  bool finite = true;     ///< every grid value is finite
};

/** \brief Result of ReamDensity::rhobar_scan over a lattice-constant grid. */
struct RhoBarScan {
  std::vector<double> rhobar;  ///< rhobar(a_k) per grid point
  std::size_t rises = 0;       ///< #k with rhobar[k+1] >= rhobar[k]
  std::size_t first_rise = 0;  ///< the first such k (valid when rises > 0)
  bool finite = true;          ///< every rhobar value is finite
};

/** \brief The DM_REAM pair density rho_raw(r), in its declared RHOFORM.
 *
 *  The ONE evaluator of the DM_REAM density: DM_REAM (calc_rho, the force
 *  descriptors, eam_density) delegates to it, and the HPO density gates and
 *  reports call it, so every consumer evaluates the same floating-point
 *  expression. With x_n = s_n (r - c_n), psi_n the bare basis function and
 *  S_n the integrated bump (d_basis_functions.h):
 *  - SIGNED:   rho_raw = sum_n a_n (psi_n(r) f_c)
 *  - NONNEG:   the same with f_c replaced by max(f_c, 0)
 *  - MONOTONE: rho_raw = sum_n a_n (S_n(r) max(f_c, 0))
 *  summed in ascending n. The pair weight W is NOT included: DM_REAM
 *  multiplies it outside the sum (rho = W rho_raw).
 *
 *  Every method is defined out of line in MODELS/src/ream_density.cpp,
 *  compiled with MODELS' own flags, as the density of DM_REAM before
 *  RHOFORM was: FMA contraction in a -march=native unit that compiled the
 *  sum would change its bits. In a MODELS compiled without FMA contraction
 *  (the presets' flags on x86-64: no -march=native, no -ffast-math) the
 *  SIGNED form is the density of every tadah before 1.6.0 bit for bit (the
 *  goldens, test_ream_goldens.cpp), and the bitwise relations below hold:
 *  value_deriv() is value() and deriv(), scan_grid() is value() point by
 *  point, DM_REAM::pair_density() and eam_density() are calc_rho's
 *  increment. A MODELS compiled with FMA contraction (-march=native on an
 *  FMA machine, AArch64 at default flags) gives them to rounding only.
 *
 *  Guarantees, on a density that passed validate():
 *  - SIGNED: none (bitwise the density of every tadah before 1.6.0 in a
 *    MODELS compiled without FMA contraction).
 *  - NONNEG: rho_raw >= 0 exactly.
 *  - MONOTONE: rho_raw >= 0 exactly and non-increasing in r to rounding
 *    (S_B5 has 1-ulp rises for |x| < ~1.2; Cut_Poly/PolyS rise at the 1e-15
 *    level near rcut). */
class ReamDensity {
public:
  BasisKind basis = BasisKind::Blip;  ///< TYPEMB basis code
  RhoForm form = RhoForm::Signed;     ///< RHOFORM
  tadah::core::v_type crho;     ///< CGRIDMB: centres / knots
  tadah::core::v_type srho;     ///< SGRIDMB: widths (empty for the knot family)
  tadah::core::v_type arho;     ///< AMPGRIDMB: amplitudes
  tadah::core::v_type rho_deg;  ///< KDEGMB: knot degrees 3|5 (empty = all 5)

  /** Knot degree of term @p n (5 when KDEGMB is absent). */
  int deg(std::size_t n) const;

  /** Width of term @p n (1.0, ignored, for the knot family). */
  double width(std::size_t n) const;

  /** The TYPEMB basis code of a bare DM_REAM context (TYPEMB token 1), as
   *  DM_REAM reads it: an integer literal ([+-]digits) naming a known basis
   *  (basis_kind_from_code). Throws std::runtime_error, without a prefix
   *  ("TYPEMB basis code 'x' is not an integer (known: 0=Blip, ...)"), when
   *  the token is absent, is not an integer literal or names no basis;
   *  DM_REAM prefixes "DM_REAM: ", from_context "DM_REAM density: ". There
   *  is no fallback: such a token was read as 0 (Blip), or as its integer
   *  prefix ("2.5" as 2). */
  static BasisKind basis_from_context(const tadah::core::Context &ctx);

  /** Reads RHOFORM, the TYPEMB basis code (basis_from_context), CGRIDMB,
   *  SGRIDMB, AMPGRIDMB and KDEGMB through Context::has_stored /
   *  stored_value / D_Base::get_grid only, so it never materialises a
   *  blueprint default and never logs. @p ctx must be a bare DM_REAM context
   *  (TYPEMB token 0 is DM_REAM). Throws DensityFormError when RHOFORM (the
   *  migration text), CGRIDMB or AMPGRIDMB is absent, when the basis code is
   *  absent, not an integer or unknown, or when TYPEMB is not DM_REAM. Does
   *  not validate(). */
  static ReamDensity from_context(const tadah::core::Context &ctx);

  /** Validates the grids for the form (DensityFormError naming the offending
   *  1-based index and value):
   *  - every form: CGRIDMB/AMPGRIDMB sizes, the knot family's absent
   *    SGRIDMB, CGRIDMB/SGRIDMB sizes and no zero SGRIDMB for the bump
   *    families, KDEGMB size and entries in {3,5};
   *  - NONNEG and MONOTONE: every AMPGRIDMB entry finite and >= 0, every
   *    CGRIDMB entry finite;
   *  - NONNEG: every SGRIDMB entry finite (the knot family has none), and
   *    > 0 for the Gaussian basis (exp(-s (r - c)^2) overflows for s < 0);
   *  - MONOTONE: not the knot family, every SGRIDMB entry finite and > 0,
   *    and the many-body cutoff class @p rctype (Cut_Base::label(),
   *    RCTYPEMB; required) on the whitelist of rhoform_cutoff_whitelisted().
   *  The density pair weights (WATOM) are checked by
   *  D_Base::require_positive_density_weights. */
  void validate(const std::string &rctype) const;

  /** validate() with the label of the cutoff @p fc (nullptr: no cutoff). */
  void validate(Cut_Base *fc) const;

  /** The checks the DM_REAM constructor applies to the density of the bare
   *  DM_REAM context @p ctx, without building the descriptor or a cutoff:
   *  from_context(), validate() against the stored RCTYPEMB (absent:
   *  Cut_Dummy, the DM_Base default) and, under NONNEG and MONOTONE,
   *  D_Base::require_positive_density_weights. Throws DensityFormError. */
  static void validate_context(const tadah::core::Context &ctx);

  /** rho_raw(r) for the cutoff value @p fc = f_c(r). */
  double value(double r, double fc) const;

  /** d rho_raw / dr for @p fc = f_c(r) and @p fcp = f_c'(r) (fcp is never
   *  clamped: rho' = sum_n a_n (T_n' f_c + T_n f_c')). */
  double deriv(double r, double fc, double fcp) const;

  /** value() and deriv() together, through the fused Blip5 primitives:
   *  @p v is bitwise equal to value(r, fc) and @p d to deriv(r, fc, fcp)
   *  (without FMA contraction, see the class). */
  void value_deriv(double r, double fc, double fcp,
                   double &v, double &d) const;

  /** rho_raw on the grid of linspace(@p r_lo, @p r_hi, @p n), f_c from
   *  @p fc; calc_rho's rule applies: a point r > fc.get_rcut() is 0.
   *  Every grid value is bitwise equal to value(r_i, f_c(r_i)) (without
   *  FMA contraction, see the class): the sum is
   *  bump-major in ascending n over each term's contiguous grid-index
   *  support (widened by one index on each side); the skipped terms are
   *  exact zeros (MONOTONE adds a_n (1.0 f_c) below a bump's inner edge,
   *  where S_n = 1 exactly). Ties go to the FIRST index. @p rho, when not
   *  null, receives the grid values; @p term_abs, when not null, receives
   *  sum_n |a_n T_n(r_i) f_c| per point (T_n = psi_n or S_n), the scale of
   *  the rounding in rho. Throws std::invalid_argument unless n >= 2 and
   *  r_lo < r_hi are finite. */
  DensityScan scan_grid(double r_lo, double r_hi, std::size_t n, Cut_Base &fc,
                        std::vector<double> *rho = nullptr,
                        std::vector<double> *term_abs = nullptr) const;

  /** Lattice density rhobar(a_k) = sum_s m_s rho_raw(d_s a_k) of a
   *  monatomic lattice (@p shells from eam_lattice::shells, ascending d)
   *  for every lattice constant a_k in @p a, summed in ascending d over the
   *  shells with d_s a_k <= fc.get_rcut() (calc_rho's rule; f_c from @p fc
   *  as in value()); for a_k > 0 the sum stops at the first shell beyond
   *  the cutoff, so @p shells must ascend in d. Counts the rises
   *  rhobar[k+1] >= rhobar[k]. */
  RhoBarScan rhobar_scan(const std::vector<eam_lattice::Shell> &shells,
                         const std::vector<double> &a, Cut_Base &fc) const;

  /** numpy.linspace(lo, hi, n) bit for bit: x_i = double(i) * step + lo for
   *  i < n - 1 and x_{n-1} = hi, step = (hi - lo) / (n - 1). n = 1 gives
   *  {lo}, n = 0 an empty grid. */
  static std::vector<double> linspace(double lo, double hi, std::size_t n);
};

}  // namespace models
}  // namespace tadah

#endif
