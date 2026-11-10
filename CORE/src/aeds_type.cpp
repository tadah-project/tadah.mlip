#include <tadah/core/aeds_type.h>
#include <tadah/core/aed_type.h>

namespace tadah {
namespace core {

aeds_type2::aeds_type2() {}

aeds_type2::aeds_type2(size_t dim, size_t natoms)
    : dim_(dim), natoms_(natoms) {
    data_ = static_cast<double*>(malloc(dim * natoms * sizeof(double)));
    std::fill(data_, data_ + dim * natoms, 0.0);

    aptr = new aed_type*[natoms];
    for (size_t i = 0; i < natoms; ++i) {
        aptr[i] = new aed_type(&data_[i * dim], dim);
    }
}

aeds_type2::aeds_type2(const aeds_type2& other)
    : dim_(other.dim_), natoms_(other.natoms_) {
    data_ = static_cast<double*>(malloc(dim_ * natoms_ * sizeof(double)));
    std::memcpy(data_, other.data_, dim_ * natoms_ * sizeof(double));

    aptr = new aed_type*[natoms_];
    for (size_t i = 0; i < natoms_; ++i) {
        aptr[i] = new aed_type(&data_[i * dim_], dim_);
    }
}

aeds_type2::aeds_type2(aeds_type2&& other) noexcept
    : data_(other.data_), dim_(other.dim_), natoms_(other.natoms_), aptr(other.aptr) {
    other.data_ = nullptr;
    other.aptr = nullptr;
    other.dim_ = 0;
    other.natoms_ = 0;
}

aeds_type2::~aeds_type2() {
    for (size_t i = 0; i < natoms_; ++i) {
        delete aptr[i];
    }
    delete[] aptr;
    free(data_);
}

aeds_type2& aeds_type2::operator=(const aeds_type2& other) {
    if (this != &other) {
        for (size_t i = 0; i < natoms_; ++i) {
            delete aptr[i];
        }
        delete[] aptr;
        free(data_);

        dim_ = other.dim_;
        natoms_ = other.natoms_;

        data_ = static_cast<double*>(malloc(dim_ * natoms_ * sizeof(double)));
        std::memcpy(data_, other.data_, dim_ * natoms_ * sizeof(double));

        aptr = new aed_type*[natoms_];
        for (size_t i = 0; i < natoms_; ++i) {
            aptr[i] = new aed_type(&data_[i * dim_], dim_);
        }
    }
    return *this;
}

aeds_type2& aeds_type2::operator=(aeds_type2&& other) noexcept {
    if (this != &other) {
        for (size_t i = 0; i < natoms_; ++i) {
            delete aptr[i];
        }
        delete[] aptr;
        free(data_);

        data_ = other.data_;
        dim_ = other.dim_;
        natoms_ = other.natoms_;
        aptr = other.aptr;

        other.data_ = nullptr;
        other.aptr = nullptr;
        other.dim_ = 0;
        other.natoms_ = 0;
    }
    return *this;
}

void aeds_type2::resize(size_t dim, size_t natoms) {
    for (size_t i = 0; i < natoms_; ++i) {
        delete aptr[i];
    }
    delete[] aptr;

    dim_ = dim;
    natoms_ = natoms;

    data_ = static_cast<double*>(realloc(data_, dim * natoms * sizeof(double)));
    std::fill(data_, data_ + dim * natoms, 0.0);

    aptr = new aed_type*[natoms];
    for (size_t i = 0; i < natoms; ++i) {
        aptr[i] = new aed_type(&data_[i * dim], dim);
    }
}

void aeds_type2::set_zero() {
    std::fill(data_, data_ + dim_ * natoms_, 0.0);
}

std::ostream& operator<<(std::ostream& os, const aeds_type2& v) {
    for (size_t i = 0; i < v.dim_; ++i) {
        for (size_t j = 0; j < v.natoms_; ++j) {
            os << v.data_[j * v.dim_ + i] << " ";
        }
        os << std::endl;
    }
    return os;
}


} // namespace core
} // namespace tadah

