// test_fixed_weights.cpp
//
// FIXINDEX/FIXWEIGHT coverage for M_BLR_Train / M_KRR_Train: partial fixing
// (non-prefix, unsorted index sets), the all-fixed scatter, and the
// lone-key/out-of-bounds error paths. The partial-fix reference is the
// conditional least-squares solution computed directly on the free columns
// with the fixed contribution subtracted from the target.

#include "catch2/catch.hpp"
#include <tadah/models/m_blr_train.h>
#include <tadah/models/m_krr_train.h>
#include <tadah/models/functions/basis_functions/bf_linear.h>
#include <tadah/models/functions/kernels/kern_linear.h>
#include <tadah/models/functions/kernels/kern_rbf.h>
#include <tadah/models/ols.h>
#include <tadah/core/context.h>
#include <tadah/core/core_types.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

using namespace tadah::core;
using namespace tadah::models;

namespace {

// Deterministic, well-conditioned 8x4 design matrix.
phi_type make_phi() {
  phi_type Phi(8, 4);
  double v = 0.17;
  for (size_t j = 0; j < Phi.cols(); ++j)
    for (size_t i = 0; i < Phi.rows(); ++i) {
      v = std::fmod(v * 1.618 + 0.31 * static_cast<double>(j + 1), 1.0) - 0.5;
      Phi(i, j) = v + (i == j ? 2.0 : 0.0);  // diagonal boost: full rank
    }
  return Phi;
}

t_type make_target(const phi_type &Phi) {
  t_type w_true(Phi.cols());
  for (size_t j = 0; j < Phi.cols(); ++j)
    w_true[j] = 0.5 * static_cast<double>(j + 1) * (j % 2 ? -1.0 : 1.0);
  t_type T = Phi * w_true;
  for (size_t i = 0; i < T.size(); ++i)  // perturb so fixing changes the fit
    T[i] += 0.01 * static_cast<double>(i % 3);
  return T;
}

Context make_ctx() {
  Context c;
  c.add("VERBOSE", "0");
  c.add("LAMBDA", "0");
  c.add("OALGO", "1");  // GELSD, rcond 1e-8 (same route the trainer takes)
  return c;
}

// Conditional LS reference: OLS on the free columns of Phi against
// T - Phi_fixed * w_fixed. fix_idx is 1-based, paired with w_fix.
t_type reference_free_weights(const phi_type &Phi, const t_type &T,
                              const std::vector<size_t> &fix_idx,
                              const std::vector<double> &w_fix) {
  std::vector<size_t> free_cols;
  for (size_t j = 0; j < Phi.cols(); ++j) {
    bool fixed = false;
    for (size_t k : fix_idx) fixed |= (k - 1 == j);
    if (!fixed) free_cols.push_back(j);
  }
  phi_type Phi_r(Phi.rows(), free_cols.size());
  for (size_t j = 0; j < free_cols.size(); ++j)
    for (size_t i = 0; i < Phi.rows(); ++i)
      Phi_r(i, j) = Phi(i, free_cols[j]);
  t_type T_r = T;
  for (size_t k = 0; k < fix_idx.size(); ++k)
    for (size_t i = 0; i < Phi.rows(); ++i)
      T_r[i] -= Phi(i, fix_idx[k] - 1) * w_fix[k];

  t_type w(free_cols.size());
  OLS ols;
  ols.solve(Phi_r, T_r, w, 1e-8, Algorithm::GELSD);
  return w;
}

}  // namespace

TEST_CASE("BLR partial fix: single non-prefix index", "[fixed_weights]") {
  // Fixing a column that is NOT a prefix of {1..k} exercises the
  // move_map-based reassembly; before the two-scatter fix this read w_f out
  // of bounds and produced garbage in the fixed slot.
  Context c = make_ctx();
  c.add("FIXINDEX", "2");
  c.add("FIXWEIGHT", "0.7");

  const phi_type Phi0 = make_phi();
  const t_type T0 = make_target(Phi0);

  BF_Linear bf;
  M_BLR_Train<BF_Linear> m(bf, c);
  phi_type Phi = Phi0;
  t_type T = T0;
  m.train(Phi, T);

  const t_type &w = m.get_weights();
  REQUIRE(w.size() == 4);
  REQUIRE(w[1] == 0.7);  // exact: fixed value must be placed verbatim

  const t_type wref = reference_free_weights(Phi0, T0, {2}, {0.7});
  REQUIRE(std::abs(w[0] - wref[0]) < 1e-10);
  REQUIRE(std::abs(w[2] - wref[1]) < 1e-10);
  REQUIRE(std::abs(w[3] - wref[2]) < 1e-10);
}

