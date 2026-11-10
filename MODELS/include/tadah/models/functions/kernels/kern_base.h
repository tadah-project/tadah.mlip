#ifndef KERN_BASE_H
#define KERN_BASE_H

#include <tadah/core/core_types.h>
#include <tadah/models/functions/function_base.h>

#include <iostream>
#include <string>
#include <vector>

namespace tadah {
namespace models {

/** \brief Abstract class to be used as a base for all kernels.
 *
 *  - b = basis vector
 *  - af = atomic energy descriptor
 *  - ff = force descriptor
 *  - all derivatives are defined wrt to the second argument
 */
class Kern_Base: public virtual Function_Base {
    public:
        tadah::core::Matrix basis;
        Kern_Base();
        Kern_Base(const tadah::core::Context &c);
        virtual ~Kern_Base();

        // /**
        //  * Calculate kernel value given two vectors
        //  *
        //  * - b = basis vector
        //  * - af = atomic energy descriptor
        //  */
        virtual double operator() (const tadah::core::aed_type &b, const tadah::core::aed_type &af) const=0;
        //
        // /**
        //  * Calculate the kernel derivative wrt to the second argument
        //  *
        //  * - b = basis vector
        //  * - af = atomic energy descriptor
        //  */
        virtual tadah::core::aed_type derivative(const tadah::core::aed_type &b, const tadah::core::aed_type &af) const=0;

        /**
         * Calculate inner product of the kernel derivative
         * wrt to the second argument (b wrt af) with the force descriptor
         *
         * - b = basis vector
         * - af = atomic energy descriptor
         * - ff = force descriptor (TODO i-th dir component of it)
         *   TODO consider calculating all 3-directions at once
         */
        virtual double prime(const tadah::core::aed_type &b, const tadah::core::aed_type &af,
                const tadah::core::aed_type &ff) const=0;

        /** \brief Closed-form derivative of K(b, af) with respect to a
         *  named kernel hyperparameter.
         *
         *  Used by HPO analytical / hybrid gradients — parallel
         *  to D2_Base::calc_dDdtheta on the kernel arm. The default
         *  returns false: "this kernel does not own theta_name". Concrete
         *  kernels override for the (theta_name, arg_pos) pairs they
         *  implement.
         *
         *  Calling convention:
         *   - theta_name is the OPTIM key consumed by the kernel
         *     constructor; today the only registered key is "MPARAMS".
         *   - arg_pos is the 1-based numerical-token position within the
         *     key's value list (matching the OPTIM (n) parser
         *     convention in HPO; skips non-numerical tokens). See
         *     HPOTargets::ui_offset.
         *   - dK_out receives the scalar dK / d theta on return.
         *   - Return true iff this call wrote dK_out. The aggregator
         *     uses the bool to detect "no kernel owned this theta" and
         *     route via FD fallback (HYBRID) or raise (ANALYTICAL).
         */
        virtual bool calc_dKdtheta(
            const std::string& /*theta_name*/,
            int                /*arg_pos*/,
            const tadah::core::aed_type& /*b*/,
            const tadah::core::aed_type& /*af*/,
            double& /*dK_out*/) const {
          return false;
        }

        /** \brief Set basis for calculations */
        virtual void set_basis(const tadah::core::Matrix b);

        /** \brief One weight per basis vector (basis.cols()), whatever
         *  dsize is; 0 before the kernel has a basis. */
        std::size_t weights_count(std::size_t dsize) const override;

        /** \brief The length of the basis vectors (basis.rows()). */
        std::size_t basis_vector_size() const override;

        /** \brief Return basis stored by this object */
        virtual const tadah::core::Matrix &get_basis() const;
        /** \brief Return basis stored by this object */
        virtual tadah::core::Matrix get_basis();

