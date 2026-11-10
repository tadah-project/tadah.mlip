#ifndef DM_MJOIN_H
#define DM_MJOIN_H

#include "tadah/core/registry.h"
#include <cstddef>
#include <tadah/models/descriptors/dm/dm_base.h>
#include <tadah/models/descriptors/d_mjoin.h>

namespace tadah {
namespace models {

/** \brief Meta many-body descriptor for combining multiple DM descriptors.
 *
 * This descriptor provides an interface for concatenating various many-body descriptors.
 * The resulting descriptor can then be used by Tadah! like any standard many-body descriptor.
 *
 * Each descriptor must have a specified type in a configuration file, along with a cutoff function, 
 * cutoff distance, and other optional keys that are typically expected for this descriptor,
 * such as SGRIDMB and CGRIDMB.
 *
 * When listing descriptors under the TYPEMB key, include parameters relevant to this descriptor.
 *
 * Here is an example of configuring these descriptors:
 * 
 * ```
 * TYPEMB    DM_mJoin         # Meta descriptor for concatenating many-body descriptors
 * TYPEMB    DM_EAD 1 5 5 * *    # L number, cgrid, sgrid, list of element pairs
 * RCTYPEMB  Cut_Cos
 * RCUTMB    3.0
 * CGRIDMB   LIN 5 0 3.0      # DM_EAD: 5 Gaussian centres from 0 to 3.0
 * SGRIDMB   GEOM 5 1.0 10.0  # DM_EAD: 5 widths from 1.0 to 10.0
 *
 * TYPEMB    DM_Blip 0 7 7 Ta Ta   # L number, cgrid, sgrid, list of element pairs
 * RCTYPEMB  Cut_Tanh
 * RCUTMB    7.5
 * SGRIDMB   GEOM 7 0.1 10    # DM_Blip: 7 blip widths from 0.1 to 10
 * CGRIDMB   0 0 0 0 0 0 0    # DM_Blip: 7 blip centres
 * ```
 *
 * The values of a list key form one list, whatever lines they are written
 * on: each descriptor takes as many values as its TYPEMB line asks for, in
 * the order of the TYPEMB lines, and more or fewer values are refused. A
 * line gives values or generators: LIN n start stop gives n evenly spaced
 * values, GEOM n start stop n values in a geometric progression.
 *
 * There is no limit to the number of descriptors that can be concatenated.
 *
 * \details
 * - Ensure the types and grids are correctly specified in the configuration file.
 * - The cutoff functions (RCTYPEMB) and distances (RCUTMB) must be defined for each descriptor.
 * - Both SGRIDMB and CGRIDMB should be included if relevant, with their sizes matching the given descriptors.
 */
class DM_mJoin : public DM_Base, public D_mJoin {
private:
  std::vector<DM_Base*> ds;
  std::vector<tadah::core::Context> contexts;
  std::string lab = "DM_mJoin";
  void build_children(tadah::core::Context &);
public:
  using DM_Base::calc_drhoi_dtheta;
  using DM_Base::calc_daed_dtheta;
  using DM_Base::calc_dXijdri_dtheta;
  DM_mJoin();
  ~DM_mJoin();
  DM_mJoin(tadah::core::Context &c);
  DM_mJoin(const tadah::core::Context &c);
  void calc_aed(
    tadah::core::rho_type& rho,
    tadah::core::aed_type &aed) const override;
  void calc_dXijdri_dXjidri(
    const int Zi,
    const int Zj,
    const double rij,
    const double rij_sq,
    const tadah::core::Vec3d &vec_ij,
    tadah::core::rho_type& rhoi,
    tadah::core::rho_type& rhoj,
    tadah::core::fd_type &fd_ij,
    const double scale=1) const override;
  void calc_dXijdri(
    const int Zi,
    const int Zj,
    const double rij,
    const double rij_sq,
    const tadah::core::Vec3d &vec_ij,
    tadah::core::rho_type& rhoi,
    tadah::core::fd_type &fd_ij, 
    const double scale=1) const override;
  std::string label() const override;
  /** Every joined descriptor applies to every element pair. */
  bool applies_to_every_element_pair() const override;
  void init_rhoi(tadah::core::rho_type& rhoi) override;
  void calc_rho(
    const int Zi,
    const int Zj,
    const double rij,
    const double rij_sq,
    const tadah::core::Vec3d &vec_ij,
    tadah::core::rho_type& rho,
    const double scale=1) const override;
  void set_fidx(size_t fidx_) override;
  void set_rfidx(size_t rfidx_) override;
  void set_drfidx(size_t drfidx_) override;

