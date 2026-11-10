#ifndef M_KRR_MD_H
#define M_KRR_MD_H

#include <tadah/md/m_md_base.h>
#include <tadah/core/context.h>
#include <tadah/core/normaliser_core.h>
#include <tadah/models/m_krr_core.h>
#include <tadah/models/functions/function_base.h>

#include <iostream>

namespace tadah {
namespace md {

template
<class K=models::Function_Base&>
class M_KRR_MD: private models::M_KRR_Core<K>, public M_MD_Base {

    public:
        M_KRR_MD(K &kernel, core::Context &c):
            models::M_KRR_Core<K>(kernel,c)
    {
        norm = core::Normaliser_Core(c);
    }
        M_KRR_MD(core::Context &c):
            models::M_KRR_Core<K>(c)
    {
        norm = core::Normaliser_Core(c);
    }

        double epredict(const core::aed_type &aed) const {
            return models::M_KRR_Core<K>::kernel.epredict(models::M_KRR_Core<K>::weights,aed);
        };

        double fpredict(const core::fd_type &fdij, const core::aed_type &aedi, const size_t k) const {
            return models::M_KRR_Core<K>::kernel.fpredict(models::M_KRR_Core<K>::weights,fdij,aedi,k);
        }

        core::force_type fpredict(const core::fd_type &fdij, const core::aed_type &aedi) const {
            return models::M_KRR_Core<K>::kernel.fpredict(models::M_KRR_Core<K>::weights,fdij,aedi);
        }
};

} // namespace md
} // namespace tadah
#endif
