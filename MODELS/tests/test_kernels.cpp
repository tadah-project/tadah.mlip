#include "catch2/catch.hpp"
#include <tadah/models/functions/f_all.h>
#include <tadah/core/context.h>
#include <tadah/core/core_user_config_parser.h>
#include <tadah/core/core_types.h>
#include <string>

using namespace tadah::core;
using namespace tadah::models;

//template <typename W, typename B,
//typename F, double(F::*func)(W&, B&)>
//double central_difference() {
////double central_difference(F *p, W& w, B&b, double h=1e-12) {
//    //return (p->func(w*(b+h))-p->func(w*(b-h)))/(2.0*h);
//}

TEST_CASE( "Testing Kern_Base", "[kernels]" ) {
  Kern_Quadratic kern;
  Matrix basis(3,2);
  basis.random(12123133,-1e-1,1e-1);
  kern.set_basis(basis);
  Matrix basis2 = kern.get_basis();
  for(size_t i=0;i<3;++i)
    for(size_t j=0;j<2;++j)
      REQUIRE(basis(i,j)==basis2(i,j));

  REQUIRE(Kern_Linear().get_label()=="Kern_Linear");
  REQUIRE(Kern_Polynomial().get_label()=="Kern_Polynomial");
  REQUIRE(Kern_Quadratic().get_label()=="Kern_Quadratic");
  REQUIRE(Kern_LQ().get_label()=="Kern_LQ");
  REQUIRE(Kern_RBF().get_label()=="Kern_RBF");
  REQUIRE(Kern_Sigmoid().get_label()=="Kern_Sigmoid");

  aed_type w(2);
  w.random(123,-1e-1,1e-1);
  aed_type aed(3);
  aed.random(993,-1e-1,1e-1);
  fd_type fd(3);
  fd.random(753,-1e-1,1e-1);


  REQUIRE_THAT(-1.82049009349779262e-07, 
               Catch::Matchers::WithinRel(kern.epredict(w,aed)));

  force_type f(-1.98275464733636505e-06,
      -2.09764072099347695e-07,
      1.56681997348973148e-06);

  force_type fp = kern.fpredict(w,fd,aed);

  for(size_t i=0;i<3;++i) {
    REQUIRE_THAT(f(i), Catch::Matchers::WithinRel(kern.fpredict(w,fd,aed,i)));
    REQUIRE_THAT(f(i), Catch::Matchers::WithinRel(fp(i)));
  }

  Matrix matbas(3,2);
  Context cbasis = CoreUserConfigParser().parse("tests_data/basis");
  Kern_Quadratic kern2;
  kern2.read_basis_from_context(cbasis, matbas);
  Matrix test_basis(3,2);
  test_basis(0,0)=1.0;
  test_basis(1,0)=2.0;
  test_basis(2,0)=3.0;
  test_basis(0,1)=4.0;
  test_basis(1,1)=5.0;
  test_basis(2,1)=6.0;

  for(size_t i=0;i<2;++i)
    for(size_t j=0;j<3;++j) {
      REQUIRE_THAT(matbas(j,i), 
                   Catch::Matchers::WithinAbs(test_basis(j,i),1e-12));
    }
}
TEST_CASE( "Testing Kern_Linear 1", "[kernels]" ) {
  using K=Kern_Linear;
  Context c;
  REQUIRE_NOTHROW(K(c));
  K kern;
  aed_type w(4);
  w.random();
  aed_type aed(4);
  aed.random();
  fd_type fd(4);
  fd.random();
  double epred = w*aed;
  REQUIRE(epred==kern.epredict(w,aed));
  REQUIRE_THAT(epred, Catch::Matchers::WithinRel(kern.epredict(w,aed)));

  double fx = -1*(w*fd(0));
  double fy = -1*(w*fd(1));
  double fz = -1*(w*fd(2));
  force_type f = kern.fpredict(w,fd,aed);
  REQUIRE_THAT(fx, Catch::Matchers::WithinRel(kern.fpredict(w,fd,aed,0)));
  REQUIRE_THAT(fy, Catch::Matchers::WithinRel(kern.fpredict(w,fd,aed,1)));
  REQUIRE_THAT(fz, Catch::Matchers::WithinRel(kern.fpredict(w,fd,aed,2)));

  REQUIRE_THAT(fx, Catch::Matchers::WithinRel(f(0)));
  REQUIRE_THAT(fy, Catch::Matchers::WithinRel(f(1)));
  REQUIRE_THAT(fz, Catch::Matchers::WithinRel(f(2)));
}
TEST_CASE( "Testing Kern_Linear 2", "[kernels]" ) {
  // Kern_Linear and BF_Linear should give the same answer
  Kern_Linear kern;
  BF_Linear bf;
  aed_type w(2);
  w.random();
  aed_type aed(2);
  aed.random();
  fd_type fd(2);
  fd.random();
  REQUIRE_THAT(bf.epredict(w,aed), Catch::Matchers::WithinAbs(kern.epredict(w,aed),1e-12));

  force_type fkern = kern.fpredict(w,fd,aed);
  force_type fbf = bf.fpredict(w,fd,aed);
  double fx = bf.fpredict(w,fd,aed,0);
  double fy = bf.fpredict(w,fd,aed,1);
  double fz = bf.fpredict(w,fd,aed,2);
  REQUIRE_THAT(fkern[0], Catch::Matchers::WithinAbs(fbf[0],1e-12));
  REQUIRE_THAT(fkern[1], Catch::Matchers::WithinAbs(fbf[1],1e-12));
  REQUIRE_THAT(fkern[2], Catch::Matchers::WithinAbs(fbf[2],1e-12));

  REQUIRE_THAT(fx, Catch::Matchers::WithinAbs(fbf[0],1e-12));
  REQUIRE_THAT(fy, Catch::Matchers::WithinAbs(fbf[1],1e-12));
  REQUIRE_THAT(fz, Catch::Matchers::WithinAbs(fbf[2],1e-12));
}
TEST_CASE( "Testing Kern_Linear 3", "[kernels]" ) {
  using K=Kern_Linear;
  K kern;

  aed_type w(2);
  w.random(12345);
  aed_type aed(2);
  aed.random(12345);
  fd_type fd(2);
  fd.random(12345);

  // test operator()
  REQUIRE_THAT((w*aed), Catch::Matchers::WithinRel(kern(w,aed),1e-10));
  // test derivative
  REQUIRE(w.isApprox(kern.derivative(w,aed),1e-10));
  // test prime
  for (size_t k=0; k<3; ++k) {
    REQUIRE_THAT(kern.derivative(w, aed)*fd(k),
                 Catch::Matchers::WithinRel(kern.prime(w,aed,fd(k)),1e-10));
  }
}
TEST_CASE( "Testing Kern_Quadratic", "[kernels]" ) {
  using K=Kern_Quadratic;
  Context c;
  REQUIRE_NOTHROW(K(c));
  K kern;
  Matrix basis(2,10);
  basis.random();
  kern.set_basis(basis);

  aed_type w(2);
  w.random(12345);
  aed_type aed(2);
  aed.random(12345);
  fd_type fd(2);
  fd.random(12345);

  for (size_t i=0; i<basis.cols(); ++i) {
    const aed_type &b = basis.col(i);
    // test operator()
    REQUIRE_THAT((b*aed)*(b*aed), 
                 Catch::Matchers::WithinRel(kern(b,aed),1e-10));
    // test derivative
    REQUIRE((2*(b*aed)*b).
            isApprox(kern.derivative(b,aed),1e-10));
    // test prime
    for (size_t k=0; k<3; ++k) {
      REQUIRE( (2 * b * aed * fd(k) * b) == Approx(kern.prime(b, aed, fd(k))).epsilon(1e-10) );
      REQUIRE_THAT(kern.derivative(b, aed)*fd(k),
                   Catch::Matchers::WithinRel(kern.prime(b,aed,fd(k)),1e-10));
    }

  }
}
TEST_CASE( "Testing Kern_LQ", "[kernels]" ) {
  using K=Kern_LQ;
  Context c;
  REQUIRE_NOTHROW(K(c));
  K kern;
  Matrix basis(2,10);
  basis.random();
  kern.set_basis(basis);

  aed_type w(2);
  w.random(12345);
  aed_type aed(2);
  aed.random(12345);
  fd_type fd(2);
  fd.random(12345);

  for (size_t i=0; i<basis.cols(); ++i) {
    const aed_type &b = basis.col(i);
    // test operator()
    REQUIRE_THAT(((b*aed)*(b*aed))+(b*aed), 
                 Catch::Matchers::WithinRel(kern(b,aed),1e-10));
    // test derivative
    REQUIRE(((2*(b*aed)*b)+b).
            isApprox(kern.derivative(b,aed),1e-10));
    // test prime
    for (size_t k=0; k<3; ++k) {
      REQUIRE_THAT(((2*(b*aed)*b)+b)*fd(k),
                   Catch::Matchers::WithinRel(kern.prime(b,aed,fd(k)),1e-10));
    }
  }
}
TEST_CASE( "Testing Kern_RBF", "[kernels]" ) {
  std::streambuf *orig = std::cout.rdbuf();
  std::stringstream ss;
  std::cout.rdbuf (ss.rdbuf()); // redirect cout
  using K=Kern_RBF;
  Context c;
  double s=0.15273;
  double g=1.0/(2.0*s*s);
  c.add("MPARAMS", s);
  c.remove("VERBOSE");
  c.add("VERBOSE",1);

  REQUIRE_NOTHROW(K(c));
  K kern(c);
  Matrix basis(2,10);
  basis.random(123,-1e-1,1e-1);
  kern.set_basis(basis);

  aed_type w(2);
  w.random(23,-1e-1,1e-1);
  aed_type aed(2);
  aed.random(143,-1e-1,1e-1);
  fd_type fd(2);
  fd.random(133,-1e-1,1e-1);

  for (size_t i=0; i<basis.cols(); ++i) {
    const aed_type &b = basis.col(i);
    // test operator()
    REQUIRE_THAT(exp(-g*((aed-b)*(aed-b))), 
                 Catch::Matchers::WithinRel(kern(b,aed)));
    // test derivative
    REQUIRE((-2.0*g*(aed-b)*(exp(-g*((aed-b)*(aed-b))))).
            isApprox(kern.derivative(b,aed),1e-10));
    // test prime
    for (size_t k=0; k<3; ++k) {
      REQUIRE_THAT((-2.0*g*(aed-b)*(exp(-g*((aed-b)*(aed-b)))))*fd(k),
                   Catch::Matchers::WithinRel(kern.prime(b,aed,fd(k)),1e-10));
    }
  }
  std::cout.rdbuf (orig); // restore
}
TEST_CASE( "Testing Kern_Sigmoid", "[kernels]" ) {
  std::streambuf *orig = std::cout.rdbuf();
  std::stringstream ss;
  std::cout.rdbuf (ss.rdbuf()); // redirect cout
  using K=Kern_Sigmoid;
  Context c;
  double g=1.123;
  double s=0.15273;
  c.add("MPARAMS", g);
  c.add("MPARAMS", s);
  c.remove("VERBOSE");
  c.add("VERBOSE",1);

  REQUIRE_NOTHROW(K(c));
  K kern(c);
  Matrix basis(2,10);
  basis.random(123,-1e-1,1e-1);
  kern.set_basis(basis);

  aed_type w(2);
  w.random(23,-1e-1,1e-1);
  aed_type aed(2);
  aed.random(143,-1e-1,1e-1);
  fd_type fd(2);
  fd.random(133,-1e-1,1e-1);

  for (size_t i=0; i<basis.cols(); ++i) {
    const aed_type &b = basis.col(i);
    // test operator()
    REQUIRE_THAT(std::tanh(g*(b*aed)+s), Catch::Matchers::WithinRel(kern(b,aed)));
    // test derivative
    REQUIRE(( g*(1.0-std::pow(( std::tanh(g*(b*aed)+s)  ),2))*b ).
            isApprox(kern.derivative(b,aed),1e-10));
    // test prime
    for (size_t k=0; k<3; ++k) {
      REQUIRE_THAT((  g*(1.0-std::pow(( std::tanh(g*(b*aed)+s)  ),2))*b   )*fd(k),
                   Catch::Matchers::WithinRel(kern.prime(b,aed,fd(k)),1e-10));
    }
  }
  std::cout.rdbuf (orig); // restore
}
TEST_CASE( "Testing Kern_Polynomial", "[kernels]" ) {
  std::streambuf *orig = std::cout.rdbuf();
  std::stringstream ss;
  std::cout.rdbuf (ss.rdbuf()); // redirect cout
  using K=Kern_Polynomial;
  Context c;
  int d=3;
  double g=1.123;
  double s=0.15273;
  c.add("MPARAMS", d);
  c.add("MPARAMS", g);
  c.add("MPARAMS", s);
  c.remove("VERBOSE");
  c.add("VERBOSE",1);

  REQUIRE_NOTHROW(K(c));
  K kern(c);
  Matrix basis(2,10); // 10 basis vectors
  basis.random(123,-1e-1,1e-1);
  kern.set_basis(basis);

  aed_type w(2);
  w.random(23,-1e-1,1e-1);
  aed_type aed(2);
  aed.random(143,-1e-1,1e-1);
  fd_type fd(2);
  fd.random(133,-1e-1,1e-1);

  for (size_t i=0; i<basis.cols(); ++i) {
    const aed_type &b = basis.col(i);
    // test operator()
    REQUIRE_THAT(std::pow(g*(b*aed)+s,d), Catch::Matchers::WithinRel(kern(b,aed)));
    // test derivative
    REQUIRE(( d*g*std::pow(g*(b*aed)+s,d-1)*b).
            isApprox(kern.derivative(b,aed),1e-10));
    // test prime
    for (size_t k=0; k<3; ++k) {
      REQUIRE_THAT(( d*g*std::pow(g*(b*aed)+s,d-1)*b)*fd(k),
                   Catch::Matchers::WithinRel(kern.prime(b,aed,fd(k)),1e-10));
    }
  }
  std::cout.rdbuf (orig); // restore
}
