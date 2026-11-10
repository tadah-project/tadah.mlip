#include <tadah/models/functions/kernels/kern_polynomial.h>

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
namespace tadah {
namespace models {


namespace {
// MPARAMS (1), the degree, read as an integer: an exact integer value in
// any spelling (2, 2.0); another value is refused (Context's integer read),
// with the kernel and the parameter named.
int polynomial_degree(const tadah::core::Context &context) {
    try {
        return context.get<int>("MPARAMS",0);
    } catch (const std::exception &e) {
        throw std::runtime_error(
            std::string("Kern_Polynomial: MPARAMS (1), the degree d: ") +
            e.what());
    }
}
}  // namespace

Kern_Polynomial::Kern_Polynomial () {}
Kern_Polynomial::Kern_Polynomial (const tadah::core::Context &context):
    Function_Base(context),
    Kern_Base(context),
    d(polynomial_degree(context)),
    gamma(context.get<double>("MPARAMS",1)),
    c(context.get<double>("MPARAMS",2))
{
    std::ostringstream settings;
    settings.copyfmt(std::cout);
    settings << " | degree: " << d << " | gamma: " << gamma << " | c: " << c;
    print_settings(label, settings.str());
    read_basis_from_context(context,basis);
}
double Kern_Polynomial::operator() (const tadah::core::aed_type& b, const tadah::core::aed_type& af) const
{
    return std::pow(gamma*(b*af)+c,d);
}
tadah::core::aed_type Kern_Polynomial::derivative(const tadah::core::aed_type& b, const tadah::core::aed_type& af) const
{
    return  d*gamma*std::pow(gamma*(b*af)+c,d-1)*b;
}
double Kern_Polynomial::prime(const tadah::core::aed_type& b, const tadah::core::aed_type& af, const tadah::core::aed_type& ff) const
{
    return derivative(b, af)*ff;
}
// Closed-form derivatives of K w.r.t. gamma and c. d is integer and
// is not HPO-tunable. Common factor: d * (gamma * b.af + c)^(d-1).
bool Kern_Polynomial::calc_dKdtheta(
    const std::string& theta_name,
    int                arg_pos,
    const tadah::core::aed_type& b,
    const tadah::core::aed_type& af,
    double& dK_out) const
{
    if (theta_name != "MPARAMS") return false;
    const double dot = b * af;
    const double base = gamma * dot + c;
    const double common = static_cast<double>(d) * std::pow(base, d - 1);
    if (arg_pos == 2) {        // dK/d gamma
        dK_out = common * dot;
        return true;
    }
    if (arg_pos == 3) {        // dK/d c
        dK_out = common;
        return true;
    }
    return false;
}

std::vector<MParamSlot> Kern_Polynomial::mparams() const
{
    return {{"d", true, true}, {"gamma", false, false}, {"c", false, false}};
}

std::string Kern_Polynomial::get_label() const
{
    return label;
}

std::vector<size_t> Kern_Polynomial::intercept_columns(const tadah::core::Context &c) const
{
  return dot_product_intercept_columns(c);
}

} // namespace models
} // namespace tadah