TEST_CASE("BLR partial fix: unsorted non-prefix pair", "[fixed_weights]") {
  // FIXINDEX order defines the FIXWEIGHT pairing: 4 -> 0.3, 1 -> -1.1.
  Context c = make_ctx();
  c.add("FIXINDEX", "4,1");
  c.add("FIXWEIGHT", "0.3");
  c.add("FIXWEIGHT", "-1.1");

  const phi_type Phi0 = make_phi();
  const t_type T0 = make_target(Phi0);

  BF_Linear bf;
  M_BLR_Train<BF_Linear> m(bf, c);
  phi_type Phi = Phi0;
  t_type T = T0;
  m.train(Phi, T);

  const t_type &w = m.get_weights();
  REQUIRE(w.size() == 4);
  REQUIRE(w[3] == 0.3);
  REQUIRE(w[0] == -1.1);

  const t_type wref = reference_free_weights(Phi0, T0, {4, 1}, {0.3, -1.1});
  REQUIRE(std::abs(w[1] - wref[0]) < 1e-10);
  REQUIRE(std::abs(w[2] - wref[1]) < 1e-10);
}

TEST_CASE("BLR all fixed: permuted scatter, no LS solve", "[fixed_weights]") {
  Context c = make_ctx();
  c.add("FIXINDEX", "3,1,2");
  c.add("FIXWEIGHT", "10.0");
  c.add("FIXWEIGHT", "20.0");
  c.add("FIXWEIGHT", "30.0");

  phi_type Phi(8, 3);
  for (size_t j = 0; j < Phi.cols(); ++j)
    for (size_t i = 0; i < Phi.rows(); ++i)
      Phi(i, j) = static_cast<double>(i + j + 1);
  t_type T(8);
  for (size_t i = 0; i < T.size(); ++i) T[i] = 1.0;

  BF_Linear bf;
  M_BLR_Train<BF_Linear> m(bf, c);
  m.train(Phi, T);

  const t_type &w = m.get_weights();
  REQUIRE(w.size() == 3);
  REQUIRE(w[2] == 10.0);  // FIXINDEX[0] = 3
  REQUIRE(w[0] == 20.0);  // FIXINDEX[1] = 1
  REQUIRE(w[1] == 30.0);  // FIXINDEX[2] = 2
  REQUIRE(m.is_trained());
}

TEST_CASE("BLR partial fix: prediction consistency", "[fixed_weights]") {
  // Phi0 * w must equal Phi_free * w_free + Phi_fixed * w_fixed; this holds
  // iff every weight landed in its original column slot.
  Context c = make_ctx();
  c.add("FIXINDEX", "3");
  c.add("FIXWEIGHT", "0.25");

  const phi_type Phi0 = make_phi();
  const t_type T0 = make_target(Phi0);

  BF_Linear bf;
  M_BLR_Train<BF_Linear> m(bf, c);
  phi_type Phi = Phi0;
  t_type T = T0;
  m.train(Phi, T);

  const t_type &w = m.get_weights();
  const t_type wref = reference_free_weights(Phi0, T0, {3}, {0.25});
  const t_type pred = Phi0 * w;
  for (size_t i = 0; i < pred.size(); ++i) {
    double expected = Phi0(i, 2) * 0.25;
    expected += Phi0(i, 0) * wref[0] + Phi0(i, 1) * wref[1] +
                Phi0(i, 3) * wref[2];
    REQUIRE(std::abs(pred[i] - expected) < 1e-10);
  }
}

