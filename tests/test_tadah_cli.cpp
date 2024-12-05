#include "catch2/catch.hpp"
#include "../bin/tadah_cli.h"

TEST_CASE("help", "[TadahCLI]") {
  TadahCLI cli;

  SECTION("Run help") {
    const char* argv[] = { "tadah", "-h"};
    int argc = sizeof(argv) / sizeof(argv[0]);
    REQUIRE_THROWS_AS(cli.app.parse(argc, const_cast<char**>(argv)), CLI::CallForHelp);
  }

  SECTION("Run no args") {
    const char* argv[] = { "tadah",};
    int argc = sizeof(argv) / sizeof(argv[0]);
    REQUIRE_NOTHROW(cli.app.parse(argc, const_cast<char**>(argv)));
  }
}
TEST_CASE("Subcommand: train", "[TadahCLI]") {
  TadahCLI cli;

  SECTION("Run train subcommand - config file is not specified") {
    const char* argv[] = { "tadah", "train", "-c"};
    int argc = sizeof(argv) / sizeof(argv[0]);
    REQUIRE_THROWS(cli.app.parse(argc, const_cast<char**>(argv)));
  }
  SECTION("Run train subcommand - config file does not exist") {
    const char* argv[] = { "tadah", "train", "-c", "config.yabayaba" };
    int argc = sizeof(argv) / sizeof(argv[0]);
    REQUIRE_THROWS(cli.app.parse(argc, const_cast<char**>(argv)));
  }
}

TEST_CASE("Subcommand: predict", "[TadahCLI]") {
  TadahCLI cli;

  SECTION("Run predict subcommand - config is not specified") {
    const char* argv[] = { "tadah", "predict", "-c"};
    int argc = sizeof(argv) / sizeof(argv[0]);
    REQUIRE_THROWS(cli.app.parse(argc, const_cast<char**>(argv)));
  }
  SECTION("Run predict subcommand - config file does not exist") {
    const char* argv[] = { "tadah", "predict", "-c", "config.yabayaba" };
    int argc = sizeof(argv) / sizeof(argv[0]);
    REQUIRE_THROWS(cli.app.parse(argc, const_cast<char**>(argv)));
  }


  SECTION("Run predict subcommand - db file is not specified") {
    const char* argv[] = { "tadah", "predict", "-d"};
    int argc = sizeof(argv) / sizeof(argv[0]);
    REQUIRE_THROWS(cli.app.parse(argc, const_cast<char**>(argv)));
  }
  SECTION("Run predict subcommand -  db file does not exist") {
    const char* argv[] = { "tadah", "predict", "-d", "db.yabayaba" };
    int argc = sizeof(argv) / sizeof(argv[0]);
    REQUIRE_THROWS(cli.app.parse(argc, const_cast<char**>(argv)));
  }


  SECTION("Run predict subcommand - pot file is not specified") {
    const char* argv[] = { "tadah", "predict", "-p"};
    int argc = sizeof(argv) / sizeof(argv[0]);
    REQUIRE_THROWS(cli.app.parse(argc, const_cast<char**>(argv)));
  }
  SECTION("Run predict subcommand -  pot file does not exist") {
    const char* argv[] = { "tadah", "predict", "-p", "pot.yabayaba" };
    int argc = sizeof(argv) / sizeof(argv[0]);
    REQUIRE_THROWS(cli.app.parse(argc, const_cast<char**>(argv)));
  }
}
