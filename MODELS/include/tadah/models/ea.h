#ifndef SVD_MULTIPLIER_H
#define SVD_MULTIPLIER_H
#include <tadah/models/svd.h>
#include <tadah/core/lapack.h>
#include <vector>
#include <algorithm>

/**
 * @class SVDMultiplier
 * @brief Class to compute A * x using the SVD decomposition of A without allocating memory during computation.
 */
class SVDMultiplier {
public:
    /**
     * @brief Constructor that initializes the multiplier with the SVD decomposition.
     *
     * @param svd The SVD object containing the decomposition of A.
     */
    explicit SVDMultiplier(const tadah::models::SVD& svd)
        : m_(svd.rows()), n_(svd.cols()), min_mn_(std::min(m_, n_)),
          U_(svd.getU()), S_(svd.getS()), VT_(svd.getVT()),
          y_(min_mn_), z_(min_mn_) {}

    /**
     * @brief Computes the matrix-vector product A * x using the SVD decomposition.
     *
     * @param x Pointer to the input vector x (of size n).
     * @param result Pointer to the output vector where A * x will be stored (of size m).
     */
    void compute(const double* x, double* result) {
        // Step 1: Compute y = V^T * x
        {
            char trans = 'N'; // V^T is already transposed
            int m = min_mn_;
            int n = n_;
            double alpha = 1.0;
            int lda = m;
            int incx = 1;
            double beta = 0.0;
            int incy = 1;

      tadah::core::dgemv_(&trans, &m, &n, &alpha, VT_, &lda, x, &incx, &beta, y_.data(), &incy);
        }

        // Step 2: Compute z = S * y (element-wise multiplication)
        for (int i = 0; i < min_mn_; ++i) {
            z_[i] = S_[i] * y_[i];
        }

        // Step 3: Compute result = U * z
        {
            char trans = 'N';
            int m = m_;
            int n = min_mn_;
            double alpha = 1.0;
            int lda = m;
            int incx = 1;
            double beta = 0.0;
            int incy = 1;

      tadah::core::dgemv_(&trans, &m, &n, &alpha, U_, &lda, z_.data(), &incx, &beta, result, &incy);
        }
    }

private:
    int m_;                  // Number of rows in A
    int n_;                  // Number of columns in A
    int min_mn_;             // Minimum of m and n

    const double* U_;        // Pointer to U matrix
    const double* S_;        // Pointer to singular values
    const double* VT_;       // Pointer to V^T matrix

    std::vector<double> y_;  // Intermediate vector y (size min_mn_)
    std::vector<double> z_;  // Intermediate vector z (size min_mn_)
};
#endif

#ifndef M_EA_H
#define M_EA_H

#include <tadah/core/context.h>
#include <tadah/models/evidence_approximation_error.h>
#include <tadah/models/svd.h>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <tadah/models/ols.h>
#include <tadah/core/core_types.h>
#include <tadah/core/lapack.h>
#include <tadah/core/logger.h>
#include <tadah/core/number_format.h>
#include <cstdio>

namespace tadah {
namespace models {

/**
 * @class EA
 * @brief The evidence approximation of Bayesian linear regression (LAMBDA
 *        -1): iterates alpha = gamma / |w|^2 and beta = (N - gamma) / RSS
 *        to their fixed point, lambda = alpha / beta.
 *
 * N is the number of observations, @p n_obs of the constructor: the data
 * rows of the design matrix of priority above 0 that are not zero in every
 * column (zero_rows()), without the ridge-augmentation rows the design matrix appends
 * (one per column) and without the rows of priority 0, which are zero rows
 * of the weighted matrix. gamma = sum_i beta s_i^2 / (beta s_i^2 + alpha)
 * runs over the directions the solve keeps (s_i above
 * OLS::pseudoinverse_threshold), as
 * the weights do. (Before, N was max(rows,
 * cols), which counted the augmentation rows and the weight-0 rows, and
 * gamma kept every s_i with beta s_i^2 above the smallest double.)
 *
 * The iteration converges when the relative changes of alpha and beta
 * both fall below 5e-12, and stops after 41 iterations otherwise (run()
 * returns 0; stopped_at_limit_text() says so). An alpha or beta that is
 * not finite is refused, before lambda is solved with: |w|^2 = 0 (every
 * weight 0, as when every target is zero, or too small to square in double
 * precision) makes alpha infinite or nan, and a fit that reproduces every
 * target (RSS = 0) makes beta infinite.
 *
 * run() writes nothing on standard output. Its iterations (the relative
 * changes of alpha and beta, alpha, beta, lambda and gamma of each) and how
 * it ended are one INFO message of the Logger (VERBOSE 2); a caller reports
 * a stop at the limit (LinearRegressor::train: a WARNING).
 */
class EA {
  private:
    const tadah::core::Context &context;
    tadah::models::SVD &svd;
    tadah::core::t_type &T;
    size_t n_obs;
    size_t iterations_ = 0;
    double last_change_alpha_ = 0.0;
    double last_change_beta_ = 0.0;
    double alpha_ = 0.0;
    double beta_ = 0.0;

