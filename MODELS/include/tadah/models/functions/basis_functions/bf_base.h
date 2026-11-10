#ifndef BASIS_FUNCTIONS_H
#define BASIS_FUNCTIONS_H

#include <tadah/core/core_types.h>
#include <tadah/models/functions/function_base.h>

#include <iostream>

namespace tadah {
namespace models {

/** \brief Base class to be used by all basis functions */
struct BF_Base: public virtual Function_Base {
  BF_Base();
  BF_Base(const tadah::core::Context &c);
  virtual ~BF_Base();

  // virtual double epredict(const tadah::core::t_type &, const tadah::core::aed_type& ) const=0;
  // virtual double fpredict(const tadah::core::t_type &, const tadah::core::fd_type &,
  //         const tadah::core::aed_type& , const size_t ) const=0;

  // virtual tadah::core::force_type fpredict(const tadah::core::t_type &, const tadah::core::fd_type &,
  //         const tadah::core::aed_type& ) const=0;
};
} // namespace models
} // namespace tadah
#endif
