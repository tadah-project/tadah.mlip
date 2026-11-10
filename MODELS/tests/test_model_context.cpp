// test_model_context.cpp
//
// A fit writes nothing into its Context but the ALPHA and BETA of LAMBDA -1
// (LinearRegressor::train), and reads no SIGMA an earlier version of
// Tadah! wrote there (a deprecated key a program may still add); a model
// built from a Context with WEIGHTS (a potential used as the configuration)
// can be trained, an M_KRR model as an M_BLR one.

#include "catch2/catch.hpp"
#include <tadah/core/context.h>
#include <tadah/core/core_types.h>
#include <tadah/models/functions/basis_functions/bf_linear.h>
#include <tadah/models/functions/kernels/kern_linear.h>
#include <tadah/models/linear_regressor.h>
#include <tadah/models/m_blr_train.h>
#include <tadah/models/m_krr_train.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

using namespace tadah::core;
using namespace tadah::models;

namespace {

std::uint64_t bits(double v) {
  std::uint64_t u;
  std::memcpy(&u, &v, sizeof u);
  return u;
}

// Deterministic, well-conditioned 8x4 design matrix (as test_fixed_weights).
phi_type make_phi() {
  phi_type Phi(8, 4);
  double v = 0.17;
  for (size_t j = 0; j < Phi.cols(); ++j)
    for (size_t i = 0; i < Phi.rows(); ++i) {
      v = std::fmod(v * 1.618 + 0.31 * static_cast<double>(j + 1), 1.0) - 0.5;
      Phi(i, j) = v + (i == j ? 2.0 : 0.0);
    }
  return Phi;
}

t_type make_target(const phi_type &Phi) {
  t_type w_true(Phi.cols());
  for (size_t j = 0; j < Phi.cols(); ++j)
    w_true[j] = 0.5 * static_cast<double>(j + 1) * (j % 2 ? -1.0 : 1.0);
  t_type T = Phi * w_true;
  for (size_t i = 0; i < T.size(); ++i) T[i] += 0.01 * static_cast<double>(i % 3);
  return T;
}

/* make_phi() as the DesignMatrix gives it to a fit: its 8 rows, then one
 * ridge-augmentation row per column (zero for OALGO 3), so that the 8 rows
 * are the observations. */
phi_type make_phi_aug() {
  const phi_type Phi8 = make_phi();
  phi_type Phi(Phi8.rows() + Phi8.cols(), Phi8.cols());
  Phi.set_zero();
  for (size_t j = 0; j < Phi8.cols(); ++j)
    for (size_t i = 0; i < Phi8.rows(); ++i) Phi(i, j) = Phi8(i, j);
  return Phi;
}

t_type make_target_aug() {
  const t_type T8 = make_target(make_phi());
  t_type T(T8.size() + 4);
  for (size_t i = 0; i < T.size(); ++i) T[i] = i < T8.size() ? T8[i] : 0.0;
  return T;
}

Context make_ctx(const std::string &oalgo) {
  Context c;
  c.add("VERBOSE", "0");
  c.add("LAMBDA", "0");
  c.add("OALGO", oalgo);
  return c;
}

/* A SIGMA as an earlier OALGO 3 fit of a 4-weight model leaves it: N = 4
 * and 16 values. */
void add_earlier_sigma(Context &c) {
  c.add("SIGMA", "4");
  for (int k = 0; k < 16; ++k) c.add("SIGMA", 0.25 * (k + 1));
}

}  // namespace

/* Pins: two OALGO 3 fits (LinearRegressor::train) on one Context (that of
 * a model trained twice, or a potential used as the configuration) write
 * nothing into it (no SIGMA) and give the same weights, bit for bit.
 * Before the fix (WCOV and WNOISE): each fit wrote SIGMA into the Context,
 * N = 4 and the 16 values (config_add_sigma replaced the earlier SIGMA;
 * before fix/hpo-trainer-reuse it appended to it, 34 values after the
 * second fit). */
TEST_CASE("OALGO 3: LinearRegressor::train writes nothing into the Context",
          "[sigma]") {
  Context c = make_ctx("3");
  const phi_type Phi0 = make_phi_aug();
  const t_type T0 = make_target_aug();
  t_type w[2];
  for (int fit = 0; fit < 2; ++fit) {
    phi_type Phi = Phi0;
    t_type T = T0;
    LinearRegressor::train(c, Phi, T, w[fit]);
    INFO("fit " << fit);
    CHECK_FALSE(c.has_stored("SIGMA"));
  }
  REQUIRE(w[0].size() == 4);
  REQUIRE(w[1].size() == 4);
  for (size_t j = 0; j < 4; ++j) CHECK(bits(w[1][j]) == bits(w[0][j]));
}

