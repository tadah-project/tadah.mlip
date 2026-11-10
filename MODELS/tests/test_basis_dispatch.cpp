#include "catch2/catch.hpp"

#include <cstdint>
#include <cstring>
#include <memory>
#include <string>

#include <tadah/core/registry.h>
#include <tadah/models/descriptors/basis_kind.h>
#include <tadah/models/descriptors/basis_dispatch.h>
#include <tadah/models/descriptors/d_basis_functions.h>
#include <tadah/models/descriptors/d_all.h>

using namespace tadah::models;

// ---------------------------------------------------------------------------
//  BasisKind: wire codes, traits, and the two parsers.
// ---------------------------------------------------------------------------
TEST_CASE("BasisKind wire codes are frozen 0..3", "[basis_kind]") {
  REQUIRE(static_cast<int>(BasisKind::Blip)     == 0);
  REQUIRE(static_cast<int>(BasisKind::Gaussian) == 1);
  REQUIRE(static_cast<int>(BasisKind::Knot5)    == 2);
  REQUIRE(static_cast<int>(BasisKind::Blip5)    == 3);
}

TEST_CASE("BasisKind traits", "[basis_kind]") {
  REQUIRE(basis_traits(BasisKind::Blip).compact_support);
  REQUIRE(basis_traits(BasisKind::Blip5).compact_support);
  REQUIRE(basis_traits(BasisKind::Knot5).compact_support);
  REQUIRE_FALSE(basis_traits(BasisKind::Gaussian).compact_support);  // the smell

  REQUIRE(basis_traits(BasisKind::Knot5).eta_is_gauge);              // no SGRID*
  REQUIRE_FALSE(basis_traits(BasisKind::Blip).eta_is_gauge);
  REQUIRE(std::string(basis_name(BasisKind::Gaussian)) == "Gaussian");
}

TEST_CASE("basis_kind_from_code round-trips and rejects unknown", "[basis_kind]") {
  for (int c = 0; c <= 3; ++c)
    REQUIRE(basis_code(basis_kind_from_code(c)) == c);
  REQUIRE_THROWS_AS(basis_kind_from_code(4),  std::runtime_error);
  REQUIRE_THROWS_AS(basis_kind_from_code(-1), std::runtime_error);
}

/* Pins: the REFIT_BASIS spellings map to their family (aliases included;
 * BLIP is Blip, as BLIP5 and KNOT5 are theirs), and a spelling of no family
 * is no basis: basis_kind_from_name gives none, basis_kind_from_string
 * throws naming it.
 * Before the fix: an unknown spelling ("nonsense", a typo such as "Blipp")
 * was Blip ("lenient by design"), so tadah refit fitted Blip while it
 * warned of the experimental Gaussian basis. */
TEST_CASE("basis_kind_from_string honours the refit aliases", "[basis_kind]") {
  REQUIRE(basis_kind_from_string("Gaussian") == BasisKind::Gaussian);
  REQUIRE(basis_kind_from_string("D2_BP")    == BasisKind::Gaussian);
  REQUIRE(basis_kind_from_string("Knot3")    == BasisKind::Knot5);   // alias
  REQUIRE(basis_kind_from_string("KnotMix")  == BasisKind::Knot5);   // alias
  REQUIRE(basis_kind_from_string("M6")       == BasisKind::Blip5);   // alias
  REQUIRE(basis_kind_from_string("Blip")     == BasisKind::Blip);
  REQUIRE(basis_kind_from_string("BLIP")     == BasisKind::Blip);
  REQUIRE(basis_kind_from_name("BLIP5")      == BasisKind::Blip5);
  REQUIRE_FALSE(basis_kind_from_name("nonsense").has_value());
  REQUIRE_FALSE(basis_kind_from_name("Blipp").has_value());
  REQUIRE_THROWS_WITH(basis_kind_from_string("nonsense"),
                      "BasisKind: 'nonsense' names no basis (Blip, Blip5, "
                      "Knot5 and its aliases, Gaussian).");
}

