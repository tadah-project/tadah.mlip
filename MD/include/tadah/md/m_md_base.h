#ifndef M_MD_BASE_H
#define M_MD_BASE_H

#include <tadah/core/core_types.h>
#include <tadah/core/normaliser_core.h>
#include <tadah/models/functions/function_base.h>
#include <tadah/models/m_predict.h>

namespace tadah {
namespace md {

class M_MD_Base: public models::M_Predict {

    public:
      core::Normaliser_Core norm;    // TODO?
        virtual ~M_MD_Base();
};

struct M_MD_Dummy: public M_MD_Base {
    double epredict(const core::aed_type &) const;
    double fpredict(const core::fd_type &, const core::aed_type &, size_t ) const;
    core::force_type fpredict(const core::fd_type &,
            const core::aed_type &) const;
};
} // namespace md
} // namespace tadah
// c++17 solution...
//template<> inline Registry<M_MD_Base,Function_Base&,Config&>::Map Registry<M_MD_Base,Function_Base&,Config&>::registry{};
#endif
