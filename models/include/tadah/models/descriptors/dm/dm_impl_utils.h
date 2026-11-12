#ifndef DM_IMPL_UTILS_H
#define DM_IMPL_UTILS_H

/**
 * \file dm_impl_utils.h
 * \brief Shared implementation utilities for many-body descriptor classes.
 *
 * Provides small, zero-overhead helpers used across DM_Blip, DM_mEAD, and
 * DM_Poly implementations:
 *
 * - \ref tadah::models::detail::StackOrHeapBuffer — stack-first scratch buffer
 * - \ref tadah::models::detail::FACT_TBL — compile-time factorials 0!…6!
 * - \ref tadah::models::detail::fact_cached — O(1) factorial lookup
 * - \ref tadah::models::detail::pow_i06 — branchless integer powers 0–6
 * - \ref tadah::models::detail::precompute_powers — Cartesian power tables
 *
 * All symbols live in `tadah::models::detail` and are either `inline`,
 * `constexpr`, or class templates — safe to include in multiple translation
 * units without ODR violations.
 *
 * \note `precompute_radials` is intentionally **not** included here because
 * its implementation is radial-basis-specific (Blip vs Gaussian) and will
 * become a radial policy in Stage 2 of the DM_Poly implementation.
 */

#include <array>
#include <cmath>
#include <cstddef>
#include <memory>

namespace tadah {
namespace models {
namespace detail {

/* ========================================================================= *
 *  Stack-or-heap scratch buffer                                              *
 * ========================================================================= */

/**
 * \brief Stack-first dual scratch buffer for radial pre-computation.
 *
 * Provides two contiguous `double` arrays of size `n`:
 * - `g()`  — radial function values
 * - `dg()` — radial function derivatives
 *
 * When `n ≤ N` the arrays live on the stack (zero heap allocation).
 * When `n > N` a single heap block of size `2n` is allocated.
 *
 * \tparam N  Stack capacity (default 64).
 */
template<std::size_t N = 64>
class StackOrHeapBuffer {
public:
  explicit StackOrHeapBuffer(std::size_t n)
  : sz(n), use_heap(n > N), heap(use_heap ? new double[2*n] : nullptr)
  {}

  double*     g()          { return use_heap ?  heap.get()     : stack; }
  double*     dg()         { return use_heap ? &heap.get()[sz] : &stack[N]; }
  std::size_t size() const { return sz; }

private:
  std::size_t               sz;
  bool                      use_heap;
  std::unique_ptr<double[]> heap;
  alignas(64) double        stack[2*N];
};

/* ========================================================================= *
 *  Factorials and integer powers                                             *
 * ========================================================================= */

/** \brief Compile-time factorials 0! … 6! */
constexpr std::array<double,7> FACT_TBL = {1., 1., 2., 6., 24., 120., 720.};

/** \brief O(1) factorial lookup for \f$ n \in \{0,\ldots,6\} \f$. */
inline double fact_cached(int n) { return FACT_TBL[static_cast<std::size_t>(n)]; }

/**
 * \brief Branchless integer power \f$ x^n \f$ for \f$ n \in \{0,\ldots,6\} \f$.
 */
inline double pow_i06(double x, int n)
{
  switch (n) {
    case 0:  return 1.0;
    case 1:  return x;
    case 2:  return x*x;
    case 3:  return x*x*x;
    case 4:  { double x2 = x*x; return x2*x2; }
    case 5:  { double x2 = x*x; return x2*x2*x; }
    case 6:  { double x2 = x*x; return x2*x2*x2; }
    default: return std::pow(x, n);
  }
}

/* ========================================================================= *
 *  Cartesian power tables                                                    *
 * ========================================================================= */

/**
 * \brief Pre-compute \f$ x^0, x^1, \ldots, x^L \f$.
 *
 * \param x  coordinate value
 * \param L  maximum power (\f$ 0 \le L \le 6 \f$)
 * \param p  output array of length ≥7: `p[n] = x^n`
 */
inline void precompute_powers(double x, int L, double p[7])
{
  p[0] = 1.0;
  for (int n = 1; n <= L; ++n) p[n] = p[n-1]*x;
#if defined(__SANITIZE_ADDRESS__) || defined(ADDRESS_SANITIZER)
  for (int n = L+1; n <= 6; ++n) p[n] = 0.0;
#endif
}

/**
 * \brief Pre-compute \f$ x^0,\ldots,x^L \f$ and their derivatives.
 *
 * \param x   coordinate value
 * \param L   maximum power (\f$ 0 \le L \le 6 \f$)
 * \param p   output: `p[n] = x^n`
 * \param dp  output: `dp[n] = n·x^{n-1}`
 */
inline void precompute_powers(double x, int L, double p[7], double dp[7])
{
  p[0] = 1.0; dp[0] = 0.0;
  for (int n = 1; n <= L; ++n) {
    p[n]  = p[n-1]*x;
    dp[n] = n*p[n-1];
  }
#if defined(__SANITIZE_ADDRESS__) || defined(ADDRESS_SANITIZER)
  for (int n = L+1; n <= 6; ++n) { p[n] = 0.0; dp[n] = 0.0; }
#endif
}

} // namespace detail
} // namespace models
} // namespace tadah

#endif // DM_IMPL_UTILS_H
