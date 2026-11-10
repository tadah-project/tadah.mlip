/* The DM_REAM density before RHOFORM (every tadah before 1.6.0), frozen, for
 * the goldens of test_ream_goldens.cpp.
 *
 * ream_density_before_rhoform.cpp copies the code that computed it at
 * MODELS 4c4144b, the tree ream_goldens_data.h was captured from:
 * DM_REAM::calc_rho, calc_dXijdri and eam_density with rho_raw and
 * rho_raw_deriv (src/dm_ream.cpp), the radial dispatch
 * (descriptors/basis_dispatch.h), the Blip, Blip5, knot and Gaussian
 * primitives (descriptors/d_basis_functions.h) and Cut_Dummy, Cut_Cos and
 * Cut_Poly (src/cutoffs.cpp), every expression as it was, so that it gives
 * the bits that tadah gave on the platform it runs on. Only the C library's
 * cos, sin and exp are reached through Libm, so that a test can give the
 * copy other results of them: test_ream_goldens.cpp gives it those recorded
 * in ream_goldens_libm.h, with which it gives the stored goldens bit for
 * bit whatever the C library. The file is compiled with
 * MODELS' flags and never with -march=native (tests/CMakeLists.txt): a
 * compiler that fuses its multiply-adds into FMAs would change its bits. Do
 * not edit the copy; the goldens anchor it.
 */
#ifndef TADAH_MODELS_TESTS_REAM_DENSITY_BEFORE_RHOFORM_H
#define TADAH_MODELS_TESTS_REAM_DENSITY_BEFORE_RHOFORM_H

#include <string>
#include <vector>

namespace ream_before_rhoform {

/// The C library functions the density calls.
struct Libm {
  double (*cos)(double);
  double (*sin)(double);
  double (*exp)(double);
};

/// The platform's cos, sin and exp.
Libm platform_libm();

/// A single-species DM_REAM density as a configuration states it.
struct Density {
  int bcode = 0;          ///< TYPEMB basis code: 0 Blip, 1 Gaussian, 2 Knot5, 3 Blip5
  std::string cutoff;     ///< RCTYPEMB: Cut_Dummy, Cut_Cos or Cut_Poly
  double rcut = 0.0;      ///< RCUTMB
  double w = 0.0;         ///< the pair weight (WATOM of the species)
  std::vector<double> c;  ///< CGRIDMB
  std::vector<double> s;  ///< SGRIDMB (empty for the knot family)
  std::vector<double> a;  ///< AMPGRIDMB
  std::vector<double> kdeg;  ///< KDEGMB (empty: degree 5)
};

/// The values a golden point holds: calc_rho's increment at scale 1, the
/// calc_dXijdri force prefactor (scale 1, g'_m(rho_i) = 1, unit vector
/// (1, 0, 0)), and eam_density's rho and rho'.
struct Values {
  double rho;
  double pref;
  double eam_rho;
  double eam_rhop;
};

/// The density before RHOFORM at distance @p r, through @p lm.
Values evaluate(const Density &d, double r, const Libm &lm);

}  // namespace ream_before_rhoform

#endif  // TADAH_MODELS_TESTS_REAM_DENSITY_BEFORE_RHOFORM_H
