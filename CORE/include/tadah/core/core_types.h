#ifndef TADAH_CORE_CORE_TYPES_H
#define TADAH_CORE_CORE_TYPES_H
#include <tadah/core/maths.h>
#include <tadah/core/fd_type.h>
#include <tadah/core/aed_type.h>
#include <tadah/core/aeds_type.h>

#include <vector>

namespace tadah {
namespace core {

typedef Vec3d force_type;
typedef aed_type t_type;
typedef aed_type rho_type;
typedef aeds_type2 rhos_type;
typedef Matrix phi_type;
typedef Matrix3d stress_type;

typedef std::vector<double> v_type;

////////////////////////////////////////////////////////////////////////////////
// Define dummy mat class to satisfy Sigma in core model classes. //////////////
////////////////////////////////////////////////////////////////////////////////
#ifdef M_MD_BASE_H
//#define TEMP_MAT_SIGMA
class mat {
    public:
        t_type diagonal() {
            return t_type();
        }
        t_type diagonal() const {
            return t_type();
        }

};
#endif
/////////////////////////////////////////

} // namespace core
} // namespace tadah
#endif  // TADAH_CORE_CORE_TYPES_H
