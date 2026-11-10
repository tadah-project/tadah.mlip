#ifndef D2_BASE_H
#define D2_BASE_H

#include <tadah/models/cut_all.h>
#include <tadah/models/descriptors/d_base.h>

namespace tadah {
namespace models {

/** \brief Base class for all two-body type descriptors.
 *
 * All two-body descriptors **must** inherit this class.
 */
class D2_Base: public D_Base {
public:
  D2_Base() {};
  D2_Base(const tadah::core::Context &c): D_Base(c) {
    //if (c.get<bool>("INIT2B")) {
    if (c.contains("RCUT2BMAX") && c.contains("RCTYPE2B")) {
      double rcut2b = c.get<double>("RCUT2BMAX");
      set_fcut(tadah::core::factory<Cut_Base,double&>( c.get<std::string>("RCTYPE2B"), rcut2b ), true);
    }
    else {
      set_fcut(new Cut_Dummy(0), true);
    }
  }
  virtual ~D2_Base() {};
  virtual std::vector<std::string> get_init_atoms(const tadah::core::Context &c) override;

  /** \brief D_Base::depends_on(), and RCUT2B: the values pass through the
   *  cutoff function, whose radius it is (Cut_Dummy truncates them there).
   */
  bool depends_on(const std::string &theta_name, int arg_pos) const override {
    return theta_name == "RCUT2B" || D_Base::depends_on(theta_name, arg_pos);
  }

  /** \brief Calculate \ref AED
   *
   * Calculate Atomic Energy Descriptor for the atom local environment.
   */
  virtual void calc_aed(
    const int Zi,
    const int Zj,
    const double rij,
    const double rij_sq,
    tadah::core::aed_type& aed,
    const double scale=1)const=0;

  /** \brief Calculate \ref FD
   *
   * Calculate Force Descriptor for the atom local environment.
   *
   * For two body this is essentially df/dr where f is a descriptor function.
   * Do not include negative sign (as you mifht think E=-df/dr). The negative sign 
   * is included elsewhere in the code.
   *
   * Computes x-direction only.
   *
   * The resulting vector must be scaled by the unit directional vector delij/rij
   * where delij = r_i - r_j, and rij is |r_i - r_j|
   */
  virtual void calc_dXijdri(
    const int Zi,
    const int Zj,
    const double rij,
    const double rij_sq,
    tadah::core::fd_type &fd_ij,
    const double scale=1) const=0;

  /** \brief Calculate \ref AED + \ref FD */
  virtual void calc_all(
    const int Zi,
    const int Zj,
    const double rij,
    const double rij_sq,
    tadah::core::aed_type& aed,
    tadah::core::fd_type &fd_ij,
    const double scale=1) const=0;

  /** \brief Closed-form derivative of \ref AED with respect to a named
   *  hyperparameter.
   *
   * Used by HPO analytical / hybrid gradients to assemble
   * dPhi/dtheta one pair at a time, mirroring how calc_aed assembles
   * Phi. The default returns false: "this descriptor does not own
   * `theta_name`". Concrete descriptors override for the (theta_name,
   * arg_pos) pairs they implement.
   *
   * Calling convention:
   *  - `theta_name` is the OPTIM key (e.g. "CGRID2B", "SGRID2B",
   *    "RCUT2B"); always upper-case.
   *  - `arg_pos` is the 1-based index into the vector parameter (k for
   *    "CGRID2B[k]"), or -1 for scalar parameters.
   *  - `dD_buf` is the pre-zeroed aed-shaped accumulator. Concrete
   *    implementations `+=` their contribution at `fidx + c` where c
   *    is the descriptor-component index.
   *  - Return `true` iff this call wrote something into `dD_buf`. The
   *    aggregator uses the return value to detect "no descriptor owned
   *    this theta" and route the gradient via FD fallback (HYBRID) or
   *    raise (ANALYTICAL).
   *  - Cross-cutting parameters (RCUT2B / RCUTMB) flow through the
   *    cutoff (`fcut->calc_drcut`); each D2 descriptor that uses a
   *    non-Cut_Dummy cutoff contributes additively.
   *
   * Default impl returns false (no-op). New descriptors can be added
   * without implementing this method; HYBRID will FD-fallback on
   * their parameters until an analytical version lands.
   */
  virtual bool calc_dDdtheta(
      const std::string& /*theta_name*/,
      int                /*arg_pos*/,
      const int          /*Zi*/,
      const int          /*Zj*/,
      const double       /*rij*/,
      const double       /*rij_sq*/,
      tadah::core::aed_type& /*dD_buf*/,
      const double       /*scale=1*/) const {
    return false;
  }

  /** \brief Closed-form derivative of \ref FD with respect to a named
   *  hyperparameter.
   *
   * Used by HPO analytical / hybrid gradients to assemble
   * dPhi/dtheta force rows one pair at a time, mirroring how
   * calc_dXijdri assembles fd_ij. The default returns false: "this
   * descriptor does not own `theta_name` for the force chain".
   * Concrete descriptors override for the (theta_name, arg_pos) pairs
   * they implement.
   *
   * Computes the radial-direction (column 0) component only; the
   * caller scales by the unit directional vector delij/rij to produce
   * the 3-axis derivative, matching the descriptors_calc.hpp axis
   * expansion pattern.
   *
   * Calling convention parallels calc_dDdtheta:
   *  - `theta_name` is the OPTIM key.
   *  - `arg_pos` is the 1-based vector index, or -1 for scalar params.
   *  - `dfd_ij` is the pre-zeroed fd-shaped accumulator; this method
   *    `+=` into column 0 (radial). Columns 1, 2 are left untouched
   *    so the caller's axis-expansion step (mirroring
   *    descriptors_calc.hpp:233-238) can run unchanged on the result.
   *  - Returns `true` iff something was written into `dfd_ij`.
   *
   * Default impl returns false; concrete descriptors override for the
   * theta keys they support.
   */
  virtual bool calc_dXijdri_dtheta(
      const std::string& /*theta_name*/,
      int                /*arg_pos*/,
      const int          /*Zi*/,
      const int          /*Zj*/,
      const double       /*rij*/,
      const double       /*rij_sq*/,
      tadah::core::fd_type& /*dfd_ij*/,
      const double       /*scale=1*/) const {
    return false;
  }

  /** Central difference approximation to \ref FD
   *
   * Calculate Force Descriptor for the atom local environment
   * using central difference approximation.
   *
   * fc: a pointer to initialised cutoff object.
   * r: a separation between two particles
   * fd: appropriate size container to store computation
   * h: central difference parameter
   */
  void calc_fd_approx(const int Zi, const int Zj, double r, tadah::core::fd_type &fd, double h=1e-8)  const{
    tadah::core::aed_type aed_n((*this).size()); // r+h
    tadah::core::aed_type aed_p((*this).size()); // r-h
    aed_n.set_zero();
    aed_p.set_zero();
    double rn = r-h;
    double rp = r+h;
    (*this).calc_aed(Zi,Zj,rn,rn*rn,aed_n);
    (*this).calc_aed(Zi,Zj,rp,rp*rp,aed_p);

    fd(0) = (0.5/h)*(aed_p - aed_n);
  }
};
} // namespace models
} // namespace tadah
#endif
