#ifndef DM_func_H
#define DM_func_H

#include <tadah/core/context.h>
#include <tadah/models/cutoffs.h>
#include <tadah/models/descriptors/d_base.h>
#include <tadah/models/descriptors/d_basis_functions.h>

#include <cstdlib>

namespace tadah {
namespace models {

class F_Base {
public:
  /** \brief True only for \f$\mathcal{F}(\rho)=\rho^2\f$ (F_SQ).
   *
   * DM_mEAD applies \f$\mathcal{F}\f$ to individual orbital densities
   * \f$\rho^{\eta,r_s,l_x,l_y,l_z}\f$ before the rotational-invariance
   * restoring quadratic sum.  This is invariant only when
   * \f$\mathcal{F}=\rho^2\f$; any other choice breaks SO(3) invariance for
   * \f$L>0\f$.
   */
  static constexpr bool rotationally_invariant = false;

  /**
   * @brief Whether this embedding function expands dimensions.
   *
   * When true, the embedding function takes a single rho value (from
   * radial grid of size 1) and produces multiple output channels
   * (one per embedding grid point).  DM_mEAD uses this to adjust
   * its inner loops accordingly.
   */
  static constexpr bool expands_dimensions = false;

  /**
   * @brief The list keys the function reads from the context
   *        (CEMBFUNC, SEMBFUNC); none for F_SQ.
   *
   * DM_mEAD gives them SIZE parameters on its TYPE line (its `keys`);
   * DM_Poly reads them whole (its `context_keys`).
   */
  static std::vector<std::string> context_keys() { return {}; }
  /** @brief Whether the function runs without its context_keys (F_SQRT:
   *  every SEMBFUNC value 1 when SEMBFUNC is not set). */
  static constexpr bool context_keys_optional = false;
  /**
   * @brief The scalar keys the function reads from the context (RCUTENV of
   *        F_mEnv); none for the others. A descriptor of the function
   *        depends on them (D_Base::depends_on).
   */
  static std::vector<std::string> scalar_keys() { return {}; }

  /**
   * @brief Returns the number of output channels per orbital per L.
   *
   * For most embedding functions this equals the radial grid size (ng).
   * For expanding functions (e.g. F_Blip) this equals the embedding
   * function grid size.
   */
  virtual size_t output_dim() const = 0;

  /**
   * @brief Calculates the embedding function value.
   * @param rho Electron density.
   * @param p Index for model parameters.
   * @return Function value F(rho).
   */
  virtual double calc_F(double rho, size_t p) const = 0;

  /**
   * @brief Calculates the first derivative of the embedding function.
   * @param rho Electron density.
   * @param p Index for model parameters.
   * @return Derivative dF/d(rho).
   */
  virtual double calc_dF(double rho, size_t p) const = 0;

  /**
   * @brief Closed-form derivative of calc_F w.r.t. a Context-tunable
   *        embedding-function parameter, with rho held fixed.
   *
   * Called by DM_mEAD<F>::calc_daed_dtheta on the embedding side
   * (theta_name in {CEMBFUNC, SEMBFUNC, RCUTENV, ...} — anything F
   * owns rather than DM_mEAD's radial layer). The radial side
   * (CGRIDMB / SGRIDMB / RCUTMB) is handled by DM_mEAD directly.
   *
   * Returns `true` iff this F owns the (theta_name, arg_pos) pair AND
   * the derivative is non-zero at output channel `p`. Component-local
   * theta (e.g. F_Blip's CEMBFUNC[i] only touches p == i-1) must
   * return `false` when `p` doesn't match — the orchestrator counts
   * `true` returns to decide whether the theta is FD-fallback-able.
   *
   * Defaults to `false` (no ownership). F_Blip and F_mEnv override.
   *
   * @param rho        Electron density at this orbital.
   * @param p          Output-channel index (0..output_dim()-1).
   * @param theta_name OPTIM key, upper-case.
   * @param arg_pos    1-based index for vector parameters, -1 for scalars.
   * @param out        Filled with d calc_F / d theta on success.
   */
  virtual bool calc_dF_dtheta(double /*rho*/, size_t /*p*/,
                              const std::string& /*theta_name*/,
                              int /*arg_pos*/,
                              double& /*out*/) const {
    return false;
  }

