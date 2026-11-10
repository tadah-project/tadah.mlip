#ifndef M_KRR_TRAIN_H
#define M_KRR_TRAIN_H

#include <tadah/models/linear_regressor.h>
#include <tadah/models/functions/function_base.h>
#include <tadah/models/functions/kernels/kern_base.h>
#include <tadah/models/m_train.h>
#include <tadah/models/m_krr_core.h>
#include <tadah/core/maths.h>
#include <tadah/core/context.h>
#include <tadah/models/memory/imodels_workspace_manager.h>

#include <iostream>
#include <stdexcept>
#include <limits>
#include <string>

namespace tadah {
namespace models {

/**
 * @brief Refuses FIXINDEX and FIXWEIGHT for M_KRR implementation 2 (MODEL
 * M_KRR <kernel> 2, M_KRR_Train::train2).
 *
 * Its solve, (K + LAMBDA sigma^2 I) w = t on the basis vectors (sigma =
 * ESIGMA in eV, the error scale Tadah.MLIP passes), fits every weight at
 * once and pins none, so the keys would be ignored. A no-op without either
 * key; with either, whatever its values, throws std::runtime_error stating
 * that solve and naming @p kernel and implementation 1, which pins them.
 */
inline void refuse_fixed_weights_in_train2(const tadah::core::Context &ctx,
                                           const std::string &kernel) {
  if (!ctx.has_stored("FIXINDEX") && !ctx.has_stored("FIXWEIGHT")) return;
  throw std::runtime_error(
      "MODEL M_KRR " + kernel + " 2 cannot pin weights: implementation 2 "
      "solves (K + LAMBDA sigma^2 I) w = t, sigma = ESIGMA in eV, for the "
      "weights of all its basis vectors at once, so FIXINDEX and FIXWEIGHT "
      "would be ignored. Implementation 1 (MODEL M_KRR " + kernel + " 1) "
      "pins them; implementation 2 fits without FIXINDEX and FIXWEIGHT.");
}

/**
 * @brief The LAMBDA rules of M_KRR implementation 2 (`MODEL M_KRR
 * <kernel> 2`, M_KRR_Train::train2), checked on the configuration alone,
 * with its messages: LinearRegressor::require_lambda_request(), then
 * LAMBDA -1, which this solve, (K + LAMBDA sigma^2 I) w = t with a fixed
 * LAMBDA (sigma = ESIGMA in eV, the error scale Tadah.MLIP passes), does
 * not run; its message states that solve. tadah hpo applies them when a
 * run starts.
 *
 * @throws std::runtime_error naming the value, @p kernel and
 * implementation 1.
 */
inline void require_krr_implementation2_lambda(
    const tadah::core::Context &ctx, const std::string &kernel) {
  LinearRegressor::require_lambda_request(ctx);
  if (ctx.get<double>("LAMBDA") < 0)
    throw std::runtime_error(
        "MODEL M_KRR " + kernel + " 2 cannot use LAMBDA -1: implementation "
        "2 solves (K + LAMBDA sigma^2 I) w = t, sigma = ESIGMA in eV, with a "
        "fixed LAMBDA and does not run the evidence approximation. "
        "Implementation 1 (MODEL M_KRR " +
        kernel + " 1) with OALGO 3 runs it; LAMBDA >= 0 fits with either.");
}

/**
 * @class M_KRR_Train
 * @brief Implements Kernel Ridge Regression (KRR) training.
 *
 * Extends KRR core and train functionalities with specific implementations.
 *
 * train() solves for the weights of the design-matrix columns as they
 * are: for a non-linear kernel, column j holds K(b_j, x) summed over the
 * atoms (and its derivatives), and weight j belongs to basis vector b_j.
 * (The columns used to be rotated first by the right singular vectors of
 * the kernel matrix of the basis, EKM. The rotation was orthogonal, so it
 * did not change a fit without intercept columns, but with BIAS true it
 * left rotated column 0 out of the ridge penalty.)
 *
 * @tparam Kern Kernel type, default is Function_Base.
 */
template <class Kern = Function_Base&>
class M_KRR_Train : public M_KRR_Core<Kern>, public M_Train {
public:
    using M_KRR_Core<Kern>::trained;
    using M_KRR_Core<Kern>::context;
    using M_KRR_Core<Kern>::kernel;
    using M_KRR_Core<Kern>::weights;

