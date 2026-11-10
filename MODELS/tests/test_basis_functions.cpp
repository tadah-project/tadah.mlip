#include "catch2/catch.hpp"
#include <iomanip>
#include <limits>
#include <stdexcept>
#include <tadah/core/registry.h>
#include <tadah/models/functions/f_all.h>
#include <string>

using namespace tadah::core;
using namespace tadah::models;

//template <typename W, typename B,
//typename F, double(F::*func)(W&, B&)>
//double central_difference() {
////double central_difference(F *p, W& w, B&b, double h=1e-12) {
//    //return (p->func(w*(b+h))-p->func(w*(b-h)))/(2.0*h);
//}

TEST_CASE( "Testing BF_Linear", "[basis_functions]" ) {
  BF_Linear bf;
  aed_type w(2);
  w.random();
  aed_type aed(2);
  aed.random();
  fd_type fd(2);
  fd.random();
  double epred = w*aed;
  //std::cout << bf.epredict(w,aed) <<std::endl;
  //std::cout << epred <<std::endl;
  REQUIRE(epred==bf.epredict(w,aed));
  REQUIRE_THAT(epred, Catch::Matchers::WithinRel(bf.epredict(w,aed)));

  double fx = -1*(w*fd(0));
  double fy = -1*(w*fd(1));
  double fz = -1*(w*fd(2));
  //central_difference<
  //  aed_type, aed_type,BF_Linear, &BF_Linear::epredict>
  //  ();
  force_type f = bf.fpredict(w,fd,aed);
  REQUIRE_THAT(fx, Catch::Matchers::WithinRel(bf.fpredict(w,fd,aed,0)));
  REQUIRE_THAT(fy, Catch::Matchers::WithinRel(bf.fpredict(w,fd,aed,1)));
  REQUIRE_THAT(fz, Catch::Matchers::WithinRel(bf.fpredict(w,fd,aed,2)));

  //std::cout<< std::setprecision(12) << fx << std::endl;
  //std::cout<< std::setprecision(12) << bf.fpredict(w,fd,aed,0) << std::endl;
  //std::cout<< std::setprecision(12) << f(0) << std::endl;
  //std::cout<< std::setprecision(12) << f << std::endl;
  //std::cout << "---"  << std::endl;
  REQUIRE_THAT(fx, Catch::Matchers::WithinRel(f(0)));
  REQUIRE_THAT(fy, Catch::Matchers::WithinRel(f(1)));
  REQUIRE_THAT(fz, Catch::Matchers::WithinRel(f(2)));
}
TEST_CASE( "Testing BF_Polynomial2", "[basis_functions]" ) {
  BF_Polynomial2 bf;
  aed_type w(3);
  w.random(12345);
  aed_type aed(2);
  aed.random(12345);
  fd_type fd(2);
  fd.random(12345);
  double epred = 100145490585.3138427734;

  REQUIRE(epred==bf.epredict(w,aed));
  REQUIRE_THAT(epred, Catch::Matchers::WithinRel(bf.epredict(w,aed),1e-10));

  double fx = -1349445733748.7724609; 
  double fy =   1034198604505.706665;
  double fz =  2212718923559.9160156;
  force_type f = bf.fpredict(w,fd,aed);

  REQUIRE_THAT(fx, Catch::Matchers::WithinRel(bf.fpredict(w,fd,aed,0),1e-10));
  REQUIRE_THAT(fy, Catch::Matchers::WithinRel(bf.fpredict(w,fd,aed,1),1e-10));
  REQUIRE_THAT(fz, Catch::Matchers::WithinRel(bf.fpredict(w,fd,aed,2),1e-10));

  REQUIRE_THAT(fx, Catch::Matchers::WithinRel(f(0),1e-10));
  REQUIRE_THAT(fy, Catch::Matchers::WithinRel(f(1),1e-10));
  REQUIRE_THAT(fz, Catch::Matchers::WithinRel(f(2),1e-10));
}
