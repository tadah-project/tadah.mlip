#include "catch2/catch.hpp"
#include <tadah/core/core_user_config_parser.h>
#include <fstream>
#include <sstream>
#include <filesystem>

using namespace tadah::core;
namespace {
const std::string block =
    "DESCRIPTOR ta DM_Embed\n"
    " PAIRS Ta Ta\n"
    " DENSITY\n  BASIS Blip5\n  FORM NONNEG\n"
    "  CENTRES 1 2\n  WIDTHS 0.4 0.5\n  AMPLITUDES 1 2\n"
    "  CUTOFF\n   TYPE Cut_Poly\n   RADIUS 6\n  ENDCUTOFF\n"
    " ENDDENSITY\n"
    " EMBEDDING\n  BASIS Gaussian\n  CENTRES 0 1\n  ETA 2 2\n"
    " ENDEMBEDDING\nENDDESCRIPTOR\n";
}

TEST_CASE("Structured blocks preserve boundaries and repeatable declarations",
          "[unified-dm][config]") {
  CoreUserConfigParser parser;
  std::istringstream input(block + "DESCRIPTOR tb FutureDescriptor\n"
                                  " UNKNOWN future values\nENDDESCRIPTOR\n");
  auto c = parser.parse(input);
  std::ostringstream out;
  c.write_potential(out, 3);
  REQUIRE(out.str().find("DESCRIPTOR ta DM_Embed") != std::string::npos);
  REQUIRE(out.str().find("DESCRIPTOR tb FutureDescriptor") != std::string::npos);
  REQUIRE(out.str().find("ENDDENSITY") != std::string::npos);
  REQUIRE(out.str().find("UNKNOWN future values") != std::string::npos);
  Context transported;
  transported.deserialize(c.serialize());
  REQUIRE(transported == c);
  std::istringstream roundtrip(out.str());
  auto reread = parser.parse(roundtrip);
  std::ostringstream written;
  reread.write_potential(written, 3);
  REQUIRE(written.str() == out.str());
}

TEST_CASE("Structured blocks append across CONFIG includes in declaration order",
          "[unified-dm][config]") {
  const auto path = std::filesystem::temp_directory_path() /
                    "tadah-unified-block-include.config";
  { std::ofstream f(path); f << block; }
  struct Cleanup { std::filesystem::path p; ~Cleanup() { std::filesystem::remove(p); } } cleanup{path};
  std::istringstream input("CONFIG " + path.string() +
                          "\nDESCRIPTOR tb Future\nENDDESCRIPTOR\n");
  auto c = CoreUserConfigParser().parse(input);
  std::ostringstream out; out << c;
  REQUIRE(out.str().find("DESCRIPTOR ta") < out.str().find("DESCRIPTOR tb"));
}

TEST_CASE("Structured blocks reject missing and mismatched boundaries",
          "[unified-dm][config]") {
  for (const auto &text : {
      "DESCRIPTOR ta DM_Embed\nDENSITY\nENDDESCRIPTOR\n",
      "DESCRIPTOR ta DM_Embed\nDENSITY\nENDCUTOFF\nENDDESCRIPTOR\n",
      "DESCRIPTOR ta DM_Embed\n", "ENDDESCRIPTOR\n"}) {
    std::istringstream input(text);
    REQUIRE_THROWS(CoreUserConfigParser().parse(input));
  }
}

TEST_CASE("Component radii survive ordinary context updates and copies", "[unified-dm][config]") {
  Context c;c.set_component_cutoffs({6.25,4.0});c.add("VERBOSE",0);
  REQUIRE(c.get<double>("RCUTMAX")==6.25);
  auto candidate=c;candidate.set_component_cutoffs({5.5});candidate.add("NUMERIC",12);
  REQUIRE(candidate.get<double>("RCUTMAX")==5.5);
  REQUIRE(c.get<double>("RCUTMAX")==6.25);
  candidate.set_component_cutoffs({});candidate.add("VERBOSE",1);
  REQUIRE(candidate.get<double>("RCUTMAX")==0);
}

/* Pins: outside a CONFIG include, Context::merge gives the structured blocks
 * of the overriding context the precedence a key has: when ctx2 declares
 * blocks of a kind, the merged context holds ctx2's blocks of that kind and
 * none of ctx1's; when ctx2 has no block of that kind, ctx1's are kept;
 * without overwrite (allowOverwrite false) and across a CONFIG include they
 * are appended, as repeated declarations compose. Before the fix every
 * merge appended, so two contexts that both declared DESCRIPTOR cu failed
 * with "duplicate descriptor ID 'cu'" when merged. */
TEST_CASE("Context::merge gives structured blocks the precedence of a key",
          "[unified-dm][config]") {
  auto parse = [](const std::string &text) {
    std::istringstream in(text);
    return CoreUserConfigParser().parse(in);
  };
  const std::string other =
      "DESCRIPTOR ta DM_Embed\n PAIRS Ta Ta\n DENSITY\n  BASIS Blip5\n"
      "  FORM SIGNED\n  CENTRES 1.5 2\n  WIDTHS 0.4 0.5\n  AMPLITUDES 3 4\n"
      "  CUTOFF\n   TYPE Cut_Poly\n   RADIUS 5\n  ENDCUTOFF\n ENDDENSITY\n"
      " EMBEDDING\n  BASIS Gaussian\n  CENTRES 0 1\n  ETA 2 2\n"
      " ENDEMBEDDING\nENDDESCRIPTOR\n";
  const Context base = parse(block + "VERBOSE 1\n");
  const Context over = parse(other + "VERBOSE 2\n");
  const Context flat = parse("VERBOSE 3\n");

  const Context merged = Context::merge(base, over);
  REQUIRE(merged.blocks("DESCRIPTOR").size() == 1);
  REQUIRE(merged.blocks("DESCRIPTOR")[0].lines == over.blocks("DESCRIPTOR")[0].lines);
  REQUIRE(merged.get<int>("VERBOSE") == 2);

  const Context kept = Context::merge(base, flat);
  REQUIRE(kept.blocks("DESCRIPTOR").size() == 1);
  REQUIRE(kept.blocks("DESCRIPTOR")[0].lines == base.blocks("DESCRIPTOR")[0].lines);
  REQUIRE(kept.get<int>("VERBOSE") == 3);

  REQUIRE(Context::merge(flat, base).blocks("DESCRIPTOR").size() == 1);

  REQUIRE(Context::merge(base, over, false).blocks("DESCRIPTOR").size() == 2);

  // A CONFIG include after a block of the including file composes with it.
  const auto path = std::filesystem::temp_directory_path() /
                    "tadah-unified-block-merge.config";
  { std::ofstream f(path); f << block; }
  struct Cleanup { std::filesystem::path p; ~Cleanup() { std::filesystem::remove(p); } } cleanup{path};
  const Context composed =
      parse("DESCRIPTOR tb Future\nENDDESCRIPTOR\nCONFIG " + path.string() + "\n");
  REQUIRE(composed.blocks("DESCRIPTOR").size() == 2);
  REQUIRE(composed.blocks("DESCRIPTOR")[0].lines[0] == "DESCRIPTOR tb Future");
  REQUIRE(composed.blocks("DESCRIPTOR")[1].lines == base.blocks("DESCRIPTOR")[0].lines);
}
