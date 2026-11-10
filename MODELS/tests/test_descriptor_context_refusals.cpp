/* A descriptor is built from a context by its constructor, which refuses a
 * context it cannot evaluate: grids of different sizes, an Lmax above 6, a
 * KDEG2B list of another length than CGRID2B or with a degree other than 3
 * or 5, a negative Mie exponent. The library has no other way to give a
 * descriptor new parameters: a caller with a new context builds a new
 * descriptor (or a new DC_Selector), and the constructor's checks apply.
 *
 * The contexts below are those that D_Base::reinit (removed in Tadah! 1.6.0)
 * accepted although the constructors refused them; on the descriptor it
 * updated, the next evaluation read or wrote out of bounds
 * (AddressSanitizer: heap-buffer-overflow in D2_Blip, D2_Blip5, D2_BP,
 * DM_Blip, DM_EAD, DM_mRLR and DM_PS_SQ; a read past the factorial table at
 * Lmax 7 in DM_EAD, DM_Blip, DM_mBlip and DM_PS_SQ). Each is refused by the
 * constructor.
 */

#include "catch2/catch.hpp"
#include <tadah/core/context.h>
#include <tadah/core/core_types.h>
#include <tadah/core/periodic_table.h>
#include <tadah/models/descriptors/d_all.h>

#include <functional>
#include <string>
#include <utility>
#include <vector>

using namespace tadah::core;
using namespace tadah::models;

namespace {

const double kRc = 5.0;

/* Many-body context: TYPEMB <label> <Lmax> 1 1 H H (the DM_Blip / DM_EAD
 * layout). */
Context mb_ctx(const std::string &label, int lmax, const std::string &cg,
               const std::string &sg) {
  Context c;
  c.add("RCTYPEMB", "Cut_Cos");
  c.add("RCUTMB", kRc);
  c.remove("INITMB");
  c.add("INITMB", "true");
  for (const std::string &t :
       {label, std::to_string(lmax), std::string("1"), std::string("1"),
        std::string("H"), std::string("H")})
    c.add("TYPEMB", t);
  c.add("CGRIDMB", cg);
  c.add("SGRIDMB", sg);
  return c;
}

/* DM_PS_SQ: TYPEMB DM_PS_SQ <Lmax> <Lmax> <Lmax>. */
Context ps_ctx(int lmax, const std::string &cg, const std::string &sg) {
  Context c;
  c.add("RCTYPEMB", "Cut_Cos");
  c.add("RCUTMB", kRc);
  c.remove("INITMB");
  c.add("INITMB", "true");
  c.add("TYPEMB", "DM_PS_SQ");
  for (int k = 0; k < 3; ++k) c.add("TYPEMB", lmax);
  c.add("CGRIDMB", cg);
  c.add("SGRIDMB", sg);
  return c;
}

/* The DM_mEAD layout: TYPEMB <label> <Lmax> 1 1 2 2 H H, with CEMBFUNC and
 * SEMBFUNC. */
Context mead_ctx(const std::string &label, int lmax, const std::string &cg,
                 const std::string &sg, const std::string &cemb) {
  Context c;
  c.add("RCTYPEMB", "Cut_Cos");
  c.add("RCUTMB", kRc);
  c.remove("INITMB");
  c.add("INITMB", "true");
  c.add("CGRIDMB", cg);
  c.add("SGRIDMB", sg);
  c.add("CEMBFUNC", cemb);
  c.add("SEMBFUNC", "1.0 1.0");
  for (const std::string &t :
       {label, std::to_string(lmax), std::string("1"), std::string("1"),
        std::string("2"), std::string("2"), std::string("H"),
        std::string("H")})
    c.add("TYPEMB", t);
  return c;
}

/* Two-body context: TYPE2B <label> 1 1 H H. */
Context d2_ctx(const std::string &label, const std::string &cg,
               const std::string &sg) {
  Context c;
  c.add("RCTYPE2B", "Cut_Cos");
  c.add("RCUT2B", kRc);
  c.remove("INIT2B");
  c.add("INIT2B", "true");
  for (const std::string &t :
       {label, std::string("1"), std::string("1"), std::string("H"),
        std::string("H")})
    c.add("TYPE2B", t);
  c.add("CGRID2B", cg);
  c.add("SGRID2B", sg);
  return c;
}

/* D2_Knot5: TYPE2B D2_Knot5 3 <n> H H, three knots, KDEG2B @p kdeg. */
Context knot5_ctx(const std::string &kdeg, int n) {
  Context c;
  c.add("RCTYPE2B", "Cut_Cos");
  c.add("RCUT2B", kRc);
  c.remove("INIT2B");
  c.add("INIT2B", "true");
  for (const std::string &t :
       {std::string("D2_Knot5"), std::string("3"), std::to_string(n),
        std::string("H"), std::string("H")})
    c.add("TYPE2B", t);
  c.add("CGRID2B", "2.5 3.5 4.5");
  c.add("KDEG2B", kdeg);
  return c;
}

/* D2_MIE: TYPE2B D2_MIE <n> <m> H H. */
Context mie_ctx(const std::string &n, const std::string &m) {
  Context c;
  c.add("RCTYPE2B", "Cut_Cos");
  c.add("RCUT2B", kRc);
  c.remove("INIT2B");
  c.add("INIT2B", "true");
  for (const std::string &t :
       {std::string("D2_MIE"), n, m, std::string("H"), std::string("H")})
    c.add("TYPE2B", t);
  return c;
}

struct Refused {
  std::string what;                // the descriptor and the context
  std::function<void()> build;     // builds the descriptor
  std::string named;               // a word the refusal has
};

}  // namespace

