#include <tadah/models/functions/function_base.h>
namespace tadah {
namespace models {


Function_Base::Function_Base(): verbose(0) {
}
Function_Base::Function_Base(const tadah::core::Context &c) {
  verbose = (c.contains("VERBOSE")) ? c.get<int>("VERBOSE") : 0;
}
void Function_Base::set_verbose(int v) { verbose=v; }
int Function_Base::get_verbose() { return verbose; }

Function_Base::~Function_Base() {}

double Function_Base::operator() (const tadah::core::aed_type& , const tadah::core::aed_type& ) const { return 0.0; }
tadah::core::aed_type Function_Base::derivative(const tadah::core::aed_type& , const tadah::core::aed_type& ) const { return tadah::core::aed_type(); }
double Function_Base::prime(const tadah::core::aed_type& , const tadah::core::aed_type& ,
                            const tadah::core::aed_type& ) const { return 0.0; }
void Function_Base::set_basis(const tadah::core::Matrix ) {}
std::size_t Function_Base::basis_vector_size() const { return 0; }

} // namespace models
} // namespace tadah

