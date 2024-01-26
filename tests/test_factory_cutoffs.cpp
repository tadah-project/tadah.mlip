#include "catch2/catch.hpp"
#include <limits>
#include <stdexcept>
#include "../CORE/registry.h"
#include "../MODELS/cutoffs/cutoffs.h"
#include <string>

TEST_CASE( "Testing Factory: Cutoffs", "[factory_cutoffs]" ) {

    double rcut2b = 6.2;
    double rcut2bsq = rcut2b*rcut2b;

    for (auto c:Registry<Cut_Base,double>::registry) {
        std::string cuttype = c.first;
        Cut_Base *c2b = factory<Cut_Base,double>( cuttype, rcut2b );

        REQUIRE( c2b->label() == cuttype );

        REQUIRE( c2b->calc(rcut2b) < std::numeric_limits<double>::min() );
        REQUIRE( c2b->calc_prime(rcut2b) < std::numeric_limits<double>::min() );
        REQUIRE( std::abs(c2b->get_rcut()-rcut2b)<std::numeric_limits<double>::min() );
        REQUIRE( std::abs(c2b->get_rcut_sq()-rcut2bsq)<std::numeric_limits<double>::min() );

        // cutoff cannot be negative
        double temp = -0.1;
        REQUIRE_THROWS(factory<Cut_Base,double>( cuttype, temp ));
        REQUIRE_THROWS_AS(c2b->set_rcut(temp), std::runtime_error);
        REQUIRE_THROWS_AS(c2b->test_rcut(temp), std::runtime_error);

        // recheck after resetting cutoff
        rcut2b=3.4;
        rcut2bsq=rcut2b*rcut2b;
        c2b->set_rcut(100000);
        c2b->set_rcut(rcut2b);
        REQUIRE( c2b->calc(rcut2b) < std::numeric_limits<double>::min() );
        REQUIRE( c2b->calc_prime(rcut2b) < std::numeric_limits<double>::min() );
        REQUIRE( std::abs(c2b->get_rcut()-rcut2b)<std::numeric_limits<double>::min() );
        REQUIRE( std::abs(c2b->get_rcut_sq()-rcut2bsq)<std::numeric_limits<double>::min() );
    };
}