/* Pins: the constructor of each descriptor refuses the contexts that
 * D_Base::reinit accepted: CGRID 2.0 3.0 4.0 4.5 with SGRID 1.0 1.0 for
 * D2_Blip, D2_Blip5, D2_BP, DM_Blip, DM_EAD, DM_mRLR (CEMBFUNC of four
 * values) and DM_PS_SQ (CGRIDMB 2.5 3.0 with SGRIDMB 1.0 for DM_mBlip);
 * Lmax 7 for DM_EAD, DM_Blip, DM_mBlip and DM_PS_SQ; KDEG2B 4 3 (two
 * degrees for three knots) and 4 4 4 (a degree other than 3 or 5) for
 * D2_Knot5; a negative Mie exponent (-6 12) for D2_MIE. Each throws, and
 * the message names the key or the limit (CGRID, Lmax, KDEG2B, Mie); each
 * descriptor builds from the same context with consistent values.
 * Before the fix: passes as well (a pin: the constructors already refused
 * these contexts; D_Base::reinit, which skipped their checks, accepted
 * each of them, and the descriptor it updated then read or wrote out of
 * bounds). */
TEST_CASE("A descriptor's constructor refuses the contexts D_Base::reinit "
          "accepted",
          "[descriptor][constructor][refusal]") {
  PeriodicTable::initialize();
  const std::string grid4 = "2.0 3.0 4.0 4.5", sgrid2 = "1.0 1.0";
  const std::vector<Refused> refused = {
      {"D2_Blip, 4 CGRID2B and 2 SGRID2B",
       [&] { D2_Blip d(d2_ctx("D2_Blip", grid4, sgrid2)); }, "CGRID2B"},
      {"D2_Blip5, 4 CGRID2B and 2 SGRID2B",
       [&] { D2_Blip5 d(d2_ctx("D2_Blip5", grid4, sgrid2)); }, "CGRID2B"},
      {"D2_BP, 4 CGRID2B and 2 SGRID2B",
       [&] { D2_BP d(d2_ctx("D2_BP", grid4, sgrid2)); }, "CGRID2B"},
      {"D2_Knot5, KDEG2B 4 3 for 3 knots",
       [&] { D2_Knot5 d(knot5_ctx("4 3", 3)); }, "KDEG2B"},
      {"D2_Knot5, KDEG2B 4 4 4",
       [&] { D2_Knot5 d(knot5_ctx("4 4 4", 3)); }, "KDEG2B"},
      {"D2_MIE, exponents -6 12", [&] { D2_MIE d(mie_ctx("-6", "12")); },
       "Mie"},
      {"DM_Blip, 4 CGRIDMB and 2 SGRIDMB",
       [&] { DM_Blip d(mb_ctx("DM_Blip", 1, grid4, sgrid2)); }, "CGRIDMB"},
      {"DM_Blip, Lmax 7",
       [&] { DM_Blip d(mb_ctx("DM_Blip", 7, "2.0 3.0", sgrid2)); }, "Lmax"},
      {"DM_EAD, 4 CGRIDMB and 2 SGRIDMB",
       [&] { DM_EAD d(mb_ctx("DM_EAD", 1, grid4, sgrid2)); }, "CGRIDMB"},
      {"DM_EAD, Lmax 7",
       [&] { DM_EAD d(mb_ctx("DM_EAD", 7, "2.0 3.0", sgrid2)); }, "Lmax"},
      {"DM_mBlip, 2 CGRIDMB and 1 SGRIDMB",
       [&] {
         DM_mEAD<F_Blip> d(mead_ctx("DM_mBlip", 1, "2.5 3.0", "1.0", "0.5 1.0"));
       },
       "CGRIDMB"},
      {"DM_mBlip, Lmax 7",
       [&] {
         DM_mEAD<F_Blip> d(mead_ctx("DM_mBlip", 7, "2.5", "1.0", "0.5 1.0"));
       },
       "Lmax"},
      {"DM_mRLR, 4 CGRIDMB, 2 SGRIDMB and 4 CEMBFUNC",
       [&] {
         DM_mEAD<F_RLR> d(
             mead_ctx("DM_mRLR", 1, grid4, sgrid2, "0.5 1.0 1.5 2.0"));
       },
       "CGRIDMB"},
      {"DM_PS_SQ, 4 CGRIDMB and 2 SGRIDMB",
       [&] { DM_Poly<F_SQ> d(ps_ctx(1, grid4, sgrid2)); }, "CGRIDMB"},
      {"DM_PS_SQ, Lmax 7",
       [&] { DM_Poly<F_SQ> d(ps_ctx(7, "2.0 3.0", sgrid2)); }, "Lmax"}};
  for (const Refused &r : refused) {
    INFO(r.what);
    CHECK_THROWS_WITH(r.build(), Catch::Contains(r.named));
  }

  // The same descriptors from consistent contexts build.
  CHECK_NOTHROW(D2_Blip(d2_ctx("D2_Blip", "2.0 3.0", sgrid2)));
  CHECK_NOTHROW(D2_Blip5(d2_ctx("D2_Blip5", "2.0 3.0", sgrid2)));
  CHECK_NOTHROW(D2_BP(d2_ctx("D2_BP", "2.0 3.0", sgrid2)));
  CHECK_NOTHROW(D2_Knot5(knot5_ctx("5 3 5", 3)));
  CHECK_NOTHROW(D2_MIE(mie_ctx("6", "12")));
  CHECK_NOTHROW(DM_Blip(mb_ctx("DM_Blip", 6, "2.0 3.0", sgrid2)));
  CHECK_NOTHROW(DM_EAD(mb_ctx("DM_EAD", 6, "2.0 3.0", sgrid2)));
  CHECK_NOTHROW(
      DM_mEAD<F_Blip>(mead_ctx("DM_mBlip", 1, "2.5", "1.0", "0.5 1.0")));
  CHECK_NOTHROW(
      DM_mEAD<F_RLR>(mead_ctx("DM_mRLR", 1, "2.0 3.0", sgrid2, "0.5 1.0")));
  CHECK_NOTHROW(DM_Poly<F_SQ>(ps_ctx(1, "2.0 3.0", sgrid2)));
}
