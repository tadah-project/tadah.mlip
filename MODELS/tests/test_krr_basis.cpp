// test_krr_basis.cpp
//
// The basis of a non-linear KRR kernel: how a potential's SBASIS/BASIS are
// read (Kern_Base::read_basis_from_context), how a KRR model with WEIGHTS
// checks that its basis is there and holds one vector per weight
// (M_KRR_Core), and the empirical kernel map (EKM), which keeps the basis
// it is given and has no state between calls.
//
// Before the fix: a potential without BASIS loaded with an empty basis and
// predicted E = 0; a BASIS without SBASIS was ignored; SBASIS 0 divided by
// zero; a BASIS length that is not a multiple of SBASIS was written past
// the end of a vector; EKM::configure dropped basis vectors with
// K(b, b) <= 0 from the caller's matrix and appended the indices of every
// call to a member.

#include "catch2/catch.hpp"

#include <tadah/core/context.h>
#include <tadah/core/core_types.h>
#include <tadah/models/svd.h>  // ekm.h used SVD without including it
#include <tadah/models/ekm.h>
#include <tadah/models/functions/kernels/kern_all.h>
#include <tadah/models/m_krr_train.h>

#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>

using namespace tadah::core;
using namespace tadah::models;

namespace {

/* The message of the exception f() throws, "" when it throws none. */
template <typename F>
std::string thrown(F f) {
  try {
    f();
  } catch (const std::exception &e) {
    return e.what();
  }
  return "";
}

bool contains(const std::string &s, const std::string &part) {
  return s.find(part) != std::string::npos;
}

/* A Kern_RBF context (sigma 1) with the given SBASIS, BASIS and WEIGHTS
 * lines ("" leaves a key out). */
Context rbf_context(const std::string &sbasis, const std::string &basis,
                    const std::string &weights) {
  Context c;
  c.add("MPARAMS", 1.0);
  c.add("VERBOSE", 0);
  if (!sbasis.empty()) c.add("SBASIS", sbasis);
  if (!basis.empty()) c.add("BASIS", basis);
  if (!weights.empty()) c.add("WEIGHTS", weights);
  return c;
}

}  // namespace

/* Pins: Kern_Base::read_basis_from_context refuses a BASIS that does not
 * come with an SBASIS dividing it, naming the keys: BASIS without SBASIS
 * ("BASIS without SBASIS"), a BASIS of 5 values with SBASIS 2 ("not a
 * multiple of SBASIS 2"), and SBASIS 0 with a BASIS ("SBASIS 0"). SBASIS
 * without BASIS (a training configuration) leaves the basis empty, and
 * SBASIS 2 with 6 values reads two vectors of 3 (pins).
 * Before the fix: BASIS without SBASIS was ignored (no throw, empty
 * basis); 5 values with SBASIS 2 read 2 vectors of 2 values after writing
 * 5 values into a vector of 4 (no throw); SBASIS 0 divided by zero
 * (SIGFPE, this section runs last). */
TEST_CASE("Kern_Base reads BASIS only with an SBASIS that divides it",
          "[kernels][krr_basis]") {
  SECTION("SBASIS without BASIS: no basis (a pin)") {
    const Context c = rbf_context("4", "", "");
    Kern_RBF k(c);
    CHECK(k.get_basis().cols() == 0);
  }
  SECTION("SBASIS 2 with 6 values: two vectors of 3 (a pin)") {
    const Context c = rbf_context("2", "1 2 3 4 5 6", "");
    Kern_RBF k(c);
    REQUIRE(k.get_basis().cols() == 2);
    REQUIRE(k.get_basis().rows() == 3);
    CHECK(k.get_basis()(2, 1) == 6.0);
  }
  SECTION("BASIS without SBASIS") {
    const Context c = rbf_context("", "1 2 3 4 5 6", "");
    const std::string msg = thrown([&] { Kern_RBF k(c); });
    INFO("message: " << msg);
    CHECK(contains(msg, "BASIS without SBASIS"));
  }
  SECTION("5 BASIS values with SBASIS 2") {
    const Context c = rbf_context("2", "1 2 3 4 5", "");
    const std::string msg = thrown([&] { Kern_RBF k(c); });
    INFO("message: " << msg);
    CHECK(contains(msg, "BASIS has 5 values"));
    CHECK(contains(msg, "not a multiple of SBASIS 2"));
  }
  SECTION("SBASIS 0 with a BASIS") {
    const Context c = rbf_context("0", "1 2 3 4 5 6", "");
    const std::string msg = thrown([&] { Kern_RBF k(c); });
    INFO("message: " << msg);
    CHECK(contains(msg, "SBASIS 0"));
  }
}

/* Pins: a KRR model of a non-linear kernel (M_KRR_Train<Kern_RBF>) built
 * from a context with WEIGHTS refuses it, naming the keys, when there is
 * no basis ("WEIGHTS but no basis vectors (SBASIS and BASIS)") and when
 * the number of WEIGHTS differs from the basis vectors ("2 WEIGHTS but
 * SBASIS 3"); with 2 WEIGHTS and 2 basis vectors it predicts
 * sum_j w_j K(b_j, x) (1e-15 relative); Kern_Linear with WEIGHTS and no
 * basis loads (a pin: it has one weight per descriptor component).
 * Before the fix: both refused contexts loaded; the first predicted 0 for
 * every x, the second read past its 2 weights. */
