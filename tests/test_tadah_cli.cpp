#include "catch2/catch.hpp"
// #define private public
#include <tadah.mlip/cli/tadah_cli.h>
// #undef private

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <unistd.h>

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

namespace {

/* A scratch working directory for a run's outputs, removed with its files. */
struct PredictScratchCwd {
  std::filesystem::path dir, old;
  PredictScratchCwd()
      : dir(std::filesystem::temp_directory_path() /
            ("tadah_cli_predict_nn_" + std::to_string(::getpid()))),
        old(std::filesystem::current_path()) {
    std::filesystem::create_directories(dir);
    std::filesystem::current_path(dir);
  }
  ~PredictScratchCwd() {
    std::error_code ec;
    std::filesystem::current_path(old, ec);
    std::filesystem::remove_all(dir, ec);
  }
};

/* The predicted column of a `tadah predict` output file (energy.pred,
 * forces.pred): a header line, then `index true predicted` per line. A
 * value that is not a finite number (nan, inf) does not parse, so its row
 * is missing from the result. */
std::vector<double> predicted_column(const std::string &file) {
  std::ifstream in(file);
  REQUIRE(in.is_open());
  std::vector<double> out;
  std::string line;
  REQUIRE(std::getline(in, line));  // header
  while (std::getline(in, line)) {
    std::istringstream ls(line);
    long index;
    double truth, pred;
    if (ls >> index >> truth >> pred) out.push_back(pred);
  }
  return out;
}

/* One structure of a Tadah! dataset: the n x n x n supercell of the 2-atom
 * bcc cell of lattice constant a (atoms at 0 and (a/2, a/2, a/2)), built
 * here along the cell rows; zero energy, stress and forces. */
std::string bcc_structure(double a, int n) {
  std::ostringstream os;
  os << "bcc Ta a=" << a << " " << n << "x" << n << "x" << n << "\n0.0\n";
  os << std::setprecision(17);
  const double L = n * a;
  os << L << " 0.0 0.0\n0.0 " << L << " 0.0\n0.0 0.0 " << L << "\n";
  os << "0.0 0.0 0.0\n0.0 0.0 0.0\n0.0 0.0 0.0\n";
  for (int x = 0; x < n; ++x)
    for (int y = 0; y < n; ++y)
      for (int z = 0; z < n; ++z)
        for (double f : {0.0, 0.5})
          os << "Ta " << (x + f) * a << " " << (y + f) * a << " "
             << (z + f) * a << " 0.0 0.0 0.0\n";
  os << "\n";
  return os.str();
}

}  // namespace

