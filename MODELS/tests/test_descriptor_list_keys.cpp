// test_descriptor_list_keys.cpp
//
// The TYPE2B / TYPEMB lines of a configuration are split by
// D_mJoin::parse_context: `tadah train` and `tadah hpo` run it on every
// configuration (D_mJoin::expand_grids), and D2_mJoin / DM_mJoin run it on
// their own lines. For each descriptor of the line it takes the
// descriptor's parameters, one RCTYPE*/RCUT* value, and the number of list
// values (CGRID2B, SGRID2B, KDEG2B; CGRIDMB, SGRIDMB, AMPGRIDMB, CEMBFUNC,
// SEMBFUNC, KDEGMB, KDEGEMB) that each SIZE parameter asks for. It read
// them without checking that they were there:
//  - a list key that is not set (D2_BP 2 2 without CGRID2B) was read
//    through a null pointer: a segmentation fault, for every descriptor
//    with list keys;
//  - a line without its parameters (`TYPE2B D2_BP`) was read past its end:
//    a segmentation fault; a non-integer SIZE (`D2_BP * *`) was reported
//    as 'stoi';
//  - fewer RCTYPE2B/RCUT2B values than descriptors were read past the end
//    of the values (std::bad_alloc in `tadah train`).
// Each is now refused with a message that names the key and the line.

#include "catch2/catch.hpp"

#include <tadah/core/context.h>
#include <tadah/core/periodic_table.h>
#include <tadah/core/registry.h>
#include <tadah/models/descriptors/d_all.h>
#include <tadah/models/descriptors/d_mjoin.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace tadah::core;
using namespace tadah::models;
using Catch::Matchers::Contains;

namespace {

/* A TYPE line of one descriptor and the list keys it reads. */
struct LineSpec {
  std::string type_key;               // TYPE2B or TYPEMB
  std::string line;                   // e.g. "D2_BP 2 2 Ta Ta"
  std::vector<std::string> list_keys; // keys, then data_keys
  std::size_t n = 0;                  // values per list key
};

/* The line of `label` with its family parameters set to 1, each SIZE
 * parameter set to `n`, and the element pair Ta Ta. */
LineSpec spec_for(const std::string &label, std::size_t n) {
  std::unique_ptr<D_Base> d(factory<D_Base>(label));
  LineSpec s;
  s.type_key = label.rfind("D2_", 0) == 0 ? "TYPE2B" : "TYPEMB";
  s.list_keys = d->keys;
  s.list_keys.insert(s.list_keys.end(), d->data_keys.begin(),
                     d->data_keys.end());
  s.n = n;
  s.line = label;
  const std::size_t fparam = d->nparams - s.list_keys.size();
  for (std::size_t j = 0; j < fparam; ++j) s.line += " 1";
  for (std::size_t k = 0; k < s.list_keys.size(); ++k)
    s.line += " " + std::to_string(n);
  s.line += " Ta Ta";
  return s;
}

/* The context of `s` with every list key set to n values of 3, except the
 * keys in `unset`. */
Context context_for(const LineSpec &s,
                    const std::vector<std::string> &unset) {
  PeriodicTable::initialize();
  const std::string sfx = s.type_key.substr(4);  // 2B or MB
  Context c;
  c.add("VERBOSE", 0);
  c.add("INIT" + sfx, "true");
  c.add("RC" + s.type_key, "Cut_Cos");
  c.add("RCUT" + sfx, 4.5);
  c.add(s.type_key, s.line);
  for (const auto &k : s.list_keys) {
    bool skip = false;
    for (const auto &u : unset) skip = skip || u == k;
    if (skip) continue;
    for (std::size_t i = 0; i < s.n; ++i) c.add(k, 3);
  }
  return c;
}

/* Every registered descriptor whose TYPE line takes list values, mJoin
 * excluded (its keys are the TYPE/RCTYPE/RCUT keys it splits). A DM_Poly
 * whose channel function reads CEMBFUNC/SEMBFUNC whole is included: its
 * line is checked and split as any other. */
std::vector<std::string> descriptors_with_list_keys() {
  std::vector<std::string> out;
  for (const auto &label : Registry<D_Base>::names()) {
    if (label.find("mJoin") != std::string::npos) continue;
    std::unique_ptr<D_Base> d(factory<D_Base>(label));
    if (!d->keys.empty() || !d->data_keys.empty()) out.push_back(label);
  }
  return out;
}

/* The message that names the keys of `unset` that the line of `s` needs. */
void check_refused(const LineSpec &s, const std::vector<std::string> &unset) {
  Context c = context_for(s, unset);
  std::string what;
  try {
    D_mJoin::expand_grids(c);
  } catch (const std::exception &e) {
    what = e.what();
  }
  INFO(s.type_key << " " << s.line << " without " << unset.front()
                  << (unset.size() > 1 ? " ..." : "") << ": '" << what
                  << "'");
  CHECK_THAT(what, Contains(s.type_key + " " + s.line));
  CHECK_THAT(what, Contains(unset.size() == 1 ? " is not set"
                                              : " are not set"));
  for (const auto &k : unset)
    CHECK_THAT(what, Contains(std::to_string(s.n) + " " + k + " value"));
}

} // namespace