TEST_CASE("A non-linear KRR model with WEIGHTS needs one basis vector per "
          "weight",
          "[krr_basis]") {
  SECTION("no basis") {
    Context c = rbf_context("", "", "0.5 -0.25");
    const std::string msg = thrown([&] { M_KRR_Train<Kern_RBF> m(c); });
    INFO("message: " << msg);
    CHECK(contains(msg, "WEIGHTS but no basis vectors (SBASIS and BASIS)"));
    CHECK(contains(msg, "Kern_RBF"));
  }
  SECTION("SBASIS without BASIS") {
    Context c = rbf_context("2", "", "0.5 -0.25");
    const std::string msg = thrown([&] { M_KRR_Train<Kern_RBF> m(c); });
    INFO("message: " << msg);
    CHECK(contains(msg, "WEIGHTS but no basis vectors (SBASIS and BASIS)"));
  }
  SECTION("more basis vectors than weights") {
    Context c = rbf_context("3", "1 0 0 0.5 0.2 0.1 0.3 0.6 0.2", "0.5 -0.25");
    const std::string msg = thrown([&] { M_KRR_Train<Kern_RBF> m(c); });
    INFO("message: " << msg);
    CHECK(contains(msg, "2 WEIGHTS but SBASIS 3"));
  }
  SECTION("one basis vector per weight") {
    Context c = rbf_context("2", "1 0 0 0.5 0.2 0.1", "0.5 -0.25");
    M_KRR_Train<Kern_RBF> m(c);
    aed_type x(3);
    x(0) = 0.7;
    x(1) = 0.1;
    x(2) = -0.3;
    const double k0 = std::exp(-0.5 * (0.09 + 0.01 + 0.09));
    const double k1 = std::exp(-0.5 * (0.04 + 0.01 + 0.16));
    CHECK(m.predict(x) == Approx(0.5 * k0 - 0.25 * k1).epsilon(1e-15));
  }
  SECTION("Kern_Linear needs no basis (a pin)") {
    Context c;
    c.add("VERBOSE", 0);
    c.add("WEIGHTS", "0.5 -0.25 1.0");
    M_KRR_Train<Kern_Linear> m(c);
    CHECK(m.get_weights().size() == 3);
  }
}

namespace {

/* (Q Q^T - I) entries are below 1e-12 for a square Q. */
void check_square_orthogonal(const Matrix &Q, std::size_t n) {
  REQUIRE(Q.rows() == n);
  REQUIRE(Q.cols() == n);
  for (std::size_t i = 0; i < n; ++i)
    for (std::size_t j = 0; j < n; ++j) {
      double qqt = 0;
      for (std::size_t k = 0; k < n; ++k) qqt += Q(i, k) * Q(j, k);
      INFO("(Q Q^T)(" << i << "," << j << ") = " << qqt);
      CHECK(std::fabs(qqt - (i == j ? 1.0 : 0.0)) < 1e-12);
    }
}

/* The 3 x 3 basis with columns b1 = (1, 0.5, 0.2), b2 = (0.3, 1.2, -0.4)
 * and b3 (given). */
Matrix basis_with(double x, double y, double z) {
  Matrix B(3, 3);
  B(0, 0) = 1.0;
  B(1, 0) = 0.5;
  B(2, 0) = 0.2;
  B(0, 1) = 0.3;
  B(1, 1) = 1.2;
  B(2, 1) = -0.4;
  B(0, 2) = x;
  B(1, 2) = y;
  B(2, 2) = z;
  return B;
}

}  // namespace

/* Pins: EKM<Kern_Quadratic>::configure leaves the caller's basis as it was
 * and keeps no state between calls: on the basis [b1, b2, b3] (every
 * K(b, b) > 0) two calls leave 3 columns and every value, and give the
 * same 3 x 3 EKM_mat, orthogonal to 1e-12; on [b1, b2, 0] (the zero
 * vector has K(0, 0) = 0) the basis keeps its 3 columns and EKM_mat is
 * 3 x 3 and orthogonal.
 * Before the fix: the second call on [b1, b2, b3] appended the indices of
 * the first again and replaced the basis by one of 6 columns (the last 3
 * zero); on [b1, b2, 0] configure dropped the zero vector from the
 * caller's basis (2 columns left). */
TEST_CASE("EKM::configure keeps the basis and gives a square orthogonal map, "
          "the same on every call",
          "[krr_basis][ekm]") {
  Kern_Quadratic kern;
  SECTION("two calls on a basis of non-zero vectors") {
    EKM<Kern_Quadratic> ekm(kern);
    Matrix B = basis_with(-0.7, 0.1, 0.9);
    const Matrix B0 = B;
    ekm.configure(B);
    REQUIRE(B.cols() == 3);
    check_square_orthogonal(ekm.EKM_mat, 3);
    const Matrix first = ekm.EKM_mat;
    ekm.configure(B);
    INFO("basis columns after the second call: " << B.cols());
    REQUIRE(B.cols() == 3);
    for (std::size_t i = 0; i < 3; ++i)
      for (std::size_t j = 0; j < 3; ++j) CHECK(B(i, j) == B0(i, j));
    check_square_orthogonal(ekm.EKM_mat, 3);
    for (std::size_t i = 0; i < 3; ++i)
      for (std::size_t j = 0; j < 3; ++j) CHECK(ekm.EKM_mat(i, j) == first(i, j));
  }
  SECTION("a zero basis vector") {
    EKM<Kern_Quadratic> ekm(kern);
    Matrix B = basis_with(0.0, 0.0, 0.0);
    const Matrix B0 = B;
    ekm.configure(B);
    INFO("basis columns after the call: " << B.cols());
    REQUIRE(B.cols() == 3);
    for (std::size_t i = 0; i < 3; ++i)
      for (std::size_t j = 0; j < 3; ++j) CHECK(B(i, j) == B0(i, j));
    check_square_orthogonal(ekm.EKM_mat, 3);
  }
}
