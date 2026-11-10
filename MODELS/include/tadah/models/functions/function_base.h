#ifndef FUNCTION_BASE_h
#define FUNCTION_BASE_h

#include <tadah/core/core_types.h>
#include <tadah/core/context.h>

#include <cstddef>
#include <string>
#include <vector>

namespace tadah {
namespace models {

/** \brief One value of \ref MPARAMS that a function reads
 *  (Function_Base::mparams()). */
struct MParamSlot {
  std::string name;       ///< the parameter, e.g. "sigma" (Kern_RBF) or "d"
  bool integer = false;   ///< read as an integer (an exact integer value)
  bool positive = false;  ///< a searched value (OPTIM MPARAMS) must be > 0
};

/** Base class for Kernels and Basis Functions */
struct Function_Base {

private:
  int verbose;

public:
  Function_Base();
  Function_Base(const tadah::core::Context &c);
  void set_verbose(int v);
  int get_verbose();

  // Derived classes must implement Derived() and Derived(tadah::core::Context)
  virtual ~Function_Base();

  // For Kernels
  virtual double operator() (const tadah::core::aed_type& , const tadah::core::aed_type& ) const;
  virtual tadah::core::aed_type derivative(const tadah::core::aed_type& , const tadah::core::aed_type& ) const;
  virtual double prime(const tadah::core::aed_type& , const tadah::core::aed_type& ,
                       const tadah::core::aed_type& ) const;
  virtual void set_basis(const tadah::core::Matrix );

  /** \brief Return label of this object */
  virtual std::string get_label() const=0;

  /** \brief The \ref MPARAMS values this function reads when it is built,
   *  in order: element i is MPARAMS (i+1).
   *
   *  - Kern_RBF: sigma (positive);
   *  - Kern_Polynomial: d (an integer, positive), gamma, c;
   *  - Kern_Sigmoid: gamma, c;
   *  - every other function (Kern_Linear, Kern_Quadratic, Kern_LQ,
   *    BF_Linear, BF_Polynomial2): none, the default.
   *
   *  tadah hpo refuses at startup an OPTIM MPARAMS (i) of a function that
   *  reads fewer than i values, of an integer value, and a box that is not
   *  positive for a positive one. */
  virtual std::vector<MParamSlot> mparams() const { return {}; }

  /** \brief The columns of this function's design matrix that are
   *  intercepts, as 0-based column (weight) indices in ascending order.
   *
   *  An intercept column depends on an atom only through its species:
   *  every atom of a species adds the same value to the energy row, and
   *  the force and stress rows are 0. The regression leaves these columns
   *  out of the LAMBDA ridge penalty (LinearRegressor::train), OALGO 3,
   *  which penalises every column alike, refuses a model with LAMBDA != 0
   *  that has one, and BIAS true training refuses data whose compositions
   *  cannot determine them (MLIP require_intercepts_determined).
   *
   *  With BIAS true and ATOM the descriptor starts with N =
   *  tadah::core::n_intercept_columns() one-hot intercept components, in
   *  ATOM order (component k is 1 for an atom of species k, 0 otherwise).
   *  - BF_Linear, Kern_Linear: the descriptor's intercept columns, 0 ..
   *    N-1;
   *  - BF_Polynomial2: the product (k,k) of each intercept component with
   *    itself (1 for species k), k < N, at index k D - k (k - 1) / 2 of
   *    the products (i,ii), i <= ii, taken row by row (D = DSIZE, which
   *    the context must hold when N >= 2: std::logic_error otherwise); the
   *    products of two different intercept components are 0 for every
   *    atom and are not intercepts;
   *  - Kern_Quadratic, Kern_LQ, Kern_Polynomial, Kern_Sigmoid (functions
   *    of b . x): each column k < N whose basis vector has no component
   *    other than k (the intercept vector e_k that training puts there):
   *    K(b_k, x) then depends on x only through intercept component k;
   *  - Kern_RBF: none (K(b, x) depends on every component of x).
   *  With BIAS false there is none.
   *
   *  Pure, so that a new function states its own. */
  virtual std::vector<size_t> intercept_columns(const tadah::core::Context &c) const=0;
  virtual double epredict(const tadah::core::t_type &, const tadah::core::aed_type& ) const=0;
  virtual double fpredict(const tadah::core::t_type &, const tadah::core::fd_type &,
                          const tadah::core::aed_type&, const size_t ) const=0;

  virtual tadah::core::force_type fpredict(const tadah::core::t_type &, const tadah::core::fd_type &,
                              const tadah::core::aed_type& ) const=0;

  /** \brief Number of model weights for a descriptor of @p dsize entries:
   *  the design-matrix width. BF_Linear and Kern_Linear: dsize;
   *  BF_Polynomial2: dsize (dsize + 1) / 2; a kernel with a basis: the
   *  number of basis vectors, whatever dsize is (0 before it has a basis). */
  virtual std::size_t weights_count(std::size_t dsize) const=0;

  /** \brief Number of descriptor entries of each basis vector the weights
   *  multiply (a kernel's basis.rows()); 0 for a function without a basis
   *  (the basis functions and Kern_Linear act on the descriptor itself). */
  virtual std::size_t basis_vector_size() const;
};
} // namespace models
} // namespace tadah
#endif
