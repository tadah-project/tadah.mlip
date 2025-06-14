#include "catch2/catch.hpp"
// #define private public
#include <tadah.mlip/cli/tadah_cli.h>
// #undef private

using namespace tadah::mlip::cli;

TEST_CASE("help", "[TadahCLI]") {
  TadahCLI cli;

  SECTION("Run help") {
    const char* argv[] = { "tadah", "-h"};
    int argc = sizeof(argv) / sizeof(argv[0]);
    REQUIRE_NOTHROW(cli.run(argc, const_cast<char**>(argv)));
  }

  SECTION("Run no args") {
    const char* argv[] = { "tadah",};
    int argc = sizeof(argv) / sizeof(argv[0]);
    REQUIRE_NOTHROW(cli.run(argc, const_cast<char**>(argv)));
  }
}
TEST_CASE("Subcommand: train", "[TadahCLI]") {
  TadahCLI cli;

  SECTION("Run train subcommand - config file is not specified") {
    const char* argv[] = { "tadah", "train", "-c"};
    int argc = sizeof(argv) / sizeof(argv[0]);
    REQUIRE_NOTHROW(cli.run(argc, const_cast<char**>(argv)));
  }
  SECTION("Run train subcommand - config file does not exist") {
    const char* argv[] = { "tadah", "train", "-c", "config.yabayaba" };
    int argc = sizeof(argv) / sizeof(argv[0]);
    REQUIRE_THROWS(cli.run(argc, const_cast<char**>(argv)));
  }
}

TEST_CASE("Subcommand: predict", "[TadahCLI]") {
  TadahCLI cli;

  SECTION("Run predict subcommand - config is not specified") {
    const char* argv[] = { "tadah", "predict", "-p"};
    int argc = sizeof(argv) / sizeof(argv[0]);
    REQUIRE_NOTHROW(cli.run(argc, const_cast<char**>(argv))); // incomplete command, should show warning
  }
  SECTION("Run predict subcommand - config file does not exist") {
    const char* argv[] = { "tadah", "predict", "-p", "config.yabayaba" };
    int argc = sizeof(argv) / sizeof(argv[0]);
    REQUIRE_NOTHROW(cli.run(argc, const_cast<char**>(argv))); // incomplete command, should show warning
  }


  SECTION("Run predict subcommand -  pot file does not exist") {
    const char* argv[] = { "tadah", "predict", "-p", "pot.yabayaba", "--structure", "mp-42" };
    int argc = sizeof(argv) / sizeof(argv[0]);
    REQUIRE_THROWS(cli.run(argc, const_cast<char**>(argv)));
  }
}
