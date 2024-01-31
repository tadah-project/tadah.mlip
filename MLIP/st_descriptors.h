#ifndef ST_DESCRIPTORS_H
#define ST_DESCRIPTORS_H

#include "../LIBS/Eigen/Dense"
#include "../LIBS/Eigen/src/Core/Array.h"
#include "../LIBS/Eigen/src/Core/util/Constants.h"
#include "structure.h"
#include "../CORE/config/config.h"

#include <vector>

// Temp solution to patch lammps stack smashing
// when Eigen::VectorXd is used as a container
// lammps has a tendency to crash occasionally
// Only observed so far when hpo is used
// - It seem to be fine for aeds and fd
// - The crash happens during vector resizing
// - could be memory allocation incompatibility
class my_vec {
    private:
        std::vector<double> v;
    public:
        void setZero() {
            std::fill(v.begin(), v.end(), 0);
        }
        double &operator()(const size_t i) {
            return v[i];
        }
        void resize(size_t n) {
            v.resize(n);
        }
        size_t size() const {
            return v.size();
        }
};


/** \brief Container for a structure descriptors.
 *
 * Contains:
 * - Atomic Energy Descriptors
 * - Force Descriptors (Optional)
 * - Stress Descriptors (Optional)
 *
 * To fully initialise this object it needs to know:
 * - number of atoms in a structure (from Structure)
 * - number of nn for every atom (from Structure)
 * - dimension of the descriptor vector (Config)
 * - whether force and stress is being calculated (from Config)
 *
 *   \note
 *       Required Config keys:
 *       \ref FORCE \ref STRESS.
 *       \ref INTERNAL_KEY \ref DSIZE.
 *
 */
struct StDescriptors {

    /** This constructor fully initialise this object
     *
     * Requires:
     * - Structure st to have NN calculated
     * - Config c to contain keys: \ref DSIZE, \ref FORCE, \ref STRESS
     */
    StDescriptors(const Structure &s, const Config &c);

    /** Default constructor. Object is left uninitialised */
    StDescriptors();

    /** AED for all atoms
     *
     * This object will always be initialised given
     * provided Structure contains atoms.
     *
     * Dimensions:
     * [number of atoms, descriptor size]
     */
    aeds_type2 aeds;

    /** FD for all atoms
     *
     * Dimensions:
     * [number of atoms, number of atom NN, descriptor size, 3]
     */
    std::vector<std::vector<fd_type>> fd;

    /** SD for a Structure
     *
     * Dimensions:
     * [descriptor size, 6]
     */
    sd_type sd;

    /** True if descriptors are normalised */
    bool normalised=false;

    rhos_type rhos;

    aed_type2 & get_aed(const size_t i);
    const aed_type2 &get_aed(const size_t i) const;
    rho_rtype get_rho(const size_t i);

    size_t naed() const;
    size_t dim() const;

};
#endif