  void init() override;
  /** Value @p i of the cutoff key is the i-th joined descriptor's: its
   *  answer for its value 0, "" past the last descriptor. */
  std::string cutoff_fixed_by(std::size_t i) const override;
  /** WATOM weighs its values when it weighs those of a descriptor it
   *  joins. */
  bool reads_watom() const override;
  /** Its own keys, and what a descriptor it joins depends on (each joined
   *  descriptor is asked with the same index). */
  bool depends_on(const std::string &theta_name, int arg_pos) const override;

  /** EMBF0 of the joined DM_REAM children (one global key: the children read
   *  copies of the same context); nullopt when no child has an embedding
   *  reference. */
  std::optional<EmbF0Mode> embf0_mode() const override;
  /** Sum of the children's raw F(0) (each indexes its own block of w_full);
   *  nullopt when no child reports one. */
  std::optional<double>
  raw_embedding_f0(const tadah::core::t_type &w_full) const override;

  /** The analytical theta-derivatives of the joined descriptor, for the
   *  dPhi/dtheta walker. A named component path (<ID>.DENSITY.CENTRES, ...:
   *  a name with a '.') names one DM_Embed child by its ID: it is passed to
   *  every child, and only the child with that ID owns it. A legacy key
   *  (CGRIDMB, SGRIDMB, RCUTMB, WATOM, CEMBFUNC, ...) indexes the JOINED
   *  lists of the parent context, which no child reads against its own
   *  grid: it is not owned (false), so GRAD_STRATEGY HYBRID takes it by
   *  finite differences and ANALYTICAL names it as a parameter it cannot
   *  differentiate. */
  bool calc_drhoi_dtheta(const std::string &name,int pos,int zi,int zj,double r,double rsq,
      const core::Vec3d &v,core::rho_type &rho,double scale=1) const override {
    if(!is_component_path(name)) return false;
    bool owned=false;
    for(auto *d:ds) owned=d->calc_drhoi_dtheta(name,pos,zi,zj,r,rsq,v,rho,scale)||owned;
    return owned;
  }
  /** See calc_drhoi_dtheta. */
  bool calc_daed_dtheta(const std::string &name,int pos,const core::rho_type &rho,
      const core::rho_type &drho,core::aed_type &aed,double scale=1) const override {
    if(!is_component_path(name)) return false;
    bool owned=false;
    for(auto *d:ds) owned=d->calc_daed_dtheta(name,pos,rho,drho,aed,scale)||owned;
    return owned;
  }
  /** See calc_drhoi_dtheta. */
  bool calc_dXijdri_dtheta(const std::string &name,int pos,int zi,int zj,double r,double rsq,
      const core::Vec3d &v,const core::rho_type &rho,const core::rho_type &drho,
      core::fd_type &fd,double scale=1) const override {
    if(!is_component_path(name)) return false;
    bool owned=false;
    for(auto *d:ds) owned=d->calc_dXijdri_dtheta(name,pos,zi,zj,r,rsq,v,rho,drho,fd,scale)||owned;
    return owned;
  }

private:
  /** A named component path: <ID>.<CHANNEL>.<FIELD>. */
  static bool is_component_path(const std::string &name) {
    return name.find('.') != std::string::npos;
  }
};

} // namespace models
} // namespace tadah
#endif
