#include <tadah/models/functions/f_all.h>
#include <tadah/core/registry.h>
namespace tadah {
namespace models {


template<>
tadah::core::Registry<Function_Base>::Map& tadah::core::Registry<Function_Base>::registry =
  tadah::core::Registry<tadah::models::Function_Base>::instance();
template<>
tadah::core::Registry<Function_Base,tadah::core::Context&>::Map& tadah::core::Registry<Function_Base,tadah::core::Context&>::registry =
  tadah::core::Registry<tadah::models::Function_Base,tadah::core::Context&>::instance();

tadah::core::Registry<Function_Base>::Register<BF_Linear> BF_Linear_1( "BF_Linear" );
tadah::core::Registry<Function_Base,tadah::core::Context&>::Register<BF_Linear> BF_Linear_2( "BF_Linear" );
tadah::core::Registry<Function_Base>::Register<BF_Polynomial2> BF_Polynomial2_1( "BF_Polynomial2" );
tadah::core::Registry<Function_Base,tadah::core::Context&>::Register<BF_Polynomial2> BF_Polynomial2_2( "BF_Polynomial2" );
tadah::core::Registry<Function_Base>::Register<Kern_Linear> Kern_Linear_1( "Kern_Linear" );
tadah::core::Registry<Function_Base,tadah::core::Context&>::Register<Kern_Linear> Kern_Linear_2( "Kern_Linear" );
tadah::core::Registry<Function_Base>::Register<Kern_LQ> Kern_LQ_1( "Kern_LQ" );
tadah::core::Registry<Function_Base,tadah::core::Context&>::Register<Kern_LQ> Kern_LQ_2( "Kern_LQ" );
tadah::core::Registry<Function_Base>::Register<Kern_Polynomial> Kern_Polynomial_1( "Kern_Polynomial" );
tadah::core::Registry<Function_Base,tadah::core::Context&>::Register<Kern_Polynomial> Kern_Polynomial_2( "Kern_Polynomial" );
tadah::core::Registry<Function_Base>::Register<Kern_Quadratic> Kern_Quadratic_1( "Kern_Quadratic" );
tadah::core::Registry<Function_Base,tadah::core::Context&>::Register<Kern_Quadratic> Kern_Quadratic_2( "Kern_Quadratic" );
tadah::core::Registry<Function_Base>::Register<Kern_RBF> Kern_RBF_1( "Kern_RBF" );
tadah::core::Registry<Function_Base,tadah::core::Context&>::Register<Kern_RBF> Kern_RBF_2( "Kern_RBF" );
tadah::core::Registry<Function_Base>::Register<Kern_Sigmoid> Kern_Sigmoid_1( "Kern_Sigmoid" );
tadah::core::Registry<Function_Base,tadah::core::Context&>::Register<Kern_Sigmoid> Kern_Sigmoid_2( "Kern_Sigmoid" );


} // namespace models
} // namespace tadah
