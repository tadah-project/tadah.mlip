#ifndef D2_MJOIN_H
#define D2_MJOIN_H
#include "tadah/core/registry.h"
#include <cstddef>
#include <tadah/models/descriptors/d2/d2_base.h>
#include <tadah/models/descriptors/d_mjoin.h>

namespace tadah {
namespace models {

/** \brief Meta two-body descriptor for combining multiple D2 descriptors.
 *
 * This descriptor provides a convenient interface for concatenating multiple two-body descriptors. 
 * The resulting descriptor can then be used by Tadah! like any standard two-body descriptor.
 *
 * Each descriptor must have a specified type in a configuration file, along with a cutoff function, 
 * cutoff distance, and optionally SGRID2B and CGRID2B values if applicable.
 *
 * When listing descriptors under the TYPE2B key, you must include parameters relevant to this descriptor.
 *
 * Here is an example of how to configure these descriptors:
 * 
 * ```
 * TYPE2B    D2_mJoin     # <-- Meta descriptor for concatenating two-body descriptors
 * TYPE2B    D2_MIE 11 6 Ta Ta   # <-- MIE exponents
 * RCTYPE2B  Cut_Cos
 * RCUT2B    3.0
 *
 * TYPE2B    D2_Blip 6 6 Ta Ta   # <-- grid sizes
 * RCTYPE2B  Cut_Tanh
 * RCUT2B    7.5
 * SGRID2B   GEOM 6 0.1 10   # D2_Blip: 6 blip widths from 0.1 to 10
 * CGRID2B   0 0 0 0 0 0     # D2_Blip: 6 blip centres
 * ```
 *
 * The values of a list key form one list, whatever lines they are written
 * on: each descriptor takes as many values as its TYPE2B line asks for, in
 * the order of the TYPE2B lines, and more or fewer values are refused. A
 * line gives values or generators: LIN n start stop gives n evenly spaced
 * values, GEOM n start stop n values in a geometric progression.
 *
 * There is no limit to the number of descriptors that can be concatenated.
 *
 * \details
 * - Ensure the types and grids are correctly specified in the configuration file.
 * - The cutoff functions (RCTYPE2B) and distances (RCUT2B) must be defined for each descriptor.
 * - Both SGRID2B and CGRID2B should be included if relevant, with their sizes matching the given descriptors.
 */
class D2_mJoin : public D2_Base, public D_mJoin {
private:
  std::vector<D2_Base*> ds;
  std::vector<tadah::core::Context> contexts;
  std::string lab = "D2_mJoin";
public:
  D2_mJoin();
  ~D2_mJoin();
  D2_mJoin(tadah::core::Context &c);
  D2_mJoin(const tadah::core::Context &c);
  
  void calc_aed(const int Zi, const int Zj, const double rij, const double rij_sq, tadah::core::aed_type &aed, const double scale=1) const override;
  void calc_dXijdri(const int Zi, const int Zj, const double rij, const double rij_sq, tadah::core::fd_type &fd_ij, const double scale=1) const override;
  void calc_all(const int Zi, const int Zj, const double rij, const double rij_sq, tadah::core::aed_type &aed, tadah::core::fd_type &fd_ij, const double scale=1) const override;
  std::string label() const override;
  /** Every joined descriptor applies to every element pair. */
  bool applies_to_every_element_pair() const override;

  void set_fidx(size_t fidx_) override;
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
};
} // namespace models
} // namespace tadah
#endif
