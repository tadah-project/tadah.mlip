#ifndef M_BLR_MD_H
#define M_BLR_MD_H

#include <tadah/md/m_md_base.h>
#include <tadah/core/context.h>
#include <tadah/core/normaliser_core.h>
#include <tadah/models/m_blr_core.h>
#include <tadah/models/functions/function_base.h>

#include <iostream>

namespace tadah {
namespace md {

template
<class BF=models::Function_Base&>
class M_BLR_MD: private models::M_BLR_Core<BF>, public M_MD_Base {

    public:
        M_BLR_MD(BF &bf, core::Context &c):
            models::M_BLR_Core<BF>(bf,c)
    {
        norm = core::Normaliser_Core(c);
    }
        M_BLR_MD(core::Context &c):
            models::M_BLR_Core<BF>(c)
    {
        norm = core::Normaliser_Core(c);
    }

        double epredict(const core::aed_type &aed) const{
            return models::M_BLR_Core<BF>::bf.epredict(models::M_BLR_Core<BF>::weights,aed);
        };

        double fpredict(const core::fd_type &fdij, const core::aed_type &aedi, const size_t k) const {
            return models::M_BLR_Core<BF>::bf.fpredict(models::M_BLR_Core<BF>::weights,fdij,aedi,k);
        }

        core::force_type fpredict(const core::fd_type &fdij, const core::aed_type &aedi) const {
            return models::M_BLR_Core<BF>::bf.fpredict(models::M_BLR_Core<BF>::weights,fdij,aedi);
        }
};
} // namespace md
} // namespace tadah
#endif

