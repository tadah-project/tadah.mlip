#include "catch2/catch.hpp"
#include <tadah/models/descriptors/d2/d2_all.h>
#include <tadah/core/maths.h>
#include <tadah/core/periodic_table.h>
#include <tadah/models/cutoffs.h>

using namespace tadah::core;
using namespace tadah::models;
template<typename D, typename C>
void test_d2(Context &c, double r) {
  // std::streambuf *orig = std::cout.rdbuf();
  std::stringstream ss;
  // std::cout.rdbuf (ss.rdbuf()); // redirect cout
  c.add("TYPE2B","H");
  c.add("TYPE2B","H");
  std::cout << c << std::endl;
  D d(c);
  // std::cout.rdbuf (orig); // restore
  double rcut = c.get<double>("RCUT2B");
  // d.init_for_atoms(1,1);
  C fc(rcut);
  d.set_fcut(&fc, false);
  aed_type aed(d.size());
  d.calc_aed(1,1,r,r*r,aed);
  fd_type fd(d.size());
  d.calc_dXijdri(1,1,r,r*r,fd);
  fd_type fda(d.size());
  d.calc_fd_approx(1,1, r, fda, 1e-6);

  // Convert to std::vector, as catch2 matchers require these
  std::vector<double> fx(fd[0].data(),fd[0].data()+fd[0].size());
  std::vector<double> fy(fd[1].data(),fd[1].data()+fd[1].size());
  std::vector<double> fz(fd[2].data(),fd[2].data()+fd[2].size());

  std::vector<double> fxa(fda[0].data(),
                          fda[0].data()+fda[0].size());
  std::vector<double> fya(fda[1].data(),
                          fda[1].data()+fda[1].size());
  std::vector<double> fza(fda[2].data(),
                          fda[2].data()+fda[2].size());

  REQUIRE_THAT(fxa, Catch::Matchers::Approx(fx).margin(1e-5));
  // D2_Base::calc_fd_approx writes into its fd_type: fda(i, 0), the
  // storage the fd_type's other readers use, is its column 0. Before the
  // fix the assignment 'fd(0) = ...' gave the column a buffer of its own
  // and fda(i, 0) stayed 0.
  for (size_t i = 0; i < fxa.size(); ++i) REQUIRE(fda(i, 0) == fxa[i]);
  REQUIRE(fda[0].data() == fda.ptr());
  // y and z should be zero
  REQUIRE_THAT(fya, Catch::Matchers::Approx(fy));
  REQUIRE_THAT(fza, Catch::Matchers::Approx(fz));

  aed_type aed2(d.size());
  fd_type fd2(d.size());
  d.calc_all(1,1,r,r*r,aed2,fd2);
  std::vector<double> fx2(fd2[0].data(),fd2[0].data()+fd2[0].size());
  std::vector<double> fy2(fd2[1].data(),fd2[1].data()+fd2[1].size());
  std::vector<double> fz2(fd2[2].data(),fd2[2].data()+fd2[2].size());

  REQUIRE_THAT(fx2, Catch::Matchers::Approx(fx).epsilon(1e-12));
  REQUIRE_THAT(fy2, Catch::Matchers::Approx(fy).epsilon(1e-12));
  REQUIRE_THAT(fz2, Catch::Matchers::Approx(fz).epsilon(1e-12));
}

// `rcuts` are the cutoffs the descriptor is tested with (a descriptor whose
// cutoff is fixed, D2_EAM, passes its own).
template<typename D>
void test_D2(Context &c,
             const std::vector<double> &rcuts = {10, 2.1, 2.5, 3.5, 6.7}) {
  // std::streambuf *orig = std::cout.rdbuf();
  std::stringstream ss;
  // std::cout.rdbuf (ss.rdbuf()); // redirect cout
  c.remove("VERBOSE");
  c.add("VERBOSE", 1);
  c.add("ATOM", "H");
  c.add("WATOM", 1);
  // We cannot test r=rcut as finite difference will fail
  // but we can test r slightly smaller than rcut
  std::vector<double> rs = {2,2.0001,2.0123,5,11};

  for (const auto &r: rs) {
    for (const auto &rcut: rcuts) {
      c.remove("RCUT2B");
      c.add("RCUT2B", rcut);
      test_d2<D,Cut_Dummy>(c,r);
      test_d2<D,Cut_Cos>(c,r);
      test_d2<D,Cut_Tanh>(c,r);
      test_d2<D,Cut_PolyS>(c,r);
    }
  }
  Context c2; // INIT2B=false
  D d(c2);
  REQUIRE(d.size()==0);
  // std::cout.rdbuf (orig); // restore
}