// ---------------------------------------------------------------------------
//  basis_dispatch: numeric identity with the raw primitives for all 4 bases.
//  These are the exact expressions DM_REAM's psi/dpsi/gemb/dgemb used to inline.
// ---------------------------------------------------------------------------
TEST_CASE("radial_value/deriv identical to raw primitives", "[basis_dispatch]") {
  const double r = 2.3, c = 1.7, s = 1.4, fc = 0.8, fcp = -0.35;

  REQUIRE(radial_value(BasisKind::Blip,     r, fc, s, c) == B (s * (r - c), fc));
  REQUIRE(radial_value(BasisKind::Blip5,    r, fc, s, c) == B5(s * (r - c), fc));
  REQUIRE(radial_value(BasisKind::Gaussian, r, fc, s, c) == G (r, s, c, fc));
  REQUIRE(radial_value(BasisKind::Knot5,    r, fc, s, c, 5) == K5L(r - c, fc));
  REQUIRE(radial_value(BasisKind::Knot5,    r, fc, s, c, 3) == K3L(r - c, fc));

  REQUIRE(radial_deriv(BasisKind::Blip,     r, fc, fcp, s, c) == dB (s * (r - c), s, fc, fcp));
  REQUIRE(radial_deriv(BasisKind::Blip5,    r, fc, fcp, s, c) == dB5(s * (r - c), s, fc, fcp));
  REQUIRE(radial_deriv(BasisKind::Gaussian, r, fc, fcp, s, c) == dG (r, s, c, fc, fcp));
  REQUIRE(radial_deriv(BasisKind::Knot5,    r, fc, fcp, s, c, 5) == dK5L(r - c, 1.0, fc, fcp));
  REQUIRE(radial_deriv(BasisKind::Knot5,    r, fc, fcp, s, c, 3) == dK3L(r - c, 1.0, fc, fcp));

  for (BasisKind k : {BasisKind::Blip, BasisKind::Blip5,
                      BasisKind::Knot5, BasisKind::Gaussian}) {
    double value = 0.0, deriv = 0.0;
    radial_value_deriv(k, r, fc, fcp, s, c, value, deriv);
    REQUIRE(value == radial_value(k, r, fc, s, c));
    REQUIRE(deriv == radial_deriv(k, r, fc, fcp, s, c));
  }
}

TEST_CASE("embedding_value/deriv identical to raw primitives (RIGHT knots, no cutoff)",
          "[basis_dispatch]") {
  const double rho = 3.1, c = 2.0, s = 0.9;

  REQUIRE(embedding_value(BasisKind::Blip,     rho, s, c) == B (s * (rho - c)));
  REQUIRE(embedding_value(BasisKind::Blip5,    rho, s, c) == B5(s * (rho - c)));
  REQUIRE(embedding_value(BasisKind::Gaussian, rho, s, c) == G (rho, s, c));
  REQUIRE(embedding_value(BasisKind::Knot5,    rho, s, c, 5) == K5R(rho - c));
  REQUIRE(embedding_value(BasisKind::Knot5,    rho, s, c, 3) == K3R(rho - c));

  REQUIRE(embedding_deriv(BasisKind::Blip,     rho, s, c) == dB (s * (rho - c), s));
  REQUIRE(embedding_deriv(BasisKind::Blip5,    rho, s, c) == dB5(s * (rho - c), s));
  REQUIRE(embedding_deriv(BasisKind::Gaussian, rho, s, c) == dG (rho, s, c));
  REQUIRE(embedding_deriv(BasisKind::Knot5,    rho, s, c, 5) == dK5R(rho - c, 1.0));
  REQUIRE(embedding_deriv(BasisKind::Knot5,    rho, s, c, 3) == dK3R(rho - c, 1.0));

  for (BasisKind k : {BasisKind::Blip, BasisKind::Blip5,
                      BasisKind::Knot5, BasisKind::Gaussian}) {
    double value = 0.0, deriv = 0.0;
    embedding_value_deriv(k, rho, s, c, value, deriv);
    REQUIRE(value == embedding_value(k, rho, s, c));
    REQUIRE(deriv == embedding_deriv(k, rho, s, c));
  }
}

