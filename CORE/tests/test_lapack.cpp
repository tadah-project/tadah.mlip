#include "catch2/catch.hpp"
#include <tadah/core/lapack.h>
#include <tadah/core/core_types.h>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <stdexcept>
#include <string>
#include <utility>

using namespace tadah::core;

TEST_CASE( "Testing condition_number()", "[lapack]" ) {
    Matrix m(2,2);
    m(0,0)=1.0;
    m(0,1)=2.0;
    m(1,0)=3.0;
    m(1,1)=4.0;
    REQUIRE_THAT( condition_number(m,'U'), Catch::Matchers::WithinAbs(0.22916666666667, 1e-12) );
}

TEST_CASE( "Testing solve_posv() 1", "[lapack]" ) {
    Matrix m(2,2);
    m(0,0)=2;
    m(0,1)=6;
    m(1,0)=6;
    m(1,1)=20;

    aed_type a(2);
    a(0)=11.11;
    a(1)=21.12;
    aed_type res=solve_posv(m,a,'U');
    REQUIRE_THAT( res(0), Catch::Matchers::WithinAbs(23.87, 1e-12) );
    REQUIRE_THAT( res(1), Catch::Matchers::WithinAbs(-6.10499999999998, 1e-12) );
}

TEST_CASE( "Testing solve_posv() 2", "[lapack]" ) {
    // matrix is to positive definite
    Matrix m(2,2);
    m(0,0)=2;
    m(0,1)=6;
    m(1,0)=6;
    m(1,1)=14;

    aed_type a(2);
    a(0)=11.11;
    a(1)=21.12;
    aed_type res=solve_posv(m,a,'U');
    REQUIRE_THAT( res(0), Catch::Matchers::WithinAbs(11.11, 1e-12) );
    REQUIRE_THAT( res(1), Catch::Matchers::WithinAbs(21.12, 1e-12) );
}

TEST_CASE( "Testing norm_X()", "[lapack]" ) {
    Matrix m(2,2);
    m(0,0)=1.0;
    m(0,1)=2.0;
    m(1,0)=3.0;
    m(1,1)=4.0;
    REQUIRE_THAT( norm_X(m,'m'), Catch::Matchers::WithinAbs(0.25, 1e-12) );
    REQUIRE_THAT( norm_X(m,'o'), Catch::Matchers::WithinAbs(0.1666666666667, 1e-12) );
    REQUIRE_THAT( norm_X(m,'i'), Catch::Matchers::WithinAbs(0.14285714285714, 1e-12) );
    REQUIRE_THAT( norm_X(m,'e'), Catch::Matchers::WithinAbs(0.18257418583505, 1e-12) );
}
TEST_CASE( "Testing T_dgemv()", "[lapack]" ) {
    Matrix m(2,2);
    m(0,0)=2;
    m(0,1)=6;
    m(1,0)=6;
    m(1,1)=14;

    aed_type a(2);
    a(0)=11.11;
    a(1)=21.12;
    aed_type res=T_dgemv(m,a,'T');
    REQUIRE_THAT( res(0), Catch::Matchers::WithinAbs(148.94, 1e-12) );
    REQUIRE_THAT( res(1), Catch::Matchers::WithinAbs(362.34, 1e-12) );
}
