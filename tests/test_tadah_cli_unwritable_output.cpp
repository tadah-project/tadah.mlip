// test_tadah_cli_unwritable_output.cpp
//
// An output file that cannot be written makes `tadah` exit non-zero. The
// potential writer of `tadah train` logged an ERROR and returned, so a run
// with `-o no_such_dir/pot.tadah` exited 0 and wrote no potential.

#include "catch2/catch.hpp"
#include <tadah.mlip/cli/tadah_cli.h>

#include <exception>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

namespace {

/* A scratch working directory for the run, removed with its files. */
struct ScratchCwd {
  std::filesystem::path dir, old;
  ScratchCwd()
      : dir(std::filesystem::temp_directory_path() /
            ("tadah_cli_unwritable_" + std::to_string(::getpid()))),
        old(std::filesystem::current_path()) {
    std::filesystem::create_directories(dir);
    std::filesystem::current_path(dir);
  }
  ~ScratchCwd() {
    std::error_code ec;
    std::filesystem::current_path(old, ec);
    std::filesystem::remove_all(dir, ec);
  }
};

}  // namespace

TEST_CASE("Subcommand: train: an OUTFILE that cannot be written yields a "
          "non-zero status",
          "[TadahCLI][exit-status]") {
  ScratchCwd cwd;
  {
    std::ofstream db("train.db");
    db << "test_bcc_ta\n1.0 1.0 1.0\n-0.001 0.0\n"
          "3.301 0.0 0.0\n0.0 3.301 0.0\n0.0 0.0 6.602\n"
          "0.0 0.0 0.0\n0.0 0.0 0.0\n0.0 0.0 0.0\n"
          "Ta 0.07 0.0 0.0  0.0 0.0 0.0\n"
          "Ta 1.7205 1.6505 1.6505  0.0 0.0 0.0\n"
          "Ta 0.07 0.0 3.301  0.0 0.0 0.0\n"
          "Ta 1.7205 1.6505 4.9515  0.0 0.0 0.0\n\n";
  }
  {
    std::ofstream os("config");
    os << "DBFILE train.db\nATOM Ta\nWATOM 1\nINIT2B true\nINITMB false\n"
          "TYPE2B D2_BP 5 5 Ta Ta\n"
          "CGRID2B 0.3 0.6 1.0 1.4 1.8\nSGRID2B 2.0 2.7 3.4 4.2 5.0\n"
          "RCTYPE2B Cut_Cos\nRCUT2B 5.0\n"
          "FORCE false\nSTRESS false\nBIAS false\nNORM false\nVERBOSE 0\n"
          "MODEL M_BLR BF_Linear\nLAMBDA 1e-3\nOALGO 1\n";
  }
  auto run = [](const char *outfile) {
    tadah::mlip::cli::TadahCLI cli;
    const char *argv[] = {"tadah", "train", "-c", "config", "-o", outfile};
    int argc = sizeof(argv) / sizeof(argv[0]);
    /* main() maps an escaping exception to status 1: either outcome is a
     * failure signal; only a returned 0 is wrong. */
    int status = 0;
    std::string what;
    try {
      status = cli.run(argc, const_cast<char **>(argv));
    } catch (const std::exception &e) {
      status = 1;
      what = e.what();
    }
    return std::make_pair(status, what);
  };
  const auto bad = run("no_such_dir/pot.tadah");
  CHECK(bad.first != 0);
  CHECK(bad.second.find("no_such_dir/pot.tadah") != std::string::npos);
  CHECK_FALSE(std::filesystem::exists("no_such_dir"));
  // the same run with a writable OUTFILE succeeds
  const auto ok = run("pot.tadah");
  CHECK(ok.first == 0);
  CHECK(std::filesystem::exists("pot.tadah"));
}