  /** \brief Second derivative of F: F''(rho, p) = d^2 F / d rho^2.
   *
   * Used by DM_mEAD<F>::calc_dXijdri_dtheta for the radial-theta chain
   * d(F'(rho))/d theta_radial = F''(rho) * drho/d theta_radial — i.e.
   * how the rho-dependence of theta propagates through F'(rho) into the
   * force descriptor.
   *
   * Returns `true` iff this F has an analytical second derivative AND
   * filled `out`. Default returns false; DM_mEAD falls back by reporting
   * the radial-theta force derivative as unowned and letting the
   * orchestrator route to FD.
   *
   * F_Blip, F_mEnv override; F_RLR / F_SQRT / F_SQ leave the default
   * (force-side radial closed forms not yet wired for those F's).
   */
  virtual bool calc_d2F(double /*rho*/, size_t /*p*/,
                        double& /*out*/) const {
    return false;
  }

  /** \brief Closed-form derivative of F'(rho) (i.e. dF/drho) wrt a
   *  Context-tunable embedding-function parameter, with rho held fixed.
   *
   * Companion to calc_dF_dtheta but for F' rather than F: used by
   * DM_mEAD<F>::calc_dXijdri_dtheta to chain
   *    d(fd_ij)/d theta_emb = scale * w * f[L,o] * (∂F'/∂theta) * term_k
   * for embedding-side theta (CEMBFUNC, SEMBFUNC, RCUTENV) — rho is
   * independent of these, so the only contribution is through F'.
   *
   * Returns `true` iff this F owns the (theta_name, arg_pos) pair AND
   * the derivative is non-zero at output channel `p`. Component-local
   * theta (CEMBFUNC[i] only touches p == i-1) returns false when p
   * doesn't match. Defaults to false; F_Blip and F_mEnv override. */
  virtual bool calc_dFp_dtheta(double /*rho*/, size_t /*p*/,
                               const std::string& /*theta_name*/,
                               int /*arg_pos*/,
                               double& /*out*/) const {
    return false;
  }

  virtual ~F_Base() = default;
};

/**
 * @class F_RLR
 * @brief Implements an embedding function of the form: \f$ s \rho \log(c \rho) \f$
 *
 * This class supports embedding functions characterized by two main parameters:
 * - **SEMBFUNC**: Controls the depth, \f$ s \f$, of the embedding function. 
 * - **CEMBFUNC**: Determines the x-intercept, with the x-intercept at \f$ 1/c \f$.
 * 
 *  Require: size(**SEMBFUNC**)=size(**CEMBFUNC**)=size(**[C/S]GRIDMB**)
 * 
 * The number of keys for these parameters must match the entries in the mEAD descriptor.
 *
 */
class F_RLR:  public F_Base {
private:
  const tadah::core::Context *context=nullptr;
  tadah::core::v_type sfuncgrid;   // controls depth
  tadah::core::v_type cfuncgrid;   // control x-intercep at 1/c
  double eps=1e-10; // small value to avoid log(0)
public:
  static std::vector<std::string> context_keys() { return {"CEMBFUNC", "SEMBFUNC"}; }
  F_RLR(const tadah::core::Context &context, size_t cSize, size_t sSize);
  F_RLR() = default;
  size_t output_dim() const override final { return sfuncgrid.size(); }
  double calc_F(double rho,size_t p) const override final;
  double calc_dF(double rho,size_t p) const override final;
};

/**
 * @class F_SQRT
 * @brief Implements \f$ s \sqrt{\rho} \f$.
 *
 * Optional parameter:
 * - **SEMBFUNC**: Controls the strength, \f$ s \f$, of the embedding function.
 *   If no; value is provided, the default is 1 for every s/cgrid point.
 *
 *  Require: size(**SEMBFUNC**)=(0 or size(**[C/S]GRIDMB**)) and size(**CEMBFUNC**)=0
 *
 */
class F_SQRT : public F_Base {
private:
  const tadah::core::Context *context=nullptr;
  tadah::core::v_type sfuncgrid;   // controls depth
  double eps=1e-10; // small value to avoid sqrt(0)

public:
  static std::vector<std::string> context_keys() { return {"SEMBFUNC"}; }
  static constexpr bool context_keys_optional = true;
  /**
   * @brief Constructs F_SQRT from a Context object.
   * @param context Reference to a Context instance.
   *
   * The constructor extracts or initializes relevant parameter for the model from
   * the provided configuration.
   *
   * SEMBFUNC valuas are set to 1 if not specified in the Context.
   */
  F_SQRT(const tadah::core::Context &context, size_t, size_t sSize);

  F_SQRT() = default;

  size_t output_dim() const override final { return sfuncgrid.size(); }

  /**
   * @brief Calculates the embedding function value using a square-root dependence.
   * @param rho Electron density.
   * @param p Index for model parameters. Here p=0
   * @return Embedding function value \f$ F(\rho) = s_{p} \sqrt{ c_{p} \rho } \f$.
   */
  double calc_F(double rho, size_t p) const override final;

