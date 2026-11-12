// test_intercept_columns.cpp
//
// Function_Base::intercept_columns(context): the design-matrix columns
// (0-based weight indices) of a function that are intercepts, columns
// whose value for an atom depends only on its species (every atom of a
// species adds the same value to the energy row; the force and stress rows
// are 0). The ridge leaves them out of the LAMBDA penalty, and BIAS true
// training refuses data whose compositions cannot determine them. With
// BIAS true the descriptor starts with N = size(ATOM) one-hot intercept
// components, so with two or more species a function's intercept columns
// are not always its leading columns: BF_Polynomial2's weights are the
// products (i,ii), i <= ii, row by row, and the constant of species k is
// the product (k,k).

#include "catch2/catch.hpp"
#include <tadah/core/context.h>
#include <tadah/core/core_types.h>
#include <tadah/core/periodic_table.h>
#include <tadah/models/functions/f_all.h>

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace tadah::core;
using namespace tadah::models;

namespace {

using Cols = std::vector<std::size_t>;

/* BIAS and ATOM, and DSIZE (0: none). */
Context ctx(bool bias, const std::vector<std::string> &atoms,
            std::size_t dsize = 0) {
  Context c;
  c.add("BIAS", bias ? "true" : "false");
  for (const auto &a : atoms) c.add("ATOM", a);
  if (dsize) c.addInternal("DSIZE", dsize);
  return c;
}

/* Basis vectors of length 5 (two intercept components, three descriptor
 * ones): e0, then `second`, then two descriptor-like vectors. */
Matrix basis_with(const std::vector<double> &second) {
  Matrix b(5, 4);
  b.set_zero();
  b(0, 0) = 1.0;
  for (std::size_t k = 0; k < 5; ++k) b(k, 1) = second[k];
  for (std::size_t k = 0; k < 5; ++k) {
    b(k, 2) = k < 2 ? (k == 0 ? 1.0 : 0.0) : 0.3 * k;
    b(k, 3) = k < 2 ? (k == 1 ? 1.0 : 0.0) : -0.2 * k;
  }
  return b;
}

}  // namespace

/* Pins: intercept_columns() of each function, as 0-based column indices:
 *   - BF_Linear and Kern_Linear: the descriptor's intercept columns, {0, 1}
 *     for ATOM Nb Ta with BIAS true, {0} for ATOM Ta, none with BIAS false;
 *   - BF_Polynomial2: the product (k,k) of each intercept component with
 *     itself, row-major over i <= ii: {0, 5} for ATOM Nb Ta and DSIZE 5,
 *     {0, 6, 11} for three species and DSIZE 6, {0} for ATOM Ta, none with
 *     BIAS false; with two species and no DSIZE it throws
 *     std::logic_error naming DSIZE (the column of (1,1) depends on it);
 *   - Kern_Quadratic, Kern_LQ, Kern_Polynomial, Kern_Sigmoid (K(b, x) =
 *     f(b . x)): each column k < N whose basis vector has no component
 *     other than k, so that K(b_k, x) depends on x only through the
 *     intercept component k: {0, 1} for the basis e0, e1, ... on ATOM Nb
 *     Ta, {0} when the second basis vector has descriptor components too,
 *     none without a basis or with BIAS false;
 *   - Kern_RBF: none (K(b, x) depends on every component of x).
 * Before the fix: intercept_columns() returned a count of leading columns
 * (size_t), so this file did not compile; the count was 1 for
 * BF_Polynomial2 and for the dot-product kernels whatever the number of
 * species (column 0 only). */
TEST_CASE("Function_Base::intercept_columns lists the intercept columns of "
          "each function by index",
          "[intercept_columns]") {
  PeriodicTable::initialize();
  SECTION("BF_Linear and Kern_Linear") {
    for (const Function_Base *f :
         std::vector<const Function_Base *>{new BF_Linear(), new Kern_Linear()}) {
      std::unique_ptr<const Function_Base> owner(f);
      CAPTURE(f->get_label());
      CHECK(f->intercept_columns(ctx(true, {"Nb", "Ta"})) == Cols{0, 1});
      CHECK(f->intercept_columns(ctx(true, {"Ta"})) == Cols{0});
      CHECK(f->intercept_columns(ctx(false, {"Nb", "Ta"})).empty());
    }
  }
  SECTION("BF_Polynomial2") {
    BF_Polynomial2 p;
    CHECK(p.intercept_columns(ctx(true, {"Nb", "Ta"}, 5)) == Cols{0, 5});
    CHECK(p.intercept_columns(ctx(true, {"Nb", "Ta", "W"}, 6)) ==
          Cols{0, 6, 11});
    CHECK(p.intercept_columns(ctx(true, {"Ta"}, 4)) == Cols{0});
    CHECK(p.intercept_columns(ctx(true, {"Ta"})) == Cols{0});
    CHECK(p.intercept_columns(ctx(false, {"Nb", "Ta"}, 5)).empty());
    CHECK_THROWS_AS(p.intercept_columns(ctx(true, {"Nb", "Ta"})),
                    std::logic_error);
    CHECK_THROWS_WITH(p.intercept_columns(ctx(true, {"Nb", "Ta"})),
                      Catch::Contains("DSIZE"));
  }
  SECTION("kernels of b . x") {
    std::vector<std::unique_ptr<Kern_Base>> kernels;
    kernels.emplace_back(new Kern_Quadratic());
    kernels.emplace_back(new Kern_LQ());
    kernels.emplace_back(new Kern_Polynomial());
    kernels.emplace_back(new Kern_Sigmoid());
    for (auto &k : kernels) {
      CAPTURE(k->get_label());
      CHECK(k->intercept_columns(ctx(true, {"Nb", "Ta"})).empty());  // no basis
      k->set_basis(basis_with({0, 1, 0, 0, 0}));
      CHECK(k->intercept_columns(ctx(true, {"Nb", "Ta"})) == Cols{0, 1});
      CHECK(k->intercept_columns(ctx(true, {"Ta"})) == Cols{0});
      CHECK(k->intercept_columns(ctx(false, {"Nb", "Ta"})).empty());
      k->set_basis(basis_with({0, 1, 0.4, 0, 0}));
      CHECK(k->intercept_columns(ctx(true, {"Nb", "Ta"})) == Cols{0});
    }
  }
  SECTION("Kern_RBF") {
    Kern_RBF k;
    k.set_basis(basis_with({0, 1, 0, 0, 0}));
    CHECK(k.intercept_columns(ctx(true, {"Nb", "Ta"})).empty());
  }
}
