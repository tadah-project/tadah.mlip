#include <tadah/models/functions/kernels/kern_rbf.h>

#include <cmath>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
namespace tadah {
namespace models {


Kern_RBF::Kern_RBF () {}
Kern_RBF::Kern_RBF (const tadah::core::Context &c):
    Function_Base(c),
    Kern_Base(c),
    sigma(c.get<double>("MPARAMS")),
    gamma(1.0/(sigma*sigma))
{
    // sigma 0 (or one whose square underflows) has no width: gamma is not
    // finite, K is 0 off the basis vector and nan on it. A negative sigma
    // is the kernel of |sigma| (gamma = 1/sigma^2) and is kept.
    if (!(sigma != 0.0) || !std::isfinite(sigma) || !std::isfinite(gamma))
        throw std::invalid_argument(
            "Kern_RBF: MPARAMS (1), the width sigma, is " +
            c(std::string("MPARAMS"))[0] + ": sigma must be non-zero and "
            "finite, with 1/sigma^2 finite (K(b, x) = exp(-|b - x|^2 / "
            "(2 sigma^2))).");
    std::ostringstream settings;
    settings.copyfmt(std::cout);
    settings << " | sigma: " << sigma << " | gamma: " << gamma;
    print_settings(label, settings.str());

    read_basis_from_context(c,basis);
}
double Kern_RBF::operator() (const tadah::core::aed_type& b, const tadah::core::aed_type& af) const
{
    const tadah::core::aed_type v = (af-b);
    return exp(-0.5*gamma*(v*v));
}
tadah::core::aed_type Kern_RBF::derivative(const tadah::core::aed_type& b, const tadah::core::aed_type& af) const
{
    return  gamma*(b-af)*(*this)(b,af);
}
double Kern_RBF::prime(const tadah::core::aed_type& b, const tadah::core::aed_type& af, const tadah::core::aed_type& ff) const
{
  // std::cout << "f:     " << ff << std::endl;
    return derivative(b, af)*ff;
}
// Closed-form derivative of K w.r.t. sigma. Note: gamma = 1 / sigma^2
// is a derived member computed in the constructor; HPO never sees gamma
// directly. dK/d sigma = K * ||b - af||^2 / sigma^3.
bool Kern_RBF::calc_dKdtheta(
    const std::string& theta_name,
    int                arg_pos,
    const tadah::core::aed_type& b,
    const tadah::core::aed_type& af,
    double& dK_out) const
{
    if (theta_name != "MPARAMS" || arg_pos != 1) return false;
    const tadah::core::aed_type v = af - b;
    const double v_sq = v * v;
    const double k    = std::exp(-0.5 * gamma * v_sq);
    dK_out = k * v_sq / (sigma * sigma * sigma);
    return true;
}

std::string Kern_RBF::get_label() const
{
    return label;
}

std::vector<MParamSlot> Kern_RBF::mparams() const
{
    return {{"sigma", false, true}};
}

// K(b, x) depends on every component of x, whatever b: no column is constant.
std::vector<size_t> Kern_RBF::intercept_columns(const tadah::core::Context &) const
{
  return {};
}

} // namespace models
} // namespace tadah