    /**
     * @brief Constructor using a configuration object.
     *
     * Initializes KRR core with the given configuration.
     *
     * @param c Configuration object reference.
     */
    M_KRR_Train(tadah::core::Context &c) : M_KRR_Core<Kern>(c) {}

    /**
     * @brief Constructor using a kernel and configuration object.
     *
     * Initializes KRR core with the given kernel and configuration.
     *
     * @param kernel Kernel object.
     * @param c Configuration object reference.
     */
    M_KRR_Train(Kern &kernel, tadah::core::Context &c) : M_KRR_Core<Kern>(kernel, c) {}
    M_KRR_Train(Kern &kernel, tadah::core::Context &c, tadah::models::memory::IModelsWorkspaceManager &workspaceManager) :
    M_KRR_Core<Kern>(kernel, c),
    M_Train(workspaceManager)
  {}

    /**
     * @brief Trains the model on given feature and target matrices, whose
     *        observations are rows - cols (a design matrix with its
     *        ridge-augmentation rows and no rows of priority 0).
     */
  void train(tadah::core::phi_type &Phi, tadah::core::t_type &T) {
    train(Phi, T, LinearRegressor::augmented_observations(Phi));
  }

    /**
     * @brief Trains the model on given feature and target matrices.
     *
     * Uses the linear regressor on the design-matrix columns as they are;
     * the kernel's intercept columns (Function_Base::intercept_columns())
     * are left out of the ridge penalty. Nothing is written into the
     * Context but the ALPHA and BETA of LAMBDA -1.
     *
     * @param Phi Feature matrix.
     * @param T Target vector.
     * @param n_obs The observations: the data rows of Phi of priority
     *        above 0 that are not zero in every column (zero_rows()),
     *        without the ridge-augmentation rows.
     * @throws std::runtime_error if the object is already trained.
     */
  void train(tadah::core::phi_type &Phi, tadah::core::t_type &T, size_t n_obs) {
    if (trained) {
      throw std::runtime_error("This object is already trained!");
    }

    // The weights FIXWEIGHT pins (core::parse_fixindex: 1-based, in
    // declaration order, checked against the Phi.cols() weights and the
    // FIXWEIGHT values; a lone key is refused); none without the keys.
    std::vector<size_t> indices =
        tadah::core::parse_fixindex(context, Phi.cols());

    if (!indices.empty()) {
      // Adjust for 0-based indexing
      for (auto &i : indices) i--;

      tadah::core::t_type w_f(indices.size());
      context.get("FIXWEIGHT", w_f);

      // DesignMatrix's layout of the full matrix: its data rows, then one
      // ridge row per weight. The ridge rows of the pinned weights stay
      // (zero rows) when their columns go, as in M_BLR_Train, so the data
      // rows are counted here, before the columns are removed.
      const size_t data_rows = LinearRegressor::Layout::of(Phi).data_rows;

      tadah::core::t_type T_r;
      auto move_map = prep_train_with_residuals(Phi, T, indices, w_f, T_r);

      // The pinned columns leave the regression, as in M_BLR_Train: the
      // rightmost columns are the FIXINDEX ones. (They were set to 0 and
      // kept: an unpenalised zero column, a pinned intercept, made OALGO 2
      // and 4 singular.)
      Phi.resize(Phi.rows(), Phi.cols() - indices.size());
      if (Phi.cols() > 0) {
        // The kernel's intercept columns among the free ones, at their new
        // positions: the ridge leaves those out and penalises every other
        // free column. n_obs is that of the full design matrix.
        const std::vector<size_t> intercepts = kernel.intercept_columns(context);
        LinearRegressor::train(context, Phi, T_r, weights, *workspaceManager_,
                               free_intercept_columns(intercepts, move_map,
                                                      Phi.cols()),
                               n_obs,
                               LinearRegressor::Layout::fixed(data_rows, move_map,
                                                              Phi.cols()));
      } else {
        // Every weight is pinned: nothing to solve.
        weights = tadah::core::t_type();
      }

      // The solved weights back at the original positions of the free
      // columns, each pinned value at its own index.
      const size_t n_free = weights.size();
      tadah::core::t_type w_full(n_free + indices.size());
      for (size_t j = 0; j < n_free; ++j) {
        w_full[move_map[j]] = weights[j];
      }
      for (size_t k = 0; k < indices.size(); ++k) {
        w_full[indices[k]] = w_f[k];
      }
      weights = w_full;
      trained = true;
    }
    else {
      LinearRegressor::train(context, Phi, T, weights, *workspaceManager_,
                             kernel.intercept_columns(context), n_obs,
                             LinearRegressor::Layout::of(Phi));
      trained = true;
    }

  }