/* The sweep covers the descriptors with list keys (D2_BP, D2_Blip,
 * D2_Blip5, D2_Knot5, DM_Blip, DM_EAD, DM_REAM, the DM_m* family and the
 * DM_Poly family, those whose channel function reads CEMBFUNC/SEMBFUNC
 * whole included), and with every key set D_mJoin::expand_grids splits each
 * of their lines without an error. The refusals below hold for each of
 * them (before the fix: a DM_Poly line whose channel function reads
 * CEMBFUNC/SEMBFUNC was left to the descriptor, unchecked). */
TEST_CASE("Descriptor list keys: every descriptor line with its list keys "
          "set is split",
          "[D_mJoin][list_keys]") {
  const auto labels = descriptors_with_list_keys();
  for (const char *expected : {"D2_BP", "D2_Blip", "D2_Blip5", "D2_Knot5",
                               "DM_Blip", "DM_EAD", "DM_REAM", "DM_mBlip",
                               "DM_mBlipEnv", "DM_PS_SQ", "DM_GOW_SQ",
                               "DM_PS_SQRT", "DM_GOW_Blip", "DM_OW_RLR"}) {
    INFO(expected);
    CHECK(std::find(labels.begin(), labels.end(), expected) != labels.end());
  }
  for (const auto &label : labels) {
    const LineSpec s = spec_for(label, 2);
    Context c = context_for(s, {});
    INFO(s.type_key << " " << s.line);
    REQUIRE_NOTHROW(D_mJoin::expand_grids(c));
    for (const auto &k : s.list_keys) CHECK(c.size(k) == 2);
  }
}

/* Pins: a two-body line whose list key is not set is refused, naming the
 * line, the key and the number of values it asks for: for D2_BP, D2_Blip,
 * D2_Blip5 and D2_Knot5, without each of their list keys (CGRID2B,
 * SGRID2B, KDEG2B) in turn, and without all of them ('... takes 2 CGRID2B
 * values and 2 SGRID2B values, but CGRID2B and SGRID2B are not set:
 * TYPE2B D2_BP 1 ... ').
 * Before the fix: a segmentation fault at the first line (D_mJoin::
 * parse_context read the values of a key it had not found through a null
 * pointer). */
TEST_CASE("Descriptor list keys: a two-body line whose list key is not set "
          "is refused naming it",
          "[D_mJoin][list_keys]") {
  for (const auto &label : descriptors_with_list_keys()) {
    if (label.rfind("D2_", 0) != 0) continue;
    const LineSpec s = spec_for(label, 2);
    for (const auto &k : s.list_keys) check_refused(s, {k});
    check_refused(s, s.list_keys);
  }
}

/* Pins: the same for every many-body descriptor with list keys (DM_Blip,
 * DM_EAD, DM_REAM, DM_mBlip, DM_mRLR, DM_mSQRT, DM_mBlipEnv and the 16
 * DM_Poly descriptors), without each of their list keys (CGRIDMB, SGRIDMB,
 * AMPGRIDMB, CEMBFUNC, SEMBFUNC, KDEGMB, KDEGEMB) in turn and without all
 * of them.
 * Before the fix: a segmentation fault at the first line. */
