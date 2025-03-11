#define CATCH_CONFIG_MAIN
#include <tadah/mlip/cli/unified_input.h>
#include "catch2/catch.hpp"
#include <filesystem>
#include <fstream>

TEST_CASE("UnifiedInput: Local file detection", "[UnifiedInput]") {
  std::string test_filename = "temp_test_file.txt";
  std::ofstream ofs(test_filename);
  ofs << "Test content";
  ofs.close();
  
  std::vector<std::string> inputs = { test_filename };
  auto result = UnifiedInput::processInput(inputs, false, false);
  
  REQUIRE(!result.empty());
  REQUIRE(result[0].substr(0,6) == "local:");
  std::filesystem::remove(test_filename);
}

TEST_CASE("UnifiedInput: Online source detection", "[UnifiedInput]") {
  std::vector<std::string> inputs = { "mp:mp-1234" };
  auto result = UnifiedInput::processInput(inputs);
  
  REQUIRE(!result.empty());
  REQUIRE(result[0].substr(0,7) == "online:");
}

TEST_CASE("UnifiedInput: Force local override", "[UnifiedInput]") {
  std::string test_filename = "temp_test_file2.txt";
  std::ofstream ofs(test_filename);
  ofs << "Content";
  ofs.close();
  
  std::vector<std::string> inputs = { "mp:mp-1234" };
  auto result = UnifiedInput::processInput(inputs, true, false);
  
  REQUIRE(result[0].substr(0,6) == "local:");
  std::filesystem::remove(test_filename);
}