  /**
   * @brief Calculates the derivative of the square-root embedding function.
   * @param rho Electron density.
   * @param p Index for model parameters. Here p=0
   * @return First derivative \f$ dF/d\rho = \frac{s_{p} \, {2 \sqrt{\rho}} \f$.
   */
  double calc_dF(double rho, size_t p) const override final;
};

class F_SQ : public F_Base {
private:
  const tadah::core::Context *context=nullptr;
  size_t out_dim = 0;

public:
  /** F_SQ = ρ² is the only channel function that preserves rotational
   *  invariance in DM_mEAD for all L. */
  static constexpr bool rotationally_invariant = true;

  F_SQ(const tadah::core::Context &context, size_t, size_t);

  F_SQ() = default;

  size_t output_dim() const override final { return out_dim; }

  double calc_F(double rho, size_t p) const override final;

  double calc_dF(double rho, size_t p) const override final;
};

  /**
   * @class F_Blip
   * @brief Implements a "blip" embedding function.
   *
   * F_Blip is a dimension-expanding embedding function:
   * it requires CGRID/SGRID of size 1 (a single radial basis function
   * producing one ρ per orbital) and CEMBFUNC/SEMBFUNC of size ≥ 1.
   * Each (orbital, embedding-grid-point) pair produces one descriptor
   * component, so the descriptor dimension per L is CEMBFUNC.size().
   *
   * @param context Reference to a Context instance.
   */
class F_Blip:  public F_Base {
private:
  const tadah::core::Context *context=nullptr;
  tadah::core::v_type sfuncgrid;   // controls width of the "blip"
  tadah::core::v_type cfuncgrid;   // control blip offset
public:
  static std::vector<std::string> context_keys() { return {"CEMBFUNC", "SEMBFUNC"}; }
  static constexpr bool expands_dimensions = true;

  F_Blip(const tadah::core::Context &context, size_t cSize, size_t sSize);
  F_Blip() = default;
  size_t output_dim() const override final { return sfuncgrid.size(); }
  double calc_F(double rho,size_t p) const override final;
  double calc_dF(double rho,size_t p) const override final;

  /** Closed-form derivatives for CEMBFUNC[i] / SEMBFUNC[i] with rho
   *  held fixed. Component-local: returns false unless p == i-1.
   *
   *  With x_c = eta_p * (rho - mu_p), the underlying chain is:
   *    dB(x_c)/dCEMBFUNC[i] = dB/dx_c * dx_c/dmu_p = -eta_p * dB/dx_c
   *    dB(x_c)/dSEMBFUNC[i] = dB/dx_c * dx_c/deta_p = (rho - mu_p) * dB/dx_c
   *
   *  Both expressed in terms of the existing dB(x_c, eta) = eta * dB/dx_c
   *  helper from d_basis_functions.h. */
  bool calc_dF_dtheta(double rho, size_t p,
                      const std::string& theta_name, int arg_pos,
                      double& out) const override final;

  /** F''(rho, p) = eta_p^2 * d2B/dx_c^2(x_c). Always returns true. */
  bool calc_d2F(double rho, size_t p, double& out) const override final;

  /** Closed-form derivatives of F'(rho, p) = eta_p * dB/dx_c w.r.t.
   *  CEMBFUNC[i] / SEMBFUNC[i] with rho held fixed (component-local on p).
   *
   *    d F'/d CEMBFUNC[i] (i-1 == p): -eta_p^2 * d2B/dx_c^2 (x_c)
   *    d F'/d SEMBFUNC[i] (i-1 == p): dB/dx_c(x_c) + eta_p*(rho-mu_p)*d2B/dx_c^2(x_c)
   *
   *  Returns false on unknown theta_name / out-of-range arg_pos /
   *  component mismatch. */
  bool calc_dFp_dtheta(double rho, size_t p,
                       const std::string& theta_name, int arg_pos,
                       double& out) const override final;
};

/**
 * @class F_mEnv
 * @brief F_Blip multiplied by an envelope cutoff that suppresses ρ near 0.
 *
 * Identical interface to F_Blip (dimension-expanding, requires CGRID/SGRID
 * of size 1 and equal-length CEMBFUNC/SEMBFUNC) but the embedded value is:
 * \f$ F(\rho) = f_{\text{env}}(\rho)\, B(\rho, \eta_p, \mu_p) \f$,
 * where \f$ f_{\text{env}} \f$ is a cutoff-style function (template parameter)
 * with width set by the new \ref RCUTENV key.
 *
 * Required Config keys: \ref RCUTENV \ref CEMBFUNC \ref SEMBFUNC.
 */
template <typename F>
class F_mEnv : public F_Base {
private:
  const tadah::core::Context *context=nullptr;
  tadah::core::v_type sfuncgrid;   // controls width of the "blip"
  tadah::core::v_type cfuncgrid;   // control blip offset
  F fcut;
public:
  static std::vector<std::string> context_keys() { return {"CEMBFUNC", "SEMBFUNC"}; }
  static std::vector<std::string> scalar_keys() { return {"RCUTENV"}; }
  static constexpr bool expands_dimensions = true;

