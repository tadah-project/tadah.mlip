#ifndef TADAH_CORE_NORMALISER_CORE_H
#define TADAH_CORE_NORMALISER_CORE_H

#include <tadah/core/context.h>
#include <tadah/core/core_types.h>
#include <tadah/core/logger.h>

namespace tadah {
namespace core {

// First `bias` columns of Phi are intercept columns (1.0 for the atom's
// species, 0 elsewhere). They must be excluded from mean/std-dev normalisation
// because their statistics are uninformative (constant per species).
class Normaliser_Core {
    public:
        size_t bias;

        v_type std_dev;
        v_type mean;

        Normaliser_Core ();
        Normaliser_Core (const Context &c);

        /** Normalise AED */
        void normalise(aed_type &aed);

        /** Normalise FD */

        void normalise(fd_type &fd);
        void normalise(fd_type &fd, size_t k);
};
} // namespace core
} // namespace tadah
#endif
