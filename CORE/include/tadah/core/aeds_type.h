#ifndef TADAH_CORE_AEDS_TYPE_H
#define TADAH_CORE_AEDS_TYPE_H
#include <tadah/core/aed_type.h>

#include <cstdlib>
#include <cstring>
#include <iostream>

namespace tadah {
namespace core {

class aeds_type2 {
  /* Column major order
   *
   * Each column represents single aed_type of dimension dim_
   */
    double* data_ = nullptr;
    size_t dim_ = 0;  // nrows
    size_t natoms_ = 0; // ncols
    aed_type** aptr = nullptr;

public:
    aeds_type2();
    aeds_type2(size_t dim, size_t natoms);
    aeds_type2(const aeds_type2& other);
    aeds_type2(aeds_type2&& other) noexcept;
    ~aeds_type2();

    aeds_type2& operator=(const aeds_type2& other);
    aeds_type2& operator=(aeds_type2&& other) noexcept;

    inline double& operator()(size_t r, size_t c) {
        return data_[c * dim_ + r];
    }

    inline const double& operator()(size_t r, size_t c) const {
        return data_[c * dim_ + r];
    }

    inline aed_type& operator[](size_t c) {
        return *aptr[c];
    }

    inline const aed_type& operator[](size_t c) const {
        return *aptr[c];
    }

    inline aed_type& col(size_t c) {
        return *aptr[c];
    }

    inline const aed_type& col(size_t c) const {
        return *aptr[c];
    }

    inline size_t cols() const {
        return natoms_;
    }

    inline size_t rows() const {
        return dim_;
    }

    inline size_t size() const {
        return dim_ * natoms_;
    }

    void resize(size_t dim, size_t natoms);
    void set_zero();


    friend std::ostream& operator<<(std::ostream& os, const aeds_type2& v);
};

} // namespace core
} // namespace tadah
#endif // AEDS_TYPE2_H
