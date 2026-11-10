#include <tadah/models/functions/kernels/kern_base.h>
#include <tadah/core/intercept_layout.h>

#include <iostream>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
namespace tadah {
namespace models {

namespace {
/* The record the open KernelSettingsLineScopes share: how many are open,
 * how many of them are Lines::None, and the labels whose line was printed
 * since the first of them opened. */
struct SettingsLineRecord {
  std::mutex mutex;
  int open = 0;
  int none = 0;
  std::set<std::string> printed;
};
SettingsLineRecord &settings_line_record() {
  static SettingsLineRecord r;
  return r;
}
}  // namespace

KernelSettingsLineScope::KernelSettingsLineScope(Lines lines) : lines_(lines)
{
  SettingsLineRecord &r = settings_line_record();
  std::lock_guard<std::mutex> lock(r.mutex);
  if (r.open++ == 0) r.printed.clear();
  if (lines_ == Lines::None) ++r.none;
}
KernelSettingsLineScope::~KernelSettingsLineScope()
{
  SettingsLineRecord &r = settings_line_record();
  std::lock_guard<std::mutex> lock(r.mutex);
  if (lines_ == Lines::None) --r.none;
  if (--r.open == 0) r.printed.clear();
}

Kern_Base::Kern_Base() {}
Kern_Base::Kern_Base(const tadah::core::Context &c): Function_Base(c) {}
Kern_Base::~Kern_Base() {}
void Kern_Base::print_settings(const std::string &label,
                               const std::string &settings)
{
  if (get_verbose() <= 0) return;
  const std::string line = "\n" + label + settings + "\n";
  SettingsLineRecord &r = settings_line_record();
  std::lock_guard<std::mutex> lock(r.mutex);
  if (r.none > 0) return;
  if (r.open > 0 && !r.printed.insert(label).second) return;
  std::cout << line << std::flush;
}
void Kern_Base::set_basis(const tadah::core::Matrix b)
{
  basis = b;
}
double Kern_Base::epredict(const tadah::core::t_type &kweights, const tadah::core::aed_type &aed) const
{
  double energy=0;
  for (size_t b=0; b<basis.cols(); ++b) {
    energy += kweights[b]*(*this)(basis.col(b),aed);
  }
  return energy;
}
double Kern_Base::fpredict(const tadah::core::t_type &kweights, const tadah::core::fd_type &fdij,
                           const tadah::core::aed_type &aedi, const size_t k) const
{
  double res=0.0;
  for (size_t b=0; b<basis.cols(); ++b) {
    res -= kweights[b]*(*this).prime(basis.col(b),aedi,fdij(k));
  }
  return res;
}
tadah::core::force_type Kern_Base::fpredict(const tadah::core::t_type &kweights, const tadah::core::fd_type &fdij,
                               const tadah::core::aed_type &aedi) const
{
  tadah::core::force_type v;
  v.set_zero();
  for (size_t b=0; b<basis.cols(); ++b) {
    for (size_t k=0; k<3; ++k) {
      v(k) -= kweights[b]*(*this).prime(basis.col(b),aedi,fdij(k));
    }
  }
  return v;
}
void Kern_Base::read_basis_from_context(const tadah::core::Context &context, tadah::core::Matrix &basis) {
  // storage is in column-major order
  if (!context.contains("BASIS")) return;
  if (!context.contains("SBASIS"))
    throw std::runtime_error(
        "BASIS without SBASIS: SBASIS names the number of basis vectors "
        "stored in BASIS.");
  const size_t sbasis = context.get<size_t>("SBASIS");
  const size_t nvalues = context.size("BASIS");
  if (sbasis == 0)
    throw std::runtime_error(
        "SBASIS 0 with " + std::to_string(nvalues) + " BASIS values: "
        "SBASIS must be the number of basis vectors stored in BASIS.");
  if (nvalues == 0 || nvalues % sbasis != 0)
    throw std::runtime_error(
        "BASIS has " + std::to_string(nvalues) + " values, which is not a "
        "multiple of SBASIS " + std::to_string(sbasis) + ": BASIS stores "
        "SBASIS basis vectors of the descriptor's length, one after another.");
  size_t vlen = nvalues/sbasis;
  std::vector<double> temp(sbasis*vlen);
  context.get<std::vector<double>>("BASIS",temp);
  basis.resize(vlen,sbasis);  // aeds are in column-major order
  size_t ii=0;
  for (size_t i=0;i<sbasis; ++i) {
    for (size_t j=0;j<vlen; ++j) {
      basis(j,i) = temp[ii++];
    }
  }
}
std::size_t Kern_Base::weights_count(std::size_t) const {
  return basis.cols();
}
std::size_t Kern_Base::basis_vector_size() const {
  return basis.rows();
}
std::vector<size_t> Kern_Base::dot_product_intercept_columns(const tadah::core::Context &c) const {
  // Column k < N is an intercept when its basis vector has no component
  // other than k (intercept component k): K(b_k, x) = f(b_kk x_k).
  const size_t n = tadah::core::n_intercept_columns(c);
  std::vector<size_t> cols;
  for (size_t k=0; k<n && k<basis.cols(); ++k) {
    bool only_k = true;
    for (size_t j=0; j<basis.rows() && only_k; ++j)
      if (j != k && basis(j,k) != 0.0) only_k = false;
    if (only_k) cols.push_back(k);
  }
  return cols;
}
const tadah::core::Matrix &Kern_Base::get_basis() const {
  return basis;
}
tadah::core::Matrix Kern_Base::get_basis() {
  return basis;
}

} // namespace models
} // namespace tadah

