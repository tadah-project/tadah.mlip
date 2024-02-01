#ifndef BASIS_H
#define BASIS_H

#include "../../CORE/config/config.h"
#include "../../CORE/core_types.h"
#include "../st_descriptors_db.h"
#include "../st_descriptors.h"

#include <numeric>
#include <stdexcept>
#include <vector>

template <typename K>
class Basis {
    private:
        Config &config;
        int verbose;
    public:
        aeds_type2 b;
        Basis(Config &c):
            config(c),
            verbose(c.get<int>("VERBOSE"))

    {    }

        void build_random_basis(size_t s, StDescriptorsDB &st_desc_db) {

            // generate indices
            std::vector<std::tuple<size_t,size_t>> indices;
            size_t counter=0;
            for( size_t st = 0; st < st_desc_db.size(); st++ ) {
                for( size_t a = 0; a < st_desc_db(st).naed() ; a++ ) {
                    indices.push_back(std::tuple<size_t,size_t>(st,a));
                    counter++;
                }
            }

            if (counter < s) {
                throw std::runtime_error("The number of requestd basis vectors is\n \
                        larger than the amount of available AEDs\n");
            }

            std::random_shuffle(indices.begin(), indices.end());

            b.resize(st_desc_db(0).dim(),s);
            // set first basis function as "bias vector"
            aed_type2 temp(st_desc_db(0).dim());
            temp.setZero();
            temp(0) = 1;
            b.col(0)=temp;
            for (size_t i=1; i<s; ++i) {
                size_t st = std::get<0>(indices[i]);
                size_t a = std::get<1>(indices[i]);
                const aed_type2 &aed = st_desc_db(st).get_aed(a);
                for (size_t j=0; j<aed.size(); ++j) {
                    b.col(i)[j]=aed[j];
                }
                //b.col(i) = st_desc_db(st).get_aed(a);
            }

        }
        void read_basis_from_config() {
            size_t sbasis = config.get<size_t>("SBASIS");
            size_t vlen = config("BASIS").size()/sbasis;
            std::vector<double> temp(sbasis*vlen);
            config.get<std::vector<double>>("BASIS",temp);
            b.resize(vlen,sbasis);
            size_t ii=0;
            for (size_t i=0;i<sbasis; ++i) {
                for (size_t j=0;j<vlen; ++j) {
                    b(j,i) = temp[ii++];
                }
            }
        }
};
#endif