  F_mEnv(const tadah::core::Context &c, size_t cSize, size_t sSize):
    context(&c),
    fcut(c.get<double>("RCUTENV"))
  {
    if (!context->get<bool>("INITMB")) return;
    D_Base::get_grid(*context,"SEMBFUNC",sfuncgrid);
    D_Base::get_grid(*context,"CEMBFUNC",cfuncgrid);
    if (cSize != 1 || sSize != 1) {
      throw std::runtime_error(
          "F_mEnv: requires CGRIDMB and SGRIDMB of size 1, got "
          + std::to_string(cSize) + " and " + std::to_string(sSize) + ".");
    }
    if (sfuncgrid.size() != cfuncgrid.size()) {
      throw std::runtime_error(
          "F_mEnv: SEMBFUNC and CEMBFUNC must have the same size, got "
          + std::to_string(sfuncgrid.size()) + " and "
          + std::to_string(cfuncgrid.size()) + ".");
    }
  }
  F_mEnv() = default;
  size_t output_dim() const override final { return sfuncgrid.size(); }
  double calc_F(double rho, size_t p) const override final {
    return fcut.calc(rho) * B(rho, sfuncgrid[p], cfuncgrid[p]);
  }
  double calc_dF(double rho, size_t p) const override final {
    return fcut.calc(rho) * dB(rho, sfuncgrid[p], cfuncgrid[p])
         + fcut.calc_prime(rho) * B(rho, sfuncgrid[p], cfuncgrid[p]);
  }
  /** Closed-form derivatives for F_mEnv's embedding-side theta.
   *
   *  Product rule on F(rho, p) = fcut(rho) * B(rho, eta_p, mu_p):
   *
   *    dF/dCEMBFUNC[i] (only p == i-1): fcut(rho) * dB/dmu_p
   *    dF/dSEMBFUNC[i] (only p == i-1): fcut(rho) * dB/deta_p
   *    dF/dRCUTENV   (every p):         dfcut/drcut(rho) * B(rho, eta_p, mu_p)
   *
   *  RCUTENV is the only fanned-out parameter (fcut is shared across
   *  all output channels). CEMBFUNC / SEMBFUNC are component-local
   *  exactly like F_Blip's overrides. */
  bool calc_dF_dtheta(double rho, size_t p,
                      const std::string& theta_name, int arg_pos,
                      double& out) const override final {
    out = 0.0;
    if (theta_name == "CEMBFUNC") {
      if (arg_pos < 1 ||
          static_cast<std::size_t>(arg_pos) > cfuncgrid.size()) return false;
      const std::size_t i = static_cast<std::size_t>(arg_pos - 1);
      if (i != p) return false;
      const double eta = sfuncgrid[p];
      const double mu  = cfuncgrid[p];
      const double x_c = eta * (rho - mu);
      /* dB/dmu = -eta * dB/dx_c = -dB(x_c, eta). */
      out = fcut.calc(rho) * (-dB(x_c, eta));
      return true;
    }
    if (theta_name == "SEMBFUNC") {
      if (arg_pos < 1 ||
          static_cast<std::size_t>(arg_pos) > sfuncgrid.size()) return false;
      const std::size_t i = static_cast<std::size_t>(arg_pos - 1);
      if (i != p) return false;
      const double eta = sfuncgrid[p];
      const double mu  = cfuncgrid[p];
      if (eta == 0.0) return true;  /* out=0, theta is owned. */
      const double x_c = eta * (rho - mu);
      /* dB/deta = (rho - mu) * dB/dx_c = ((rho-mu)/eta) * dB(x_c, eta). */
      out = fcut.calc(rho) * (rho - mu) / eta * dB(x_c, eta);
      return true;
    }
    if (theta_name == "RCUTENV") {
      /* Fanned out — every p contributes. dfcut/drcut comes from the
       * cutoff's calc_drcut method. */
      out = fcut.calc_drcut(rho) * B(rho, sfuncgrid[p], cfuncgrid[p]);
      return true;
    }
    return false;
  }

