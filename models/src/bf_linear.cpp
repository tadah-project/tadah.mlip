#include <tadah/models/functions/basis_functions/bf_linear.h>
#include <tadah/core/intercept_layout.h>
namespace tadah {
namespace models {


BF_Linear::BF_Linear() {}
BF_Linear::BF_Linear(const tadah::core::Context &c):
  Function_Base(c),
  BF_Base(c) {}
std::string BF_Linear::get_label() const {
    return label;
}
std::size_t BF_Linear::weights_count(std::size_t dsize) const {
    return dsize;
}
double BF_Linear::epredict(const tadah::core::t_type &weights, const tadah::core::aed_type& aed) const
{
    return weights*aed;
}
double BF_Linear::fpredict(const tadah::core::t_type &weights, const tadah::core::fd_type &fdij,
        const tadah::core::aed_type& , const size_t k) const
{
    return -1*(fdij(k) * weights);
}
tadah::core::force_type BF_Linear::fpredict(const tadah::core::t_type &weights, const tadah::core::fd_type &fdij,
        const tadah::core::aed_type& ) const
{
    return -1*(fdij * weights);
}

std::vector<size_t> BF_Linear::intercept_columns(const tadah::core::Context &c) const
{
  // the descriptor's intercept columns, 0 .. N-1
  std::vector<size_t> cols(tadah::core::n_intercept_columns(c));
  for (size_t k = 0; k < cols.size(); ++k) cols[k] = k;
  return cols;
}

} // namespace models
} // namespace tadah