    /* True for a value that is not NaN or +-inf; a test on the bits, which
     * -ffast-math does not optimise away. */
    static bool finite_bits(double v) {
      std::uint64_t u = 0;
      std::memcpy(&u, &v, sizeof u);
      return (u & UINT64_C(0x7ff0000000000000)) != UINT64_C(0x7ff0000000000000);
    }
  public:
    /** The iterations of the last run() stop after this many updates
     *  without convergence. */
    static constexpr size_t max_iterations = 41;
    /** The relative change of alpha and of beta below which run()
     *  converges. */
    static constexpr double tolerance = 5e-12;

    EA(const tadah::core::Context &c, tadah::models::SVD &svd, tadah::core::t_type &T,
       size_t n_obs):
      context(c),
      svd(svd),
      T(T),
      n_obs(n_obs)
  {}

    /** Iterates alpha and beta from the values given to their fixed
     *  point; @p alpha and @p beta receive the last ones.
     *  @return 1 when it converged, 0 when it stopped after
     *  max_iterations (stopped_at_limit_text()).
     *  @throws EvidenceApproximationError when an iteration gives an alpha
     *  or beta that is not finite, or with no more observations than
     *  effective weights. */
    int run(double &alpha, double &beta) {
      // Phi = USV^T
      const size_t N = n_obs;
      size_t sizeS = std::min(svd.rows(), svd.cols());
      size_t maxsteps=max_iterations-1;
      const double EPS2 = tolerance;
      int convergence_status = 1;
      double test1 = 2.0*EPS2;
      double test2 = 2.0*EPS2;
      double alpha_old = alpha;
      double beta_old = beta;
      size_t counter=0;
      double lambda = alpha/beta;

      double *s = svd.getS();

      // square of the S matrix contains all eigenvalues
      // of Phi^T*Phi = (VSU^T)(USV^T) = VS^2V^T = S^2
      double *eval = new double[sizeS];
      for (size_t i=0; i<sizeS; ++i)
        eval[i] = s[i]*s[i];

      // The iterations, one row each, for the INFO message of how the
      // iteration ended (log_iterations).
      std::string rows;
      auto log_iterations = [&rows](const std::string &how) {
        char head[96];
        std::snprintf(head, sizeof head, "%9s  %9s  %9s  %9s  %9s  %9s",
                      "del alpha", "del beta", "alpha", "beta", "lambda",
                      "gamma");
        tadah::core::Logger::instance().log(
            tadah::core::LogLevel::INFO,
            "LAMBDA -1 (evidence approximation): " + how + ":\n" + head +
                rows);
      };
      // D is a filter factors matrix
      double *d = new double[sizeS];

      tadah::core::t_type m_n(sizeS);
      int m = svd.rows();
      int n = svd.cols();
      tadah::core::t_type Tpred(m);
    double rcond = context.size("OALGO")==2 ? context.get<double>("OALGO",1) : 1e-8;
      // the directions the solve keeps (solveWithPseudoinverse)
      const double thresh = OLS::pseudoinverse_threshold(s, static_cast<int>(sizeS), rcond);
    //
        // Create a workspace manager
        tadah::models::memory::ModelsWorkspaceManager workspaceManager;

        // Create OLS instance with the workspace manager
        tadah::models::OLS ols(workspaceManager);

        SVDMultiplier svdMultiplier(svd);
      while (test1>EPS2 || test2>EPS2) {

        // regularised least square problem (Tikhonov Regularization):
        /*RidgeRegression::solve(svd,T,m_n,lambda,rcond);*/
        /*OLS::solve_with_svd_with_workspace(m, n, T, m_n, svd, ws, rcond, lambda);*/
        ols.solveWithPseudoinverse(m, n, T, m_n, svd.getSVDWorkspace(), rcond, lambda);
        double gamma = 0.0;
        for (size_t j=0; j<sizeS; j++) {
          if (s[j] > thresh) {
            gamma += (beta*eval[j])/(beta*eval[j]+alpha);
          }
        }
        const double wnorm2 = m_n*m_n;
        alpha = gamma / wnorm2;

        /*tadah::core::t_type Tpred = svd.predict(m_n,t,x);*/
        svdMultiplier.compute(m_n.data(), Tpred.data());
        double temp = (T-Tpred)*(T-Tpred);

        if (!(static_cast<double>(N) > gamma)) {
          delete [] eval;
          delete [] d;
          log_iterations("refused at iteration " + std::to_string(counter + 1));
          throw EvidenceApproximationError(
              "LAMBDA -1 (evidence approximation): the fit has " +
              std::to_string(N) + " observations (rows of priority above 0 "
              "that are not zero in every column) for " +
              std::to_string(gamma) +
              " effective parameters; it needs more observations than "
              "effective parameters to estimate the noise: more data, or "
              "LAMBDA >= 0, fits.");
        }
        beta = (static_cast<double>(N)-gamma)/temp;
        if (!finite_bits(alpha) || !finite_bits(beta)) {
          delete [] eval;
          delete [] d;
          std::string why;
          if (!(wnorm2 > 0.0))
            why += "|w|^2 is 0 (the weights of the solve are 0, as when "
                   "every target is zero, or too small to square in double "
                   "precision, as when the scales of the design matrix and "
                   "the targets are too far apart), so alpha = gamma / "
                   "|w|^2 is not finite";
          if (!(temp > 0.0))
            why += std::string(why.empty() ? "" : "; ") +
                   "the weights reproduce every target (the residual sum of "
                   "squares is 0, as when every target is zero), so beta = "
                   "(n_obs - gamma) / RSS is not finite";
          if (why.empty())
            why = "|w|^2 = " + tadah::core::format_roundtrip(wnorm2) +
                  " and RSS = " + tadah::core::format_roundtrip(temp) +
                  " are beyond double precision";
          log_iterations("refused at iteration " + std::to_string(counter + 1));
          throw EvidenceApproximationError(
              "LAMBDA -1 (evidence approximation): iteration " +
              std::to_string(counter + 1) + " gave alpha " +
              tadah::core::format_roundtrip(alpha) + " and beta " +
              tadah::core::format_roundtrip(beta) + ", which are not all "
              "finite: " + why + ". The prior and the noise cannot be "
              "estimated from these targets; no weights were fitted "
              "(LAMBDA >= 0 fits without the estimate).");
        }

        test1 = std::fabs(alpha_old-alpha)/alpha;
        test2 = std::fabs(beta_old-beta)/beta;
        {
          char row[128];
          std::snprintf(row, sizeof row, "\n%.3e  %.3e  %.3e  %.3e  %.3e  %.3e",
                        test1, test2, alpha, beta, alpha / beta, gamma);
          rows += row;
        }

        lambda = alpha/beta;
        alpha_old = alpha;
        beta_old = beta;
        last_change_alpha_ = test1;
        last_change_beta_ = test2;

        // The limit stops only an iteration that has not converged: one
        // whose last update brings both changes below the tolerance has
        // converged, on whichever iteration it does.
        if (++counter>maxsteps && (test1>EPS2 || test2>EPS2)) {
          convergence_status = 0;
          break;
        }
      }
      iterations_ = counter;
      alpha_ = alpha;
      beta_ = beta;
      log_iterations(convergence_status
                         ? "converged in " + std::to_string(counter) +
                               " iterations"
                         : "stopped at its iteration limit, " +
                               std::to_string(counter) + " iterations");

      delete [] eval;
      delete [] d;


      return convergence_status;
    };

    /** The iterations of the last run(). */
    size_t iterations() const { return iterations_; }

    /** What a WARNING says of a run() that returned 0: the iteration
     *  limit, the last relative changes of alpha and beta, the tolerance,
     *  and that the fit takes the last alpha and beta. */
    std::string stopped_at_limit_text() const {
      return "LAMBDA -1 (evidence approximation): stopped at its iteration "
             "limit, " + std::to_string(iterations_) + " iterations, without "
             "converging: the last relative changes of alpha and beta were " +
             tadah::core::format_sig(last_change_alpha_, 3) + " and " +
             tadah::core::format_sig(last_change_beta_, 3) + " (convergence "
             "needs both below " + tadah::core::format_sig(tolerance, 3) +
             "). The fit takes lambda = alpha / beta of the last iteration "
             "(alpha " + tadah::core::format_sig(alpha_, 6) + ", beta " +
             tadah::core::format_sig(beta_, 6) + "), which the potential "
             "records as ALPHA and BETA.";
    }
};
} // namespace models
} // namespace tadah
#endif

