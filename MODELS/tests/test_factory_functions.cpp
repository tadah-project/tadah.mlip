#include "catch2/catch.hpp"
#include <limits>
#include <stdexcept>
#include <tadah/core/registry.h>
#include <tadah/models/functions/f_all.h>
#include <string>

using namespace tadah::core;
using namespace tadah::models;
TEST_CASE( "Testing Factory: Functions", "[factory_functions]" ) {

  for (auto f:Registry<Function_Base>::registry) {
    std::string fb_name = f.first;
    Function_Base *fb = factory<Function_Base>(fb_name);

    REQUIRE( fb->get_label() == fb_name );

    REQUIRE( fb->get_label() == fb_name );
    fb->set_verbose(1);
    REQUIRE( fb->get_verbose() == 1 );
    fb->set_verbose(0);
    REQUIRE( fb->get_verbose() == 0 );
    fb->set_verbose(2);
    REQUIRE( fb->get_verbose() == 2 );

    // epredict()
    // fpredict()
    // fpredict(k)
    // set_verbose()
    // get_verbose()
    // KERNELS ONLY: operator()  
    // KERNELS ONLY: derivative()
    // KERNELS ONLY: prime()
    // KERNELS ONLY: set_basis()

    //REQUIRE( c2b->calc(rcut2b) < std::numeric_limits<double>::min() );
    //REQUIRE( c2b->calc_prime(rcut2b) < std::numeric_limits<double>::min() );
    //REQUIRE( std::abs(c2b->get_rcut()-rcut2b)<std::numeric_limits<double>::min() );
    //REQUIRE( std::abs(c2b->get_rcut_sq()-rcut2bsq)<std::numeric_limits<double>::min() );

    //// cutoff cannot be negative
    //double temp = -0.1;
    //REQUIRE_THROWS(factory<Cut_Base,double>( cuttype, temp ));
    //REQUIRE_THROWS_AS(c2b->set_rcut(temp), std::runtime_error);
    //REQUIRE_THROWS_AS(c2b->test_rcut(temp), std::runtime_error);

    //// recheck after resetting cutoff
    //rcut2b=3.4;
    //rcut2bsq=rcut2b*rcut2b;
    //c2b->set_rcut(100000);
    //c2b->set_rcut(rcut2b);
    //REQUIRE( c2b->calc(rcut2b) < std::numeric_limits<double>::min() );
    //REQUIRE( c2b->calc_prime(rcut2b) < std::numeric_limits<double>::min() );
    //REQUIRE( std::abs(c2b->get_rcut()-rcut2b)<std::numeric_limits<double>::min() );
    //REQUIRE( std::abs(c2b->get_rcut_sq()-rcut2bsq)<std::numeric_limits<double>::min() );
    if (fb) delete fb;
  }
}
