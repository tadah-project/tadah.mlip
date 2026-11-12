// test_kernel_mparams.cpp
//
// The kernels read their parameters from MPARAMS when they are built:
// Kern_RBF sigma (gamma = 1/sigma^2), Kern_Polynomial the degree d, gamma
// and c, Kern_Sigmoid gamma and c. The degree is an integer, and a value
// that is not one is refused rather than truncated; Kern_RBF refuses a
// sigma of 0, which defines no kernel. A negative sigma defines the kernel
// of |sigma|, and potentials with one keep loading.

#include "catch2/catch.hpp"
#include <tadah/core/context.h>
#include <tadah/core/core_types.h>
#include <tadah/models/functions/kernels/kern_polynomial.h>
#include <tadah/models/functions/kernels/kern_rbf.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>

using namespace tadah::core;
using namespace tadah::models;

namespace {

std::uint64_t kbits(double v) {
  std::uint64_t u;
  std::memcpy(&u, &v, sizeof u);
  return u;
}

Context mparams_context(const std::string &mparams) {
  Context c;
  c.add("VERBOSE", "0");
  c.add("MPARAMS", mparams);
  return c;
}

aed_type vec2(double a, double b) {
  aed_type v(2);
  v(0) = a;
  v(1) = b;
  return v;
}

template <class K> std::string construction_error(const std::string &mparams) {
  try {
    Context c = mparams_context(mparams);
    K k(c);
  } catch (const std::exception &e) {
    return e.what();
  }
  return "";
}

}  // namespace

/* Pins: Kern_Polynomial reads MPARAMS (1), its degree d, as an integer: the
 * spellings "2", "2.0" and "2e0" of the degree 2 build the kernel
 * (gamma b.x + c)^2 (b.x = 1.1, gamma = 0.5, c = 0.3: K equals
 * std::pow(0.85, 2) bit for bit), and "2.7" is refused, naming MPARAMS and
 * the value 2.7.
 * Before the fix: "2.7" built the kernel of degree 2 (the integer prefix
 * of the text): a potential that records MPARAMS 2.7 predicted as one of
 * degree 2, and an HPO search over the degree scored truncated values. */
TEST_CASE("Kern_Polynomial reads its degree from MPARAMS as an integer",
          "[kernels][mparams]") {
  const aed_type b = vec2(1.0, 0.5);
  const aed_type x = vec2(0.6, 1.0);   // b.x = 1.1
  for (const char *d : {"2", "2.0", "2e0"}) {
    INFO("degree " << d);
    Context c = mparams_context(std::string(d) + " 0.5 0.3");
    Kern_Polynomial k(c);
    CHECK(kbits(k(b, x)) == kbits(std::pow(0.5 * 1.1 + 0.3, 2)));
  }
  const std::string msg = construction_error<Kern_Polynomial>("2.7 0.5 0.3");
  INFO("message: " << msg);
  CHECK(msg.find("MPARAMS") != std::string::npos);
  CHECK(msg.find("2.7") != std::string::npos);
}

/* Pins: Kern_RBF refuses sigma 0 (MPARAMS 0, -0 and 0.0), naming Kern_RBF,
 * MPARAMS and sigma; a negative sigma builds the kernel of |sigma| (MPARAMS
 * -0.5 gives K bit for bit equal to MPARAMS 0.5, gamma = 1/sigma^2), so a
 * potential that records one loads as before.
 * Before the fix: sigma 0 built a kernel with gamma = inf (K(b, x) = 0 for
 * b != x and nan for b = x), without an error; the negative sigma: passes
 * as well (a pin of the behaviour the refusal keeps). */
TEST_CASE("Kern_RBF refuses sigma 0 and evaluates a negative sigma as its "
          "magnitude",
          "[kernels][mparams]") {
  for (const char *s : {"0", "-0", "0.0"}) {
    const std::string msg = construction_error<Kern_RBF>(s);
    INFO("MPARAMS " << s << ": " << msg);
    CHECK(msg.find("Kern_RBF") != std::string::npos);
    CHECK(msg.find("MPARAMS") != std::string::npos);
    CHECK(msg.find("sigma") != std::string::npos);
  }
  Context cn = mparams_context("-0.5");
  Context cp = mparams_context("0.5");
  Kern_RBF kn(cn);
  Kern_RBF kp(cp);
  const aed_type b = vec2(1.0, 0.5);
  const aed_type x = vec2(0.6, 1.0);
  CHECK(kbits(kn(b, x)) == kbits(kp(b, x)));
  CHECK(kbits(kn(b, b)) == kbits(1.0));
}