TEST_CASE("BLR lone FIXINDEX/FIXWEIGHT throws", "[fixed_weights]") {
  const phi_type Phi0 = make_phi();
  const t_type T0 = make_target(Phi0);
  BF_Linear bf;

  SECTION("lone FIXINDEX") {
    Context c = make_ctx();
    c.add("FIXINDEX", "2");
    M_BLR_Train<BF_Linear> m(bf, c);
    phi_type Phi = Phi0;
    t_type T = T0;
    REQUIRE_THROWS_WITH(m.train(Phi, T), Catch::Contains("requires"));
  }
  SECTION("lone FIXWEIGHT") {
    Context c = make_ctx();
    c.add("FIXWEIGHT", "0.7");
    M_BLR_Train<BF_Linear> m(bf, c);
    phi_type Phi = Phi0;
    t_type T = T0;
    REQUIRE_THROWS_WITH(m.train(Phi, T), Catch::Contains("requires"));
  }
}

/* Pins: FIXINDEX 7 on the 8 x 4 design matrix is refused by M_BLR_Train
 * and M_KRR_Train with the message of core::parse_fixindex, "FIXINDEX:
 * index 7 is outside 1-4 (the model has 4 weights)".
 * Before the fix: both said "FIXINDEX: Indices out of bounds: valid range
 * is from 1 to 4" (this case, then for M_BLR_Train alone, checked "out of
 * bounds"), so the expectation fails; the new text is the one every site
 * gives. */
TEST_CASE("BLR FIXINDEX out of bounds throws", "[fixed_weights]") {
  const std::string bound =
      "FIXINDEX: index 7 is outside 1-4 (the model has 4 weights)";
  phi_type Phi = make_phi();
  t_type T = make_target(Phi);
  SECTION("M_BLR_Train") {
    Context c = make_ctx();
    c.add("FIXINDEX", "7");
    c.add("FIXWEIGHT", "0.7");
    BF_Linear bf;
    M_BLR_Train<BF_Linear> m(bf, c);
    REQUIRE_THROWS_WITH(m.train(Phi, T), bound);
  }
  SECTION("M_KRR_Train") {
    Context c = make_ctx();
    c.add("FIXINDEX", "7");
    c.add("FIXWEIGHT", "0.7");
    Kern_Linear kern;
    M_KRR_Train<Kern_Linear> m(kern, c);
    REQUIRE_THROWS_WITH(m.train(Phi, T), bound);
  }
}

namespace {

/* The weights M_BLR_Train<BF_Linear> (krr false) or
 * M_KRR_Train<Kern_Linear> (krr true) fit on the 8 x 4 design matrix with
 * FIXINDEX `fixindex` (split on whitespace into its values, as a
 * configuration line gives them) and the FIXWEIGHT values `fixweight`. */
t_type fit_with_fixindex(bool krr, const std::string &fixindex,
                         const std::vector<double> &fixweight) {
  Context c = make_ctx();
  c.add("FIXINDEX", fixindex);
  for (double v : fixweight) c.add("FIXWEIGHT", v);
  phi_type Phi = make_phi();
  t_type T = make_target(Phi);
  if (krr) {
    Kern_Linear kern;
    M_KRR_Train<Kern_Linear> m(kern, c);
    m.train(Phi, T);
    return m.get_weights();
  }
  BF_Linear bf;
  M_BLR_Train<BF_Linear> m(bf, c);
  m.train(Phi, T);
  return m.get_weights();
}

std::string refusal_with_fixindex(bool krr, const std::string &fixindex,
                                  const std::vector<double> &fixweight) {
  try {
    (void)fit_with_fixindex(krr, fixindex, fixweight);
  } catch (const std::runtime_error &e) {
    return e.what();
  }
  return "<no exception>";
}

}  // namespace

