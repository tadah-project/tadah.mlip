#include <tadah/models/functions/kernels/kern_linear.h>
#include <tadah/core/intercept_layout.h>
namespace tadah {
namespace models {


Kern_Linear::Kern_Linear() {}
Kern_Linear::Kern_Linear (const tadah::core::Context &c):
    Function_Base(c),
    Kern_Base(c)
{}
double Kern_Linear::operator() (const tadah::core::aed_type& b, const tadah::core::aed_type& af) const
{
    return b*af;;
}
tadah::core::aed_type Kern_Linear::derivative(const tadah::core::aed_type& b, const tadah::core::aed_type&) const
{
    return b;
}
double Kern_Linear::prime(const tadah::core::aed_type& b, const tadah::core::aed_type& , const tadah::core::aed_type& ff) const
{
    // return derivative(b,af).transpose() * ff;
    return b * ff;
}
std::string Kern_Linear::get_label() const
{
    return label;
}
std::size_t Kern_Linear::weights_count(std::size_t dsize) const
{
    return dsize;
}
std::size_t Kern_Linear::basis_vector_size() const
{
    return 0;
}
double Kern_Linear::epredict(const tadah::core::t_type &weights, const tadah::core::aed_type& aed) const
{
    return weights*aed;
}
double Kern_Linear::fpredict(const tadah::core::t_type &weights, const tadah::core::fd_type &fdij,
        const tadah::core::aed_type& , const size_t k) const
{
    return -(fdij(k) * weights);
}
tadah::core::force_type Kern_Linear::fpredict(const tadah::core::t_type &weights, const tadah::core::fd_type &fdij,
        const tadah::core::aed_type& ) const
{
    return -(fdij * weights);
}

std::vector<size_t> Kern_Linear::intercept_columns(const tadah::core::Context &c) const
{
  // the descriptor's intercept columns, 0 .. N-1
  std::vector<size_t> cols(tadah::core::n_intercept_columns(c));
  for (size_t k = 0; k < cols.size(); ++k) cols[k] = k;
  return cols;
}

} // namespace models
} // namespace tadah