        virtual double epredict(const tadah::core::t_type &kweights, const tadah::core::aed_type &aed) const;
        virtual double fpredict(const tadah::core::t_type &kweights, const tadah::core::fd_type &fdij,
                const tadah::core::aed_type &aedi, const size_t k) const;
        virtual tadah::core::force_type fpredict(const tadah::core::t_type &kweights, const tadah::core::fd_type &fdij,
                const tadah::core::aed_type &aedi) const;

        /** \brief Read the basis vectors of a potential: SBASIS vectors
         *  stored column after column in BASIS.
         *
         *  Without BASIS nothing is read and \p b is left as it is (a
         *  training configuration names SBASIS, the number of basis
         *  vectors to draw, and has no BASIS). A BASIS is refused, naming
         *  the keys, when SBASIS is missing or 0, or when the number of
         *  BASIS values is not a multiple of SBASIS. Whether the vectors
         *  have the descriptor's length is checked where that length is
         *  known (M_KRR in MLIP, pair_style tadah). */
        virtual void read_basis_from_context(const tadah::core::Context &context,
                tadah::core::Matrix &b);

    protected:
        /** \brief Function_Base::intercept_columns() of a kernel
         *  K(b, x) = f(b . x): each basis column k < N (N =
         *  tadah::core::n_intercept_columns(), the descriptor's intercept
         *  components) that is zero but for its component k, so that
         *  K(b_k, x) depends on x only through intercept component k. */
        std::vector<size_t> dot_product_intercept_columns(const tadah::core::Context &c) const;

        /** \brief Prints the kernel's settings line, "\n" @p label
         *  @p settings "\n" (Kern_RBF: "Kern_RBF | sigma: 1 | gamma: 1"),
         *  when the kernel is built at VERBOSE > 0: at every build, but
         *  while a KernelSettingsLineScope is open at the first build of
         *  each @p label in it only, and while one of Lines::None is open
         *  not at all. Called by the kernels' constructors; @p settings is
         *  formatted as std::cout formats numbers. */
        void print_settings(const std::string &label,
                            const std::string &settings = "");
};

/** \brief While an object of this class lives, a kernel built at
 *  VERBOSE > 0 prints its settings line (Kern_Base::print_settings(),
 *  "Kern_RBF | sigma: 1 | gamma: 1") at the first build of its kind only,
 *  or, for Lines::None, not at all.
 *
 *  Outside every scope each kernel built at VERBOSE > 0 prints its line:
 *  tadah train, tadah predict and pair_style tadah print the line of every
 *  model they build. tadah hpo holds a scope
 *  for each run (hpo_run()): the run builds the model of every evaluation,
 *  and the potential each PC_LAMMPS script runs, from one configuration,
 *  and prints the line of the first model it builds, the -c model's. A
 *  PC_ISOLATION PROCESS worker, a process of its own that builds the
 *  potentials of the run's scripts, holds one of Lines::None: the run has
 *  printed the line.
 *
 *  The scopes open at the same time, in any thread, share one record of
 *  the kernels that printed their line, keyed on the label, not on the
 *  settings: it starts empty when a scope opens while none is open, so a
 *  second run in the process prints its own line. A build at VERBOSE 0, or
 *  one while a Lines::None scope is open, prints nothing and does not use
 *  up the line.
 */
class KernelSettingsLineScope {
    public:
        /** \brief What the kernels built while the scope is open print. */
        enum class Lines {
            FirstOfEachKernel,  ///< the line of the first build of each kernel
            None                ///< no line
        };
        /** \brief Opens the scope: from now, until it closes, the kernels
         *  built print what @p lines says. Starts the record afresh when no
         *  other scope is open. */
        explicit KernelSettingsLineScope(Lines lines = Lines::FirstOfEachKernel);
        /** \brief Closes the scope; the record is dropped when it was the
         *  last one open. */
        ~KernelSettingsLineScope();
        KernelSettingsLineScope(const KernelSettingsLineScope &) = delete;
        KernelSettingsLineScope &operator=(const KernelSettingsLineScope &) = delete;

    private:
        Lines lines_;
};
} // namespace models
} // namespace tadah

#endif
