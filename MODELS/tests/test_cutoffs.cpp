#include "catch2/catch.hpp"
#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>
#include <tadah/core/registry.h>
#include <tadah/models/cutoffs.h>
#include <string>

using namespace tadah::core;
using namespace tadah::models;
TEST_CASE( "Testing Cutoffs: Cut_Dummy", "[Cut_Dummy]" ) {

    REQUIRE_NOTHROW(Cut_Dummy());

    double rcut2b = 6.2;
    double rcut2bsq = rcut2b*rcut2b;
    using Cut = Cut_Dummy;
    std::string cuttype = "Cut_Dummy";
    Cut_Base *c2b = new Cut( rcut2b );

    REQUIRE( c2b->label() == cuttype );

    REQUIRE( c2b->calc(rcut2b) < std::numeric_limits<double>::min() );
    REQUIRE( c2b->calc_prime(rcut2b) < std::numeric_limits<double>::min() );
    REQUIRE( std::abs(c2b->get_rcut()-rcut2b)<std::numeric_limits<double>::min() );
    REQUIRE( std::abs(c2b->get_rcut_sq()-rcut2bsq)<std::numeric_limits<double>::min() );

    // cutoff cannot be negative
    double temp = -0.1;
    REQUIRE_THROWS(Cut( temp ));
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

    delete c2b;
}
TEST_CASE( "Testing Cutoffs: Cut_Cos", "[Cut_Cos]" ) {

    REQUIRE_NOTHROW(Cut_Cos());
    double rcut2b = 6.2;
    double rcut2bsq = rcut2b*rcut2b;
    using Cut = Cut_Cos;
    std::string cuttype = "Cut_Cos";
    Cut_Base *c2b = new Cut( rcut2b );

    REQUIRE( c2b->label() == cuttype );

    REQUIRE( c2b->calc(rcut2b) < std::numeric_limits<double>::min() );
    REQUIRE( c2b->calc(0) == 1);
    REQUIRE( c2b->calc_prime(rcut2b) < std::numeric_limits<double>::min() );
    REQUIRE( std::abs(c2b->get_rcut()-rcut2b)<std::numeric_limits<double>::min() );
    REQUIRE( std::abs(c2b->get_rcut_sq()-rcut2bsq)<std::numeric_limits<double>::min() );

  REQUIRE_THAT(c2b->calc(2.0),
               Catch::Matchers::WithinAbs(0.76448200516348, 1e-12));
  REQUIRE_THAT(c2b->calc_prime(2.0),
               Catch::Matchers::WithinAbs(-0.21500762619812, 1e-12));


    // cutoff cannot be negative
    double temp = -0.1;
    REQUIRE_THROWS(Cut( temp ));
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

    delete c2b;
}
TEST_CASE( "Testing Cutoffs: Cut_Tanh", "[Cut_Tanh]" ) {
    REQUIRE_NOTHROW(Cut_Tanh());

    double rcut2b = 6.2;
    double rcut2bsq = rcut2b*rcut2b;
    using Cut = Cut_Tanh;
    std::string cuttype = "Cut_Tanh";
    Cut_Base *c2b = new Cut( rcut2b );

    REQUIRE( c2b->label() == cuttype );


    REQUIRE( c2b->calc(rcut2b) < std::numeric_limits<double>::min() );
    REQUIRE( c2b->calc_prime(rcut2b) < std::numeric_limits<double>::min() );
    REQUIRE( std::abs(c2b->get_rcut()-rcut2b)<std::numeric_limits<double>::min() );
    REQUIRE( std::abs(c2b->get_rcut_sq()-rcut2bsq)<std::numeric_limits<double>::min() );
  REQUIRE_THAT(c2b->calc(2.0),
               Catch::Matchers::WithinAbs(0.20521106432925, 1e-12));
  REQUIRE_THAT(c2b->calc_prime(2.0),
               Catch::Matchers::WithinAbs(-0.1097751763050810, 1e-12));

    // cutoff cannot be negative
    double temp = -0.1;
    REQUIRE_THROWS(Cut( temp ));
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

    delete c2b;
}
TEST_CASE( "Testing Cutoffs: Cut_PolyS", "[Cut_PolyS]" ) {

    REQUIRE_NOTHROW(Cut_PolyS());
    double rcut2b = 6.2;
    double rcut2bsq = rcut2b*rcut2b;
    using Cut = Cut_PolyS;
    std::string cuttype = "Cut_PolyS";
    Cut_Base *c2b = new Cut( rcut2b );

    REQUIRE( c2b->label() == cuttype );

    REQUIRE( c2b->calc(rcut2b) < std::numeric_limits<double>::min() );
    REQUIRE( c2b->calc_prime(rcut2b) < std::numeric_limits<double>::min() );
    REQUIRE( std::abs(c2b->get_rcut()-rcut2b)<std::numeric_limits<double>::min() );
    REQUIRE( std::abs(c2b->get_rcut_sq()-rcut2bsq)<std::numeric_limits<double>::min() );

    // cutoff cannot be negative
    double temp = -0.1;
    REQUIRE_THROWS(Cut( temp ));
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

  REQUIRE_THAT(c2b->calc(2.0),
               Catch::Matchers::WithinAbs(1, 1e-12));
  REQUIRE_THAT(c2b->calc_prime(2.0),
               Catch::Matchers::WithinAbs(0, 1e-12));

  REQUIRE_THAT(c2b->calc(3.0),
               Catch::Matchers::WithinAbs(0.31744, 1e-12));
  REQUIRE_THAT(c2b->calc_prime(3.0),
               Catch::Matchers::WithinAbs(-1.728, 1e-12));
    delete c2b;
}
TEST_CASE( "Testing Cutoffs: Cut_CosS", "[Cut_CosS]" ) {

    REQUIRE_NOTHROW(Cut_CosS());
    double rcut2b = 6.2;
    double rcut2bsq = rcut2b*rcut2b;
    using Cut = Cut_CosS;
    std::string cuttype = "Cut_CosS";
    Cut_Base *c2b = new Cut( rcut2b );

    REQUIRE( c2b->label() == cuttype );

    REQUIRE( c2b->calc(rcut2b) < std::numeric_limits<double>::min() );
    REQUIRE( c2b->calc_prime(rcut2b) < std::numeric_limits<double>::min() );
    REQUIRE( std::abs(c2b->get_rcut()-rcut2b)<std::numeric_limits<double>::min() );
    REQUIRE( std::abs(c2b->get_rcut_sq()-rcut2bsq)<std::numeric_limits<double>::min() );

    // cutoff cannot be negative
    double temp = -0.1;
    REQUIRE_THROWS(Cut( temp ));
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

  REQUIRE_THAT(c2b->calc(2.0),
               Catch::Matchers::WithinAbs(1, 1e-12));
  REQUIRE_THAT(c2b->calc_prime(2.0),
               Catch::Matchers::WithinAbs(0, 1e-12));

  REQUIRE_THAT(c2b->calc(3.0),
               Catch::Matchers::WithinAbs(0.3454915028, 1e-10));
  REQUIRE_THAT(c2b->calc_prime(3.0),
               Catch::Matchers::WithinAbs(-1.4939160824, 1e-10));
    delete c2b;
}
TEST_CASE( "Testing Cutoffs: Cut_PT", "[Cut_PT]" ) {

    REQUIRE_NOTHROW(Cut_PT());
    double rcut2b = 6.2;
    double rcut2bsq = rcut2b*rcut2b;
    using Cut = Cut_PT;
    std::string cuttype = "Cut_PT";
    Cut_Base *c2b = new Cut( rcut2b );

    REQUIRE( c2b->label() == cuttype );

    REQUIRE( c2b->calc(rcut2b) < std::numeric_limits<double>::min() );
    REQUIRE( c2b->calc_prime(rcut2b) < std::numeric_limits<double>::min() );
    REQUIRE( std::abs(c2b->get_rcut()-rcut2b)<std::numeric_limits<double>::min() );
    REQUIRE( std::abs(c2b->get_rcut_sq()-rcut2bsq)<std::numeric_limits<double>::min() );

    // cutoff cannot be negative
    double temp = -0.1;
    REQUIRE_THROWS(Cut( temp ));
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

  REQUIRE_THAT(c2b->calc(2.0),
               Catch::Matchers::WithinAbs(0.2475729061984162, 1e-12));
  REQUIRE_THAT(c2b->calc_prime(2.0),
               Catch::Matchers::WithinAbs(-0.4020924339864764, 1e-12));

  REQUIRE_THAT(c2b->calc(3.0),
               Catch::Matchers::WithinAbs(0.0005734995495457, 1e-10));
  REQUIRE_THAT(c2b->calc_prime(3.0),
               Catch::Matchers::WithinAbs(-0.0122699895740221, 1e-10));
    delete c2b;
}

