#include "catch2/catch.hpp"
#include <limits>
#include <stdexcept>
#include <tadah/core/registry.h>
#include <tadah/core/utils.h>
#include <tadah/models/cutoffs.h>
#include <string>

using namespace tadah::core;
using namespace tadah::models;
TEST_CASE( "Testing Factory: Cutoffs", "[factory_cutoffs]" ) {

  double rcut2b = 6.2;
  double rcut2bsq = rcut2b*rcut2b;

  for (auto c:Registry<Cut_Base,double&>::registry) {
    std::string cuttype = c.first;
    Cut_Base *c2b = factory<Cut_Base,double&>( cuttype, rcut2b );

    REQUIRE( c2b->label() == cuttype );

    double vrcut = c2b->val_at_rcut();
    REQUIRE( std::abs(c2b->calc(rcut2b) - vrcut) < std::numeric_limits<double>::min() );
    REQUIRE( c2b->calc_prime(rcut2b) < std::numeric_limits<double>::min() );
    REQUIRE( std::abs(c2b->get_rcut()-rcut2b)<std::numeric_limits<double>::min() );
    REQUIRE( std::abs(c2b->get_rcut_sq()-rcut2bsq)<std::numeric_limits<double>::min() );

    // cutoff cannot be negative
    double temp = -0.1;
    REQUIRE_THROWS(factory<Cut_Base,double&>( cuttype, temp ));
    REQUIRE_THROWS_AS(c2b->set_rcut(temp), std::runtime_error);
    REQUIRE_THROWS_AS(c2b->test_rcut(temp), std::runtime_error);

    // recheck after resetting cutoff
    rcut2b=3.4;
    rcut2bsq=rcut2b*rcut2b;
    c2b->set_rcut(100000);
    c2b->set_rcut(rcut2b);
    vrcut = c2b->val_at_rcut();
    REQUIRE( std::abs(c2b->calc(rcut2b) - vrcut) < std::numeric_limits<double>::min() );
    REQUIRE( c2b->calc_prime(rcut2b) < std::numeric_limits<double>::min() );
    REQUIRE( std::abs(c2b->get_rcut()-rcut2b)<std::numeric_limits<double>::min() );
    REQUIRE( std::abs(c2b->get_rcut_sq()-rcut2bsq)<std::numeric_limits<double>::min() );
    if (c2b) delete c2b;
  };
}
TEST_CASE( "Testing Factory: Cutoff's Derivatives", "[factory_cutoffs]" ) {

  double rcut2b = 6.2;
  std::vector<double> rs = {0.0, 1.0, 3.3, 4.234, rcut2b};

  for (auto c:Registry<Cut_Base,double&>::registry) {
    std::string cuttype = c.first;
    Cut_Base *c2b = factory<Cut_Base,double&>( cuttype, rcut2b );
    if (c2b->kind() == Cut_Base::Kind::Discontinuous) { delete c2b; continue; }

    REQUIRE( c2b->label() == cuttype );
    for (const double r : rs) {
      double prime_approx = central_difference([&](double x){ return c2b->calc(x); }, r, 1e-8);
      REQUIRE( std::abs(c2b->calc_prime(r)-prime_approx)<1e-6);
    }

    if (c2b) delete c2b;
  };
}
