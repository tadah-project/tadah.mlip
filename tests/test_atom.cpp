#include "catch2/catch.hpp"
#include "../MLIP/atom.h"
#include <string>

using vec=Eigen::Vector3d;

std::string symbol1="Ti";
std::string name1="Titanium";
int Z1 = 22;
vec pos1(1.0,2.0,3.0);
vec force1(4.0,5.0,6.0);
Element element1(symbol1,name1,Z1);

std::string symbol2="Nb";
std::string name2="Niobium";
int Z2 = 41;
vec pos2(-1.0,-2.0,-3.0);
vec force2(-4.0,-5.0,-6.0);
Element element2(symbol2,name2,Z2);

TEST_CASE( "Testing Atom class constructor", "[atom]" ) {


    // Trivial case 1/2
    Atom a(element1, 1.0, 2.0, 3.0, 
            4.0, 5.0, 6.0);

    REQUIRE( pos1 == a.position );
    REQUIRE( force1 == a.force );
    REQUIRE( name1 == a.name );
    REQUIRE( symbol1 == a.symbol );
    REQUIRE( Z1 == a.Z );

    // Trivial case 2/2
    Atom b;
    b.position = pos1;
    b.force = force1;
    b.name = name1;
    b.symbol = symbol1;
    b.Z = Z1;

    REQUIRE( pos1 == b.position );
    REQUIRE( force1 == b.force );
    REQUIRE( name1 == b.name );
    REQUIRE( symbol1 == b.symbol );
    REQUIRE( Z1 == b.Z );
}
TEST_CASE( "Testing Atom operator==", "[atom_operator==]" ) {
    Atom a(element1, 1.0, 2.0, 3.0, 
            4.0, 5.0, 6.0);
    Atom b(element1, 1.0, 2.0, 3.0, 
            4.0, 5.0, 6.0);
    REQUIRE(a==b);

    Atom c(element2, -1.0, -2.0, -3.0, 
            -4.0, -5.0, -6.0);
    REQUIRE(!(a==c));
}
TEST_CASE( "Testing Atom copy", "[atom_copy]" ) {
    Atom a(element1, 1.0, 2.0, 3.0, 
            4.0, 5.0, 6.0);
    Atom b=a;
    REQUIRE(a==b);
}
