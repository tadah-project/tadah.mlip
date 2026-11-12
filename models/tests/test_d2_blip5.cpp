// test_d2_blip5.cpp
//
// Quintic B-spline bump (M6, Blip5): primitive math audits (peak value,
// FD parity, C^4 continuity at every knot), the D2_Blip5 descriptor
// (factory dispatch, value/force FD, dtheta parity) and DM_REAM bcode 3.

#include "catch2/catch.hpp"

#include <tadah/core/context.h>
#include <tadah/core/core_types.h>
#include <tadah/core/periodic_table.h>
#include <tadah/core/registry.h>
#include <tadah/models/cutoffs.h>
#include <tadah/models/descriptors/d2/d2_blip5.h>
#include <tadah/models/descriptors/d2/d2_base.h>
#include <tadah/models/descriptors/dm/dm_ream.h>
#include <tadah/models/descriptors/d_basis_functions.h>

#include <cmath>
#include <sstream>
#include <string>
#include <vector>

namespace {

using tadah::core::aed_type;
using tadah::core::fd_type;
using tadah::core::Context;
using tadah::core::PeriodicTable;
using tadah::models::Cut_Cos;
using tadah::models::D2_Blip5;
using tadah::models::DM_REAM;
using namespace tadah::models;

struct Blip5Fixture {
  Blip5Fixture() { PeriodicTable::initialize(); }
};

Context make_blip5_context(const std::vector<double>& cgrid,
                           const std::vector<double>& sgrid, double rcut) {
  Context c;
  c.add("ATOM", "H");
  c.add("WATOM", 1);
  c.add("INIT2B", "true");
  c.add("VERBOSE", 0);
  {
    std::ostringstream ss;
    ss.precision(17);
    for (std::size_t i = 0; i < sgrid.size(); ++i)
      ss << (i ? " " : "") << sgrid[i];
    c.add("SGRID2B", ss.str());
  }
  {
    std::ostringstream ss;
    ss.precision(17);
    for (std::size_t i = 0; i < cgrid.size(); ++i)
      ss << (i ? " " : "") << cgrid[i];
    c.add("CGRID2B", ss.str());
  }
  c.add("RCUT2B", rcut);
  c.add("TYPE2B", "D2_Blip5");
  c.add("TYPE2B", 2);
  c.add("TYPE2B", 2);
  c.add("TYPE2B", "H");
  c.add("TYPE2B", "H");
  return c;
}

}  // namespace

TEST_CASE_METHOD(Blip5Fixture, "B5 primitives: peak, FD parity, C4 knots",
                 "[blip5]") {
  // peak-normalised M6: B5(0) = 1; support edge
  REQUIRE(B5(0.0) == Approx(1.0));
  REQUIRE(B5(3.0) == 0.0);
  REQUIRE(B5(-3.0) == 0.0);
  REQUIRE(B5(2.999) > 0.0);
  // symmetry
  REQUIRE(B5(1.3) == Approx(B5(-1.3)));
  REQUIRE(d2B5(0.0) == Approx(-20.0 / 11.0));  // survey closed form

  // Fused value/derivative helper is exactly the two original primitives.
  for (double x : {-3.0, -2.7, -2.0, -1.5, -1.0, -0.4, 0.0,
                    0.6, 1.0, 1.2, 2.0, 2.3, 3.0}) {
    double value = 0.0, deriv = 0.0;
    B5_value_deriv(x, 1.7, value, deriv);
    REQUIRE(value == B5(x));
    REQUIRE(deriv == dB5(x, 1.7));
  }

  // derivative convention: dB5(x_c, eta) == eta * d/dx_c B5
  const double h = 1e-6;
  for (double x : {-2.7, -1.5, -0.4, 0.0, 0.6, 1.2, 2.3}) {
    for (double eta : {1.0, 1.7}) {
      const double fd = eta * (B5(x + h) - B5(x - h)) / (2 * h);
      REQUIRE(dB5(x, eta) == Approx(fd).margin(1e-7));
    }
    const double fd2 = (dB5(x + h, 1.0) - dB5(x - h, 1.0)) / (2 * h);
    REQUIRE(d2B5(x) == Approx(fd2).margin(1e-5));
  }

  // C^4: B5, B5', B5'' one-sided limits agree at EVERY knot {0,1,2,3}
  const double e = 1e-9;
  for (double k : {0.0, 1.0, 2.0, 3.0}) {
    REQUIRE(B5(k - e) == Approx(B5(k + e)).margin(1e-8));
    REQUIRE(dB5(k - e, 1.0) == Approx(dB5(k + e, 1.0)).margin(1e-8));
    REQUIRE(d2B5(k - e) == Approx(d2B5(k + e)).margin(1e-7));
  }
}

