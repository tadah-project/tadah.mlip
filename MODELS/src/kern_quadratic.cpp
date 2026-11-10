#include <tadah/models/functions/kernels/kern_quadratic.h>
namespace tadah {
namespace models {


Kern_Quadratic::Kern_Quadratic () {}
Kern_Quadratic::Kern_Quadratic (const tadah::core::Context &c):
    Function_Base(c),
    Kern_Base(c)
{
    print_settings(label);

    read_basis_from_context(c,basis);
}
double Kern_Quadratic::operator() (const tadah::core::aed_type& b, const tadah::core::aed_type& af) const
{
    double x = b*af;
    return  x*x;
}
tadah::core::aed_type Kern_Quadratic::derivative(const tadah::core::aed_type& b, const tadah::core::aed_type& af) const
{
    double temp = 2*(b*af);
    return b*temp;
}
double Kern_Quadratic::prime(const tadah::core::aed_type& b, const tadah::core::aed_type& af, const tadah::core::aed_type& ff) const
{
    //return dot(derivative(b, af), ff);
    // it is faster this way
    double scale = 2.0*(b*af);
    return scale*(ff*b);
}
std::string Kern_Quadratic::get_label() const
{
    return label;
}

std::vector<size_t> Kern_Quadratic::intercept_columns(const tadah::core::Context &c) const
{
  return dot_product_intercept_columns(c);
}

} // namespace models
} // namespace tadah

