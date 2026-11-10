#include <tadah/models/functions/kernels/kern_sigmoid.h>

#include <iostream>
#include <sstream>

namespace tadah {
namespace models {


Kern_Sigmoid::Kern_Sigmoid () {}
Kern_Sigmoid::Kern_Sigmoid (const tadah::core::Context &context):
    Function_Base(context),
    Kern_Base(context),
    gamma(context.get<double>("MPARAMS",0)),
    c(context.get<double>("MPARAMS",1))
{
    std::ostringstream settings;
    settings.copyfmt(std::cout);
    settings << " | gamma: " << gamma << " | c: " << c;
    print_settings(label, settings.str());

    read_basis_from_context(context,basis);
}
double Kern_Sigmoid::operator() (const tadah::core::aed_type& b, const tadah::core::aed_type& af) const
{
    return std::tanh(gamma*(b*af)+c);
}
tadah::core::aed_type Kern_Sigmoid::derivative(const tadah::core::aed_type& b, const tadah::core::aed_type& af) const
{
    return gamma*(1.0-std::pow((*this)(af,b),2))*b;
}
double Kern_Sigmoid::prime(const tadah::core::aed_type& b, const tadah::core::aed_type& af, const tadah::core::aed_type& ff) const
{
    return derivative(b, af)*ff;
}
// Closed-form derivatives of K w.r.t. gamma and c.
// K = tanh(u), u = gamma * (b.af) + c; (1 - K^2) is the sech^2 factor.
bool Kern_Sigmoid::calc_dKdtheta(
    const std::string& theta_name,
    int                arg_pos,
    const tadah::core::aed_type& b,
    const tadah::core::aed_type& af,
    double& dK_out) const
{
    if (theta_name != "MPARAMS") return false;
    const double dot = b * af;
    const double k = std::tanh(gamma * dot + c);
    const double sech2 = 1.0 - k * k;
    if (arg_pos == 1) {        // dK/d gamma
        dK_out = sech2 * dot;
        return true;
    }
    if (arg_pos == 2) {        // dK/d c
        dK_out = sech2;
        return true;
    }
    return false;
}

std::vector<MParamSlot> Kern_Sigmoid::mparams() const
{
    return {{"gamma", false, false}, {"c", false, false}};
}

std::string Kern_Sigmoid::get_label() const
{
    return label;
}

std::vector<size_t> Kern_Sigmoid::intercept_columns(const tadah::core::Context &c) const
{
  return dot_product_intercept_columns(c);
}

} // namespace models
} // namespace tadah