// Fixture for initialization
struct TestFixture {
    TestFixture() {
        PeriodicTable::initialize();
    }
};

TEST_CASE_METHOD(TestFixture, "Testing D2_Dummy", "[D2]") {
    Context c;
    double rcut = 5.1;
    c.remove("INIT2B");
    c.add("INIT2B", "true");
    c.add("RCUT2B", rcut);
    test_d2<D2_Dummy, Cut_Dummy>(c, 1);

    REQUIRE_NOTHROW(D2_Dummy());
    REQUIRE(D2_Dummy().label() == "D2_Dummy");
}

TEST_CASE_METHOD(TestFixture, "Testing D2_LJ", "[D2]") {
    Context c;
    c.remove("INIT2B");
    c.add("INIT2B", "true");
    c.add("TYPE2B", "D2_LJ");
    test_D2<D2_LJ>(c);

    D2_LJ d(c);
    REQUIRE(d.size() == 2);
    REQUIRE(d.label() == "D2_LJ");
}

TEST_CASE_METHOD(TestFixture, "Testing D2_BLIP", "[D2]") {
    // std::streambuf *orig = std::cout.rdbuf();
    std::stringstream ss;
    // std::cout.rdbuf(ss.rdbuf()); // redirect cout
    using D = D2_Blip;

    Context c;
    c.remove("INIT2B");
    c.add("INIT2B", "true");
    c.add("SGRID2B", "0.01 0.4 5.6");
  c.add("CGRID2B", "0.0 1.2 3.4");
  c.add("TYPE2B", "D2_Blip");
  c.add("TYPE2B", 3);
  c.add("TYPE2B", 3);
    test_D2<D>(c);
    
    D d(c);
    REQUIRE(d.size() == 3);
    REQUIRE(d.label() == "D2_Blip");

    // Test throws
    c.remove("SGRID2B");
    c.remove("CGRID2B");
    c.add("SGRID2B", "0.4 5.6");
    c.add("CGRID2B", "0.0 1.2 3.4");
    REQUIRE_THROWS(D(c));
    
    c.remove("SGRID2B");
    c.remove("CGRID2B");
    c.add("SGRID2B", "0.4 5.6");
    c.add("CGRID2B", "0.0");
    REQUIRE_THROWS(D(c));

    c.remove("SGRID2B");
    c.remove("CGRID2B");
    c.add("SGRID2B", "0.0 0.4 5.6");
    c.add("CGRID2B", "0.0 1.2 3.4");
    REQUIRE_THROWS(D(c));
    
    // std::cout.rdbuf(orig); // restore
}

TEST_CASE_METHOD(TestFixture, "Testing D2_BP", "[D2]") {
    // std::streambuf *orig = std::cout.rdbuf();
    std::stringstream ss;
    // std::cout.rdbuf(ss.rdbuf()); // redirect cout
    using D = D2_BP;

    Context c;
    c.remove("INIT2B");
    c.add("INIT2B", "true");
    c.add("SGRID2B", "0.01 0.4");
    c.add("CGRID2B", "0.0 1.2");
  c.add("TYPE2B", "D2_BP");
  c.add("TYPE2B", 2);
  c.add("TYPE2B", 2);
    test_D2<D>(c);
    
    D d(c);
    REQUIRE(d.size() == 2);
    REQUIRE(d.label() == "D2_BP");

    // Test throws
    c.remove("SGRID2B");
    c.remove("CGRID2B");
    c.add("SGRID2B", "0.4 5.6");
    c.add("CGRID2B", "0.0 1.2 3.4");
    REQUIRE_THROWS(D(c));
    
    c.remove("SGRID2B");
    c.remove("CGRID2B");
    c.add("SGRID2B", "0.4 5.6");
    c.add("CGRID2B", "0.0");
    REQUIRE_THROWS(D(c));

    c.remove("SGRID2B");
    c.remove("CGRID2B");
    c.add("SGRID2B", "0.0 0.4");
    c.add("CGRID2B", "0.0 1.2");
    REQUIRE_THROWS(D(c));
    
    // std::cout.rdbuf(orig); // restore
}