TEST_CASE_METHOD(Blip5Fixture, "D2_Blip5: factory + value + force FD",
                 "[blip5]") {
  const double rcut = 6.0;
  const std::vector<double> cgrid = {2.0, 3.0, 4.0, 5.0};
  const std::vector<double> sgrid = {1.1, 0.9, 1.0, 1.2};
  Context c = make_blip5_context(cgrid, sgrid, rcut);

  using tadah::core::factory;
  auto* d_ptr = factory<tadah::models::D2_Base, const Context&>("D2_Blip5", c);
  REQUIRE(d_ptr != nullptr);
  REQUIRE(d_ptr->label() == "D2_Blip5");
  delete d_ptr;

  D2_Blip5 d(c);
  Cut_Cos fc(rcut);
  d.set_fcut(&fc, false);

  const double rij = 3.3;
  aed_type aed(d.size());
  aed.set_zero();
  d.calc_aed(1, 1, rij, rij * rij, aed);
  for (std::size_t k = 0; k < cgrid.size(); ++k) {
    const double expect = B5(sgrid[k] * (rij - cgrid[k])) * fc.calc(rij);
    REQUIRE(aed(k) == Approx(expect).margin(1e-12));
  }

  fd_type fd(d.size());
  fd.set_zero();
  d.calc_dXijdri(1, 1, rij, rij * rij, fd);
  const double h = 1e-6;
  aed_type ap(d.size()), am(d.size());
  ap.set_zero(); am.set_zero();
  d.calc_aed(1, 1, rij + h, (rij + h) * (rij + h), ap);
  d.calc_aed(1, 1, rij - h, (rij - h) * (rij - h), am);
  for (std::size_t k = 0; k < cgrid.size(); ++k)
    REQUIRE(fd(k, 0) == Approx((ap(k) - am(k)) / (2 * h)).margin(1e-6));

  // The combined path used by collapse-table construction matches the
  // separate descriptor calls exactly, including accumulation semantics.
  aed_type both_aed(d.size());
  fd_type both_fd(d.size());
  both_aed.set_zero();
  both_fd.set_zero();
  d.calc_all(1, 1, rij, rij * rij, both_aed, both_fd);
  for (std::size_t k = 0; k < cgrid.size(); ++k) {
    REQUIRE(both_aed(k) == aed(k));
    REQUIRE(both_fd(k, 0) == fd(k, 0));
  }
}

TEST_CASE_METHOD(Blip5Fixture, "DM_REAM Blip5 (bcode 3): symmetric channels",
                 "[blip5]") {
  Context c;
  c.add("VERBOSE", 0);
  c.add("INITMB", "true");
  c.add("RCUTMB", 6.0);
  c.add("RCTYPEMB", "Cut_Cos");
  c.add("TYPEMB", "DM_REAM");
  c.add("TYPEMB", 3);          // Blip5
  c.add("TYPEMB", 3); c.add("TYPEMB", 3); c.add("TYPEMB", 3);
  c.add("TYPEMB", 3); c.add("TYPEMB", 3);
  c.add("TYPEMB", 0); c.add("TYPEMB", 0);
  c.add("TYPEMB", "H");
  c.add("TYPEMB", "H");
  c.add("CGRIDMB", "2.0 3.5 5.0");
  c.add("SGRIDMB", "1.0 1.0 1.0");
  c.add("AMPGRIDMB",    "0.40 0.25 0.12");
  c.add("RHOFORM", "SIGNED");
  c.add("CEMBFUNC", "0.0 0.5 1.0");
  c.add("SEMBFUNC", "1.5 1.5 1.5");
  DM_REAM dm(c);
  Cut_Cos fc(6.0);
  dm.set_fcut(&fc, false);

  const double r = 3.0;
  tadah::core::rho_type rhoi;
  dm.init_rhoi(rhoi);
  tadah::core::Vec3d del(r, 0.0, 0.0);
  dm.calc_rho_mb(1, 1, del, rhoi);
  const double expect_rho = 0.40 * B5(r - 2.0, fc.calc(r)) +
                            0.25 * B5(r - 3.5, fc.calc(r)) +
                            0.12 * B5(r - 5.0, fc.calc(r));
  REQUIRE(rhoi(0) == Approx(expect_rho).margin(1e-12));

  aed_type aed(dm.size());
  aed.set_zero();
  dm.calc_aed(rhoi, aed);
  const double rho_i = rhoi(0);
  // EMBF0 ZERO (default): zero-referenced embedding g(rho) - g(0).
  REQUIRE(aed(0) == Approx(B5(1.5 * (rho_i - 0.0)) - B5(1.5 * (0.0 - 0.0))).margin(1e-12));
  REQUIRE(aed(1) == Approx(B5(1.5 * (rho_i - 0.5)) - B5(1.5 * (0.0 - 0.5))).margin(1e-12));
  REQUIRE(aed(2) == Approx(B5(1.5 * (rho_i - 1.0)) - B5(1.5 * (0.0 - 1.0))).margin(1e-12));

  // EMBF0 FREE: the raw Blip5 basis of tadah 1.5.0.
  c.add("EMBF0", "FREE");
  DM_REAM dm_free(c);
  dm_free.set_fcut(&fc, false);
  aed_type aed_free(dm_free.size());
  aed_free.set_zero();
  dm_free.calc_aed(rhoi, aed_free);
  REQUIRE(aed_free(0) == Approx(B5(1.5 * (rho_i - 0.0))).margin(1e-12));
  REQUIRE(aed_free(1) == Approx(B5(1.5 * (rho_i - 0.5))).margin(1e-12));
  REQUIRE(aed_free(2) == Approx(B5(1.5 * (rho_i - 1.0))).margin(1e-12));
}
