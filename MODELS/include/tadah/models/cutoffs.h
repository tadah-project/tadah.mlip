#ifndef CUTOFFS_H
#define CUTOFFS_H

#include <string>
#include <tadah/core/context.h>
#include <tadah/core/registry.h>

namespace tadah {
namespace models {

/** Base class to be inherited by all cutoffs */
class Cut_Base {
    public:
        enum class Kind { Standard, Discontinuous, Inverse };
        virtual ~Cut_Base();
        virtual std::string label()=0;
        virtual double calc(double r)=0;
        virtual double get_rcut()=0;
        virtual void set_rcut(const double r)=0;
        virtual double get_rcut_sq()=0;
        virtual double calc_prime(double r)=0;
        /** Closed-form derivative of f_c(r) with respect to r_cut.
         *
         * Used by HPO analytical / hybrid gradients to chain RCUT2B /
         * RCUT_MB through descriptors that consume the cutoff. Default
         * is 0 (Cut_Dummy is piecewise-constant in rcut almost
         * everywhere). Smooth cutoffs override.
         */
        virtual double calc_drcut(double /*r*/) { return 0.0; }
        /** Closed-form mixed derivative d(df_c/dr)/d r_cut, at fixed r.
         *
         * Required by force-row chain rule in HPO analytical / hybrid
         * gradients: d2_base::calc_dXijdri (the radial force-descriptor
         * coefficient) is a product G * f_c'(r) + ... so chaining RCUT
         * through the force chain needs d(f_c'(r))/d rcut on top of
         * df_c/d rcut already exposed via calc_drcut. Default is 0
         * (Cut_Dummy's calc_prime is identically zero). Smooth cutoffs
         * override.
         */
        virtual double calc_prime_drcut(double /*r*/) { return 0.0; }
        /** Closed-form second derivative d^2 f_c / dr^2 at fixed rcut.
         *
         * Required by F_mEnv::calc_d2F to close the radial-θ force chain
         * for DM_mEAD<F_mEnv> configs: F''(rho) = fcut''(rho)*B(rho)
         * + 2 fcut'(rho)*B'(rho) + fcut(rho)*B''(rho), and B''(rho) is
         * already available; only fcut''(rho) was missing. Default is 0
         * (Cut_Dummy's f is piecewise-constant; its second derivative is
         * identically zero away from r=rcut). Smooth cutoffs override.
         */
        virtual double calc_double_prime(double /*r*/) { return 0.0; }
        void test_rcut(const double r);
        /** Behavioural kind; drives test dispatch without string comparisons. */
        virtual Kind kind() const { return Kind::Standard; }
        /** Expected value of calc() at exactly r == rcut. */
        virtual double val_at_rcut() const { return 0.0; }
};

/**
 * @class Cut_Dummy
 * @brief Represents a basic cutoff function with a sharp transition at the cutoff radius.
 *
 * The `Cut_Dummy` class implements a simple cutoff function defined by:
 * \f[
 * f_c(r) =
 * \begin{cases}
 * 1, & \text{if } r \leq r_{\text{cut}} \\
 * 0, & \text{if } r > r_{\text{cut}}
 * \end{cases}
 * \f]
 *
 * and its derivative:
 * \f[
 * f_c'(r) = 0
 * \f]
 *
 * where:
 * - \f$ r \f$ is the radial distance.
 * - \f$ r_{\text{cut}} \f$ is the cutoff radius.
 *
 * **Characteristics:**
 * - The function value is constant (1) within the cutoff radius and zero beyond it.
 * - The derivative of the function is zero everywhere except at \f$ r = r_{\text{cut}} \f$ where it is undefined due to the discontinuity.
 *
 * **Note:** Since the function is discontinuous at \f$ r = r_{\text{cut}} \f$ its derivative is not defined at that point. In practical computations, the derivative function `calc_prime` returns `0.0` for all \f$ r \f$
 */
class Cut_Dummy : public Cut_Base {
    private:
        std::string lab = "Cut_Dummy";
        double rcut, rcut_sq, rcut_inv;
    public:
        Cut_Dummy();
        Cut_Dummy(double rcut);
        std::string label() ;
        void set_rcut(const double r);
        double get_rcut();
        double get_rcut_sq();
        double calc(double r);
        double calc_prime(double r);
        Kind kind() const override { return Kind::Discontinuous; }
};

/**
 * @class Cut_Cos
 * @brief Cosine cutoff function.
 *
 * The `Cut_Cos` class implements a smooth cosine cutoff function defined by:
 * \f[
 * f_c(r) =
 * \begin{cases}
 *  \dfrac{1}{2}\left[ \cos\left( \dfrac{\pi r}{r_c} \right) + 1 \right], & \text{if } r \leq r_c \\
 *  0, & \text{if } r > r_c
 * \end{cases}
 * \f]
 *
 * This function smoothly transitions from 1 to 0 over the interval \f$ 0 \leq r \leq r_c \f$.
 * It is commonly used in molecular simulations to smoothly truncate interactions without introducing discontinuities in the potential energy or its derivatives.
 *
 * **Reference:**
 *
 * Behler, J., & Parrinello, M. (2007). *Generalized neural-network representation of high-dimensional potential-energy surfaces*. Physical Review Letters, 98(14), 146401. [DOI:10.1103/PhysRevLett.98.146401](https://doi.org/10.1103/PhysRevLett.98.146401)
 */
class Cut_Cos : public Cut_Base {
    private:
        std::string lab = "Cut_Cos";
        double rcut, rcut_sq, rcut_inv;
    public:
        Cut_Cos();
        Cut_Cos(double rcut);
        std::string label() ;
        void set_rcut(const double r);
        double get_rcut();
        double get_rcut_sq();
        double calc(double r);
        double calc_prime(double r);
        double calc_drcut(double r) override;
        double calc_prime_drcut(double r) override;
        double calc_double_prime(double r) override;
};

/**
 * @class Cut_Tanh
 * @brief Hyperbolic tangent cutoff function.
 *
 * The `Cut_Tanh` class implements a smooth cutoff function using the hyperbolic tangent, defined by:
 * \f[
 * f_c(r) =
 * \begin{cases}
 *  \tanh^3\left( 1 - \dfrac{r}{r_c} \right), & \text{if } r \leq r_c \\
 *  0, & \text{if } r > r_c
 * \end{cases}
 * \f]
 *
 * This function transitions from \f$\tanh^3(1)\f$ at zero to 0 at the cutoff,
 * with C2 matching to zero. It is not peak-normalized to 1.
 *
 * **Reference:**
 *
 * Behler, J. (2011). Atom-centered symmetry functions for constructing high-dimensional neural network potentials. *Journal of Chemical Physics*, 134(7), 074106. [DOI:10.1063/1.3553717](https://doi.org/10.1063/1.3553717)
 */
class Cut_Tanh : public Cut_Base {
    private:
        std::string lab = "Cut_Tanh";
        double rcut, rcut_sq, rcut_inv;
    public:
        Cut_Tanh();
        Cut_Tanh(double rcut);
        std::string label() ;
        void set_rcut(const double r);
        double get_rcut();
        double get_rcut_sq();
        double calc(double r);
        double calc_prime(double r);
        double calc_drcut(double r) override;
        double calc_prime_drcut(double r) override;
        double calc_double_prime(double r) override;
};

/**
 * @class Cut_PolyS
 * @brief Polynomial cutoff function ensuring smoothness up to the second derivative.
 *
 * The `Cut_PolyS` class implements a polynomial cutoff function defined as:

 * \f[
 * f_c(r) =
 * \begin{cases}
 * 1, & \text{if } r \leq r_{\text{inner}} \\
 * \left( r - r_c + 1 \right )^3 \left[ r \left( 15 - 6 r \right ) - 10 \right ] + 1, & \text{if } r_{\text{inner}} < r \leq r_c \\
 * 0, & \text{if } r > r_c
 * \end{cases}
 * \f]

 * where:
 * - \f$ r \f$ is the radial distance.
 * - \f$ r_c \f$ is the cutoff radius.
 * - \f$ r_{\text{inner}} = r_c - 1 \f$ is the inner cutoff radius.

 * This function provides a smooth transition from 1 to 0 between \f$ r_{\text{inner}} \f$ and \f$ r_c \f$, ensuring continuity and smoothness in the potential and its derivatives up to the second order.

 * **Reference:**

 * Singraber, A., Behler, J., & Dellago, C. (2019). *Library-Based LAMMPS Implementation of High-Dimensional Neural Network Potentials*. Journal of Chemical Theory and Computation, 15(1), 182–190. [DOI:10.1021/acs.jctc.8b00770](https://doi.org/10.1021/acs.jctc.8b00770)
 */
class Cut_PolyS : public Cut_Base {
    private:
        std::string lab = "Cut_PolyS";
        double rcut, rcut_sq, rcut_inv;
        double rcut_inner;
    public:
        Cut_PolyS();
        Cut_PolyS(double rcut);
        std::string label() ;
        void set_rcut(const double r);
        double get_rcut();
        double get_rcut_sq();
        double calc(double r);
        double calc_prime(double r);
        double calc_drcut(double r) override;
        double calc_prime_drcut(double r) override;
        double calc_double_prime(double r) override;
};
/**
 * @class Cut_Poly
 * @brief Represents a 5th order polynomial cutoff function with zero first and second derivatives at the cutoff radius.
 *
 * This class implements a smooth cutoff function defined by:
 * \f[
 * f(r) =
 * \begin{cases}
 * 1 - 10 \left( \dfrac{r}{r_{\text{cut}}} \right)^3 + 15 \left( \dfrac{r}{r_{\text{cut}}} \right)^4 - 6 \left( \dfrac{r}{r_{\text{cut}}} \right)^5, & \text{for } r \leq r_{\text{cut}} \\
 * 0, & \text{for } r > r_{\text{cut}}
 * \end{cases}
 * \f]
 * and its derivative:
 * \f[
 * f'(r) =
 * \begin{cases}
 * \left( -30 \left( \dfrac{r}{r_{\text{cut}}} \right)^2 + 60 \left( \dfrac{r}{r_{\text{cut}}} \right)^3 - 30 \left( \dfrac{r}{r_{\text{cut}}} \right)^4 \right) \dfrac{1}{r_{\text{cut}}}, & \text{for } r \leq r_{\text{cut}} \\
 * 0, & \text{for } r > r_{\text{cut}}
 * \end{cases}
 * \f]
 * where:
 * - \f$ r \f$ is the radial distance.
 * - \f$ r_{\text{cut}} \f$ is the cutoff radius.
 *
 * The function smoothly transitions from 1 to 0 within the cutoff radius \f$ r_{\text{cut}} \f$, ensuring that the function and its first and second derivatives are zero at \f$ r \geq r_{\text{cut}} \f$.
 *
 */
class Cut_Poly : public Cut_Base {
    private:
        std::string lab = "Cut_Poly";
        double rcut, rcut_sq, rcut_inv;
    public:
        Cut_Poly();
        Cut_Poly(double rcut);
        std::string label() ;
        void set_rcut(const double r);
        double get_rcut();
        double get_rcut_sq();
        double calc(double r);
        double calc_prime(double r);
        double calc_drcut(double r) override;
        double calc_prime_drcut(double r) override;
        double calc_double_prime(double r) override;
};
/**
 * @class Cut_CosS
 * @brief Smooth cosine cutoff function with a smoothing interval.
 *
 * The `Cut_CosS` class implements a smooth cosine cutoff function that transitions smoothly from 1 to 0 over a specified smoothing interval between the inner cutoff radius \f$ r_{\text{inner}} \f$ and the outer cutoff radius \f$ r_{\text{cut}} \f$ The function is defined as:
 *
 * \f[
 * f(r) =
 * \begin{cases}
 * 1, & \text{if } r \leq r_{\text{inner}} \\
 * \dfrac{1}{2}\left[1 + \cos\left( \dfrac{\pi (r - r_{\text{inner}})}{r_{\text{cut}} - r_{\text{inner}}} \right) \right], & \text{if } r_{\text{inner}} < r < r_{\text{cut}} \\
 * 0, & \text{if } r \geq r_{\text{cut}}
 * \end{cases}
 * \f]
 *
 * and its derivative:
 *
 * \f[
 * f'(r) =
 * \begin{cases}
 * 0, & \text{if } r \leq r_{\text{inner}} \text{ or } r \geq r_{\text{cut}} \\
 * -\dfrac{\pi}{2 (r_{\text{cut}} - r_{\text{inner}})} \sin\left( \dfrac{\pi (r - r_{\text{inner}})}{r_{\text{cut}} - r_{\text{inner}}} \right), & \text{if } r_{\text{inner}} < r < r_{\text{cut}}
 * \end{cases}
 * \f]
 *
 * where:
 *
 * - \f$ r \f$ is the radial distance.
 * - \f$ r_{\text{cut}} \f$ is the outer cutoff radius (`rcut`).
 * - \f$ r_{\text{inner}} \f$ is the inner cutoff radius (`rcut_inner`), defined as \f$ r_{\text{inner}} = r_{\text{cut}} - 1.0 \f$.
 *
 * **Characteristics:**
 *
 * - For \f$ r \leq r_{\text{inner}} \f$ the function \f$ f(r) = 1 \f$ and \f$ f'(r) = 0 \f$
 * - For \f$ r_{\text{inner}} < r < r_{\text{cut}} \f$, the function transitions smoothly from 1 to 0.
 * - For \f$ r \geq r_{\text{cut}} \f$, the function \f$ f(r) = 0 \f$ and \f$ f'(r) = 0 \f$.
 *
 * **Note:** The function ensures continuity and smoothness in simulations or calculations where a smooth cutoff is required.
 */
class Cut_CosS : public Cut_Base {
    private:
        std::string lab = "Cut_CosS";
        double rcut, rcut_sq, rcut_inv;
        double rcut_inner;
    public:
        Cut_CosS();
        Cut_CosS(double rcut);
        std::string label() ;
        void set_rcut(const double r);
        double get_rcut();
        double get_rcut_sq();
        double calc(double r);
        double calc_prime(double r);
        double calc_drcut(double r) override;
        double calc_prime_drcut(double r) override;
        double calc_double_prime(double r) override;
};
/**
 * @class Cut_PT
 * @brief Represents a smooth cutoff function using hyperbolic tangent smoothing.
 *
 * This class implements a smooth cutoff function defined by:
 * \f[
 * f(r) = N\left[\dfrac{1}{2} + \dfrac{1}{2} \tanh\left( \dfrac{1}{2} \left( \dfrac{r_c}{r + \dfrac{r_c}{2}} + \dfrac{r_c}{r - r_c} \right) \right)\right],\qquad N=\dfrac{2}{1+\tanh(1/2)}
 * \f]
 * and its derivative:
 * \f[
 * f'(r) = -N\dfrac{r_c}{4} \left( \dfrac{1}{\cosh\left( \dfrac{r_c}{2} \left( \dfrac{1}{r + \dfrac{r_c}{2}} + \dfrac{1}{r - r_c} \right) \right)} \right)^2 \left( \dfrac{1}{\left( r + \dfrac{r_c}{2} \right)^2} + \dfrac{1}{\left( r - r_c \right)^2} \right)
 * \f]
 * where:
 * - \f$ r \f$ is the radial distance.
 * - \f$ r_c \f$ is the cutoff radius.
 *
 * On the physical interval it transitions from f(0)=1 to zero at r_c.
 * The expression's asymptote at -r_c/2 lies outside that interval.
 *
 */
class Cut_PT : public Cut_Base {
    private:
        std::string lab="Cut_PT";
        double rcut;
        double rcut_sq;
        double rcut_inv;
        double rcut_inner;
        double norm = 1.0;  // precomputed 1/f(0) so that calc(0)==1