TEST_CASE("embedding_value == fused value at rho = 0, bitwise (EMBF0 g(0))",
          "[basis_dispatch][embf0]") {
  // DM_REAM's EMBF0 ZERO reference g_m(0) is evaluated with embedding_value
  // (calc_aed) and with embedding_value_deriv (eam_embed): the two must agree
  // bitwise at rho = 0 so both paths subtract the same reference. Covers the
  // support edges and the RIGHT-active knots with mu < 0 (the only knots
  // with g(0) != 0).
  auto bits = [](double v) {
    std::uint64_t u;
    std::memcpy(&u, &v, sizeof u);
    return u;
  };
  struct Case { BasisKind k; double s, c; int deg; };
  const Case cases[] = {
      {BasisKind::Blip, 0.9, 0.0, 5},     {BasisKind::Blip, 1.3, -0.7, 5},
      {BasisKind::Blip, 1.3, 0.7, 5},     {BasisKind::Blip, 2.0, 1.0, 5},
      {BasisKind::Blip, 2.0, -0.6, 5},    {BasisKind::Blip5, 0.9, 0.0, 5},
      {BasisKind::Blip5, 1.5, -1.1, 5},   {BasisKind::Blip5, 1.5, 1.9, 5},
      {BasisKind::Blip5, 2.0, 1.5, 5},    {BasisKind::Blip5, 0.5, -5.0, 5},
      {BasisKind::Gaussian, 0.9, 0.0, 5}, {BasisKind::Gaussian, 2.5, -0.8, 5},
      {BasisKind::Gaussian, 40.0, 0.3, 5},
      {BasisKind::Knot5, 1.0, -0.8, 5},   {BasisKind::Knot5, 1.0, -0.8, 3},
      {BasisKind::Knot5, 1.0, -2.3, 5},   {BasisKind::Knot5, 1.0, -2.3, 3},
      {BasisKind::Knot5, 1.0, 0.0, 5},    {BasisKind::Knot5, 1.0, 0.0, 3},
      {BasisKind::Knot5, 1.0, 0.4, 5},    {BasisKind::Knot5, 1.0, 0.4, 3},
  };
  for (const Case &cs : cases) {
    INFO(basis_name(cs.k) << " s=" << cs.s << " c=" << cs.c << " deg=" << cs.deg);
    double value = 0.0, deriv = 0.0;
    embedding_value_deriv(cs.k, 0.0, cs.s, cs.c, value, deriv, cs.deg);
    REQUIRE(bits(value) == bits(embedding_value(cs.k, 0.0, cs.s, cs.c, cs.deg)));
    REQUIRE(bits(deriv) == bits(embedding_deriv(cs.k, 0.0, cs.s, cs.c, cs.deg)));
  }
  // g(0) != 0 exactly for the knots with mu < 0 and zero for mu >= 0
  REQUIRE(embedding_value(BasisKind::Knot5, 0.0, 1.0, -0.8, 5) == K5R(0.8));
  REQUIRE(embedding_value(BasisKind::Knot5, 0.0, 1.0, -0.8, 3) == K3R(0.8));
  REQUIRE(embedding_value(BasisKind::Knot5, 0.0, 1.0, -0.8, 5) > 0.0);
  REQUIRE(embedding_value(BasisKind::Knot5, 0.0, 1.0, 0.4, 5) == 0.0);
  REQUIRE(embedding_value(BasisKind::Knot5, 0.0, 1.0, 0.0, 3) == 0.0);
}

TEST_CASE("bare radial matches the legacy models::B/G function-pointer shape",
          "[basis_dispatch]") {
  const double r = 2.05, c = 1.6, s = 1.2;
  // The analysis engine called fb(range, s, c) with fb in {B, dB, G, dG}
  // (the 3-arg overloads). The bare dispatch must reproduce them exactly.
  REQUIRE(radial_value_bare(BasisKind::Blip,     r, s, c) == B(r, s, c));
  REQUIRE(radial_value_bare(BasisKind::Gaussian, r, s, c) == G(r, s, c));
  REQUIRE(radial_deriv_bare(BasisKind::Blip,     r, s, c) == dB(r, s, c));
  REQUIRE(radial_deriv_bare(BasisKind::Gaussian, r, s, c) == dG(r, s, c));
}

// ---------------------------------------------------------------------------
//  Descriptor basis_kind(): the single virtual that replaced the
//  descriptor-name string ladders (analysis engine, capabilities()).
// ---------------------------------------------------------------------------
TEST_CASE("descriptor basis_kind() reports the radial family",
          "[basis_kind][descriptor]") {
  using tadah::core::factory;
  auto bk = [](const char *n) {
    std::unique_ptr<D_Base> d(factory<D_Base>(n));
    return d->basis_kind();
  };
  REQUIRE(bk("D2_Blip")  == BasisKind::Blip);
  REQUIRE(bk("D2_Blip5") == BasisKind::Blip5);
  REQUIRE(bk("D2_Knot5") == BasisKind::Knot5);
  REQUIRE(bk("D2_BP")    == BasisKind::Gaussian);
  REQUIRE(bk("DM_Blip")  == BasisKind::Blip);
  REQUIRE(bk("DM_EAD")   == BasisKind::Gaussian);
  REQUIRE(bk("DM_mBlip") == BasisKind::Gaussian);  // DM_mEAD<F_Blip>: radial GTO
  // Tabulated / fixed / dummy descriptors have no selectable analytic basis.
  REQUIRE_FALSE(bk("DM_EAM").has_value());
  REQUIRE_FALSE(bk("D2_Dummy").has_value());
}
