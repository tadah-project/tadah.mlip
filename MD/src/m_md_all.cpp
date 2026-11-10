#include <tadah/core/registry.h>
#include <tadah/md/m_md_base.h>
#include <tadah/md/m_blr_md.h>
#include <tadah/md/m_krr_md.h>

namespace tadah {
namespace md {
  
template<>
tadah::core::Registry<M_MD_Base,tadah::models::Function_Base&,core::Context&>::Map&
tadah::core::Registry<M_MD_Base,tadah::models::Function_Base&,core::Context&>::registry =
  tadah::core::Registry<tadah::md::M_MD_Base,tadah::models::Function_Base&,tadah::core::Context&>::instance();

tadah::core::Registry<M_MD_Base,tadah::models::Function_Base&,core::Context&>::Register<M_BLR_MD<>> M_BLR_MD_1("M_BLR");
tadah::core::Registry<M_MD_Base,tadah::models::Function_Base&,core::Context&>::Register<M_KRR_MD<>> M_KRR_MD_1("M_KRR");

} // namespace md
} // namespace tadah
