#include "catch2/catch.hpp"
#include "../CORE/element.h"

TEST_CASE( "Testing Element class constructor", "[element]" ) {
    char symbol1[]="Ti";
    int Z1 = 22;
    Element e1(symbol1,Z1);
    Element e2(symbol1,Z1);

    REQUIRE(e1==e2);

    char symbol3[]="Nb";
    int Z3 = 41;
    Element e3(symbol3,Z3);
    REQUIRE(!(e1==e3));
    REQUIRE(!(e2==e3));
}
TEST_CASE( "Testing Element class copy operator", "[element_copy]" ) {
    char symbol1[]="Ti";
    int Z1 = 22;
    Element e1(symbol1,Z1);
    Element e2=e1;
    REQUIRE(e1==e2);
}
