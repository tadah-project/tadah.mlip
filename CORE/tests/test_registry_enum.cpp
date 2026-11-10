// Catch2 v2 unit tests for the new Registry::names() / Registry::view()
// enumeration API used by the HPO bound-validation pipeline.

#include "catch2/catch.hpp"
#include <tadah/core/registry.h>

#include <algorithm>
#include <string>
#include <vector>

// NOTE: dummy types are at namespace scope (not anonymous) so the explicit
// definition of Registry<DummyBase>::registry below has external linkage,
// matching the convention used in MODELS/src/d2_all.cpp etc. (see d2_all.cpp:6).
struct DummyBase {
  virtual ~DummyBase() = default;
  virtual std::string label() const = 0;
};

struct DummyA : DummyBase { std::string label() const override { return "A"; } };
struct DummyB : DummyBase { std::string label() const override { return "B"; } };
struct DummyC : DummyBase { std::string label() const override { return "C"; } };

// Per-specialisation definition of the static `registry` member.
// Required because factory<>() (registry.h) odr-uses it. Mirrors the
// pattern in MODELS/src/d2_all.cpp:5-7.
template<>
tadah::core::Registry<DummyBase>::Map&
    tadah::core::Registry<DummyBase>::registry =
        tadah::core::Registry<DummyBase>::instance();

// Static registrations exist for the lifetime of the test binary.
static tadah::core::Registry<DummyBase>::Register<DummyA> reg_a("DummyA");
static tadah::core::Registry<DummyBase>::Register<DummyB> reg_b("DummyB");
static tadah::core::Registry<DummyBase>::Register<DummyC> reg_c("DummyC");

TEST_CASE("Registry::names returns every registered type, sorted") {
  using R = tadah::core::Registry<DummyBase>;
  auto names = R::names();
  REQUIRE(names.size() >= 3);
  REQUIRE(std::find(names.begin(), names.end(), "DummyA") != names.end());
  REQUIRE(std::find(names.begin(), names.end(), "DummyB") != names.end());
  REQUIRE(std::find(names.begin(), names.end(), "DummyC") != names.end());

  // Order is map (lexicographic). The dummies appear in lex order relative
  // to each other.
  auto pos_a = std::find(names.begin(), names.end(), "DummyA");
  auto pos_b = std::find(names.begin(), names.end(), "DummyB");
  auto pos_c = std::find(names.begin(), names.end(), "DummyC");
  REQUIRE(pos_a < pos_b);
  REQUIRE(pos_b < pos_c);
}

TEST_CASE("Registry::view aliases the live map and stays in sync") {
  using R = tadah::core::Registry<DummyBase>;
  const auto& m = R::view();
  REQUIRE(m.find("DummyA") != m.end());
  REQUIRE(m.find("DummyB") != m.end());
  // names() and view() must agree on cardinality.
  REQUIRE(m.size() == R::names().size());
}

TEST_CASE("factory<>() still works after enumeration extensions") {
  // Smoke: existing API is unchanged.
  auto* p = tadah::core::factory<DummyBase>("DummyA");
  REQUIRE(p != nullptr);
  REQUIRE(p->label() == "A");
  delete p;
}