TEST_CASE("Descriptor list keys: a many-body line whose list key is not set "
          "is refused naming it",
          "[D_mJoin][list_keys]") {
  for (const auto &label : descriptors_with_list_keys()) {
    if (label.rfind("DM_", 0) != 0) continue;
    const LineSpec s = spec_for(label, 2);
    for (const auto &k : s.list_keys) check_refused(s, {k});
    check_refused(s, s.list_keys);
  }
}

/* Pins: a SIZE parameter of 0 asks for no value, so its key need not be
 * set: `TYPE2B D2_Knot5 2 0 Ta Ta` without KDEG2B (the knot degree is
 * optional) is split, and the context gets no KDEG2B.
 * Before the fix: passes as well (a pin: the refusal must not reach a key
 * the line asks no value of). */
TEST_CASE("Descriptor list keys: a SIZE of 0 needs no key",
          "[D_mJoin][list_keys]") {
  PeriodicTable::initialize();
  Context c;
  c.add("VERBOSE", 0);
  c.add("INIT2B", "true");
  c.add("RCTYPE2B", "Cut_Cos");
  c.add("RCUT2B", 4.5);
  c.add("TYPE2B", "D2_Knot5 2 0 Ta Ta");
  c.add("CGRID2B", "3.0 4.5");
  REQUIRE_NOTHROW(D_mJoin::expand_grids(c));
  CHECK(c.size("CGRID2B") == 2);
  CHECK_FALSE(c.has_stored("KDEG2B"));
}

/* Pins: D2_mJoin and DM_mJoin refuse a joined descriptor whose list key is
 * not set, naming the line of that descriptor: D2_mJoin of D2_LJ and
 * D2_BP 2 2 without SGRID2B ('TYPE2B D2_BP 2 2 Ta Ta' and 'SGRID2B is not
 * set'), DM_mJoin of DM_Blip 1 2 2 without CGRIDMB.
 * Before the fix: a segmentation fault in the D2_mJoin constructor. */
TEST_CASE("Descriptor list keys: a joined descriptor whose list key is not "
          "set is refused naming it",
          "[D_mJoin][list_keys]") {
  PeriodicTable::initialize();
  {
    Context c;
    c.add("VERBOSE", 0);
    c.add("INIT2B", "true");
    c.add("TYPE2B", "D2_mJoin");
    c.add("TYPE2B", "D2_LJ Ta Ta");
    c.add("TYPE2B", "D2_BP 2 2 Ta Ta");
    c.add("RCTYPE2B", "Cut_Cos Cut_Cos");
    c.add("RCUT2B", "4.5 4.5");
    c.add("CGRID2B", "2.5 3.5");
    REQUIRE_THROWS_WITH(D2_mJoin(c),
                        Contains("TYPE2B D2_BP 2 2 Ta Ta") &&
                            Contains("takes 2 SGRID2B values, but SGRID2B "
                                     "is not set"));
  }
  {
    Context c;
    c.add("VERBOSE", 0);
    c.add("INITMB", "true");
    c.add("TYPEMB", "DM_mJoin");
    c.add("TYPEMB", "DM_Blip 1 2 2 Ta Ta");
    c.add("RCTYPEMB", "Cut_Cos");
    c.add("RCUTMB", 4.5);
    c.add("SGRIDMB", "0.5 0.5");
    REQUIRE_THROWS_WITH(DM_mJoin(c),
                        Contains("TYPEMB DM_Blip 1 2 2 Ta Ta") &&
                            Contains("takes 2 CGRIDMB values, but CGRIDMB "
                                     "is not set"));
  }
}

/* Pins: a descriptor line without its parameters is refused as the
 * descriptor itself refuses it (element_tokens), naming the line:
 * `TYPE2B D2_BP` ('takes 2 parameters before its element pairs; the line
 * has 0'), alone and as the second descriptor of a D2_mJoin.
 * Before the fix: a segmentation fault (D_mJoin::parse_context read the
 * parameters past the end of the line). */