/* Pins: f(0) and f(r_cut) of every cutoff are the values of the
 * documentation's cutoff table (Tadah.MLIP docs, cutoffs.rst): 1 and 0 for
 * the spatial tapers, tanh(1)^3 (about 0.442) and 0 for Cut_Tanh (the
 * documented normalisation), 1 and 0 for the normalised Cut_PT, and 0 and
 * 1 for the inverse envelope Cut_SinInv. The documentation was corrected,
 * not the code: this pins the code to it (no test checked f(0) before). */
TEST_CASE("Cutoff values at the origin and the radius are those of the "
          "cutoff table",
          "[cutoffs][unified-dm]") {
  const double rc = 5.0;
  const double tanh3 = std::pow(std::tanh(1.0), 3);
  const struct { const char *name; double f0, frc; } table[] = {
      {"Cut_Cos", 1, 0},   {"Cut_CosS", 1, 0}, {"Cut_Poly", 1, 0},
      {"Cut_PolyS", 1, 0}, {"Cut_PT", 1, 0},   {"Cut_Tanh", tanh3, 0},
      {"Cut_Dummy", 1, 0}, {"Cut_SinInv", 0, 1}};
  for (const auto &row : table) {
    CAPTURE(row.name);
    double r = rc;
    std::unique_ptr<Cut_Base> c(
        tadah::core::factory<Cut_Base, double &>(row.name, r));
    REQUIRE(c);
    CHECK(c->calc(0.0) == Approx(row.f0).margin(1e-14));
    CHECK(c->calc(rc) == Approx(row.frc).margin(1e-14));
  }
}
