// test_function_mparams.cpp
//
// Function_Base::mparams(): the MPARAMS values each kernel and basis
// function reads, in order, whether a value is an integer, and whether a
// searched value must be positive. tadah hpo checks OPTIM MPARAMS against
// it at startup.

#include "catch2/catch.hpp"
#include <tadah/models/functions/basis_functions/bf_linear.h>
#include <tadah/models/functions/basis_functions/bf_polynomial2.h>
#include <tadah/models/functions/kernels/kern_linear.h>
#include <tadah/models/functions/kernels/kern_lq.h>
#include <tadah/models/functions/kernels/kern_polynomial.h>
#include <tadah/models/functions/kernels/kern_quadratic.h>
#include <tadah/models/functions/kernels/kern_rbf.h>
#include <tadah/models/functions/kernels/kern_sigmoid.h>

#include <string>
#include <vector>

using namespace tadah::models;

namespace {

std::string layout(const Function_Base &f) {
  std::string s;
  for (const MParamSlot &p : f.mparams()) {
    if (!s.empty()) s += " ";
    s += p.name;
    if (p.integer) s += ":int";
    if (p.positive) s += ":>0";
  }
  return s;
}

}  // namespace

/* Pins: mparams() of every function: Kern_RBF "sigma", real and positive;
 * Kern_Polynomial "d" (integer, positive), "gamma", "c"; Kern_Sigmoid
 * "gamma", "c"; none for Kern_Linear, Kern_Quadratic, Kern_LQ, BF_Linear
 * and BF_Polynomial2 (they read no MPARAMS).
 * Before the fix: does not compile (mparams() and MParamSlot are new);
 * nothing stated which MPARAMS a function reads, and tadah hpo accepted
 * OPTIM MPARAMS on a function that reads none (the search changed
 * nothing) and on the integer degree (truncated). */
TEST_CASE("Function_Base::mparams lists the MPARAMS each function reads",
          "[functions][mparams]") {
  CHECK(layout(Kern_RBF()) == "sigma:>0");
  CHECK(layout(Kern_Polynomial()) == "d:int:>0 gamma c");
  CHECK(layout(Kern_Sigmoid()) == "gamma c");
  CHECK(layout(Kern_Linear()).empty());
  CHECK(layout(Kern_Quadratic()).empty());
  CHECK(layout(Kern_LQ()).empty());
  CHECK(layout(BF_Linear()).empty());
  CHECK(layout(BF_Polynomial2()).empty());
}