TEST_CASE("Descriptor list keys: a line without its parameters is refused",
          "[D_mJoin][list_keys]") {
  PeriodicTable::initialize();
  for (const bool joined : {false, true}) {
    INFO("D2_mJoin: " << joined);
    Context c;
    c.add("VERBOSE", 0);
    c.add("INIT2B", "true");
    if (joined) {
      c.add("TYPE2B", "D2_mJoin");
      c.add("TYPE2B", "D2_LJ Ta Ta");
    }
    c.add("TYPE2B", "D2_BP");
    c.add("RCTYPE2B", joined ? "Cut_Cos Cut_Cos" : "Cut_Cos");
    c.add("RCUT2B", joined ? "4.5 4.5" : "4.5");
    c.add("CGRID2B", "2.5 3.5");
    c.add("SGRID2B", "0.5 0.5");
    REQUIRE_THROWS_WITH(D_mJoin::expand_grids(c),
                        Contains("TYPE2B D2_BP takes 2 parameters before its "
                                 "element pairs; the line has 0: TYPE2B "
                                 "D2_BP"));
  }
}

/* Pins: a SIZE parameter that is not a non-negative integer is refused as
 * the descriptor refuses it: `TYPE2B D2_BP * *` names parameter 1, the
 * number of CGRID2B values, and its token "*".
 * Before the fix: 'stoi' (std::stoi's message), naming nothing. */
TEST_CASE("Descriptor list keys: a SIZE parameter that is not an integer "
          "is refused naming it",
          "[D_mJoin][list_keys]") {
  PeriodicTable::initialize();
  Context c;
  c.add("VERBOSE", 0);
  c.add("INIT2B", "true");
  c.add("TYPE2B", "D2_BP * *");
  c.add("RCTYPE2B", "Cut_Cos");
  c.add("RCUT2B", 4.5);
  c.add("CGRID2B", "2.5 3.5");
  c.add("SGRID2B", "0.5 0.5");
  REQUIRE_THROWS_WITH(D_mJoin::expand_grids(c),
                      Contains("parameter 1, the number of CGRID2B values, "
                               "must be a non-negative integer, not \"*\": "
                               "TYPE2B D2_BP * *"));
}

/* Pins: a joined line needs one RCTYPE2B and one RCUT2B value per
 * descriptor: a
 * D2_mJoin of D2_LJ and D2_BP with one RCTYPE2B value is refused naming
 * RCTYPE2B, the 2 descriptors and the 1 value; with two RCTYPE2B values
 * and one RCUT2B value, naming RCUT2B.
 * Before the fix: the values were read past their end (undefined
 * behaviour): a segmentation fault here; `tadah train` on such a
 * configuration stopped with 'std::bad_alloc'. */
TEST_CASE("Descriptor list keys: a joined line needs one cutoff value per "
          "descriptor",
          "[D_mJoin][list_keys]") {
  PeriodicTable::initialize();
  for (const bool two_types : {false, true}) {
    const std::string key = two_types ? "RCUT2B" : "RCTYPE2B";
    INFO(key);
    Context c;
    c.add("VERBOSE", 0);
    c.add("INIT2B", "true");
    c.add("TYPE2B", "D2_mJoin");
    c.add("TYPE2B", "D2_LJ Ta Ta");
    c.add("TYPE2B", "D2_BP 2 2 Ta Ta");
    c.add("RCTYPE2B", two_types ? "Cut_Cos Cut_Cos" : "Cut_Cos");
    c.add("RCUT2B", two_types ? "4.5" : "4.5 4.5");
    c.add("CGRID2B", "2.5 3.5");
    c.add("SGRID2B", "0.5 0.5");
    REQUIRE_THROWS_WITH(D_mJoin::expand_grids(c),
                        Contains("TYPE2B names 2 descriptors") &&
                            Contains(key + " has 1 value"));
  }
}

/* Pins: a list key with fewer values than a SIZE parameter asks for is
 * refused naming the line, the key, the values the line takes and the
 * values there are:
 *  - TYPE2B D2_BP 3 3 Ta Ta with 2 CGRID2B values: "TYPE2B D2_BP takes 3
 *    CGRID2B values, but CGRID2B has 2 values: TYPE2B D2_BP 3 3 Ta Ta";
 *  - a D2_mJoin of two D2_BP 2 2 Ta Ta lines with 3 CGRID2B values (the
 *    first takes 2): "TYPE2B D2_BP (descriptor 2 of 2) takes 2 CGRID2B
 *    values, but CGRID2B has 1 value left of 3 (the descriptors before it
 *    take 2): TYPE2B D2_BP 2 2 Ta Ta";
 *  - every descriptor with list keys (each SIZE 2), each key in turn with 1
 *    value: the message names the line and "takes 2 <key> values, but <key>
 *    has 1 value".
 * Before the fix: "Error: Not enough data for key 'CGRID2B' in context.
 * Expected 3 but got 2. Token: D2_BP", naming the label but not the line,
 * and for the joined line the key's total, not what was left ("Expected 2
 * but got 3"). */