/* Pins: FIXINDEX given as separate values ('FIXINDEX 4 1', the values '4'
 * and '1', and 'FIXINDEX 4, 1', the values '4,' and '1') pins what the
 * comma list 'FIXINDEX 4,1' pins, for M_BLR_Train and M_KRR_Train: the
 * same four weights, bit for bit, weight 4 = 0.3 and weight 1 = -1.1.
 * Before the fix: the trainers read FIXINDEX with the lenient
 * core::parse_indices, which joined the values into '4 1', dropped it as
 * one malformed segment ("Error: Non-numeric value or incorrect syntax
 * found: 4 1" on stderr) and threw "FIXWEIGHT and FIXINDEX differ in size:
 * 2 and 0" ('4, 1' was read, its joined text '4, 1' being a comma list). */
TEST_CASE("BLR and KRR FIXINDEX given as separate values pins what the "
          "comma list pins",
          "[fixed_weights][fixindex]") {
  for (int krr = 0; krr < 2; ++krr) DYNAMIC_SECTION(
      (krr ? "M_KRR_Train<Kern_Linear>" : "M_BLR_Train<BF_Linear>")) {
    const t_type ref = fit_with_fixindex(krr, "4,1", {0.3, -1.1});
    REQUIRE(ref.size() == 4);
    CHECK(ref[3] == 0.3);
    CHECK(ref[0] == -1.1);
    for (const char *fi : {"4 1", "4, 1"}) {
      INFO("FIXINDEX " << fi);
      t_type w;
      REQUIRE_NOTHROW(w = fit_with_fixindex(krr, fi, {0.3, -1.1}));
      REQUIRE(w.size() == 4);
      for (size_t j = 0; j < 4; ++j) CHECK(w[j] == ref[j]);
    }
  }
}

/* Pins: M_BLR_Train and M_KRR_Train refuse a FIXINDEX that names a weight
 * twice or holds a descending range, with core::parse_fixindex's message:
 *  - '2,2' with FIXWEIGHT 0.7 0.7, and with FIXWEIGHT 0.7 (one value for
 *    the one weight, FIXWEIGHT pairing by position): "FIXINDEX 2,2 names
 *    weight 2 twice";
 *  - '3-2' with FIXWEIGHT 0.1 0.2: "FIXINDEX: index '3-2' is a descending
 *    range".
 * Before the fix: '2,2' was deduplicated to weight 2 with a stderr warning,
 * so with one FIXWEIGHT value the fit ran (weight 2 = 0.7) and with two it
 * threw "FIXWEIGHT and FIXINDEX differ in size: 2 and 1"; '3-2' expanded to
 * nothing ("differ in size: 2 and 0"). After core::parse_fixindex first
 * read FIXINDEX, '2,2' with one value was refused as "FIXINDEX 2,2 has 2
 * indices but FIXWEIGHT has 1 value" (this case pinned that): it compared
 * the count before it looked for a repeat. */
TEST_CASE("BLR and KRR FIXINDEX naming a weight twice or a descending range "
          "is refused",
          "[fixed_weights][fixindex]") {
  for (int krr = 0; krr < 2; ++krr) DYNAMIC_SECTION(
      (krr ? "M_KRR_Train<Kern_Linear>" : "M_BLR_Train<BF_Linear>")) {
    CHECK_THAT(refusal_with_fixindex(krr, "2,2", {0.7, 0.7}),
               Catch::Contains("FIXINDEX 2,2 names coefficient 2 twice"));
    CHECK_THAT(refusal_with_fixindex(krr, "2,2", {0.7}),
               Catch::Contains("FIXINDEX 2,2 names coefficient 2 twice"));
    CHECK_THAT(refusal_with_fixindex(krr, "3-2", {0.1, 0.2}),
               Catch::Contains("FIXINDEX: index '3-2' is a descending range"));
  }
}