/* Pins: M_BLR_Train<BF_Linear> and M_KRR_Train<Kern_Linear> fit whatever
 * SIGMA the Context carried from an earlier fit (the 17 values of a
 * 4-weight OALGO 3 fit): with OALGO 1 and 3, without FIXINDEX, with
 * FIXINDEX 2 and with FIXINDEX 1-4 (every weight pinned, nothing solved),
 * the fit gives 4 weights, the FIXWEIGHT values when every weight is
 * pinned. The Context's SIGMA is not read.
 * Before the fix (WCOV and WNOISE): passes as well for the weights (the
 * case pinned the covariance each fit held, and get_weights_uncertainty()
 * tested LAMBDA, not the fit). */
TEST_CASE("A fit reads no SIGMA of its Context",
          "[sigma]") {
  struct Pins {
    const char *name;
    const char *fixindex;   // nullptr: no FIXINDEX
    std::vector<double> fixweight;
  };
  const Pins pins[] = {{"no FIXINDEX", nullptr, {}},
                       {"FIXINDEX 2", "2", {0.7}},
                       {"FIXINDEX 1-4", "1-4", {0.1, 0.2, 0.3, 0.4}}};
  for (const char *oalgo : {"1", "3"}) {
    for (const Pins &p : pins) {
      for (const char *model : {"M_BLR BF_Linear", "M_KRR Kern_Linear"}) {
        const bool all_pinned = p.fixindex && std::string(p.fixindex) == "1-4";
        DYNAMIC_SECTION(model << ", OALGO " << oalgo << ", " << p.name) {
          Context c = make_ctx(oalgo);
          if (p.fixindex) {
            c.add("FIXINDEX", p.fixindex);
            for (double w : p.fixweight) c.add("FIXWEIGHT", w);
          }
          add_earlier_sigma(c);
          phi_type Phi = make_phi_aug();
          t_type T = make_target_aug();
          t_type w;
          auto fit = [&](auto &m) {
            REQUIRE_NOTHROW(m.train(Phi, T));
            w = m.get_weights();
          };
          if (std::string(model) == "M_BLR BF_Linear") {
            BF_Linear bf;
            M_BLR_Train<BF_Linear> m(bf, c);
            fit(m);
          } else {
            Kern_Linear k;
            M_KRR_Train<Kern_Linear> m(k, c);
            fit(m);
          }
          REQUIRE(w.size() == 4);
          if (all_pinned)
            for (size_t i = 0; i < 4; ++i) CHECK(w[i] == p.fixweight[i]);
          if (p.fixindex && !all_pinned) CHECK(w[1] == 0.7);
        }
      }
    }
  }
}

/* Pins: an M_KRR model built from a Context that has WEIGHTS (a potential
 * used as the configuration, `tadah train -c pot.tadah`, or an HPO -c
 * potential) holds those weights but is not trained, and train() fits it:
 * the weights are those of the same fit on a Context without WEIGHTS, bit
 * for bit. The same for M_BLR (which already behaved so).
 * Before the fix: M_KRR_Core::init set trained when WEIGHTS were present,
 * so train() threw "This object is already trained!". */
TEST_CASE("An M_KRR model built with WEIGHTS can be trained",
          "[krr][weights]") {
  const phi_type Phi0 = make_phi();
  const t_type T0 = make_target(Phi0);
  t_type w_ref;
  {
    Context c = make_ctx("1");
    Kern_Linear k;
    M_KRR_Train<Kern_Linear> m(k, c);
    phi_type Phi = Phi0;
    t_type T = T0;
    m.train(Phi, T);
    w_ref = m.get_weights();
  }
  REQUIRE(w_ref.size() == 4);
  for (const char *model : {"M_KRR Kern_Linear", "M_BLR BF_Linear"}) {
    DYNAMIC_SECTION(model) {
      Context c = make_ctx("1");
      c.add("WEIGHTS", "9 8 7 6");
      phi_type Phi = Phi0;
      t_type T = T0;
      t_type w;
      bool trained_before = true;
      std::string msg;
      auto fit = [&](auto &m) {
        trained_before = m.is_trained();
        CHECK(m.get_weights().size() == 4);
        try {
          m.train(Phi, T);
        } catch (const std::exception &e) {
          msg = e.what();
        }
        w = m.get_weights();
      };
      if (std::string(model) == "M_KRR Kern_Linear") {
        Kern_Linear k;
        M_KRR_Train<Kern_Linear> m(k, c);
        fit(m);
      } else {
        BF_Linear bf;
        M_BLR_Train<BF_Linear> m(bf, c);
        fit(m);
      }
      INFO("train: " << msg);
      CHECK_FALSE(trained_before);
      CHECK(msg.empty());
      REQUIRE(w.size() == 4);
      for (size_t i = 0; i < 4; ++i) CHECK(bits(w[i]) == bits(w_ref[i]));
    }
  }
}