TEST_CASE("Descriptor list keys: a list key with too few values is refused "
          "naming the line",
          "[D_mJoin][list_keys]") {
  PeriodicTable::initialize();
  {
    Context c;
    c.add("VERBOSE", 0);
    c.add("INIT2B", "true");
    c.add("TYPE2B", "D2_BP 3 3 Ta Ta");
    c.add("RCTYPE2B", "Cut_Cos");
    c.add("RCUT2B", 4.5);
    c.add("CGRID2B", "2.5 3.5");
    c.add("SGRID2B", "0.5 0.5 0.5");
    CHECK_THROWS_WITH(D_mJoin::expand_grids(c),
                      Contains("TYPE2B D2_BP takes 3 CGRID2B values, but CGRID2B has "
                               "2 values: TYPE2B D2_BP 3 3 Ta Ta"));
  }
  {
    Context c;
    c.add("VERBOSE", 0);
    c.add("INIT2B", "true");
    c.add("TYPE2B", "D2_mJoin");
    c.add("TYPE2B", "D2_BP 2 2 Ta Ta");
    c.add("TYPE2B", "D2_BP 2 2 Ta Ta");
    c.add("RCTYPE2B", "Cut_Cos Cut_Cos");
    c.add("RCUT2B", "4.5 4.5");
    c.add("CGRID2B", "2.5 3.5 4.0");
    c.add("SGRID2B", "0.5 0.5 0.5 0.5");
    CHECK_THROWS_WITH(D_mJoin::expand_grids(c),
                      Contains("TYPE2B D2_BP (descriptor 2 of 2) takes 2 CGRID2B "
                               "values, but CGRID2B has 1 value left of 3 (the "
                               "descriptors before it take 2): TYPE2B D2_BP 2 2 Ta "
                               "Ta"));
  }
  for (const auto &label : descriptors_with_list_keys()) {
    const LineSpec s = spec_for(label, 2);
    for (const auto &key : s.list_keys) {
      Context c = context_for(s, {key});
      c.add(key, 3);
      std::string what;
      try {
        D_mJoin::expand_grids(c);
      } catch (const std::exception &e) {
        what = e.what();
      }
      INFO(s.type_key << " " << s.line << " with 1 " << key << " value: '" << what << "'");
      CHECK_THAT(what, Contains(s.type_key + " " + s.line));
      CHECK_THAT(what, Contains("takes 2 " + key + " values, but " + key + " has 1 value"));
    }
  }
}

/* A list key with more values than the descriptors of its TYPE line take is
 * refused, as one with fewer is, naming the line, the key, the values the
 * line takes and the values there are:
 *  - TYPEMB DM_mBlipEnv 0 1 1 7 7 Ta Ta with CGRIDMB 2.88 3.0: "TYPEMB
 *    DM_mBlipEnv takes 1 CGRIDMB value, but CGRIDMB has 2 values: TYPEMB
 *    DM_mBlipEnv 0 1 1 7 7 Ta Ta";
 *  - a D2_mJoin of two D2_BP 2 2 Ta Ta lines with 5 CGRID2B values:
 *    "TYPE2B names 2 descriptors (D2_BP, D2_BP), which take 4 CGRID2B
 *    values, but CGRID2B has 5 values: each descriptor takes its CGRID2B
 *    values, in the order of the TYPE2B lines";
 *  - a SIZE of 0 with values: D2_Knot5 2 0 Ta Ta with KDEG2B 3;
 *  - every descriptor with list keys (each SIZE 2), each key in turn with
 *    3 values;
 *  - more RCTYPE2B or RCUT2B values than descriptors: "TYPE2B names 1
 *    descriptor (D2_BP) but RCUT2B has 2 values: each descriptor takes one
 *    RCUT2B value, in the order of the TYPE2B lines" (its message for
 *    fewer values said "give one RCUT2B value per descriptor").
 * Before the fix: each was split without an error; the extra values were
 * dropped (`tadah train` trained with CGRIDMB 2.88 and wrote CGRIDMB 2.88
 * to the potential) and the extra cutoff values were kept but never read. */