TEST_CASE_METHOD(TestFixture, "Testing D2_EAM", "[D2]") {
    // std::streambuf *orig = std::cout.rdbuf();
    std::stringstream ss;
    // std::cout.rdbuf(ss.rdbuf()); // redirect cout
    
    // The form a configuration file gives: `TYPE2B D2_EAM 0` (the
    // placeholder parameter; test_d2 adds the element pair H H) and SETFL.
    // It used to omit the descriptor name and the parameter, so the first
    // added H stood for both. Before the fix: passes as well (a pin:
    // D2_EAM read SETFL and dropped its parameter).
    // RCUT2B is the setfl file's cutoff, the only one D2_EAM takes (it
    // refuses another: test_eam_descriptors.cpp). Before the cutoff fix
    // the harness's five cutoffs were each replaced in the context by the
    // file's, so all five runs were at 5.58190742024 as this one is.
    // Before that fix: passes as well (a pin).
    Context c;
    c.remove("INIT2B");
    c.add("INIT2B", "true");
    c.add("TYPE2B", "D2_EAM");
    c.add("TYPE2B", 0);
    c.add("SETFL", "tests_data/eam.alloy");
    test_D2<D2_EAM>(c, {5.58190742024});

    D2_EAM d(c);
    REQUIRE(d.size() == 1);
    REQUIRE(d.label() == "D2_EAM");

    // Test throws, naming the SETFL path. Before the fix: passes as well (a
    // pin: D2_EAM has always read SETFL).
    Context c_err;
    c_err.remove("INIT2B");
    c_err.add("INIT2B", "true");
    c_err.add("TYPE2B", "D2_EAM");
    c_err.add("TYPE2B", 0);
    c_err.add("TYPE2B", "H");
    c_err.add("TYPE2B", "H");
    c_err.add("SETFL", "path/to/nowhere");
    CHECK_THROWS_WITH(D2_EAM(c_err), Catch::Contains("path/to/nowhere"));
    
    // std::cout.rdbuf(orig); // restore
}

TEST_CASE_METHOD(TestFixture, "Testing D2_MIE", "[D2]") {
    Context c;
    c.remove("INIT2B");
    c.add("INIT2B", "true");
    c.add("TYPE2B", "D2_MIE");
    c.add("TYPE2B", 7);
    c.add("TYPE2B", 13);
    test_D2<D2_MIE>(c);

    D2_MIE d(c);
    REQUIRE(d.size() == 2);
    REQUIRE(d.label() == "D2_MIE");

    // Test throws
    c.remove("TYPE2B");
    c.add("TYPE2B", "D2_MIE");
    c.add("TYPE2B", 7);
    REQUIRE_THROWS(D2_MIE(c));

    c.remove("TYPE2B");
    c.add("TYPE2B", "D2_MIE");
    c.add("TYPE2B", 7);
    c.add("TYPE2B", -8);
    REQUIRE_THROWS(D2_MIE(c));

    c.remove("TYPE2B");
    c.add("TYPE2B", "D2_MIE");
    c.add("TYPE2B", -7);
    c.add("TYPE2B", 8);
    REQUIRE_THROWS(D2_MIE(c));

}
TEST_CASE_METHOD(TestFixture, "Testing D2_ZBL", "[D2]") {
    Context c;
    c.remove("INIT2B");
    c.add("INIT2B", "true");
    c.add("TYPE2B", "D2_ZBL");
    c.add("TYPE2B", -1);
    c.add("TYPE2B", -1);
    c.add("TYPE2B", -1);
    c.add("TYPE2B", -1);
    test_D2<D2_ZBL>(c);

    D2_ZBL d(c);
    REQUIRE(d.size() == 1);
    REQUIRE(d.label() == "D2_ZBL");

    // Test throws
    c.remove("TYPE2B");
    c.add("TYPE2B", "D2_ZBL");
    c.add("TYPE2B", 7);
    REQUIRE_THROWS(D2_ZBL(c));

    c.remove("TYPE2B");
    c.add("TYPE2B", "D2_ZBL");
    c.add("TYPE2B", -7);
    c.add("TYPE2B", -8);
    REQUIRE_THROWS(D2_ZBL(c));

    c.remove("TYPE2B");
    c.add("TYPE2B", "D2_ZBL");
    c.add("TYPE2B", 7);
    c.add("TYPE2B", 8);
    c.add("TYPE2B", 9);
    REQUIRE_THROWS(D2_ZBL(c));

}