/* Pins: M_KRR_Train::train2, the solve of MODEL M_KRR <kernel> 2 ((K +
 * LAMBDA sigma^2 I) w = t on the basis vectors, sigma = ESIGMA in eV,
 * every weight at once), refuses a context with FIXINDEX or FIXWEIGHT, for
 * Kern_Linear and Kern_RBF (MPARAMS 1.0), on four basis vectors: "MODEL
 * M_KRR Kern_Linear 2 cannot pin weights: ..." stating that solve and
 * naming implementation 1, for FIXINDEX 1 / FIXWEIGHT 14,
 * FIXINDEX 2-1 with two values and a lone FIXINDEX 1 alike (the solve pins
 * nothing, whatever the keys say); without them it fits its weights: 4 for
 * Kern_RBF, one per basis vector, and 8 for Kern_Linear, one per component
 * of the 8-component basis vectors (the descriptor weights of its basis
 * vectors; it had the 4 coefficients until the potential-content fix,
 * test_krr_train2_linear.cpp).
 * Before the fix: train2 read neither key: it fitted every weight with
 * the pins ignored (weight 1 was not 14), and '2-1' and a lone FIXINDEX
 * passed too (tadah train exited 0 and copied FIXINDEX and FIXWEIGHT into
 * the potential); then the refusal wrote the solve (K + LAMBDA I) w = t,
 * without the error scale that train2 applies. */
TEST_CASE("M_KRR implementation 2 (train2) refuses FIXINDEX and FIXWEIGHT",
          "[fixed_weights][fixindex][krr]") {
  for (const char *kern : {"Kern_Linear", "Kern_RBF"}) {
    auto train2 = [&](const std::string &fixindex,
                      const std::vector<double> &fixweight) {
      Context c = make_ctx();
      c.remove("LAMBDA");
      c.add("LAMBDA", "1e-3");
      c.add("MPARAMS", "1.0");
      if (!fixindex.empty()) c.add("FIXINDEX", fixindex);
      for (double v : fixweight) c.add("FIXWEIGHT", v);
      // Four basis vectors (the columns of the 8x4 matrix), one target
      // value each.
      Matrix B = make_phi();
      t_type T(B.cols());
      for (size_t j = 0; j < T.size(); ++j)
        T[j] = 0.3 * static_cast<double>(j + 1);
      if (std::string(kern) == "Kern_Linear") {
        Kern_Linear k;
        M_KRR_Train<Kern_Linear> m(k, c);
        m.train2(B, T);
        return m.get_weights();
      }
      Kern_RBF k(c);
      M_KRR_Train<Kern_RBF> m(k, c);
      m.train2(B, T);
      return m.get_weights();
    };
    DYNAMIC_SECTION(kern) {
      const std::string refusal =
          std::string("MODEL M_KRR ") + kern + " 2 cannot pin weights: "
          "implementation 2 solves (K + LAMBDA sigma^2 I) w = t, sigma = "
          "ESIGMA in eV, for the weights of all its basis vectors at once, "
          "so FIXINDEX and FIXWEIGHT would be ignored. Implementation 1 "
          "(MODEL M_KRR " + kern + " 1) pins them; implementation 2 fits "
          "without FIXINDEX and FIXWEIGHT.";
      t_type w;
      REQUIRE_NOTHROW(w = train2("", {}));
      CHECK(w.size() == (std::string(kern) == "Kern_Linear" ? 8u : 4u));
      CHECK_THROWS_WITH(train2("1", {14.0}), refusal);
      CHECK_THROWS_WITH(train2("2-1", {14.0, 3.0}), refusal);
      CHECK_THROWS_WITH(train2("1", {}), refusal);
    }
  }
}

TEST_CASE("KRR linear partial fix places fixed values", "[fixed_weights]") {
  // Locks current KRR behaviour with Kern_Linear (no EKM projection):
  // fixed columns are zeroed for the solve and the fixed values written
  // back by index; free weights match the conditional LS reference.
  Context c = make_ctx();
  c.add("FIXINDEX", "4,1");
  c.add("FIXWEIGHT", "0.3");
  c.add("FIXWEIGHT", "-1.1");

  const phi_type Phi0 = make_phi();
  const t_type T0 = make_target(Phi0);

  Kern_Linear kern;
  M_KRR_Train<Kern_Linear> m(kern, c);
  phi_type Phi = Phi0;
  t_type T = T0;
  m.train(Phi, T);

  const t_type &w = m.get_weights();
  REQUIRE(w.size() == 4);
  REQUIRE(w[3] == 0.3);
  REQUIRE(w[0] == -1.1);

  const t_type wref = reference_free_weights(Phi0, T0, {4, 1}, {0.3, -1.1});
  REQUIRE(std::abs(w[1] - wref[0]) < 1e-8);
  REQUIRE(std::abs(w[2] - wref[1]) < 1e-8);
}

