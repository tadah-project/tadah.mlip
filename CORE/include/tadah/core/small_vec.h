#pragma once
#include <cstddef>
#include <cstring>
#include <memory>

namespace tadah::core
{
template<std::size_t N = 256>
class SmallVec
{
    double  small_[N];
    double* data_;
    std::size_t n_;

public:
    explicit SmallVec(std::size_t n) : data_( small_), n_(n)
    {
        if(n > N) data_ = new double[n];
    }
    ~SmallVec(){ if(data_ != small_) delete [] data_; }

    SmallVec(const SmallVec&)            = delete;
    SmallVec& operator=(const SmallVec&) = delete;

    double& operator()(std::size_t i)             { return data_[i]; }
    const double& operator()(std::size_t i) const { return data_[i]; }

    void set_zero() { std::memset(data_, 0, n_ * sizeof(double)); }

    operator double*()             { return data_; }
    operator const double*() const { return data_; }

    double* data()             { return data_; }
};
}   // namespace tadah::core
