#include <tadah/md/m_md_base.h>

namespace tadah {
namespace md {
  
M_MD_Base::~M_MD_Base() {}
double M_MD_Dummy::epredict(const core::aed_type &) const
{ return 0.0; }
double M_MD_Dummy::fpredict(const core::fd_type &, const core::aed_type &, size_t ) const
{return 0.0; }
core::force_type M_MD_Dummy::fpredict(const core::fd_type &,
        const core::aed_type &) const 
{return core::force_type(); }

} // namespace md
} // namespace tadah