TEST_CASE("Descriptor list keys: a list key with more values than the line "
          "takes is refused naming the line",
          "[D_mJoin][list_keys]") {
  PeriodicTable::initialize();
  const auto refusal = [](Context c) {
    std::string what;
    try {
      D_mJoin::expand_grids(c);
    } catch (const std::exception &e) {
      what = e.what();
    }
    return what;
  };
  {
    Context c;
    c.add("VERBOSE", 0);
    c.add("INITMB", "true");
    c.add("TYPEMB", "DM_mBlipEnv 0 1 1 7 7 Ta Ta");
    c.add("RCTYPEMB", "Cut_Cos");
    c.add("RCUTMB", 5.0);
    c.add("CGRIDMB", "2.88 3.0");
    c.add("SGRIDMB", "0.2");
    c.add("CEMBFUNC", "1 2 3 4 5 6 7");
    c.add("SEMBFUNC", "1 1 1 1 1 1 1");
    CHECK(refusal(c) ==
          "TYPEMB DM_mBlipEnv takes 1 CGRIDMB value, but CGRIDMB has 2 "
          "values: TYPEMB DM_mBlipEnv 0 1 1 7 7 Ta Ta");
  }
  {
    Context c;
    c.add("VERBOSE", 0);
    c.add("INIT2B", "true");
    c.add("TYPE2B", "D2_mJoin");
    c.add("TYPE2B", "D2_BP 2 2 Ta Ta");
    c.add("TYPE2B", "D2_BP 2 2 Ta Ta");
    c.add("RCTYPE2B", "Cut_Cos Cut_Cos");
    c.add("RCUT2B", "4.5 4.5");
    c.add("CGRID2B", "2.5 3.5 4.0 4.2 4.4");
    c.add("SGRID2B", "0.5 0.5 0.5 0.5");
    CHECK(refusal(c) ==
          "TYPE2B names 2 descriptors (D2_BP, D2_BP), which take 4 CGRID2B "
          "values, but CGRID2B has 5 values: each descriptor takes its "
          "CGRID2B values, in the order of the TYPE2B lines");
  }
  {
    Context c;
    c.add("VERBOSE", 0);
    c.add("INIT2B", "true");
    c.add("RCTYPE2B", "Cut_Cos");
    c.add("RCUT2B", 4.5);
    c.add("TYPE2B", "D2_Knot5 2 0 Ta Ta");
    c.add("CGRID2B", "3.0 4.5");
    c.add("KDEG2B", "3");
    CHECK(refusal(c) ==
          "TYPE2B D2_Knot5 takes 0 KDEG2B values, but KDEG2B has 1 value: "
          "TYPE2B D2_Knot5 2 0 Ta Ta");
  }
  for (const auto &label : descriptors_with_list_keys()) {
    const LineSpec s = spec_for(label, 2);
    for (const auto &key : s.list_keys) {
      Context c = context_for(s, {});
      c.add(key, 3);
      const std::string what = refusal(c);
      INFO(s.type_key << " " << s.line << " with 3 " << key << " values: '"
                      << what << "'");
      CHECK_THAT(what, Contains(s.type_key + " " + s.line));
      CHECK_THAT(what, Contains("takes 2 " + key + " values, but " + key +
                                " has 3 values"));
    }
  }
  for (const std::string key : {"RCTYPE2B", "RCUT2B"}) {
    INFO(key);
    Context c;
    c.add("VERBOSE", 0);
    c.add("INIT2B", "true");
    c.add("TYPE2B", "D2_BP 2 2 Ta Ta");
    c.add("RCTYPE2B", key == "RCTYPE2B" ? "Cut_Cos Cut_Cos" : "Cut_Cos");
    c.add("RCUT2B", key == "RCUT2B" ? "4.5 5.0" : "4.5");
    c.add("CGRID2B", "2.5 3.5");
    c.add("SGRID2B", "0.5 0.5");
    CHECK(refusal(c) == "TYPE2B names 1 descriptor (D2_BP) but " + key +
                            " has 2 values: each descriptor takes one " +
                            key + " value, in the order of the TYPE2B "
                            "lines");
  }
}