TEST_CASE("Subcommand: predict: forces vanish on perfect 2-atom bcc cells "
          "thinner than the cutoff",
          "[TadahCLI][predict][nn]") {
  /* BUG-nnfinder-missing-images (2026-09-29): with a two-body potential of
   * cutoff 10 A, `tadah predict -F` printed forces of several eV/A on
   * perfect 2-atom bcc cells, whose forces vanish by inversion symmetry:
   * the neighbour list missed periodic images of the cells thinner than the
   * cutoff (a = 3.41 A, rc/a < 3; a = 2.60 A). The dataset also holds each
   * crystal as a supercell whose edges exceed the cutoff (3x3x3 and
   * 4x4x4): the 2-atom cell must give its energy per atom. The potential is
   * a linear D2_BP model in the format of the published potentials. */
  PredictScratchCwd cwd;
  {
    std::ofstream os("pot.tadah");
    os << "ATOM     Ta\n"
          "BIAS     false\n"
          "CGRID2B     3.0     5.0     7.0     9.0\n"
          "DIMER     false     0     false\n"
          "FORCE     false\n"
          "INIT2B     true\n"
          "INITMB     false\n"
          "MODEL     M_BLR     BF_Linear\n"
          "NORM     false\n"
          "RCTYPE2B     Cut_Cos\n"
          "RCUT2B     10.0\n"
          "SGRID2B     0.5     0.5     0.5     0.5\n"
          "STRESS     false\n"
          "TYPE2B     D2_BP     4     4     Ta     Ta\n"
          "VERBOSE     0\n"
          "WATOM     73\n"
          "WEIGHTS     0.02     -0.01     0.005     0.04\n";
  }
  // the thin cells first, then their supercells (edges 10.23 and 10.4 A)
  const std::array<double, 2> a = {3.41, 2.60};
  const std::array<int, 2> n_super = {3, 4};
  std::vector<size_t> natoms;
  std::vector<std::string> labels;
  auto label_a = [](double x) {
    std::ostringstream os;
    os << x;
    return os.str();
  };
  {
    std::ofstream os("bcc.tadah");
    for (int k = 0; k < 2; ++k) {
      os << bcc_structure(a[k], 1);
      natoms.push_back(2);
      labels.push_back("a = " + label_a(a[k]) + " A, 2 atoms");
    }
    for (int k = 0; k < 2; ++k) {
      REQUIRE(n_super[k] * a[k] >= 10.0);
      os << bcc_structure(a[k], n_super[k]);
      const size_t n = n_super[k];
      natoms.push_back(2 * n * n * n);
      labels.push_back("a = " + label_a(a[k]) + " A, supercell");
    }
  }

  TadahCLI cli;
  const char *argv[] = {"tadah", "predict", "-p", "pot.tadah",
                        "-d", "bcc.tadah", "-F", "-v", "0"};
  int argc = sizeof(argv) / sizeof(argv[0]);
  int status = -1;
  REQUIRE_NOTHROW(status = cli.run(argc, const_cast<char **>(argv)));
  REQUIRE(status == 0);

  const std::vector<double> e = predicted_column("energy.pred");
  const std::vector<double> f = predicted_column("forces.pred");
  REQUIRE(e.size() == natoms.size());
  size_t ncomp = 0;
  for (size_t n : natoms) ncomp += 3 * n;
  REQUIRE(f.size() == ncomp);

  size_t first = 0;
  for (size_t s = 0; s < natoms.size(); ++s) {
    double f_max = 0.0;
    for (size_t c = first; c < first + 3 * natoms[s]; ++c)
      f_max = std::max(f_max, std::fabs(f[c]));
    first += 3 * natoms[s];
    INFO(labels[s] << ": E/N " << e[s] << " eV, largest force component "
                   << f_max << " eV/A");
    CHECK(f_max < 1e-10);
  }
  for (int k = 0; k < 2; ++k) {
    INFO("a = " << a[k] << " A: E/N of the 2-atom cell and of its supercell");
    CHECK(e[k] == Approx(e[2 + k]).epsilon(1e-10));
  }
}

/* Pins: the explain hook of a tadah built without HPO (TadahCLI sets it
 * when TADAH_ENABLE_NFP is off): ALGO and LIB, in any letter case, the keys
 * that name a `tadah hpo` optimiser, are an error that says this tadah has
 * no HPO and how to build one ("tadah explain: ALGO names the optimiser of
 * a `tadah hpo` target, and this tadah is built without HPO
 * (TADAH_ENABLE_NFP OFF): configure it with -DTADAH_ENABLE_NFP=ON"); any
 * other name is not its own (false, nothing written). A build with HPO
 * sets HPO's optimiser help instead (test_tadah_cli_exit_status.cpp).
 * Before the fix: none compiles (explain_without_hpo does not exist), and
 * such a tadah said "tadah explain: no command or configuration key
 * 'ALGO'". */
TEST_CASE("explain_without_hpo: ALGO and LIB say that this tadah has no HPO",
          "[TadahCLI][explain]") {
  for (const char* name : {"ALGO", "algo", "LIB"}) {
    INFO(name);
    std::ostringstream out;
    CHECK_THROWS_WITH(
        explain_without_hpo(name, out),
        Catch::StartsWith(std::string("tadah explain: ") + name +
                          " names the optimiser of a `tadah hpo` target, and "
                          "this tadah is built without HPO (TADAH_ENABLE_NFP "
                          "OFF): configure it with -DTADAH_ENABLE_NFP=ON"));
    CHECK(out.str().empty());
  }
  std::ostringstream out;
  CHECK_FALSE(explain_without_hpo("LD_LBFGS", out));
  CHECK_FALSE(explain_without_hpo("bogus", out));
  CHECK(out.str().empty());
}
