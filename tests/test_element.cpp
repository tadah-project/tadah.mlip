#include "catch2/catch.hpp"
#include "../CORE/element.h"

TEST_CASE( "Testing Element class constructor", "[element]" ) {
    std::string symbol1="Ti";
    std::string name1="Titanium";
    int Z1 = 22;
    Element e1(symbol1,name1,Z1);
    Element e2(symbol1,name1,Z1);

    REQUIRE(e1==e2);

    std::string symbol3="Nb";
    std::string name3="Niobium";
    int Z3 = 41;
    Element e3(symbol3,name3,Z3);
    REQUIRE(!(e1==e3));
    REQUIRE(!(e2==e3));
}
TEST_CASE( "Testing Element class copy operator", "[element_copy]" ) {
    std::string symbol1="Ti";
    std::string name1="Titanium";
    int Z1 = 22;
    Element e1(symbol1,name1,Z1);
    Element e2=e1;
    REQUIRE(e1==e2);
}