/* Pins: with FIXINDEX 2 / FIXWEIGHT 0.7, OALGO 3 (SVD) and LAMBDA 0.1 on
 * the 8 x 4 design matrix (with the 4 ridge-augmentation rows a design
 * matrix appends, zero for OALGO 3), M_BLR_Train<BF_Linear> and
 * M_KRR_Train<Kern_Linear> pin weight 2 at 0.7, and the free weights (1, 3,
 * 4) are those of the same fit on the free columns alone (the 8 rows and 3
 * augmentation rows: the same 8 observations; 1e-12 relative); the Context
 * holds no SIGMA.
 * Before the fix (WCOV and WNOISE): passes as well for the weights (the
 * case pinned the covariance of every weight, zero for the pinned one).
 * Before WCOV and WNOISE: the Context held SIGMA, 4 then the 16 values
 * (this case checked it), and the matrices of 8 rows had no augmentation
 * rows. */
TEST_CASE("BLR and KRR with FIXINDEX and OALGO 3: the free weights of the "
          "fit on the free columns",
          "[fixed_weights][sigma]") {
  const phi_type Phi8 = make_phi();
  const t_type T8 = make_target(Phi8);
  // The design matrix of a fit: the 8 rows, then 4 augmentation rows.
  phi_type Phi0(12, 4);
  Phi0.set_zero();
  t_type T0(12);
  for (size_t i = 0; i < 12; ++i) T0[i] = i < 8 ? T8[i] : 0.0;
  for (size_t j = 0; j < 4; ++j)
    for (size_t i = 0; i < 8; ++i) Phi0(i, j) = Phi8(i, j);
  auto oalgo3 = [] {
    Context c;
    c.add("VERBOSE", "0");
    c.add("LAMBDA", "0.1");
    c.add("OALGO", "3");
    return c;
  };
  // The reference: the same fit on the free columns 0, 2, 3 (8 rows and 3
  // augmentation rows).
  phi_type Phi_r(11, 3);
  Phi_r.set_zero();
  t_type T_r(11);
  const size_t free_cols[3] = {0, 2, 3};
  for (size_t j = 0; j < 3; ++j)
    for (size_t i = 0; i < 8; ++i) Phi_r(i, j) = Phi8(i, free_cols[j]);
  for (size_t i = 0; i < 11; ++i)
    T_r[i] = i < 8 ? T8[i] - Phi8(i, 1) * 0.7 : 0.0;
  Context cr = oalgo3();
  BF_Linear bfr;
  M_BLR_Train<BF_Linear> ref(bfr, cr);
  ref.train(Phi_r, T_r);
  const t_type wref = ref.get_weights();
  REQUIRE(wref.size() == 3);

  for (int krr = 0; krr < 2; ++krr) DYNAMIC_SECTION(
      (krr ? "M_KRR_Train<Kern_Linear>" : "M_BLR_Train<BF_Linear>")) {
    Context c = oalgo3();
    c.add("FIXINDEX", "2");
    c.add("FIXWEIGHT", "0.7");
    phi_type Phi = Phi0;
    t_type T = T0;
    t_type w;
    if (krr) {
      Kern_Linear k;
      M_KRR_Train<Kern_Linear> m(k, c);
      m.train(Phi, T);
      w = m.get_weights();
    } else {
      BF_Linear bf;
      M_BLR_Train<BF_Linear> m(bf, c);
      m.train(Phi, T);
      w = m.get_weights();
    }
    REQUIRE(w.size() == 4);
    CHECK(w[1] == 0.7);
    for (size_t a = 0; a < 3; ++a)
      CHECK(std::abs(w[free_cols[a]] - wref[a]) <
            1e-12 * std::abs(wref[a]));
    CHECK_FALSE(c.has_stored("SIGMA"));
  }
}