  /** F''(rho, p) via product rule on F(rho, p) = fcut(rho) * B(x_c),
   *  x_c = eta_p * (rho - mu_p):
   *
   *    F''(rho) = fcut''(rho) * B(x_c)
   *             + 2 * fcut'(rho) * eta_p * b'(x_c)
   *             + fcut(rho)     * eta_p^2 * b''(x_c)
   *
   *  fcut'' is supplied by Cut_Base::calc_double_prime (default 0; smooth
   *  cutoffs override). Closes the radial-θ force chain for
   *  DM_mEAD<F_mEnv> configs.
   *
   *  Note on smoothness: the blip B is C^2 globally (b'' is continuous,
   *  piecewise linear; only b''' jumps at the knots x_c = 0, +-1, +-2).
   *  Caller fixtures that probe rho exactly on a blip knot see the b'''
   *  jump in FD probes of this function (same geometry-sensitivity note
   *  as F_Blip in project_hpo_analytical_gradients_phase2). */
  bool calc_d2F(double rho, size_t p, double& out) const override final {
    const double eta = sfuncgrid[p];
    const double mu  = cfuncgrid[p];
    const double x_c = eta * (rho - mu);
    out = fcut.calc_double_prime(rho) * B(x_c)
        + 2.0 * fcut.calc_prime(rho)  * eta * dB(x_c, 1.0)
        + fcut.calc(rho)              * eta * eta * d2B(x_c);
    return true;
  }

  /** Closed-form derivatives of F'(rho, p) wrt embedding-side theta
   *  with rho held fixed.
   *
   *    F'(rho, p) = fcut'(rho) * B(x_c) + fcut(rho) * eta_p * b'(x_c),
   *    x_c = eta_p * (rho - mu_p)
   *
   *    d F'/d CEMBFUNC[i] (p == i-1):
   *      -eta_p * [fcut'(rho) * b'(x_c) + fcut(rho) * eta_p * b''(x_c)]
   *    d F'/d SEMBFUNC[i] (p == i-1):
   *      fcut'(rho) * (rho-mu_p) * b'(x_c)
   *      + fcut(rho) * [b'(x_c) + eta_p*(rho-mu_p)*b''(x_c)]
   *    d F'/d RCUTENV (every p):
   *      fcut'_rcut(rho) * B(x_c) + fcut_rcut(rho) * eta_p * b'(x_c)
   *      (fcut_rcut and fcut'_rcut come from Cut_Base::calc_drcut /
   *       calc_prime_drcut, treating rho as the cutoff input.) */
  bool calc_dFp_dtheta(double rho, size_t p,
                       const std::string& theta_name, int arg_pos,
                       double& out) const override final {
    out = 0.0;
    if (theta_name == "CEMBFUNC") {
      if (arg_pos < 1 ||
          static_cast<std::size_t>(arg_pos) > cfuncgrid.size()) return false;
      const std::size_t i = static_cast<std::size_t>(arg_pos - 1);
      if (i != p) return false;
      const double eta = sfuncgrid[p];
      const double mu  = cfuncgrid[p];
      const double x_c = eta * (rho - mu);
      const double b1  = dB(x_c, 1.0);
      const double b2  = d2B(x_c);
      out = -eta * (fcut.calc_prime(rho) * b1 + fcut.calc(rho) * eta * b2);
      return true;
    }
    if (theta_name == "SEMBFUNC") {
      if (arg_pos < 1 ||
          static_cast<std::size_t>(arg_pos) > sfuncgrid.size()) return false;
      const std::size_t i = static_cast<std::size_t>(arg_pos - 1);
      if (i != p) return false;
      const double eta = sfuncgrid[p];
      const double mu  = cfuncgrid[p];
      const double x_c = eta * (rho - mu);
      const double b1  = dB(x_c, 1.0);
      const double b2  = d2B(x_c);
      out = fcut.calc_prime(rho) * (rho - mu) * b1
          + fcut.calc(rho)       * (b1 + eta * (rho - mu) * b2);
      return true;
    }
    if (theta_name == "RCUTENV") {
      const double eta = sfuncgrid[p];
      const double mu  = cfuncgrid[p];
      const double x_c = eta * (rho - mu);
      const double b   = B(x_c);
      const double b1  = dB(x_c, 1.0);
      out = fcut.calc_prime_drcut(rho) * b
          + fcut.calc_drcut(rho)       * eta * b1;
      return true;
    }
    return false;
  }
};

} // namespace models
} // namespace tadah

#endif
