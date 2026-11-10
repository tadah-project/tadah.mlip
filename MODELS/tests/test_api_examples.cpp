// test_api_examples.cpp
//
// The class documentation of DM_mEAD (DM_mRLR, DM_mSQRT, DM_mBlip,
// DM_mBlipEnv), DM_mJoin and D2_mJoin, published in the API reference,
// gives configuration examples. Each example is read here as a training
// configuration is: its grids (values and LIN/GEOM generators) are split
// among the descriptors of its TYPE lines by the counts those lines ask
// for (D_mJoin::expand_grids), and its model is built (DC_Selector).

#include "catch2/catch.hpp"

#include <tadah/core/context.h>
#include <tadah/core/periodic_table.h>
#include <tadah/models/dc_selector.h>
#include <tadah/models/descriptors/d_mjoin.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using tadah::core::Context;

namespace {

/* The configuration lines of each ``` block of the header's comments,
 * without the comment markers and the '#' comments. */
std::vector<std::vector<std::string>> examples_of(const std::string &header) {
  std::ifstream in(std::string(TADAH_MODELS_INCLUDE_DIR) + "/" + header);
  REQUIRE(in.good());
  std::vector<std::vector<std::string>> blocks;
  bool inside = false;
  std::string line;
  while (std::getline(in, line)) {
    std::size_t p = line.find_first_not_of(" \t");
    if (p == std::string::npos || line[p] != '*') continue;
    std::string text = line.substr(p + 1);
    const std::size_t hash = text.find('#');
    if (hash != std::string::npos) text = text.substr(0, hash);
    const std::size_t b = text.find_first_not_of(" \t");
    if (b == std::string::npos) continue;
    text = text.substr(b, text.find_last_not_of(" \t") - b + 1);
    if (text == "```") {
      inside = !inside;
      if (inside) blocks.emplace_back();
      continue;
    }
    if (inside) blocks.back().push_back(text);
  }
  REQUIRE_FALSE(inside);
  return blocks;
}

Context context_of(const std::vector<std::string> &lines) {
  tadah::core::PeriodicTable::initialize();
  Context c;
  c.add("VERBOSE", 0);
  bool two_body = false, many_body = false;
  for (const auto &l : lines) {
    std::istringstream is(l);
    std::string key;
    is >> key;
    std::string value;
    std::getline(is, value);
    c.add(key, value);
    two_body = two_body || key == "TYPE2B";
    many_body = many_body || key == "TYPEMB";
  }
  c.add("INIT2B", two_body ? "true" : "false");
  c.add("INITMB", many_body ? "true" : "false");
  return c;
}

} // namespace

/* Before the fix: the examples gave grids as '-2 7 0.1 10' and
 * '-1 5 0 3.0', generator codes that get_grid does not have (it reads them
 * as the values -2, 7, 0.1, 10), so each such example was refused
 * ('TYPEMB DM_mRLR takes 7 SGRIDMB values, but SGRIDMB has 4 values:
 * ...'). */
TEST_CASE("The configuration examples of the API reference are read and "
          "build their model",
          "[api_examples][list_keys]") {
  const std::vector<std::pair<std::string, std::size_t>> headers = {
      {"tadah/models/descriptors/dm/dm_mEAD.h", 3},
      {"tadah/models/descriptors/dm/dm_mjoin.h", 1},
      {"tadah/models/descriptors/d2/d2_mjoin.h", 1}};
  for (const auto &[header, n] : headers) {
    const auto blocks = examples_of(header);
    CHECK(blocks.size() == n);
    for (std::size_t i = 0; i < blocks.size(); ++i) {
      std::string text;
      for (const auto &l : blocks[i]) text += l + "\n";
      INFO(header << ", example " << i + 1 << ":\n" << text);
      Context c = context_of(blocks[i]);
      std::string what;
      try {
        tadah::models::D_mJoin::expand_grids(c);
        tadah::models::DC_Selector model(c);
        CHECK(model.descriptor_size() > 0);
      } catch (const std::exception &e) {
        what = e.what();
      }
      CHECK(what == "");
    }
  }
}
