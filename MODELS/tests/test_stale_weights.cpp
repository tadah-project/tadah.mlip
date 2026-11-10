// A training configuration may carry the WEIGHTS of a potential: `tadah
// train -c pot.tadah` retrains a potential from its own file (the remedy
// for a potential whose weights no longer fit its descriptor, e.g. a
// multi-species BIAS potential trained before 1.6.0). The fit replaces
// those weights; they do not make a model "trained".

#include "catch2/catch.hpp"

#include <tadah/core/context.h>
#include <tadah/core/core_types.h>
#include <tadah/models/functions/f_all.h>
#include <tadah/models/m_blr_train.h>
#include <tadah/models/m_krr_train.h>

#include <string>

using namespace tadah::core;
using namespace tadah::models;

namespace {

/* A context with 3 stale WEIGHTS for a 2-weight least-squares problem. */
Context stale_context() {
  Context c;
  c.add("LAMBDA", 0);
  c.add("OALGO", 1);
  c.add("VERBOSE", 0);
  c.add("BIAS", "false");
  for (double w : {9.0, 9.0, 9.0}) c.add("WEIGHTS", w);
  return c;
}

/* y = 2 x0 - 3 x1 on four rows, plus two (zero) augmentation rows. */
void problem(phi_type &Phi, t_type &T) {
  const double x[4][2] = {{1, 0}, {0, 1}, {1, 1}, {2, 1}};
  Phi = phi_type(6, 2);
  Phi.set_zero();
  T = t_type(6);
  for (std::size_t i = 0; i < 6; ++i) T(i) = 0.0;
  for (std::size_t i = 0; i < 4; ++i) {
    Phi(i, 0) = x[i][0];
    Phi(i, 1) = x[i][1];
    T(i) = 2.0 * x[i][0] - 3.0 * x[i][1];
  }
}

}  // namespace

// Pins: an M_KRR_Train (Kern_Linear) and an M_BLR_Train (BF_Linear) built
// from a context that carries 3 stale WEIGHTS are not trained, train on a
// 2-column problem, and hold the fitted weights (2, -3) afterwards.
// Before the fix: M_KRR_Core marked a model whose context had WEIGHTS
// trained, so M_KRR_Train::train threw "This object is already trained!"
// (`tadah train -c <an M_KRR potential>` stopped with that error, rc 1);
// the M_BLR section passes (M_BLR_Core never did this).
TEST_CASE("A model built from a context with stale WEIGHTS is not trained "
          "and trains",
          "[stale_weights]") {
  SECTION("M_KRR_Train, Kern_Linear") {
    Context c = stale_context();
    M_KRR_Train<Kern_Linear> krr(c);
    CHECK_FALSE(krr.is_trained());
    phi_type Phi;
    t_type T;
    problem(Phi, T);
    REQUIRE_NOTHROW(krr.train(Phi, T));
    CHECK(krr.is_trained());
    REQUIRE(krr.get_weights().size() == 2);
    CHECK(krr.get_weights()(0) == Approx(2.0).epsilon(1e-12));
    CHECK(krr.get_weights()(1) == Approx(-3.0).epsilon(1e-12));
  }
  SECTION("M_BLR_Train, BF_Linear") {
    Context c = stale_context();
    M_BLR_Train<BF_Linear> blr(c);
    CHECK_FALSE(blr.is_trained());
    phi_type Phi;
    t_type T;
    problem(Phi, T);
    REQUIRE_NOTHROW(blr.train(Phi, T));
    REQUIRE(blr.get_weights().size() == 2);
    CHECK(blr.get_weights()(0) == Approx(2.0).epsilon(1e-12));
    CHECK(blr.get_weights()(1) == Approx(-3.0).epsilon(1e-12));
  }
}