    /**
     * @brief Standard KRR training using covariance matrix computation.
     *
     * Trains the model on energies using the covariance matrix K: the
     * coefficients w of the basis vectors b_j (the columns of @p B) solve
     * (K + LAMBDA sigma^2 I) w = T, K_ij = K(b_i, b_j), sigma = @p
     * error_scale: the kernel ridge regression of the targets with every
     * residual over sigma, sum_i ((T_i - f(b_i)) / sigma)^2 + LAMBDA |f|^2,
     * so LAMBDA weighs against the residuals in units of sigma, as in
     * every other fit (Tadah.MLIP passes ESIGMA in eV for energies per
     * atom; 1, the default, weighs the residuals in the unit of T). A
     * non-linear kernel's
     * weights are w (it predicts sum_j w_j K(b_j, x)). Kern_Linear's are
     * the descriptor weights B w = sum_j w_j b_j, one per row of @p B: its
     * prediction is linear in the descriptor, weights . x (Kern_Linear::
     * epredict), which is sum_j w_j K(b_j, x) for these weights. It pins
     * no weight: a context with FIXINDEX or FIXWEIGHT is refused
     * (refuse_fixed_weights_in_train2).
     *
     * @param B Basis matrix.
     * @param T Target vector.
     * @param error_scale sigma, the error scale of every residual in the
     *        unit of @p T (> 0).
     * @throws std::runtime_error for a LAMBDA that requests nothing
     *         (LinearRegressor::require_lambda_request: a second value, a
     *         negative value other than -1), for LAMBDA -1 (the evidence
     *         approximation, which this solve does not run), or with
     *         FIXINDEX or FIXWEIGHT.
     * @throws std::logic_error for an error scale that is not a finite
     *         number > 0 (Tadah.MLIP refuses such an ESIGMA first).
     */
    void train2(tadah::core::Matrix &B, tadah::core::t_type &T,
                double error_scale = 1.0) {
        refuse_fixed_weights_in_train2(context, kernel.get_label());
        require_krr_implementation2_lambda(context, kernel.get_label());
        if (!(error_scale > 0.0) ||
            !(error_scale < std::numeric_limits<double>::infinity()))
            throw std::logic_error(
                "M_KRR_Train::train2: the error scale of the targets is a "
                "finite number > 0, not " + std::to_string(error_scale));
        // LAMBDA against the residuals over the error scale.
        const double lambda = context.template get<double>("LAMBDA") *
                              error_scale * error_scale;

        if (M_KRR_Core<Kern>::is_verbose()) std::cout << "Computing Kernel tadah::core::Matrix" << std::endl;
        tadah::core::Matrix K(B.cols(), B.cols());

        for (size_t i = 0; i < K.cols(); i++) {
            for (size_t j = 0; j < K.cols(); j++) {
                K(i, j) = kernel(B.col(i), B.col(j));
                if (i == j) K(i, j) += lambda;
            }
        }
        if (M_KRR_Core<Kern>::is_verbose()) std::cout << "tadah::core::Matrix condition number: " << condition_number(K) << std::endl;

        if (M_KRR_Core<Kern>::is_verbose()) std::cout << "Solving..." << std::flush;
        // The one solve of implementation 2: Cholesky of K + LAMBDA sigma^2
        // I. It reads no OALGO; a configuration with an OALGO other than
        // the default is refused before the fit (MLIP
        // refuse_what_krr_implementation2_ignores).
        weights = solve_posv(K, T);
        if (M_KRR_Core<Kern>::is_verbose()) std::cout << "Done" << std::endl;

        // Kern_Linear predicts weights . x: its weights are the descriptor
        // weights the coefficients make, B w.
        if (kernel.get_label() == "Kern_Linear") {
            tadah::core::t_type w(B.rows());
            for (size_t i = 0; i < B.rows(); ++i) {
                double s = 0;
                for (size_t j = 0; j < B.cols(); ++j) s += B(i, j) * weights(j);
                w(i) = s;
            }
            weights = w;
        }


        trained = true;

    }
};

} // namespace models
} // namespace tadah
#endif // M_KRR_TRAIN_H
