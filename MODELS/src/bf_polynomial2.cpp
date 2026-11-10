#include <tadah/models/functions/basis_functions/bf_polynomial2.h>
#include <tadah/core/intercept_layout.h>

#include <stdexcept>
#include <string>
#include <vector>
namespace tadah {
namespace models {


BF_Polynomial2::BF_Polynomial2() {}
BF_Polynomial2::BF_Polynomial2(const tadah::core::Context &c): 
  Function_Base(c),
  BF_Base(c)
{}
std::string BF_Polynomial2::get_label() const
{
  return label;
}
std::size_t BF_Polynomial2::weights_count(std::size_t dsize) const
{
  return dsize*(dsize+1)/2;
}
double BF_Polynomial2::epredict(const tadah::core::t_type &weights, const tadah::core::aed_type& aed) const
{
  size_t b=0;
  double res=0.0;
  for (size_t i=0; i<aed.size(); ++i) {
    for (size_t ii=i; ii<aed.size(); ++ii) {
      res+= weights(b++)*aed(i)*aed(ii);
    }
  }
  return res;
}
double BF_Polynomial2::fpredict(const tadah::core::t_type &weights, const tadah::core::fd_type &fdij,
                                const tadah::core::aed_type& aedi, const size_t k) const
{
  double res=0.0;
  size_t b=0;
  for (size_t i=0; i<fdij.rows(); ++i) {
    for (size_t ii=i; ii<fdij.rows(); ++ii) {
      res -= weights(b++)*(fdij(i,k)*aedi(ii) + fdij(ii,k)*aedi(i));
    }
  }
  return res;
}
tadah::core::force_type BF_Polynomial2::fpredict(const tadah::core::t_type &weights, const tadah::core::fd_type &fdij,
                                    const tadah::core::aed_type& aedi) const
{
  tadah::core::force_type v;
  for (size_t k=0; k<3; ++k) {
    size_t b=0;
    for (size_t i=0; i<fdij.rows(); ++i) {
      for (size_t ii=i; ii<fdij.rows(); ++ii) {
        v(k) -= weights(b++)*(fdij(i,k)*aedi(ii) + fdij(ii,k)*aedi(i));
      }
    }
  }
  return v;
}

std::vector<size_t> BF_Polynomial2::intercept_columns(const tadah::core::Context &c) const
{
  // The product (k,k) of intercept component k with itself is 1 for an
  // atom of species k and 0 for the others; with the products (i,ii),
  // i <= ii, taken row by row (epredict), (k,k) is column
  // k D - k (k - 1) / 2. The product of two different intercept components
  // is 0 for every atom.
  const size_t n = tadah::core::n_intercept_columns(c);
  std::vector<size_t> cols;
  if (n == 0) return cols;
  cols.push_back(0);
  if (n == 1) return cols;
  const size_t D = c.get<size_t>("DSIZE");
  if (D < n)
    throw std::logic_error(
        "BF_Polynomial2::intercept_columns: the intercept columns of " +
        std::to_string(n) + " species depend on the descriptor size, but "
        "DSIZE is " + std::to_string(D) + " (DescriptorsCalc sets it)");
  for (size_t k = 1; k < n; ++k) cols.push_back(k * D - k * (k - 1) / 2);
  return cols;
}

} // namespace models
} // namespace tadah

