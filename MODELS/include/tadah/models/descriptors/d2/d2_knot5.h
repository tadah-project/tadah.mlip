#ifndef D2_KNOT5_H
#define D2_KNOT5_H
#include <tadah/models/descriptors/d2/d2_base.h>
#include <tadah/models/descriptors/d_basis_functions.h>

namespace tadah {
namespace models {

/** \brief One-sided quintic knot-function two-body descriptor.
 *
 * \f[
 * V_i^{\eta,r_s} =\sum_{j \neq i} K5L(\eta(r_{ij}-r_s))\,f_c(r_{ij}),
 * \qquad K5L(x_c) = \max(0,\,-x_c)^5
 * \f]
 *
 * i.e. with \f$\eta = 1\f$ each basis function is the truncated power
 * \f$(r_s - r)_+^5\f$ — the classic Mendelev/Ackland knotted-polynomial
 * EAM form: LEFT-active (contributes only for \f$ r < r_s \f$) and
 * vanishing \f$C^4\f$-smoothly at its knot \f$ r_s \f$. With every knot
 * \f$ r_s \le r_{cut} \f$ the summed pair function vanishes
 * \f$C^4\f$-smoothly at the cutoff BY CONSTRUCTION (the hard Cut_Dummy
 * truncation becomes a mathematical no-op).
 *
 * \ref CGRID2B parameters are the knot positions \f$ r_s \f$ — the ONLY
 * shape parameters of this family. There is NO width key: \f$\eta\f$ is a
 * pure gauge (\f$(c,\eta)\to(c s^5, \eta/s)\f$ leaves the function
 * invariant), so the descriptor evaluates with \f$\eta \equiv 1\f$ and
 * REJECTS an SGRID2B key if present. Optional \ref KDEG2B (a data_keys
 * entry: mJoin-splittable, never OPTIM) carries per-atom degrees 3|5.
 *
 * TYPE2B layout (d_base.h keys contract):
 * `D2_Knot5 <nC> <nK> <El> <El>` — one SIZE token per keys entry
 * ({CGRID2B}) then per data_keys entry ({KDEG2B}; 0 = all quintic).
 *
 * The family is \f$C^4\f$ globally — only the 5th derivative jumps at the
 * knots, so second-derivative observables (elastic constants) see no
 * knot-scale sawtooth at \f$O(h^2)\f$.
 *
 * Required keys:
 * \ref INIT2B \ref CGRID2B \ref SGRID2B
 */
class D2_Knot5 : public D2_Base {
private:
  std::string lab="D2_Knot5";
  tadah::core::v_type cgrid;
  tadah::core::v_type dgrid;  ///< optional per-atom degree 3|5 (empty = all 5)
  /** Last-resort guard (ctor): a knot beyond rcut is
   *  hard-truncated by the cutoff — warn, once per process
   *  (log_warning_once), with both values as the shortest text that reads
   *  back to them. Every command sets RCUT2B to the largest knot on a
   *  TYPE2B line of D2_Knot5 alone (potedit::normalize_knot_rcut), so the
   *  warning comes where the stated cutoff is kept: a D2_mJoin line, or a
   *  program or pair_style tadah that reads a potential file as written;
   *  no auto-raise here because widening fcut without resizing the caller's
   *  neighbour list would not recover the missing pairs. */
  void warn_if_knots_beyond_rcut();

  /** degree of atom c (KDEG2B absent => quintic) */
  int deg(std::size_t c) const {
    return c < dgrid.size() ? static_cast<int>(dgrid[c]) : 5;
  }
  /** value / r-derivative of atom c at x_c (degree-dispatched) */
  double kval(std::size_t c, double x_c, double f) const {
    return deg(c) == 3 ? K3L(x_c, f) : K5L(x_c, f);
  }
  double kder(std::size_t c, double x_c, double eta, double f,
              double fp) const {
    return deg(c) == 3 ? dK3L(x_c, eta, f, fp) : dK5L(x_c, eta, f, fp);
  }

public:
  D2_Knot5();
  D2_Knot5(const tadah::core::Context &context);
  void calc_aed(
    const int Zi,
    const int Zj,
    const double rij,
    const double ,
    tadah::core::aed_type &aed,
    const double scale=1) const override;
  void calc_dXijdri(
    const int Zi,
    const int Zj,
    const double rij,
    const double rij_sq,
    tadah::core::fd_type &fd_ij,
    const double scale=1) const override;
  void calc_all(
    const int Zi,
    const int Zj,
    const double rij,
    const double rij_sq,
    tadah::core::aed_type &aed,
    tadah::core::fd_type &fd_ij,
    const double scale=1) const override;

  /** Closed-form analytical derivatives w.r.t. hyperparameters — same
   *  shift-scale chain rule as D2_Blip (see d2_blip.h), with the K5L
   *  bare derivatives. The family is C^4, so the b''-based mixed force
   *  derivatives are SMOOTHER than the blip case (d2K5L is continuous).
   *
   *  Supported (theta_name, arg_pos) pairs:
   *   - "CGRID2B" arg_pos = k (1-based) -> dD/d cgrid[k-1]
   *   (no SGRID2B: the knot family has no width parameter)
   *   - "RCUT2B"  arg_pos = -1          -> dD/d rcut (all components)
   *  Anything else returns false (HYBRID FD-fallback, ANALYTICAL throw). */
  bool calc_dDdtheta(
      const std::string& theta_name,
      int                arg_pos,
      const int          Zi,
      const int          Zj,
      const double       rij,
      const double       rij_sq,
      tadah::core::aed_type& dD_buf,
      const double       scale = 1) const override;

  /** Closed-form mixed derivative d(fd_radial)/d theta — see D2_Blip. */
  bool calc_dXijdri_dtheta(
      const std::string& theta_name,
      int                arg_pos,
      const int          Zi,
      const int          Zj,
      const double       rij,
      const double       rij_sq,
      tadah::core::fd_type& dfd_ij,
      const double       scale = 1) const override;

  std::string label() const override;
  void init() override;
  std::optional<DescriptorMeta> meta() const override;
  std::optional<BasisKind> basis_kind() const override { return BasisKind::Knot5; }
};
} // namespace models
} // namespace tadah
#endif