    public:
        Cut_PT();
        Cut_PT(double rcut_value);
        std::string label();
        void set_rcut(const double r);
        double get_rcut();
        double get_rcut_sq();
        double calc(double r);

        double calc_prime(double r);
        double calc_drcut(double r) override;
        double calc_prime_drcut(double r) override;
        double calc_double_prime(double r) override;
};

/* @class Cut_SinInv
 * @brief Smooth envelope function which transitions from 0 to 1 ("inverse cutoff").
 *
 * Used as a multiplicative envelope by F_mEnv to suppress the embedding-function
 * contribution near r=0 (where the radial Gaussian is largest) and recover full
 * amplitude at r >= rcut. Definition:
 * \f[
 *  f(r) =
 *  \begin{cases}
 *     \sin\!\left(\dfrac{\pi r}{2 r_{\text{cut}}}\right), & r \le r_{\text{cut}}\\
 *     1,                                                  & r >  r_{\text{cut}}
 *  \end{cases}
 * \f]
 */
class Cut_SinInv : public Cut_Base {
    private:
        std::string lab = "Cut_SinInv";
        double rcut, rcut_sq, rcut_inv;
    public:
        Cut_SinInv();
        Cut_SinInv(double rcut);
        std::string label();
        void set_rcut(const double r);
        double get_rcut();
        double get_rcut_sq();
        double calc(double r);
        double calc_prime(double r);
        double calc_drcut(double r) override;
        double calc_prime_drcut(double r) override;
        double calc_double_prime(double r) override;
        // Const overloads — F_mEnv::calc_F/calc_dF call calc/calc_prime
        // through a const member, so the underlying envelope must be
        // callable on a const Cut_SinInv as well. F_mEnv::calc_dF_dtheta
        // adds calc_drcut to the same const-callable surface for the
        // RCUTENV chain; calc_prime_drcut is added for the analogous
        // F_mEnv::calc_dFp_dtheta force-side chain. calc_double_prime
        // is added for F_mEnv::calc_d2F (radial-θ force chain).
        double calc(double r) const;
        double calc_prime(double r) const;
        double calc_drcut(double r) const;
        double calc_prime_drcut(double r) const;
        double calc_double_prime(double r) const;
        Kind kind() const override { return Kind::Inverse; }
        double val_at_rcut() const override { return 1.0; }
};

} // namespace models
} // namespace tadah
#endif
