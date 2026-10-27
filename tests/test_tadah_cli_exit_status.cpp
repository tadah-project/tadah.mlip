// test_tadah_cli_exit_status.cpp
//
// A task that fails inside its engine must make `tadah` exit non-zero.
// IEngine::run returns a status; the scheduler runs the engines and the CLI
// returns the outcome to main(). An `hpo` task whose training configuration
// has neither a training DBFILE nor an ATOM key is rejected by the HPO
// engine during run() (after its validate() step passed), so the run
// produces nothing; the process must not report success. Likewise a run
// whose best potential cannot be saved: NLopt used to swallow that
// fatal error as a graceful "nlopt failure" and the run exited 0.
//
// The [validation-metric] cases run `tadah hpo` with PC_FRMSE: FORCE and
// STRESS (from -c, the default, or -F/-S) decide only what training fits;
// validation predicts forces because PC_FRMSE asks for them, the journal says
// where FORCE/STRESS came from, and FORCE in the --validation file is refused
// at its line. The [startup] cases run `tadah hpo` with a PRESTAGE refinement
// on NLOPT:GN_DIRECT_L, which does not start at its candidate, and with an
// INNER block that names a global optimiser (no nested global search); each
// is refused at startup, at its line; and with a PLAIN NLOPT:GN_DIRECT_L
// whose INIT and RESTART blocks have no effect on it, each warned about at
// startup, naming its line.
//
// The [data] cases run the `tadah data` tools on small structure files:
// `data convert` on a file that is not DFT output, or that does not exist,
// exits non-zero with a message naming the file (and the recognised
// formats) instead of crashing; `data print -s` and `data write -s` read a
// structure file; `data dedup` needs --threshold, keeps the first
// of each set of equal structures, and matches atoms independently of their
// order and periodically. They need no HPO and run in every build.
//
// The [coincident] cases: `tadah predict`, `tadah train` and `tadah hpo`
// refuse a dataset holding a structure with two atoms at the same position
// before they predict, train or evaluate, naming the structure and the atoms
// (NNFinder's refusal). The predict and train cases run in every build, the
// hpo case needs NFP.
//
// The [index] cases run `tadah data print`, `tadah analysis descriptor` and
// `tadah analysis bfunc` with -i: the indices are 1-based, parsed strictly
// and checked against the number of structures (or basis functions) before
// anything is read; they run in every build.
//
// The [descriptor] case runs `tadah train` with a TYPE2B descriptor whose
// grid keys are not set; it runs in every build.
//
// The [oalgo] cases run `tadah train` with OALGO 4 and OALGO 2 on fewer
// data rows than weights, and on fewer non-zero data rows than weights (a
// structure of priority 0; a dimer's y and z force rows and an isolated
// atom's energy, of priority 1); they run in every build.
//
// The [kernel_line] case runs `tadah train` twice and `tadah predict` in one
// process: each prints the settings line of its kernel; it runs in every
// build.
//
// The [one_command] cases run `tadah` with --task (no option of tadah or of
// a command), with two commands on one command line (refused before either
// runs) and with a command that groups others alone (refused, naming its
// commands); they run in every build.
//
// The [not-built] cases run `tadah hpo`,
// `tadah -h`, `tadah hpo -h` and `tadah explain hpo`: a tadah built without
// TADAH_ENABLE_NFP refuses hpo before any task runs, naming the build
// option, and its help says so; a tadah with NFP does not.
//
// The [refit] cases run `tadah refit` (dense) with a configuration number
// out of its range, which is refused naming the key and the token, and with
// a key of the other REFIT_MODE, which is refused naming the key and the
// mode that reads it (in dense mode in every build; in compact mode with
// NFP and Ceres, and the refusal of compact mode without them).
//
// The [potential record] and [dataset keys] cases run `tadah train`, `tadah
// predict`, `tadah refit` and, with NFP, `tadah hpo`: the training record of
// the potentials they write, the keys that prepared the training data (in the
// record, ignored by predict and by a retrain from a potential), and
// predict's LSCALE.
//
// The [nonlinear] cases run `tadah train`, `tadah predict` and `tadah hpo`
// with every non-linear model (M_KRR Kern_RBF, Kern_Polynomial,
// Kern_Quadratic, Kern_LQ, Kern_Sigmoid; M_BLR BF_Polynomial2), which the design matrix's column-layout check refused,
// and `tadah predict` with a KRR potential whose basis does not fit. The hpo
// case needs NFP.
//
// The [hpo-check] cases run `tadah hpo --check` and `tadah hpo -c` of a
// configuration file with CHECK true: a valid setup prints its summary and
// exits 0
// without an evaluation or a file; a target file with an unknown key, a
// missing dataset and a DUMP or BDUMP directory in a directory that does
// not exist exit non-zero with the message of `tadah hpo`; a run with CHECK
// false, given or by default, writes no CHECK line into its potentials;
// CHECK is a setting of the run: `tadah train` writes no CHECK key line,
// and `tadah hpo -c` of a potential with a CHECK true line runs. They need
// NFP.

#include "tadah_test_skip.hpp"  // before catch2/catch.hpp
#include "catch2/catch.hpp"
#include <tadah.mlip/cli/tadah_cli.h>
#include <tadah/core/key_notice.h>
#include <tadah/core/periodic_table.h>
#include <tadah/mlip/structure.h>
#include <tadah/mlip/structure_db.h>
#include <tadah/mlip/structure_readers/vasp_poscar_reader.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <set>
#include <initializer_list>
#include <iomanip>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <fcntl.h>
#include <unistd.h>
#include <tuple>
#include <vector>

#ifdef TADAH_ENABLE_NFP
#include <tadah/hpo/io_retry.h>
#endif  // TADAH_ENABLE_NFP

namespace {

struct TempFile {
  std::string path;
  TempFile(const std::string& stem, const std::string& body) {
    path = (std::filesystem::temp_directory_path() /
            ("tadah_cli_exit_" + stem + "_" + std::to_string(::getpid()))).string();
    std::ofstream os(path);
    os << body;
  }
  ~TempFile() { std::remove(path.c_str()); }
};

/* A scratch working directory for the run's outputs, restored on exit.
 * Each test case names its own directory (`stem`), and a directory left
 * behind by a crashed process with the same pid is removed first, so no
 * stale best_pot.tadah or journal can be read as this run's output. */
struct ScratchCwd {
  std::filesystem::path dir, old;
  explicit ScratchCwd(const std::string& stem = "run")
      : dir(std::filesystem::temp_directory_path() /
            ("tadah_cli_exit_" + stem + "_" + std::to_string(::getpid()))),
        old(std::filesystem::current_path()) {
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    std::filesystem::current_path(dir);
  }
  ~ScratchCwd() {
    std::error_code ec;
    std::filesystem::current_path(old, ec);
    std::filesystem::remove_all(dir, ec);
  }
};

std::string slurp(const std::filesystem::path& p) {
  std::ifstream in(p);
  std::ostringstream oss;
  oss << in.rdbuf();
  return oss.str();
}

/* ---- helpers of the [validation-metric] cases ------------------------ */

/* Redirects file descriptors 1 and 2 to the files <stem>.stdout and
 * <stem>.stderr in the working directory while it lives. HPOEngine reports
 * an error on std::cerr and the Logger writes to std::cout; redirecting the
 * descriptors (not the streams' rdbuf) also catches C stdio and stays safe
 * when worker threads write. */
struct CaptureFds {
  std::string out_path, err_path;
  int saved_out = -1, saved_err = -1;
  explicit CaptureFds(const std::string& stem)
      : out_path(stem + ".stdout"), err_path(stem + ".stderr") {
    flush_all();
    saved_out = ::dup(STDOUT_FILENO);
    saved_err = ::dup(STDERR_FILENO);
    const int fo = ::open(out_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    const int fe = ::open(err_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (saved_out < 0 || saved_err < 0 || fo < 0 || fe < 0) {
      for (int fd : {saved_out, saved_err, fo, fe})
        if (fd >= 0) ::close(fd);
      throw std::runtime_error("CaptureFds: cannot redirect stdout/stderr");
    }
    ::dup2(fo, STDOUT_FILENO);
    ::dup2(fe, STDERR_FILENO);
    ::close(fo);
    ::close(fe);
  }
  ~CaptureFds() { restore(); }
  void restore() {
    if (saved_out < 0) return;
    flush_all();
    ::dup2(saved_out, STDOUT_FILENO);
    ::dup2(saved_err, STDERR_FILENO);
    ::close(saved_out);
    ::close(saved_err);
    saved_out = saved_err = -1;
  }
  static void flush_all() {
    std::cout.flush();
    std::cerr.flush();
    std::fflush(stdout);
    std::fflush(stderr);
  }
  CaptureFds(const CaptureFds&) = delete;
  CaptureFds& operator=(const CaptureFds&) = delete;
};

struct CliRun {
  int status = 0;
  std::string what;  // an exception that escaped TadahCLI::run
  std::string out;   // captured standard output
  std::string err;   // captured standard error
};

/* Runs `tadah <args...>` in process, in the working directory, capturing
 * its standard output and error. main() maps an escaping exception to
 * status 1, and so does this. */
CliRun run_tadah(std::vector<std::string> args) {
  std::vector<char*> argv;
  for (auto& a : args) argv.push_back(&a[0]);
  argv.push_back(nullptr);
  CliRun r;
  const std::string stem = "cli_" + (args.size() > 1 ? args[1] : "run");
  {
    CaptureFds cap(stem);
    tadah::mlip::cli::TadahCLI cli;
    try {
      r.status = cli.run(static_cast<int>(args.size()), argv.data());
    } catch (const std::exception& e) {
      r.status = 1;
      r.what = e.what();
    }
  }
  r.out = slurp(stem + ".stdout");
  r.err = slurp(stem + ".stderr");
  return r;
}

bool contains(const std::string& s, const std::string& what) {
  return s.find(what) != std::string::npos;
}

/* The values after `key` on the first non-comment line of a potential
 * file whose first token is `key`; empty when there is none.
 * (Outside the TADAH_ENABLE_NFP block: cases of every build use it.) */
std::vector<std::string> pot_key(const std::filesystem::path& p,
                                 const std::string& key) {
  std::ifstream in(p);
  std::string line;
  while (std::getline(in, line)) {
    std::istringstream ss(line);
    std::string k;
    if (!(ss >> k) || k[0] == '#' || k != key) continue;
    std::vector<std::string> v;
    for (std::string t; ss >> t;) v.push_back(t);
    return v;
  }
  return {};
}

#ifdef TADAH_ENABLE_NFP

/* Two rattled 4-atom bcc Ta cells (a = 3.301 A) with nonzero reference
 * forces. The rattle breaks the lattice symmetry, so the pinned model below
 * predicts forces of the same size as the references (RMS about 0.19 eV/A
 * against 0.17 eV/A); in a perfect lattice they would vanish by symmetry
 * and a prediction without forces could not be told apart. */
const char* const kRattledTaDb =
    "rattled_bcc_ta_1\n1.0 1.0 1.0\n-0.30 0.0\n"
    "3.301 0.0 0.0\n0.0 3.301 0.0\n0.0 0.0 6.602\n"
    "0.0 0.0 0.0\n0.0 0.0 0.0\n0.0 0.0 0.0\n"
    "Ta 0.05 -0.03 0.02  0.31 -0.12 0.05\n"
    "Ta 1.70 1.62 1.68  -0.22 0.17 -0.09\n"
    "Ta -0.04 0.06 3.33  0.04 0.21 -0.15\n"
    "Ta 1.61 1.69 4.92  -0.13 -0.26 0.19\n"
    "\n"
    "rattled_bcc_ta_2\n1.0 1.0 1.0\n-0.24 0.0\n"
    "3.301 0.0 0.0\n0.0 3.301 0.0\n0.0 0.0 6.602\n"
    "0.0 0.0 0.0\n0.0 0.0 0.0\n0.0 0.0 0.0\n"
    "Ta 0.10 0.00 -0.05  -0.27 0.08 0.14\n"
    "Ta 1.66 1.58 1.62  0.19 -0.23 0.06\n"
    "Ta 0.02 -0.08 3.27  0.11 0.16 -0.21\n"
    "Ta 1.69 1.71 5.00  -0.03 -0.01 0.01\n"
    "\n";
constexpr std::size_t kValForceComponents = 2 * 4 * 3;

/* Writes, in the working directory: val.db, config.val (DBFILE val.db),
 * config.hpo and the training configuration `cfg_name`. Every weight is
 * pinned (FIXINDEX 1-5) and there is no training DBFILE, so nothing is
 * fitted: FORCE/STRESS cannot change the weights. (Before the fix they
 * also decided what validation predicted.) `switches` holds the
 * FORCE/STRESS lines, if any.
 * The hpotarget has PC_ERMSE + PC_FRMSE and MAXEVAL 1, so the run makes
 * exactly one evaluation, at the seed FIXWEIGHT[1] = -15, and
 * best_pot.tadah is that evaluation's potential. */
void write_frmse_fixture(const std::string& cfg_name,
                         const std::string& switches) {
  { std::ofstream os("val.db"); os << kRattledTaDb; }
  { std::ofstream os("config.val"); os << "DBFILE val.db\n"; }
  {
    std::ofstream os(cfg_name);
    os << "ATOM Ta\nWATOM 1\nINIT2B true\nINITMB false\n"
          "TYPE2B D2_BP 5 5 Ta Ta\n"
          "CGRID2B 0.3 0.6 1.0 1.4 1.8\nSGRID2B 2.0 2.7 3.4 4.2 5.0\n"
          "RCTYPE2B Cut_Cos\nRCUT2B 5.0\n"
          "BIAS false\nNORM false\nVERBOSE 0\n"
          "MODEL M_BLR BF_Linear\nLAMBDA 1e-3\nOALGO 4\n"
          "FIXINDEX 1-5\nFIXWEIGHT -15.0 22.5 -32.5 17.5 -10.0\n"
       << switches;
  }
  {
    std::ofstream os("config.hpo");
    os << "OPTIMIZER\nLIB NLOPT\nALGO LN_SBPLX\nMAXEVAL 1\nENDOPTIMIZER\n"
          "LOSS L2\nPC_ERMSE 0 1\nPC_FRMSE 0 1\n"
          "OPTIM FIXWEIGHT (1) -30.0 0.0\n";
  }
}

/* The cells of column `col` in an HPO table (stats.tadah, loss.tadah):
 * the header is the line starting with "#ITERATION", every other
 * non-comment line is a row. nullopt when the column does not exist. */
std::optional<std::vector<std::string>> table_column(
    const std::filesystem::path& p, const std::string& col) {
  std::ifstream in(p);
  std::string line;
  std::optional<std::size_t> idx;
  std::vector<std::string> cells;
  while (std::getline(in, line)) {
    std::istringstream ss(line);
    std::vector<std::string> tok;
    for (std::string t; ss >> t;) tok.push_back(t);
    if (tok.empty()) continue;
    if (tok[0] == "#ITERATION") {
      for (std::size_t i = 0; i < tok.size(); ++i)
        if (tok[i] == col) idx = i;
      continue;
    }
    if (tok[0][0] == '#' || !idx) continue;
    cells.push_back(*idx < tok.size() ? tok[*idx] : std::string());
  }
  if (!idx) return std::nullopt;
  return cells;
}

/* True for a finite number written as text. Decided on the text, not with
 * std::isfinite: the tests are built with -ffast-math, under which the
 * compiler may assume every double is finite. */
bool is_finite_number(const std::string& s) {
  if (s.empty()) return false;
  for (char c : s)
    if (!std::isdigit(static_cast<unsigned char>(c)) && c != '+' &&
        c != '-' && c != '.' && c != 'e' && c != 'E')
      return false;
  char* end = nullptr;
  std::strtod(s.c_str(), &end);
  return end == s.c_str() + s.size();
}

/* The validation forces predicted by `tadah predict -F` (PredictEngine, a
 * route independent of HPO's validation) for best_pot.tadah, and the FRMSE
 * against the references, computed here from forces.pred:
 * sqrt(sum over components (f_ref - f_pred)^2 / n), the definition of
 * Analytics::calc_f_rmse for one dataset. */
struct PredictedForces {
  CliRun run;
  std::size_t n = 0;
  double frmse = 0, rms_ref = 0, rms_pred = 0;
};
PredictedForces predict_forces_of_best_pot() {
  PredictedForces p;
  p.run = run_tadah({"tadah", "predict", "-F", "-p", "best_pot.tadah",
                     "-d", "val.db", "-o", "energy.pred", "forces.pred"});
  std::ifstream in("forces.pred");
  std::string line;
  double se = 0, sr = 0, sp = 0;
  while (std::getline(in, line)) {
    std::istringstream ss(line);
    std::string idx;
    double ref = 0, pred = 0;
    if (!(ss >> idx) || idx == "Index") continue;  // header
    if (!(ss >> ref >> pred)) continue;
    se += (ref - pred) * (ref - pred);
    sr += ref * ref;
    sp += pred * pred;
    ++p.n;
  }
  if (p.n) {
    p.frmse = std::sqrt(se / p.n);
    p.rms_ref = std::sqrt(sr / p.n);
    p.rms_pred = std::sqrt(sp / p.n);
  }
  return p;
}

/* The text of the journal section `title` ("-- <title> ----..."), up to
 * the next section header; empty when there is no such section. */
std::string journal_section(const std::string& journal,
                            const std::string& title) {
  const std::string head = "\n-- " + title + " ";
  const auto b = journal.find(head);
  if (b == std::string::npos) return {};
  const auto e = journal.find("\n-- ", b + head.size());
  return journal.substr(b + 1, e == std::string::npos ? std::string::npos
                                                       : e - b - 1);
}

/* The value of the "  <key> : <value>" line carrying the key text `key`:
 * the text after the first ':' that follows the key, leading blanks
 * removed. nullopt when no line carries the key. */
std::optional<std::string> kv_value(const std::string& section,
                                    const std::string& key) {
  std::istringstream in(section);
  std::string line;
  while (std::getline(in, line)) {
    const auto k = line.find(key);
    if (k == std::string::npos) continue;
    const auto colon = line.find(':', k + key.size());
    if (colon == std::string::npos) continue;
    std::string v = line.substr(colon + 1);
    const auto first = v.find_first_not_of(' ');
    return first == std::string::npos ? std::string() : v.substr(first);
  }
  return std::nullopt;
}

/* The leading run of letters of `v` ("no" in "no (default)"). */
std::string leading_word(const std::string& v) {
  std::size_t n = 0;
  while (n < v.size() && std::isalpha(static_cast<unsigned char>(v[n]))) ++n;
  return v.substr(0, n);
}

/* True when `s` names "line <n>" (and not, for n = 2, "line 20"). */
bool names_line(const std::string& s, int n) {
  const std::string what = "line " + std::to_string(n);
  for (auto p = s.find(what); p != std::string::npos;
       p = s.find(what, p + 1)) {
    const auto after = p + what.size();
    if (after == s.size() ||
        !std::isdigit(static_cast<unsigned char>(s[after])))
      return true;
  }
  return false;
}

/* Shared checks of a successful PC_FRMSE run in the working directory:
 * one evaluation, its FRMSE in stats.tadah finite and equal to the FRMSE of
 * `tadah predict -F` on best_pot.tadah, and the journal's "Validation
 * predicts" naming energies and forces only. stats.tadah prints with
 * "Output digits 6" (%.6e, 7 significant digits), a relative rounding of at
 * most 5e-7; the two predictions use the same weights (best_pot.tadah keeps
 * them to 18 digits) and differ only by floating-point rounding, so the
 * tolerance is rel 1e-6 (the -F run of this fixture, measured when the
 * case was written: 1.7e-7). */
void check_frmse_measured(const std::string& journal) {
  const auto frmse1 = table_column("stats.tadah", "FRMSE[1]");
  const auto frmse_avg = table_column("stats.tadah", "FRMSE[avg]");
  INFO("stats.tadah:\n" << slurp("stats.tadah"));
  REQUIRE(frmse1);
  REQUIRE(frmse_avg);
  REQUIRE(frmse1->size() == 1);  // MAXEVAL 1
  REQUIRE(frmse_avg->size() == 1);
  REQUIRE(is_finite_number(frmse1->front()));
  REQUIRE(is_finite_number(frmse_avg->front()));

  const PredictedForces ref = predict_forces_of_best_pot();
  INFO("tadah predict status " << ref.run.status << ", stderr:\n"
                               << ref.run.err << ref.run.what);
  REQUIRE(ref.run.status == 0);
  REQUIRE(ref.n == kValForceComponents);
  /* Fixture check: the model's forces are far from zero, so the FRMSE of
   * a prediction without forces (the reference RMS) differs from the true
   * FRMSE by far more than the tolerance. */
  REQUIRE(ref.rms_pred > 0.1);
  REQUIRE(std::fabs(ref.frmse - ref.rms_ref) > 0.1 * ref.rms_ref);

  CHECK(std::strtod(frmse1->front().c_str(), nullptr) ==
        Approx(ref.frmse).epsilon(1e-6));
  CHECK(std::strtod(frmse_avg->front().c_str(), nullptr) ==
        Approx(ref.frmse).epsilon(1e-6));

  const std::string setup = journal_section(journal, "POTENTIAL SETUP");
  const auto predicts = kv_value(setup, "Validation predicts");
  REQUIRE(predicts);
  CHECK(contains(*predicts, "energies"));
  CHECK(contains(*predicts, "forces (PC_FRMSE)"));
  CHECK_FALSE(contains(*predicts, "stresses"));
  CHECK_FALSE(contains(*predicts, "no validation set"));
}

#endif  // TADAH_ENABLE_NFP

/* ---- helpers of the [data] cases ------------------------------------- */

/* Everything a run reported: the escaping exception, stderr and stdout. */
std::string reported(const CliRun& r) {
  return r.what + "\n" + r.err + "\n" + r.out;
}

/* A dataset entry in a 5 A cubic cell with zero energy, stress and forces;
 * `atoms` holds "<symbol> <x> <y> <z>" lines. */
std::string dataset_entry(const std::string& label,
                          const std::vector<std::string>& atoms) {
  std::string s = label + "\n1 1 1\n-1.0 0.0\n5 0 0\n0 5 0\n0 0 5\n"
                          "0 0 0\n0 0 0\n0 0 0\n";
  for (const auto& a : atoms) s += a + " 0 0 0\n";
  return s + "\n";
}

/* The labels of the structures in a dataset file, in order. */
std::vector<std::string> dataset_labels(const std::string& path) {
  tadah::core::PeriodicTable::initialize();
  tadah::mlip::StructureDB db;
  db.add(path);
  std::vector<std::string> labels;
  for (size_t i = 0; i < db.size(); ++i) labels.push_back(db(i).label);
  return labels;
}

/* NaCl in an orthorhombic 4 x 5 x 6 A cell, P 1 with its one symmetry
 * operator written out, so the reading does not depend on how the
 * space-group symbol is looked up: Na at (0, 0, 0) and Cl at
 * (0.5, 0.5, 0.5) -> (2, 2.5, 3) A; volume 120 A^3. */
const char* const kNaClP1Cif =
    "data_NaCl_P1\n"
    "_cell_length_a 4.0\n_cell_length_b 5.0\n_cell_length_c 6.0\n"
    "_cell_angle_alpha 90\n_cell_angle_beta 90\n_cell_angle_gamma 90\n"
    "_symmetry_space_group_name_H-M 'P 1'\n"
    "loop_\n_symmetry_equiv_pos_as_xyz\n'x,y,z'\n"
    "loop_\n_atom_site_label\n_atom_site_type_symbol\n"
    "_atom_site_fract_x\n_atom_site_fract_y\n_atom_site_fract_z\n"
    "Na1 Na 0.0 0.0 0.0\nCl1 Cl 0.5 0.5 0.5\n";

/* The whitespace-separated tokens of the first line of `text` whose first
 * token is `key`; empty when there is none. */
std::vector<std::string> line_tokens(const std::string& text,
                                     const std::string& key) {
  std::istringstream in(text);
  std::string line;
  while (std::getline(in, line)) {
    std::istringstream ss(line);
    std::vector<std::string> tok;
    for (std::string t; ss >> t;) tok.push_back(t);
    if (!tok.empty() && tok[0] == key) return tok;
  }
  return {};
}

}  // namespace

#ifdef TADAH_ENABLE_NFP

TEST_CASE("Subcommand: hpo: a fatal error inside the engine yields a non-zero status",
          "[TadahCLI][exit-status]") {
  /* Passes HPOEngine::validate (INIT2B set) and fails in HPOEngine::run:
   * no DBFILE and no ATOM key -> "the training configuration declares
   * the element set with the ATOM key". */
  TempFile cfg("config", "INIT2B true\nINITMB false\n");
  TempFile tgt("target", "OPTIM WATOM (1) 0.1 5.0\n");

  tadah::mlip::cli::TadahCLI cli;
  const char* argv[] = { "tadah", "hpo", "--hpotarget", tgt.path.c_str(),
                         "-c", cfg.path.c_str() };
  int argc = sizeof(argv) / sizeof(argv[0]);

  /* main() maps an escaping exception to status 1, so either outcome is a
   * failure signal; only a returned 0 is wrong. */
  int status = 0;
  try {
    status = cli.run(argc, const_cast<char**>(argv));
  } catch (const std::exception&) {
    status = 1;
  }
  CHECK(status != 0);
}

TEST_CASE("Subcommand: hpo: a best-potential persistence failure yields a non-zero status",
          "[TadahCLI][exit-status][failure_handling]") {
  /* A complete, fast run: all weights pinned, no training DBFILE, loss from
   * the validation EMAE. Every lock open after the fresh-run reset fails
   * with EROFS -- permanent, so the first publication fails at once (a
   * transient errno would be retried for 30 s first). */
  ScratchCwd cwd;
  const auto dir = cwd.dir;
  {
    std::ofstream db(dir / "val.db");
    db << "test_bcc_ta\n1.0 1.0 1.0\n-0.001 0.0\n"
          "3.301 0.0 0.0\n0.0 3.301 0.0\n0.0 0.0 6.602\n"
          "0.0 0.0 0.0\n0.0 0.0 0.0\n0.0 0.0 0.0\n"
          "Ta 0.07 0.0 0.0  0.0 0.0 0.0\n"
          "Ta 1.7205 1.6505 1.6505  0.0 0.0 0.0\n"
          "Ta 0.07 0.0 3.301  0.0 0.0 0.0\n"
          "Ta 1.7205 1.6505 4.9515  0.0 0.0 0.0\n\n";
  }
  { std::ofstream os(dir / "config.val"); os << "DBFILE " << (dir / "val.db").string() << "\n"; }
  {
    std::ofstream os(dir / "config");
    os << "ATOM Ta\nWATOM 1\nINIT2B true\nINITMB false\n"
          "TYPE2B D2_BP 5 5 Ta Ta\n"
          "CGRID2B 0.3 0.6 1.0 1.4 1.8\nSGRID2B 2.0 2.7 3.4 4.2 5.0\n"
          "RCTYPE2B Cut_Cos\nRCUT2B 5.0\n"
          "FORCE false\nSTRESS false\nBIAS false\nNORM false\nVERBOSE 0\n"
          "MODEL M_BLR BF_Linear\nLAMBDA 1e-3\nOALGO 4\n"
          "FIXINDEX 1-5\nFIXWEIGHT 0.01 0.02 0.03 0.04 0.05\n";
  }
  {
    std::ofstream os(dir / "config.hpo");
    os << "OPTIMIZER\nLIB NLOPT\nALGO LN_SBPLX\nMAXEVAL 3\nENDOPTIMIZER\n"
          "LOSS L2\nPC_EMAE 0 1\nOPTIM FIXWEIGHT (1) -1.0 1.0\n";
  }
  const std::string cfg = (dir / "config").string();
  const std::string tgt = (dir / "config.hpo").string();
  const std::string val = (dir / "config.val").string();

  tadah::hpo::io::testing::ScopedFaults faults(
      "durable.lock_open,EROFS,skip=1,count=always");
  tadah::mlip::cli::TadahCLI cli;
  const char* argv[] = { "tadah", "hpo", "-c", cfg.c_str(),
                         "--hpotarget", tgt.c_str(), "--validation", val.c_str() };
  int argc = sizeof(argv) / sizeof(argv[0]);

  int status = 0;
  try {
    status = cli.run(argc, const_cast<char**>(argv));
  } catch (const std::exception&) {
    status = 1;
  }
  CHECK(status != 0);
  CHECK(faults.injected("durable.lock_open") >= 1);
  const std::string journal = slurp(dir / "hpo_run.log");
  INFO(journal);
  CHECK(journal.find("FAILED -- best-potential persistence") != std::string::npos);
}

/* ---- the [validation-metric] cases -------------------------------------
 *
 * Each of these cases runs `tadah hpo` in this process, as CTest gives it:
 * catch_discover_tests registers every TEST_CASE as its own CTest entry,
 * one test_tadah process each. Two `tadah hpo` runs in one process used to
 * share the per-thread trainer cache (HPO_Host's tls_trainer_cache_): the
 * second run reused the first run's trainer and rewrote only the searched
 * parameters, so run together in one process (for example `test_tadah
 * "[validation-metric]"`) a case that followed another `tadah hpo` run with
 * a different training FORCE failed its best_pot.tadah FORCE check. Every
 * evaluation now trains a model of its own configuration ("Subcommand: a
 * second hpo run in the process evaluates its own configuration" pins
 * it).
 */

/* Pins: PC_FRMSE works with FORCE false written in the -c file. Validation
 * predicts forces because PC_FRMSE asks for them, whatever FORCE the
 * potential was fitted with: exit 0, a finite FRMSE equal to `tadah predict
 * -F` on best_pot.tadah. best_pot.tadah keeps the training FORCE false and
 * STRESS false. The journal's POTENTIAL SETUP says "Fit forces (FORCE) : no
 * (configuration file config_ffalse.tadah)", the same for STRESS, and
 * "Validation predicts : energies, forces (PC_FRMSE)".
 * Before the fix validation predicted with the model's FORCE false, so no
 * forces were predicted and Analytics::calc_f_rmse read past the empty
 * predicted atoms: SIGSEGV at the first evaluation (the test process
 * died). */
TEST_CASE("Subcommand: hpo: PC_FRMSE with FORCE false in the -c file measures "
          "validation forces",
          "[TadahCLI][hpo][validation-metric]") {
  ScratchCwd cwd("frmse_ffalse");
  write_frmse_fixture("config_ffalse.tadah", "FORCE false\nSTRESS false\n");
  const CliRun r = run_tadah({"tadah", "hpo", "-c", "config_ffalse.tadah",
                              "--hpotarget", "config.hpo",
                              "--validation", "config.val"});
  INFO("hpo stderr:\n" << r.err << r.what);
  REQUIRE(r.status == 0);
  REQUIRE(std::filesystem::exists("best_pot.tadah"));
  CHECK(pot_key("best_pot.tadah", "FORCE") == std::vector<std::string>{"false"});
  CHECK(pot_key("best_pot.tadah", "STRESS") == std::vector<std::string>{"false"});

  const std::string journal = slurp("hpo_run.log");
  INFO("hpo_run.log:\n" << journal);
  const std::string setup = journal_section(journal, "POTENTIAL SETUP");
  const auto force = kv_value(setup, "Fit forces (FORCE)");
  REQUIRE(force);
  CHECK(leading_word(*force) == "no");
  CHECK(contains(*force, "(configuration file "));
  CHECK(contains(*force, "config_ffalse.tadah)"));
  const auto stress = kv_value(setup, "Fit stresses (STRESS)");
  REQUIRE(stress);
  CHECK(leading_word(*stress) == "no");
  CHECK(contains(*stress, "(configuration file "));
  CHECK(contains(*stress, "config_ffalse.tadah)"));

  check_frmse_measured(journal);
}

/* Pins: PC_FRMSE works when the -c file has no FORCE (or STRESS) key, so
 * FORCE is the blueprint default false: exit 0, the FRMSE equals `tadah
 * predict -F`, best_pot.tadah says FORCE false and STRESS false, and the
 * journal says "Fit forces (FORCE) : no (default)" and the same for STRESS.
 * Before the fix: the same SIGSEGV as with FORCE false written in the
 * file. */
TEST_CASE("Subcommand: hpo: PC_FRMSE with no FORCE key in the -c file measures "
          "validation forces",
          "[TadahCLI][hpo][validation-metric]") {
  ScratchCwd cwd("frmse_nokey");
  write_frmse_fixture("config_nokey.tadah", "");
  const CliRun r = run_tadah({"tadah", "hpo", "-c", "config_nokey.tadah",
                              "--hpotarget", "config.hpo",
                              "--validation", "config.val"});
  INFO("hpo stderr:\n" << r.err << r.what);
  REQUIRE(r.status == 0);
  REQUIRE(std::filesystem::exists("best_pot.tadah"));
  CHECK(pot_key("best_pot.tadah", "FORCE") == std::vector<std::string>{"false"});
  CHECK(pot_key("best_pot.tadah", "STRESS") == std::vector<std::string>{"false"});

  const std::string journal = slurp("hpo_run.log");
  INFO("hpo_run.log:\n" << journal);
  const std::string setup = journal_section(journal, "POTENTIAL SETUP");
  const auto force = kv_value(setup, "Fit forces (FORCE)");
  REQUIRE(force);
  CHECK(leading_word(*force) == "no");
  CHECK(contains(*force, "(default)"));
  CHECK_FALSE(contains(*force, "configuration file"));
  const auto stress = kv_value(setup, "Fit stresses (STRESS)");
  REQUIRE(stress);
  CHECK(leading_word(*stress) == "no");
  CHECK(contains(*stress, "(default)"));
  CHECK_FALSE(contains(*stress, "configuration file"));

  check_frmse_measured(journal);
}

/* Pins: -F overrides FORCE false in the -c file (training FORCE true, as in
 * `tadah train`), best_pot.tadah says FORCE true and keeps STRESS false, and
 * the journal records the override: "Fit forces (FORCE) : yes (command line
 * -F/--force; overrides FORCE false in config_ffalse.tadah)", while STRESS
 * still comes from the file. The FRMSE equals `tadah predict -F` (all
 * weights pinned, so the same as with FORCE false).
 * Before the fix the run succeeded and the FRMSE was right, but POTENTIAL
 * SETUP had no "Fit forces (FORCE)" line: the REQUIRE on it failed. */
TEST_CASE("Subcommand: hpo: -F overrides FORCE false in the -c file and the "
          "journal records it",
          "[TadahCLI][hpo][validation-metric]") {
  ScratchCwd cwd("frmse_cli_force");
  write_frmse_fixture("config_ffalse.tadah", "FORCE false\nSTRESS false\n");
  const CliRun r = run_tadah({"tadah", "hpo", "-c", "config_ffalse.tadah",
                              "-F", "--hpotarget", "config.hpo",
                              "--validation", "config.val"});
  INFO("hpo stderr:\n" << r.err << r.what);
  REQUIRE(r.status == 0);
  REQUIRE(std::filesystem::exists("best_pot.tadah"));
  CHECK(pot_key("best_pot.tadah", "FORCE") == std::vector<std::string>{"true"});
  CHECK(pot_key("best_pot.tadah", "STRESS") == std::vector<std::string>{"false"});

  const std::string journal = slurp("hpo_run.log");
  INFO("hpo_run.log:\n" << journal);
  const std::string setup = journal_section(journal, "POTENTIAL SETUP");
  const auto force = kv_value(setup, "Fit forces (FORCE)");
  REQUIRE(force);
  CHECK(leading_word(*force) == "yes");
  CHECK(contains(*force, "command line -F/--force"));
  CHECK(contains(*force, "overrides FORCE false in"));
  CHECK(contains(*force, "config_ffalse.tadah"));
  const auto stress = kv_value(setup, "Fit stresses (STRESS)");
  REQUIRE(stress);
  CHECK(leading_word(*stress) == "no");
  CHECK(contains(*stress, "(configuration file "));
  CHECK(contains(*stress, "config_ffalse.tadah)"));
  CHECK_FALSE(contains(*stress, "command line"));

  check_frmse_measured(journal);
}

/* Pins: FORCE in the --validation file is refused (validation follows the
 * metric, so the key has no role there): non-zero exit, no best_pot.tadah,
 * and the refusal (the escaping exception, stderr, stdout or the journal)
 * names FORCE as a word, "not accepted", "line 2" and the file
 * config_force.val. The training configuration has FORCE true, so the run
 * is valid apart from that line.
 * Before the fix the key was silently ignored and the run exited 0. */
TEST_CASE("Subcommand: hpo: FORCE in the --validation file is refused at its "
          "line",
          "[TadahCLI][hpo][validation-metric]") {
  ScratchCwd cwd("frmse_val_force");
  write_frmse_fixture("config_ftrue.tadah", "FORCE true\nSTRESS false\n");
  { std::ofstream os("config_force.val"); os << "DBFILE val.db\nFORCE true\n"; }
  const CliRun r = run_tadah({"tadah", "hpo", "-c", "config_ftrue.tadah",
                              "--hpotarget", "config.hpo",
                              "--validation", "config_force.val"});
  const std::string journal = std::filesystem::exists("hpo_run.log")
                                  ? slurp("hpo_run.log")
                                  : std::string();
  INFO("exception: " << r.what << "\nstderr:\n" << r.err
                     << "\nhpo_run.log:\n" << journal);
  CHECK(r.status != 0);
  CHECK_FALSE(std::filesystem::exists("best_pot.tadah"));

  /* FORCE as a word: not the tail of ZERO_COM_FORCE. */
  auto has_word_force = [](const std::string& s) {
    for (auto p = s.find("FORCE"); p != std::string::npos;
         p = s.find("FORCE", p + 1)) {
      const bool left_ok =
          p == 0 || !(std::isalnum(static_cast<unsigned char>(s[p - 1])) ||
                      s[p - 1] == '_');
      const auto q = p + 5;
      const bool right_ok =
          q == s.size() || !(std::isalnum(static_cast<unsigned char>(s[q])) ||
                             s[q] == '_');
      if (left_ok && right_ok) return true;
    }
    return false;
  };
  auto names_refusal = [&](const std::string& s) {
    return has_word_force(s) && contains(s, "not accepted") &&
           names_line(s, 2) && contains(s, "config_force.val");
  };
  CHECK((names_refusal(r.what) || names_refusal(r.err) ||
         names_refusal(r.out) || names_refusal(journal)));
}

/* Pins: `tadah hpo` with a STRATEGY TYPE PRESTAGE whose refinement is
 * NLOPT:GN_DIRECT_L -- DIRECT evaluates the centre of the OPTIM box first and
 * never reads the candidate it is given -- is refused at startup: a non-zero
 * exit, no best_pot.tadah and no loss.tadah (nothing is evaluated), and the
 * refusal (the escaping exception, stderr, stdout or the journal) names line
 * 14 of config.hpo (ENDOPTIMIZER), says "STRATEGY TYPE PRESTAGE cannot refine
 * with NLOPT:GN_DIRECT_L: it does not start at the point it is given" and
 * lists NLOPT:LN_BOBYQA and DLIB:BFGS among the local optimisers that start
 * at each candidate.
 * Before the fix the run was accepted: it explored, refined with DIRECT
 * from the centre of the box, never at the candidate, and exited 0 with a
 * best_pot.tadah: the checks fail. */
TEST_CASE("Subcommand: hpo: a PRESTAGE refinement on a search that does not "
          "start at its candidate is refused at startup, naming the line",
          "[TadahCLI][hpo][startup]") {
  ScratchCwd cwd("prestage_direct");
  write_frmse_fixture("config.tadah", "FORCE true\nSTRESS false\n");
  {
    std::ofstream os("config.hpo");
    os << "OPTIMIZER\n"               // line 1
          "  STRATEGY\n"
          "    TYPE PRESTAGE\n"
          "    LIB NLOPT\n"
          "    ALGO GN_DIRECT_L\n"    // line 5: the refinement
          "    MAXEVAL 4\n"
          "    EXPLORE\n"
          "      LIB TADAH\n"
          "      ALGO RANDOM\n"
          "      MAXEVAL 3\n"
          "      SEED 1\n"
          "    ENDEXPLORE\n"
          "  ENDSTRATEGY\n"
          "ENDOPTIMIZER\n"            // line 14
          "LOSS L2\nPC_ERMSE 0 1\nPC_FRMSE 0 1\n"
          "OPTIM FIXWEIGHT (1) -30.0 0.0\n";
  }
  const CliRun r = run_tadah({"tadah", "hpo", "-c", "config.tadah",
                              "--hpotarget", "config.hpo",
                              "--validation", "config.val"});
  const std::string journal = std::filesystem::exists("hpo_run.log")
                                  ? slurp("hpo_run.log")
                                  : std::string();
  INFO("exception: " << r.what << "\nstderr:\n" << r.err
                     << "\nhpo_run.log:\n" << journal);
  CHECK(r.status != 0);
  CHECK_FALSE(std::filesystem::exists("best_pot.tadah"));
  CHECK_FALSE(std::filesystem::exists("loss.tadah"));
  auto names_refusal = [&](const std::string& s) {
    return names_line(s, 14) && contains(s, "ENDOPTIMIZER") &&
           contains(s, "STRATEGY TYPE PRESTAGE cannot refine with "
                       "NLOPT:GN_DIRECT_L: it does not start at the point it "
                       "is given") &&
           contains(s, "NLOPT:LN_BOBYQA") && contains(s, "DLIB:BFGS");
  };
  CHECK((names_refusal(r.what) || names_refusal(r.err) ||
         names_refusal(r.out) || names_refusal(journal)));
}

/* Pins: `tadah hpo` with a PLAIN NLOPT:GN_DIRECT_L -- which never reads the
 * point it is given and draws no random numbers -- under an INIT block
 * (line 2, METHOD CONFIG) and an active RESTART block (line 10) runs (exit
 * 0, a best_pot.tadah) and warns about both at startup, once each, on
 * standard error ("[HPO] WARNING: ") and in the journal: "the INIT block
 * on line 2: the STRATEGY-level optimiser NLOPT:GN_DIRECT_L does not start
 * at the point it is given (DIRECT evaluates the centre of the OPTIM box
 * first and then divides the whole box; NLopt never reads that point), so
 * the search never evaluates the block's INIT points. ..." and "the RESTART
 * block on line 10: the STRATEGY-level optimiser NLOPT:GN_DIRECT_L does not
 * start ... and draws no random numbers, so every RESTART round, whatever
 * points it starts from, asks for round 0's points again, which the run's
 * record answers: no round after round 0 evaluates a new point, ...".
 * Before the fix: the run gave neither warning (the checks fail). */
TEST_CASE("Subcommand: hpo: an INIT and a RESTART block on a search that "
          "does not start at its point are warned about at startup, naming "
          "their lines",
          "[TadahCLI][hpo][startup]") {
  ScratchCwd cwd("plain_direct_init_restart");
  write_frmse_fixture("config.tadah", "FORCE true\nSTRESS false\n");
  {
    std::ofstream os("config.hpo");
    os << "OPTIMIZER\n"                // line 1
          "  INIT\n"                   // line 2
          "    METHOD CONFIG\n"
          "  ENDINIT\n"
          "  STRATEGY\n"
          "    TYPE PLAIN\n"
          "    LIB NLOPT\n"
          "    ALGO GN_DIRECT_L\n"
          "    MAXEVAL 6\n"
          "    RESTART\n"              // line 10
          "      AFTER_STAGNATION 2\n"
          "      MAX 1\n"
          "    ENDRESTART\n"
          "  ENDSTRATEGY\n"
          "ENDOPTIMIZER\n"
          "LOSS L2\nPC_ERMSE 0 1\nPC_FRMSE 0 1\n"
          "OPTIM FIXWEIGHT (1) -30.0 0.0\n";
  }
  const CliRun r = run_tadah({"tadah", "hpo", "-c", "config.tadah",
                              "--hpotarget", "config.hpo",
                              "--validation", "config.val"});
  const std::string journal = std::filesystem::exists("hpo_run.log")
                                  ? slurp("hpo_run.log")
                                  : std::string();
  INFO("exception: " << r.what << "\nstderr:\n" << r.err
                     << "\nhpo_run.log:\n" << journal);
  CHECK(r.status == 0);
  CHECK(std::filesystem::exists("best_pot.tadah"));
  const std::string direct =
      "the STRATEGY-level optimiser NLOPT:GN_DIRECT_L does not start at the "
      "point it is given (DIRECT evaluates the centre of the OPTIM box first "
      "and then divides the whole box; NLopt never reads that point)";
  const std::string init = "the INIT block on line 2: " + direct +
                           ", so the search never evaluates the block's INIT "
                           "points.";
  const std::string restart =
      "the RESTART block on line 10: " + direct +
      " and draws no random numbers, so every RESTART round, whatever points "
      "it starts from, asks for round 0's points again, which the run's "
      "record answers: no round after round 0 evaluates a new point";
  const auto count = [](const std::string& text, const std::string& what) {
    std::size_t n = 0;
    for (auto p = text.find(what); p != std::string::npos;
         p = text.find(what, p + what.size()))
      ++n;
    return n;
  };
  CHECK(count(r.err, "[HPO] WARNING: " + init) == 1);
  CHECK(count(r.err, "[HPO] WARNING: " + restart) == 1);
  CHECK(count(journal, init) == 1);
  CHECK(count(journal, restart) == 1);
}

/* Pins: `tadah hpo` with STRATEGY TYPE PLAIN NLOPT:G_MLSL_LDS whose INNER
 * block names NLOPT:GN_CRS2_LM, a global optimiser, is refused at startup (no
 * nested global search): a non-zero exit, no best_pot.tadah and no loss.tadah
 * (nothing is evaluated), and the refusal (the escaping exception, stderr,
 * stdout or the journal) names line 11 of config.hpo (ENDINNER), says "the
 * INNER block of the STRATEGY-level optimiser: NLOPT:G_MLSL_LDS cannot run
 * NLOPT:GN_CRS2_LM as its INNER search: an INNER search is a local search"
 * and lists NLOPT:LN_BOBYQA among the local NLopt algorithms to use.
 * Before the fix the run was accepted: MLSL ran a CRS2 search of the whole
 * box as each of its local searches and the run exited 0 with a
 * best_pot.tadah: the checks fail. */
TEST_CASE("Subcommand: hpo: an INNER block that names a global optimiser is "
          "refused at startup, naming its ENDINNER line",
          "[TadahCLI][hpo][startup]") {
  ScratchCwd cwd("inner_global");
  write_frmse_fixture("config.tadah", "FORCE true\nSTRESS false\n");
  {
    std::ofstream os("config.hpo");
    os << "OPTIMIZER\n"               // line 1
          "  STRATEGY\n"
          "    TYPE PLAIN\n"
          "    LIB NLOPT\n"
          "    ALGO G_MLSL_LDS\n"     // line 5
          "    MAXEVAL 6\n"
          "    INNER\n"
          "      LIB NLOPT\n"
          "      ALGO GN_CRS2_LM\n"   // line 9: the INNER search
          "      MAXEVAL 3\n"
          "    ENDINNER\n"            // line 11
          "  ENDSTRATEGY\n"
          "ENDOPTIMIZER\n"
          "LOSS L2\nPC_ERMSE 0 1\nPC_FRMSE 0 1\n"
          "OPTIM FIXWEIGHT (1) -30.0 0.0\n";
  }
  const CliRun r = run_tadah({"tadah", "hpo", "-c", "config.tadah",
                              "--hpotarget", "config.hpo",
                              "--validation", "config.val"});
  const std::string journal = std::filesystem::exists("hpo_run.log")
                                  ? slurp("hpo_run.log")
                                  : std::string();
  INFO("exception: " << r.what << "\nstderr:\n" << r.err
                     << "\nhpo_run.log:\n" << journal);
  CHECK(r.status != 0);
  CHECK_FALSE(std::filesystem::exists("best_pot.tadah"));
  CHECK_FALSE(std::filesystem::exists("loss.tadah"));
  auto names_refusal = [&](const std::string& s) {
    return names_line(s, 11) && contains(s, "ENDINNER") &&
           contains(s, "the INNER block of the STRATEGY-level optimiser: "
                       "NLOPT:G_MLSL_LDS cannot run NLOPT:GN_CRS2_LM as its "
                       "INNER search: an INNER search is a local search") &&
           contains(s, "NLOPT:LN_BOBYQA");
  };
  CHECK((names_refusal(r.what) || names_refusal(r.err) ||
         names_refusal(r.out) || names_refusal(journal)));
}

#endif  // TADAH_ENABLE_NFP

/* ---- the [data] cases ---------------------------------------------------
 *
 * Inputs are written into a scratch working directory (ScratchCwd), which
 * each case removes. */

/* Pins: `tadah data convert --dft-file <POSCAR>` (a valid structure file,
 * not DFT output) exits non-zero without writing the output file, and the
 * report names the file and the recognised formats OUTCAR, vasprun.xml,
 * .castep, .md and .geom.
 * Before the fix: DatasetReaderSelector returned nullptr ("Unknown type!
 * Returning nullptr.") and DataConvertEngine called parse_data() on it:
 * SIGSEGV (exit 139 from the binary; here the test process died). */
TEST_CASE("Subcommand: data convert: a structure file is refused, naming it "
          "and the recognised formats",
          "[TadahCLI][exit-status][data]") {
  ScratchCwd cwd("convert_poscar");
  {
    std::ofstream os("POSCAR_fe_bcc");
    os << "Fe bcc\n1.0\n2.87 0 0\n0 2.87 0\n0 0 2.87\nFe\n2\nDirect\n"
          "0 0 0\n0.5 0.5 0.5\n";
  }
  const CliRun r = run_tadah({"tadah", "data", "convert", "--dft-file",
                              "POSCAR_fe_bcc", "-o", "out.tadah"});
  const std::string rep = reported(r);
  INFO("status " << r.status << "\n" << rep);
  CHECK(r.status != 0);
  CHECK_FALSE(std::filesystem::exists("out.tadah"));
  CHECK(contains(rep, "POSCAR_fe_bcc"));
  for (const char* fmt : {"OUTCAR", "vasprun.xml", ".castep", ".md", ".geom"})
    CHECK(contains(rep, fmt));
}

/* Pins: `tadah data convert --dft-file <missing file>` exits non-zero
 * without writing the output file, and the report names the file, says it
 * cannot be opened (it holds "open", in any case) and, as for an
 * unrecognised file, names the recognised formats.
 * Before the fix: "Could not open file: ...", then the same nullptr
 * dereference: SIGSEGV. */
TEST_CASE("Subcommand: data convert: a missing file is refused, naming it "
          "and the recognised formats",
          "[TadahCLI][exit-status][data]") {
  ScratchCwd cwd("convert_missing");
  REQUIRE_FALSE(std::filesystem::exists("no_such_file.xyz"));
  const CliRun r = run_tadah({"tadah", "data", "convert", "--dft-file",
                              "no_such_file.xyz", "-o", "out.tadah"});
  const std::string rep = reported(r);
  INFO("status " << r.status << "\n" << rep);
  CHECK(r.status != 0);
  CHECK_FALSE(std::filesystem::exists("out.tadah"));
  CHECK(contains(rep, "no_such_file.xyz"));
  std::string lower = rep;
  for (auto& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  CHECK(contains(lower, "open"));
  for (const char* fmt : {"OUTCAR", "vasprun.xml", ".castep", ".md", ".geom"})
    CHECK(contains(rep, fmt));
}

/* Pins: `tadah data print -s missing.cif`, a path of a CIF name that does
 * not exist, exits 1 with "the structure file missing.cif does not exist"
 * and not with the refusal of a format tadah does not read ("is neither a
 * structure file tadah reads"); `tadah data write -s sub/POSCAR -o x.vasp
 * -f vasp` alike, writing nothing.
 * Before the fix: "missing.cif is neither a structure file tadah reads (CIF,
 * POSCAR or CASTEP .cell) nor an online id (mp-..., cod-..., aflow:... or a
 * NOMAD entry id)": a missing file read as an unknown format. */
TEST_CASE("Subcommand: data print -s of a structure file that does not "
          "exist names it as missing",
          "[TadahCLI][exit-status][data]") {
  ScratchCwd cwd("print_missing");
  REQUIRE_FALSE(std::filesystem::exists("missing.cif"));
  {
    const CliRun r = run_tadah({"tadah", "data", "print", "-s",
                                "missing.cif"});
    const std::string rep = reported(r);
    INFO("status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    CHECK(contains(rep, "the structure file missing.cif does not exist"));
    CHECK_FALSE(contains(rep, "is neither a structure file"));
  }
  {
    const CliRun r = run_tadah({"tadah", "data", "write", "-s", "sub/POSCAR",
                                "-o", "x.vasp", "-f", "vasp"});
    const std::string rep = reported(r);
    INFO("status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    CHECK(contains(rep, "the structure file sub/POSCAR does not exist"));
    CHECK_FALSE(std::filesystem::exists("x.vasp"));
  }
}

/* Pins: `tadah data print -s <cif>` (the help's own example) prints the
 * structure and exits 0: the summary has 2 atoms, the elements Na and Cl,
 * and the volume 4 * 5 * 6 = 120 A^3 (printed with 6 decimals, so checked
 * to 1e-6).
 * Before the fix: DataPrintEngine asked the context for the size of DBFILE,
 * which -s does not set: "Key not found in the Context object: DBFILE",
 * status 1. */
TEST_CASE("Subcommand: data print -s prints a structure read from a CIF file",
          "[TadahCLI][exit-status][data]") {
  ScratchCwd cwd("print_cif");
  { std::ofstream os("nacl_p1.cif"); os << kNaClP1Cif; }
  const CliRun r = run_tadah({"tadah", "data", "print", "-s", "nacl_p1.cif"});
  INFO("status " << r.status << "\n" << reported(r));
  REQUIRE(r.status == 0);
  CHECK_FALSE(contains(reported(r), "Key not found"));

  const auto atoms = line_tokens(r.out, "Atoms");
  REQUIRE(atoms.size() >= 3);
  CHECK(atoms[2] == "2");
  CHECK(std::find(atoms.begin(), atoms.end(), "Na") != atoms.end());
  CHECK(std::find(atoms.begin(), atoms.end(), "Cl") != atoms.end());
  const auto volume = line_tokens(r.out, "Volume");
  REQUIRE(volume.size() >= 3);
  CHECK(std::strtod(volume[2].c_str(), nullptr) == Approx(120.0).margin(1e-6));
}

/* Pins: `tadah data write -s <cif> -o nacl.vasp -f vasp` exits 0 and
 * writes a POSCAR that Tadah's POSCAR reader reads back as the CIF's
 * structure: lattice rows (4, 0, 0), (0, 5, 0), (0, 0, 6), Na at (0, 0, 0)
 * and Cl at (2, 2.5, 3) A (by hand from the fractional coordinates; the
 * file is written with 18 significant digits, so 1e-9 A).
 * Before the fix: DataWriteEngine generated the indices 0-based and then
 * decremented them, so index 0 became SIZE_MAX: "Index i is out of
 * range.", status 1, no file. */
TEST_CASE("Subcommand: data write -s -f vasp writes a CIF structure as a "
          "readable POSCAR",
          "[TadahCLI][exit-status][data]") {
  ScratchCwd cwd("write_cif");
  { std::ofstream os("nacl_p1.cif"); os << kNaClP1Cif; }
  const CliRun r = run_tadah({"tadah", "data", "write", "-s", "nacl_p1.cif",
                              "-o", "nacl.vasp", "-f", "vasp"});
  INFO("status " << r.status << "\n" << reported(r));
  REQUIRE(r.status == 0);
  REQUIRE(std::filesystem::exists("nacl.vasp"));
  INFO("nacl.vasp:\n" << slurp("nacl.vasp"));

  tadah::core::PeriodicTable::initialize();
  tadah::mlip::VaspPoscarReader reader;
  REQUIRE_NOTHROW(reader.read("nacl.vasp"));
  const tadah::mlip::Structure st = reader.getStructure();
  const double cell[3][3] = {{4, 0, 0}, {0, 5, 0}, {0, 0, 6}};
  for (int i = 0; i < 3; ++i)
    for (int j = 0; j < 3; ++j) {
      INFO("cell(" << i << "," << j << ")");
      CHECK(st.cell(i, j) == Approx(cell[i][j]).margin(1e-9));
    }
  REQUIRE(st.natoms() == 2);
  for (const auto& a : st.atoms) {
    INFO("atom " << a.symbol);
    const bool na = a.symbol == "Na";
    CHECK((na || a.symbol == "Cl"));
    CHECK(a.position[0] == Approx(na ? 0.0 : 2.0).margin(1e-9));
    CHECK(a.position[1] == Approx(na ? 0.0 : 2.5).margin(1e-9));
    CHECK(a.position[2] == Approx(na ? 0.0 : 3.0).margin(1e-9));
  }
  CHECK(st.atoms[0].symbol != st.atoms[1].symbol);
}

/* A dataset of `n` one-atom structures labelled S1 ... Sn, in that order. */
void write_numbered_dataset(const std::string& path, int n) {
  std::ofstream os(path);
  for (int k = 1; k <= n; ++k)
    os << dataset_entry("S" + std::to_string(k), {"Na 0 0 0"});
}

std::vector<std::string> numbered_labels(std::initializer_list<int> ks) {
  std::vector<std::string> v;
  for (int k : ks) v.push_back("S" + std::to_string(k));
  return v;
}

/* Pins: `tadah data sample --uniform N` keeps every N-th structure,
 * starting with the first: of S1 ... S5, N = 1 keeps all five, N = 2 S1, S3,
 * S5, N = 3 S1, S4 and N = 5 S1.
 * Before the fix: the loop counter was incremented twice per pass and read
 * after the increment, so the sample started at S2, skipped structures and
 * read one past the last (N = 1: S2, S4 and the structure after S5). */
TEST_CASE("Subcommand: data sample --uniform keeps every N-th structure",
          "[TadahCLI][exit-status][data][sample]") {
  ScratchCwd cwd("sample_uniform");
  write_numbered_dataset("db.tadah", 5);
  const std::vector<std::pair<int, std::vector<std::string>>> cases = {
      {1, numbered_labels({1, 2, 3, 4, 5})},
      {2, numbered_labels({1, 3, 5})},
      {3, numbered_labels({1, 4})},
      {5, numbered_labels({1})}};
  for (const auto& c : cases) {
    INFO("--uniform " << c.first);
    std::filesystem::remove("out.tadah");
    const CliRun r = run_tadah({"tadah", "data", "sample", "-d", "db.tadah", "-o",
                                "out.tadah", "--uniform", std::to_string(c.first)});
    INFO("status " << r.status << "\n" << reported(r));
    REQUIRE(r.status == 0);
    CHECK(dataset_labels("out.tadah") == c.second);
  }
}

/* Pins: `tadah data sample --uniform 0` is refused with exit status 1 and a
 * message naming --uniform, without writing the output; so is a step larger
 * than the number of structures (6 of 5), naming both numbers.
 * Before the fix: --uniform 0 divided by zero (SIGFPE); --uniform 6 said
 * only "Selected number is larger than total number of configurations
 * available." */
TEST_CASE("Subcommand: data sample --uniform 0 is refused",
          "[TadahCLI][exit-status][data][sample]") {
  ScratchCwd cwd("sample_uniform_zero");
  write_numbered_dataset("db.tadah", 5);
  {
    const CliRun r = run_tadah({"tadah", "data", "sample", "-d", "db.tadah", "-o",
                                "out.tadah", "--uniform", "0"});
    const std::string rep = reported(r);
    INFO("status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    CHECK(contains(rep, "--uniform"));
    CHECK_FALSE(std::filesystem::exists("out.tadah"));
  }
  {
    const CliRun r = run_tadah({"tadah", "data", "sample", "-d", "db.tadah", "-o",
                                "out.tadah", "--uniform", "6"});
    const std::string rep = reported(r);
    INFO("status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    CHECK(contains(rep, "--uniform 6"));
    CHECK(contains(rep, "5 structures"));
    CHECK_FALSE(std::filesystem::exists("out.tadah"));
  }
}

/* Pins: `tadah data sample --random N` draws N distinct structures from
 * the whole dataset, each with the same chance, and writes them in their
 * dataset order: of S1 ... S10, 40 draws of 5 each give 5 distinct
 * structures in increasing order, and together every one of the ten (a
 * correct draw misses a given structure in all 40 with probability 2^-40);
 * --random 10 gives all ten in order.
 * Before the fix: only the indices 0 ... N-1 were shuffled, so every draw
 * was S1 ... S5 in some order and S6 ... S10 were never sampled. */
TEST_CASE("Subcommand: data sample --random draws from the whole dataset",
          "[TadahCLI][exit-status][data][sample]") {
  ScratchCwd cwd("sample_random");
  write_numbered_dataset("db.tadah", 10);
  std::set<std::string> seen;
  for (int draw = 0; draw < 40; ++draw) {
    INFO("draw " << draw + 1);
    std::filesystem::remove("out.tadah");
    const CliRun r = run_tadah({"tadah", "data", "sample", "-d", "db.tadah", "-o",
                                "out.tadah", "--random", "5"});
    INFO("status " << r.status << "\n" << reported(r));
    REQUIRE(r.status == 0);
    const std::vector<std::string> labels = dataset_labels("out.tadah");
    REQUIRE(labels.size() == 5);
    std::vector<int> ks;
    for (const auto& l : labels) ks.push_back(std::stoi(l.substr(1)));
    CHECK(std::is_sorted(ks.begin(), ks.end()));
    CHECK(std::adjacent_find(ks.begin(), ks.end()) == ks.end());
    seen.insert(labels.begin(), labels.end());
  }
  CHECK(seen.size() == 10);
  std::filesystem::remove("out.tadah");
  const CliRun r = run_tadah({"tadah", "data", "sample", "-d", "db.tadah", "-o",
                              "out.tadah", "--random", "10"});
  INFO("status " << r.status << "\n" << reported(r));
  REQUIRE(r.status == 0);
  CHECK(dataset_labels("out.tadah") == numbered_labels({1, 2, 3, 4, 5, 6, 7, 8, 9, 10}));
}

/* Pins: `tadah data sample -i` takes 1-based indices: of S1 ... S5, -i 5
 * gives S5 and -i 1-5 all five; -i 0 and -i 6 are refused with exit status
 * 1 and a message naming the index and the 5 structures, without writing
 * the output.
 * Before the fix: the last index and the whole dataset were refused
 * ("Selected index is larger than the number of available structures.",
 * "Number of indices is larger ..."), and index 0 read the structure
 * before the first. */
TEST_CASE("Subcommand: data sample -i takes 1-based indices up to the last structure",
          "[TadahCLI][exit-status][data][sample]") {
  ScratchCwd cwd("sample_index");
  write_numbered_dataset("db.tadah", 5);
  const std::vector<std::pair<std::string, std::vector<std::string>>> ok = {
      {"5", numbered_labels({5})}, {"1-5", numbered_labels({1, 2, 3, 4, 5})}};
  for (const auto& c : ok) {
    INFO("-i " << c.first);
    std::filesystem::remove("out.tadah");
    const CliRun r = run_tadah({"tadah", "data", "sample", "-d", "db.tadah", "-o",
                                "out.tadah", "-i", c.first});
    INFO("status " << r.status << "\n" << reported(r));
    REQUIRE(r.status == 0);
    CHECK(dataset_labels("out.tadah") == c.second);
  }
  for (const char* bad : {"0", "6"}) {
    INFO("-i " << bad);
    std::filesystem::remove("out.tadah");
    const CliRun r = run_tadah({"tadah", "data", "sample", "-d", "db.tadah", "-o",
                                "out.tadah", "-i", bad});
    const std::string rep = reported(r);
    INFO("status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    CHECK(contains(rep, std::string("index ") + bad));
    CHECK(contains(rep, "5 structures"));
    CHECK_FALSE(std::filesystem::exists("out.tadah"));
  }
}

/* Pins: `tadah data sample` needs exactly one selection: without --uniform,
 * --random or --index (the help's former example) it exits 1 naming the
 * three options, "tadah data sample needs one selection: --uniform N
 * (every N-th structure), --random N (N structures at random) or --index
 * <indices>", and writes nothing (two selections, which the command line
 * refuses as excluding each other: test_command_inputs.cpp of MLIP).
 * Before the fix: no selection wrote an empty dataset with exit 0; with two,
 * the first (UNIFORM, then RANDOM, then INDEX) ran and the other was ignored
 * silently. Then the message of no selection ended with a note on the keys
 * of a file, which the check of its end fails on, and began "tadah data
 * sample: say what to sample: ...". */
TEST_CASE("Subcommand: data sample needs exactly one selection",
          "[TadahCLI][exit-status][data][sample]") {
  ScratchCwd cwd("sample_selection");
  write_numbered_dataset("db.tadah", 5);
  {
    const CliRun r = run_tadah({"tadah", "data", "sample", "-d", "db.tadah", "-o", "out.tadah"});
    const std::string rep = reported(r);
    INFO("status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    CHECK(contains(rep, "tadah data sample needs one selection: --uniform N "
                        "(every N-th structure), --random N (N structures at "
                        "random) or --index <indices>\n"));
    CHECK_FALSE(std::filesystem::exists("out.tadah"));
  }
}

/* Pins: `tadah data sample -i` parses every value strictly and checks the
 * bounds before expanding a range, on S1 ... S5:
 *  - a descending range (-i 1,5-3) is refused, naming the segment (a value
 *    that is not an index pattern, like -i x, the command line refuses
 *    already);
 *  - -i 1-10000000 is refused at once, naming 10000000;
 *  - several -i values are one list: -i 1,3 5 gives S1, S3, S5;
 *  - repeated indices are removed: -i 1,1,2 gives S1, S2.
 * Before the fix: 5-3 and the second value were dropped silently (S1, exit
 * 0), and the range was expanded to ten million indices before the check. */
TEST_CASE("Subcommand: data sample -i parses strictly and checks bounds first",
          "[TadahCLI][exit-status][data][sample]") {
  ScratchCwd cwd("sample_index_parse");
  write_numbered_dataset("db.tadah", 5);
  const std::vector<std::pair<std::vector<std::string>, std::string>> bad = {
      {{"1,5-3"}, "'5-3'"}, {{"1-10000000"}, "index 10000000"}};
  for (const auto& b : bad) {
    INFO("-i " << b.first[0]);
    std::filesystem::remove("out.tadah");
    std::vector<std::string> args = {"tadah", "data", "sample", "-d", "db.tadah", "-o",
                                     "out.tadah", "-i"};
    args.insert(args.end(), b.first.begin(), b.first.end());
    const CliRun r = run_tadah(args);
    const std::string rep = reported(r);
    INFO("status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    CHECK(contains(rep, b.second));
    CHECK_FALSE(std::filesystem::exists("out.tadah"));
  }
  const std::vector<std::pair<std::vector<std::string>, std::vector<std::string>>> ok = {
      {{"1,3", "5"}, numbered_labels({1, 3, 5})}, {{"1,1,2"}, numbered_labels({1, 2})}};
  for (const auto& c : ok) {
    INFO("-i " << c.first[0] << (c.first.size() > 1 ? " " + c.first[1] : ""));
    std::filesystem::remove("out.tadah");
    std::vector<std::string> args = {"tadah", "data", "sample", "-d", "db.tadah", "-o",
                                     "out.tadah", "-i"};
    args.insert(args.end(), c.first.begin(), c.first.end());
    const CliRun r = run_tadah(args);
    INFO("status " << r.status << "\n" << reported(r));
    REQUIRE(r.status == 0);
    CHECK(dataset_labels("out.tadah") == c.second);
  }
}

/* Pins: a data command whose output file cannot be written exits 1 with a
 * message naming the file: `tadah data sample -o nodir/out.tadah` (a
 * missing directory).
 * Before the fix: StructureDB::dump_to_file printed 'Error: Could not open
 * file for writing' and returned; the command logged 'Sampled data written
 * to' and exited 0. */
TEST_CASE("Subcommand: data sample to an unwritable file exits 1",
          "[TadahCLI][exit-status][data][sample]") {
  ScratchCwd cwd("sample_unwritable");
  write_numbered_dataset("db.tadah", 5);
  const CliRun r = run_tadah({"tadah", "data", "sample", "-d", "db.tadah", "-o",
                              "nodir/out.tadah", "--uniform", "2"});
  const std::string rep = reported(r);
  INFO("status " << r.status << "\n" << rep);
  CHECK(r.status == 1);
  CHECK(contains(rep, "nodir/out.tadah"));
  CHECK_FALSE(std::filesystem::exists("nodir"));
}

/* Pins: the index list of `tadah data sample -i` is one list however it
 * is spaced, as the command-line schema accepts it: on S1 ... S5, '-i "1, 3"' and '-i 1, 3' give S1, S3,
 * '-i "1 - 3"' S1, S2, S3, '-i "1-5 : 2"' S1, S3, S5, and '-i "1, 3, 5"'
 * S1, S3, S5.
 * Before the fix: each command-line value was parsed on its own, so '1,'
 * left an empty segment and '-' or ':' a segment of its own, all refused
 * ("index '' is not an index"). */
TEST_CASE("Subcommand: data sample -i accepts the spacing the schema allows",
          "[TadahCLI][exit-status][data][sample]") {
  ScratchCwd cwd("sample_index_spacing");
  write_numbered_dataset("db.tadah", 5);
  const std::vector<std::pair<std::vector<std::string>, std::vector<std::string>>> cases = {
      {{"1, 3"}, numbered_labels({1, 3})},
      {{"1,", "3"}, numbered_labels({1, 3})},
      {{"1 - 3"}, numbered_labels({1, 2, 3})},
      {{"1-5 : 2"}, numbered_labels({1, 3, 5})},
      {{"1, 3, 5"}, numbered_labels({1, 3, 5})}};
  for (const auto& c : cases) {
    std::string shown;
    for (const auto& v : c.first) shown += " '" + v + "'";
    INFO("-i" << shown);
    std::filesystem::remove("out.tadah");
    std::vector<std::string> args = {"tadah", "data", "sample", "-d", "db.tadah", "-o",
                                     "out.tadah", "-i"};
    args.insert(args.end(), c.first.begin(), c.first.end());
    const CliRun r = run_tadah(args);
    INFO("status " << r.status << "\n" << reported(r));
    REQUIRE(r.status == 0);
    CHECK(dataset_labels("out.tadah") == c.second);
  }
}

/* Pins: `tadah data balance -i` takes the same strict 1-based list as data
 * sample: on S1 ... S5 (energy -1 eV each), '--eshift 1.0 -i 5' shifts only
 * S5 (the last structure) to -2 eV; '-i 0' and '-i 5-3' are refused with
 * exit 1, naming the index, without writing the output.
 * Before the fix: '-i 5' was refused ("Index out of range."), '-i 0' wrote
 * far out of bounds (element SIZE_MAX of a vector<bool>: undefined
 * behaviour, a crash of the test executable) and '-i 5-3' silently selected
 * nothing. */
TEST_CASE("Subcommand: data balance -i takes 1-based indices up to the last structure",
          "[TadahCLI][exit-status][data][balance]") {
  ScratchCwd cwd("balance_index");
  write_numbered_dataset("db.tadah", 5);
  {
    const CliRun r = run_tadah({"tadah", "data", "balance", "-d", "db.tadah", "-o",
                                "out.tadah", "--eshift", "1.0", "-i", "5"});
    INFO("status " << r.status << "\n" << reported(r));
    REQUIRE(r.status == 0);
    tadah::core::PeriodicTable::initialize();
    tadah::mlip::StructureDB db;
    db.add("out.tadah");
    REQUIRE(db.size() == 5);
    for (size_t k = 0; k < 4; ++k) CHECK(db(k).energy == Approx(-1.0));
    CHECK(db(4).energy == Approx(-2.0));
  }
  for (const auto& bad : std::vector<std::pair<std::string, std::string>>{
           {"0", "index 0"}, {"5-3", "'5-3'"}}) {
    INFO("-i " << bad.first);
    std::filesystem::remove("out.tadah");
    const CliRun r = run_tadah({"tadah", "data", "balance", "-d", "db.tadah", "-o",
                                "out.tadah", "--eshift", "1.0", "-i", bad.first});
    const std::string rep = reported(r);
    INFO("status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    CHECK(contains(rep, bad.second));
    CHECK_FALSE(std::filesystem::exists("out.tadah"));
  }
}

/* Pins: `tadah data write` exits 1, naming the file, when the structure file
 * cannot be written (-f castep, vasp and lammps to /dev/full, which refuses
 * every write). Skipped where /dev/full does not exist.
 * Before the fix: the CASTEP cell, POSCAR and LAMMPS writers checked only
 * that the file opened, so data write exited 0 with nothing written. */
TEST_CASE("Subcommand: data write to a file that cannot be written exits 1",
          "[TadahCLI][exit-status][data]") {
  if (!std::filesystem::exists("/dev/full")) TADAH_SKIP("/dev/full does not exist here");
  ScratchCwd cwd("write_full");
  write_numbered_dataset("db.tadah", 2);
  for (const char* format : {"castep", "vasp", "lammps"}) {
    INFO("-f " << format);
    const CliRun r = run_tadah({"tadah", "data", "write", "-d", "db.tadah", "-o", "/dev/full",
                                "-f", format, "-i", "1"});
    const std::string rep = reported(r);
    INFO("status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    CHECK(contains(rep, "/dev/full"));
  }
}

/* Pins: a data command with several output files checks that all of them can
 * be written before it writes any: `tadah data split -o p1.tadah
 * nodir/p2.tadah --even --append` (nodir missing) exits 1 naming
 * nodir/p2.tadah and leaves p1.tadah unwritten.
 * Before the fix: p1.tadah received its half before the second file failed,
 * so a rerun after creating nodir appended that half twice. */
TEST_CASE("Subcommand: data split checks every output before writing any",
          "[TadahCLI][exit-status][data]") {
  ScratchCwd cwd("split_outputs");
  write_numbered_dataset("db.tadah", 4);
  const CliRun r = run_tadah({"tadah", "data", "split", "-d", "db.tadah", "-o", "p1.tadah",
                              "nodir/p2.tadah", "--even", "--append"});
  const std::string rep = reported(r);
  INFO("status " << r.status << "\n" << rep);
  CHECK(r.status == 1);
  CHECK(contains(rep, "nodir/p2.tadah"));
  CHECK_FALSE(std::filesystem::exists("p1.tadah"));
}

/* Pins: data dedup, balance and write, which also write several files,
 * check them all before writing any: with a second output in a missing
 * directory (nodir), each exits 1 naming nodir/... and leaves the first
 * output unwritten:
 *  - `data dedup -d a.tadah b.tadah -o o1.tadah nodir/o2.tadah --threshold 1e-6`;
 *  - `data balance -d a.tadah b.tadah -o o1.tadah nodir/o2.tadah --eshift 1.0`;
 *  - `data write -d a.tadah -i 1,2 -o o1.cell nodir/o2.cell -f castep`.
 * Before the fix: each wrote o1 before the second output failed. */
TEST_CASE("Subcommand: data dedup, balance and write check every output before writing any",
          "[TadahCLI][exit-status][data]") {
  ScratchCwd cwd("multi_outputs");
  write_numbered_dataset("a.tadah", 2);
  { std::ofstream os("b.tadah"); os << dataset_entry("T1", {"Na 1 1 1"}); }
  const std::vector<std::pair<std::vector<std::string>, std::string>> runs = {
      {{"tadah", "data", "dedup", "-d", "a.tadah", "b.tadah", "-o", "o1.tadah",
        "nodir/o2.tadah", "--threshold", "1e-6"}, "o1.tadah"},
      {{"tadah", "data", "balance", "-d", "a.tadah", "b.tadah", "-o", "o1.tadah",
        "nodir/o2.tadah", "--eshift", "1.0"}, "o1.tadah"},
      {{"tadah", "data", "write", "-d", "a.tadah", "-i", "1,2", "-o", "o1.cell",
        "nodir/o2.cell", "-f", "castep"}, "o1.cell"}};
  for (const auto& run : runs) {
    INFO(run.first[2]);
    const CliRun r = run_tadah(run.first);
    const std::string rep = reported(r);
    INFO("status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    CHECK(contains(rep, "nodir/o2"));
    CHECK_FALSE(std::filesystem::exists(run.second));
  }
}

/* Pins: `tadah data write -i` takes the strict 1-based list of data sample
 * and checks every selected structure before writing any file: on S1 ... S3,
 *  - `-i 1 3 -o a.cell b.cell` (two values, one list) writes S1 and S3;
 *  - `-i 1,4 -o a.cell b.cell` and `-i 0 -o a.cell` exit 1 naming the index
 *    (index 4, index 0), with nothing written;
 *  - with S2's cell singular, `-f vasp -i 1,2 -o a.vasp b.vasp` exits 1
 *    naming the singular cell, with a.vasp not written.
 * Before the fix: the two values were read as one malformed entry, '1 3', and
 * dropped ("Number of OUTFILE must be equal to the number of INDEX"); index 4
 * and the singular cell were refused only after a.cell or a.vasp had been
 * written; index 0 became the largest size_t ("Index i is out of range."). */
TEST_CASE("Subcommand: data write checks every index and structure before writing any",
          "[TadahCLI][exit-status][data]") {
  ScratchCwd cwd("write_indices");
  write_numbered_dataset("db.tadah", 3);
  {
    const CliRun r = run_tadah({"tadah", "data", "write", "-d", "db.tadah", "-i", "1", "3",
                                "-o", "a.cell", "b.cell", "-f", "castep"});
    INFO("status " << r.status << "\n" << reported(r));
    CHECK(r.status == 0);
    CHECK(contains(slurp("a.cell"), "S1"));
    CHECK(contains(slurp("b.cell"), "S3"));
  }
  const std::vector<std::pair<std::vector<std::string>, std::string>> bad = {
      {{"tadah", "data", "write", "-d", "db.tadah", "-i", "1,4", "-o", "c.cell", "d.cell",
        "-f", "castep"}, "index 4"},
      {{"tadah", "data", "write", "-d", "db.tadah", "-i", "0", "-o", "c.cell", "-f", "castep"},
       "index 0"}};
  for (const auto& b : bad) {
    INFO(b.second);
    const CliRun r = run_tadah(b.first);
    const std::string rep = reported(r);
    INFO("status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    CHECK(contains(rep, b.second));
    CHECK_FALSE(std::filesystem::exists("c.cell"));
  }
  {
    std::ofstream os("singular.tadah");
    os << dataset_entry("S1", {"Na 0 0 0"});
    os << "S2\n1 1 1\n-1.0 0.0\n5 0 0\n0 5 0\n10 0 0\n0 0 0\n0 0 0\n0 0 0\nNa 0 0 0 0 0 0\n\n";
  }
  const CliRun r = run_tadah({"tadah", "data", "write", "-d", "singular.tadah", "-i", "1,2",
                              "-o", "a.vasp", "b.vasp", "-f", "vasp"});
  const std::string rep = reported(r);
  INFO("status " << r.status << "\n" << rep);
  CHECK(r.status == 1);
  CHECK(contains(rep, "singular cell"));
  CHECK_FALSE(std::filesystem::exists("a.vasp"));
}

/* ---- helpers of the [weight0] cases ------------------------------------ */

/* A linear two-body potential (D2_BP, Ta, cutoff 4.5 A) with fixed weights,
 * in the format of the published potentials. */
const char* const kPairPotential =
    "ATOM     Ta\nBIAS     false\nCGRID2B     2.5     3.0     3.5     4.0\n"
    "DIMER     false     0     false\nFORCE     false\nINIT2B     true\n"
    "INITMB     false\nMODEL     M_BLR     BF_Linear\nNORM     false\n"
    "RCTYPE2B     Cut_Cos\nRCUT2B     4.5\nSGRID2B     0.5     0.5     0.5     0.5\n"
    "STRESS     false\nTYPE2B     D2_BP     4     4     Ta     Ta\nVERBOSE     0\n"
    "WATOM     73\nWEIGHTS     0.02     -0.01     0.005     0.04\n";

/* A two-atom Ta structure in a 5 A cubic cell, with the weights line
 * `weights`, reference forces `f` on every component and reference stress
 * `s` on every entry. */
std::string weighted_entry(const std::string& label, const std::string& weights,
                           const std::string& second_atom, const std::string& f,
                           const std::string& s) {
  std::string e = label + "\n" + weights + "\n-1.0\n5 0 0\n0 5 0\n0 0 5\n";
  for (int r = 0; r < 3; ++r) e += s + " " + s + " " + s + "\n";
  e += "Ta 0 0 0 " + f + " " + f + " " + f + "\n";
  e += "Ta " + second_atom + " " + f + " " + f + " " + f + "\n\n";
  return e;
}

/* The lines of `out` that start with `metric` (e.g. "Force RMSE"). */
std::string metric_line(const std::string& out, const std::string& metric) {
  std::istringstream is(out);
  for (std::string line; std::getline(is, line);)
    if (line.rfind(metric, 0) == 0) return line;
  return "";
}

/* The whitespace-separated fields after the row name `metric` of the line
 * of `out` that starts with it (e.g. "Force RMSE (eV/Å)": the mean, then
 * one value per dataset); none without such a line. */
std::vector<std::string> metric_fields(const std::string& out,
                                       const std::string& metric) {
  const std::string line = metric_line(out, metric);
  std::vector<std::string> f;
  if (line.empty()) return f;
  std::istringstream is(line.substr(metric.size()));
  for (std::string w; is >> w;) f.push_back(w);
  return f;
}

/* Pins: `tadah predict -F -S -A` leaves a structure stored with force and
 * stress priority 0 (priorities line `1 0 0`) out of the force and stress
 * metrics: on S1, W, S3, where W's reference forces (50 eV/A) and stress
 * (0.5 eV/A^3) are far from the prediction, every Force and Stress MAE,
 * RMSE and R^2 line equals the one for S1, S3 alone.
 * Before the fix: W was included, so the force metrics were off by O(10)
 * eV/A and the stress metrics by O(0.1) eV/A^3. */
TEST_CASE("Subcommand: predict -A leaves structures with priority 0 out of "
          "the force and stress metrics",
          "[TadahCLI][predict][weight0]") {
  ScratchCwd cwd("predict_weight0");
  std::ofstream("pot.tadah") << kPairPotential;
  const std::string s1 = weighted_entry("S1", "1 1 1", "1.40 1.30 1.20", "0.1", "0.001");
  const std::string w = weighted_entry("W", "1 0 0", "1.50 1.45 1.35", "50", "0.5");
  const std::string s3 = weighted_entry("S3", "1 1 1", "1.60 1.25 1.30", "-0.1", "0.002");
  std::ofstream("full.tadah") << s1 << w << s3;
  std::ofstream("kept.tadah") << s1 << s3;
  const CliRun full = run_tadah({"tadah", "predict", "-p", "pot.tadah", "-d", "full.tadah",
                                 "-F", "-S", "-A"});
  INFO("full: status " << full.status << "\n" << reported(full));
  REQUIRE(full.status == 0);
  const CliRun kept = run_tadah({"tadah", "predict", "-p", "pot.tadah", "-d", "kept.tadah",
                                 "-F", "-S", "-A"});
  INFO("kept: status " << kept.status << "\n" << reported(kept));
  REQUIRE(kept.status == 0);
  for (const char* metric : {"Force MAE", "Force RMSE", "Force R^2", "Stress MAE",
                             "Stress RMSE", "Stress R^2"}) {
    INFO(metric);
    const std::string line = metric_line(kept.out, metric);
    REQUIRE_FALSE(line.empty());
    CHECK(metric_line(full.out, metric) == line);
  }
}

/* Pins: `tadah predict -A` measures a dataset whose structures all have force
 * priority 0 (stress priority 0) in no force (stress) metric: with -F -A on
 * S.tadah (priorities 1) and nof.tadah (fweight 0) it exits 0, prints nan
 * for nof.tadah in each Force line, a mean equal to S.tadah's value, and a
 * line naming nof.tadah as not measured; the same with -S for nos.tadah
 * (sweight 0). With only the file without the data it exits 1 before
 * predicting, naming it and the priority ("force priority 0", "stress
 * priority 0"). The other priority refuses nothing (nof.tadah -S -A and
 * nos.tadah -F -A, and -A alone, exit 0).
 * Before the fix: the run with both files was refused. (Until the priority
 * wording the refusal said "force weight 0".) */
TEST_CASE("Subcommand: predict -A leaves out a dataset without the data, and "
          "refuses when no dataset has it",
          "[TadahCLI][predict][weight0]") {
  ScratchCwd cwd("predict_all_weight0");
  std::ofstream("pot.tadah") << kPairPotential;
  std::ofstream("S.tadah") << weighted_entry("S1", "1 1 1", "1.40 1.30 1.20", "0.1", "0.001")
                           << weighted_entry("S3", "1 1 1", "1.60 1.25 1.30", "-0.1", "0.002");
  std::ofstream("nof.tadah") << weighted_entry("N1", "1 0 1", "1.40 1.30 1.20", "0", "0.001")
                             << weighted_entry("N2", "1 0 1", "1.60 1.25 1.30", "0", "0.002");
  std::ofstream("nos.tadah") << weighted_entry("N1", "1 1 0", "1.40 1.30 1.20", "0.1", "0")
                             << weighted_entry("N2", "1 1 0", "1.60 1.25 1.30", "-0.1", "0");
  struct Case { std::string file, flag, metric, unit, weight; };
  for (const Case& c : {Case{"nof.tadah", "-F", "Force", " (eV/Å)", "force priority 0"},
                        Case{"nos.tadah", "-S", "Stress", " (GPa)", "stress priority 0"}}) {
    INFO(c.file);
    {
      const CliRun r = run_tadah({"tadah", "predict", "-p", "pot.tadah", "-d", "S.tadah",
                                  c.file, c.flag, "-A"});
      const std::string rep = reported(r);
      INFO("both files: status " << r.status << "\n" << rep);
      CHECK(r.status == 0);
      for (const std::string kind : {" MAE" + c.unit, " RMSE" + c.unit, std::string(" R^2")}) {
        const std::vector<std::string> f = metric_fields(r.out, c.metric + kind);
        INFO(c.metric + kind << ": " << metric_line(r.out, c.metric + kind));
        REQUIRE(f.size() == 3u);  // mean, Val1, Val2
        CHECK(f[0] == f[1]);
        CHECK((f[2] == "nan" || f[2] == "-nan"));  // as printed (no isnan: fast-math)
      }
      CHECK(contains(rep, c.file));
      CHECK(contains(rep, "not measured"));
    }
    {
      std::filesystem::remove("energy.pred");
      const CliRun r = run_tadah({"tadah", "predict", "-p", "pot.tadah", "-d", c.file,
                                  c.flag, "-A"});
      const std::string rep = reported(r);
      INFO("alone: status " << r.status << "\n" << rep);
      CHECK(r.status == 1);
      CHECK(contains(rep, c.file));
      CHECK(contains(rep, c.weight));
      CHECK_FALSE(std::filesystem::exists("energy.pred"));
    }
  }
  // The other priority refuses nothing: nof.tadah has sweight 1 and
  // nos.tadah fweight 1.
  const std::vector<std::tuple<std::string, std::string, std::string>> other = {
      {"nof.tadah", "-S", "Stress RMSE"}, {"nos.tadah", "-F", "Force RMSE"}};
  for (const auto& c : other) {
    INFO(std::get<0>(c) << " " << std::get<1>(c) << " -A");
    const CliRun r = run_tadah({"tadah", "predict", "-p", "pot.tadah", "-d", std::get<0>(c),
                                std::get<1>(c), "-A"});
    INFO("status " << r.status << "\n" << reported(r));
    CHECK(r.status == 0);
    CHECK_FALSE(metric_line(r.out, std::get<2>(c)).empty());
    const CliRun e = run_tadah({"tadah", "predict", "-p", "pot.tadah", "-d", std::get<0>(c), "-A"});
    INFO("-A alone: status " << e.status << "\n" << reported(e));
    CHECK(e.status == 0);
    CHECK_FALSE(metric_line(e.out, "Energy RMSE").empty());
  }
}

/* Pins: `tadah predict -F -A` gives no R^2 for a dataset whose reference
 * values are all equal (no spread for the residual to be measured
 * against): on V.tadah (forces 0.1 and -0.1) and Z.tadah (every force 0),
 * the Force R^2 line has nan for Z.tadah and a mean equal to V.tadah's
 * value, and a note says why: "Note: dataset 2 (Z.tadah): every reference
 * force component has the same value, so its Force R^2 has no value (nan)
 * and is left out of the mean.". The energies per atom of each file are
 * all equal (-0.5 eV), so its Energy R^2 has none either, with its note.
 * Before the fix: Z.tadah's Force R^2 was -inf (1 - SS_res / 0) and so was
 * the mean; the Energy R^2 were -inf. */
TEST_CASE("Subcommand: predict -A gives no R^2 for a dataset whose reference "
          "values are all equal",
          "[TadahCLI][predict][r_sq]") {
  ScratchCwd cwd("predict_r_sq_equal");
  std::ofstream("pot.tadah") << kPairPotential;
  std::ofstream("V.tadah") << weighted_entry("V1", "1 1 1", "1.40 1.30 1.20", "0.1", "0.001")
                           << weighted_entry("V2", "1 1 1", "1.60 1.25 1.30", "-0.1", "0.002");
  std::ofstream("Z.tadah") << weighted_entry("Z1", "1 1 1", "1.40 1.30 1.20", "0", "0.001")
                           << weighted_entry("Z2", "1 1 1", "1.60 1.25 1.30", "0", "0.002");
  const CliRun r = run_tadah({"tadah", "predict", "-p", "pot.tadah", "-d", "V.tadah",
                              "Z.tadah", "-F", "-A"});
  const std::string rep = reported(r);
  INFO("status " << r.status << "\n" << rep);
  REQUIRE(r.status == 0);
  const std::vector<std::string> f = metric_fields(r.out, "Force R^2");
  REQUIRE(f.size() == 3u);  // mean, Val1, Val2
  CHECK((f[1] != "nan" && f[1] != "-nan" && f[1] != "-inf" && f[1] != "inf"));
  CHECK(f[0] == f[1]);
  CHECK((f[2] == "nan" || f[2] == "-nan"));  // as printed (no isnan: fast-math)
  CHECK(contains(r.out, "Note: dataset 2 (Z.tadah): every reference force "
                        "component has the same value, so its Force R^2 has no "
                        "value (nan) and is left out of the mean."));
  CHECK(contains(r.out, "Note: dataset 1 (V.tadah): every reference energy "
                        "per atom has the same value, so its Energy R^2 has no "
                        "value (nan) and is left out of the mean."));
  const std::vector<std::string> e = metric_fields(r.out, "Energy R^2");
  REQUIRE(e.size() == 3u);
  for (const auto& v : e) CHECK((v == "nan" || v == "-nan"));
}

/* The entry of weighted_entry() with the reference energy `energy`. */
std::string entry_with_energy_weight(const std::string& label, const std::string& weights,
                                     const std::string& energy,
                                     const std::string& second_atom) {
  std::string e = weighted_entry(label, weights, second_atom, "0.1", "0.001");
  const std::string from = "\n-1.0\n";
  e.replace(e.find(from), from.size(), "\n" + energy + "\n");
  return e;
}

/* Pins: `tadah predict -A` leaves a structure stored with energy priority 0
 * (priorities line `0 1 1`, a placeholder energy of 1000 eV) out of the
 * energy metrics: on S1, W, S3 every Energy MAE, RMSE and R^2 line equals
 * the one for S1, S3 alone. A dataset whose structures all have energy
 * priority 0 (noe.tadah) is not measured: with S.tadah, each Energy line
 * prints nan for it and a mean equal to S.tadah's value, and a note names
 * it and "energy priority 0"; alone it exits 0 (the energy is predicted
 * whatever the metrics measure).
 * Before the fix: W's 1000 eV were counted, and noe.tadah's placeholders
 * gave numbers. (Until the priority wording the note said "energy weight
 * 0".) */
TEST_CASE("Subcommand: predict -A leaves structures with energy priority 0 "
          "out of the energy metrics",
          "[TadahCLI][predict][weight0]") {
  ScratchCwd cwd("predict_eweight0");
  std::ofstream("pot.tadah") << kPairPotential;
  const std::string s1 = entry_with_energy_weight("S1", "1 1 1", "-1.0", "1.40 1.30 1.20");
  const std::string w = entry_with_energy_weight("W", "0 1 1", "1000.0", "1.50 1.45 1.35");
  const std::string s3 = entry_with_energy_weight("S3", "1 1 1", "-1.2", "1.60 1.25 1.30");
  std::ofstream("full.tadah") << s1 << w << s3;
  std::ofstream("kept.tadah") << s1 << s3;
  std::ofstream("S.tadah") << s1 << s3;
  std::ofstream("noe.tadah")
      << entry_with_energy_weight("N1", "0 1 1", "1000.0", "1.40 1.30 1.20")
      << entry_with_energy_weight("N2", "0 1 1", "2000.0", "1.60 1.25 1.30");
  const CliRun full = run_tadah({"tadah", "predict", "-p", "pot.tadah", "-d", "full.tadah", "-A"});
  INFO("full: status " << full.status << "\n" << reported(full));
  REQUIRE(full.status == 0);
  const CliRun kept = run_tadah({"tadah", "predict", "-p", "pot.tadah", "-d", "kept.tadah", "-A"});
  INFO("kept: status " << kept.status << "\n" << reported(kept));
  REQUIRE(kept.status == 0);
  for (const char* metric : {"Energy MAE", "Energy RMSE", "Energy R^2"}) {
    INFO(metric);
    const std::string line = metric_line(kept.out, metric);
    REQUIRE_FALSE(line.empty());
    CHECK(metric_line(full.out, metric) == line);
  }
  const CliRun both = run_tadah({"tadah", "predict", "-p", "pot.tadah", "-d", "S.tadah",
                                 "noe.tadah", "-A"});
  const std::string rep = reported(both);
  INFO("both files: status " << both.status << "\n" << rep);
  CHECK(both.status == 0);
  for (const std::string kind : {" MAE (meV/atom)", " RMSE (meV/atom)", " R^2"}) {
    const std::vector<std::string> f = metric_fields(both.out, "Energy" + kind);
    INFO("Energy" + kind << ": " << metric_line(both.out, "Energy" + kind));
    REQUIRE(f.size() == 3u);  // mean, Val1, Val2
    CHECK(f[0] == f[1]);
    CHECK((f[2] == "nan" || f[2] == "-nan"));  // as printed (no isnan: fast-math)
  }
  CHECK(contains(rep, "noe.tadah"));
  CHECK(contains(rep, "energy priority 0"));
  const CliRun alone = run_tadah({"tadah", "predict", "-p", "pot.tadah", "-d", "noe.tadah", "-A"});
  INFO("alone: status " << alone.status << "\n" << reported(alone));
  CHECK(alone.status == 0);
}

/* Pins: `tadah predict` does not re-apply the training-only weight knobs a
 * potential records (`tadah train` writes its whole configuration into
 * pot.tadah): with `WDBFILE 1 0` in the potential, `predict -F -A` on two
 * files of weight 1 exits 0 and prints the same force metrics as with a
 * potential without it.
 * Before the fix: WDBFILE multiplied the weights of the second file by 0 at
 * predict time, so its structures were left out of the force metrics and
 * the dataset was refused. */
TEST_CASE("Subcommand: predict ignores the training-only weights a potential records",
          "[TadahCLI][predict][weight0]") {
  ScratchCwd cwd("predict_wdbfile");
  std::ofstream("pot.tadah") << kPairPotential;
  std::ofstream("pot_w.tadah") << kPairPotential << "WDBFILE     1     0\n";
  std::ofstream("a.tadah") << weighted_entry("S1", "1 1 1", "1.40 1.30 1.20", "0.1", "0.001");
  std::ofstream("b.tadah") << weighted_entry("S3", "1 1 1", "1.60 1.25 1.30", "-0.1", "0.002");
  const CliRun plain = run_tadah({"tadah", "predict", "-p", "pot.tadah", "-d", "a.tadah",
                                  "b.tadah", "-F", "-A"});
  INFO("plain: status " << plain.status << "\n" << reported(plain));
  REQUIRE(plain.status == 0);
  const CliRun w = run_tadah({"tadah", "predict", "-p", "pot_w.tadah", "-d", "a.tadah",
                              "b.tadah", "-F", "-A"});
  INFO("WDBFILE: status " << w.status << "\n" << reported(w));
  REQUIRE(w.status == 0);
  for (const char* metric : {"Force MAE", "Force RMSE", "Force R^2"}) {
    INFO(metric);
    CHECK(metric_line(w.out, metric) == metric_line(plain.out, metric));
  }
}

/* The predicted column of a `tadah predict` output file (a header line, then
 * `index true predicted [error]` per line). */
std::vector<double> predicted_values(const std::string& file) {
  std::ifstream in(file);
  std::vector<double> out;
  std::string line;
  std::getline(in, line);  // header
  while (std::getline(in, line)) {
    std::istringstream ls(line);
    double index, truth, pred;
    if (ls >> index >> truth >> pred) out.push_back(pred);
  }
  return out;
}

/* Pins: `tadah predict -A` refuses, before predicting, a dataset that
 * loading leaves without structures (here every structure is dropped by
 * --ffilter 0.01, below its reference forces of 0.1 eV/A), naming the file
 * and saying so, with or without -F.
 * Before the fix: with -F it said every structure had force weight 0 (their
 * fweight is 1); without -F it printed energy metrics of no structure. The
 * check that the message names no priority 0 reads both wordings. */
TEST_CASE("Subcommand: predict -A refuses a dataset left empty by the filters",
          "[TadahCLI][predict][weight0]") {
  ScratchCwd cwd("predict_filtered_empty");
  std::ofstream("pot.tadah") << kPairPotential;
  std::ofstream("hot.tadah") << weighted_entry("H1", "1 1 1", "1.40 1.30 1.20", "0.1", "0.001")
                             << weighted_entry("H2", "1 1 1", "1.60 1.25 1.30", "-0.1", "0.002");
  for (const bool force : {true, false}) {
    INFO("-F " << force);
    std::filesystem::remove("energy.pred");
    std::vector<std::string> args = {"tadah", "predict", "-p", "pot.tadah", "-d", "hot.tadah",
                                     "--ffilter", "0.01", "-A"};
    if (force) args.push_back("-F");
    const CliRun r = run_tadah(args);
    const std::string rep = reported(r);
    INFO("status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    CHECK(contains(rep, "hot.tadah"));
    CHECK(contains(rep, "no structures after loading"));
    CHECK_FALSE(contains(rep, "force weight 0"));
    CHECK_FALSE(contains(rep, "force priority 0"));
    CHECK_FALSE(std::filesystem::exists("energy.pred"));
  }
}

/* Pins: the help example of `tadah data merge`, '-d db1.tadah db2.tadah -o
 * merged.tadah', merges the two files.
 * Before the fix: -d was registered twice (as a key and in the input group),
 * so every command-line merge exited 106 ("Exactly 1 option from
 * [-t,--task,-d,--dbfile] is required"). */
TEST_CASE("Subcommand: data merge merges its -d files",
          "[TadahCLI][exit-status][data]") {
  ScratchCwd cwd("merge_example");
  write_numbered_dataset("db1.tadah", 2);
  { std::ofstream os("db2.tadah"); os << dataset_entry("T1", {"Na 0 0 0"}); }
  const CliRun r = run_tadah({"tadah", "data", "merge", "-d", "db1.tadah", "db2.tadah",
                              "-o", "merged.tadah"});
  INFO("status " << r.status << "\n" << reported(r));
  REQUIRE(r.status == 0);
  CHECK(dataset_labels("merged.tadah") == std::vector<std::string>{"S1", "S2", "T1"});
}

/* Pins: `tadah data dedup` without --threshold (as the help examples were
 * written) exits non-zero without writing the output file, and the report
 * says that --threshold is needed (it names "--threshold"), not the
 * internal "Key not found".
 * Before the fix: "Key not found in the Context object: THRESHOLD",
 * status 1, so the "--threshold" and "Key not found" CHECKs failed. */
TEST_CASE("Subcommand: data dedup without --threshold says it needs "
          "--threshold",
          "[TadahCLI][exit-status][data]") {
  ScratchCwd cwd("dedup_nothreshold");
  {
    std::ofstream os("db.tadah");
    os << dataset_entry("T1", {"Na 0 0 0", "Cl 2.5 2.5 2.5"})
       << dataset_entry("T2", {"Na 0 0 0", "Cl 2.5 2.5 2.5"});
  }
  const CliRun r = run_tadah({"tadah", "data", "dedup", "-d", "db.tadah",
                              "-o", "out.tadah"});
  const std::string rep = reported(r);
  INFO("status " << r.status << "\n" << rep);
  CHECK(r.status != 0);
  CHECK_FALSE(std::filesystem::exists("out.tadah"));
  CHECK(contains(rep, "--threshold"));
  CHECK_FALSE(contains(rep, "Key not found"));
  CHECK(contains(rep, "at which two atom positions count as equal, such as "
                      "--threshold 1e-6\n"));
  CHECK_FALSE(contains(rep, "e.g."));
}

/* Pins: tadah analysis dataset_stats says what it lacks: with a -c
 * configuration without an RCUT2B or RCUTMB line or a DESCRIPTOR block,
 * and -d, "analysis dataset_stats: no cutoff: the statistics take the
 * largest cutoff of a configuration given with -c (RCUT2B, RCUTMB or the
 * cutoff RADIUS of a DESCRIPTOR block), and no line gives one (--dbfile
 * and --structure give none)"; with a -c
 * configuration of "RCUT2B 0" "analysis dataset_stats: the cutoff of the
 * statistics is the largest RCUT2B or RCUTMB value, 0, which is not above
 * 0", before the data are read (its DBFILE line names absent.tadah, a file
 * that does not exist); with a -c configuration without DBFILE or STRUCTURE
 * "analysis dataset_stats: no structures loaded: the configuration has no
 * DBFILE or STRUCTURE line"; with -c and -d of a file of no structure
 * "analysis dataset_stats: no structures loaded: the datasets of DBFILE
 * (empty.tadah) hold no structure". Each exits 1 and writes no report.
 * Before the fix: "... RCUTMAX is not set or <= 0. Set at least one of
 * RCUT2B / RCUTMB in your config so the audit cutoff is well-defined." and
 * "... no structures loaded — provide at least one DBFILE or STRUCTURE.",
 * and, for the configuration of "RCUT2B 0", "DBFILE does not exist:
 * absent.tadah" (the data were read before the cutoff was checked).
 * Before the fix that names only what the input accepts: "... RCUTMAX is
 * not set or <= 0; an RCUT2B or RCUTMB line in the configuration (-c) sets
 * it" with -d, which -c could not join, and "... no structures loaded; a
 * DBFILE or STRUCTURE line in the configuration, --dbfile or --structure
 * gives them" with -c, which --dbfile and --structure could not join: the
 * four checks of the messages fail.
 * Before the fix that lets -c join -d and -s: the first and the fourth
 * command lines (then -d alone, without the -c) are refused, "--dbfile
 * excludes --config" (status 108): their two checks fail; the second
 * reads the DBFILE of the configuration before its cutoff is checked,
 * "DBFILE does not exist: absent.tadah" (status 1): its check of the
 * message fails. (-d without -c is refused on the command line since that
 * fix, "--dbfile requires --config", before the data are read.)
 * Before the fix that takes the cutoff RADIUS of a DESCRIPTOR block as a
 * cutoff of the configuration: the first message named RCUT2B and RCUTMB
 * alone, "... no cutoff: the statistics take the largest RCUT2B or RCUTMB
 * value of a configuration given with -c as their cutoff, and no line gives
 * one ...": its check fails. */
TEST_CASE("Subcommand: analysis dataset_stats names what it lacks and "
          "what its input accepts",
          "[TadahCLI][exit-status][analysis]") {
  ScratchCwd cwd("dataset_stats_lacks");
  std::ofstream("db.tadah") << dataset_entry("T1", {"Na 0 0 0",
                                                    "Cl 2.5 2.5 2.5"});
  std::ofstream("stats.config") << "RCUT2B 4.0\n";
  std::ofstream("nocut.config") << "DBFILE db.tadah\n";
  std::ofstream("zero.config") << "RCUT2B 0\nDBFILE absent.tadah\n";
  std::ofstream("empty.tadah") << "";
  for (const auto& [args, message] :
       std::vector<std::pair<std::vector<std::string>, std::string>>{
           {{"tadah", "analysis", "dataset_stats", "-c", "nocut.config",
             "-d", "db.tadah", "-o", "st.tadah"},
            "analysis dataset_stats: no cutoff: the statistics take the "
            "largest cutoff of a configuration given with -c (RCUT2B, RCUTMB "
            "or the cutoff RADIUS of a DESCRIPTOR block), and no line gives "
            "one (--dbfile and --structure give none)"},
           {{"tadah", "analysis", "dataset_stats", "-c", "zero.config", "-o",
             "st.tadah"},
            "analysis dataset_stats: the cutoff of the statistics is the "
            "largest RCUT2B or RCUTMB value, 0, which is not above 0"},
           {{"tadah", "analysis", "dataset_stats", "-c", "stats.config", "-o",
             "st.tadah"},
            "analysis dataset_stats: no structures loaded: the configuration "
            "has no DBFILE or STRUCTURE line"},
           {{"tadah", "analysis", "dataset_stats", "-c", "stats.config",
             "-d", "empty.tadah", "-o", "st.tadah"},
            "analysis dataset_stats: no structures loaded: the datasets of "
            "DBFILE (empty.tadah) hold no structure"}}) {
    const CliRun r = run_tadah(args);
    const std::string rep = reported(r);
    std::string line;
    for (std::size_t i = 3; i < args.size(); ++i) line += " " + args[i];
    INFO(line << ": status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    CHECK(contains(rep, message));
    CHECK_FALSE(std::filesystem::exists("st.tadah"));
  }
}

/* Pins: `tadah data dedup --threshold 1e-6` on T1 = T2 = T3 and a different
 * T4 (Cl at x = 2.0 instead of 2.5) keeps the first of the three equal
 * structures and T4: exactly T1, T4, in that order.
 * Before the fix: the prune removed the first index of every duplicate pair
 * ((1,2), (1,3), (2,3)), so index 1 was removed three times and the later
 * structures shifted into it: "Removed: 3", only T4 kept. */
TEST_CASE("Subcommand: data dedup keeps the first of three equal structures",
          "[TadahCLI][exit-status][data]") {
  ScratchCwd cwd("dedup_triple");
  {
    std::ofstream os("db.tadah");
    for (const char* l : {"T1", "T2", "T3"})
      os << dataset_entry(l, {"Na 0 0 0", "Cl 2.5 2.5 2.5"});
    os << dataset_entry("T4", {"Na 0 0 0", "Cl 2.0 2.5 2.5"});
  }
  const CliRun r = run_tadah({"tadah", "data", "dedup", "-d", "db.tadah",
                              "-o", "out.tadah", "--threshold", "1e-6"});
  INFO("status " << r.status << "\n" << reported(r));
  REQUIRE(r.status == 0);
  REQUIRE(std::filesystem::exists("out.tadah"));
  CHECK(dataset_labels("out.tadah") == std::vector<std::string>{"T1", "T4"});
}

/* Pins: `tadah data balance --eshift -5` shifts the energy of each
 * structure by -5 eV per atom and writes the priorities of the data file
 * as they are: of B1 (priorities 1 1 1, -11.5 eV) and B2 (1 0 0, no forces
 * or stress data, -11.4 eV), two atoms each, the output has B1 at -1.5 eV
 * with 1 1 1 and B2 at -1.4 eV with 1 0 0. The options of the retired
 * rescaling, --rescale, --threshold, --force (-F) and --stress (-S), are
 * refused: each exits non-zero, naming the option and why ("tadah data
 * balance shifts energies only"), without writing the output.
 * Before the fix: --rescale replaced the priorities by inverse magnitudes
 * (B1's fweight 1/0.2 and sweight 1/sqrt(3 * 0.01^2), B2's 0 kept), so the
 * runs with the four options exited 0 and wrote the output; then the
 * command line refused them as unknown options ("The following argument
 * was not expected"). */
TEST_CASE("Subcommand: data balance shifts energies, keeps the priorities and "
          "refuses the options of the removed rescaling",
          "[TadahCLI][exit-status][data]") {
  ScratchCwd cwd("balance_priorities");
  {
    std::ofstream os("db.tadah");
    os << "B1\n1 1 1\n-11.5\n3.3 0 0\n0 3.3 0\n0 0 3.3\n0.01 0 0\n0 0.01 0\n0 0 0.01\n"
          "Ta 0 0 0 0.1 0 0\nTa 1.65 1.65 1.65 -0.1 0 0\n\n"
       << "B2\n1 0 0\n-11.4\n3.31 0 0\n0 3.31 0\n0 0 3.31\n0 0 0\n0 0 0\n0 0 0\n"
          "Ta 0 0 0 0 0 0\nTa 1.655 1.655 1.655 0 0 0\n\n";
  }
  {
    const CliRun r = run_tadah({"tadah", "data", "balance", "-d", "db.tadah",
                                "-o", "out.tadah", "--eshift", "-5.0"});
    INFO("status " << r.status << "\n" << reported(r));
    REQUIRE(r.status == 0);
    tadah::core::PeriodicTable::initialize();
    tadah::mlip::StructureDB db;
    db.add("out.tadah");
    REQUIRE(db.size() == 2);
    CHECK(db(0).energy == Approx(-1.5).margin(1e-12));
    CHECK(db(1).energy == Approx(-1.4).margin(1e-12));
    CHECK(db(0).eweight == 1.0);
    CHECK(db(0).fweight == 1.0);
    CHECK(db(0).sweight == 1.0);
    CHECK(db(1).eweight == 1.0);
    CHECK(db(1).fweight == 0.0);
    CHECK(db(1).sweight == 0.0);
  }
  const std::vector<std::vector<std::string>> removed = {
      {"--rescale"}, {"--threshold", "0.1"}, {"--force"}, {"-F"},
      {"--stress"}, {"-S"}};
  for (const auto& option : removed) {
    std::filesystem::remove("out2.tadah");
    std::vector<std::string> args = {"tadah", "data", "balance", "-d",
                                     "db.tadah", "-o", "out2.tadah",
                                     "--eshift", "-5.0"};
    args.insert(args.end(), option.begin(), option.end());
    const CliRun r = run_tadah(args);
    const std::string rep = reported(r);
    INFO(option[0] << ": status " << r.status << "\n" << rep);
    CHECK(r.status != 0);
    CHECK(contains(rep, option[0]));
    CHECK(contains(rep, "tadah data balance shifts energies only"));
    CHECK_FALSE(std::filesystem::exists("out2.tadah"));
  }
}

/* Pins: `tadah data dedup` on two copies of a structure whose cell block
 * is all zeros exits 1 without writing the output, and the report names
 * the file, the structure and the singular cell.
 * Before the fix: no atom matched through the NaN inverse cell, so it
 * exited 0 and kept both copies, silently. */
TEST_CASE("Subcommand: data dedup refuses a structure with a singular cell",
          "[TadahCLI][exit-status][data]") {
  ScratchCwd cwd("dedup_singular");
  {
    std::ofstream os("db.tadah");
    for (const char* l : {"Z1", "Z2"})
      os << l << "\n1 1 1\n-1.0 0.0\n0 0 0\n0 0 0\n0 0 0\n"
         << "0 0 0\n0 0 0\n0 0 0\n"
         << "Ta 0 0 0 0 0 0\nTa 1.5 1.5 1.5 0 0 0\n\n";
  }
  const CliRun r = run_tadah({"tadah", "data", "dedup", "-d", "db.tadah",
                              "-o", "out.tadah", "--threshold", "1e-6"});
  const std::string rep = reported(r);
  INFO("status " << r.status << "\n" << rep);
  CHECK(r.status == 1);
  CHECK_FALSE(std::filesystem::exists("out.tadah"));
  CHECK(contains(rep, "db.tadah"));
  CHECK(contains(rep, "singular"));
  CHECK(contains(rep, "Z1"));
}

/* Pins: `tadah data dedup --threshold 1e-6` matches atoms independently of
 * their order, periodically, and each atom at most once. Of
 *   S1 = [Na (0,0,0), Cl (2.5,2.5,2.5)],
 *   S2 = S1 with Cl listed first,
 *   S3 = S1 with Na stored at x + a0 = (5,0,0),
 *   F1 = [Na, Na (coincident), Cl], F2 = [Na, Na (coincident), K elsewhere],
 * it keeps exactly S1, F1, F2: S2 and S3 are copies of S1, and F1 and F2
 * differ.
 * Before the fix: is_the_same depended on the atom order and ignored the
 * cell, and counted the coincident Na of F2 twice; so S2 and S3 were kept
 * and F1 was removed as a duplicate of F2: S1, S2, S3, F2. */
TEST_CASE("Subcommand: data dedup removes copies with the atoms in another "
          "order or as a periodic image, and not coincident-atom look-alikes",
          "[TadahCLI][exit-status][data]") {
  ScratchCwd cwd("dedup_perm");
  {
    std::ofstream os("db.tadah");
    os << dataset_entry("S1", {"Na 0 0 0", "Cl 2.5 2.5 2.5"})
       << dataset_entry("S2", {"Cl 2.5 2.5 2.5", "Na 0 0 0"})
       << dataset_entry("S3", {"Na 5.0 0 0", "Cl 2.5 2.5 2.5"})
       << dataset_entry("F1", {"Na 0 0 0", "Na 0 0 0", "Cl 2.5 2.5 2.5"})
       << dataset_entry("F2", {"Na 0 0 0", "Na 0 0 0", "K 1.0 1.0 1.0"});
  }
  const CliRun r = run_tadah({"tadah", "data", "dedup", "-d", "db.tadah",
                              "-o", "out.tadah", "--threshold", "1e-6"});
  INFO("status " << r.status << "\n" << reported(r));
  REQUIRE(r.status == 0);
  REQUIRE(std::filesystem::exists("out.tadah"));
  CHECK(dataset_labels("out.tadah") ==
        std::vector<std::string>{"S1", "F1", "F2"});
}

/* ---- the [coincident] cases ---------------------------------------------
 *
 * NNFinder refuses a structure with two atoms at one position, naming the
 * structure and the atoms. `tadah predict`, `tadah train` and `tadah hpo`
 * build their neighbour lists before they predict, train or evaluate, so
 * each stops there with that message. */

namespace {

/* A bcc Ta structure and "Ta3 coincident", whose atoms 1 and 3 are both at
 * the origin (5 A cubes, see dataset_entry). */
std::string coincident_dataset() {
  return dataset_entry("bcc Ta", {"Ta 0 0 0", "Ta 2.5 2.5 2.5"}) +
         dataset_entry("Ta3 coincident",
                       {"Ta 0 0 0", "Ta 2.5 2.5 2.5", "Ta 0 0 0"});
}

/* `rep` names the structure (`which`, e.g. "structure 2 of 2", and its
 * label) and the atoms, as NNFinder's refusal does. */
void check_names_coincident(const std::string& rep, const std::string& which) {
  CHECK(contains(rep, which));
  CHECK(contains(rep, "'Ta3 coincident'"));
  CHECK(contains(rep, "atoms 1 and 3 (Ta, Ta)"));
  CHECK(contains(rep, "same position"));
}

}  // namespace

/* Pins: `tadah predict` with a fixed pair potential (kPairPotential) on a
 * dataset whose second structure has two Ta atoms at the same position exits
 * 1 without writing energy.pred, and the report names the structure
 * ("structure 2 of 2", 'Ta3 coincident') and the atoms ("atoms 1 and 3 (Ta,
 * Ta)", "same position"); with -F -S and without them.
 * Before the fix: both runs exited 0 and wrote energy.pred (with -F -S the
 * structure's forces and stresses were nan; without them its energy was a
 * number), and nothing named the structure, so every CHECK failed. */
TEST_CASE("Subcommand: predict refuses a structure with two atoms at the same "
          "position, naming it",
          "[TadahCLI][predict][coincident]") {
  ScratchCwd cwd("predict_coincident");
  std::ofstream("pot.tadah") << kPairPotential;
  std::ofstream("db.tadah") << coincident_dataset();
  for (const bool with_fs : {true, false}) {
    INFO("-F -S: " << with_fs);
    for (const char* f : {"energy.pred", "forces.pred", "stress.pred"})
      std::filesystem::remove(f);
    std::vector<std::string> args = {"tadah", "predict", "-p", "pot.tadah",
                                     "-d", "db.tadah"};
    if (with_fs) {
      args.push_back("-F");
      args.push_back("-S");
    }
    const CliRun r = run_tadah(args);
    const std::string rep = reported(r);
    INFO("status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    CHECK_FALSE(std::filesystem::exists("energy.pred"));
    check_names_coincident(rep, "structure 2 of 2");
  }
}

/* Pins: `tadah train` on the same dataset exits 1 without writing the
 * potential, and the report names the structure and the atoms, both when
 * the fit uses forces and stresses (FORCE/STRESS true) and in an
 * energy-only fit (FORCE/STRESS false).
 * Before the fix: with FORCE/STRESS true, exit 1 with only a LAPACK error
 * naming no structure (here "On entry to DLASCL parameter number 4 had an
 * illegal value" and "Error in DGELSD: info = 4"; with MKL "Parameter 4 was
 * incorrect on entry to DGELSD", info = -4), so the four name CHECKs failed; energy-only, exit 0 and
 * pot.tadah written, fitted to the unphysical structure (the D2_BP energy is
 * finite at r = 0), so every CHECK failed. */
TEST_CASE("Subcommand: train refuses a structure with two atoms at the same "
          "position, naming it",
          "[TadahCLI][train][coincident]") {
  ScratchCwd cwd("train_coincident");
  std::ofstream("db.tadah") << coincident_dataset();
  for (const bool with_fs : {true, false}) {
    INFO("FORCE/STRESS " << (with_fs ? "true" : "false"));
    std::filesystem::remove("pot.tadah");
    {
      std::ofstream os("config");
      os << "DBFILE db.tadah\nATOM Ta\nWATOM 1\nINIT2B true\nINITMB false\n"
            "TYPE2B D2_BP 5 5 Ta Ta\n"
            "CGRID2B 0.3 0.6 1.0 1.4 1.8\nSGRID2B 2.0 2.7 3.4 4.2 5.0\n"
            "RCTYPE2B Cut_Cos\nRCUT2B 5.0\n"
         << (with_fs ? "FORCE true\nSTRESS true\n"
                     : "FORCE false\nSTRESS false\n")
         << "BIAS false\nNORM false\nVERBOSE 0\n"
            "MODEL M_BLR BF_Linear\nLAMBDA 1e-3\nOALGO 1\n";
    }
    const CliRun r = run_tadah({"tadah", "train", "-c", "config", "-o",
                                "pot.tadah"});
    const std::string rep = reported(r);
    INFO("status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    CHECK_FALSE(std::filesystem::exists("pot.tadah"));
    check_names_coincident(rep, "structure 2 of 2");
  }
}

#ifdef TADAH_ENABLE_NFP
/* Pins: `tadah hpo` whose --validation set holds, after the two rattled Ta
 * cells, a structure with two Ta atoms at the same position stops at
 * startup: exit 1, no evaluation (no loss.tadah, no best_pot.tadah), and
 * both the report and the journal (its startup ERROR) name the structure
 * ("structure 3 of 3", 'Ta3 coincident') and the atoms.
 * Before the fix: the host started and made its evaluation, so loss.tadah
 * was written; the structure's predicted forces were nan; nothing named the
 * structure. The loss.tadah CHECK and the name CHECKs failed. */
TEST_CASE("Subcommand: hpo: a validation structure with two atoms at the same "
          "position stops the run at startup, naming it",
          "[TadahCLI][hpo][coincident]") {
  ScratchCwd cwd("hpo_coincident");
  write_frmse_fixture("config.tadah", "FORCE false\nSTRESS false\n");
  {
    std::ofstream os("val.db", std::ios::app);
    os << "Ta3 coincident\n1.0 1.0 1.0\n-0.30 0.0\n"
          "3.301 0.0 0.0\n0.0 3.301 0.0\n0.0 0.0 3.301\n"
          "0.0 0.0 0.0\n0.0 0.0 0.0\n0.0 0.0 0.0\n"
          "Ta 0.0 0.0 0.0  0.0 0.0 0.0\n"
          "Ta 1.6505 1.6505 1.6505  0.0 0.0 0.0\n"
          "Ta 0.0 0.0 0.0  0.0 0.0 0.0\n\n";
  }
  const CliRun r = run_tadah({"tadah", "hpo", "-c", "config.tadah",
                              "--hpotarget", "config.hpo",
                              "--validation", "config.val"});
  const std::string journal = std::filesystem::exists("hpo_run.log")
                                  ? slurp("hpo_run.log")
                                  : std::string();
  const std::string rep = reported(r);
  INFO("status " << r.status << "\n" << rep << "\nhpo_run.log:\n" << journal);
  CHECK(r.status == 1);
  CHECK_FALSE(std::filesystem::exists("loss.tadah"));
  CHECK_FALSE(std::filesystem::exists("best_pot.tadah"));
  check_names_coincident(rep, "structure 3 of 3");
  CHECK(contains(journal, "startup"));
  check_names_coincident(journal, "structure 3 of 3");
}
#endif  // TADAH_ENABLE_NFP

namespace {

/* ---- helpers of the [analysis] dataset_stats case -------------------- */

/* The first line of `text` that contains `what`; empty when there is none. */
std::string first_line_with(const std::string& text, const std::string& what) {
  std::istringstream in(text);
  std::string line;
  while (std::getline(in, line))
    if (contains(line, what)) return line;
  return {};
}

/* The number written after "distance " in `line`; nullopt when there is no
 * such text or no number after it. */
std::optional<double> distance_after(const std::string& line) {
  const std::string key = "distance ";
  const auto p = line.find(key);
  if (p == std::string::npos) return std::nullopt;
  const char* begin = line.c_str() + p + key.size();
  char* end = nullptr;
  const double v = std::strtod(begin, &end);
  if (end == begin) return std::nullopt;
  return v;
}

/* The r_min of the element pair `a b` in a dataset_stats file
 * (TADAH_DATASTATS_V1: "<a> <b> n_pairs <n> r_min <x> r_max <y>"); nullopt
 * when the file has no such line. */
std::optional<double> stats_pair_r_min(const std::string& text,
                                       const std::string& a,
                                       const std::string& b) {
  std::istringstream in(text);
  std::string line;
  while (std::getline(in, line)) {
    std::istringstream ss(line);
    std::vector<std::string> tok;
    for (std::string t; ss >> t;) tok.push_back(t);
    if (tok.size() >= 6 && tok[0] == a && tok[1] == b &&
        tok[2] == "n_pairs" && tok[4] == "r_min")
      return std::strtod(tok[5].c_str(), nullptr);
  }
  return std::nullopt;
}

/* The n_pairs and r_max of the element pair `a b` in a dataset_stats file
 * (TADAH_DATASTATS_V1: "<a> <b> n_pairs <n> r_min <x> r_max <y>"); nullopt
 * when the file has no such line. */
std::optional<std::pair<long, double>> stats_pair_count_r_max(
    const std::string& text, const std::string& a, const std::string& b) {
  std::istringstream in(text);
  std::string line;
  while (std::getline(in, line)) {
    std::istringstream ss(line);
    std::vector<std::string> tok;
    for (std::string t; ss >> t;) tok.push_back(t);
    if (tok.size() >= 8 && tok[0] == a && tok[1] == b &&
        tok[2] == "n_pairs" && tok[6] == "r_max")
      return std::make_pair(std::strtol(tok[3].c_str(), nullptr, 10),
                            std::strtod(tok[7].c_str(), nullptr));
  }
  return std::nullopt;
}

}  // namespace

/* Pins: `tadah analysis dataset_stats -c <config> --close-contact 0.5`, the
 * help's -c route with the new option, lists on standard output every pair
 * of atoms closer than 0.5 A, by structure (1-based index and label), atoms
 * (1-based, with symbols) and distance, after the line "Atom pairs closer
 * than 0.5 A (CLOSE_CONTACT): <K>"; and the r_min it writes counts a pair
 * at distance 0. Of
 *   clean-Ta      Ta (0,0,0), Ta (2.5,2.5,2.5) in a 5 A cell (4.33 A apart),
 *   close-Ta      the same plus Ta (0.3,0,0): atoms 1 and 3 at 0.3 A,
 *   coincident-Ta the same plus Ta (0,0,0): atoms 1 and 3 at 0,
 * it lists exactly the pairs of structures 2 and 3.
 * Before the fix: there was no --close-contact option; the command line was
 * refused (CLI11 "The following arguments were not expected: --close-contact
 * 0.5", status 109) and nothing ran. The r_min check fails on its own while
 * the pair loop skips rij <= 0: r_min is then the 0.3 A pair. */
TEST_CASE("Subcommand: analysis dataset_stats --close-contact lists the "
          "close pairs, and r_min counts coincident atoms",
          "[TadahCLI][exit-status][analysis]") {
  ScratchCwd cwd("dataset_stats_close");
  {
    std::ofstream os("db.tadah");
    os << dataset_entry("clean-Ta", {"Ta 0 0 0", "Ta 2.5 2.5 2.5"})
       << dataset_entry("close-Ta",
                        {"Ta 0 0 0", "Ta 2.5 2.5 2.5", "Ta 0.3 0 0"})
       << dataset_entry("coincident-Ta",
                        {"Ta 0 0 0", "Ta 2.5 2.5 2.5", "Ta 0 0 0"});
  }
  { std::ofstream os("stats.cfg"); os << "DBFILE db.tadah\nRCUT2B 4.5\n"; }

  const CliRun r = run_tadah({"tadah", "analysis", "dataset_stats", "-c",
                              "stats.cfg", "-o", "stats.tadah",
                              "--close-contact", "0.5"});
  INFO("status " << r.status << "\n" << reported(r));
  REQUIRE(r.status == 0);

  CHECK(contains(r.out, "Atom pairs closer than 0.5 A (CLOSE_CONTACT): 2"));
  const std::string close = first_line_with(r.out, "structure 2 ('close-Ta'):");
  const std::string coinc =
      first_line_with(r.out, "structure 3 ('coincident-Ta'):");
  REQUIRE_FALSE(close.empty());
  REQUIRE_FALSE(coinc.empty());
  CHECK(contains(close, "atoms 1 and 3 (Ta, Ta)"));
  CHECK(contains(coinc, "atoms 1 and 3 (Ta, Ta)"));
  const auto r_close = distance_after(close);
  const auto r_coinc = distance_after(coinc);
  REQUIRE(r_close);
  REQUIRE(r_coinc);
  CHECK(*r_close == Approx(0.3).margin(1e-6));
  CHECK(*r_coinc == Approx(0.0).margin(1e-6));
  CHECK_FALSE(contains(r.out, "('clean-Ta')"));

  REQUIRE(std::filesystem::exists("stats.tadah"));
  const auto r_min = stats_pair_r_min(slurp("stats.tadah"), "Ta", "Ta");
  REQUIRE(r_min);
  CHECK(*r_min == 0.0);
}

/* Pins: `tadah analysis dataset_stats -c run.cfg -d ta.tadah -o st.tadah`,
 * where run.cfg has RCUT2B 5.5, a DBFILE line of nacl.tadah and a STRUCTURE
 * line of nacl.cif, exits 0 and writes the report that `-c ref.cfg -o
 * ref.tadah` writes (ref.cfg: RCUT2B 5.5 and DBFILE ta.tadah), byte for
 * byte: the statistics of the datasets of -d alone, at the cutoff of the
 * configuration. bcc-Ta (Ta at (0, 0, 0) and (2.5, 2.5, 2.5) in a 5 A
 * cube) gives one structure, elements Ta, and inside 5.5 A the 8
 * neighbours at 4.33 A and the 6 images at 5 A of each atom: 28 ordered
 * Ta-Ta pairs, r_max 5 (16 pairs at a cutoff of 4.5 A).
 * Before the fix: -c excluded -d; the command line was refused, "--dbfile
 * excludes --config" (status 108), and no report was written. */
TEST_CASE("Subcommand: analysis dataset_stats -c with -d gives the "
          "statistics of the datasets of -d at the cutoff of the "
          "configuration",
          "[TadahCLI][exit-status][analysis]") {
  ScratchCwd cwd("dataset_stats_c_d");
  std::ofstream("ta.tadah") << dataset_entry("bcc-Ta",
                                             {"Ta 0 0 0", "Ta 2.5 2.5 2.5"});
  std::ofstream("nacl.tadah") << dataset_entry("NaCl",
                                               {"Na 0 0 0", "Cl 2.5 2.5 2.5"});
  std::ofstream("nacl.cif") << kNaClP1Cif;
  std::ofstream("run.cfg")
      << "RCUT2B 5.5\nDBFILE nacl.tadah\nSTRUCTURE nacl.cif\n";
  std::ofstream("ref.cfg") << "RCUT2B 5.5\nDBFILE ta.tadah\n";

  const CliRun ref = run_tadah({"tadah", "analysis", "dataset_stats", "-c",
                                "ref.cfg", "-o", "ref.tadah"});
  INFO("reference: status " << ref.status << "\n" << reported(ref));
  REQUIRE(ref.status == 0);
  const CliRun r = run_tadah({"tadah", "analysis", "dataset_stats", "-c",
                              "run.cfg", "-d", "ta.tadah", "-o", "st.tadah"});
  INFO("-c run.cfg -d ta.tadah: status " << r.status << "\n" << reported(r));
  REQUIRE(r.status == 0);
  REQUIRE(std::filesystem::exists("st.tadah"));

  const std::string st = slurp("st.tadah");
  CHECK(st == slurp("ref.tadah"));
  CHECK(line_tokens(st, "n_structures") ==
        std::vector<std::string>{"n_structures", "1"});
  CHECK(line_tokens(st, "elements") ==
        std::vector<std::string>{"elements", "1", "Ta"});
  const auto tata = stats_pair_count_r_max(st, "Ta", "Ta");
  REQUIRE(tata);
  CHECK(tata->first == 28);
  CHECK(tata->second == Approx(5.0).margin(1e-12));
}

/* Pins: `tadah analysis dataset_stats -c run.cfg -s nacl.cif -o st.tadah`,
 * where run.cfg has RCUT2B 4.5 and a DBFILE line of ta.tadah, exits 0 and
 * writes the report that `-c ref.cfg -o ref.tadah` writes (ref.cfg: RCUT2B
 * 4.5 and STRUCTURE nacl.cif), byte for byte: the statistics of the
 * structures of -s alone, at the cutoff of the configuration, whose DBFILE
 * line is not read. The NaCl cell (4 x 5 x 6 A) gives one structure,
 * elements Cl Na, and inside 4.5 A the Na-Na pair of the cell's 4 A images
 * (2 ordered pairs, r_max 4), which a cutoff below 4 A would not hold.
 * Before the fix: -c excluded -s; the command line was refused,
 * "--structure excludes --config" (status 108), and no report was
 * written. */
TEST_CASE("Subcommand: analysis dataset_stats -c with -s gives the "
          "statistics of the structures of -s at the cutoff of the "
          "configuration, without its DBFILE",
          "[TadahCLI][exit-status][analysis]") {
  ScratchCwd cwd("dataset_stats_c_s");
  std::ofstream("ta.tadah") << dataset_entry("bcc-Ta",
                                             {"Ta 0 0 0", "Ta 2.5 2.5 2.5"});
  std::ofstream("nacl.cif") << kNaClP1Cif;
  std::ofstream("run.cfg") << "RCUT2B 4.5\nDBFILE ta.tadah\n";
  std::ofstream("ref.cfg") << "RCUT2B 4.5\nSTRUCTURE nacl.cif\n";

  const CliRun ref = run_tadah({"tadah", "analysis", "dataset_stats", "-c",
                                "ref.cfg", "-o", "ref.tadah"});
  INFO("reference: status " << ref.status << "\n" << reported(ref));
  REQUIRE(ref.status == 0);
  const CliRun r = run_tadah({"tadah", "analysis", "dataset_stats", "-c",
                              "run.cfg", "-s", "nacl.cif", "-o", "st.tadah"});
  INFO("-c run.cfg -s nacl.cif: status " << r.status << "\n" << reported(r));
  REQUIRE(r.status == 0);
  REQUIRE(std::filesystem::exists("st.tadah"));

  const std::string st = slurp("st.tadah");
  CHECK(st == slurp("ref.tadah"));
  CHECK(line_tokens(st, "n_structures") ==
        std::vector<std::string>{"n_structures", "1"});
  CHECK(line_tokens(st, "elements") ==
        std::vector<std::string>{"elements", "2", "Cl", "Na"});
  const auto nana = stats_pair_count_r_max(st, "Na", "Na");
  REQUIRE(nana);
  CHECK(nana->first == 2);
  CHECK(nana->second == Approx(4.0).margin(1e-12));
}

/* Pins: `tadah analysis dataset_stats` with -d or -s and without -c, the
 * configuration that gives the cutoff, is refused on the command line,
 * before any data are read: "--dbfile requires --config" and "--structure
 * requires --config", for files that do not exist (absent.tadah,
 * absent.cif), which the refusal does not name; no report is written.
 * Before the fix: -d and -s ran without -c; the data were read first, so
 * the runs stopped at the missing files, "DBFILE does not exist:
 * absent.tadah" and "the structure file absent.cif does not exist" (with a
 * dataset that exists, at "analysis dataset_stats: no cutoff: ..."). */
TEST_CASE("Subcommand: analysis dataset_stats refuses -d or -s without -c, "
          "naming --config, before the data are read",
          "[TadahCLI][exit-status][analysis]") {
  ScratchCwd cwd("dataset_stats_no_c");
  for (const auto& [option, file] :
       std::vector<std::pair<std::string, std::string>>{
           {"-d", "absent.tadah"}, {"-s", "absent.cif"}}) {
    const CliRun r = run_tadah({"tadah", "analysis", "dataset_stats", option,
                                file, "-o", "st.tadah"});
    const std::string rep = reported(r);
    INFO(option << ": status " << r.status << "\n" << rep);
    CHECK(r.status != 0);
    CHECK(contains(rep, std::string(option == "-d" ? "--dbfile"
                                                   : "--structure") +
                            " requires --config"));
    CHECK_FALSE(contains(rep, file));
    CHECK_FALSE(std::filesystem::exists("st.tadah"));
  }
}

/* Pins: `tadah analysis dataset_stats -c cfg -d ta.tadah -s nacl.cif` is
 * refused on the command line, "--structure excludes --dbfile": the data
 * are the datasets of -d or the structures of -s, with the cutoff of -c.
 * No report is written.
 * Before the fix: -c excluded both, and the refusal named that,
 * "--structure excludes --config" (status 108). */
TEST_CASE("Subcommand: analysis dataset_stats refuses -d with -s",
          "[TadahCLI][exit-status][analysis]") {
  ScratchCwd cwd("dataset_stats_d_s");
  std::ofstream("ta.tadah") << dataset_entry("bcc-Ta",
                                             {"Ta 0 0 0", "Ta 2.5 2.5 2.5"});
  std::ofstream("nacl.cif") << kNaClP1Cif;
  std::ofstream("cfg") << "RCUT2B 4.5\n";
  const CliRun r = run_tadah({"tadah", "analysis", "dataset_stats", "-c",
                              "cfg", "-d", "ta.tadah", "-s", "nacl.cif",
                              "-o", "st.tadah"});
  const std::string rep = reported(r);
  INFO("status " << r.status << "\n" << rep);
  CHECK(r.status != 0);
  CHECK(contains(rep, "--structure excludes --dbfile"));
  CHECK_FALSE(std::filesystem::exists("st.tadah"));
}

/* ---- [kernel_line]: the settings line of a kernel --------------------- */

namespace {

/* Writes db.tadah, three Ta dimers, and config.train, a D2_BP pair
 * potential with four basis functions trained on it (to pot.tadah). */
void write_ta_dimer_training() {
  std::ofstream("db.tadah")
      << weighted_entry("S1", "1 1 1", "2.60 0.40 0.30", "0", "0")
      << weighted_entry("S2", "1 1 1", "3.10 0.20 0.50", "0", "0")
      << weighted_entry("S3", "1 1 1", "3.70 0.10 0.20", "0", "0");
  std::ofstream("config.train")
      << "DBFILE db.tadah\nATOM Ta\nWATOM 73\nINIT2B true\nINITMB false\n"
         "TYPE2B D2_BP 4 4 Ta Ta\nCGRID2B 2.5 3.0 3.5 4.0\n"
         "SGRID2B 0.5 0.5 0.5 0.5\nRCTYPE2B Cut_Cos\nRCUT2B 4.5\n"
         "FORCE false\nSTRESS false\nBIAS false\nNORM false\n"
         "MODEL M_BLR BF_Linear\nLAMBDA 1e-3\nOALGO 1\n";
}

}  // namespace

namespace {

/* The number of lines of @p text that contain @p needle. */
std::size_t lines_with(const std::string& text, const std::string& needle) {
  std::istringstream in(text);
  std::size_t n = 0;
  for (std::string line; std::getline(in, line);)
    if (line.find(needle) != std::string::npos) ++n;
  return n;
}

}  // namespace

/* Pins: each run prints the settings line of the kernel it builds at
 * VERBOSE 1, also when one process runs several (run_tadah runs tadah in
 * this process): on the three Ta dimers with M_KRR Kern_RBF 1 (SBASIS 3),
 * `tadah train -c config.a -v 1` (MPARAMS 1.0, pot_a.tadah), `tadah train
 * -c config.b -v 1` (MPARAMS 2.0, pot_b.tadah) and `tadah predict -d
 * db.tadah -p pot_a.tadah -v 1` exit 0, and their standard output has
 * "Kern_RBF | sigma: 1 | gamma: 1" once, "Kern_RBF | sigma: 2 | gamma:
 * 0.25" once and "Kern_RBF | sigma: 1 | gamma: 1" once, and no other
 * "Kern_RBF |" line.
 * Before the fix: one line in all, the first training's: the line was
 * printed by the first kernel of its kind in the process and by no later
 * one, so the second training's settings were never printed. (The three
 * runs were the three sections of one input, run by one tadah command.) */
TEST_CASE("Subcommand: each run prints the settings line of the kernel it "
          "builds",
          "[TadahCLI][kernel_line]") {
  ScratchCwd cwd("task_kernel_line");
  write_ta_dimer_training();
  std::string base = slurp("config.train");
  const std::string linear = "MODEL M_BLR BF_Linear\n";
  const auto at = base.find(linear);
  REQUIRE(at != std::string::npos);
  base.replace(at, linear.size(), "MODEL M_KRR Kern_RBF 1\nSBASIS 3\n");
  std::ofstream("config.a") << base << "MPARAMS 1.0\nOUTFILE pot_a.tadah\n";
  std::ofstream("config.b") << base << "MPARAMS 2.0\nOUTFILE pot_b.tadah\n";
  const std::vector<std::pair<std::vector<std::string>, std::string>> runs = {
      {{"tadah", "train", "-c", "config.a", "-v", "1"},
       "Kern_RBF | sigma: 1 | gamma: 1"},
      {{"tadah", "train", "-c", "config.b", "-v", "1"},
       "Kern_RBF | sigma: 2 | gamma: 0.25"},
      {{"tadah", "predict", "-d", "db.tadah", "-p", "pot_a.tadah", "-v", "1"},
       "Kern_RBF | sigma: 1 | gamma: 1"}};
  for (const auto& run : runs) {
    const CliRun r = run_tadah(run.first);
    INFO(run.first[1] << " " << run.first[3] << ": status " << r.status
                      << "\n" << reported(r));
    CHECK(r.status == 0);
    CHECK(lines_with(r.out, run.second) == 1u);
    CHECK(lines_with(r.out, "Kern_RBF |") == 1u);
  }
  CHECK(std::filesystem::exists("pot_a.tadah"));
  CHECK(std::filesystem::exists("pot_b.tadah"));
}

/* Pins: `tadah train` on the three Ta dimers (D2_BP, 4 weights) reads
 * FIXINDEX given as separate values as the comma list: 'FIXINDEX 1 2 3 4'
 * with FIXWEIGHT 14 2500 -3 0.5 (every weight pinned) exits 0 and writes
 * exactly 'WEIGHTS 14 2500 -3 0.5'; 'FIXINDEX 1 3' (FIXWEIGHT 0.5 -0.25)
 * writes the WEIGHTS line 'FIXINDEX 1,3' writes, weights 1 and 3 the pinned
 * values; 'FIXINDEX 1,1' with one FIXWEIGHT value exits 1 ("FIXINDEX 1,1
 * names weight 1 twice") and 'FIXINDEX 3-1' exits 1 naming the descending
 * range, neither writing pot.tadah.
 * Before the fix: the separate values were joined into one malformed
 * segment and dropped ("Error: Non-numeric value or incorrect syntax
 * found: 1 2 3 4", then "FIXWEIGHT and FIXINDEX differ in size: 4 and
 * 0", exit 1), '3-1' expanded to nothing ("differ in size: 3 and 0"), and
 * '1,1' was deduplicated with a stderr warning and trained (exit 0). After
 * core::parse_fixindex first read FIXINDEX, '1,1' exited 1 with "FIXINDEX
 * 1,1 has 2 indices but FIXWEIGHT has 1 value" (this case pinned that): it
 * compared the count before it looked for a repeat. */
TEST_CASE("Subcommand: train reads FIXINDEX given as separate values as the "
          "comma list",
          "[TadahCLI][exit-status][train][fixindex]") {
  ScratchCwd cwd("train_fixindex");
  write_ta_dimer_training();
  const std::string base = slurp("config.train");
  auto train = [&](const std::string& name, const std::string& keys) {
    std::ofstream(name) << base << keys;
    std::filesystem::remove("pot.tadah");
    return run_tadah({"tadah", "train", "-c", name, "-o", "pot.tadah"});
  };
  SECTION("every weight pinned") {
    const CliRun r =
        train("all.cfg", "FIXINDEX 1 2 3 4\nFIXWEIGHT 14 2500 -3 0.5\n");
    INFO("status " << r.status << "\n" << reported(r));
    REQUIRE(r.status == 0);
    CHECK(line_tokens(slurp("pot.tadah"), "WEIGHTS") ==
          std::vector<std::string>{"WEIGHTS", "14", "2500", "-3", "0.5"});
  }
  SECTION("two weights pinned, as with the comma list") {
    const CliRun c = train("comma.cfg", "FIXINDEX 1,3\nFIXWEIGHT 0.5 -0.25\n");
    INFO("comma: status " << c.status << "\n" << reported(c));
    REQUIRE(c.status == 0);
    const auto comma = line_tokens(slurp("pot.tadah"), "WEIGHTS");
    REQUIRE(comma.size() == 5u);
    CHECK(comma[1] == "0.5");
    CHECK(comma[3] == "-0.25");
    const CliRun s = train("space.cfg", "FIXINDEX 1 3\nFIXWEIGHT 0.5 -0.25\n");
    INFO("space: status " << s.status << "\n" << reported(s));
    REQUIRE(s.status == 0);
    CHECK(line_tokens(slurp("pot.tadah"), "WEIGHTS") == comma);
  }
  SECTION("a weight named twice and a descending range are refused") {
    const CliRun t = train("twice.cfg", "FIXINDEX 1,1\nFIXWEIGHT 0.5\n");
    const std::string rt = reported(t);
    INFO("twice: status " << t.status << "\n" << rt);
    CHECK(t.status == 1);
    CHECK(contains(rt, "FIXINDEX 1,1 names weight 1 twice"));
    CHECK_FALSE(std::filesystem::exists("pot.tadah"));
    const CliRun d = train("desc.cfg", "FIXINDEX 3-1\nFIXWEIGHT 0.5 -0.25 1\n");
    const std::string rd = reported(d);
    INFO("descending: status " << d.status << "\n" << rd);
    CHECK(d.status == 1);
    CHECK(contains(rd, "FIXINDEX: index '3-1' is a descending range"));
    CHECK_FALSE(std::filesystem::exists("pot.tadah"));
  }
}

/* Pins: `tadah train` with M_KRR implementation 2 on the three Ta dimers
 * (MODEL M_KRR Kern_Linear 2 or Kern_RBF 2 with MPARAMS 1.0, SBASIS 3)
 * trains without FIXINDEX (exit 0: Kern_Linear with 4 weights, the
 * descriptor weights of its basis vectors, DSIZE 4; Kern_RBF with 3, one
 * per basis vector; Kern_Linear's potential had the 3 coefficients of its
 * basis vectors until the potential-content fix, a potential `tadah
 * predict` refused), and refuses FIXINDEX 1 /
 * FIXWEIGHT 14, FIXINDEX 2-1 / FIXWEIGHT 14 3 and FIXINDEX 99 / FIXWEIGHT
 * 14 alike, exit 1 with "MODEL M_KRR <kernel> 2 cannot pin weights" and no
 * potential: its solve, (K + LAMBDA I) w = t, fits every weight at once.
 * Before the fix: implementation 2 never read FIXINDEX or FIXWEIGHT: each
 * of them exited 0 and wrote the WEIGHTS of the run without them (Kern_RBF
 * with FIXINDEX 1 gave weight 1 = 1.47e-272, not 14), copying FIXINDEX and
 * FIXWEIGHT into the potential. */
TEST_CASE("Subcommand: train refuses FIXINDEX with M_KRR implementation 2",
          "[TadahCLI][exit-status][train][fixindex][krr]") {
  ScratchCwd cwd("train_fixindex_krr2");
  write_ta_dimer_training();
  const std::string base = slurp("config.train");
  const std::string blr = "MODEL M_BLR BF_Linear\n";
  REQUIRE(base.find(blr) != std::string::npos);
  for (const char* kern : {"Kern_Linear", "Kern_RBF"}) DYNAMIC_SECTION(kern) {
    std::string cfg = base;
    cfg.replace(cfg.find(blr), blr.size(),
                std::string("MODEL M_KRR ") + kern + " 2\nSBASIS 3\n" +
                    (std::string(kern) == "Kern_RBF" ? "MPARAMS 1.0\n" : ""));
    auto train = [&](const std::string& name, const std::string& keys) {
      std::ofstream(name) << cfg << keys;
      std::filesystem::remove("pot.tadah");
      return run_tadah({"tadah", "train", "-c", name, "-o", "pot.tadah"});
    };
    const CliRun plain = train("plain.cfg", "");
    INFO("no FIXINDEX: status " << plain.status << "\n" << reported(plain));
    REQUIRE(plain.status == 0);
    // key + 4 (DSIZE) for Kern_Linear, key + 3 (SBASIS) for Kern_RBF
    CHECK(line_tokens(slurp("pot.tadah"), "WEIGHTS").size() ==
          (std::string(kern) == "Kern_Linear" ? 5u : 4u));
    const std::string refusal =
        std::string("MODEL M_KRR ") + kern + " 2 cannot pin weights";
    for (const char* keys : {"FIXINDEX 1\nFIXWEIGHT 14\n",
                             "FIXINDEX 2-1\nFIXWEIGHT 14 3\n",
                             "FIXINDEX 99\nFIXWEIGHT 14\n"}) {
      const CliRun r = train("pinned.cfg", keys);
      const std::string rep = reported(r);
      INFO(keys << "status " << r.status << "\n" << rep);
      CHECK(r.status == 1);
      CHECK(contains(rep, refusal));
      CHECK_FALSE(std::filesystem::exists("pot.tadah"));
    }
  }
}

/* ---- the [bias] cases ----------------------------------------------------
 *
 * BIAS true with two species (Nb, Ta): one one-hot intercept column per ATOM
 * species on every path. Before the fix `tadah train` gave every atom one
 * intercept column shared by the species (DSIZE = 1 + descriptors), while
 * pair_style tadah builds one per species, so the potential's LAMMPS
 * energies and forces were wrong. */

namespace {

/* A 4-atom bcc cell (a = 3.3 A, 3.3 x 3.3 x 6.6) with the given elements,
 * moved a little by `shift`; energy -7.6 per Nb and -8.9 per Ta plus
 * `extra` (the fits below need determined intercepts, not physics). */
std::string nbta_entry(const std::string& label,
                       const std::vector<std::string>& el, double shift,
                       double extra) {
  const double p[4][3] = {{0.0, 0.0, 0.0},
                          {1.65, 1.65, 1.65},
                          {0.0, 0.0, 3.3},
                          {1.65, 1.65, 4.95}};
  double e = extra;
  std::ostringstream os;
  for (int a = 0; a < 4; ++a) {
    e += el[a] == "Nb" ? -7.6 : -8.9;
    os << el[a] << " " << p[a][0] + shift * (a + 1) << " "
       << p[a][1] - 0.5 * shift * a << " " << p[a][2] + 0.3 * shift
       << " 0 0 0\n";
  }
  std::ostringstream s;
  s << label << "\n1 1 1\n" << e << " 0.0\n3.3 0 0\n0 3.3 0\n0 0 6.6\n"
    << "0 0 0\n0 0 0\n0 0 0\n" << os.str() << "\n";
  return s.str();
}

/* Eight cells of every Nb-Ta composition (`fixed`: all Nb2 Ta2). */
std::string nbta_dataset(bool fixed) {
  const std::vector<std::vector<std::string>> varied = {
      {"Nb", "Nb", "Nb", "Nb"}, {"Nb", "Nb", "Nb", "Ta"},
      {"Nb", "Ta", "Nb", "Ta"}, {"Nb", "Ta", "Ta", "Ta"},
      {"Ta", "Ta", "Ta", "Ta"}, {"Ta", "Nb", "Ta", "Ta"},
      {"Ta", "Nb", "Nb", "Ta"}, {"Nb", "Ta", "Nb", "Nb"}};
  std::string db;
  for (std::size_t i = 0; i < varied.size(); ++i)
    db += nbta_entry("S" + std::to_string(i + 1),
                     fixed ? std::vector<std::string>{"Nb", "Ta", "Ta", "Nb"}
                           : varied[i],
                     0.02 * double(i + 1), 0.05 * std::sin(double(i)));
  return db;
}

/* D2_BP (3 functions, every pair), Cut_Cos 5 A, M_BLR BF_Linear, BIAS true:
 * the keys of a training configuration and of its potential. */
const char* const kNbTaKeys =
    "ATOM Nb Ta\nINIT2B true\nINITMB false\nTYPE2B D2_BP 3 3 * *\n"
    "CGRID2B 2.4 3.1 3.9\nSGRID2B 1.0 1.2 0.9\nRCTYPE2B Cut_Cos\n"
    "RCUT2B 5.0\nFORCE false\nSTRESS false\nBIAS true\nNORM false\n"
    "LAMBDA 0\nOALGO 1\nMODEL M_BLR BF_Linear\n";

}  // namespace

/* Pins: `tadah train` on Nb-Ta cells of varied composition with BIAS true
 * writes 2 + 3 WEIGHTS (one per species' intercept, then the D2_BP
 * weights), and `tadah predict` with that potential exits 0 and predicts
 * every structure; with FIXINDEX it prints a WARNING that the descriptor
 * weights start at index 3.
 * Before the fix: 1 + 3 WEIGHTS (one intercept shared by Nb and Ta), and no
 * WARNING. */
TEST_CASE("Subcommand: train with BIAS true and two species writes one "
          "weight per species' intercept",
          "[TadahCLI][train][bias]") {
  ScratchCwd cwd("bias_train");
  std::ofstream("db.tadah") << nbta_dataset(false);
  std::ofstream("config") << "DBFILE db.tadah\n" << kNbTaKeys;
  const CliRun r = run_tadah({"tadah", "train", "-c", "config", "-o",
                              "pot.tadah"});
  INFO("status " << r.status << "\n" << reported(r));
  REQUIRE(r.status == 0);
  CHECK(line_tokens(slurp("pot.tadah"), "WEIGHTS").size() == 6u);  // key + 5
  const CliRun p = run_tadah({"tadah", "predict", "-p", "pot.tadah", "-d",
                              "db.tadah"});
  INFO("predict: status " << p.status << "\n" << reported(p));
  CHECK(p.status == 0);
  CHECK(predicted_values("energy.pred").size() == 8u);

  std::ofstream("config_fix") << "DBFILE db.tadah\n" << kNbTaKeys
                              << "FIXINDEX 3\nFIXWEIGHT 0.05\n";
  const CliRun f = run_tadah({"tadah", "train", "-c", "config_fix", "-o",
                              "pot_fix.tadah"});
  const std::string rep = reported(f);
  INFO("FIXINDEX: status " << f.status << "\n" << rep);
  CHECK(f.status == 0);
  CHECK(contains(rep, "FIXINDEX with BIAS true"));
  CHECK(contains(rep, "start at index 3"));
}

/* Pins: `tadah train` with M_BLR BF_Polynomial2, BIAS true on the Nb-Ta
 * cells (DSIZE 5: 15 weights, the products (i,ii), i <= ii) and FIXINDEX 4
 * prints a WARNING naming the function's intercept weights, "weights 1 (Nb)
 * and 6 (Ta)" (the products (0,0) and (1,1)), and not the linear models'
 * "descriptor weights start at index 3"; it trains (exit 0).
 * Before the fix: the WARNING said "weights 1 to 2 are the per-species
 * intercepts ... the descriptor weights start at index 3", wrong for
 * BF_Polynomial2, whose weight 2 is the Nb x Ta product. */
TEST_CASE("Subcommand: train warns of FIXINDEX with BIAS true naming the "
          "intercept weights of BF_Polynomial2",
          "[TadahCLI][train][bias]") {
  ScratchCwd cwd("bias_poly2_fixindex");
  std::ofstream("db.tadah") << nbta_dataset(false);
  std::string keys = kNbTaKeys;
  keys.replace(keys.find("MODEL M_BLR BF_Linear"), 21,
               "MODEL M_BLR BF_Polynomial2");
  std::ofstream("config") << "DBFILE db.tadah\n" << keys
                          << "FIXINDEX 4\nFIXWEIGHT 0.05\n";
  const CliRun r = run_tadah({"tadah", "train", "-c", "config", "-o",
                              "pot.tadah"});
  const std::string rep = reported(r);
  INFO("status " << r.status << "\n" << rep);
  CHECK(r.status == 0);
  CHECK(contains(rep, "FIXINDEX with BIAS true"));
  CHECK(contains(rep, "weights 1 (Nb) and 6 (Ta)"));
  CHECK_FALSE(contains(rep, "start at index 3"));
}

/* Pins: a two-species BIAS potential with one intercept column shared by
 * the species (1 + 3 WEIGHTS, as tadah trained it before 1.6.0):
 * `tadah predict` exits 1 without writing energy.pred, naming WEIGHTS,
 * DSIZE, BIAS and ATOM ("has 4 WEIGHTS", "5 components (DSIZE)") and saying
 * to retrain; `tadah train -c` with the potential itself (plus a DBFILE)
 * retrains it into 2 + 3 WEIGHTS (exit 0).
 * Before the fix: predict exited 0 with the shared intercept (and LAMMPS
 * used the weights with one intercept column per species); the retrained
 * potential had 1 + 3 WEIGHTS again. Before the one-guard rewording: the
 * message said "WEIGHTS has 4 values" and "DSIZE 5". */
TEST_CASE("Subcommand: predict refuses a two-species BIAS potential with one "
          "shared intercept column; train -c retrains it",
          "[TadahCLI][predict][bias]") {
  ScratchCwd cwd("bias_legacy");
  std::ofstream("db.tadah") << nbta_dataset(false);
  std::ofstream("legacy.tadah") << kNbTaKeys
                                << "WEIGHTS -8.2 0.1 -0.05 0.02\n";
  const CliRun r = run_tadah({"tadah", "predict", "-p", "legacy.tadah", "-d",
                              "db.tadah"});
  const std::string rep = reported(r);
  INFO("status " << r.status << "\n" << rep);
  CHECK(r.status == 1);
  CHECK_FALSE(std::filesystem::exists("energy.pred"));
  CHECK(contains(rep, "has 4 WEIGHTS"));
  CHECK(contains(rep, "5 components (DSIZE)"));
  CHECK(contains(rep, "ATOM Nb Ta"));
  CHECK(contains(rep, "retrain"));

  std::ofstream("legacy_cfg.tadah") << slurp("legacy.tadah")
                                    << "DBFILE db.tadah\n";
  const CliRun t = run_tadah({"tadah", "train", "-c", "legacy_cfg.tadah",
                              "-o", "retrained.tadah"});
  INFO("train -c legacy: status " << t.status << "\n" << reported(t));
  CHECK(t.status == 0);
  REQUIRE(std::filesystem::exists("retrained.tadah"));
  CHECK(line_tokens(slurp("retrained.tadah"), "WEIGHTS").size() == 6u);
}

/* Pins: `tadah train` refuses (exit 1, no potential) BIAS true data whose
 * every structure has the composition Nb2 Ta2 (the intercepts cannot be
 * determined: rank 1 of 2), naming the problem and the remedies; and NORM
 * true with M_KRR Kern_Linear and BIAS false (exit 1, as for BF_Linear).
 * Before the fix: both trained (exit 0): the fixed composition with one
 * shared intercept, and the Kern_Linear NORM potential with its mean
 * correction folded into its first descriptor weight.
 * Before the fix that states the rule: the refusal of NORM true under
 * Kern_Linear without BIAS read "Configuration: NORM=true with
 * MODEL=Kern_Linear requires BIAS=true. ... Set BIAS=true or NORM=false.":
 * the checks of it fail. */
TEST_CASE("Subcommand: train refuses intercepts the data cannot determine "
          "and NORM with Kern_Linear without BIAS",
          "[TadahCLI][train][bias]") {
  ScratchCwd cwd("bias_refusals");
  std::ofstream("fixed.tadah") << nbta_dataset(true);
  std::ofstream("config") << "DBFILE fixed.tadah\n" << kNbTaKeys;
  const CliRun r = run_tadah({"tadah", "train", "-c", "config", "-o",
                              "pot.tadah"});
  const std::string rep = reported(r);
  INFO("fixed composition: status " << r.status << "\n" << rep);
  CHECK(r.status == 1);
  CHECK_FALSE(std::filesystem::exists("pot.tadah"));
  CHECK(contains(rep, "cannot determine"));
  CHECK(contains(rep, "Nb2 Ta2"));
  CHECK(contains(rep, "BIAS false"));

  std::ofstream("db.tadah") << nbta_dataset(false);
  std::string keys = kNbTaKeys;
  keys.replace(keys.find("BIAS true"), 9, "BIAS false");
  keys.replace(keys.find("NORM false"), 10, "NORM true");
  keys.replace(keys.find("MODEL M_BLR BF_Linear"), 21,
               "MODEL M_KRR Kern_Linear");
  std::ofstream("config_norm") << "DBFILE db.tadah\n" << keys;
  const CliRun n = run_tadah({"tadah", "train", "-c", "config_norm", "-o",
                              "pot_norm.tadah"});
  const std::string nrep = reported(n);
  INFO("NORM Kern_Linear: status " << n.status << "\n" << nrep);
  CHECK(n.status == 1);
  CHECK_FALSE(std::filesystem::exists("pot_norm.tadah"));
  CHECK(contains(nrep,
                 "NORM true with MODEL Kern_Linear requires BIAS true: "));
  CHECK(contains(nrep, "; BIAS true or NORM false is accepted."));
}

/* Pins: `tadah train` refuses FIXINDEX/FIXWEIGHT with NORM true (exit 1,
 * no potential): the Nb-Ta cells, BIAS true, NORM true, FIXINDEX 1 /
 * FIXWEIGHT -7.6 (the Nb intercept the composition refusal says to pin),
 * for M_BLR BF_Linear and M_KRR Kern_Linear, naming FIXINDEX/FIXWEIGHT and
 * NORM true, as tadah hpo refuses the same file at startup.
 * Before the fix: both trained (exit 0), and the potential's WEIGHTS began
 * -7.64937, not the FIXWEIGHT -7.6 it also recorded (the pinned
 * normalised-space intercept moved by the conversion to raw-descriptor
 * weights). */
TEST_CASE("Subcommand: train refuses FIXINDEX/FIXWEIGHT with NORM true",
          "[TadahCLI][train][norm][fixindex]") {
  ScratchCwd cwd("norm_fixindex");
  std::ofstream("db.tadah") << nbta_dataset(false);
  for (const char* model : {"MODEL M_BLR BF_Linear", "MODEL M_KRR Kern_Linear"}) {
    INFO(model);
    std::string keys = kNbTaKeys;
    keys.replace(keys.find("NORM false"), 10, "NORM true");
    keys.replace(keys.find("MODEL M_BLR BF_Linear"), 21, model);
    std::ofstream("config") << "DBFILE db.tadah\n" << keys
                            << "FIXINDEX 1\nFIXWEIGHT -7.6\n";
    std::filesystem::remove("pot.tadah");
    const CliRun r = run_tadah({"tadah", "train", "-c", "config", "-o",
                                "pot.tadah"});
    const std::string rep = reported(r);
    INFO("status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    CHECK_FALSE(std::filesystem::exists("pot.tadah"));
    CHECK(contains(rep, "FIXINDEX/FIXWEIGHT is not supported with NORM true"));
  }
}

/* Pins: the composition refusal (every cell Nb2 Ta2, BIAS true) names the
 * remedies its configuration can take: under NORM false BIAS false with
 * explicit ESHIFT values or ESHIFT_ATOM, and the FIXINDEX/FIXWEIGHT pin;
 * under NORM true neither (NORM true with BF_Linear needs BIAS true, and
 * FIXINDEX is refused there), but NORM false; never ESHIFT_DBATOM, whose
 * fit of the species energies is singular on such data: BIAS false with
 * ESHIFT_DBATOM true on the same cells trains with ESHIFT 0 0 and the
 * WARNING "normal-equations matrix is singular".
 * Before the fix: both refusals said "Use BIAS false (ESHIFT_ATOM,
 * ESHIFT_DBATOM or ESHIFT remove a per-species reference energy), ... or
 * pin intercepts with FIXINDEX/FIXWEIGHT".
 * Before the fix that states the remedies: "Use BIAS false with explicit
 * ESHIFT values ... or pin intercepts with FIXINDEX/FIXWEIGHT": the checks
 * fail. */
TEST_CASE("Subcommand: train's composition refusal names the remedies the "
          "configuration can take",
          "[TadahCLI][train][bias]") {
  ScratchCwd cwd("bias_remedies");
  std::ofstream("fixed.tadah") << nbta_dataset(true);
  std::ofstream("config") << "DBFILE fixed.tadah\n" << kNbTaKeys;
  const CliRun r = run_tadah({"tadah", "train", "-c", "config", "-o",
                              "pot.tadah"});
  const std::string rep = reported(r);
  INFO("NORM false: status " << r.status << "\n" << rep);
  CHECK(r.status == 1);
  CHECK(contains(rep, " BIAS false with explicit ESHIFT values"));
  CHECK_FALSE(contains(rep, "Use BIAS false"));
  CHECK(contains(rep, "ESHIFT_ATOM"));
  CHECK(contains(rep, "intercepts pinned with FIXINDEX/FIXWEIGHT"));
  CHECK_FALSE(contains(rep, "ESHIFT_DBATOM"));

  std::string keys = kNbTaKeys;
  keys.replace(keys.find("NORM false"), 10, "NORM true");
  std::ofstream("config_norm") << "DBFILE fixed.tadah\n" << keys;
  const CliRun n = run_tadah({"tadah", "train", "-c", "config_norm", "-o",
                              "pot.tadah"});
  const std::string nrep = reported(n);
  INFO("NORM true: status " << n.status << "\n" << nrep);
  CHECK(n.status == 1);
  CHECK(contains(nrep, "cannot determine"));
  CHECK(contains(nrep, "NORM false"));
  CHECK_FALSE(contains(nrep, "FIXINDEX"));
  CHECK_FALSE(contains(nrep, "BIAS false"));
  CHECK_FALSE(contains(nrep, "ESHIFT_DBATOM"));

  keys = kNbTaKeys;
  keys.replace(keys.find("BIAS true"), 9, "BIAS false\nESHIFT_DBATOM true");
  std::ofstream("config_dbatom") << "DBFILE fixed.tadah\n" << keys;
  const CliRun d = run_tadah({"tadah", "train", "-c", "config_dbatom", "-o",
                              "pot_dbatom.tadah"});
  const std::string drep = reported(d);
  INFO("ESHIFT_DBATOM: status " << d.status << "\n" << drep);
  CHECK(d.status == 0);
  CHECK(contains(drep, "normal-equations matrix is singular"));
  CHECK(line_tokens(slurp("pot_dbatom.tadah"), "ESHIFT") ==
        std::vector<std::string>{"ESHIFT", "0", "0"});
}

namespace {
/* The lines of a potential's training record (defined with the
 * training-record cases below). */
std::vector<std::string> record_of(const std::string& pot);
/* `text` with the line of `key` replaced by `line` ("" removes it; defined
 * with the [retrain] cases below). */
std::string with_line(const std::string& text, const std::string& key,
                      const std::string& line);
}  // namespace

/* Pins: a potential used as the training configuration (tadah train -c,
 * the retrain route the multi-species BIAS refusal names) with the
 * UNCERTAINTY line tadah train 1.0.0 to 1.5.0 wrote into every potential
 * (the two-species legacy potential: 1 + 3 WEIGHTS, OALGO 1, UNCERTAINTY true,
 * DBFILE) is refused (exit 1), naming the line as a retired key marked as
 * converted by deleting it ("UNCERTAINTY true (line N of legacy_cfg.tadah;
 * converted by deleting the line)"), the reason and the conversion, and
 * ending "With this line deleted (...), the configuration trains"; it
 * writes neither a potential nor
 * weights.tadah; with the line deleted it retrains (exit 0), its
 * potential with 2 + 3 WEIGHTS and no UNCERTAINTY line or record line; a
 * training configuration (no WEIGHTS) with UNCERTAINTY true is refused
 * alike, naming config.train; and --uncertainty on the command line is
 * refused as the option of the retired key.
 * Before the fix: the potential's line was not read (a WARNING said that
 * it asked the run that wrote it for weights.tadah) and the retrain exited
 * 0; the configuration's line and --uncertainty were requests, refused
 * with OALGO 1 for needing OALGO 3; then the refusal did not mark the
 * line, gave no reason and said to put converted keys in its place. */
TEST_CASE("Subcommand: train -c refuses the UNCERTAINTY line of a potential "
          "and of a configuration",
          "[TadahCLI][train][uncertainty][bias]") {
  ScratchCwd cwd("uncertainty_legacy");
  std::ofstream("db.tadah") << nbta_dataset(false);
  const std::string legacy = std::string(kNbTaKeys) + "UNCERTAINTY true\n" +
                             "WEIGHTS -8.2 0.1 -0.05 0.02\n" +
                             "DBFILE db.tadah\n";
  std::ofstream("legacy_cfg.tadah") << legacy;
  const CliRun t = run_tadah({"tadah", "train", "-c", "legacy_cfg.tadah",
                              "-o", "re.tadah"});
  const std::string rep = reported(t);
  INFO("retrain: status " << t.status << "\n" << rep);
  CHECK(t.status == 1);
  CHECK(contains(rep, "UNCERTAINTY true (line "));
  CHECK(contains(rep, " of legacy_cfg.tadah; converted by deleting the "
                      "line)"));
  CHECK(contains(rep, "retired"));
  CHECK(contains(rep, "UNCERTAINTY: tadah train --uncertainty wrote "
                      "weights.tadah, each coefficient of the fit beside a "
                      "diagonal entry of SIGMA."));
  CHECK(contains(rep, "Deleting the option, or the UNCERTAINTY line, "
                      "converts it"));
  CHECK(contains(rep, "With this line deleted (in the file that has it"));
  CHECK_FALSE(std::filesystem::exists("re.tadah"));
  CHECK_FALSE(std::filesystem::exists("weights.tadah"));

  std::ofstream("converted.tadah") << with_line(legacy, "UNCERTAINTY", "");
  const CliRun ok = run_tadah({"tadah", "train", "-c", "converted.tadah",
                               "-o", "re.tadah"});
  const std::string okrep = reported(ok);
  INFO("converted: status " << ok.status << "\n" << okrep);
  CHECK(ok.status == 0);
  CHECK_FALSE(contains(okrep, "UNCERTAINTY"));
  CHECK_FALSE(std::filesystem::exists("weights.tadah"));
  REQUIRE(std::filesystem::exists("re.tadah"));
  const std::string pot = slurp("re.tadah");
  CHECK(line_tokens(pot, "WEIGHTS").size() == 6u);
  CHECK(line_tokens(pot, "UNCERTAINTY").empty());
  for (const std::string& line : record_of(pot)) {
    INFO("record line: " << line);
    CHECK(line.rfind("UNCERTAINTY", 0) != 0);
  }

  std::ofstream("config.train") << kNbTaKeys << "UNCERTAINTY true\n"
                                << "DBFILE db.tadah\n";
  const CliRun c = run_tadah({"tadah", "train", "-c", "config.train", "-o",
                              "re3.tadah"});
  const std::string crep = reported(c);
  INFO("configuration: status " << c.status << "\n" << crep);
  CHECK(c.status == 1);
  CHECK(contains(crep, "UNCERTAINTY true (line "));
  CHECK(contains(crep, " of config.train; converted by deleting the line)"));
  CHECK_FALSE(std::filesystem::exists("re3.tadah"));

  const CliRun u = run_tadah({"tadah", "train", "-c", "converted.tadah",
                              "-o", "re2.tadah", "--uncertainty"});
  const std::string urep = reported(u);
  INFO("--uncertainty: status " << u.status << "\n" << urep);
  CHECK(u.status == 1);
  CHECK(contains(urep, "--uncertainty: UNCERTAINTY is a retired key"));
  CHECK_FALSE(std::filesystem::exists("re2.tadah"));
}

/* Pins: a linear NORM potential (M_BLR BF_Linear, BIAS true, NORM true on
 * the Nb-Ta cells, 5 WEIGHTS) predicts the same energies whatever its NMEAN
 * and NSTDEV hold: a linear model predicts with its converted weights. The
 * potential with NMEAN and NSTDEV of 10 values, the trained 5 preceded by 5
 * others (what `tadah train -c <potential>` wrote before 1.6.0: the
 * potential's values, then the new fit's), exits 0 and writes an
 * energy.pred identical to the trained potential's, as it did before the
 * per-species fix.
 * Before the fix: predict refused it, exit 1, "NORM true: NMEAN has 10
 * values, but the descriptor has DSIZE 5 entries". */
TEST_CASE("Subcommand: predict runs a linear NORM potential whose NMEAN and "
          "NSTDEV a retrain appended to",
          "[TadahCLI][predict][bias][norm]") {
  ScratchCwd cwd("bias_norm_appended");
  std::ofstream("db.tadah") << nbta_dataset(false);
  std::string keys = kNbTaKeys;
  keys.replace(keys.find("NORM false"), 10, "NORM true");
  std::ofstream("config") << "DBFILE db.tadah\n" << keys;
  const CliRun t = run_tadah({"tadah", "train", "-c", "config", "-o",
                              "pot.tadah"});
  INFO("train: status " << t.status << "\n" << reported(t));
  REQUIRE(t.status == 0);
  const std::string pot = slurp("pot.tadah");
  REQUIRE(line_tokens(pot, "NMEAN").size() == 6u);   // key + 5
  REQUIRE(line_tokens(pot, "NSTDEV").size() == 6u);
  // The same potential with 5 other values before each of NMEAN, NSTDEV.
  std::istringstream in(pot);
  std::ostringstream appended;
  for (std::string line; std::getline(in, line);) {
    std::istringstream ss(line);
    std::string key;
    ss >> key;
    if (key == "NMEAN" || key == "NSTDEV") {
      std::vector<std::string> v;
      for (std::string x; ss >> x;) v.push_back(x);
      appended << key;
      for (const std::string& x : v) appended << " " << 1.1 * std::stod(x) + 0.01;
      for (const std::string& x : v) appended << " " << x;
      appended << "\n";
    } else {
      appended << line << "\n";
    }
  }
  std::ofstream("appended.tadah") << appended.str();
  REQUIRE(line_tokens(slurp("appended.tadah"), "NMEAN").size() == 11u);

  const CliRun p = run_tadah({"tadah", "predict", "-p", "pot.tadah", "-d",
                              "db.tadah", "-o", "energy_pot.pred"});
  INFO("predict pot.tadah: status " << p.status << "\n" << reported(p));
  REQUIRE(p.status == 0);
  const CliRun a = run_tadah({"tadah", "predict", "-p", "appended.tadah",
                              "-d", "db.tadah", "-o", "energy_app.pred"});
  INFO("predict appended.tadah: status " << a.status << "\n" << reported(a));
  CHECK(a.status == 0);
  REQUIRE(std::filesystem::exists("energy_app.pred"));
  CHECK(slurp("energy_app.pred") == slurp("energy_pot.pred"));
}

/* Pins: `tadah predict` refuses (exit 1, no energy.pred) a linear potential
 * with NORM true and BIAS false, M_KRR Kern_Linear and M_BLR BF_Linear on
 * the Nb-Ta cells (3 WEIGHTS, NMEAN and NSTDEV, as an earlier version wrote
 * them), naming NORM true and BIAS false and saying to retrain: the
 * conversion that wrote such weights took descriptor component 0 for an
 * intercept, so their energies are wrong; tadah train refuses the
 * configuration (NORM true with a linear model needs BIAS true).
 * Before the fix: predict exited 0 and wrote the potential's wrong energies
 * (an M_KRR Kern_Linear NORM potential of the quickstart Ta data predicted
 * -0.44 eV/atom against -8.04 eV/atom of its NORM false fit). */
TEST_CASE("Subcommand: predict refuses a linear potential with NORM true and "
          "BIAS false",
          "[TadahCLI][predict][norm]") {
  ScratchCwd cwd("norm_no_bias");
  std::ofstream("db.tadah") << nbta_dataset(false);
  for (const std::string model : {"MODEL M_KRR Kern_Linear", "MODEL M_BLR BF_Linear"}) {
    std::string keys = kNbTaKeys;
    keys.replace(keys.find("BIAS true"), 9, "BIAS false");
    keys.replace(keys.find("NORM false"), 10, "NORM true");
    keys.replace(keys.find("MODEL M_BLR BF_Linear"), 21, model);
    std::ofstream("pot.tadah") << keys << "WEIGHTS 0.19 -0.8 0.02\n"
                               << "NMEAN 0.4 0.3 0.2\nNSTDEV 0.2 0.3 0.4\n";
    std::filesystem::remove("energy.pred");
    const CliRun r = run_tadah({"tadah", "predict", "-p", "pot.tadah", "-d",
                                "db.tadah"});
    const std::string rep = reported(r);
    INFO(model << ": status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    CHECK_FALSE(std::filesystem::exists("energy.pred"));
    CHECK(contains(rep, "NORM true"));
    CHECK(contains(rep, "BIAS false"));
    CHECK(contains(rep, "retrain"));
  }
}

/* Pins: `tadah analysis bfunc -c <training configuration> --type 2b -r 1
 * 5 5` plots the D2_Blip basis of a configuration with BIAS true and no
 * ATOM (as in the shipped examples/CLI/ex_1/config.train: tadah train
 * takes ATOM from the DBFILE): exit 0, 4 functions x 5 points, and the
 * same output, byte for byte, as with BIAS false (the radial basis does
 * not depend on the intercept columns).
 * Before the fix: exit 1, "BIAS true needs ATOM: BIAS gives one intercept
 * column per ATOM species, and the configuration lists no ATOM species"
 * (the engine built the DC_Selector of a whole model to read the
 * descriptor's basis); with BIAS false it plotted. */
TEST_CASE("Subcommand: analysis bfunc plots the basis of a BIAS true "
          "configuration without ATOM",
          "[TadahCLI][analysis][bias]") {
  ScratchCwd cwd("bfunc_bias_no_atom");
  const std::string keys =
      "DBFILE tdata.db\nINIT2B true\nMODEL M_BLR BF_Linear\n"
      "TYPE2B D2_Blip 4 4 Ta Ta\nRCTYPE2B Cut_Cos\nRCUT2B 5.3\n"
      "SGRID2B GEOM 4 0.1 1.0\nCGRID2B LIN 4 1.0 5.3\nLAMBDA 1e-7\n"
      "OALGO 1\n";
  std::ofstream("config_bias") << keys << "BIAS true\n";
  std::ofstream("config_nobias") << keys << "BIAS false\n";
  const CliRun f = run_tadah({"tadah", "analysis", "bfunc", "-c",
                              "config_nobias", "--type", "2b", "-r", "1",
                              "5", "5", "-o", "nobias.out"});
  INFO("BIAS false: status " << f.status << "\n" << reported(f));
  REQUIRE(f.status == 0);
  const CliRun t = run_tadah({"tadah", "analysis", "bfunc", "-c",
                              "config_bias", "--type", "2b", "-r", "1", "5",
                              "5", "-o", "bias.out"});
  INFO("BIAS true: status " << t.status << "\n" << reported(t));
  CHECK(t.status == 0);
  REQUIRE(std::filesystem::exists("bias.out"));
  std::istringstream in(slurp("bias.out"));
  std::size_t rows = 0;
  for (std::string line; std::getline(in, line);)
    if (line.find_first_not_of(" \t") != std::string::npos) ++rows;
  CHECK(rows == 20u);
  CHECK(slurp("bias.out") == slurp("nobias.out"));
}

/* ---- [not-built]: tadah hpo in a tadah built without NFP -------------- */

namespace {

/* What a tadah built without TADAH_ENABLE_NFP says of hpo. */
const char* const kHpoNotBuilt =
    "tadah hpo needs the Nested Fitting Procedure, which this tadah was "
    "built without; reconfigure with -DTADAH_ENABLE_NFP=ON (see "
    "Installation)";

/* `text` with each run of whitespace one space (the help wraps its lines). */
std::string one_line(const std::string& text) {
  std::string out;
  bool blank = false;
  for (char ch : text) {
    if (std::isspace(static_cast<unsigned char>(ch))) {
      blank = true;
      continue;
    }
    if (blank && !out.empty()) out += ' ';
    blank = false;
    out += ch;
  }
  return out;
}

/* The hpo target of the [not-built] cases: one OPTIM line and a loss. */
void write_hpo_target() {
  std::ofstream("config.hpo") << "LOSS L2\nPC_ERMSE 0 1\n"
                                 "OPTIM CGRID2B (1) 2.3 2.7\n";
}

}  // namespace

#ifndef TADAH_ENABLE_NFP
/* Pins: in a tadah built without TADAH_ENABLE_NFP, `tadah hpo -c
 * config.train --hpotarget config.hpo` exits 1 with "tadah hpo needs the
 * Nested Fitting Procedure, which this tadah was built without; reconfigure
 * with -DTADAH_ENABLE_NFP=ON (see Installation)" and writes no
 * best_pot.tadah.
 * Before the fix: it exited 1 with "EngineFactory::create: No engine found
 * for HPO". */
TEST_CASE("Subcommand: hpo in a tadah built without NFP is refused, naming "
          "TADAH_ENABLE_NFP",
          "[TadahCLI][exit-status][not-built]") {
  ScratchCwd cwd("hpo_not_built");
  write_ta_dimer_training();
  write_hpo_target();
  {
    const CliRun r = run_tadah({"tadah", "hpo", "-c", "config.train",
                                "--hpotarget", "config.hpo"});
    const std::string rep = reported(r);
    INFO("tadah hpo: status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    CHECK(contains(rep, kHpoNotBuilt));
    CHECK_FALSE(contains(rep, "No engine found"));
    CHECK_FALSE(std::filesystem::exists("best_pot.tadah"));
  }
}

/* Pins: in a tadah built without TADAH_ENABLE_NFP, `tadah -h` lists hpo as
 * "hpo Optimises the model architecture and hyperparameters. tadah hpo needs
 * the Nested Fitting Procedure, ...; reconfigure with -DTADAH_ENABLE_NFP=ON
 * (see Installation)." (whitespace collapsed: the listing wraps), `tadah
 * hpo -h` has the same two sentences, and `tadah explain hpo` ends with a
 * blank line and that message with a full stop; each exits 0.
 * Before the fix: none of them had the message (the listing and `tadah
 * hpo -h` said only "Optimize the model architecture and
 * hyperparameters.", and `tadah explain hpo` described the nested fitting
 * procedure). */
TEST_CASE("Subcommand: tadah -h, tadah hpo -h and tadah explain hpo say "
          "that a tadah built without NFP has no hpo",
          "[TadahCLI][exit-status][not-built]") {
  ScratchCwd cwd("hpo_not_built_help");
  const std::string described = std::string("Optimises the model architecture "
                                            "and hyperparameters. ") +
                                kHpoNotBuilt + ".";
  {
    const CliRun r = run_tadah({"tadah", "-h"});
    INFO("tadah -h: status " << r.status << "\n" << reported(r));
    CHECK(r.status == 0);
    CHECK(contains(one_line(r.out), "hpo " + described));
  }
  {
    const CliRun r = run_tadah({"tadah", "hpo", "-h"});
    INFO("tadah hpo -h: status " << r.status << "\n" << reported(r));
    CHECK(r.status == 0);
    CHECK(contains(one_line(r.out), described));
  }
  {
    const CliRun r = run_tadah({"tadah", "explain", "hpo"});
    INFO("tadah explain hpo: status " << r.status << "\n" << reported(r));
    CHECK(r.status == 0);
    std::vector<std::string> lines;
    std::istringstream in(r.out);
    for (std::string l; std::getline(in, l);) lines.push_back(l);
    REQUIRE(lines.size() >= 2);
    CHECK(lines[lines.size() - 2].empty());
    CHECK(lines.back() == std::string(kHpoNotBuilt) + ".");
  }
}
#else
/* Pins: in a tadah built with TADAH_ENABLE_NFP, `tadah -h` lists hpo as
 * "hpo Optimises the model architecture and hyperparameters.", and neither
 * it, `tadah hpo -h` nor `tadah explain hpo` (each exits 0 and prints
 * something) says "which this tadah was built without".
 * Before the fix: passes as well (a pin: only a tadah without NFP has the
 * message). */
TEST_CASE("Subcommand: tadah -h, tadah hpo -h and tadah explain hpo of a "
          "tadah built with NFP do not say hpo is not built",
          "[TadahCLI][exit-status][not-built]") {
  ScratchCwd cwd("hpo_built_help");
  for (const auto& args : std::vector<std::vector<std::string>>{
           {"tadah", "-h"}, {"tadah", "hpo", "-h"}, {"tadah", "explain", "hpo"}}) {
    const CliRun r = run_tadah(args);
    INFO(args[1] << ": status " << r.status << "\n" << reported(r));
    CHECK(r.status == 0);
    CHECK_FALSE(contains(r.out, "which this tadah was built without"));
    CHECK_FALSE(r.out.empty());
  }
  const CliRun h = run_tadah({"tadah", "-h"});
  CHECK(contains(one_line(h.out),
                 "hpo Optimises the model architecture and hyperparameters."));
}
#endif  // TADAH_ENABLE_NFP

/* ---- [descriptor]: a TYPE2B line needs its list keys -------------------- */

/* Pins: `tadah train` refuses a D2_BP 5 5 model without CGRID2B and
 * SGRID2B: exit 1 naming both keys and the line,
 *   TYPE2B D2_BP takes 5 CGRID2B values and 5 SGRID2B values, but CGRID2B
 *   and SGRID2B are not set: TYPE2B D2_BP 5 5 * *
 * and no potential written; with the grids the same configuration trains
 * (exit 0).
 * Before the fix: a segmentation fault (exit status 139 from the shell)
 * that ends the test process. */
TEST_CASE("Subcommand: train refuses a TYPE2B descriptor without its grid "
          "keys, naming them",
          "[TadahCLI][exit-status][train][descriptor]") {
  ScratchCwd cwd("train_no_grids");
  write_ta_dimer_training();
  const std::string head =
      "DBFILE db.tadah\nATOM Ta\nWATOM 73\nINIT2B true\nINITMB false\n"
      "TYPE2B D2_BP 5 5 * *\nRCTYPE2B Cut_Cos\nRCUT2B 4.5\n"
      "FORCE false\nSTRESS false\nBIAS false\nNORM false\nVERBOSE 0\n"
      "MODEL M_BLR BF_Linear\nLAMBDA 1e-3\nOALGO 1\n";
  std::ofstream("nogrid.train") << head;
  const CliRun r = run_tadah({"tadah", "train", "-c", "nogrid.train", "-o",
                              "pot.tadah"});
  const std::string rep = reported(r);
  INFO("status " << r.status << "\n" << rep);
  CHECK(r.status == 1);
  CHECK(contains(rep, "TYPE2B D2_BP takes 5 CGRID2B values and 5 SGRID2B "
                      "values, but CGRID2B and SGRID2B are not set: TYPE2B "
                      "D2_BP 5 5 * *"));
  CHECK_FALSE(std::filesystem::exists("pot.tadah"));

  std::ofstream("grid.train")
      << head << "CGRID2B 2.5 3.0 3.5 4.0 4.5\nSGRID2B 0.5 0.5 0.5 0.5 0.5\n";
  const CliRun g = run_tadah({"tadah", "train", "-c", "grid.train", "-o",
                              "pot.tadah"});
  INFO("with the grids: status " << g.status << "\n" << reported(g));
  CHECK(g.status == 0);
  CHECK(std::filesystem::exists("pot.tadah"));
}

/* ---- [oalgo]: fewer data rows than weights ----------------------------- */

namespace {

/* What the OALGO 2 and 4 messages say of the zero data rows. */
const char *const kZeroDataRows =
    " (a data row that is zero in every column adds nothing to the rank: a "
    "row of priority 0, or a row the descriptors make zero, such as the y "
    "and z force rows of a linear molecule along x)";

}  // namespace

/* Pins: `tadah train` with M_BLR BF_Linear, TYPE2B D2_BP 3 3 (3 weights),
 * two structures and energies only (2 data rows), LAMBDA 0 and OALGO 4
 * exits 1 with "OALGO 4 (normal equations, Cholesky) cannot fit these 3
 * weights: the design matrix has 2 data rows for 3 weights, LAMBDA 0:
 * fewer data rows than weights, so it does not have full column rank and
 * the least-squares weights are not unique." and the remedies, and writes
 * no potential; OALGO 2 the same with "OALGO 2 (DGELS, QR)"; OALGO 1 fits
 * it (exit 0, the minimum-norm weights).
 * Before the fix: whether OALGO 4 stopped depended on the rounding of
 * Phi^T Phi: the tadah executable exited 0 and wrote 'WEIGHTS
 * -120782.129684208601 36264.9827692464169 -27.1111461915769318' (DPOTRF
 * factored the rounded matrix; with D2_BP 4 4 or 5 5 it stopped), while in
 * this test process DPOTRF stopped with "the matrix Phi^T Phi of the design
 * matrix (2 data rows for 3 weights, LAMBDA 0) is not positive definite (its
 * leading minor of order 3 is not), ..."; OALGO 2 said "the design matrix
 * (2 data rows for 3 weights, LAMBDA 0) does not have full column rank: its
 * column 3 is zero or a linear combination of the columns before it (fewer
 * data rows than weights), ...".
 * Before the fix that states the remedies: the refusal read "... are not
 * unique. Use OALGO 1 (DGELSD) or OALGO 3 (SVD), which give the minimum-norm
 * weights, LAMBDA > 0 ...", an instruction: the check fails. Before the fix
 * of the remedies' wording: "... The fit is made with OALGO 1 (DGELSD) ...",
 * which reads as a fit that was made. */
TEST_CASE("Subcommand: train with OALGO 4 or 2 refuses fewer data rows than "
          "weights",
          "[TadahCLI][exit-status][train][oalgo]") {
  ScratchCwd cwd("train_oalgo_few_rows");
  std::ofstream("db.tadah")
      << "s1\n1.0 1.0 1.0\n-0.30\n"
         "3.301 0.0 0.0\n0.0 3.301 0.0\n0.0 0.0 6.602\n"
         "0.0 0.0 0.0\n0.0 0.0 0.0\n0.0 0.0 0.0\n"
         "Ta 0.05 -0.03 0.02  0.31 -0.12 0.05\n"
         "Ta 1.70 1.62 1.68  -0.22 0.17 -0.09\n"
         "Ta -0.04 0.06 3.33  0.04 0.21 -0.15\n"
         "Ta 1.61 1.69 4.92  -0.13 -0.26 0.19\n\n"
         "s2\n1.0 1.0 1.0\n-0.24\n"
         "3.301 0.0 0.0\n0.0 3.301 0.0\n0.0 0.0 6.602\n"
         "0.0 0.0 0.0\n0.0 0.0 0.0\n0.0 0.0 0.0\n"
         "Ta 0.10 0.00 -0.05  -0.27 0.08 0.14\n"
         "Ta 1.66 1.58 1.62  0.19 -0.23 0.06\n"
         "Ta 0.02 -0.08 3.27  0.11 0.16 -0.21\n"
         "Ta 1.69 1.71 5.00  -0.03 -0.01 0.01\n\n";
  const std::string head =
      "DBFILE db.tadah\nATOM Ta\nWATOM 1\nINIT2B true\nINITMB false\n"
      "TYPE2B D2_BP 3 3 Ta Ta\nCGRID2B 0.3 1.0 1.8\nSGRID2B 2.0 3.4 5.0\n"
      "RCTYPE2B Cut_Cos\nRCUT2B 5.0\nFORCE false\nSTRESS false\n"
      "BIAS false\nNORM false\nVERBOSE 0\nMODEL M_BLR BF_Linear\n"
      "LAMBDA 0\n";
  const std::string few =
      " cannot fit these 3 weights: the design matrix has 2 data rows for 3 "
      "weights, LAMBDA 0: fewer data rows than weights, so it does not have "
      "full column rank and the least-squares weights are not unique. A fit "
      "is possible with OALGO 1 (DGELSD) or OALGO 3 (SVD), which give the "
      "minimum-norm weights, with LAMBDA > 0";
  for (const auto& [oalgo, name] :
       {std::pair<std::string, std::string>{"4", "normal equations, Cholesky"},
        {"2", "DGELS, QR"}}) {
    INFO("OALGO " << oalgo);
    std::ofstream("c" + oalgo + ".train") << head << "OALGO " << oalgo << "\n";
    const CliRun r = run_tadah({"tadah", "train", "-c", "c" + oalgo + ".train",
                                "-o", "pot" + oalgo + ".tadah"});
    const std::string rep = reported(r);
    INFO("status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    CHECK(contains(rep, "OALGO " + oalgo + " (" + name + ")" + few));
    CHECK_FALSE(std::filesystem::exists("pot" + oalgo + ".tadah"));
  }
  std::ofstream("c1.train") << head << "OALGO 1\n";
  const CliRun g = run_tadah({"tadah", "train", "-c", "c1.train", "-o",
                              "pot1.tadah"});
  INFO("OALGO 1: status " << g.status << "\n" << reported(g));
  CHECK(g.status == 0);
  CHECK(std::filesystem::exists("pot1.tadah"));
}

/* Pins: the case above with a third structure of energy priority 0 (its
 * priorities line '0.0 1.0 1.0'), whose energy row is zero in the design
 * matrix: `tadah train` with OALGO 4 exits 1 with "OALGO 4 (normal
 * equations, Cholesky) cannot fit these 3 weights: the design matrix has 2
 * non-zero data rows of 3 (a data row that is zero in every column adds
 * nothing to the rank: a row of priority 0, or a row the descriptors make
 * zero, such as the y and z force rows of a linear molecule along x) for 3
 * weights, LAMBDA 0: fewer non-zero data rows than weights, so it does not
 * have full column rank and the least-squares weights are not unique." and
 * the remedies, and writes no potential; OALGO 2 the same with "OALGO 2
 * (DGELS, QR)"; OALGO 1 fits it (exit 0). Zero rows of priority 1: the
 * next case. (Until the zero-row wording fix the parenthesis was "(a row of
 * weight 0 is zero)", and until the priority wording "a row of weight 0".)
 * Before the fix: the pre-check counted the zero row as data (3 data rows
 * for 3 weights) and LAPACK decided: both exited 1, OALGO 4 with "the
 * matrix Phi^T Phi of the design matrix (3 data rows for 3 weights, LAMBDA
 * 0) is not positive definite (its leading minor of order 3 is not), ..."
 * and OALGO 2 with "the design matrix (3 data rows for 3 weights, LAMBDA 0)
 * does not have full column rank: its column 3 is zero or a linear
 * combination of the columns before it, ..." (whether DPOTRF stops depends
 * on the rounding of Phi^T Phi: on 2 other rows, of sine values, and a
 * zero row for 3 weights, test_linear_regressor_messages.cpp's OALGO 4
 * fitted without an error).
 * Before the fix that states the remedies: the refusal read "... are not
 * unique. Use OALGO 1 (DGELSD) or OALGO 3 (SVD), which give the minimum-norm
 * weights, LAMBDA > 0 ...", an instruction: the check fails. Before the fix
 * of the remedies' wording: "... The fit is made with OALGO 1 (DGELSD) ...",
 * which reads as a fit that was made. */
TEST_CASE("Subcommand: train with OALGO 4 or 2 refuses fewer non-zero data "
          "rows than weights, a structure of priority 0 included",
          "[TadahCLI][exit-status][train][oalgo]") {
  ScratchCwd cwd("train_oalgo_weight_zero_rows");
  std::ofstream("db.tadah")
      << "s1\n1.0 1.0 1.0\n-0.30\n"
         "3.301 0.0 0.0\n0.0 3.301 0.0\n0.0 0.0 6.602\n"
         "0.0 0.0 0.0\n0.0 0.0 0.0\n0.0 0.0 0.0\n"
         "Ta 0.05 -0.03 0.02  0.31 -0.12 0.05\n"
         "Ta 1.70 1.62 1.68  -0.22 0.17 -0.09\n"
         "Ta -0.04 0.06 3.33  0.04 0.21 -0.15\n"
         "Ta 1.61 1.69 4.92  -0.13 -0.26 0.19\n\n"
         "s2\n1.0 1.0 1.0\n-0.24\n"
         "3.301 0.0 0.0\n0.0 3.301 0.0\n0.0 0.0 6.602\n"
         "0.0 0.0 0.0\n0.0 0.0 0.0\n0.0 0.0 0.0\n"
         "Ta 0.10 0.00 -0.05  -0.27 0.08 0.14\n"
         "Ta 1.66 1.58 1.62  0.19 -0.23 0.06\n"
         "Ta 0.02 -0.08 3.27  0.11 0.16 -0.21\n"
         "Ta 1.69 1.71 5.00  -0.03 -0.01 0.01\n\n"
         "s3\n0.0 1.0 1.0\n-0.27\n"
         "3.301 0.0 0.0\n0.0 3.301 0.0\n0.0 0.0 6.602\n"
         "0.0 0.0 0.0\n0.0 0.0 0.0\n0.0 0.0 0.0\n"
         "Ta 0.00 0.04 0.03  0.12 -0.05 0.07\n"
         "Ta 1.64 1.66 1.70  -0.11 0.09 -0.02\n"
         "Ta 0.06 -0.02 3.30  0.03 0.10 -0.08\n"
         "Ta 1.66 1.63 4.96  -0.05 -0.12 0.06\n\n";
  const std::string head =
      "DBFILE db.tadah\nATOM Ta\nWATOM 1\nINIT2B true\nINITMB false\n"
      "TYPE2B D2_BP 3 3 Ta Ta\nCGRID2B 0.3 1.0 1.8\nSGRID2B 2.0 3.4 5.0\n"
      "RCTYPE2B Cut_Cos\nRCUT2B 5.0\nFORCE false\nSTRESS false\n"
      "BIAS false\nNORM false\nVERBOSE 0\nMODEL M_BLR BF_Linear\n"
      "LAMBDA 0\n";
  const std::string few =
      " cannot fit these 3 weights: the design matrix has 2 non-zero data "
      "rows of 3" + std::string(kZeroDataRows) + " for 3 weights, LAMBDA 0: "
      "fewer non-zero data rows than weights, so it does not have full column "
      "rank and the least-squares weights are not unique. A fit is possible "
      "with OALGO 1 (DGELSD) or OALGO 3 (SVD), which give the minimum-norm "
      "weights, with LAMBDA > 0";
  for (const auto& [oalgo, name] :
       {std::pair<std::string, std::string>{"4", "normal equations, Cholesky"},
        {"2", "DGELS, QR"}}) {
    INFO("OALGO " << oalgo);
    std::ofstream("c" + oalgo + ".train") << head << "OALGO " << oalgo << "\n";
    const CliRun r = run_tadah({"tadah", "train", "-c", "c" + oalgo + ".train",
                                "-o", "pot" + oalgo + ".tadah"});
    const std::string rep = reported(r);
    INFO("status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    CHECK(contains(rep, "OALGO " + oalgo + " (" + name + ")" + few));
    CHECK_FALSE(std::filesystem::exists("pot" + oalgo + ".tadah"));
  }
  std::ofstream("c1.train") << head << "OALGO 1\n";
  const CliRun g = run_tadah({"tadah", "train", "-c", "c1.train", "-o",
                              "pot1.tadah"});
  INFO("OALGO 1: status " << g.status << "\n" << reported(g));
  CHECK(g.status == 0);
  CHECK(std::filesystem::exists("pot1.tadah"));
}

/* Pins: data rows that are zero with priority 1 are counted as zero, and
 * the message does not blame priority 0 (every priorities line is '1.0 1.0
 * 1.0'): `tadah train` with OALGO 4 and with OALGO 2
 * exits 1 and writes no potential,
 *  - on a Ta dimer along x with FORCE true and TYPE2B D2_BP 4 4 (4
 *    weights), whose y and z force rows are zero (1 energy and 6 force
 *    rows, 3 not zero): "... cannot fit these 4 weights: the design matrix
 *    has 3 non-zero data rows of 7 (a data row that is zero in every
 *    column adds nothing to the rank: a row of priority 0, or a row the
 *    descriptors make zero, such as the y and z force rows of a linear
 *    molecule along x) for 4 weights, LAMBDA 0: fewer non-zero data rows
 *    than weights, ...";
 *  - on the two structures of the cases above and an isolated Ta atom (no
 *    neighbour within RCUT2B 5 in a 30 A box), energies only, BIAS false,
 *    D2_BP 3 3, whose energy row is zero: "... cannot fit these 3 weights:
 *    the design matrix has 2 non-zero data rows of 3 (a data row that is
 *    zero ...) for 3 weights, ...".
 * Before the fix: the counts and the refusals were the same, but the
 * parenthesis was "(a row of weight 0 is zero)", which sent the user to
 * look for a weight 0 that none of these rows has.
 * Before the fix that states the remedies: the refusal read "... are not
 * unique. Use OALGO 1 (DGELSD) or OALGO 3 (SVD), which give the minimum-norm
 * weights, LAMBDA > 0 ...", an instruction: the check fails. Before the fix
 * of the remedies' wording: "... The fit is made with OALGO 1 (DGELSD) ...",
 * which reads as a fit that was made. */
TEST_CASE("Subcommand: train with OALGO 4 or 2 counts zero data rows of "
          "priority 1, a dimer's y and z forces and an isolated atom's energy",
          "[TadahCLI][exit-status][train][oalgo]") {
  ScratchCwd cwd("train_oalgo_zero_rows_weight_one");
  std::ofstream("dimer.tadah")
      << "dimer\n1.0 1.0 1.0\n-1.20\n"
         "20.0 0.0 0.0\n0.0 20.0 0.0\n0.0 0.0 20.0\n"
         "0.0 0.0 0.0\n0.0 0.0 0.0\n0.0 0.0 0.0\n"
         "Ta 5.0 5.0 5.0  0.31 0.0 0.0\n"
         "Ta 7.3 5.0 5.0  -0.31 0.0 0.0\n\n";
  std::ofstream("isolated.tadah")
      << "s1\n1.0 1.0 1.0\n-0.30\n"
         "3.301 0.0 0.0\n0.0 3.301 0.0\n0.0 0.0 6.602\n"
         "0.0 0.0 0.0\n0.0 0.0 0.0\n0.0 0.0 0.0\n"
         "Ta 0.05 -0.03 0.02  0.31 -0.12 0.05\n"
         "Ta 1.70 1.62 1.68  -0.22 0.17 -0.09\n"
         "Ta -0.04 0.06 3.33  0.04 0.21 -0.15\n"
         "Ta 1.61 1.69 4.92  -0.13 -0.26 0.19\n\n"
         "s2\n1.0 1.0 1.0\n-0.24\n"
         "3.301 0.0 0.0\n0.0 3.301 0.0\n0.0 0.0 6.602\n"
         "0.0 0.0 0.0\n0.0 0.0 0.0\n0.0 0.0 0.0\n"
         "Ta 0.10 0.00 -0.05  -0.27 0.08 0.14\n"
         "Ta 1.66 1.58 1.62  0.19 -0.23 0.06\n"
         "Ta 0.02 -0.08 3.27  0.11 0.16 -0.21\n"
         "Ta 1.69 1.71 5.00  -0.03 -0.01 0.01\n\n"
         "isolated Ta atom\n1.0 1.0 1.0\n-0.01\n"
         "30.0 0.0 0.0\n0.0 30.0 0.0\n0.0 0.0 30.0\n"
         "0.0 0.0 0.0\n0.0 0.0 0.0\n0.0 0.0 0.0\n"
         "Ta 0.0 0.0 0.0  0.0 0.0 0.0\n\n";
  const std::string common =
      "ATOM Ta\nWATOM 1\nINIT2B true\nINITMB false\nRCTYPE2B Cut_Cos\n"
      "RCUT2B 5.0\nSTRESS false\nBIAS false\nNORM false\nVERBOSE 0\n"
      "MODEL M_BLR BF_Linear\nLAMBDA 0\n";
  const std::string dimer =
      "DBFILE dimer.tadah\n" + common +
      "TYPE2B D2_BP 4 4 Ta Ta\nCGRID2B 0.3 1.0 1.8 2.5\n"
      "SGRID2B 2.0 3.4 5.0 1.0\nFORCE true\n";
  const std::string isolated =
      "DBFILE isolated.tadah\n" + common +
      "TYPE2B D2_BP 3 3 Ta Ta\nCGRID2B 0.3 1.0 1.8\nSGRID2B 2.0 3.4 5.0\n"
      "FORCE false\n";
  const std::string tail =
      " LAMBDA 0: fewer non-zero data rows than weights, so it does not have "
      "full column rank and the least-squares weights are not unique. A fit "
      "is possible with OALGO 1 (DGELSD) or OALGO 3 (SVD), which give the "
      "minimum-norm weights, with LAMBDA > 0";
  for (const auto& [oalgo, name] :
       {std::pair<std::string, std::string>{"4", "normal equations, Cholesky"},
        {"2", "DGELS, QR"}}) {
    for (const auto& [label, config, expected] :
         {std::tuple<std::string, std::string, std::string>{
              "dimer", dimer,
              " cannot fit these 4 weights: the design matrix has 3 non-zero "
              "data rows of 7" + std::string(kZeroDataRows) + " for 4 weights,"},
          {"isolated", isolated,
           " cannot fit these 3 weights: the design matrix has 2 non-zero data "
           "rows of 3" + std::string(kZeroDataRows) + " for 3 weights,"}}) {
      INFO(label << ", OALGO " << oalgo);
      const std::string base = label + oalgo;
      std::ofstream(base + ".train") << config << "OALGO " << oalgo << "\n";
      const CliRun r = run_tadah({"tadah", "train", "-c", base + ".train",
                                  "-o", base + ".pot"});
      const std::string rep = reported(r);
      INFO("status " << r.status << "\n" << rep);
      CHECK(r.status == 1);
      CHECK(contains(rep, "OALGO " + oalgo + " (" + name + ")" + expected +
                              tail));
      CHECK_FALSE(contains(rep, "weight 0 is zero"));
      CHECK_FALSE(std::filesystem::exists(base + ".pot"));
    }
  }
}

/* ---- [eam]: D2_EAM and DM_EAM read the setfl file named by SETFL ------ */

namespace {

/* A single-element (Ta) setfl file with analytic tables, cutoff `rc_text`
 * (4.5 A unless given), rc: F(rho) = -sqrt(rho) on rho in [0, 50],
 * rho(r) = (rc - r)^2 and phi(r) = 0.5 (rc - r)^3 (z2r = r phi) below the
 * cutoff, 0 beyond (tables to 5 A). */
std::string small_ta_setfl(const std::string& rc_text = "4.5") {
  const int nrho = 501, nr = 501;
  const double drho = 0.1, dr = 0.01, rc = std::stod(rc_text);
  std::ostringstream os;
  os.precision(17);
  os << "Small Ta setfl of the DM_EAM CLI test\nanalytic tables\n\n"
     << "1 Ta\n"
     << nrho << " " << drho << " " << nr << " " << dr << " " << rc_text
     << "\n"
     << "73 180.95 3.304 bcc\n";
  for (int i = 0; i < nrho; ++i) os << -std::sqrt(i * drho) << "\n";
  for (int i = 0; i < nr; ++i) {
    const double r = i * dr;
    os << (r < rc ? (rc - r) * (rc - r) : 0.0) << "\n";
  }
  for (int i = 0; i < nr; ++i) {
    const double r = i * dr;
    os << (r < rc ? r * 0.5 * (rc - r) * (rc - r) * (rc - r) : 0.0) << "\n";
  }
  return os.str();
}

/* A two-atom bcc Ta cell with lattice parameter `a`, energy `e`, no forces. */
std::string bcc_ta_entry(const std::string& label, double a, double e) {
  std::ostringstream os;
  os.precision(17);
  os << label << "\n1 1 1\n" << e << " 0.0\n"
     << a << " 0 0\n0 " << a << " 0\n0 0 " << a << "\n"
     << "0 0 0\n0 0 0\n0 0 0\n"
     << "Ta 0 0 0 0 0 0\n"
     << "Ta " << a / 2 << " " << a / 2 << " " << a / 2 << " 0 0 0\n\n";
  return os.str();
}

}  // namespace

/* Pins: `tadah train` builds a DM_EAM model from a configuration file, the
 * many-body block alone (INIT2B false) and with the D2_EAM pair term, both
 * reading the one SETFL file (`TYPEMB DM_EAM 0 Ta Ta`, `TYPE2B D2_EAM 0 Ta
 * Ta`, the cutoffs the file's 4.5 A): exit 0, and the potential carries
 * `SETFL eam_small.alloy` and the TYPEMB (and TYPE2B) line as written;
 * `tadah predict` loads that potential (exit 0) and writes energy.pred.
 * Before the fix: DM_EAM read its setfl path from TYPEMB parameter 1, the
 * placeholder the TYPEMB format requires to be an unsigned integer, so no
 * configuration could build it: train exited 1 with "EAMSetflIO: setfl file
 * does not exist: 0" and wrote no potential. */
TEST_CASE("Subcommand: train builds a DM_EAM model from the SETFL file",
          "[TadahCLI][train][eam]") {
  ScratchCwd cwd("train_dm_eam");
  { std::ofstream os("eam_small.alloy"); os << small_ta_setfl(); }
  {
    std::ofstream os("db.tadah");
    os << bcc_ta_entry("bcc-Ta-3.25", 3.25, -16.2)
       << bcc_ta_entry("bcc-Ta-3.30", 3.30, -16.3)
       << bcc_ta_entry("bcc-Ta-3.35", 3.35, -16.1);
  }
  for (const bool with_pair : {false, true}) {
    INFO("D2_EAM pair term: " << with_pair);
    for (const char* f : {"pot.tadah", "energy.pred"})
      std::filesystem::remove(f);
    {
      std::ofstream os("config");
      os << "DBFILE db.tadah\nATOM Ta\nWATOM 1\n";
      if (with_pair)
        os << "INIT2B true\nTYPE2B D2_EAM 0 Ta Ta\n"
              "RCTYPE2B Cut_Dummy\nRCUT2B 4.5\n";
      else
        os << "INIT2B false\n";
      os << "INITMB true\nTYPEMB DM_EAM 0 Ta Ta\n"
            "RCTYPEMB Cut_Dummy\nRCUTMB 4.5\nSETFL eam_small.alloy\n"
            "FORCE false\nSTRESS false\nBIAS false\nNORM false\nVERBOSE 0\n"
            "MODEL M_BLR BF_Linear\nLAMBDA 1e-3\nOALGO 1\n";
    }
    const CliRun t = run_tadah({"tadah", "train", "-c", "config", "-o",
                                "pot.tadah"});
    INFO("train status " << t.status << "\n" << reported(t));
    CHECK_FALSE(contains(reported(t), "setfl file does not exist"));
    REQUIRE(t.status == 0);
    REQUIRE(std::filesystem::exists("pot.tadah"));
    const std::string pot = slurp("pot.tadah");
    INFO("pot.tadah:\n" << pot);
    CHECK(line_tokens(pot, "SETFL") ==
          std::vector<std::string>{"SETFL", "eam_small.alloy"});
    CHECK(line_tokens(pot, "TYPEMB") ==
          std::vector<std::string>{"TYPEMB", "DM_EAM", "0", "Ta", "Ta"});
    if (with_pair)
      CHECK(line_tokens(pot, "TYPE2B") ==
            std::vector<std::string>{"TYPE2B", "D2_EAM", "0", "Ta", "Ta"});

    const CliRun p = run_tadah({"tadah", "predict", "-p", "pot.tadah", "-d",
                                "db.tadah"});
    INFO("predict status " << p.status << "\n" << reported(p));
    CHECK(p.status == 0);
    CHECK(std::filesystem::exists("energy.pred"));
  }
}

/* Pins: `tadah train` refuses an EAM cutoff other than the setfl file's
 * (4.5 A): a DM_EAM model with RCUTMB 3.5, and a D2_EAM model with RCUT2B
 * 3.5, exit 1 naming the descriptor, the key, both values and the file,
 *   DM_EAM: RCUTMB 3.5 differs from the cutoff 4.5 of the setfl file
 *   'eam_small.alloy' (SETFL); the RCUTMB of DM_EAM is the file's cutoff,
 *   4.5.
 * and write no potential.
 * Before the fix: both exited 0 and wrote a potential that records the
 * file's cutoff (RCUTMB 4.5, RCUT2B 4.5), while the weights had been fitted
 * with the descriptor evaluated at 3.5, so `tadah predict` on that file
 * evaluated a model that was never fitted.
 * Before the fix that states the rule: the refusal ended "(SETFL); set RCUTMB
 * to the file's cutoff, 4.5.", an instruction: the check fails. */
TEST_CASE("Subcommand: train refuses an EAM cutoff other than the setfl "
          "file's",
          "[TadahCLI][train][eam]") {
  ScratchCwd cwd("train_eam_cutoff");
  { std::ofstream os("eam_small.alloy"); os << small_ta_setfl(); }
  {
    std::ofstream os("db.tadah");
    os << bcc_ta_entry("bcc-Ta-3.25", 3.25, -16.2)
       << bcc_ta_entry("bcc-Ta-3.30", 3.30, -16.3)
       << bcc_ta_entry("bcc-Ta-3.35", 3.35, -16.1);
  }
  for (const bool pair : {false, true}) {
    const std::string label = pair ? "D2_EAM" : "DM_EAM";
    const std::string key = pair ? "RCUT2B" : "RCUTMB";
    INFO(label << " with " << key << " 3.5");
    std::filesystem::remove("pot.tadah");
    {
      std::ofstream os("config");
      os << "DBFILE db.tadah\nATOM Ta\nWATOM 1\n";
      if (pair)
        os << "INIT2B true\nTYPE2B D2_EAM 0 Ta Ta\n"
              "RCTYPE2B Cut_Dummy\nRCUT2B 3.5\nINITMB false\n";
      else
        os << "INIT2B false\nINITMB true\nTYPEMB DM_EAM 0 Ta Ta\n"
              "RCTYPEMB Cut_Dummy\nRCUTMB 3.5\n";
      os << "SETFL eam_small.alloy\n"
            "FORCE false\nSTRESS false\nBIAS false\nNORM false\nVERBOSE 0\n"
            "MODEL M_BLR BF_Linear\nLAMBDA 0\nOALGO 1\n";
    }
    const CliRun t = run_tadah({"tadah", "train", "-c", "config", "-o",
                                "pot.tadah"});
    const std::string rep = reported(t);
    INFO("train status " << t.status << "\n" << rep);
    const std::string pot = std::filesystem::exists("pot.tadah")
                                ? slurp("pot.tadah")
                                : std::string("(none)");
    INFO("pot.tadah:\n" << pot);
    CHECK(t.status == 1);
    CHECK(contains(rep, label + ": " + key +
                            " 3.5 differs from the cutoff 4.5 of the setfl "
                            "file 'eam_small.alloy' (SETFL); the " + key +
                            " of " + label + " is the file's cutoff, 4.5."));
    CHECK_FALSE(std::filesystem::exists("pot.tadah"));
  }
}

namespace {

/* The significant digits of a decimal token (leading zeros, sign, point
 * and exponent do not count). */
std::size_t significant_digits(const std::string& tok) {
  const std::string mant = tok.substr(0, tok.find_first_of("eE"));
  std::size_t n = 0;
  bool lead = true;
  for (const char ch : mant) {
    if (ch < '0' || ch > '9') continue;
    if (lead && ch == '0') continue;
    lead = false;
    ++n;
  }
  return n;
}

/* The NUMERIC EAM case of `label`: its configuration lines (the setfl
 * file eam_rc.alloy) and the line of the cutoff key it writes. */
struct NumericEamCase {
  const char* label;
  const char* config;
  const char* key;
  std::vector<std::string> cutoff_line;
};

}  // namespace

/* Pins: `tadah train` with NUMERIC 6 and with NUMERIC 3 of a model that
 * reads the setfl file eam_rc.alloy, whose cutoff is 4.51234567891 (12
 * significant digits), writes the cutoff exactly and `tadah predict` loads
 * the potential (exit 0, energy.pred written): D2_EAM ('RCUT2B
 * 4.51234567891'), DM_EAM ('RCUTMB 4.51234567891') and a D2_mJoin of D2_BP
 * and D2_EAM ('RCUT2B 4.123456789 4.51234567891', the D2_BP cutoff exact
 * too), each WEIGHTS value with at most NUMERIC significant digits (the
 * rounding NUMERIC asks for).
 * Before the fix: train wrote the cutoff rounded ('RCUT2B 4.51235',
 * 'RCUT2B 4.51' at NUMERIC 3; the mJoin line '4.12346 4.51235'), with only
 * the general NUMERIC warning, and predict exited 1 on the potential train
 * had just written: 'D2_EAM: RCUT2B 4.51235 differs from the cutoff
 * 4.51234567891 of the setfl file 'eam_rc.alloy' (SETFL); set RCUT2B to the
 * file's cutoff, 4.51234567891.' */
TEST_CASE("Subcommand: train with NUMERIC below 17 writes the EAM cutoff "
          "exactly and predict loads the potential",
          "[TadahCLI][train][eam][numeric]") {
  ScratchCwd cwd("train_eam_numeric");
  { std::ofstream os("eam_rc.alloy"); os << small_ta_setfl("4.51234567891"); }
  {
    std::ofstream os("db.tadah");
    os << bcc_ta_entry("bcc-Ta-3.25", 3.25, -16.2)
       << bcc_ta_entry("bcc-Ta-3.30", 3.30, -16.3)
       << bcc_ta_entry("bcc-Ta-3.35", 3.35, -16.1);
  }
  const std::vector<NumericEamCase> cases = {
      {"D2_EAM",
       "INIT2B true\nTYPE2B D2_EAM 0 Ta Ta\nRCTYPE2B Cut_Dummy\n"
       "RCUT2B 4.51234567891\nINITMB false\n",
       "RCUT2B", {"RCUT2B", "4.51234567891"}},
      {"DM_EAM",
       "INIT2B false\nINITMB true\nTYPEMB DM_EAM 0 Ta Ta\n"
       "RCTYPEMB Cut_Dummy\nRCUTMB 4.51234567891\n",
       "RCUTMB", {"RCUTMB", "4.51234567891"}},
      {"D2_mJoin D2_BP + D2_EAM",
       "INIT2B true\nTYPE2B D2_mJoin\nTYPE2B D2_BP 2 2 Ta Ta\n"
       "TYPE2B D2_EAM 0 Ta Ta\nRCTYPE2B Cut_Cos Cut_Dummy\n"
       "RCUT2B 4.123456789 4.51234567891\nCGRID2B 2.6 3.3\n"
       "SGRID2B 1.0 1.0\nINITMB false\n",
       "RCUT2B", {"RCUT2B", "4.123456789", "4.51234567891"}}};
  for (const NumericEamCase& tc : cases) {
    for (const int numeric : {6, 3}) {
      INFO(tc.label << ", NUMERIC " << numeric);
      for (const char* f : {"pot.tadah", "energy.pred"})
        std::filesystem::remove(f);
      {
        std::ofstream os("config");
        os << "DBFILE db.tadah\nATOM Ta\nWATOM 1\n" << tc.config
           << "SETFL eam_rc.alloy\nNUMERIC " << numeric << "\n"
           << "FORCE false\nSTRESS false\nBIAS false\nNORM false\n"
              "VERBOSE 0\nMODEL M_BLR BF_Linear\nLAMBDA 1e-3\nOALGO 1\n";
      }
      const CliRun t = run_tadah({"tadah", "train", "-c", "config", "-o",
                                  "pot.tadah"});
      INFO("train status " << t.status << "\n" << reported(t));
      REQUIRE(t.status == 0);
      REQUIRE(std::filesystem::exists("pot.tadah"));
      const std::string pot = slurp("pot.tadah");
      INFO("pot.tadah:\n" << pot);
      CHECK(line_tokens(pot, tc.key) == tc.cutoff_line);
      const std::vector<std::string> w = line_tokens(pot, "WEIGHTS");
      REQUIRE(w.size() > 1);
      for (std::size_t i = 1; i < w.size(); ++i)
        CHECK(significant_digits(w[i]) <= static_cast<std::size_t>(numeric));

      const CliRun p = run_tadah({"tadah", "predict", "-p", "pot.tadah",
                                  "-d", "db.tadah"});
      INFO("predict status " << p.status << "\n" << reported(p));
      CHECK_FALSE(contains(reported(p), "differs from the cutoff"));
      CHECK(p.status == 0);
      CHECK(std::filesystem::exists("energy.pred"));
    }
  }
}

/* Pins: `tadah train` of a D2_Blip model with NUMERIC 6 and NUMERIC 3 writes
 * its cutoff and grids exactly, 'RCUT2B 5.123456789', 'CGRID2B 2.81234567
 * 3.51234567 4.21234567', 'SGRID2B 2.0123456789 2 2' (the shortest text
 * that reads back as the configured value), every WEIGHTS value with at
 * most NUMERIC significant digits and at least one with exactly NUMERIC
 * (they are fitted numbers), and `tadah predict` loads the potential.
 * Before the fix: the cutoff and grids were rounded like the weights
 * ('RCUT2B 5.12346', 'CGRID2B 2.81235 3.51235 4.21235', 'SGRID2B 2.01235 2
 * 2' at NUMERIC 6), so the potential described another descriptor than the
 * one its weights were fitted with. The weights' rounding passes as well (a
 * pin: NUMERIC still rounds them). */
TEST_CASE("Subcommand: train with NUMERIC below 17 rounds the weights and "
          "writes the cutoff and grids exactly",
          "[TadahCLI][train][numeric]") {
  ScratchCwd cwd("train_blip_numeric");
  {
    std::ofstream os("db.tadah");
    os << bcc_ta_entry("bcc-Ta-3.25", 3.25, -16.2)
       << bcc_ta_entry("bcc-Ta-3.30", 3.30, -16.3)
       << bcc_ta_entry("bcc-Ta-3.35", 3.35, -16.1)
       << bcc_ta_entry("bcc-Ta-3.40", 3.40, -15.9);
  }
  for (const int numeric : {6, 3}) {
    INFO("NUMERIC " << numeric);
    for (const char* f : {"pot.tadah", "energy.pred"})
      std::filesystem::remove(f);
    {
      std::ofstream os("config");
      os << "DBFILE db.tadah\nATOM Ta\nWATOM 1\nINIT2B true\nINITMB false\n"
            "TYPE2B D2_Blip 3 3 Ta Ta\nRCTYPE2B Cut_Cos\n"
            "RCUT2B 5.123456789\nCGRID2B 2.81234567 3.51234567 4.21234567\n"
            "SGRID2B 2.0123456789 2.0 2.0\n"
         << "NUMERIC " << numeric << "\n"
         << "FORCE false\nSTRESS false\nBIAS false\nNORM false\n"
            "VERBOSE 0\nMODEL M_BLR BF_Linear\nLAMBDA 1e-3\nOALGO 1\n";
    }
    const CliRun t = run_tadah({"tadah", "train", "-c", "config", "-o",
                                "pot.tadah"});
    INFO("train status " << t.status << "\n" << reported(t));
    REQUIRE(t.status == 0);
    const std::string pot = slurp("pot.tadah");
    INFO("pot.tadah:\n" << pot);
    CHECK(line_tokens(pot, "RCUT2B") ==
          std::vector<std::string>{"RCUT2B", "5.123456789"});
    CHECK(line_tokens(pot, "CGRID2B") ==
          std::vector<std::string>{"CGRID2B", "2.81234567", "3.51234567",
                                   "4.21234567"});
    CHECK(line_tokens(pot, "SGRID2B") ==
          std::vector<std::string>{"SGRID2B", "2.0123456789", "2", "2"});
    const std::vector<std::string> w = line_tokens(pot, "WEIGHTS");
    REQUIRE(w.size() == 4);
    std::size_t most = 0;
    for (std::size_t i = 1; i < w.size(); ++i) {
      CHECK(significant_digits(w[i]) <= static_cast<std::size_t>(numeric));
      most = std::max(most, significant_digits(w[i]));
    }
    CHECK(most == static_cast<std::size_t>(numeric));

    const CliRun p = run_tadah({"tadah", "predict", "-p", "pot.tadah", "-d",
                                "db.tadah"});
    INFO("predict status " << p.status << "\n" << reported(p));
    CHECK(p.status == 0);
    CHECK(std::filesystem::exists("energy.pred"));
  }
}

namespace {

/* A bump case of the NUMERIC test below: its descriptor lines and the key
 * lines the potential must carry exactly. */
struct NumericBumpCase {
  const char* label;
  const char* config;
  std::vector<std::vector<std::string>> exact_lines;
};

}  // namespace

/* Pins: `tadah train` with NUMERIC 6 and NUMERIC 3 of a model with one bump
 * under a Cut_Dummy cutoff writes the cutoff and the bump's centre and width
 * exactly, so the bump support c + w/eta of the potential stays within the
 * cutoff, as in the fitted model: `tadah predict` loads the potential
 * without the bump-support WARNING ('has support reaching'), and, in an NFP
 * build, `tadah hpo` with the potential plus a DBFILE line as its -c
 * configuration passes the startup bump-support gate (exit 0, no 'bump-
 * support gate'). The cases: a D2_Blip bump inside the cutoff (RCUT2B
 * 4.9964, CGRID2B 1.9951, SGRID2B 0.6664: c + 2/eta = 4.99630), a D2_Blip5
 * bump that reaches the cutoff (RCUT2B 5.5549999, CGRID2B 3.3350001,
 * SGRID2B 1.351351473094727 = 3/(RCUT2B - CGRID2B), a bump at the edge as
 * tadah hpo's gate and refit --edit allow it), and a DM_REAM Blip density
 * bump inside the cutoff (the D2_Blip numbers on RCUTMB, CGRIDMB and
 * SGRIDMB).
 * Before the fix: each number was rounded on its own, which keeps the order
 * of the cutoff and the knots but not c + w/eta <= RCUT. At NUMERIC 3 the
 * potentials recorded 'RCUT2B 5', 'CGRID2B 2', 'SGRID2B 0.666' (reach
 * 5.003003), 'RCUT2B 5.55', 'CGRID2B 3.34', 'SGRID2B 1.35' (reach 5.562222)
 * and 'RCUTMB 5', 'CGRIDMB 2', 'SGRIDMB 0.666'; predict printed 'WARNING:
 * D2_Blip: bump 1 of CGRID2B (centre 2.000000, eta 0.666000) has support
 * reaching 5.003003, past RCUT2B = 5.000000 ...', and the D2_Blip5 and
 * DM_REAM ones; and hpo exited 1: 'bump-support gate: ... CGRID2B bump 1
 * (centre 2, eta 0.666) reaches 5.003, past RCUT2B = 5 ...'. At NUMERIC 6
 * the D2_Blip and DM_REAM lines (five digits) were written as they are (a
 * pin), and the D2_Blip5 ones were rounded ('RCUT2B 5.555', 'CGRID2B
 * 3.335', 'SGRID2B 1.35135') with a reach of 5.555002, within the
 * tolerance of the checks (no WARNING, hpo exit 0). */
TEST_CASE("Subcommand: a Cut_Dummy bump that train writes with NUMERIC "
          "below 17 stays inside the cutoff for predict and hpo",
          "[TadahCLI][train][numeric][bump]") {
  const std::vector<NumericBumpCase> cases = {
      {"D2_Blip, a bump inside RCUT2B",
       "INIT2B true\nINITMB false\nTYPE2B D2_Blip 1 1 Ta Ta\n"
       "RCTYPE2B Cut_Dummy\nRCUT2B 4.9964\nCGRID2B 1.9951\nSGRID2B 0.6664\n",
       {{"RCUT2B", "4.9964"}, {"CGRID2B", "1.9951"}, {"SGRID2B", "0.6664"}}},
      {"D2_Blip5, a bump reaching RCUT2B",
       "INIT2B true\nINITMB false\nTYPE2B D2_Blip5 1 1 Ta Ta\n"
       "RCTYPE2B Cut_Dummy\nRCUT2B 5.5549999\nCGRID2B 3.3350001\n"
       "SGRID2B 1.351351473094727\n",
       {{"RCUT2B", "5.5549999"},
        {"CGRID2B", "3.3350001"},
        {"SGRID2B", "1.351351473094727"}}},
      {"DM_REAM Blip density, a bump inside RCUTMB",
       "INIT2B false\nINITMB true\nTYPEMB DM_REAM 0 1 1 1 2 2 0 0 Ta Ta\n"
       "RCTYPEMB Cut_Dummy\nRCUTMB 4.9964\nCGRIDMB 1.9951\nSGRIDMB 0.6664\n"
       "AMPGRIDMB 1\nRHOFORM SIGNED\nCEMBFUNC 0 1\nSEMBFUNC 1 1\n",
       {{"RCUTMB", "4.9964"}, {"CGRIDMB", "1.9951"}, {"SGRIDMB", "0.6664"}}}};
  for (std::size_t i = 0; i < cases.size(); ++i) {
    const NumericBumpCase& tc = cases[i];
    for (const int numeric : {6, 3}) {
      INFO(tc.label << ", NUMERIC " << numeric);
      ScratchCwd cwd("train_bump_numeric_" + std::to_string(i) + "_" +
                     std::to_string(numeric));
      {
        std::ofstream os("db.tadah");
        os << bcc_ta_entry("bcc-Ta-3.25", 3.25, -16.2)
           << bcc_ta_entry("bcc-Ta-3.30", 3.30, -16.3)
           << bcc_ta_entry("bcc-Ta-3.35", 3.35, -16.1)
           << bcc_ta_entry("bcc-Ta-3.40", 3.40, -15.9);
      }
      {
        std::ofstream os("config");
        os << "DBFILE db.tadah\nATOM Ta\nWATOM 1\n" << tc.config
           << "NUMERIC " << numeric << "\n"
           << "FORCE false\nSTRESS false\nBIAS false\nNORM false\n"
              "VERBOSE 0\nMODEL M_BLR BF_Linear\nLAMBDA 1e-3\nOALGO 1\n";
      }
      const CliRun t = run_tadah({"tadah", "train", "-c", "config", "-o",
                                  "pot.tadah"});
      INFO("train status " << t.status << "\n" << reported(t));
      REQUIRE(t.status == 0);
      const std::string pot = slurp("pot.tadah");
      INFO("pot.tadah:\n" << pot);
      for (const std::vector<std::string>& line : tc.exact_lines)
        CHECK(line_tokens(pot, line.front()) == line);

      const CliRun p = run_tadah({"tadah", "predict", "-p", "pot.tadah",
                                  "-d", "db.tadah"});
      INFO("predict status " << p.status << "\n" << reported(p));
      CHECK_FALSE(contains(reported(p), "has support reaching"));
      CHECK(p.status == 0);
      CHECK(std::filesystem::exists("energy.pred"));

#ifdef TADAH_ENABLE_NFP
      { std::ofstream os("seed.tadah"); os << pot << "DBFILE db.tadah\n"; }
      { std::ofstream os("config.val"); os << "DBFILE db.tadah\n"; }
      {
        std::ofstream os("config.hpo");
        os << "OPTIMIZER\nLIB NLOPT\nALGO LN_SBPLX\nMAXEVAL 2\n"
              "ENDOPTIMIZER\nLOSS L2\nPC_ERMSE 0 1\n"
              "OPTIM LAMBDA (1) 1e-4 1e-2\n";
      }
      const CliRun h = run_tadah({"tadah", "hpo", "-c", "seed.tadah",
                                  "--hpotarget", "config.hpo",
                                  "--validation", "config.val"});
      INFO("hpo status " << h.status << "\n" << reported(h));
      CHECK_FALSE(contains(reported(h), "bump-support gate"));
      CHECK_FALSE(contains(reported(h), "has support reaching"));
      CHECK(h.status == 0);
#endif  // TADAH_ENABLE_NFP
    }
  }
}

#ifdef TADAH_ENABLE_NFP
/* Pins: `tadah hpo` with NUMERIC 6 in its -c configuration (a D2_mJoin of
 * D2_BP, OPTIM CGRID2B (1), and D2_EAM of the setfl cutoff 4.51234567891;
 * DUMP 1 and BDUMP 1) writes best_pot.tadah and the archives
 * dumps/pot_<N>.tadah and bdumps/best_pot_<N>.tadah with 'RCUT2B 4.123456789
 * 4.51234567891', and `tadah predict` loads best_pot.tadah and the first
 * archive of each directory (exit 0).
 * Before the fix: they recorded 'RCUT2B 4.12346 4.51235', and predict
 * exited 1 on each: 'D2_EAM: RCUT2B 4.51235 differs from the cutoff
 * 4.51234567891 of the setfl file ...'. */
TEST_CASE("Subcommand: hpo with NUMERIC below 17 writes potentials whose "
          "EAM cutoff predict accepts",
          "[TadahCLI][hpo][eam][numeric]") {
  ScratchCwd cwd("hpo_eam_numeric");
  { std::ofstream os("eam_rc.alloy"); os << small_ta_setfl("4.51234567891"); }
  {
    std::ofstream os("db.tadah");
    os << bcc_ta_entry("bcc-Ta-3.25", 3.25, -16.2)
       << bcc_ta_entry("bcc-Ta-3.30", 3.30, -16.3)
       << bcc_ta_entry("bcc-Ta-3.35", 3.35, -16.1)
       << bcc_ta_entry("bcc-Ta-3.40", 3.40, -15.9);
  }
  { std::ofstream os("config.val"); os << "DBFILE db.tadah\n"; }
  {
    std::ofstream os("config.tadah");
    os << "DBFILE db.tadah\nATOM Ta\nWATOM 1\nINIT2B true\n"
          "TYPE2B D2_mJoin\nTYPE2B D2_BP 2 2 Ta Ta\nTYPE2B D2_EAM 0 Ta Ta\n"
          "RCTYPE2B Cut_Cos Cut_Dummy\nRCUT2B 4.123456789 4.51234567891\n"
          "CGRID2B 2.6 3.3\nSGRID2B 1.0 1.0\nINITMB false\n"
          "SETFL eam_rc.alloy\nNUMERIC 6\n"
          "FORCE false\nSTRESS false\nBIAS false\nNORM false\nVERBOSE 0\n"
          "MODEL M_BLR BF_Linear\nLAMBDA 1e-3\nOALGO 1\n";
  }
  {
    std::ofstream os("config.hpo");
    os << "OPTIMIZER\nLIB NLOPT\nALGO LN_SBPLX\nMAXEVAL 3\nENDOPTIMIZER\n"
          "LOSS L2\nPC_ERMSE 0 1\nOPTIM CGRID2B (1) 2.2 3.0\n"
          "DUMP 1 dumps\nBDUMP 1 bdumps\n";
  }
  const CliRun r = run_tadah({"tadah", "hpo", "-c", "config.tadah",
                              "--hpotarget", "config.hpo",
                              "--validation", "config.val"});
  INFO("hpo status " << r.status << "\n" << reported(r));
  REQUIRE(r.status == 0);
  std::vector<std::string> pots = {"best_pot.tadah"};
  for (const char* dir : {"dumps", "bdumps"}) {
    std::vector<std::string> files;
    for (const auto& e : std::filesystem::directory_iterator(dir))
      files.push_back(e.path().string());
    std::sort(files.begin(), files.end());
    INFO(dir);
    REQUIRE_FALSE(files.empty());
    pots.push_back(files.front());
  }
  for (const std::string& pot_file : pots) {
    INFO(pot_file);
    const std::string pot = slurp(pot_file);
    INFO(pot);
    CHECK(line_tokens(pot, "RCUT2B") ==
          std::vector<std::string>{"RCUT2B", "4.123456789", "4.51234567891"});
    const CliRun p = run_tadah({"tadah", "predict", "-p", pot_file, "-d",
                                "db.tadah"});
    INFO("predict status " << p.status << "\n" << reported(p));
    CHECK_FALSE(contains(reported(p), "differs from the cutoff"));
    CHECK(p.status == 0);
  }
}
#endif  // TADAH_ENABLE_NFP

/* ---- [refit]: the configuration numbers are read strictly ------------ */

/* Pins: `tadah refit -c` with a configuration number out of its range
 * (REFIT_TOL_AMBIENT 0, OUTPUT_GRID_SCALE 0, OPTIM_PCT -5) exits 1 with a
 * message naming the key and the token, and writes no potential; with the
 * same configuration and REFIT_TOL_AMBIENT 0.5 it exits 0 and writes it.
 * A dense Blip refit of the small Ta setfl, coarse, without escalation.
 * Before the fix: the numbers were read unchecked, every run exited 0 and
 * wrote pot.tadah (REFIT_TOL_AMBIENT 0: a self-test that could never pass;
 * OUTPUT_GRID_SCALE 0: a 2-point refit.eam; OPTIM_PCT -5: bounds at
 * -5 %). */
TEST_CASE("Subcommand: refit: a configuration number out of its range is "
          "refused with status 1",
          "[TadahCLI][exit-status][refit]") {
  ScratchCwd cwd("refit_numbers");
  { std::ofstream os("eam_small.alloy"); os << small_ta_setfl(); }
  const auto config = [](const std::string& extra) {
    std::ofstream("config.refit")
        << "EAM eam_small.alloy\nRHOFORM SIGNED\nREFIT_MODE dense\n"
           "REFIT_BASIS Blip\nREFIT_LEVEL coarse\nREFIT_MAX_ESCALATIONS 0\n"
        << extra << "\n";
  };
  for (const auto& [line, message] :
       std::vector<std::pair<std::string, std::string>>{
           {"REFIT_TOL_AMBIENT 0",
            "tadah refit: REFIT_TOL_AMBIENT must be a positive number, got "
            "'0'"},
           {"OUTPUT_GRID_SCALE 0",
            "tadah refit: OUTPUT_GRID_SCALE must be a positive number, got "
            "'0'"},
           {"OPTIM_PCT -5",
            "tadah refit: OPTIM_PCT must be a positive number, got '-5'"}}) {
    config(line);
    const CliRun r = run_tadah({"tadah", "refit", "-c", "config.refit"});
    const std::string rep = reported(r);
    INFO(line << ": status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    CHECK(contains(rep, message));
    CHECK_FALSE(std::filesystem::exists("pot.tadah"));
    std::filesystem::remove("pot.tadah");
  }
  config("REFIT_TOL_AMBIENT 0.5");
  const CliRun ok = run_tadah({"tadah", "refit", "-c", "config.refit"});
  INFO("REFIT_TOL_AMBIENT 0.5: status " << ok.status << "\n" << reported(ok));
  CHECK(ok.status == 0);
  CHECK(std::filesystem::exists("pot.tadah"));
}

/* ---- [refit]: --rcut2b and --rcutmb are options of --retarget ---------- */

/* Pins: `tadah refit -c` with --rcut2b or --rcutmb (options of --retarget)
 * is refused before any task runs, naming --retarget, as --pct, --set and
 * --rhoform without --retarget are, and writes no potential. Before the
 * fix the two options had no cli_needs: the refit exited 0, RCUT2B stayed
 * as fitted, and nothing said the option had no effect. */
TEST_CASE("Subcommand: refit: --rcut2b and --rcutmb require --retarget",
          "[TadahCLI][exit-status][refit]") {
  ScratchCwd cwd("refit_rcut_flags");
  { std::ofstream os("eam_small.alloy"); os << small_ta_setfl(); }
  std::ofstream("config.refit")
      << "EAM eam_small.alloy\nRHOFORM SIGNED\nREFIT_MODE dense\n"
         "REFIT_BASIS Blip\nREFIT_LEVEL coarse\nREFIT_MAX_ESCALATIONS 0\n"
         "REFIT_TOL_AMBIENT 0.5\n";
  for (const char* flag : {"--rcut2b", "--rcutmb"}) {
    const CliRun r =
        run_tadah({"tadah", "refit", "-c", "config.refit", flag, "4.0"});
    const std::string rep = reported(r);
    INFO(flag << ": status " << r.status << "\n" << rep);
    CHECK(r.status != 0);
    CHECK(contains(rep, std::string(flag) + " requires --retarget"));
    CHECK_FALSE(std::filesystem::exists("pot.tadah"));
    std::filesystem::remove("pot.tadah");
  }
}

/* ---- [refit]: the table and the crystal of its header ----------------- */

/* Pins: `tadah refit -c` of the small Ta setfl (dense, coarse) exits 1 and
 * writes no potential when
 *  - its element line gives a0 = 330.4 (tantalum's lattice constant in
 *    picometres), whose bcc crystal has its nearest neighbours 286 A
 *    apart, beyond the table's cutoff, 4.5 A: the refusal names a0 = 330.4
 *    A, "at or beyond the table's cutoff, rcut = 4.5 A", "LAMMPS
 *    (pair_style eam/alloy) counts a pair only below rcut: the crystal has
 *    no interaction in LAMMPS" and that "a value in picometres is 100
 *    times the value in Angstrom";
 *  - its grid line gives dr = 1e-300, so that its r tables of 501 points
 *    end at 5e-298, short of rcut 4.5: the refusal names the file, line 5,
 *    "the r tables do not reach the cutoff", "nr = 501 points dr = 1e-300
 *    apart", "rcut = 4.5" and the grid line to correct.
 * `tadah train` with that table as the SETFL of a DM_EAM model is refused
 * the same way (the reader is the one every SETFL user reads with). The
 * table as written fits (exit 0, the [refit] case above).
 * Before the fix: the first refusal said "the crystal has no pair within
 * the cutoff" (tadah itself counts a pair at exactly rcut), so its third
 * needle fails. Before the gates, both refits exited 0 and wrote pot.tadah
 * (the first with a self-test of crystals without a neighbour, 0.000
 * meV/atom; the second evaluating every distance at a spline index past
 * int), and the train crashed the test process (segmentation fault:
 * DM_EAM read its table at that index). */
TEST_CASE("Subcommand: refit refuses a header lattice constant beyond the "
          "cutoff and a table whose r grid does not reach it",
          "[TadahCLI][exit-status][refit]") {
  ScratchCwd cwd("refit_gates");
  const std::string good = small_ta_setfl();
  const auto replaced = [&](const std::string& from, const std::string& to) {
    std::string t = good;
    const auto at = t.find(from);
    REQUIRE(at != std::string::npos);
    t.replace(at, from.size(), to);
    return t;
  };
  { std::ofstream os("eam_pm.alloy");
    os << replaced("\n73 180.95 3.304 bcc\n", "\n73 180.95 330.4 bcc\n"); }
  // the grid line is "501 <drho> 501 0.01 4.5"
  { std::ofstream os("eam_dr.alloy");
    os << replaced(" 501 0.01 4.5\n", " 501 1e-300 4.5\n"); }
  for (const auto& [eam, needles] :
       std::vector<std::pair<std::string, std::vector<std::string>>>{
           {"eam_pm.alloy",
            {"a0 = 330.4 A", "at or beyond the table's cutoff, rcut = 4.5 A",
             "and LAMMPS (pair_style eam/alloy) counts a pair only below "
             "rcut: the crystal has no interaction in LAMMPS",
             "a value in picometres is 100 times the value in Angstrom"}},
           {"eam_dr.alloy",
            {"EAMSetflIO: setfl file 'eam_dr.alloy', line 5: the r tables do "
             "not reach the cutoff: nr = 501 points dr = 1e-300 apart",
             "rcut = 4.5", "the grid line ('nrho drho nr dr rcut')"}}}) {
    std::ofstream("config.refit")
        << "EAM " << eam << "\nRHOFORM SIGNED\nREFIT_MODE dense\n"
           "REFIT_BASIS Blip\nREFIT_LEVEL coarse\nREFIT_MAX_ESCALATIONS 0\n";
    const CliRun r = run_tadah({"tadah", "refit", "-c", "config.refit"});
    const std::string rep = reported(r);
    INFO(eam << ": status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    for (const std::string& n : needles) {
      INFO("needle: " << n);
      CHECK(contains(rep, n));
    }
    CHECK_FALSE(std::filesystem::exists("pot.tadah"));
    std::filesystem::remove("pot.tadah");
  }
  {
    std::ofstream("config.train")
        << "DBFILE db.tadah\nATOM Ta\nWATOM 1\nINIT2B false\n"
           "INITMB true\nTYPEMB DM_EAM 0 Ta Ta\n"
           "RCTYPEMB Cut_Dummy\nRCUTMB 4.5\nSETFL eam_dr.alloy\n"
           "FORCE false\nSTRESS false\nBIAS false\nNORM false\nVERBOSE 0\n"
           "MODEL M_BLR BF_Linear\nLAMBDA 1e-3\nOALGO 1\n";
    std::ofstream("db.tadah") << bcc_ta_entry("bcc-Ta-3.25", 3.25, -16.2)
                              << bcc_ta_entry("bcc-Ta-3.30", 3.30, -16.3)
                              << bcc_ta_entry("bcc-Ta-3.35", 3.35, -16.1);
    const CliRun r = run_tadah({"tadah", "train", "-c", "config.train",
                                "-o", "pot.tadah"});
    const std::string rep = reported(r);
    INFO("train: status " << r.status << "\n" << rep);
    CHECK(r.status != 0);
    CHECK(contains(rep, "EAMSetflIO: setfl file 'eam_dr.alloy', line 5: the "
                        "r tables do not reach the cutoff"));
    CHECK_FALSE(std::filesystem::exists("pot.tadah"));
  }
}

/* ---- [refit]: a key of the other REFIT_MODE is refused ---------------- */

// A top-level OPTIMIZER block of the grammar before the stage blocks, and
// the migration text that refuses it in either REFIT_MODE.
const std::string kTopLevelOptimizerBlock =
    "OPTIMIZER\n  GLOBAL ALGO MLSL ENDGLOBAL\nENDOPTIMIZER";
const std::string kTopLevelOptimizerText =
    "tadah refit: a top-level OPTIMIZER block is no longer supported. The "
    "stage pipeline is now INIT -> EXPLORE -> REFINE -> FUSE, each its own "
    "block, with the OPTIMIZER block NESTED inside EXPLORE/REFINE/FUSE. "
    "Migration: GLOBAL folded into EXPLORE rung 0 (set its global ALGO in "
    "EXPLORE's OPTIMIZER, with an INNER local); POLISH -> REFINE; JOINT -> "
    "FUSE. The Refitting page of the documentation has examples (Stage "
    "blocks).";

/* Pins: `tadah refit -c` refuses a key that the configuration's REFIT_MODE
 * does not read, with status 1, a message naming the key and the mode that
 * reads it, and no potential written: under REFIT_MODE dense (the small Ta
 * setfl, a coarse Blip fit) REFIT_COMPACT_NMAX 3, a REFINE block (with keys
 * the compact mode would refuse, THREADS 4 and GTOL -1: the block is
 * refused as a whole) and REFIT_OBJECTIVE curve; the same configuration
 * without them exits 0 and writes pot.tadah, so the blueprint defaults
 * that the task factory stores for every refit (REFIT_COMPACT_MERGE auto,
 * REFIT_CURVE_* and REFIT_OBJECTIVE energy) are not taken for the
 * configuration's keys. In a build with the compact fitter (NFP and Ceres)
 * a compact configuration (the default mode; Blip5, small budgets) with
 * N2B 50, or with REFIT_CURVE_WEIGHTING false or REFIT_CURVE_RELAX 0.1
 * under the default REFIT_OBJECTIVE energy, exits 1 naming the key, and
 * without them exits
 * 0 and writes pot.tadah (REFIT_CONSTRAIN_F0's stored default is not
 * refused); in a build without it the compact configuration exits 1:
 * "tadah refit: REFIT_MODE compact (the default) needs the compact fitter,
 * ...". A top-level OPTIMIZER block (the grammar before the stage blocks),
 * which neither mode reads, exits 1 with the migration text ("tadah refit:
 * a top-level OPTIMIZER block is no longer supported. ... (Stage
 * blocks)."), which under REFIT_MODE dense ends " Under REFIT_MODE dense,
 * which reads no stage block, remove the OPTIMIZER block."; under compact
 * mode, in every build, before the EAM file is read (absent here).
 * Before the fix: every configuration exited 0 and wrote pot.tadah (dense
 * mode never read the stage blocks or the compact keys, compact mode never
 * read N2B, REFIT_CURVE_WEIGHTING was ignored under energy, and a build
 * without the compact fitter fitted the compact configuration in dense
 * mode after a WARNING), the top-level OPTIMIZER block under dense
 * included; under compact the block exited 1 reading the absent EAM file
 * (with NFP and Ceres: it was refused after the table was read). The mode
 * rule's first form refused the block under dense as a key of compact
 * mode, "tadah refit: the OPTIMIZER block has no effect under REFIT_MODE
 * dense: only REFIT_MODE compact reads it; remove the OPTIMIZER block, or
 * set REFIT_MODE compact", whose advice leads to the migration text, and
 * under compact still read the absent EAM file first (or, without NFP and
 * Ceres, refused compact mode). REFIT_CURVE_RELAX under energy was
 * accepted with no effect until the curve knobs were refused there (the
 * run exited 0 and wrote pot.tadah). */
TEST_CASE("Subcommand: refit: a key of the other REFIT_MODE is refused "
          "with status 1",
          "[TadahCLI][exit-status][refit]") {
  ScratchCwd cwd("refit_mode_keys");
  { std::ofstream os("eam_small.alloy"); os << small_ta_setfl(); }
  const auto run = [](const std::string& config, const std::string& extra) {
    std::filesystem::remove("pot.tadah");
    std::ofstream("config.refit") << config << extra << "\n";
    return run_tadah({"tadah", "refit", "-c", "config.refit"});
  };
  const std::string dense =
      "EAM eam_small.alloy\nRHOFORM SIGNED\nREFIT_MODE dense\n"
      "REFIT_BASIS Blip\nREFIT_LEVEL coarse\nREFIT_MAX_ESCALATIONS 0\n";
  const auto in_dense = [](const std::string& what) {
    return "tadah refit: " + what + " has no effect under REFIT_MODE dense: "
           "only REFIT_MODE compact reads it; remove " + what +
           ", or set REFIT_MODE compact";
  };
  for (const auto& [extra, message] :
       std::vector<std::pair<std::string, std::string>>{
           {"REFIT_COMPACT_NMAX 3", in_dense("REFIT_COMPACT_NMAX")},
           {"REFINE\n  OPTIMIZER\n    LIB CERES ALGO LM MAXEVAL 5 THREADS 4 "
            "GTOL -1\n  ENDOPTIMIZER\nENDREFINE",
            in_dense("the REFINE block")},
           {"REFIT_OBJECTIVE curve", in_dense("REFIT_OBJECTIVE curve")},
           {kTopLevelOptimizerBlock,
            kTopLevelOptimizerText + " Under REFIT_MODE dense, which reads "
                                     "no stage block, remove the OPTIMIZER "
                                     "block."}}) {
    const CliRun r = run(dense, extra);
    const std::string rep = reported(r);
    INFO(extra << ": status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    CHECK(contains(rep, message));
    CHECK_FALSE(std::filesystem::exists("pot.tadah"));
  }
  {
    const CliRun ok = run(dense, "");
    INFO("dense: status " << ok.status << "\n" << reported(ok));
    CHECK(ok.status == 0);
    CHECK(std::filesystem::exists("pot.tadah"));
  }
  const std::string compact =
      "EAM eam_small.alloy\nRHOFORM SIGNED\nREFIT_BASIS Blip5\n"
      "REFIT_COMPACT_NMAX 8\nREFIT_COMPACT_JITTERS 0\n"
      "REFIT_COMPACT_STALL 3\nREFIT_COMPACT_SAMPLES 200\n"
      "REFIT_TOL_AMBIENT 5\nREFIT_TOL_COMPRESSED 20\n"
      "EXPLORE\n  OPTIMIZER\n    LIB NLOPT ALGO GN_MLSL_LDS MAXEVAL 4000\n"
      "    INNER\n      LIB NLOPT ALGO LN_SBPLX MAXEVAL 1000\n    ENDINNER\n"
      "  ENDOPTIMIZER\nENDEXPLORE\n";
  {
    // compact mode, the EAM file absent: refused before it is read
    std::string absent = compact;
    absent.replace(0, absent.find('\n'), "EAM absent.alloy");
    const CliRun r = run(absent, kTopLevelOptimizerBlock);
    const std::string rep = reported(r);
    INFO("compact, top-level OPTIMIZER: status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    CHECK(contains(rep, kTopLevelOptimizerText));
    CHECK_FALSE(contains(rep, "Under REFIT_MODE dense"));
    CHECK_FALSE(std::filesystem::exists("pot.tadah"));
  }
#if defined(TADAH_ENABLE_NFP) && defined(TADAH_USE_CERES)
  for (const auto& [extra, message] :
       std::vector<std::pair<std::string, std::string>>{
           {"N2B 50",
            "tadah refit: N2B has no effect under REFIT_MODE compact (the "
            "default): only REFIT_MODE dense reads it; remove N2B, or set "
            "REFIT_MODE dense"},
           {"REFIT_CURVE_WEIGHTING false",
            "tadah refit: REFIT_CURVE_WEIGHTING has no effect under "
            "REFIT_OBJECTIVE energy (the default): only REFIT_OBJECTIVE curve "
            "reads it; remove REFIT_CURVE_WEIGHTING, or set REFIT_OBJECTIVE "
            "curve"},
           {"REFIT_CURVE_RELAX 0.1",
            "tadah refit: REFIT_CURVE_RELAX has no effect under "
            "REFIT_OBJECTIVE energy (the default): only REFIT_OBJECTIVE curve "
            "reads it; remove REFIT_CURVE_RELAX, or set REFIT_OBJECTIVE "
            "curve"}}) {
    const CliRun r = run(compact, extra);
    const std::string rep = reported(r);
    INFO(extra << ": status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    CHECK(contains(rep, message));
    CHECK_FALSE(std::filesystem::exists("pot.tadah"));
  }
  const CliRun ok = run(compact, "");
  INFO("compact: status " << ok.status << "\n" << reported(ok));
  CHECK(ok.status == 0);
  CHECK(std::filesystem::exists("pot.tadah"));
#else
  const CliRun r = run(compact, "");
  const std::string rep = reported(r);
  INFO("compact: status " << r.status << "\n" << rep);
  CHECK(r.status == 1);
  CHECK(contains(rep, "tadah refit: REFIT_MODE compact (the default) needs "
                      "the compact fitter, which this tadah was built "
                      "without (it needs TADAH_ENABLE_NFP and "
                      "TADAH_ENABLE_CERES); REFIT_MODE dense fits without "
                      "it"));
  CHECK_FALSE(std::filesystem::exists("pot.tadah"));
#endif
}

/* ---- [refit]: --edit and --retarget refuse the keys of a refit ------- */

/* Pins: on the potential and hpotarget of a dense refit of the small Ta
 * setfl, `tadah refit --retarget refit.hpotarget --hpofileout
 * new.hpotarget` exits 1 with "tadah refit --retarget: HPOFILEOUT has no
 * effect under --retarget: only a refit of an EAM file and --edit read it;
 * remove HPOFILEOUT (the file --retarget writes is -o, OUTFILE)" and writes
 * neither new.hpotarget nor refit_retargeted.hpotarget (its default name;
 * refit_edited.hpotarget until the --retarget default was
 * <input>_retargeted), and `tadah refit -c
 * edit.refit` with EDIT, FUNC, ADD and N2B 50 exits 1 with "tadah refit
 * --edit: N2B has no effect under --edit: only a refit of an EAM file in
 * REFIT_MODE dense reads it; remove N2B" and writes no pot_edited.tadah;
 * without HPOFILEOUT and N2B both exit 0 and write their file, so the
 * defaults the task factory stores for every refit (HPOFILEOUT
 * refit.hpotarget, REFIT_OBJECTIVE energy, REFIT_CURVE_*, ...) are not
 * refused.
 * Before the fix: both exited 0, --retarget writing refit_edited.hpotarget
 * (HPOFILEOUT is not read by --retarget) and --edit pot_edited.tadah as
 * without N2B. */
TEST_CASE("Subcommand: refit --edit and --retarget refuse a key of a refit "
          "of an EAM file with status 1",
          "[TadahCLI][exit-status][refit]") {
  ScratchCwd cwd("refit_output_mode_keys");
  { std::ofstream os("eam_small.alloy"); os << small_ta_setfl(); }
  std::ofstream("config.refit")
      << "EAM eam_small.alloy\nRHOFORM SIGNED\nREFIT_MODE dense\n"
         "REFIT_BASIS Blip\nREFIT_LEVEL coarse\nREFIT_MAX_ESCALATIONS 0\n";
  {
    const CliRun fit = run_tadah({"tadah", "refit", "-c", "config.refit"});
    INFO("refit: status " << fit.status << "\n" << reported(fit));
    REQUIRE(fit.status == 0);
    REQUIRE(std::filesystem::exists("pot.tadah"));
    REQUIRE(std::filesystem::exists("refit.hpotarget"));
  }
  {
    const CliRun r = run_tadah({"tadah", "refit", "--retarget",
                                "refit.hpotarget", "--hpofileout",
                                "new.hpotarget"});
    const std::string rep = reported(r);
    INFO("--retarget --hpofileout: status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    CHECK(contains(rep, "tadah refit --retarget: HPOFILEOUT has no effect "
                        "under --retarget: only a refit of an EAM file and "
                        "--edit read it; remove HPOFILEOUT (the file "
                        "--retarget writes is -o, OUTFILE)"));
    CHECK_FALSE(std::filesystem::exists("new.hpotarget"));
    CHECK_FALSE(std::filesystem::exists("refit_retargeted.hpotarget"));
  }
  {
    const CliRun r =
        run_tadah({"tadah", "refit", "--retarget", "refit.hpotarget"});
    INFO("--retarget: status " << r.status << "\n" << reported(r));
    CHECK(r.status == 0);
    CHECK(std::filesystem::exists("refit_retargeted.hpotarget"));
  }
  const std::string edit = "EDIT pot.tadah\nFUNC phi\nADD 2.5\n";
  {
    std::ofstream("edit.refit") << edit << "N2B 50\n";
    const CliRun r = run_tadah({"tadah", "refit", "-c", "edit.refit"});
    const std::string rep = reported(r);
    INFO("--edit with N2B: status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    CHECK(contains(rep, "tadah refit --edit: N2B has no effect under --edit: "
                        "only a refit of an EAM file in REFIT_MODE dense "
                        "reads it; remove N2B"));
    CHECK_FALSE(std::filesystem::exists("pot_edited.tadah"));
  }
  std::ofstream("edit.refit") << edit;
  const CliRun r = run_tadah({"tadah", "refit", "-c", "edit.refit"});
  INFO("--edit: status " << r.status << "\n" << reported(r));
  CHECK(r.status == 0);
  CHECK(std::filesystem::exists("pot_edited.tadah"));
}

/* Pins: refit --edit and --retarget write different default hpotargets,
 * so neither overwrites the other's. On the outputs of a dense refit of the
 * small Ta setfl (pot.tadah, refit.hpotarget), `tadah refit -c edit.cfg`
 * (EDIT pot.tadah, FUNC phi, ADD 2.5) and `tadah refit -c retarget.cfg`
 * (RETARGET refit.hpotarget, PCT 10), run in either order, exit 0 and
 * leave refit_edited.hpotarget byte-identical to the file `tadah refit
 * --edit pot.tadah --func phi --add 2.5` writes alone, and
 * refit_retargeted.hpotarget byte-identical to the file `tadah refit
 * --retarget refit.hpotarget --pct 10 -o alone.hpotarget` writes.
 * Before the fix: --retarget's default was <input>_edited<ext>, the --edit
 * default refit_edited.hpotarget for this input, so the second run
 * overwrote the first run's file (exit 0): refit_edited.hpotarget was the
 * --retarget file with the --edit run first and the --edit file with it
 * second, and refit_retargeted.hpotarget was never written. */
TEST_CASE("Subcommand: refit --edit and --retarget write different default "
          "hpotargets",
          "[TadahCLI][exit-status][refit]") {
  ScratchCwd cwd("refit_edit_retarget_outputs");
  { std::ofstream os("eam_small.alloy"); os << small_ta_setfl(); }
  std::ofstream("config.refit")
      << "EAM eam_small.alloy\nRHOFORM SIGNED\nREFIT_MODE dense\n"
         "REFIT_BASIS Blip\nREFIT_LEVEL coarse\nREFIT_MAX_ESCALATIONS 0\n";
  {
    const CliRun fit = run_tadah({"tadah", "refit", "-c", "config.refit"});
    INFO("refit: status " << fit.status << "\n" << reported(fit));
    REQUIRE(fit.status == 0);
    REQUIRE(std::filesystem::exists("pot.tadah"));
    REQUIRE(std::filesystem::exists("refit.hpotarget"));
  }
  std::string edit_alone, retarget_alone;
  {
    const CliRun r = run_tadah({"tadah", "refit", "--edit", "pot.tadah",
                                "--func", "phi", "--add", "2.5"});
    INFO("--edit: status " << r.status << "\n" << reported(r));
    REQUIRE(r.status == 0);
    edit_alone = slurp("refit_edited.hpotarget");
    REQUIRE_FALSE(edit_alone.empty());
  }
  {
    const CliRun r = run_tadah({"tadah", "refit", "--retarget",
                                "refit.hpotarget", "--pct", "10", "-o",
                                "alone.hpotarget"});
    INFO("--retarget: status " << r.status << "\n" << reported(r));
    REQUIRE(r.status == 0);
    retarget_alone = slurp("alone.hpotarget");
    REQUIRE_FALSE(retarget_alone.empty());
  }
  REQUIRE(edit_alone != retarget_alone);
  std::ofstream("edit.cfg") << "EDIT pot.tadah\nFUNC phi\nADD 2.5\n";
  std::ofstream("retarget.cfg") << "RETARGET refit.hpotarget\nPCT 10\n";
  for (const bool edit_first : {true, false}) {
    for (const char* f : {"pot_edited.tadah", "refit_edited.hpotarget",
                          "refit_retargeted.hpotarget"})
      std::filesystem::remove(f);
    for (const char* cfg : edit_first
                               ? std::vector<const char*>{"edit.cfg",
                                                          "retarget.cfg"}
                               : std::vector<const char*>{"retarget.cfg",
                                                          "edit.cfg"}) {
      const CliRun r = run_tadah({"tadah", "refit", "-c", cfg});
      INFO(cfg << (edit_first ? " (edit first)" : " (retarget first)")
               << ": status " << r.status << "\n" << reported(r));
      CHECK(r.status == 0);
    }
    CHECK(std::filesystem::exists("pot_edited.tadah"));
    CHECK(slurp("refit_edited.hpotarget") == edit_alone);
    CHECK(slurp("refit_retargeted.hpotarget") == retarget_alone);
  }
}

/* Pins: `tadah refit -c` with REFIT_BASIS Blip in compact mode (the
 * default) and REFIT_COMPACT_MERGE true exits 1 with "tadah refit:
 * REFIT_COMPACT_MERGE has no effect under REFIT_BASIS Blip: only the knot
 * families (REFIT_BASIS Knot5, the compact default, Knot3 or KnotMix) read
 * it; remove REFIT_COMPACT_MERGE, or set REFIT_BASIS Knot5", before the
 * EAM file is read (absent here), in every build.
 * Before the fix: the key passed the mode rule, so the run went on and
 * exited 1 for another reason: reading the absent EAM file in a build
 * with the compact fitter, refusing compact mode in one without it. */
TEST_CASE("Subcommand: refit: REFIT_COMPACT_MERGE under REFIT_BASIS Blip is "
          "refused with status 1",
          "[TadahCLI][exit-status][refit]") {
  ScratchCwd cwd("refit_merge_blip");
  std::ofstream("config.refit")
      << "EAM absent.alloy\nRHOFORM SIGNED\nREFIT_BASIS Blip\n"
         "REFIT_COMPACT_MERGE true\n";
  const CliRun r = run_tadah({"tadah", "refit", "-c", "config.refit"});
  const std::string rep = reported(r);
  INFO("status " << r.status << "\n" << rep);
  CHECK(r.status == 1);
  CHECK(contains(rep, "tadah refit: REFIT_COMPACT_MERGE has no effect under "
                      "REFIT_BASIS Blip: only the knot families (REFIT_BASIS "
                      "Knot5, the compact default, Knot3 or KnotMix) read "
                      "it; remove REFIT_COMPACT_MERGE, or set REFIT_BASIS "
                      "Knot5"));
  CHECK_FALSE(std::filesystem::exists("pot.tadah"));
}

/* Pins, through tadah, on the outputs of a dense refit of the small Ta setfl
 * copied to in.tadah and in.hpotarget: `tadah refit --edit in.tadah --func
 * phi --add 2.5 -o pot.tadah --hpofileout refit.hpotarget` writes
 * pot.tadah and refit.hpotarget (no pot_edited.tadah or
 * refit_edited.hpotarget), so does `tadah refit -c edit.cfg` with those keys
 * in edit.cfg, and `tadah refit --retarget in.hpotarget -o pot.tadah`
 * writes pot.tadah (no in_retargeted.hpotarget).
 * Before the fix: an -o / --hpofileout equal to the default name counted as
 * unset, and all three wrote the edited or retargeted default names, without
 * a word. */
TEST_CASE("Subcommand: refit --edit and --retarget write an explicit output "
          "name that is the default one",
          "[TadahCLI][exit-status][refit]") {
  ScratchCwd cwd("refit_explicit_default_names");
  { std::ofstream os("eam_small.alloy"); os << small_ta_setfl(); }
  std::ofstream("config.refit")
      << "EAM eam_small.alloy\nRHOFORM SIGNED\nREFIT_MODE dense\n"
         "REFIT_BASIS Blip\nREFIT_LEVEL coarse\nREFIT_MAX_ESCALATIONS 0\n";
  {
    const CliRun fit = run_tadah({"tadah", "refit", "-c", "config.refit"});
    INFO("refit: status " << fit.status << "\n" << reported(fit));
    REQUIRE(fit.status == 0);
  }
  std::filesystem::rename("pot.tadah", "in.tadah");
  std::filesystem::rename("refit.hpotarget", "in.hpotarget");
  const auto clean = [] {
    for (const char* f : {"pot.tadah", "refit.hpotarget", "pot_edited.tadah",
                          "refit_edited.hpotarget", "in_retargeted.hpotarget"})
      std::filesystem::remove(f);
  };
  {
    const CliRun r = run_tadah({"tadah", "refit", "--edit", "in.tadah",
                                "--func", "phi", "--add", "2.5", "-o",
                                "pot.tadah", "--hpofileout",
                                "refit.hpotarget"});
    INFO("--edit: status " << r.status << "\n" << reported(r));
    CHECK(r.status == 0);
    CHECK(std::filesystem::exists("pot.tadah"));
    CHECK(std::filesystem::exists("refit.hpotarget"));
    CHECK_FALSE(std::filesystem::exists("pot_edited.tadah"));
    CHECK_FALSE(std::filesystem::exists("refit_edited.hpotarget"));
  }
  clean();
  {
    std::ofstream("edit.cfg")
        << "EDIT in.tadah\nFUNC phi\nADD 2.5\n"
           "OUTFILE pot.tadah\nHPOFILEOUT refit.hpotarget\n";
    const CliRun r = run_tadah({"tadah", "refit", "-c", "edit.cfg"});
    INFO("configuration file: status " << r.status << "\n" << reported(r));
    CHECK(r.status == 0);
    CHECK(std::filesystem::exists("pot.tadah"));
    CHECK(std::filesystem::exists("refit.hpotarget"));
    CHECK_FALSE(std::filesystem::exists("pot_edited.tadah"));
  }
  clean();
  {
    const CliRun r = run_tadah({"tadah", "refit", "--retarget",
                                "in.hpotarget", "-o", "pot.tadah"});
    INFO("--retarget: status " << r.status << "\n" << reported(r));
    CHECK(r.status == 0);
    CHECK(std::filesystem::exists("pot.tadah"));
    CHECK_FALSE(std::filesystem::exists("in_retargeted.hpotarget"));
  }
}

/* Pins: tadah refit refuses, with status 1 and before the EAM file is read
 * (it does not exist), a REFIT_BASIS that names no basis family (Blipp),
 * a REFIT_LEVEL or REFIT_PLACEMENT that is none of their values (acurate,
 * unifrom), and REFIT_CONSTRAIN_F0 under EMBF0 ZERO (the default), each
 * naming the key and what it takes; and fits REFIT_BASIS BLIP (the Blip
 * family) dense without the experimental-Gaussian WARNING.
 * Before the fix: all four ran (the EAM file read first), fitting Blip for
 * Blipp with the Gaussian WARNING, the balanced preset for acurate, the
 * window placement for unifrom, and REFIT_CONSTRAIN_F0 true without a word;
 * BLIP fitted Blip with the Gaussian WARNING. */
TEST_CASE("Subcommand: refit refuses a word key that is none of its values",
          "[TadahCLI][exit-status][refit]") {
  ScratchCwd cwd("refit_word_keys");
  const std::string head =
      "EAM absent.alloy\nRHOFORM SIGNED\nREFIT_MODE dense\n";
  for (const auto& [lines, message] :
       std::vector<std::pair<std::string, std::string>>{
           {"REFIT_BASIS Blipp\n",
            "tadah refit: REFIT_BASIS 'Blipp' is no basis family of tadah "
            "refit"},
           {"REFIT_BASIS Blip\nREFIT_LEVEL acurate\n",
            "tadah refit: REFIT_LEVEL must be 'coarse', 'balanced' (the "
            "default), 'fine' or 'accurate' (in any letter case), got "
            "'acurate'"},
           {"REFIT_BASIS Blip\nREFIT_PLACEMENT unifrom\n",
            "tadah refit: REFIT_PLACEMENT must be 'window' (the default) or "
            "'uniform' (in any letter case), got 'unifrom'"},
           {"REFIT_BASIS Blip\nREFIT_CONSTRAIN_F0 true\n",
            "tadah refit: REFIT_CONSTRAIN_F0 has no effect under EMBF0 ZERO "
            "(the default): only EMBF0 FREE reads it; remove "
            "REFIT_CONSTRAIN_F0, or set EMBF0 FREE"}}) {
    std::ofstream("config.refit") << head << lines;
    const CliRun r = run_tadah({"tadah", "refit", "-c", "config.refit"});
    const std::string rep = reported(r);
    INFO(lines << "status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    CHECK(contains(rep, message));
    CHECK_FALSE(contains(rep, "absent.alloy"));
  }
  { std::ofstream os("eam_small.alloy"); os << small_ta_setfl(); }
  std::ofstream("config.refit")
      << "EAM eam_small.alloy\nRHOFORM SIGNED\nREFIT_MODE dense\n"
         "REFIT_BASIS BLIP\nREFIT_LEVEL coarse\nREFIT_MAX_ESCALATIONS 0\n";
  const CliRun r = run_tadah({"tadah", "refit", "-c", "config.refit"});
  const std::string rep = reported(r);
  INFO("BLIP: status " << r.status << "\n" << rep);
  CHECK(r.status == 0);
  CHECK_FALSE(contains(rep, "EXPERIMENTAL"));
  CHECK(contains(slurp("pot.tadah"), "D2_Blip"));
}

/* ---- explain: a long description is printed as text ------------------ */

/* Pins: `tadah explain refit.optimizer` exits 0 and prints the key's long
 * description as text: a paragraph's closing '::' as ':' ("Written inside
 * a stage block, for example:"), then a blank line and the literal block
 * with its indentation ("    REFINE", "      OPTIMIZER"); no line ends with
 * "::".
 * Before the fix: the RST was printed as written, "Written inside a stage
 * block, for example::". */
TEST_CASE("Subcommand: explain: a long description is printed as text",
          "[TadahCLI][exit-status][explain]") {
  ScratchCwd cwd("explain_text");
  const CliRun r = run_tadah({"tadah", "explain", "refit.optimizer"});
  INFO("status " << r.status << "\n" << reported(r));
  CHECK(r.status == 0);
  CHECK(contains(r.out, "Written inside a stage block, for example:\n\n"
                        "    REFINE\n      OPTIMIZER\n"));
  CHECK_FALSE(contains(r.out, "::\n"));
}

/* Pins: `tadah explain` exits 1 for an option that names no command, no
 * option of its command and no configuration key, naming what it did not
 * find: "tadah explain: no command 'bogus'" (bogus.key: a dotted option
 * starts with a command), "tadah explain: refit has no option 'nosuchkey',
 * and it is no configuration key" (refit.nosuchkey), "tadah explain: no
 * command or configuration key 'bogus'" (bogus). (Until `tadah explain`
 * read the configuration keys, refit.nosuchkey's message ended at
 * 'nosuchkey' and bogus's was "tadah explain: no command 'bogus'".)
 * Before the fix: bogus.key dereferenced the end of the command map and
 * the process died of SIGSEGV; refit.nosuchkey and bogus exited 0 and
 * printed nothing. */
TEST_CASE("Subcommand: explain: an unknown command or option is refused "
          "with status 1",
          "[TadahCLI][exit-status][explain]") {
  ScratchCwd cwd("explain_unknown");
  for (const auto& [option, message] :
       std::vector<std::pair<std::string, std::string>>{
           {"refit.nosuchkey", "tadah explain: refit has no option "
                               "'nosuchkey', and it is no configuration "
                               "key"},
           {"bogus", "tadah explain: no command or configuration key "
                     "'bogus'"},
           {"bogus.key", "tadah explain: no command 'bogus'"}}) {
    const CliRun r = run_tadah({"tadah", "explain", option});
    INFO(option << ": status " << r.status << "\n" << reported(r));
    CHECK(r.status == 1);
    CHECK(contains(reported(r), message));
  }
}

/* Pins: `tadah explain <KEY>` and `tadah explain <command>.<KEY>` exit 0
 * and print a configuration key that is no option of a command: `tadah
 * explain SETFL` its long description as text ("A configuration with both
 * descriptors and one setfl file:", a blank line, "    INIT2B    true"),
 * `tadah explain TYPE2B` its examples of several lines with each line
 * indented ("EXAMPLES: \n  D2_LJ Kr Kr\n  D2_mJoin\n  D2_MIE 11 6 Ti
 * Ti\n"), `tadah explain refit.rhoform` RHOFORM's text, as `tadah
 * explain RHOFORM` does (`tadah refit --help` names RHOFORM).
 * Before the fix: the three exited 1, "tadah explain: no command 'SETFL'",
 * "... 'TYPE2B'" and "tadah explain: refit has no option 'rhoform'". */
TEST_CASE("Subcommand: explain: a configuration key that is no option of a "
          "command is explained",
          "[TadahCLI][exit-status][explain]") {
  ScratchCwd cwd("explain_config_keys");
  {
    const CliRun r = run_tadah({"tadah", "explain", "SETFL"});
    INFO("SETFL: status " << r.status << "\n" << reported(r));
    CHECK(r.status == 0);
    CHECK(contains(r.out, "A configuration with both descriptors and one "
                          "setfl file:\n\n    INIT2B    true\n"));
  }
  {
    const CliRun r = run_tadah({"tadah", "explain", "TYPE2B"});
    INFO("TYPE2B: status " << r.status << "\n" << reported(r));
    CHECK(r.status == 0);
    CHECK(contains(r.out, "EXAMPLES: \n  D2_LJ Kr Kr\n  D2_mJoin\n"
                          "  D2_MIE 11 6 Ti Ti\n"));
  }
  const CliRun r = run_tadah({"tadah", "explain", "refit.rhoform"});
  INFO("refit.rhoform: status " << r.status << "\n" << reported(r));
  CHECK(r.status == 0);
  const CliRun key = run_tadah({"tadah", "explain", "RHOFORM"});
  INFO("RHOFORM: status " << key.status << "\n" << reported(key));
  CHECK(key.status == 0);
  CHECK_FALSE(r.out.empty());
  CHECK(r.out == key.out);
}

/* ---- explain: the optimisers of tadah hpo ---------------------------- */

/* Pins: in a build with HPO (TADAH_ENABLE_NFP), `tadah explain ALGO` (in
 * any letter case, and LIB) exits 0 and prints the table of the optimisers
 * a `tadah hpo` target's LIB and ALGO can name ("LIB NLOPT", its LD_LBFGS
 * row, the refused NLOPT:LN_NEWUOA and CERES:LEVENBERG_MARQUARDT), and `tadah
 * explain <LIB>:<ALGO>` or `<ALGO>` one optimiser's help: ld_lbfgs ("Options
 * for NLOPT:LD_LBFGS", its stops), CERES:STEEPEST_DESCENT (its PARAM keys,
 * that it ignores the box, that a point it proposes outside the box is not
 * evaluated, and that the slope of that point's score points away from the
 * box, so a descent step leads back into it), CERES:LBFGS (REFUSED, naming
 * NLOPT:LD_LBFGS instead), NLOPT:LN_NEWUOA (REFUSED); an unknown NLOPT:LD_FOO
 * exits 1 naming what is wrong. A name that is no command, no configuration
 * key and no optimiser is refused as before. Without HPO, `tadah explain
 * ALGO` exits 1 saying that this tadah has no HPO.
 * Before the fix: `tadah explain ALGO` and the others exited 1 with
 * "tadah explain: no command or configuration key 'ALGO'" (and 'ld_lbfgs',
 * ...): print_caps, the ALGO help, was reachable only from HPO's tests.
 * Before the box fix (2026-10-15): CERES:LBFGS printed its PARAM keys and
 * "ignores the OPTIM box", and CERES:STEEPEST_DESCENT's help said its
 * iterates "are evaluated there, and such a point can become the best";
 * then it gave that slope the wrong sign, "... graded by its
 * distance to the box, whose slope points back into it, it counts against
 * MAXEVAL ...": the slope check fails.
 * A refusal names its recommended replacement ("The recommended
 * replacement is NLOPT:LN_BOBYQA", "The recommended replacements are
 * NLOPT:LD_LBFGS ..."); before HPO's help was rewritten for a user it said
 * "Use ...", and the two checks of the replacement fail on that text. */
TEST_CASE("Subcommand: explain: ALGO lists the optimisers of tadah hpo, and "
          "LIB:ALGO describes one",
          "[TadahCLI][exit-status][explain][explain_algo]") {
  ScratchCwd cwd("explain_algo");
#ifdef TADAH_ENABLE_NFP
  for (const char* name : {"ALGO", "algo", "LIB"}) {
    const CliRun r = run_tadah({"tadah", "explain", name});
    INFO(name << ": status " << r.status << "\n" << reported(r));
    CHECK(r.status == 0);
    CHECK(contains(r.out, "ALGO, with LIB its library, names the optimiser "
                          "of a `tadah hpo` target"));
    CHECK(contains(r.out, "\nLIB NLOPT\n"));
    CHECK(contains(r.out, "\n  LD_LBFGS "));
    CHECK(contains(r.out, "NLOPT:LN_NEWUOA,"));
    CHECK(contains(r.out, "CERES:LEVENBERG_MARQUARDT"));
  }
  {
    const CliRun r = run_tadah({"tadah", "explain", "ld_lbfgs"});
    INFO("ld_lbfgs: status " << r.status << "\n" << reported(r));
    CHECK(r.status == 0);
    CHECK(r.out.rfind("Options for NLOPT:LD_LBFGS\n", 0) == 0);
    CHECK(contains(r.out, "Stops that end a run: MAXEVAL"));
  }
  {
    const CliRun r =
        run_tadah({"tadah", "explain", "CERES:STEEPEST_DESCENT"});
    INFO("CERES:STEEPEST_DESCENT: status " << r.status << "\n"
                                           << reported(r));
    CHECK(r.status == 0);
    CHECK(contains(r.out, "Options for CERES:STEEPEST_DESCENT\n"));
    CHECK(contains(r.out, "PARAM tol_gradinf"));
    CHECK(contains(r.out, "ignores the OPTIM box"));
    CHECK(contains(r.out, "Such a point is not evaluated: it is scored as a "
                          "failure graded by its distance to the box"));
    CHECK(contains(r.out, "whose slope points away from the box, so a "
                          "descent step leads back into it; it counts "
                          "against MAXEVAL"));
    CHECK_FALSE(contains(r.out, "points back into it"));
  }
  {
    const CliRun r = run_tadah({"tadah", "explain", "CERES:LBFGS"});
    INFO("CERES:LBFGS: status " << r.status << "\n" << reported(r));
    CHECK(r.status == 0);
    CHECK(contains(r.out, "Options for CERES:LBFGS\n   * REFUSED: "));
    CHECK(contains(r.out, "The recommended replacements are NLOPT:LD_LBFGS"));
    CHECK_FALSE(contains(r.out, "PARAM tol_gradinf"));
  }
  {
    const CliRun r = run_tadah({"tadah", "explain", "NLOPT:LN_NEWUOA"});
    INFO("NLOPT:LN_NEWUOA: status " << r.status << "\n" << reported(r));
    CHECK(r.status == 0);
    CHECK(contains(r.out, "Options for NLOPT:LN_NEWUOA\n   * REFUSED: "));
    CHECK(contains(r.out, "The recommended replacement is NLOPT:LN_BOBYQA"));
  }
  {
    const CliRun r = run_tadah({"tadah", "explain", "NLOPT:LD_FOO"});
    INFO("NLOPT:LD_FOO: status " << r.status << "\n" << reported(r));
    CHECK(r.status == 1);
    CHECK(contains(reported(r), "tadah explain: unknown optimiser "
                                "NLOPT:LD_FOO: LIB NLOPT has no ALGO LD_FOO"));
  }
  /* Pins: MLSL's and AUGLAG's help says that their INNER block takes a
   * local optimiser (no nested global search). Before the fix: no such
   * entry, the checks fail. */
  {
    const CliRun r = run_tadah({"tadah", "explain", "NLOPT:G_MLSL_LDS"});
    INFO("NLOPT:G_MLSL_LDS: status " << r.status << "\n" << reported(r));
    CHECK(r.status == 0);
    CHECK(contains(r.out, "\n   * INNER ... ENDINNER: the local search it "
                          "runs, from the points it\n     samples in the "
                          "OPTIM box: LIB NLOPT and a local ALGO; a global "
                          "one"));
    CHECK(contains(r.out, "The block is required: NLopt has no default"));
  }
  {
    const CliRun r = run_tadah({"tadah", "explain", "LN_AUGLAG"});
    INFO("LN_AUGLAG: status " << r.status << "\n" << reported(r));
    CHECK(r.status == 0);
    CHECK(contains(r.out, "\n   * INNER ... ENDINNER: the local search it "
                          "runs, from its own starting\n     point: LIB NLOPT "
                          "and a local ALGO"));
    CHECK(contains(r.out, "Without one NLopt runs its default LN_COBYLA."));
  }
#else
  const CliRun r = run_tadah({"tadah", "explain", "ALGO"});
  INFO("ALGO: status " << r.status << "\n" << reported(r));
  CHECK(r.status == 1);
  CHECK(contains(reported(r), "this tadah is built without HPO"));
#endif
  const CliRun bogus = run_tadah({"tadah", "explain", "bogus"});
  CHECK(bogus.status == 1);
  CHECK(contains(reported(bogus),
                 "tadah explain: no command or configuration key 'bogus'"));
}

/* Pins: a name with two meanings prints both: `tadah explain RANDOM` (and
 * `random`) exits 0 and prints the configuration key RANDOM's text (a data
 * key: "Draws N distinct structures ..."), then, in a build with HPO, a
 * blank line, "RANDOM also names an optimiser of `tadah hpo`:" (the name as
 * written) and TADAH:RANDOM's help ("Options for TADAH:RANDOM", its stops),
 * so `tadah explain <ALGO>`, which the optimiser table and `tadah hpo
 * --help` point to, reaches it; `tadah explain TADAH:RANDOM` prints the
 * optimiser's help alone. Without HPO the key's text alone.
 * Before the fix: `tadah explain RANDOM` printed the data key's text alone
 * (a configuration key answered first and the optimiser help was not
 * asked), with nothing to say that TADAH:RANDOM was meant: the "also names"
 * and "Options for TADAH:RANDOM" checks fail. */
TEST_CASE("Subcommand: explain: a name that is a configuration key and an "
          "optimiser prints both",
          "[TadahCLI][exit-status][explain][explain_algo]") {
  ScratchCwd cwd("explain_random");
  const std::string key_text = "Draws N distinct structures from the whole "
                               "dataset";
  for (const char* name : {"RANDOM", "random"}) {
    const CliRun r = run_tadah({"tadah", "explain", name});
    INFO(name << ": status " << r.status << "\n" << reported(r));
    CHECK(r.status == 0);
    CHECK(r.out.rfind(key_text, 0) == 0);
#ifdef TADAH_ENABLE_NFP
    const std::size_t also = r.out.find(
        "\n\n" + std::string(name) +
        " also names an optimiser of `tadah hpo`:\nOptions for "
        "TADAH:RANDOM\n");
    CHECK(also != std::string::npos);
    CHECK(contains(r.out, "Stops that end a run: MAXEVAL"));
    CHECK(r.out.find(key_text, 1) == std::string::npos);
#else
    CHECK_FALSE(contains(r.out, "also names"));
    CHECK_FALSE(contains(r.out, "Options for TADAH:RANDOM"));
#endif
  }
#ifdef TADAH_ENABLE_NFP
  const CliRun r = run_tadah({"tadah", "explain", "TADAH:RANDOM"});
  INFO("TADAH:RANDOM: status " << r.status << "\n" << reported(r));
  CHECK(r.status == 0);
  CHECK(r.out.rfind("Options for TADAH:RANDOM\n", 0) == 0);
  CHECK_FALSE(contains(r.out, key_text));
  CHECK_FALSE(contains(r.out, "also names"));
#endif
}

/* Pins: `tadah hpo --help` names `tadah explain ALGO`, where the optimisers
 * of a target's LIB and ALGO are listed, and `tadah explain <LIB>:<ALGO>`.
 * Before the fix: the help said nothing of them (its long description
 * named no optimiser), and the check fails. */
TEST_CASE("Subcommand: hpo --help points to tadah explain ALGO",
          "[TadahCLI][exit-status][explain][explain_algo]") {
  ScratchCwd cwd("hpo_help_algo");
  const CliRun r = run_tadah({"tadah", "hpo", "--help"});
  INFO("status " << r.status << "\n" << reported(r));
  CHECK(r.status == 0);
  CHECK(contains(r.out, "tadah explain ALGO"));
  CHECK(contains(r.out, "tadah explain <LIB>:<ALGO>"));
}

/* ---- explain: the description of a key without a long description --- */

/* Pins: `tadah explain <command>.<key>` exits 0 and prints the key's
 * description when it has no long description, then its EXAMPLES: the
 * refit numbers' ranges, which `tadah refit --help` points to, are shown
 * (refit.refit_compact_seed "an integer from 0 to 4294967295",
 * refit.refit_reach_smin "A number from 0.1 to 1", refit.output_grid_scale
 * "a number > 0"); a key with a long description prints that, not the
 * description (refit.add).
 * Before the fix: only the long description was printed, so a key without
 * one printed an empty line and its EXAMPLES (the description is not
 * found in the output). */
TEST_CASE("Subcommand: explain: a key without a long description prints its "
          "description",
          "[TadahCLI][exit-status][explain]") {
  ScratchCwd cwd("explain_description");
  for (const auto& [key, text] :
       std::vector<std::pair<std::string, std::string>>{
           {"refit.refit_compact_seed",
            "`tadah refit` compact mode: RNG seed for the jittered ladder "
            "restarts (deterministic reruns), an integer from 0 to "
            "4294967295. Default 12345."},
           {"refit.refit_reach_smin", "A number from 0.1 to 1"},
           {"refit.output_grid_scale", "a number > 0"},
           {"refit.refit_tol_ambient", "A number > 0. Default 0.1."}}) {
    const CliRun r = run_tadah({"tadah", "explain", key});
    INFO(key << ": status " << r.status << "\n" << reported(r));
    CHECK(r.status == 0);
    CHECK(contains(r.out, text));
    CHECK(contains(r.out, "EXAMPLES:"));
  }
  const CliRun add = run_tadah({"tadah", "explain", "refit.add"});
  INFO("refit.add: status " << add.status << "\n" << reported(add));
  CHECK(add.status == 0);
  CHECK(contains(add.out, "Inserts one basis function into the curve selected "
                          "by --func"));
  CHECK_FALSE(contains(add.out, "`tadah refit --edit`: inserts a basis "
                                "function at this centre"));
}

/* Pins: `tadah data print -s si.cif` on diamond Si given only by its
 * space-group number (227, Si at 1/8 1/8 1/8, a = 5.431 A) exits 0 and
 * prints the structure's label with the space group the cell is built
 * with, origin choice 2 (8 atoms):
 *   Structure  :    CIF file: si.cif | Symmetry: F d -3 m:2 | Volume: ...
 * Before the fix: the label showed the H-M text, which this CIF has not:
 * "CIF file: si.cif | Symmetry:  | Volume: 160.191478" (an empty field). */
TEST_CASE("Subcommand: data print -s names the space group a CIF's cell is "
          "built with",
          "[TadahCLI][exit-status][data]") {
  ScratchCwd cwd("print_cif_group");
  {
    std::ofstream os("si.cif");
    os << "data_Si_number_only\n"
          "_cell_length_a 5.431\n_cell_length_b 5.431\n_cell_length_c 5.431\n"
          "_cell_angle_alpha 90\n_cell_angle_beta 90\n_cell_angle_gamma 90\n"
          "_symmetry_Int_Tables_number 227\n"
          "loop_\n_atom_site_label\n_atom_site_type_symbol\n"
          "_atom_site_fract_x\n_atom_site_fract_y\n_atom_site_fract_z\n"
          "Si1 Si 0.125 0.125 0.125\n";
  }
  const CliRun r = run_tadah({"tadah", "data", "print", "-s", "si.cif"});
  INFO("status " << r.status << "\n" << reported(r));
  REQUIRE(r.status == 0);
  const auto structure = line_tokens(r.out, "Structure");
  REQUIRE_FALSE(structure.empty());
  CHECK(contains(r.out, "CIF file: si.cif | Symmetry: F d -3 m:2 | Volume: "));
  CHECK_FALSE(contains(r.out, "Symmetry:  |"));
  const auto atoms = line_tokens(r.out, "Atoms");
  REQUIRE(atoms.size() >= 3);
  CHECK(atoms[2] == "8");
}


/* ---- the [nonlinear] cases --------------------------------------------- */

namespace {

/* The non-linear models of the [nonlinear] cases: the MODEL and MPARAMS
 * lines (MPARAMS for the scale of the descriptor below, as in MLIP's
 * test_nonlinear_model_training.cpp). */
struct NonlinearModel {
  const char* model;    // the MODEL line
  const char* mparams;  // the MPARAMS line, "" for none
  bool kernel;          // M_KRR with SBASIS/BASIS
};
const NonlinearModel kNonlinearModels[] = {
    {"MODEL M_KRR Kern_RBF 1", "MPARAMS 1.0", true},
    {"MODEL M_KRR Kern_Polynomial 1", "MPARAMS 3 0.5 1.0", true},
    {"MODEL M_KRR Kern_Quadratic 1", "", true},
    {"MODEL M_KRR Kern_LQ 1", "", true},
    {"MODEL M_KRR Kern_Sigmoid 1", "MPARAMS 0.5 -1.0", true},
    {"MODEL M_BLR BF_Polynomial2", "", false},
};

/* One cell of the dataset below: lattice constant, shear, fractional
 * coordinates. */
struct CellSpec {
  double a;
  double s0, s1, s2;
  std::vector<std::array<double, 3>> frac;
};

/* Six sheared, rattled Ta cells of 3 and 4 atoms (21 atoms: every atomic
 * descriptor differs from every other) in the dataset format, with
 * arbitrary smooth reference energies, forces and stresses. */
std::string nonlinear_dataset() {
  const std::vector<CellSpec> cells = {
      {3.30, 0.05, 0.02, -0.03,
       {{0.00, 0.01, 0.02}, {0.48, 0.52, 0.05}, {0.27, 0.55, 0.51}}},
      {3.45, -0.04, 0.03, 0.06,
       {{0.01, -0.02, 0.00}, {0.52, 0.47, 0.03}, {0.49, 0.02, 0.53},
        {-0.03, 0.51, 0.48}}},
      {3.15, 0.07, -0.05, 0.02,
       {{0.02, -0.01, 0.00}, {0.33, 0.31, 0.36}, {0.68, 0.64, 0.70}}},
      {3.60, 0.00, 0.08, -0.02,
       {{0.00, 0.00, 0.03}, {0.46, 0.50, 0.01}, {0.53, 0.04, 0.47},
        {0.02, 0.55, 0.52}}},
      {3.22, -0.06, 0.01, 0.04,
       {{0.01, 0.03, -0.02}, {0.51, 0.43, 0.10}, {0.22, 0.61, 0.57}}},
      {3.52, 0.03, -0.02, -0.05,
       {{-0.02, 0.01, 0.01}, {0.49, 0.53, -0.02}, {0.47, -0.03, 0.50},
        {0.04, 0.48, 0.55}}},
  };
  std::ostringstream os;
  os.precision(17);
  for (std::size_t s = 0; s < cells.size(); ++s) {
    const CellSpec& c = cells[s];
    const double ds = static_cast<double>(s);
    const double cell[3][3] = {{c.a, 0, 0},
                               {c.s0 * c.a, c.a, 0},
                               {c.s1 * c.a, c.s2 * c.a, c.a}};
    os << "cell_" << s << "\n1.0 1.0 1.0\n"
       << -8.1 * static_cast<double>(c.frac.size()) +
              0.37 * std::sin(1.3 * ds + 0.2)
       << "\n";
    for (const auto& row : cell) os << row[0] << " " << row[1] << " " << row[2] << "\n";
    for (int x = 0; x < 3; ++x) {
      for (int y = 0; y < 3; ++y)
        os << 0.02 * std::cos(0.9 * std::min(x, y) + 1.7 * std::max(x, y) + 0.5 * ds)
           << (y < 2 ? " " : "\n");
    }
    for (std::size_t a = 0; a < c.frac.size(); ++a) {
      os << "Ta";
      for (int k = 0; k < 3; ++k)
        os << " " << c.frac[a][0] * cell[0][k] + c.frac[a][1] * cell[1][k] +
                         c.frac[a][2] * cell[2][k];
      for (int k = 0; k < 3; ++k)
        os << " " << 0.25 * std::sin(0.7 * static_cast<double>(a) + 1.1 * k + 0.3 * ds);
      os << "\n";
    }
    os << "\n";
  }
  return os.str();
}

/* A training configuration of `m` on db.tadah: D2_Blip with 3 functions
 * (descriptor size 3, 4 with BIAS true), Cut_Cos 5 A, SBASIS 5 for a
 * kernel, and the `extra` lines (BIAS, NORM, LAMBDA, OALGO). */
std::string nonlinear_config(const NonlinearModel& m, const std::string& extra) {
  std::string c =
      "DBFILE db.tadah\nATOM Ta\nWATOM 1\nINIT2B true\nINITMB false\n"
      "TYPE2B D2_Blip 3 3 Ta Ta\nCGRID2B 2.8 3.5 4.2\nSGRID2B 2.0 2.0 2.0\n"
      "RCTYPE2B Cut_Cos\nRCUT2B 5.0\nVERBOSE 0\n";
  c += std::string(m.model) + "\n";
  if (*m.mparams) c += std::string(m.mparams) + "\n";
  if (m.kernel) c += "SBASIS 5\n";
  return c + extra;
}

/* `text` with the line of `key` replaced by `line` ("" removes it). */
std::string with_line(const std::string& text, const std::string& key,
                      const std::string& line) {
  std::istringstream in(text);
  std::string out;
  for (std::string l; std::getline(in, l);) {
    std::istringstream ss(l);
    std::string k;
    ss >> k;
    if (k == key) {
      if (!line.empty()) out += line + "\n";
      continue;
    }
    out += l + "\n";
  }
  return out;
}

}  // namespace

/* Pins: for each non-linear model (M_KRR Kern_RBF, Kern_Polynomial,
 * Kern_Quadratic, Kern_LQ, Kern_Sigmoid; M_BLR BF_Polynomial2) and each of
 * BIAS false (LAMBDA 0), BIAS true with LAMBDA 1e-3 (the intercept rules of
 * the ridge) and NORM true, `tadah train -c config -o pot.tadah -F -S`
 * exits 0; the potential has the MODEL line, and for a kernel SBASIS 5 and
 * 5 x 3 (5 x 4 with BIAS) BASIS values; a second training, the same
 * command with the first potential moved aside, writes the same file byte
 * for byte (the basis draw is fixed); `tadah predict -p pot.tadah -d
 * db.tadah -F -S` exits 0 with 6 energies and 63 force components, each a
 * number (|value| < 1e3).
 * Before the fix: the first training (M_KRR Kern_RBF, BIAS false) exited
 * 1 with "ColumnLayout: total_cols=3 disagrees with design-matrix width=5"
 * and wrote no potential (the REQUIRE stops there; the check refused every
 * non-linear model: width 6 for BF_Polynomial2, and with BIAS true
 * total_cols=4 against 5 or 10).
 * Before the training record: passes as well (a pin); the second training
 * wrote pot2.tadah (-o pot2.tadah), compared with pot.tadah: the record
 * names the run's -o (OUTFILE pot2.tadah), so the second training now runs
 * the same command. */
TEST_CASE("Subcommand: train and predict work for every non-linear model",
          "[TadahCLI][train][predict][nonlinear]") {
  ScratchCwd cwd("nonlinear_train");
  std::ofstream("db.tadah") << nonlinear_dataset();
  const std::vector<std::pair<std::string, std::string>> variants = {
      {"BIAS false", "BIAS false\nNORM false\nLAMBDA 0\nOALGO 1\n"},
      {"BIAS true, LAMBDA 1e-3", "BIAS true\nNORM false\nLAMBDA 1e-3\nOALGO 1\n"},
      {"NORM true", "BIAS false\nNORM true\nLAMBDA 0\nOALGO 1\n"}};
  for (const NonlinearModel& m : kNonlinearModels) {
    for (const auto& v : variants) {
      INFO(m.model << ", " << v.first);
      std::ofstream("config") << nonlinear_config(m, v.second);
      for (const char* f : {"pot.tadah", "pot1.tadah", "energy.pred", "forces.pred"})
        std::filesystem::remove(f);
      const CliRun t = run_tadah({"tadah", "train", "-c", "config", "-o",
                                  "pot.tadah", "-F", "-S"});
      INFO("train: status " << t.status << "\n" << reported(t));
      REQUIRE(t.status == 0);
      const std::string pot = slurp("pot.tadah");
      std::vector<std::string> want_model;
      {
        std::istringstream ms(m.model);
        for (std::string tok; ms >> tok;) want_model.push_back(tok);
      }
      CHECK(line_tokens(pot, "MODEL") == want_model);
      if (m.kernel) {
        const std::size_t dsize = v.first.rfind("BIAS true", 0) == 0 ? 4 : 3;
        CHECK(line_tokens(pot, "SBASIS") == std::vector<std::string>{"SBASIS", "5"});
        CHECK(line_tokens(pot, "BASIS").size() == 1 + 5 * dsize);
      }
      std::filesystem::rename("pot.tadah", "pot1.tadah");
      const CliRun t2 = run_tadah({"tadah", "train", "-c", "config", "-o",
                                   "pot.tadah", "-F", "-S"});
      REQUIRE(t2.status == 0);
      CHECK(slurp("pot.tadah") == pot);

      const CliRun p = run_tadah({"tadah", "predict", "-p", "pot.tadah", "-d",
                                  "db.tadah", "-F", "-S"});
      INFO("predict: status " << p.status << "\n" << reported(p));
      REQUIRE(p.status == 0);
      const std::vector<double> e = predicted_values("energy.pred");
      const std::vector<double> f = predicted_values("forces.pred");
      CHECK(e.size() == 6);
      CHECK(f.size() == 63);
      for (double x : e) CHECK(std::fabs(x) < 1e3);
      for (double x : f) CHECK(std::fabs(x) < 1e3);
    }
  }
}

/* Pins: for each non-linear model, with NORM false and NORM true, `tadah
 * train` with OALGO 3 (LAMBDA 0) exits 0 and writes a potential without
 * WCOV, WNOISE or SIGMA (uncertainty output is removed),
 * and `tadah predict -F -S` with it exits 0 with 6 energies and 63 force
 * components, each a number (|value| < 1e3).
 * Before the fix: the potentials had WCOV and WNOISE (this case, then named
 * "Subcommand: predict --error predicts what predict predicts for every
 * non-linear model", compared `predict --error` with `predict`). Before
 * that: the trainings exited 1 (the ColumnLayout check). */
TEST_CASE("Subcommand: every non-linear model trains with OALGO 3 and "
          "predicts",
          "[TadahCLI][train][predict][nonlinear]") {
  ScratchCwd cwd("nonlinear_oalgo3");
  std::ofstream("db.tadah") << nonlinear_dataset();
  for (const NonlinearModel& m : kNonlinearModels) {
    for (const char* norm : {"false", "true"}) {
      INFO(m.model << ", NORM " << norm);
      for (const char* f : {"pot.tadah", "energy.pred", "forces.pred"})
        std::filesystem::remove(f);
      std::ofstream("config") << nonlinear_config(
          m, std::string("BIAS false\nNORM ") + norm + "\nLAMBDA 0\nOALGO 3\n");
      const CliRun t = run_tadah({"tadah", "train", "-c", "config", "-o",
                                  "pot.tadah", "-F", "-S"});
      INFO("train: status " << t.status << "\n" << reported(t));
      REQUIRE(t.status == 0);
      const std::string pot = slurp("pot.tadah");
      for (const char* key : {"WCOV", "WNOISE", "SIGMA"}) {
        INFO(key);
        CHECK(line_tokens(pot, key).empty());
      }
      const CliRun p = run_tadah({"tadah", "predict", "-p", "pot.tadah", "-d",
                                  "db.tadah", "-F", "-S"});
      INFO("predict: status " << p.status << "\n" << reported(p));
      REQUIRE(p.status == 0);
      const std::vector<double> e = predicted_values("energy.pred");
      const std::vector<double> f = predicted_values("forces.pred");
      CHECK(e.size() == 6);
      CHECK(f.size() == 63);
      for (double x : e) CHECK(std::fabs(x) < 1e3);
      for (double x : f) CHECK(std::fabs(x) < 1e3);
    }
  }
}

/* Pins: `tadah train` of M_KRR Kern_RBF with SBASIS 0 or 1 exits 1, naming
 * SBASIS ("SBASIS 0", "at least 2"), and writes no potential.
 * Before the fix: SBASIS 0 wrote into a basis of 0 columns (SIGSEGV, the
 * process died); SBASIS 1 built the basis e0 alone and exited 1 with the
 * ColumnLayout message, which names no SBASIS. */
TEST_CASE("Subcommand: train refuses SBASIS 0 and 1 for a non-linear kernel",
          "[TadahCLI][train][nonlinear]") {
  ScratchCwd cwd("nonlinear_sbasis");
  std::ofstream("db.tadah") << nonlinear_dataset();
  for (const char* s : {"1", "0"}) {
    INFO("SBASIS " << s);
    std::filesystem::remove("pot.tadah");
    std::ofstream("config") << with_line(
        nonlinear_config(kNonlinearModels[0],
                         "BIAS false\nNORM false\nLAMBDA 0\nOALGO 1\n"),
        "SBASIS", std::string("SBASIS ") + s);
    const CliRun r = run_tadah({"tadah", "train", "-c", "config", "-o",
                                "pot.tadah", "-F", "-S"});
    const std::string rep = reported(r);
    INFO("status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    CHECK(contains(rep, std::string("SBASIS ") + s));
    CHECK(contains(rep, "at least 2"));
    CHECK_FALSE(std::filesystem::exists("pot.tadah"));
  }
}

/* Pins: `tadah predict` refuses an M_KRR Kern_RBF potential (written
 * here: SBASIS 5, descriptor size 3) whose basis does not fit, exiting 1
 * with a message naming the keys and writing no energy.pred: without the
 * BASIS line ("no basis vectors (SBASIS and BASIS)"); with 4 WEIGHTS ("4
 * WEIGHTS but SBASIS 5"); with 7 BASIS values ("not a multiple of SBASIS
 * 5"); with 5 vectors of 4 values ("basis vectors of 4 values each", "has
 * 3 components (DSIZE)"). With 15 BASIS values and 5 WEIGHTS it predicts
 * (a pin).
 * Before the fix: without BASIS it exited 0 and predicted E = 0 for every
 * structure; with 4 WEIGHTS it exited 0, having read past them; with 7
 * values it wrote past a vector of 5 and the process aborted (a heap
 * corruption), so the last case did not run (with vectors of 4 values
 * every kernel evaluation reads past the descriptors). */
TEST_CASE("Subcommand: predict refuses a non-linear KRR potential whose "
          "basis does not fit, naming the keys",
          "[TadahCLI][predict][nonlinear]") {
  ScratchCwd cwd("nonlinear_basis_load");
  std::ofstream("db.tadah") << nonlinear_dataset();
  auto values = [](const char* key, int n, double step) {
    std::ostringstream o;
    o << key;
    for (int v = 0; v < n; ++v) o << " " << step * (v + 1);
    return o.str();
  };
  const std::string pot =
      with_line(nonlinear_config(kNonlinearModels[0],
                                 "BIAS false\nNORM false\nLAMBDA 0\nOALGO 1\n"),
                "DBFILE", "") +
      values("BASIS", 15, 0.1) + "\n" + values("WEIGHTS", 5, 0.5) + "\n";
  {
    std::ofstream("pot.tadah") << pot;
    const CliRun r = run_tadah({"tadah", "predict", "-p", "pot.tadah", "-d",
                                "db.tadah"});
    INFO("15 BASIS values, 5 WEIGHTS: status " << r.status << "\n" << reported(r));
    CHECK(r.status == 0);
    CHECK(predicted_values("energy.pred").size() == 6);
  }
  const std::vector<std::pair<std::string, std::vector<std::string>>> bad = {
      {with_line(pot, "BASIS", ""),
       {"no basis vectors (SBASIS and BASIS)"}},
      {with_line(pot, "WEIGHTS", values("WEIGHTS", 4, 0.5)),
       {"4 WEIGHTS but SBASIS 5"}},
      {with_line(pot, "BASIS", values("BASIS", 7, 0.1)),
       {"not a multiple of SBASIS 5"}},
      {with_line(pot, "BASIS", values("BASIS", 20, 0.1)),
       {"basis vectors of 4 values each", "has 3 components (DSIZE)"}}};
  for (const auto& b : bad) {
    INFO("expected: " << b.second.front());
    std::ofstream("bad.tadah") << b.first;
    std::filesystem::remove("energy.pred");
    const CliRun r = run_tadah({"tadah", "predict", "-p", "bad.tadah", "-d",
                                "db.tadah"});
    const std::string rep = reported(r);
    INFO("status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    for (const std::string& what : b.second) CHECK(contains(rep, what));
    CHECK_FALSE(std::filesystem::exists("energy.pred"));
  }
}

/* ---- the [retrain] cases ----------------------------------------------
 *
 * `tadah train -c pot.tadah`, with a DBFILE, retrains a potential from its
 * own file: the fit replaces what the potential holds of its own fit
 * (WEIGHTS, a kernel's BASIS, NMEAN, NSTDEV, SIGMA), so none of it is read
 * when the model to train is built. */

namespace {

/* The numbers of the line of `key` in `text` (its tokens after the key). */
std::vector<double> line_numbers(const std::string& text,
                                 const std::string& key) {
  std::vector<double> v;
  const std::vector<std::string> tok = line_tokens(text, key);
  for (std::size_t i = 1; i < tok.size(); ++i) v.push_back(std::stod(tok[i]));
  return v;
}

/* The potential `pot` as a training configuration of the dataset `db`: its
 * DBFILE line, if it has one (a potential has none; an M_BLR potential
 * written before the potential-content fix kept the one it was trained
 * with), replaced by `DBFILE db`. */
std::string as_training_config(const std::string& pot, const std::string& db) {
  return with_line(pot, "DBFILE", "") + "DBFILE " + db + "\n";
}

/* The Nb-Ta keys with D2_BP of `n` functions (3 or 4). */
std::string nbta_keys_bp(const std::string& keys, int n) {
  std::string k = with_line(keys, "TYPE2B", "TYPE2B D2_BP " +
                                                std::to_string(n) + " " +
                                                std::to_string(n) + " * *");
  k = with_line(k, "CGRID2B",
                n == 4 ? "CGRID2B 2.4 3.1 3.9 4.5" : "CGRID2B 2.4 3.1 3.9");
  return with_line(k, "SGRID2B",
                   n == 4 ? "SGRID2B 1.0 1.2 0.9 1.1" : "SGRID2B 1.0 1.2 0.9");
}

}  // namespace

/* Pins: on Nb-Ta cells all of the composition Nb2 Ta2, M_KRR Kern_Quadratic
 * implementation 2 with BIAS true (SBASIS 5, LAMBDA 1e-3) trains (exit 0:
 * its energies-only solve is not composition-checked), and `tadah train -c`
 * retrains it from its potential plus the DBFILE (exit 0, 5 WEIGHTS and
 * 5 x 5 BASIS values), also from the potential of a 4-function D2_BP (BASIS
 * vectors of 6 values) given a 3-function D2_BP (DSIZE 5: 5 x 5 BASIS
 * values).
 * Before the fix: both retrains exited 1, "cannot determine ... rank 1,
 * while 2 intercepts are fitted": the check before the fit took the
 * intercepts of the potential's basis e_0, e_1 (for the 4-function
 * potential evaluating its 6-value vectors at the 5-entry descriptor, past
 * its end). */
TEST_CASE("Subcommand: train -c retrains an M_KRR implementation 2 BIAS "
          "potential from its own file",
          "[TadahCLI][train][bias][retrain]") {
  ScratchCwd cwd("retrain_krr2");
  std::ofstream("fixed.tadah") << nbta_dataset(true);
  std::string keys = with_line(kNbTaKeys, "LAMBDA", "LAMBDA 1e-3");
  keys = with_line(keys, "MODEL", "MODEL M_KRR Kern_Quadratic 2\nSBASIS 5");
  auto train = [](const std::string& cfg, const std::string& out) {
    const CliRun r = run_tadah({"tadah", "train", "-c", cfg, "-o", out});
    INFO("train -c " << cfg << ": status " << r.status << "\n"
                     << reported(r));
    CHECK(r.status == 0);
    return r.status == 0 && std::filesystem::exists(out);
  };
  std::ofstream("config") << "DBFILE fixed.tadah\n" << keys;
  REQUIRE(train("config", "pot.tadah"));
  std::ofstream("pot_cfg.tadah") << as_training_config(slurp("pot.tadah"),
                                                      "fixed.tadah");
  if (train("pot_cfg.tadah", "re.tadah")) {
    CHECK(line_numbers(slurp("re.tadah"), "WEIGHTS").size() == 5u);
    CHECK(line_numbers(slurp("re.tadah"), "BASIS").size() == 25u);
  }

  std::ofstream("config4") << "DBFILE fixed.tadah\n" << nbta_keys_bp(keys, 4);
  REQUIRE(train("config4", "pot4.tadah"));
  REQUIRE(line_numbers(slurp("pot4.tadah"), "BASIS").size() == 30u);
  std::ofstream("pot4_cfg3.tadah")
      << as_training_config(nbta_keys_bp(slurp("pot4.tadah"), 3),
                            "fixed.tadah");
  if (train("pot4_cfg3.tadah", "re3.tadah"))
    CHECK(line_numbers(slurp("re3.tadah"), "BASIS").size() == 25u);
}

/* Pins: on Nb-Ta cells of varied composition, `tadah train -c` retrains an
 * M_KRR Kern_Quadratic potential (BIAS true, SBASIS 6: 6 WEIGHTS, 6 x 5
 * BASIS values) from its own file with SBASIS 10 and with SBASIS 7 (exit
 * 0; 10 and 7 WEIGHTS, 10 x 5 and 7 x 5 BASIS values), and trains an M_BLR
 * BF_Linear potential (5 WEIGHTS) given MODEL M_KRR Kern_RBF 1, MPARAMS 1.0
 * and SBASIS 6 (exit 0, 6 WEIGHTS).
 * Before the fix: all three exited 1 when the model was built from the
 * potential: "the potential has 6 WEIGHTS but SBASIS 10 basis vectors
 * (BASIS)", "BASIS has 30 values, which is not a multiple of SBASIS 7",
 * "the potential has WEIGHTS but no basis vectors (SBASIS and BASIS)". */
TEST_CASE("Subcommand: train -c retrains a kernel potential with another "
          "SBASIS, and a linear potential as a kernel model",
          "[TadahCLI][train][bias][retrain]") {
  ScratchCwd cwd("retrain_krr_sbasis");
  std::ofstream("db.tadah") << nbta_dataset(false);
  const std::string quad =
      with_line(kNbTaKeys, "MODEL", "MODEL M_KRR Kern_Quadratic 1\nSBASIS 6");
  std::ofstream("config") << "DBFILE db.tadah\n" << quad;
  {
    const CliRun r = run_tadah({"tadah", "train", "-c", "config", "-o",
                                "pot.tadah"});
    INFO("train: status " << r.status << "\n" << reported(r));
    REQUIRE(r.status == 0);
  }
  const std::string pot = slurp("pot.tadah");
  REQUIRE(line_numbers(pot, "WEIGHTS").size() == 6u);
  REQUIRE(line_numbers(pot, "BASIS").size() == 30u);
  for (int s : {10, 7}) {
    CAPTURE(s);
    std::ofstream("re_cfg.tadah") << as_training_config(
        with_line(pot, "SBASIS", "SBASIS " + std::to_string(s)), "db.tadah");
    std::filesystem::remove("re.tadah");
    const CliRun r = run_tadah({"tadah", "train", "-c", "re_cfg.tadah", "-o",
                                "re.tadah"});
    INFO("status " << r.status << "\n" << reported(r));
    CHECK(r.status == 0);
    if (!std::filesystem::exists("re.tadah")) continue;
    const std::string re = slurp("re.tadah");
    CHECK(line_numbers(re, "WEIGHTS").size() == std::size_t(s));
    CHECK(line_numbers(re, "BASIS").size() == std::size_t(5 * s));
  }

  std::ofstream("lin_cfg") << "DBFILE db.tadah\n" << kNbTaKeys;
  {
    const CliRun r = run_tadah({"tadah", "train", "-c", "lin_cfg", "-o",
                                "lin.tadah"});
    INFO("train linear: status " << r.status << "\n" << reported(r));
    REQUIRE(r.status == 0);
  }
  const std::string lin = slurp("lin.tadah");
  REQUIRE(line_numbers(lin, "WEIGHTS").size() == 5u);
  std::ofstream("rbf_cfg.tadah") << as_training_config(
      with_line(lin, "MODEL", "MODEL M_KRR Kern_RBF 1\nMPARAMS 1.0\nSBASIS 6"),
      "db.tadah");
  const CliRun r = run_tadah({"tadah", "train", "-c", "rbf_cfg.tadah", "-o",
                              "rbf.tadah"});
  INFO("linear potential as Kern_RBF: status " << r.status << "\n"
                                               << reported(r));
  CHECK(r.status == 0);
  if (std::filesystem::exists("rbf.tadah"))
    CHECK(line_numbers(slurp("rbf.tadah"), "WEIGHTS").size() == 6u);
}

/* Pins: `tadah train` of an M_BLR BF_Linear model with OALGO 3 (BIAS true
 * on the Nb-Ta cells, LAMBDA 0) writes 5 WEIGHTS and no WCOV, WNOISE or
 * SIGMA; `tadah train -c` with that potential, the DBFILE added, a SIGMA
 * line of 1 + 5 x 5 values appended (as an OALGO 3 potential of Tadah!
 * 1.5.0 has one) and OALGO 1 or 3, is refused (status 1, no re.tadah),
 * naming the SIGMA line, a deprecated key that changes nothing a fit
 * gives ("SIGMA 5 1e-6 0 and 23 more values (line N of re_cfg.tadah)");
 * without the SIGMA line it exits 0 and writes no WCOV, WNOISE or SIGMA,
 * and with OALGO 3 the WEIGHTS of the first fit (1e-12 relative: the same
 * fit of the same data).
 * Before the fix: the potentials had the WCOV and WNOISE of their fit (this
 * case, then named "Subcommand: train -c replaces the covariance of an
 * OALGO 3 potential", compared them). Before fix/hpo-trainer-reuse: the
 * OALGO 1 potential kept the old SIGMA, and with OALGO 3 the new SIGMA was
 * appended to it. Until a fit refused the deprecated keys: the retrain
 * with the SIGMA line exited 0, the line ignored with a notice. */
TEST_CASE("Subcommand: train -c of an OALGO 3 potential writes the weights "
          "of its own fit and no covariance",
          "[TadahCLI][train][retrain]") {
  ScratchCwd cwd("retrain_sigma");
  std::ofstream("db.tadah") << nbta_dataset(false);
  std::ofstream("config") << "DBFILE db.tadah\n"
                          << with_line(kNbTaKeys, "OALGO", "OALGO 3");
  {
    const CliRun r = run_tadah({"tadah", "train", "-c", "config", "-o",
                                "pot.tadah"});
    INFO("train: status " << r.status << "\n" << reported(r));
    REQUIRE(r.status == 0);
  }
  const std::string pot = slurp("pot.tadah");
  const std::vector<double> w = line_numbers(pot, "WEIGHTS");
  REQUIRE(w.size() == 5u);
  for (const char* key : {"WCOV", "WNOISE", "SIGMA"}) {
    INFO(key);
    CHECK(line_tokens(pot, key).empty());
  }
  std::string sigma = "SIGMA 5";
  for (int i = 0; i < 5; ++i)
    for (int j = 0; j < 5; ++j) sigma += i == j ? " 1e-6" : " 0";
  for (int oalgo : {1, 3}) {
    CAPTURE(oalgo);
    std::filesystem::remove("re.tadah");
    std::ofstream("re_cfg.tadah") << as_training_config(
        with_line(pot, "OALGO", "OALGO " + std::to_string(oalgo)) + sigma +
            "\n",
        "db.tadah");
    {
      const CliRun s = run_tadah({"tadah", "train", "-c", "re_cfg.tadah",
                                  "-o", "re.tadah"});
      const std::string rep = reported(s);
      INFO("with SIGMA: status " << s.status << "\n" << rep);
      CHECK(s.status == 1);
      CHECK(contains(rep, "SIGMA 5 1e-6 0 and 23 more values (line "));
      CHECK(contains(rep, " of re_cfg.tadah): "));
      CHECK_FALSE(std::filesystem::exists("re.tadah"));
    }
    std::ofstream("re_cfg.tadah") << as_training_config(
        with_line(pot, "OALGO", "OALGO " + std::to_string(oalgo)), "db.tadah");
    const CliRun r = run_tadah({"tadah", "train", "-c", "re_cfg.tadah", "-o",
                                "re.tadah"});
    INFO("status " << r.status << "\n" << reported(r));
    REQUIRE(r.status == 0);
    const std::string re = slurp("re.tadah");
    for (const char* key : {"WCOV", "WNOISE", "SIGMA"}) {
      INFO(key);
      CHECK(line_tokens(re, key).empty());
    }
    const std::vector<double> out = line_numbers(re, "WEIGHTS");
    REQUIRE(out.size() == w.size());
    if (oalgo == 3)
      for (std::size_t i = 0; i < out.size(); ++i)
        CHECK(out[i] == Approx(w[i]).epsilon(1e-12).margin(1e-300));
  }
}

/* Pins: `tadah train` of M_KRR Kern_Quadratic with BIAS true on the Nb-Ta
 * cells (SBASIS 6) and FIXINDEX 3 prints the FIXINDEX WARNING naming the
 * kernel's intercepts, "its intercepts are weights 1 (Nb) and 2 (Ta)" (the
 * intercept vectors e_0 and e_1 its fit draws first), and trains (exit 0).
 * Before the fix: the WARNING said "weights 1 to 2 belong to its intercept
 * basis vectors e_0 .. e_1, one per species" (the kernel has no basis
 * before its fit). */
TEST_CASE("Subcommand: train warns of FIXINDEX with BIAS true naming the "
          "intercept weights of a kernel",
          "[TadahCLI][train][bias]") {
  ScratchCwd cwd("bias_kernel_fixindex");
  std::ofstream("db.tadah") << nbta_dataset(false);
  std::ofstream("config")
      << "DBFILE db.tadah\n"
      << with_line(kNbTaKeys, "MODEL", "MODEL M_KRR Kern_Quadratic 1\nSBASIS 6")
      << "FIXINDEX 3\nFIXWEIGHT 0.05\n";
  const CliRun r = run_tadah({"tadah", "train", "-c", "config", "-o",
                              "pot.tadah"});
  const std::string rep = reported(r);
  INFO("status " << r.status << "\n" << rep);
  CHECK(r.status == 0);
  CHECK(contains(rep, "FIXINDEX with BIAS true"));
  CHECK(contains(rep, "its intercepts are weights 1 (Nb) and 2 (Ta)"));
}

#ifdef TADAH_ENABLE_NFP
/* Pins: `tadah hpo` with M_KRR Kern_RBF (SBASIS 5) and OPTIM WATOM (1)
 * 0.5 2.0 (every evaluation changes the descriptors, so the cached
 * trainer redraws the basis and refits; LN_SBPLX, MAXEVAL 3, PC_ERMSE and
 * PC_FRMSE, the training set as validation set) exits 0 and writes
 * best_pot.tadah with the model's MODEL, SBASIS 5 and 15 BASIS values;
 * every evaluation has a finite ERMSE[1] in stats.tadah; the ERMSE[1] of
 * the evaluation with the smallest TOTAL_LOSS/N (loss.tadah) equals the
 * Energy RMSE of `tadah predict -A` with best_pot.tadah on the same set
 * (2e-6 relative: stats.tadah and -A each print 7 significant digits), so
 * the potential written is the model evaluated; a second run in the same
 * process gives the same losses, column by column (the ITERATION numbers
 * go on), as the basis draw is fixed.
 * Before the fix: every evaluation's training threw the ColumnLayout
 * check, no evaluation produced a loss, and the run exited 1 without
 * best_pot.tadah. */
TEST_CASE("Subcommand: hpo trains a non-linear KRR model in every evaluation",
          "[TadahCLI][hpo][nonlinear]") {
  ScratchCwd cwd("nonlinear_hpo");
  std::ofstream("db.tadah") << nonlinear_dataset();
  { std::ofstream os("config.val"); os << "DBFILE db.tadah\n"; }
  std::ofstream("config.tadah") << nonlinear_config(
      kNonlinearModels[0], "BIAS false\nNORM false\nLAMBDA 0\nOALGO 1\n"
                           "FORCE true\nSTRESS false\n");
  {
    std::ofstream os("config.hpo");
    os << "OPTIMIZER\nLIB NLOPT\nALGO LN_SBPLX\nMAXEVAL 3\nENDOPTIMIZER\n"
          "LOSS L2\nPC_ERMSE 0 1\nPC_FRMSE 0 1\n"
          "OPTIM WATOM (1) 0.5 2.0\n";
  }
  const std::vector<std::string> loss_columns = {
      "L(ERMSE)[1]", "L(FRMSE)[1]", "TOTAL_LOSS/N"};
  std::vector<std::vector<std::string>> losses[2];
  for (int run = 0; run < 2; ++run) {
    INFO("run " << run + 1);
    for (const char* f : {"best_pot.tadah", "loss.tadah", "stats.tadah"})
      std::filesystem::remove(f);
    const CliRun r = run_tadah({"tadah", "hpo", "-c", "config.tadah",
                                "--hpotarget", "config.hpo",
                                "--validation", "config.val"});
    INFO("status " << r.status << "\n" << reported(r) << "\nhpo_run.log:\n"
                   << (std::filesystem::exists("hpo_run.log")
                           ? slurp("hpo_run.log")
                           : std::string()));
    REQUIRE(r.status == 0);
    REQUIRE(std::filesystem::exists("best_pot.tadah"));
    const std::string best = slurp("best_pot.tadah");
    CHECK(line_tokens(best, "MODEL") ==
          std::vector<std::string>{"MODEL", "M_KRR", "Kern_RBF", "1"});
    CHECK(line_tokens(best, "SBASIS") == std::vector<std::string>{"SBASIS", "5"});
    CHECK(line_tokens(best, "BASIS").size() == 1 + 5 * 3);
    INFO("stats.tadah:\n" << slurp("stats.tadah") << "\nloss.tadah:\n"
                          << slurp("loss.tadah"));
    const auto ermse = table_column("stats.tadah", "ERMSE[1]");
    const auto total = table_column("loss.tadah", "TOTAL_LOSS/N");
    REQUIRE(ermse);
    REQUIRE(total);
    REQUIRE(ermse->size() == 3);
    REQUIRE(total->size() == 3);
    CHECK(table_column("stats.tadah", "#ITERATION") ==
          table_column("loss.tadah", "#ITERATION"));
    std::size_t ibest = 0;
    for (std::size_t i = 0; i < 3; ++i) {
      REQUIRE(is_finite_number((*ermse)[i]));
      REQUIRE(is_finite_number((*total)[i]));
      if (std::strtod((*total)[i].c_str(), nullptr) <
          std::strtod((*total)[ibest].c_str(), nullptr))
        ibest = i;
    }
    const CliRun p = run_tadah({"tadah", "predict", "-p", "best_pot.tadah",
                                "-d", "db.tadah", "-A"});
    INFO("predict: status " << p.status << "\n" << reported(p));
    REQUIRE(p.status == 0);
    const std::vector<std::string> ml =
        metric_fields(p.out, "Energy RMSE (meV/atom)");
    REQUIRE_FALSE(ml.empty());
    const double mean = std::stod(ml[0]);  // meV/atom, as stats.tadah
    CHECK(mean == Approx(std::strtod((*ermse)[ibest].c_str(), nullptr))
                      .epsilon(2e-6));
    for (const std::string& col : loss_columns) {
      const auto v = table_column("loss.tadah", col);
      REQUIRE(v);
      losses[run].push_back(*v);
    }
  }
  CHECK(losses[0] == losses[1]);
}
#endif  // TADAH_ENABLE_NFP

/* ---- [index]: data print, analysis descriptor and analysis bfunc -i ---- */

namespace {

/* The labels of the structures `tadah data print` printed, in order: the
 * third token of each "Structure  :    <label>" line. */
std::vector<std::string> printed_labels(const std::string& out) {
  std::vector<std::string> labels;
  std::istringstream in(out);
  for (std::string line; std::getline(in, line);) {
    std::istringstream ss(line);
    std::string key, colon, label;
    if (ss >> key >> colon >> label && key == "Structure" && colon == ":")
      labels.push_back(label);
  }
  return labels;
}

/* `tadah data print -d db.tadah -i <values...>`. */
CliRun print_indices(const std::vector<std::string>& values) {
  std::vector<std::string> args = {"tadah", "data", "print", "-d", "db.tadah", "-i"};
  args.insert(args.end(), values.begin(), values.end());
  return run_tadah(args);
}

/* Writes db.tadah, Ta dimers S1 ... S5 (5 A cubic cells), and pot.tadah,
 * the D2_BP pair potential kPairPotential. */
void write_descriptor_fixture() {
  std::ofstream os("db.tadah");
  for (int k = 1; k <= 5; ++k)
    os << weighted_entry("S" + std::to_string(k), "1 1 1",
                         std::to_string(2.4 + 0.2 * k) + " 0.3 0.2", "0", "0");
  std::ofstream("pot.tadah") << kPairPotential;
}

/* `tadah analysis descriptor -d db.tadah -p pot.tadah -o d.out -i
 * <values...>`, after removing d.out. */
CliRun descriptor_indices(const std::vector<std::string>& values) {
  std::filesystem::remove("d.out");
  std::vector<std::string> args = {"tadah", "analysis", "descriptor", "-d", "db.tadah",
                                   "-p", "pot.tadah", "-o", "d.out", "-i"};
  args.insert(args.end(), values.begin(), values.end());
  return run_tadah(args);
}

/* The structure labels of a descriptor output file, in order: the text
 * after "# Structure: " on each such line. */
std::vector<std::string> descriptor_labels(const std::string& path) {
  std::vector<std::string> labels;
  std::ifstream in(path);
  const std::string head = "# Structure: ";
  for (std::string line; std::getline(in, line);)
    if (line.rfind(head, 0) == 0) labels.push_back(line.substr(head.size()));
  return labels;
}

/* Writes blip.cfg: a D2_Blip model with 4 basis functions (centres 2, 3, 4
 * and 5 A). */
void write_bfunc_fixture() {
  std::ofstream("blip.cfg") << "ATOM Ta\nTYPE2B D2_Blip 4 4 Ta Ta\nRCTYPE2B Cut_Cos\n"
                               "RCUT2B 9.0\nCGRID2B 2.0 3.0 4.0 5.0\n"
                               "SGRID2B 1.0 1.0 1.0 1.0\nMODEL M_KRR Kern_Linear\n"
                               "INIT2B true\n";
}

/* `tadah analysis bfunc -c blip.cfg -r 1 5 4 --type 2b Y -o b.out
 * [-i <values...>]`, after removing b.out. */
CliRun bfunc_indices(const std::vector<std::string>& values) {
  std::filesystem::remove("b.out");
  std::vector<std::string> args = {"tadah", "analysis", "bfunc", "-c", "blip.cfg", "-r",
                                   "1", "5", "4", "--type", "2b", "Y", "-o", "b.out"};
  if (!values.empty()) {
    args.push_back("-i");
    args.insert(args.end(), values.begin(), values.end());
  }
  return run_tadah(args);
}

/* The blocks of a bfunc output file: its runs of non-blank lines. */
std::vector<std::string> bfunc_blocks(const std::string& path) {
  std::vector<std::string> blocks;
  std::ifstream in(path);
  std::string block;
  for (std::string line; std::getline(in, line);) {
    if (line.find_first_not_of(" \t\r") == std::string::npos) {
      if (!block.empty()) blocks.push_back(block);
      block.clear();
    } else {
      block += line + "\n";
    }
  }
  if (!block.empty()) blocks.push_back(block);
  return blocks;
}

}  // namespace

/* Pins: `tadah data print -i` parses its values strictly and checks them
 * against the 5 structures of the dataset before reading any:
 *  - -i 4-9 exits 1 with "index 9 (in '4-9') is outside 1-5 (the dataset
 *    has 5 structures)" and prints no structure;
 *  - -i 5-2 exits 1 naming the descending range ("'5-2' is a descending
 *    range"); -i 6 exits 1 naming the bound ("index 6 is outside 1-5");
 *  - -i 1 3 (two values) is one list: exit 0, S1 and S3 printed;
 *  - -i 3,1 prints S3 then S1 (the given order); -i 2,2 prints S2 once.
 * Before the fix: 4-9 printed S4 and S5 with exit 0 (6-9 dropped
 * silently), 5-2 and 6 stopped with 'No structures found in the database
 * with the given indices.', and 1 3 was dropped as 'Non-numeric' (exit 1). */
TEST_CASE("Subcommand: data print -i parses strictly and checks bounds first",
          "[TadahCLI][exit-status][data][index]") {
  ScratchCwd cwd("print_index");
  write_numbered_dataset("db.tadah", 5);
  const std::vector<std::pair<std::vector<std::string>, std::string>> bad = {
      {{"4-9"}, "index 9 (in '4-9') is outside 1-5 (the dataset has 5 structures)"},
      {{"5-2"}, "'5-2' is a descending range"},
      {{"6"}, "index 6 is outside 1-5"}};
  for (const auto& b : bad) {
    INFO("-i " << b.first[0]);
    const CliRun r = print_indices(b.first);
    const std::string rep = reported(r);
    INFO("status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    CHECK(contains(rep, "tadah data print: "));
    CHECK(contains(rep, b.second));
    CHECK(printed_labels(r.out).empty());
  }
  const std::vector<std::pair<std::vector<std::string>, std::vector<std::string>>> ok = {
      {{"1", "3"}, numbered_labels({1, 3})},
      {{"3,1"}, numbered_labels({3, 1})},
      {{"2,2"}, numbered_labels({2})}};
  for (const auto& c : ok) {
    INFO("-i " << c.first[0] << (c.first.size() > 1 ? " " + c.first[1] : ""));
    const CliRun r = print_indices(c.first);
    INFO("status " << r.status << "\n" << reported(r));
    CHECK(r.status == 0);
    CHECK(printed_labels(r.out) == c.second);
  }
}

/* Pins: `tadah data print -i 1-10000000` on 5 structures exits 1 at once,
 * naming index 10000000, before any structure is read.
 * Before the fix: the range was expanded to ten million indices and each
 * re-read the file from its start (StructureDB::add(file, i - 1) per
 * index): the command ran for minutes (killed by the test's timeout). */
TEST_CASE("Subcommand: data print -i refuses a range past the dataset at once",
          "[TadahCLI][exit-status][data][index]") {
  ScratchCwd cwd("print_index_range");
  write_numbered_dataset("db.tadah", 5);
  const CliRun r = print_indices({"1-10000000"});
  const std::string rep = reported(r);
  INFO("status " << r.status << "\n" << rep);
  CHECK(r.status == 1);
  CHECK(contains(rep, "index 10000000 (in '1-10000000') is outside 1-5"));
}

/* Pins: `tadah analysis descriptor -i` selects structures by their 1-based
 * index in the DBFILE, S1 ... S5:
 *  - -i 2 exits 0 and the output has structure S2 alone;
 *  - --index 1,3,5 (the example of the command's help) has S1, S3 and S5;
 *  - -i 6 exits 1 with "index 6 is outside 1-5 (the dataset has 5
 *    structures)" and writes no output.
 * Before the fix: every index above 1 was refused with 'Index out of bounds
 * for DBFILE.' (the largest index was compared with the number of DBFILEs,
 * 1), so -i 2 and the help's example failed; -i 6 gave the same message. */
TEST_CASE("Subcommand: analysis descriptor -i selects structures by 1-based index",
          "[TadahCLI][exit-status][analysis][index]") {
  ScratchCwd cwd("descriptor_index");
  write_descriptor_fixture();
  {
    const CliRun r = descriptor_indices({"2"});
    INFO("-i 2: status " << r.status << "\n" << reported(r));
    CHECK(r.status == 0);
    CHECK(descriptor_labels("d.out") == std::vector<std::string>{"S2"});
  }
  {
    std::filesystem::remove("d.out");
    const CliRun r = run_tadah({"tadah", "analysis", "descriptor", "-d", "db.tadah", "-p",
                                "pot.tadah", "-o", "d.out", "--index", "1,3,5"});
    INFO("--index 1,3,5: status " << r.status << "\n" << reported(r));
    CHECK(r.status == 0);
    CHECK(descriptor_labels("d.out") == numbered_labels({1, 3, 5}));
  }
  {
    const CliRun r = descriptor_indices({"6"});
    const std::string rep = reported(r);
    INFO("-i 6: status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    CHECK(contains(rep, "tadah analysis descriptor: index 6 is outside 1-5 (the dataset has "
                        "5 structures)"));
    CHECK_FALSE(std::filesystem::exists("d.out"));
  }
}

/* Pins: `tadah analysis descriptor -i 5-2` exits 1 naming the descending
 * range, and writes no output.
 * Before the fix: 5-2 expanded to no index, and the largest element of the
 * empty list was read: a segmentation fault that ends the test process. */
TEST_CASE("Subcommand: analysis descriptor -i refuses a descending range",
          "[TadahCLI][exit-status][analysis][index]") {
  ScratchCwd cwd("descriptor_index_descending");
  write_descriptor_fixture();
  const CliRun r = descriptor_indices({"5-2"});
  const std::string rep = reported(r);
  INFO("status " << r.status << "\n" << rep);
  CHECK(r.status == 1);
  CHECK(contains(rep, "tadah analysis descriptor: index '5-2' is a descending range"));
  CHECK_FALSE(std::filesystem::exists("d.out"));
}

/* Pins: `tadah analysis descriptor -i 1 3` (two values) is one list: exit
 * 0, the output has S1 and S3.
 * Before the fix: the values were joined into the one entry '1 3', dropped
 * as 'Non-numeric', and the largest element of the empty list was read: a
 * segmentation fault that ends the test process. */
TEST_CASE("Subcommand: analysis descriptor -i takes several values as one list",
          "[TadahCLI][exit-status][analysis][index]") {
  ScratchCwd cwd("descriptor_index_values");
  write_descriptor_fixture();
  const CliRun r = descriptor_indices({"1", "3"});
  INFO("status " << r.status << "\n" << reported(r));
  CHECK(r.status == 0);
  CHECK(descriptor_labels("d.out") == numbered_labels({1, 3}));
}

/* Pins: `tadah analysis descriptor -i 0` exits 1 with "index 0 is outside
 * 1-5", at once.
 * Before the fix: index 0 became StructureDB::add(file, SIZE_MAX), which
 * skipped structures past the end of the file without end: the command hung
 * (the test runs until CTest's timeout). */
TEST_CASE("Subcommand: analysis descriptor -i refuses index 0",
          "[TadahCLI][exit-status][analysis][index]") {
  ScratchCwd cwd("descriptor_index_zero");
  write_descriptor_fixture();
  const CliRun r = descriptor_indices({"0"});
  const std::string rep = reported(r);
  INFO("status " << r.status << "\n" << rep);
  CHECK(r.status == 1);
  CHECK(contains(rep, "tadah analysis descriptor: index 0 is outside 1-5"));
  CHECK_FALSE(std::filesystem::exists("d.out"));
}

/* Pins: `tadah analysis bfunc -i` takes basis-function indices 1 ... 4 of
 * the D2_Blip model of blip.cfg:
 *  - -i 0, -i 9 and -i 5-2 exit 1 naming the index or range and the 4
 *    functions ("index 0 is outside 1-4 (the D2_Blip basis has 4
 *    functions)", "index 9 is outside 1-4", "'5-2' is a descending range"),
 *    and write no output;
 *  - -i 1 3 (two values) exits 0 with exactly three blocks, functions 1 and
 *    3 (as printed without -i) and the cutoff function.
 * Before the fix: -i 0 exited 0 with a function built from the grid entry
 * before the first (an out-of-bounds read), -i 9 was dropped with a WARNING
 * and exit 0, -i 5-2 printed only the cutoff function (exit 0), and -i 1 3
 * was dropped as 'Non-numeric', leaving only the cutoff function. */
TEST_CASE("Subcommand: analysis bfunc -i takes basis-function indices 1..N",
          "[TadahCLI][exit-status][analysis][index]") {
  ScratchCwd cwd("bfunc_index");
  write_bfunc_fixture();
  const CliRun all = bfunc_indices({});
  INFO("without -i: status " << all.status << "\n" << reported(all));
  REQUIRE(all.status == 0);
  const std::vector<std::string> every = bfunc_blocks("b.out");
  REQUIRE(every.size() == 5u);  // 4 functions and the cutoff
  const std::vector<std::pair<std::string, std::string>> bad = {
      {"0", "index 0 is outside 1-4 (the D2_Blip basis has 4 functions)"},
      {"9", "index 9 is outside 1-4"},
      {"5-2", "'5-2' is a descending range"}};
  for (const auto& b : bad) {
    INFO("-i " << b.first);
    const CliRun r = bfunc_indices({b.first});
    const std::string rep = reported(r);
    INFO("status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    CHECK(contains(rep, "tadah analysis bfunc: "));
    CHECK(contains(rep, b.second));
    CHECK_FALSE(std::filesystem::exists("b.out"));
  }
  const CliRun r = bfunc_indices({"1", "3"});
  INFO("-i 1 3: status " << r.status << "\n" << reported(r));
  CHECK(r.status == 0);
  const std::vector<std::string> blocks = bfunc_blocks("b.out");
  REQUIRE(blocks.size() == 3u);
  CHECK(blocks[0] == every[0]);
  CHECK(blocks[1] == every[2]);
  CHECK(blocks[2] == every[4]);
}

/* Pins: `tadah data print -i`, `tadah analysis descriptor -i` and `tadah data
 * write -i` count a structure without atoms (its blank line straight after
 * the stress lines) as one structure, as `data print -d` does:
 *  - on S1, E2 (no atoms), S3: data print -i 3 prints S3 and -i 2 prints E2
 *    (exit 0); -i 4 exits 1 with "index 4 is outside 1-3 (the dataset has 3
 *    structures)";
 *  - on E1, S2: data print -i 2 prints S2 (exit 0);
 *  - on S1, E2, S3: analysis descriptor -i 3 writes S3 alone (exit 0), -i 4
 *    exits 1 with "index 4 is outside 1-3 (the dataset has 3 structures)";
 *    data write -i 3 -o c.cell -f castep writes S3 (exit 0).
 * Before the fix: the structures were counted up to the first one without
 * atoms, so every index past it was refused with a false count ("index 3 is
 * outside 1-1 (the dataset has 1 structures)" for S1, E2, S3; "index 2 is
 * outside 1-0 (the dataset has 0 structures)" for E1, S2), by all three
 * commands. */
TEST_CASE("Subcommand: data print, analysis descriptor and data write -i count a "
          "structure without atoms",
          "[TadahCLI][exit-status][data][index]") {
  ScratchCwd cwd("index_no_atoms");
  {
    std::ofstream os("db.tadah");
    os << weighted_entry("S1", "1 1 1", "2.6 0.3 0.2", "0", "0") << dataset_entry("E2", {})
       << weighted_entry("S3", "1 1 1", "3.0 0.3 0.2", "0", "0");
  }
  {
    std::ofstream os("first.tadah");
    os << dataset_entry("E1", {}) << dataset_entry("S2", {"Na 0 0 0"});
  }
  std::ofstream("pot.tadah") << kPairPotential;
  const std::vector<std::pair<std::string, std::string>> printed = {{"3", "S3"}, {"2", "E2"}};
  for (const auto& c : printed) {
    const CliRun r = print_indices({c.first});
    INFO("data print -i " << c.first << ": status " << r.status << "\n" << reported(r));
    CHECK(r.status == 0);
    CHECK(printed_labels(r.out) == std::vector<std::string>{c.second});
  }
  {
    const CliRun r = print_indices({"4"});
    const std::string rep = reported(r);
    INFO("data print -i 4: status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    CHECK(contains(rep, "tadah data print: index 4 is outside 1-3 (the dataset has 3 "
                        "structures)"));
  }
  {
    const CliRun r =
        run_tadah({"tadah", "data", "print", "-d", "first.tadah", "-i", "2"});
    INFO("data print -d first.tadah -i 2: status " << r.status << "\n" << reported(r));
    CHECK(r.status == 0);
    CHECK(printed_labels(r.out) == std::vector<std::string>{"S2"});
  }
  {
    const CliRun r = descriptor_indices({"3"});
    INFO("analysis descriptor -i 3: status " << r.status << "\n" << reported(r));
    CHECK(r.status == 0);
    CHECK(descriptor_labels("d.out") == std::vector<std::string>{"S3"});
  }
  {
    const CliRun r = descriptor_indices({"4"});
    const std::string rep = reported(r);
    INFO("analysis descriptor -i 4: status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    CHECK(contains(rep, "tadah analysis descriptor: index 4 is outside 1-3 (the dataset has "
                        "3 structures)"));
  }
  {
    const CliRun r = run_tadah({"tadah", "data", "write", "-d", "db.tadah", "-i", "3", "-o",
                                "c.cell", "-f", "castep"});
    INFO("data write -i 3: status " << r.status << "\n" << reported(r));
    CHECK(r.status == 0);
    CHECK(contains(slurp("c.cell"), "S3"));
  }
}

/* Pins: `tadah data print -i` and `tadah analysis descriptor -i` read only
 * the selected structures: with S1's atom line 'Ta 0 0 0 abc 0 0' (not a
 * number), data print -i 2 prints S2 and analysis descriptor -i 2 writes S2
 * alone (exit 0); data print -i 1 exits 1 naming the file, structure 1 and
 * 'abc'.
 * Before the fix: every structure up to the largest index was read (and
 * kept), so -i 2 exited 1 for S1's 'abc' ("DBFILE db.tadah: structure 1:
 * ... 'abc' is not a finite number") in both commands. */
TEST_CASE("Subcommand: data print and analysis descriptor -i read only the selected "
          "structures",
          "[TadahCLI][exit-status][data][index]") {
  ScratchCwd cwd("index_selected_only");
  {
    std::ofstream os("db.tadah");
    os << "S1\n1 1 1\n-1.0\n5 0 0\n0 5 0\n0 0 5\n0 0 0\n0 0 0\n0 0 0\n"
          "Ta 0 0 0 abc 0 0\nTa 2.6 0.3 0.2 0 0 0\n\n"
       << weighted_entry("S2", "1 1 1", "2.8 0.3 0.2", "0", "0");
  }
  std::ofstream("pot.tadah") << kPairPotential;
  {
    const CliRun r = print_indices({"2"});
    INFO("data print -i 2: status " << r.status << "\n" << reported(r));
    CHECK(r.status == 0);
    CHECK(printed_labels(r.out) == std::vector<std::string>{"S2"});
  }
  {
    const CliRun r = descriptor_indices({"2"});
    INFO("analysis descriptor -i 2: status " << r.status << "\n" << reported(r));
    CHECK(r.status == 0);
    CHECK(descriptor_labels("d.out") == std::vector<std::string>{"S2"});
  }
  {
    const CliRun r = print_indices({"1"});
    const std::string rep = reported(r);
    INFO("data print -i 1: status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    CHECK(contains(rep, "DBFILE db.tadah: structure 1: "));
    CHECK(contains(rep, "'abc'"));
    CHECK(printed_labels(r.out).empty());
  }
}

/* ---- [retrain]: a potential used as the training configuration -------- */

namespace {

/* Three Ta dimers with reference forces (FORCE true: 21 rows for the 4
 * weights of D2_BP 4). */
std::string retrain_dataset() {
  return weighted_entry("R1", "1 1 1", "2.60 0.40 0.30", "0.12", "0") +
         weighted_entry("R2", "1 1 1", "3.10 0.20 0.50", "-0.07", "0") +
         weighted_entry("R3", "1 1 1", "3.70 0.10 0.20", "0.03", "0");
}

/* The D2_BP pair configuration of the [retrain] cases with FORCE true,
 * the model `model` and the fit lines `fit`, training on db.tadah. */
std::string retrain_config(const std::string& model, const std::string& fit) {
  return "DBFILE db.tadah\nATOM Ta\nWATOM 73\nINIT2B true\nINITMB false\n"
         "TYPE2B D2_BP 4 4 Ta Ta\nCGRID2B 2.5 3.0 3.5 4.0\n"
         "SGRID2B 0.5 0.5 0.5 0.5\nRCTYPE2B Cut_Cos\nRCUT2B 4.5\n"
         "FORCE true\nSTRESS false\nBIAS false\nNORM false\nVERBOSE 0\n"
         "MODEL " + model + "\n" + fit;
}

/* Column `col` (0-based) of every line after the header of a `tadah
 * predict` output file with numbers in it (energy.pred, forces.pred, ...). */
std::vector<double> column_values(const std::string& file, std::size_t col) {
  std::ifstream in(file);
  std::vector<double> out;
  std::string line;
  std::getline(in, line);  // header
  while (std::getline(in, line)) {
    std::istringstream ls(line);
    std::vector<std::string> tok;
    for (std::string t; ls >> t;) tok.push_back(t);
    if (tok.size() > col) out.push_back(std::stod(tok[col]));
  }
  return out;
}

/* `text` with the line of `key` replaced by `line`. */
std::string replace_key_line(const std::string& text, const std::string& key,
                             const std::string& line) {
  std::istringstream in(text);
  std::ostringstream out;
  for (std::string l; std::getline(in, l);) {
    std::istringstream ls(l);
    std::string k;
    ls >> k;
    out << (k == key ? line : l) << "\n";
  }
  return out.str();
}

}  // namespace

/* Pins: `tadah train -c pot1.tadah` with the DBFILE line added (the
 * documented route), pot1.tadah the potential of an OALGO 3 fit (LAMBDA
 * 1e-3), writes a potential with the WEIGHTS of pot1.tadah (the same tokens:
 * the same data and fit) and no WCOV, WNOISE or SIGMA, and so does a third
 * train from that potential; `tadah predict` with it exits 0 and predicts
 * the 3 energies.
 * Before the fix: the potentials had WCOV and WNOISE (this case, then named
 * "Subcommand: train -c with an OALGO 3 potential writes the covariance of
 * its own fit", compared them, and ran `tadah predict --error`). Before
 * fix/hpo-trainer-reuse: the retrain wrote 34 SIGMA values (each fit
 * appended its SIGMA to the one of the configuration).
 * Before the potential-content fix: the M_BLR potential kept its DBFILE
 * line, so with the DBFILE line added the configuration listed db.tadah
 * twice. */
TEST_CASE("Subcommand: train -c with an OALGO 3 potential writes the "
          "weights of its own fit",
          "[TadahCLI][train][retrain][sigma]") {
  ScratchCwd cwd("retrain_sigma");
  std::ofstream("db.tadah") << retrain_dataset();
  std::ofstream("config.train")
      << retrain_config("M_BLR BF_Linear", "LAMBDA 1e-3\nOALGO 3\n");
  const CliRun t1 = run_tadah({"tadah", "train", "-c", "config.train", "-o", "pot1.tadah"});
  INFO("train: status " << t1.status << "\n" << reported(t1));
  REQUIRE(t1.status == 0);
  const auto w1 = line_tokens(slurp("pot1.tadah"), "WEIGHTS");
  REQUIRE(w1.size() == 1 + 4);
  std::string from = "pot1.tadah";
  for (const char* to : {"pot2.tadah", "pot3.tadah"}) {
    INFO("train -c " << from << " + DBFILE -o " << to);
    std::ofstream("from_cfg.tadah") << slurp(from) << "DBFILE db.tadah\n";
    const CliRun t = run_tadah({"tadah", "train", "-c", "from_cfg.tadah",
                                "-o", to});
    INFO("status " << t.status << "\n" << reported(t));
    REQUIRE(t.status == 0);
    const std::string pot = slurp(to);
    CHECK(line_tokens(pot, "WEIGHTS") == w1);
    for (const char* key : {"WCOV", "WNOISE", "SIGMA"}) {
      INFO(key);
      CHECK(line_tokens(pot, key).empty());
    }
    from = to;
  }
  const CliRun p = run_tadah({"tadah", "predict", "-p", "pot3.tadah", "-d", "db.tadah"});
  INFO("predict: status " << p.status << "\n" << reported(p));
  CHECK(p.status == 0);
  CHECK(predicted_values("energy.pred").size() == 3u);
}

/* Pins: `tadah train -c potk_db.tadah`, potk_db.tadah the potential of an
 * M_KRR Kern_Linear fit with the DBFILE line added (an M_KRR potential is
 * written without one), exits 0 and writes the weights of the same fit (the
 * tokens of potk.tadah's WEIGHTS line).
 * Before the fix: exit 1, "This object is already trained!" (the WEIGHTS of
 * the potential marked the M_KRR model trained). */
TEST_CASE("Subcommand: train -c with an M_KRR potential trains it",
          "[TadahCLI][train][retrain]") {
  ScratchCwd cwd("retrain_krr");
  std::ofstream("db.tadah") << retrain_dataset();
  std::ofstream("config.train")
      << retrain_config("M_KRR Kern_Linear", "LAMBDA 1e-3\nOALGO 1\n");
  const CliRun t1 = run_tadah({"tadah", "train", "-c", "config.train", "-o", "potk.tadah"});
  INFO("train: status " << t1.status << "\n" << reported(t1));
  REQUIRE(t1.status == 0);
  const auto w1 = line_tokens(slurp("potk.tadah"), "WEIGHTS");
  REQUIRE(w1.size() == 1 + 4);
  std::ofstream("potk_db.tadah") << slurp("potk.tadah") << "DBFILE db.tadah\n";
  const CliRun t2 = run_tadah({"tadah", "train", "-c", "potk_db.tadah", "-o",
                               "potk2.tadah"});
  INFO("retrain: status " << t2.status << "\n" << reported(t2));
  REQUIRE(t2.status == 0);
  CHECK(line_tokens(slurp("potk2.tadah"), "WEIGHTS") == w1);
}

/* ---- [potential-content]: a potential records no training data -------- */

namespace {

/* The keys of a potential: the first token of every line that has one and
 * is no comment. */
std::set<std::string> pot_keys(const std::string& text) {
  std::set<std::string> keys;
  std::istringstream in(text);
  for (std::string line; std::getline(in, line);) {
    std::istringstream ls(line);
    std::string k;
    if ((ls >> k) && k[0] != '#') keys.insert(k);
  }
  return keys;
}

/* The numbers after `key` on its line in `text`. */
std::vector<double> key_numbers(const std::string& text,
                                const std::string& key) {
  std::vector<double> v;
  const std::vector<std::string> tok = line_tokens(text, key);
  for (std::size_t i = 1; i < tok.size(); ++i) v.push_back(std::stod(tok[i]));
  return v;
}

/* The three Ta dimers of retrain_dataset() with the energies -1.3, -0.9 and
 * -0.4 eV (-0.65, -0.45 and -0.2 eV/atom). */
std::string distinct_energy_dataset() {
  std::string db = retrain_dataset();
  std::size_t at = 0;
  for (const char* e : {"-1.3", "-0.9", "-0.4"}) {
    at = db.find("\n-1.0\n", at);
    db.replace(at + 1, 4, e);
    at += 1;
  }
  return db;
}

}  // namespace

/* Pins: `tadah train` of one configuration (the three Ta dimers of the
 * [retrain] cases, LAMBDA -1 with OALGO 3: the evidence approximation)
 * with MODEL M_BLR BF_Linear and with M_KRR Kern_Linear writes potentials
 * of the same keys: neither has a DBFILE line, both have ALPHA and BETA,
 * the values the evidence approximation found (equal to 1e-10 relative,
 * the same fit, and not the starting guesses 1), and LAMBDA -1.
 * Before the fix: the M_BLR potential had the DBFILE, ALPHA and BETA lines
 * (ALPHA 2.719, BETA 72.11), the M_KRR one none of the three. */
TEST_CASE("Subcommand: train writes the same keys into M_BLR and M_KRR "
          "potentials: no DBFILE, the ALPHA and BETA of the fit",
          "[TadahCLI][train][retrain][potential-content]") {
  ScratchCwd cwd("pot_content_keys");
  std::ofstream("db.tadah") << retrain_dataset();
  std::string pot[2];
  const char* models[2] = {"M_BLR BF_Linear", "M_KRR Kern_Linear"};
  for (int m = 0; m < 2; ++m) {
    std::ofstream("config.train")
        << retrain_config(models[m], "LAMBDA -1\nOALGO 3\n");
    const CliRun t = run_tadah({"tadah", "train", "-c", "config.train", "-o",
                                "pot.tadah"});
    INFO(models[m] << ": status " << t.status << "\n" << reported(t));
    REQUIRE(t.status == 0);
    pot[m] = slurp("pot.tadah");
  }
  INFO("M_BLR potential:\n" << pot[0] << "\nM_KRR potential:\n" << pot[1]);
  CHECK(pot_keys(pot[0]) == pot_keys(pot[1]));
  for (const std::string& p : pot) {
    CHECK(line_tokens(p, "DBFILE").empty());
    CHECK(key_numbers(p, "LAMBDA") == std::vector<double>{-1.0});
  }
  for (const char* key : {"ALPHA", "BETA"}) {
    INFO(key);
    const std::vector<double> b = key_numbers(pot[0], key);
    const std::vector<double> k = key_numbers(pot[1], key);
    REQUIRE(b.size() == 1u);
    REQUIRE(k.size() == 1u);
    CHECK(k[0] == Approx(b[0]).epsilon(1e-10));
    CHECK(b[0] != 1.0);
  }
}

/* Pins: `tadah train -c pot.tadah` with the documented DBFILE line added,
 * pot.tadah the potential of the three Ta dimers (LAMBDA 0.1, OALGO 1),
 * writes the WEIGHTS of the first fit (1e-12 relative: the same data, the
 * same fit), for MODEL M_BLR BF_Linear and M_KRR Kern_Linear alike; neither
 * potential has a DBFILE line.
 * Before the fix: the M_BLR potential kept its DBFILE line, so its
 * configuration listed db.tadah twice and every structure was fitted twice
 * (LAMBDA at half strength): the first weight was -0.2095 for -0.1884; the
 * M_KRR case passed (a pin). */
TEST_CASE("Subcommand: train -c retrains an M_BLR or M_KRR potential with "
          "the DBFILE line added as it was trained",
          "[TadahCLI][train][retrain][potential-content]") {
  ScratchCwd cwd("pot_content_retrain");
  std::ofstream("db.tadah") << retrain_dataset();
  for (const char* model : {"M_BLR BF_Linear", "M_KRR Kern_Linear"})
    DYNAMIC_SECTION(model) {
      std::ofstream("config.train")
          << retrain_config(model, "LAMBDA 0.1\nOALGO 1\n");
      const CliRun t1 = run_tadah({"tadah", "train", "-c", "config.train",
                                   "-o", "pot1.tadah"});
      INFO("train: status " << t1.status << "\n" << reported(t1));
      REQUIRE(t1.status == 0);
      const std::string pot1 = slurp("pot1.tadah");
      CHECK(line_tokens(pot1, "DBFILE").empty());
      std::ofstream("pot1_cfg.tadah") << pot1 << "DBFILE db.tadah\n";
      const CliRun t2 = run_tadah({"tadah", "train", "-c", "pot1_cfg.tadah",
                                   "-o", "pot2.tadah"});
      INFO("retrain: status " << t2.status << "\n" << reported(t2));
      REQUIRE(t2.status == 0);
      const std::vector<double> w1 = key_numbers(pot1, "WEIGHTS");
      const std::vector<double> w2 = key_numbers(slurp("pot2.tadah"), "WEIGHTS");
      REQUIRE(w1.size() == 4u);
      REQUIRE(w2.size() == w1.size());
      for (std::size_t i = 0; i < w1.size(); ++i) {
        INFO("weight " << i + 1);
        CHECK(w2[i] == Approx(w1[i]).epsilon(1e-12));
      }
    }
}

/* Pins: `tadah train` refuses a DBFILE that lists one dataset twice, exit 1
 * without a potential, naming both entries: the same path on two lines
 * ("DBFILE db.tadah" twice: "entry 1 (db.tadah) and entry 2 (db.tadah)"),
 * two spellings of it on one line ("DBFILE db.tadah ./db.tadah": "entry 2
 * (./db.tadah)"), and an M_BLR potential written before the potential-
 * content fix (its own "DBFILE db.tadah" line) with the DBFILE line of the
 * retrain added (the message says that such a potential records its
 * DBFILE); a configuration of two different files trains (exit 0).
 * Before the fix: each trained (exit 0), fitting every structure of
 * db.tadah twice; the two files passed as well (a pin). */
TEST_CASE("Subcommand: train refuses a DBFILE that lists one dataset twice",
          "[TadahCLI][train][retrain][potential-content]") {
  ScratchCwd cwd("pot_content_twice");
  std::ofstream("db.tadah") << retrain_dataset();
  std::ofstream("db2.tadah") << distinct_energy_dataset();
  const std::string keys = with_line(
      retrain_config("M_BLR BF_Linear", "LAMBDA 0.1\nOALGO 1\n"), "DBFILE",
      "");
  {
    std::ofstream("old.train") << "DBFILE db.tadah\n" << keys;
    const CliRun r = run_tadah({"tadah", "train", "-c", "old.train", "-o",
                                "old.tadah"});
    INFO("train: status " << r.status << "\n" << reported(r));
    REQUIRE(r.status == 0);
  }
  // An M_BLR potential as written before the fix: with its DBFILE line.
  const std::string legacy = as_training_config(slurp("old.tadah"), "db.tadah");
  const struct {
    const char* name;
    std::string config;
    std::vector<std::string> named;
  } cases[] = {
      {"two lines", "DBFILE db.tadah\nDBFILE db.tadah\n" + keys,
       {"entry 1 (db.tadah) and entry 2 (db.tadah)"}},
      {"two spellings", "DBFILE db.tadah ./db.tadah\n" + keys,
       {"entry 1 (db.tadah) and entry 2 (./db.tadah)"}},
      {"a legacy M_BLR potential plus the DBFILE of the retrain",
       legacy + "DBFILE db.tadah\n",
       {"entry 1 (db.tadah) and entry 2 (db.tadah)", "M_BLR potential"}},
  };
  for (const auto& c : cases) DYNAMIC_SECTION(c.name) {
    std::ofstream("twice.train") << c.config;
    std::filesystem::remove("pot.tadah");
    const CliRun r = run_tadah({"tadah", "train", "-c", "twice.train", "-o",
                                "pot.tadah"});
    const std::string rep = reported(r);
    INFO("status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    CHECK(contains(rep, "DBFILE lists one dataset twice"));
    for (const std::string& what : c.named) CHECK(contains(rep, what));
    CHECK_FALSE(std::filesystem::exists("pot.tadah"));
  }
  SECTION("two datasets") {
    std::ofstream("two.train") << "DBFILE db.tadah db2.tadah\n" << keys;
    const CliRun r = run_tadah({"tadah", "train", "-c", "two.train", "-o",
                                "pot.tadah"});
    INFO("status " << r.status << "\n" << reported(r));
    CHECK(r.status == 0);
    CHECK(std::filesystem::exists("pot.tadah"));
  }
}

/* Pins: `tadah train` with MODEL M_KRR Kern_Linear 2 on three Ta dimers of
 * energies -1.3, -0.9 and -0.4 eV (D2_BP 4, BIAS false: DSIZE 4; LAMBDA 0;
 * FORCE false: implementation 2 fits energies only and refuses FORCE true)
 * writes 4 WEIGHTS, the descriptor weights of its basis vectors, and no
 * BASIS, with SBASIS 3 and 4 (e0 and 2 or 3 drawn dimers); `tadah predict`
 * with the potential exits 0, and with SBASIS 4 (every dimer drawn) it
 * predicts each dimer's energy per atom, -0.65, -0.45 and -0.2 eV, to 1e-9
 * (the solve fits them exactly at LAMBDA 0).
 * Before the fix: the potential's WEIGHTS were the SBASIS coefficients of
 * the basis vectors: 3 with SBASIS 3, which `tadah predict` refused (exit
 * 1, "has 3 WEIGHTS, but Kern_Linear needs 4 weights"), and 4 with SBASIS
 * 4, which it took for the descriptor weights (-2.14, -2.50 and -2.30
 * eV/atom). */
TEST_CASE("Subcommand: train with M_KRR Kern_Linear 2 writes descriptor "
          "weights that predict the fitted energies",
          "[TadahCLI][train][predict][krr][potential-content]") {
  ScratchCwd cwd("pot_content_krr2");
  std::ofstream("db.tadah") << distinct_energy_dataset();
  for (const char* sbasis : {"3", "4"}) DYNAMIC_SECTION("SBASIS " << sbasis) {
    std::ofstream("config.train") << replace_key_line(
        retrain_config("M_KRR Kern_Linear 2",
                       std::string("SBASIS ") + sbasis +
                           "\nLAMBDA 0\nOALGO 1\n"),
        "FORCE", "FORCE false");
    const CliRun t = run_tadah({"tadah", "train", "-c", "config.train", "-o",
                                "pot.tadah"});
    INFO("train: status " << t.status << "\n" << reported(t));
    REQUIRE(t.status == 0);
    const std::string pot = slurp("pot.tadah");
    CHECK(key_numbers(pot, "WEIGHTS").size() == 4u);
    CHECK(line_tokens(pot, "BASIS").empty());
    std::filesystem::remove("energy.pred");
    const CliRun p = run_tadah({"tadah", "predict", "-p", "pot.tadah", "-d",
                                "db.tadah"});
    INFO("predict: status " << p.status << "\n" << reported(p));
    CHECK(p.status == 0);
    if (std::string(sbasis) == "4") {
      const std::vector<double> e = predicted_values("energy.pred");
      REQUIRE(e.size() == 3u);
      const double want[3] = {-0.65, -0.45, -0.2};
      for (int s = 0; s < 3; ++s) {
        INFO("dimer " << s + 1);
        CHECK(e[s] == Approx(want[s]).epsilon(1e-9));
      }
    }
  }
}

/* Pins: `tadah train` refuses the SBASIS of a fit that draws basis vectors
 * before it reads the training data, exit 1 without a potential, naming the
 * model: M_KRR Kern_Linear 2 with SBASIS 1 ("M_KRR Kern_Linear 2", "SBASIS
 * 1", "at least 2 basis vectors", without the word "non-linear"), also with
 * a DBFILE that does not exist (the SBASIS refusal, not the file's); M_KRR
 * Kern_Linear 2 without SBASIS ("M_KRR Kern_Linear 2 needs SBASIS"); and
 * M_KRR Kern_RBF 1 without SBASIS ("M_KRR Kern_RBF 1 needs SBASIS").
 * Before the fix: SBASIS 1 was refused by the fit, after the data was read,
 * as "a non-linear M_KRR model" (with the missing DBFILE: the file's error);
 * without SBASIS: "Key not found in the Context object: SBASIS", for both
 * kernels. */
TEST_CASE("Subcommand: train refuses the SBASIS of a basis-drawing M_KRR "
          "fit before it reads the data",
          "[TadahCLI][train][krr][potential-content]") {
  ScratchCwd cwd("krr_sbasis_refused");
  std::ofstream("db.tadah") << distinct_energy_dataset();
  const struct {
    const char* name;
    std::string config;
    std::vector<std::string> named;
  } cases[] = {
      {"Kern_Linear 2, SBASIS 1",
       retrain_config("M_KRR Kern_Linear 2", "SBASIS 1\nLAMBDA 0\nOALGO 1\n"),
       {"M_KRR Kern_Linear 2", "SBASIS 1", "at least 2 basis vectors"}},
      {"Kern_Linear 2, SBASIS 1, no such DBFILE",
       with_line(retrain_config("M_KRR Kern_Linear 2",
                                "SBASIS 1\nLAMBDA 0\nOALGO 1\n"),
                 "DBFILE", "DBFILE no_such.tadah"),
       {"M_KRR Kern_Linear 2", "SBASIS 1", "at least 2 basis vectors"}},
      {"Kern_Linear 2, no SBASIS",
       retrain_config("M_KRR Kern_Linear 2", "LAMBDA 0\nOALGO 1\n"),
       {"M_KRR Kern_Linear 2 needs SBASIS"}},
      {"Kern_RBF 1, no SBASIS",
       retrain_config("M_KRR Kern_RBF 1", "MPARAMS 1.0\nLAMBDA 0\nOALGO 1\n"),
       {"M_KRR Kern_RBF 1 needs SBASIS"}},
  };
  for (const auto& c : cases) DYNAMIC_SECTION(c.name) {
    std::ofstream("config.train") << c.config;
    std::filesystem::remove("pot.tadah");
    const CliRun r = run_tadah({"tadah", "train", "-c", "config.train", "-o",
                                "pot.tadah"});
    const std::string rep = reported(r);
    INFO("status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    for (const std::string& what : c.named) CHECK(contains(rep, what));
    CHECK_FALSE(contains(rep, "non-linear"));
    CHECK_FALSE(contains(rep, "Key not found"));
    CHECK_FALSE(std::filesystem::exists("pot.tadah"));
  }
}

/* Pins: with BIAS true and no ATOM line, `tadah train` takes ATOM from the
 * datasets (the eight Nb-Ta cells of the [bias] cases: Nb Ta, two intercept
 * vectors) and checks the SBASIS of M_KRR Kern_Linear 2 against them before
 * the fit: SBASIS 2 is refused, exit 1 without a potential, naming "M_KRR
 * Kern_Linear 2: SBASIS 2", "at least 3 basis vectors" and "the 2
 * intercept vectors", without the word "non-linear"; SBASIS 3 trains (exit
 * 0, 5 WEIGHTS: DSIZE, 2 intercepts and the 3 D2_BP weights).
 * Before the fix: SBASIS 2 was refused by the fit's basis draw, without the
 * model, as "SBASIS 2: a non-linear M_KRR model needs at least 3 basis
 * vectors"; SBASIS 3 trained as well (a pin). */
TEST_CASE("Subcommand: train refuses the SBASIS of M_KRR Kern_Linear 2 "
          "against the species of the datasets with BIAS true and no ATOM",
          "[TadahCLI][train][krr][bias][potential-content]") {
  ScratchCwd cwd("krr2_sbasis_no_atom");
  std::ofstream("db.tadah") << nbta_dataset(false);
  const std::string keys =
      with_line(with_line(kNbTaKeys, "ATOM", ""), "MODEL",
                "MODEL M_KRR Kern_Linear 2");
  {
    std::ofstream("config") << "DBFILE db.tadah\n" << keys << "SBASIS 2\n";
    const CliRun r = run_tadah({"tadah", "train", "-c", "config", "-o",
                                "pot.tadah"});
    const std::string rep = reported(r);
    INFO("SBASIS 2: status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    CHECK(contains(rep, "M_KRR Kern_Linear 2: SBASIS 2"));
    CHECK(contains(rep, "at least 3 basis vectors"));
    CHECK(contains(rep, "the 2 intercept vectors"));
    CHECK_FALSE(contains(rep, "non-linear"));
    CHECK_FALSE(std::filesystem::exists("pot.tadah"));
  }
  std::ofstream("config") << "DBFILE db.tadah\n" << keys << "SBASIS 3\n";
  const CliRun r = run_tadah({"tadah", "train", "-c", "config", "-o",
                              "pot.tadah"});
  INFO("SBASIS 3: status " << r.status << "\n" << reported(r));
  REQUIRE(r.status == 0);
  CHECK(line_tokens(slurp("pot.tadah"), "WEIGHTS").size() == 6u);  // key + 5
}

#ifdef TADAH_ENABLE_NFP
/* Pins: two runs of `tadah hpo` in one process (run_tadah runs tadah in
 * this process; one evaluation each, at the CONFIG seed of OPTIM CGRID2B
 * (1); PC_ERMSE and PC_FRMSE), A with LAMBDA 1e-4 and FSIGMA 1, then B with
 * LAMBDA 0.1 and FSIGMA 0.1: both exit 0, and the best_pot.tadah the second
 * run writes is B's: LAMBDA 0.1, FSIGMA 0.1, and the weights of `tadah
 * train -c` with B's configuration (1e-10 relative: the same fit at the
 * seed).
 * Before the fix: the second run evaluated the first run's model (the
 * thread's trainer of A, with only CGRID2B rewritten): best_pot.tadah had
 * LAMBDA 1e-4, A's error scale (FSIGMA 1) and A's weights. (The two runs
 * were two sections of one input of one tadah command.) */
TEST_CASE("Subcommand: a second hpo run in the process evaluates its own "
          "configuration",
          "[TadahCLI][hpo][retrain]") {
  ScratchCwd cwd("two_hpo_tasks");
  std::ofstream("db.tadah") << retrain_dataset();
  { std::ofstream os("config.val"); os << "DBFILE db.tadah\n"; }
  std::ofstream("configA.tadah")
      << retrain_config("M_BLR BF_Linear", "LAMBDA 1e-4\nOALGO 4\nFSIGMA 1\n");
  std::ofstream("configB.tadah")
      << retrain_config("M_BLR BF_Linear", "LAMBDA 0.1\nOALGO 4\nFSIGMA 0.1\n");
  {
    std::ofstream os("config.hpo");
    os << "OPTIMIZER\nLIB NLOPT\nALGO LN_SBPLX\nMAXEVAL 1\nENDOPTIMIZER\n"
          "LOSS L2\nPC_ERMSE 0 1\nPC_FRMSE 0 1\n"
          "OPTIM CGRID2B (1) 2.3 2.7\n";
  }
  for (const char* config : {"configA.tadah", "configB.tadah"}) {
    const CliRun r = run_tadah({"tadah", "hpo", "-c", config, "--hpotarget",
                                "config.hpo", "--validation", "config.val"});
    INFO(config << ": status " << r.status << "\n" << reported(r));
    REQUIRE(r.status == 0);
  }
  REQUIRE(std::filesystem::exists("best_pot.tadah"));
  const std::string best = slurp("best_pot.tadah");
  INFO("best_pot.tadah:\n" << best);
  const auto lambda = line_tokens(best, "LAMBDA");
  const auto fsigma = line_tokens(best, "FSIGMA");
  REQUIRE(lambda.size() == 2);
  REQUIRE(fsigma.size() >= 2);
  CHECK(std::strtod(lambda[1].c_str(), nullptr) == 0.1);
  CHECK(std::strtod(fsigma[1].c_str(), nullptr) == 0.1);

  const CliRun t = run_tadah({"tadah", "train", "-c", "configB.tadah", "-o", "potB.tadah"});
  INFO("train B: status " << t.status << "\n" << reported(t));
  REQUIRE(t.status == 0);
  const auto wb = line_tokens(slurp("potB.tadah"), "WEIGHTS");
  const auto wh = line_tokens(best, "WEIGHTS");
  REQUIRE(wb.size() == 1 + 4);
  REQUIRE(wh.size() == wb.size());
  for (std::size_t i = 1; i < wb.size(); ++i) {
    const double b = std::strtod(wb[i].c_str(), nullptr);
    const double h = std::strtod(wh[i].c_str(), nullptr);
    INFO("weight " << i << ": hpo " << wh[i] << ", train " << wb[i]);
    CHECK(h == Approx(b).epsilon(1e-10));
  }
}
#endif  // TADAH_ENABLE_NFP

/* ---- [uncertainty]: uncertainty output is removed ----
 *
 * The errors of Tadah! 1.0.0 to 1.5.0 were not standard deviations, and
 * no calibrated estimate replaces them: tadah predict --error and tadah
 * properties pairwise --error are retired options, tadah train
 * --uncertainty the option of the retired key UNCERTAINTY, and ERROR (the
 * key of --error) and WCOV and WNOISE, which a development build of
 * Tadah! 1.6.0 wrote for an OALGO 3 fit, retired keys. The predictions and
 * the fit are unaffected. */

namespace {

const std::string kRemoved =
    "Uncertainty output is removed: the errors of Tadah! 1.0.0 to 1.5.0 "
    "were not standard deviations, and no calibrated estimate replaces "
    "them.";

/* Two Ta dimers in 5 A cells (the [weight0] entries). */
std::string two_dimers() {
  return weighted_entry("S1", "1 1 1", "1.40 1.30 1.20", "0.1", "0.001") +
         weighted_entry("S2", "1 1 1", "1.60 1.25 1.30", "-0.1", "0.002");
}

}  // namespace

/* Pins: `tadah predict --error` and `tadah properties pairwise --error`
 * exit 1 before writing anything, with the reason: "--error of tadah
 * predict is retired: tadah predict --error wrote an error computed from
 * SIGMA beside every predicted value (some of the errors negative).
 * Uncertainty output is removed: the errors of Tadah! 1.0.0 to 1.5.0 were
 * not standard deviations, and no calibrated estimate replaces them.
 * Replaced by: nothing. Deleting the option converts it: ...", and
 * likewise for tadah properties pairwise; without --error both exit 0.
 * Before the fix: both exited 0 and wrote errors (energy.pred with a
 * fourth column and error.pred; the curve's error column); then the
 * reasons said that they gave standard deviations, too narrow for many
 * structures (what a development build gave, not a release); then that
 * uncertainty output was not part of Tadah! 1.6.0, replaced by "nothing in
 * Tadah! 1.6.0". */
TEST_CASE("Subcommand: predict --error and properties pairwise --error are "
          "refused with the reason",
          "[TadahCLI][predict][uncertainty]") {
  ScratchCwd cwd("error_retired");
  std::ofstream("pot.tadah") << kPairPotential;
  std::ofstream("d.tadah") << two_dimers();
  const CliRun e = run_tadah({"tadah", "predict", "-p", "pot.tadah", "-d",
                              "d.tadah", "--error"});
  const std::string re = reported(e);
  INFO("predict --error: status " << e.status << "\n" << re);
  CHECK(e.status == 1);
  CHECK(contains(re, "--error of tadah predict is retired: tadah predict "
                     "--error wrote an error computed from SIGMA beside "
                     "every predicted value"));
  CHECK(contains(re, kRemoved));
  CHECK(contains(re, "Replaced by: nothing. Deleting the option converts "
                     "it"));
  CHECK_FALSE(std::filesystem::exists("energy.pred"));
  CHECK_FALSE(std::filesystem::exists("error.pred"));
  const CliRun p = run_tadah({"tadah", "properties", "pairwise", "-p",
                              "pot.tadah", "-r", "2.0", "5.0", "4",
                              "--atompair", "Ta", "Ta", "-o", "pw.dat",
                              "--error"});
  const std::string rp = reported(p);
  INFO("properties pairwise --error: status " << p.status << "\n" << rp);
  CHECK(p.status == 1);
  CHECK(contains(rp, "--error of tadah properties pairwise is retired: "
                     "tadah properties pairwise --error wrote an error "
                     "computed from SIGMA beside every value of the energy "
                     "curve"));
  CHECK(contains(rp, kRemoved));
  CHECK_FALSE(std::filesystem::exists("pw.dat"));

  const CliRun ok = run_tadah({"tadah", "predict", "-p", "pot.tadah", "-d",
                               "d.tadah"});
  INFO("predict: status " << ok.status << "\n" << reported(ok));
  CHECK(ok.status == 0);
  CHECK(std::filesystem::exists("energy.pred"));
  const CliRun okp = run_tadah({"tadah", "properties", "pairwise", "-p",
                                "pot.tadah", "-r", "2.0", "5.0", "4",
                                "--atompair", "Ta", "Ta", "-o", "pw.dat"});
  INFO("properties pairwise: status " << okp.status << "\n" << reported(okp));
  CHECK(okp.status == 0);
  CHECK(std::filesystem::exists("pw.dat"));
}

/* Pins: `tadah train --uncertainty` exits 1 before training, with the
 * reason ("--uncertainty: UNCERTAINTY is a retired key: tadah train
 * --uncertainty wrote ... Uncertainty output is removed: ..."), and
 * writes neither pot.tadah nor weights.tadah; without it the OALGO 3
 * configuration trains (exit 0) and its potential has no WCOV and no
 * WNOISE line.
 * Before the fix: it trained and wrote weights.tadah, and the potential had
 * WCOV and WNOISE. */
TEST_CASE("Subcommand: train --uncertainty is refused with the reason",
          "[TadahCLI][train][uncertainty]") {
  ScratchCwd cwd("uncertainty_retired");
  std::ofstream("db.tadah") << retrain_dataset();
  std::ofstream("config.train")
      << retrain_config("M_BLR BF_Linear", "LAMBDA 1e-3\nOALGO 3\n");
  const CliRun t = run_tadah({"tadah", "train", "-c", "config.train", "-o",
                              "pot.tadah", "--uncertainty"});
  const std::string rep = reported(t);
  INFO("train --uncertainty: status " << t.status << "\n" << rep);
  CHECK(t.status == 1);
  CHECK(contains(rep, "--uncertainty: UNCERTAINTY is a retired key: tadah "
                      "train --uncertainty wrote weights.tadah, each "
                      "coefficient of the fit beside a diagonal entry of "
                      "SIGMA."));
  CHECK(contains(rep, kRemoved));
  CHECK_FALSE(std::filesystem::exists("pot.tadah"));
  CHECK_FALSE(std::filesystem::exists("weights.tadah"));
  const CliRun ok = run_tadah({"tadah", "train", "-c", "config.train", "-o",
                               "pot.tadah"});
  INFO("train: status " << ok.status << "\n" << reported(ok));
  REQUIRE(ok.status == 0);
  const std::string pot = slurp("pot.tadah");
  CHECK(line_tokens(pot, "WCOV").empty());
  CHECK(line_tokens(pot, "WNOISE").empty());
  CHECK_FALSE(std::filesystem::exists("weights.tadah"));
}

/* Pins: an ERROR line (the key of --error of tadah predict) in the
 * configuration of `tadah train` (the OALGO 3 configuration, ERROR true on
 * line 19) is refused (exit 1), naming the line marked as converted by
 * deleting it ("ERROR true (line 19 of config.train; converted by deleting
 * the line)") and the reason, and no potential is written; the potential
 * of the configuration without it, with an ERROR line added, predicts what
 * it predicts without it, with one notice ("Retired key ERROR (line N of
 * err.tadah) is ignored"); a configuration file that includes the
 * configuration with CONFIG and has an UNCERTAINTY line, or an ERROR line
 * above its CONFIG line, is refused alike, with the reason, naming its
 * line.
 * Before the fix: the configuration trained and its potential had the line
 * "ERROR true", which nothing read; the potential's line gave no notice;
 * an UNCERTAINTY line was refused without the mark and the reason. */
TEST_CASE("Subcommand: train refuses an ERROR or UNCERTAINTY line of its "
          "configuration with the reason",
          "[TadahCLI][train][uncertainty]") {
  ScratchCwd cwd("error_key_retired");
  std::ofstream("db.tadah") << retrain_dataset();
  const std::string config =
      retrain_config("M_BLR BF_Linear", "LAMBDA 1e-3\nOALGO 3\n");
  std::ofstream("config.train") << config << "ERROR true\n";
  const CliRun t = run_tadah({"tadah", "train", "-c", "config.train", "-o",
                              "e.tadah"});
  const std::string rep = reported(t);
  INFO("train: status " << t.status << "\n" << rep);
  CHECK(t.status == 1);
  CHECK(contains(rep, "ERROR true (line 19 of config.train; converted by "
                      "deleting the line)"));
  CHECK(contains(rep, "ERROR: ERROR, the key of --error, asked tadah "
                      "predict and tadah properties pairwise for errors "
                      "computed from SIGMA."));
  CHECK(contains(rep, kRemoved));
  CHECK_FALSE(std::filesystem::exists("e.tadah"));

  std::ofstream("plain.train") << config;
  const CliRun ok = run_tadah({"tadah", "train", "-c", "plain.train", "-o",
                               "plain.tadah"});
  INFO("train: status " << ok.status << "\n" << reported(ok));
  REQUIRE(ok.status == 0);
  CHECK(line_tokens(slurp("plain.tadah"), "ERROR").empty());
  std::ofstream("err.tadah") << slurp("plain.tadah") << "ERROR true\n";
  const CliRun p0 = run_tadah({"tadah", "predict", "-p", "plain.tadah", "-d",
                               "db.tadah", "-o", "plain.pred"});
  REQUIRE(p0.status == 0);
  const CliRun p = run_tadah({"tadah", "predict", "-p", "err.tadah", "-d",
                              "db.tadah", "-o", "err.pred"});
  const std::string rp = reported(p);
  INFO("predict: status " << p.status << "\n" << rp);
  CHECK(p.status == 0);
  CHECK(contains(rp, "Retired key ERROR (line "));
  CHECK(contains(rp, " of err.tadah) is ignored"));
  CHECK(slurp("err.pred") == slurp("plain.pred"));

  std::ofstream("u.train") << "CONFIG plain.train\n"
                              "OUTFILE t.tadah\nUNCERTAINTY true\n";
  const CliRun tf = run_tadah({"tadah", "train", "-c", "u.train"});
  const std::string rtf = reported(tf);
  INFO("UNCERTAINTY: status " << tf.status << "\n" << rtf);
  CHECK(tf.status == 1);
  CHECK(contains(rtf, "UNCERTAINTY true (line 3 of u.train; converted by "
                      "deleting the line)"));
  CHECK(contains(rtf, "UNCERTAINTY: tadah train --uncertainty wrote "
                      "weights.tadah"));
  CHECK(contains(rtf, kRemoved));
  CHECK_FALSE(std::filesystem::exists("t.tadah"));

  std::ofstream("g.train") << "ERROR true\nCONFIG plain.train\n"
                              "OUTFILE g.tadah\n";
  const CliRun g = run_tadah({"tadah", "train", "-c", "g.train"});
  const std::string rg = reported(g);
  INFO("ERROR above CONFIG: status " << g.status << "\n" << rg);
  CHECK(g.status == 1);
  CHECK(contains(rg, "ERROR true (line 1 of g.train; converted by deleting "
                     "the line)"));
  CHECK(contains(rg, kRemoved));
  CHECK_FALSE(std::filesystem::exists("g.tadah"));
}

/* Pins: `tadah explain` of predict.error, properties.pairwise.error,
 * train.uncertainty, UNCERTAINTY, ERROR, WCOV and WNOISE exits 0 and gives
 * the reason, after "RETIRED: " (that uncertainty output is removed, the
 * errors of Tadah! 1.0.0 to 1.5.0 not being standard deviations and no
 * calibrated estimate replacing them), the line "REPLACED BY: nothing" and
 * the conversion.
 * Before the fix: each explained a working option or key (ERROR the key
 * of a configuration); then the reasons said that the error bars were too
 * narrow for many structures; then that uncertainty output was not part
 * of Tadah! 1.6.0, replaced by "nothing in Tadah! 1.6.0". */
TEST_CASE("Subcommand: explain gives why --error, --uncertainty, ERROR, "
          "WCOV and WNOISE are retired",
          "[TadahCLI][explain][uncertainty]") {
  ScratchCwd cwd("explain_uncertainty");
  for (const char* name : {"predict.error", "properties.pairwise.error",
                           "train.uncertainty", "UNCERTAINTY", "ERROR",
                           "WCOV", "WNOISE"}) {
    const CliRun x = run_tadah({"tadah", "explain", name});
    INFO("explain " << name << ": status " << x.status << "\n"
                    << reported(x));
    CHECK(x.status == 0);
    CHECK(contains(x.out, "RETIRED: "));
    CHECK(contains(x.out, kRemoved));
    CHECK(contains(x.out, "\nREPLACED BY: nothing\n"));
    CHECK(contains(x.out, "CONVERSION: Deleting the"));
  }
}

/* Pins: a potential with the WCOV and WNOISE lines a development build of
 * Tadah! 1.6.0 wrote for an OALGO 3 fit (the [weight0] pair potential with
 * OALGO 3 on line 18, WCOV of N = 4 on line 19 and WNOISE E 1 F 1 S 1 on
 * line 20) predicts what the potential without them predicts (exit 0,
 * energy.pred the same), with one notice "Retired keys WCOV (line 19 of
 * pot.tadah) and WNOISE (line 20 of pot.tadah) are ignored"; `tadah train
 * -c` of it with a DBFILE line exits 1 naming the two lines, WCOV by its
 * first values, each marked as converted by deleting it ("WCOV 4 1 0 and
 * 14 more values (line 19 of retrain.train; converted by deleting the
 * line)"), and writes no potential.
 * Before the fix: no notice, and the retrain wrote the WCOV and WNOISE of
 * its own fit. */
TEST_CASE("Subcommand: a potential with WCOV and WNOISE predicts with one "
          "notice, and a retrain from it is refused",
          "[TadahCLI][predict][train][uncertainty]") {
  ScratchCwd cwd("wcov_retired");
  const std::string with_covariance =
      std::string(kPairPotential) + "OALGO     3\n" +
      "WCOV     4     1 0 0 0     0 1 0 0     0 0 1 0     0 0 0 1\n" +
      "WNOISE     E 1     F 1     S 1\n";
  std::ofstream("pot.tadah") << with_covariance;
  std::ofstream("plain.tadah") << kPairPotential << "OALGO     3\n";
  std::ofstream("d.tadah") << two_dimers();
  const CliRun plain = run_tadah({"tadah", "predict", "-p", "plain.tadah",
                                  "-d", "d.tadah", "-o", "plain.pred"});
  REQUIRE(plain.status == 0);
  const CliRun p = run_tadah({"tadah", "predict", "-p", "pot.tadah", "-d",
                              "d.tadah", "-o", "pot.pred"});
  const std::string rp = reported(p);
  INFO("predict: status " << p.status << "\n" << rp);
  CHECK(p.status == 0);
  CHECK(slurp("pot.pred") == slurp("plain.pred"));
  CHECK(contains(rp, "Retired keys WCOV (line 19 of pot.tadah) and WNOISE "
                     "(line 20 of pot.tadah) are ignored"));
  std::ofstream("db.tadah") << retrain_dataset();
  std::ofstream("retrain.train")
      << as_training_config(with_covariance, "db.tadah");
  const CliRun t = run_tadah({"tadah", "train", "-c", "retrain.train", "-o",
                              "re.tadah"});
  const std::string rt = reported(t);
  INFO("train: status " << t.status << "\n" << rt);
  CHECK(t.status == 1);
  CHECK(contains(rt, "WCOV 4 1 0 and 14 more values (line 19 of "
                     "retrain.train; converted by deleting the line)"));
  CHECK(contains(rt, "WNOISE E 1 F 1 S 1 (line 20 of retrain.train; "
                     "converted by deleting the line)"));
  CHECK_FALSE(std::filesystem::exists("re.tadah"));
}

/* ---- the pairwise blocks, and SIGMA ------------------------------------
 *
 * SIGMA, what Tadah! 1.0.0 to 1.5.0 wrote for an OALGO 3 fit, is a
 * deprecated key. Uncertainty output is removed (the [uncertainty] cases
 * above). */

namespace {

/* The data rows of each block ("# <title>" then a header line) of a
 * `tadah properties pairwise` output file. */
std::map<std::string, std::vector<std::vector<double>>> pairwise_blocks(
    const std::string& file) {
  std::map<std::string, std::vector<std::vector<double>>> blocks;
  std::ifstream in(file);
  std::string title;
  for (std::string line; std::getline(in, line);) {
    if (line.rfind("# ", 0) == 0) {
      if (line.find("r (") == std::string::npos) title = line.substr(2);
      continue;
    }
    std::istringstream ls(line);
    std::vector<double> r;
    for (double v; ls >> v;) r.push_back(v);
    if (!r.empty()) blocks[title].push_back(r);
  }
  return blocks;
}

/* A D2_BP pair term (4 functions, 4.5 A) and a DM_mBlipEnv embedding
 * (5 functions, 5 A) for Ta, M_BLR BF_Linear, OALGO 3, LAMBDA 1e-3. */
std::string twobody_manybody_config() {
  return replace_key_line(
             retrain_config("M_BLR BF_Linear", "LAMBDA 1e-3\nOALGO 3\n"),
             "INITMB", "INITMB true") +
         "TYPEMB DM_mBlipEnv 0 1 1 5 5 Ta Ta\nRCUTMB 5.0\nRCTYPEMB Cut_Cos\n"
         "CGRIDMB 2.88\nSGRIDMB 8.0\nCEMBFUNC 0.5 0.75 1.0 1.25 1.5\n"
         "SEMBFUNC 2.0 2.0 2.0 2.0 2.0\nRCUTENV 0.6\n";
}

}  // namespace

/* Pins: a potential with a SIGMA line (an OALGO 3 potential of Tadah!
 * 1.0.0 to 1.5.0) predicts: `tadah predict` exits 0 and reports the
 * deprecation notice of SIGMA once ("Deprecated key SIGMA is ignored",
 * naming the transposed formula).
 * Before the deprecation: no notice. (This case, then named "Subcommand:
 * predict refuses --error for a potential with SIGMA only", also ran
 * `tadah predict --error`, which every potential is refused now.) */
TEST_CASE("Subcommand: predict of a potential with SIGMA gives its "
          "deprecation notice",
          "[TadahCLI][predict][sigma]") {
  ScratchCwd cwd("legacy_sigma");
  std::ofstream("pot.tadah")
      << kPairPotential << "OALGO     3\n"
      << "SIGMA     4     1 0 0 0     0 1 0 0     0 0 1 0     0 0 0 1\n";
  std::ofstream("d.tadah")
      << weighted_entry("S1", "1 1 1", "1.40 1.30 1.20", "0.1", "0.001")
      << weighted_entry("S2", "1 1 1", "1.60 1.25 1.30", "-0.1", "0.002");
  const std::size_t notices =
      tadah::core::ignored_key_reported("SIGMA") ? 0 : 1;
  const CliRun p = run_tadah({"tadah", "predict", "-p", "pot.tadah", "-d", "d.tadah"});
  const std::string rp = reported(p);
  INFO("predict: status " << p.status << "\n" << rp);
  CHECK(p.status == 0);
  std::size_t n = 0;
  for (std::size_t at = rp.find("Deprecated key SIGMA is ignored");
       at != std::string::npos;
       at = rp.find("Deprecated key SIGMA is ignored", at + 1))
    ++n;
  CHECK(n == notices);
  if (notices) CHECK(contains(rp, "transposed"));
}

/* Pins: `tadah properties pairwise` with a two-body plus many-body
 * potential (D2_BP 4 + DM_mBlipEnv 5, BIAS false) reports the energy per
 * atom in all three blocks ("Total potential", "Two-body potential",
 * "Many-body potential", headed "Energy eV/atom"): at every distance the
 * two-body and many-body energies add up to the total (1e-10 relative).
 * Before the fix: the two-body and many-body blocks gave the energy of the
 * dimer (twice the per-atom value), so they added up to twice the total. */
TEST_CASE("Subcommand: properties pairwise gives every block's energy per "
          "atom",
          "[TadahCLI][properties]") {
  ScratchCwd cwd("pairwise_blocks");
  std::ofstream("db.tadah") << retrain_dataset();
  std::ofstream("config.train") << twobody_manybody_config();
  const CliRun t = run_tadah({"tadah", "train", "-c", "config.train", "-o",
                              "pot.tadah"});
  INFO("train: status " << t.status << "\n" << reported(t));
  REQUIRE(t.status == 0);
  const CliRun p = run_tadah({"tadah", "properties", "pairwise", "-p",
                              "pot.tadah", "--range", "2.0", "6.0", "9",
                              "--atompair", "Ta", "Ta", "-o", "pw.dat"});
  INFO("pairwise: status " << p.status << "\n" << reported(p));
  REQUIRE(p.status == 0);
  auto blocks = pairwise_blocks("pw.dat");
  const auto& tot = blocks["Total potential"];
  const auto& b2 = blocks["Two-body potential"];
  const auto& mb = blocks["Many-body potential"];
  REQUIRE(tot.size() == 9u);
  REQUIRE(b2.size() == 9u);
  REQUIRE(mb.size() == 9u);
  for (std::size_t j = 0; j < 9; ++j) {
    INFO("r = " << tot[j][0]);
    CHECK(b2[j][1] + mb[j][1] == Approx(tot[j][1]).epsilon(1e-10).margin(1e-14));
  }
}

/* ---- a potential as a configuration; validation files -------------------
 *
 * A potential's key lines are the model's settings and its fit, neither
 * the training data (no DBFILE line) nor the inputs of the run that wrote
 * it (no HPOTARGET, VALIDATION or VERBOSE line); its training record, a
 * comment block no loader of a potential reads, names them for the user's
 * record ([potential record] below). */

namespace {

/* The advice every refusal of a configuration without a training DBFILE
 * ends with. */
const char* const kNoDatasetsAdvice =
    "A potential has no DBFILE line (no potential tadah writes one; its "
    "training record, the comment block above its key lines, lists the "
    "datasets its weights were fitted to in a '# DBFILE' line when there "
    "were any: a refit has none, and a potential of an earlier version has "
    "no record): a retrain from one takes a copy of it with a DBFILE line "
    "added; a potential of tadah refit or refit --edit also pins every "
    "weight with its FIXINDEX and FIXWEIGHT lines, the seed of a tadah hpo "
    "coefficient search, and the fit changes the weights when both are "
    "deleted from the copy (the Training page of the documentation, "
    "Retraining from a potential).";

}  // namespace

/* Pins: `tadah train -c pot.tadah`, with the potential tadah train wrote
 * from the three Ta dimers (M_BLR BF_Linear and M_KRR Kern_Linear; it has
 * no DBFILE line), exits 1 without writing re.tadah, saying "The training
 * configuration has no DBFILE: tadah train fits the datasets it lists." and
 * the advice (kNoDatasetsAdvice: "A potential has no DBFILE line ... a
 * retrain from one takes a copy of it with a DBFILE line added; a potential
 * of tadah refit or refit --edit also pins every weight ..."), and no "Key
 * not found"; with the DBFILE line added it trains (exit 0; a pin of the
 * documented route).
 * Before the fix of the advice's wording: "... to train from one, add a
 * DBFILE line to a copy of it; ..., so delete them from the copy for the
 * fit to change the weights ...", instructions to the reader.
 * Before the fix: exit 1 with "Key not found in the Context object:
 * DBFILE" (and the Logger's "Context::operator() -> Key not found:
 * DBFILE"), naming neither potentials nor the retrain.
 * Before the advice named the pins: it ended "add a DBFILE line to a copy of
 * it (the Training page ...).", which a refit potential so retrained followed
 * to a fit of nothing (the next case).
 * Before the training record: the advice began "A potential records no
 * DBFILE (no potential tadah writes has one): ...".
 * Before the advice said when the record lists a DBFILE: it said "the
 * DBFILE of the run that wrote it is in its training record, a comment
 * block tadah does not read", which a refit potential, an hpo run's without
 * training datasets and one of an earlier version contradict: the advice
 * checks fail. */
TEST_CASE("Subcommand: train -c of a potential without a DBFILE line is "
          "refused, pointing to the retrain route",
          "[TadahCLI][train][retrain][potential-content]") {
  for (const char* model : {"M_BLR BF_Linear", "M_KRR Kern_Linear"})
    DYNAMIC_SECTION(model) {
      ScratchCwd cwd("retrain_no_dbfile");
      std::ofstream("db.tadah") << retrain_dataset();
      std::ofstream("config")
          << retrain_config(model, "LAMBDA 0.1\nOALGO 1\n");
      {
        const CliRun t = run_tadah({"tadah", "train", "-c", "config", "-o",
                                    "pot.tadah"});
        INFO("train: status " << t.status << "\n" << reported(t));
        REQUIRE(t.status == 0);
      }
      REQUIRE(line_tokens(slurp("pot.tadah"), "DBFILE").empty());
      const CliRun r = run_tadah({"tadah", "train", "-c", "pot.tadah", "-o",
                                  "re.tadah"});
      const std::string rep = reported(r);
      INFO("train -c pot.tadah: status " << r.status << "\n" << rep);
      CHECK(r.status == 1);
      CHECK(contains(rep, "The training configuration has no DBFILE: tadah "
                          "train fits the datasets it lists."));
      CHECK(contains(rep, kNoDatasetsAdvice));
      CHECK_FALSE(contains(rep, "Key not found"));
      CHECK_FALSE(std::filesystem::exists("re.tadah"));

      std::ofstream("re_cfg.tadah")
          << as_training_config(slurp("pot.tadah"), "db.tadah");
      const CliRun ok = run_tadah({"tadah", "train", "-c", "re_cfg.tadah",
                                   "-o", "re.tadah"});
      INFO("with the DBFILE line: status " << ok.status << "\n"
                                            << reported(ok));
      CHECK(ok.status == 0);
      CHECK(std::filesystem::exists("re.tadah"));
    }
}

/* Pins: `tadah refit --edit` of a potential with the UNCERTAINTY line
 * tadah train 1.0.0 to 1.5.0 wrote into every potential (the outputs of a dense
 * refit of the small Ta setfl, with "UNCERTAINTY true" appended, as
 * old.tadah) loads it with the notice of a retired key ("Retired key
 * UNCERTAINTY (line N of old.tadah) is ignored") and writes old_edited.tadah with the line as it
 * is, and so does an edit of that potential: an edit keeps the retired
 * lines of its input. A retrain of the edited potential (a DBFILE line
 * added) is refused naming the line; with the line deleted (its
 * conversion) it trains and names no UNCERTAINTY.
 * Before the fix: UNCERTAINTY was not retired: the edits wrote no
 * UNCERTAINTY line (the input's line went into the training record) and the
 * retrain of the edited potential exited 0. */
TEST_CASE("Subcommand: refit --edit keeps the UNCERTAINTY line of a "
          "potential as a retired line",
          "[TadahCLI][exit-status][refit][potential-content][uncertainty]") {
  ScratchCwd cwd("refit_edit_uncertainty");
  { std::ofstream os("eam_small.alloy"); os << small_ta_setfl(); }
  std::ofstream("config.refit")
      << "EAM eam_small.alloy\nRHOFORM SIGNED\nREFIT_MODE dense\n"
         "REFIT_BASIS Blip\nREFIT_LEVEL coarse\nREFIT_MAX_ESCALATIONS 0\n";
  {
    const CliRun fit = run_tadah({"tadah", "refit", "-c", "config.refit"});
    INFO("refit: status " << fit.status << "\n" << reported(fit));
    REQUIRE(fit.status == 0);
  }
  std::ofstream("old.tadah") << slurp("pot.tadah") << "UNCERTAINTY true\n";
  {
    const CliRun r = run_tadah({"tadah", "refit", "--edit", "old.tadah",
                                "--func", "phi", "--add", "2.5", "-o",
                                "old_edited.tadah"});
    const std::string rep = reported(r);
    INFO("--edit old.tadah: status " << r.status << "\n" << rep);
    REQUIRE(r.status == 0);
    CHECK(contains(rep, "Retired key UNCERTAINTY (line "));
    CHECK(contains(rep, " of old.tadah) is ignored"));
  }
  const std::string edited = slurp("old_edited.tadah");
  INFO(edited);
  CHECK(line_tokens(edited, "UNCERTAINTY") ==
        std::vector<std::string>{"UNCERTAINTY", "true"});
  {
    const CliRun r = run_tadah({"tadah", "refit", "--edit",
                                "old_edited.tadah", "--func", "phi", "--add",
                                "3.0", "-o", "twice.tadah"});
    INFO("--edit old_edited.tadah: status " << r.status << "\n"
                                            << reported(r));
    REQUIRE(r.status == 0);
    CHECK(line_tokens(slurp("twice.tadah"), "UNCERTAINTY") ==
          std::vector<std::string>{"UNCERTAINTY", "true"});
  }
  std::ofstream("db.tadah") << bcc_ta_entry("B1", 3.25, -23.1)
                            << bcc_ta_entry("B2", 3.30, -23.4)
                            << bcc_ta_entry("B3", 3.36, -23.2);
  std::ofstream("retrain.train") << as_training_config(edited, "db.tadah");
  const CliRun t = run_tadah({"tadah", "train", "-c", "retrain.train", "-o",
                              "retrained.tadah"});
  const std::string rep = reported(t);
  INFO("train: status " << t.status << "\n" << rep);
  CHECK(t.status == 1);
  CHECK(contains(rep, "UNCERTAINTY true (line "));
  CHECK(contains(rep, "retired"));
  CHECK_FALSE(std::filesystem::exists("retrained.tadah"));
  std::ofstream("converted.train")
      << as_training_config(with_line(edited, "UNCERTAINTY", ""), "db.tadah");
  const CliRun c = run_tadah({"tadah", "train", "-c", "converted.train",
                              "-o", "retrained.tadah"});
  const std::string crep = reported(c);
  INFO("converted: status " << c.status << "\n" << crep);
  CHECK(c.status == 0);
  CHECK_FALSE(contains(crep, "UNCERTAINTY"));
}

/* Pins: the documented retrain of a potential that tadah refit writes --
 * its copy with a DBFILE line -- fits no weight, and tadah train says so: a
 * dense Blip refit of the small Ta setfl (REFIT_LEVEL coarse) writes
 * pot.tadah with FIXINDEX 1-84 and FIXWEIGHT for every weight (the seed of
 * a tadah hpo coefficient search) and no DBFILE; `tadah train -c
 * retrain.train` (pot.tadah plus DBFILE db.tadah, three bcc Ta cells)
 * exits 0 with the WARNING "tadah train: FIXINDEX and FIXWEIGHT pin every
 * weight (all 84 of them), so the fit changes none: the WEIGHTS of the
 * potential written are the FIXWEIGHT values. tadah refit and refit --edit
 * write FIXINDEX and FIXWEIGHT for every weight of their potentials, ...:
 * the weights are fitted to the DBFILE data when both lines are deleted
 * from the training configuration (the Training page of the documentation,
 * Retraining from a potential).", once, and pot2.tadah has the WEIGHTS of
 * pot.tadah.
 * Before the fix of its wording the WARNING ended "...: to fit the weights
 * to the DBFILE data, delete both lines from the training configuration
 * ...", an instruction.
 * Before the fix: exit 0 and no WARNING about the pins: the potential
 * written had the input weights and nothing said that none was fitted
 * (171 weights, the same result). */
TEST_CASE("Subcommand: train of a refit potential with a DBFILE line warns "
          "that its pins leave no weight to fit",
          "[TadahCLI][train][retrain][refit][potential-content]") {
  ScratchCwd cwd("retrain_refit_pins");
  { std::ofstream os("eam_small.alloy"); os << small_ta_setfl(); }
  std::ofstream("config.refit")
      << "EAM eam_small.alloy\nRHOFORM SIGNED\nREFIT_MODE dense\n"
         "REFIT_BASIS Blip\nREFIT_LEVEL coarse\nREFIT_MAX_ESCALATIONS 0\n";
  {
    const CliRun r = run_tadah({"tadah", "refit", "-c", "config.refit"});
    INFO("refit: status " << r.status << "\n" << reported(r));
    REQUIRE(r.status == 0);
  }
  const std::string pot = slurp("pot.tadah");
  REQUIRE(line_tokens(pot, "FIXINDEX") ==
          std::vector<std::string>{"FIXINDEX", "1-84"});
  REQUIRE(line_tokens(pot, "FIXWEIGHT").size() == 85u);
  REQUIRE(line_tokens(pot, "DBFILE").empty());
  std::ofstream("db.tadah") << bcc_ta_entry("B1", 3.25, -23.1)
                            << bcc_ta_entry("B2", 3.30, -23.4)
                            << bcc_ta_entry("B3", 3.36, -23.2);
  std::ofstream("retrain.train") << as_training_config(pot, "db.tadah");
  const CliRun t = run_tadah({"tadah", "train", "-c", "retrain.train", "-o",
                              "pot2.tadah"});
  const std::string rep = reported(t);
  INFO("train: status " << t.status << "\n" << rep);
  CHECK(t.status == 0);
  const std::string warning =
      "tadah train: FIXINDEX and FIXWEIGHT pin every weight (all 84 of "
      "them), so the fit changes none: the WEIGHTS of the potential written "
      "are the FIXWEIGHT values. tadah refit and refit --edit write FIXINDEX "
      "and FIXWEIGHT for every weight of their potentials, the seed of a "
      "tadah hpo coefficient search, and so does a tadah hpo run that pins "
      "every weight in its best_pot.tadah: the weights are fitted to the "
      "DBFILE data when both lines are deleted from the training "
      "configuration (the Training page of the documentation, Retraining "
      "from a potential).";
  CHECK(contains(rep, warning));
  CHECK(rep.find("pin every weight") == rep.rfind("pin every weight"));
  REQUIRE(std::filesystem::exists("pot2.tadah"));
  CHECK(line_tokens(slurp("pot2.tadah"), "WEIGHTS") ==
        line_tokens(pot, "WEIGHTS"));
}

#ifdef TADAH_ENABLE_NFP
namespace {

/* Writes, in the working directory, db.tadah (the three Ta dimers),
 * val.tadah (the same three under another name: a header names the
 * validation dataset apart from the training one), config.val (DBFILE
 * val.tadah), config.tadah (M_BLR BF_Linear on db.tadah) and config.hpo
 * (one evaluation, LN_SBPLX at the CONFIG seed of OPTIM CGRID2B (1);
 * PC_ERMSE). */
void write_hpo_potential_fixture() {
  std::ofstream("db.tadah") << retrain_dataset();
  std::ofstream("val.tadah") << retrain_dataset();
  std::ofstream("config.val") << "DBFILE val.tadah\n";
  std::ofstream("config.tadah")
      << retrain_config("M_BLR BF_Linear", "LAMBDA 0.1\nOALGO 4\n");
  std::ofstream("config.hpo")
      << "OPTIMIZER\nLIB NLOPT\nALGO LN_SBPLX\nMAXEVAL 1\nENDOPTIMIZER\n"
         "LOSS L2\nPC_ERMSE 0 1\nOPTIM CGRID2B (1) 2.3 2.7\n";
}

}  // namespace

/* Pins: `tadah hpo -c pot.tadah` with the potential tadah train wrote from
 * config.tadah (no DBFILE line, no FIXINDEX) exits 1 without
 * best_pot.tadah, refusing "No training DBFILE and no FIXINDEX/FIXWEIGHT:
 * the model weights cannot be determined" and ending with the advice ("A
 * potential has no DBFILE line ... Retraining from a potential)."); the
 * potential without its ATOM line is refused "the training configuration
 * declares the element set with the ATOM key, such as ATOM Ta." (it was
 * "the ATOM key is required in the training configuration to declare the
 * element set (e.g. 'ATOM Ta')."), with the same advice.
 * Before the fix: both refusals ended with their own remedy ("... or pin
 * every weight with FIXINDEX/FIXWEIGHT.", "(e.g. 'ATOM Ta')."), without the
 * advice.
 * Before the training record: the advice began "A potential records no
 * DBFILE (no potential tadah writes has one): ...", and the case was named
 * "..., says that a potential records none".
 * Before the advice said when the record lists a DBFILE: it said "the DBFILE
 * of the run that wrote it is in its training record, a comment block tadah
 * does not read": the advice checks fail. */
TEST_CASE("Subcommand: hpo -c of a potential without a DBFILE line says "
          "that a potential has no DBFILE line",
          "[TadahCLI][hpo][retrain][potential-content]") {
  ScratchCwd cwd("hpo_pot_no_dbfile");
  write_hpo_potential_fixture();
  {
    const CliRun t = run_tadah({"tadah", "train", "-c", "config.tadah", "-o",
                                "pot.tadah"});
    INFO("train: status " << t.status << "\n" << reported(t));
    REQUIRE(t.status == 0);
  }
  struct Case {
    const char* name;
    std::string pot;
    const char* reason;
  };
  const Case cases[] = {
      {"the potential", slurp("pot.tadah"),
       "No training DBFILE and no FIXINDEX/FIXWEIGHT: the model weights "
       "cannot be determined"},
      {"the potential without ATOM", with_line(slurp("pot.tadah"), "ATOM", ""),
       "No training DBFILE: the training configuration declares the "
       "element set with the ATOM key, such as ATOM Ta. "},
  };
  for (const Case& c : cases) DYNAMIC_SECTION(c.name) {
    std::ofstream("seed.tadah") << c.pot;
    const CliRun r = run_tadah({"tadah", "hpo", "-c", "seed.tadah",
                                "--hpotarget", "config.hpo", "--validation",
                                "config.val"});
    const std::string rep = reported(r);
    INFO("status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    CHECK(contains(rep, c.reason));
    CHECK(contains(rep, kNoDatasetsAdvice));
    CHECK_FALSE(std::filesystem::exists("best_pot.tadah"));
  }
}

/* Pins: the best_pot.tadah of `tadah hpo -c config.tadah --hpotarget
 * config.hpo --validation config.val -v 1` (one evaluation) has no
 * HPOTARGET, VALIDATION or VERBOSE key line (its training record, a comment
 * block, names them: the case "Subcommand: hpo records ... in
 * best_pot.tadah's training record") and still its MODEL and WEIGHTS,
 * and its header still names the validation dataset: the line after the
 * header's "#   Validation " line is "#   [1] 3 structs, ..." ending in
 * " val.tadah" (the training dataset is db.tadah); `tadah hpo -c
 * seed.tadah --hpotarget config.hpo`, seed.tadah being that best_pot.tadah
 * with the DBFILE line added and no --validation given, exits 1, refusing
 * PC_ERMSE "but no --validation file was given".
 * Before the fix: best_pot.tadah had the lines "HPOTARGET config.hpo",
 * "VALIDATION config.val" and "VERBOSE 1", and the second run validated on
 * the config.val its seed named, which no --validation asked for (exit 0).
 * The header's validation dataset passes before the fix as well (a pin);
 * the case checked only that best_pot.tadah has "Validation" and
 * "db.tadah", which the training dataset's line has.
 * Before the training record: passes as well (a pin); the case was named
 * "Subcommand: hpo writes no HPOTARGET, VALIDATION or VERBOSE into
 * best_pot.tadah, ...". */
TEST_CASE("Subcommand: hpo writes no HPOTARGET, VALIDATION or VERBOSE line "
          "into best_pot.tadah, and a seed potential names no validation "
          "file",
          "[TadahCLI][hpo][potential-content]") {
  ScratchCwd cwd("hpo_pot_run_inputs");
  write_hpo_potential_fixture();
  {
    const CliRun r = run_tadah({"tadah", "hpo", "-c", "config.tadah",
                                "--hpotarget", "config.hpo", "--validation",
                                "config.val", "-v", "1"});
    INFO("hpo: status " << r.status << "\n" << reported(r));
    REQUIRE(r.status == 0);
  }
  REQUIRE(std::filesystem::exists("best_pot.tadah"));
  const std::string best = slurp("best_pot.tadah");
  INFO("best_pot.tadah:\n" << best);
  for (const char* key : {"HPOTARGET", "VALIDATION", "VERBOSE", "DBFILE"}) {
    INFO("key " << key);
    CHECK(line_tokens(best, key).empty());
  }
  CHECK(line_tokens(best, "MODEL") ==
        std::vector<std::string>{"MODEL", "M_BLR", "BF_Linear"});
  CHECK(line_tokens(best, "WEIGHTS").size() == 1 + 4);
  {
    std::istringstream in(best);
    std::string line, after_validation;
    bool seen = false;
    while (std::getline(in, line)) {
      if (seen) {
        after_validation = line;
        break;
      }
      seen = line.rfind("#   Validation ", 0) == 0;
    }
    INFO("the line after the header's Validation line: " << after_validation);
    CHECK(seen);
    CHECK(after_validation.rfind("#   [1] 3 structs, ", 0) == 0);
    const std::string tail = " val.tadah";
    const bool names_val =
        after_validation.size() > tail.size() &&
        after_validation.compare(after_validation.size() - tail.size(),
                                 tail.size(), tail) == 0;
    CHECK(names_val);
  }

  std::filesystem::rename("best_pot.tadah", "first_best.tadah");
  std::ofstream("seed.tadah")
      << as_training_config(slurp("first_best.tadah"), "db.tadah");
  const CliRun r = run_tadah({"tadah", "hpo", "-c", "seed.tadah",
                              "--hpotarget", "config.hpo"});
  const std::string rep = reported(r);
  INFO("hpo -c seed.tadah: status " << r.status << "\n" << rep);
  CHECK(r.status == 1);
  CHECK(contains(rep, "but no --validation file was given"));
  CHECK_FALSE(std::filesystem::exists("best_pot.tadah"));
}

/* Pins: `tadah hpo` with a --validation file whose DBFILE lists one dataset
 * twice -- "DBFILE db.tadah" and "DBFILE ./db.tadah" -- exits 1 without
 * best_pot.tadah, refusing it naming the file (config.val), "DBFILE lists
 * one dataset twice" and both entries ("entry 1 (db.tadah) and entry 2
 * (./db.tadah)").
 * Before the fix: exit 0; every validation metric took the dataset twice
 * into its average over the validation datasets. */
TEST_CASE("Subcommand: hpo refuses a validation DBFILE that lists one "
          "dataset twice",
          "[TadahCLI][hpo][validation_file]") {
  ScratchCwd cwd("hpo_val_twice");
  write_hpo_potential_fixture();
  std::ofstream("config.val") << "DBFILE db.tadah\nDBFILE ./db.tadah\n";
  const CliRun r = run_tadah({"tadah", "hpo", "-c", "config.tadah",
                              "--hpotarget", "config.hpo", "--validation",
                              "config.val"});
  const std::string rep = reported(r);
  INFO("status " << r.status << "\n" << rep);
  CHECK(r.status == 1);
  CHECK(contains(rep, "config.val"));
  CHECK(contains(rep, "DBFILE lists one dataset twice"));
  CHECK(contains(rep, "entry 1 (db.tadah) and entry 2 (./db.tadah)"));
  CHECK_FALSE(std::filesystem::exists("best_pot.tadah"));
}

/* Pins: `tadah hpo ... --validation a.val b.val` (two validation files, of
 * the same dataset) exits 1 without best_pot.tadah, refusing "--validation
 * (VALIDATION) takes one validation configuration file" and naming both
 * ("a.val b.val").
 * Before the fix: exit 0; the run validated on a.val and b.val was not
 * read. */
TEST_CASE("Subcommand: hpo --validation takes one file",
          "[TadahCLI][hpo][validation_file]") {
  ScratchCwd cwd("hpo_two_val_files");
  write_hpo_potential_fixture();
  std::ofstream("a.val") << "DBFILE db.tadah\n";
  std::ofstream("b.val") << "DBFILE db.tadah\n";
  const CliRun r = run_tadah({"tadah", "hpo", "-c", "config.tadah",
                              "--hpotarget", "config.hpo", "--validation",
                              "a.val", "b.val"});
  const std::string rep = reported(r);
  INFO("status " << r.status << "\n" << rep);
  CHECK(r.status == 1);
  CHECK(contains(rep, "--validation (VALIDATION) takes one validation "
                      "configuration file"));
  CHECK(contains(rep, "a.val b.val"));
  CHECK_FALSE(std::filesystem::exists("best_pot.tadah"));
}
#endif  // TADAH_ENABLE_NFP

/* ---- [potential record]: the training record of a potential --------------
 *
 * A potential keeps the keys it does not use -- the DBFILE, VALIDATION,
 * HPOTARGET, VERBOSE, NUMERIC, OUTFILE, AUDIT, AUDIT_SAMPLE
 * and HEALTH_LOG its run was given, and the keys that prepared its
 * training data ([dataset keys] below) -- in a comment block above its key
 * lines, for the user's record; no loader of a potential reads it (tadah
 * predict, tadah train -c, tadah hpo -c, pair_style tadah), and refit
 * --edit reads it only to copy it. */

namespace {

/* The opening line said "(not read by tadah)" before it was corrected:
 * refit --edit reads the block, to copy it. Every case below that finds a
 * record fails on a potential with that line. */
const char* const kRecordOpen =
    "# --- training record (no key is read from it) ---";
const char* const kRecordEnd = "# --- end of training record ---";

/* The lines of the training record of the potential text @p pot, without
 * their leading "# ", those of its fit balance table (balance_of) left
 * out; empty when it has none. */
std::vector<std::string> record_lines(const std::string& pot, bool balance) {
  std::istringstream in(pot);
  std::vector<std::string> out;
  bool in_block = false, in_table = false;
  for (std::string l; std::getline(in, l);) {
    if (!in_block) {
      in_block = l == kRecordOpen;
      continue;
    }
    if (l == kRecordEnd) break;
    const std::string line = l.rfind("# ", 0) == 0 ? l.substr(2) : l;
    if (line.rfind("Fit balance:", 0) == 0)
      in_table = true;
    else if (line.rfind("  ", 0) != 0)
      in_table = false;
    if (in_table == balance) out.push_back(line);
  }
  return out;
}
std::vector<std::string> record_of(const std::string& pot) {
  return record_lines(pot, false);
}

/* The fit balance table of the training record of @p pot (its lines from
 * "Fit balance:" on), which tadah train and best_pot.tadah end the record
 * with; empty when it has none. */
std::vector<std::string> balance_of(const std::string& pot) {
  return record_lines(pot, true);
}

/* Whether @p table is a fit balance table of the energies (and forces) of
 * one dataset: "Fit balance: LAMBDA ...", the line of the error scales, the
 * column header, the rows of the quantities over every dataset and the
 * prior's row. */
bool is_balance_table(const std::vector<std::string>& table) {
  return table.size() >= 5 &&
         table[0].rfind("Fit balance: LAMBDA ", 0) == 0 &&
         table[1].rfind("  error scales: ESIGMA ", 0) == 0 &&
         table[2].rfind("  quantity  dataset", 0) == 0 &&
         table[3].rfind("  energy    all ", 0) == 0 &&
         table.back().rfind("  prior", 0) == 0;
}

/* @p pot without its training record (the block's lines). */
std::string without_record(const std::string& pot) {
  std::istringstream in(pot);
  std::string out;
  bool in_block = false;
  for (std::string l; std::getline(in, l);) {
    if (l == kRecordOpen) in_block = true;
    if (!in_block) out += l + "\n";
    if (l == kRecordEnd) in_block = false;
  }
  return out;
}

/* Whether the line after the training record of @p pot is a key line (the
 * record is the comment block right above the key lines). */
bool record_precedes_keys(const std::string& pot) {
  std::istringstream in(pot);
  bool closed = false;
  for (std::string l; std::getline(in, l);) {
    if (closed) {
      std::istringstream ls(l);
      std::string k;
      return (ls >> k) && k[0] != '#';
    }
    closed = l == kRecordEnd;
  }
  return false;
}

}  // namespace

/* Pins: `tadah train -c config` (config: DBFILE db.tadah and the Nb-Ta keys
 * of the BIAS cases) writes pot.tadah whose training record, right above
 * its key lines, is "DBFILE db.tadah" alone: the blueprint defaults the run
 * uses and nobody wrote (VERBOSE 1, OUTFILE pot.tadah) are not recorded;
 * `tadah train -c config -o pot2.tadah -v 0` records "DBFILE db.tadah",
 * "VERBOSE 0" and "OUTFILE pot2.tadah" (the command line's values), and no
 * key line of them; `tadah predict -p pot.tadah -d db.tadah` writes
 * energy.pred byte for byte as with bare.tadah, pot.tadah without its
 * record; and the documented retrain, `tadah train -c retrain.train -o
 * pot3.tadah` (pot.tadah, its record a comment, with "DBFILE db2.tadah"
 * added), records "DBFILE db2.tadah" and "OUTFILE pot3.tadah", and
 * db.tadah appears nowhere in pot3.tadah. Each record ends with the fit
 * balance table of its fit (pot.tadah's and pot2.tadah's the same).
 * Before the fix: no potential had a training record (an earlier rule
 * dropped the keys).
 * Before the opening line was corrected: it read
 * "# --- training record (not read by tadah) ---": record_of finds no
 * record and the record checks fail. */
TEST_CASE("Subcommand: train records the keys the run was given in the "
          "potential's training record",
          "[TadahCLI][train][potential-content][training-record]") {
  ScratchCwd cwd("train_record");
  std::ofstream("db.tadah") << nbta_dataset(false);
  std::ofstream("db2.tadah") << nbta_dataset(false);
  std::ofstream("config") << "DBFILE db.tadah\n" << kNbTaKeys;
  {
    const CliRun t = run_tadah({"tadah", "train", "-c", "config"});
    INFO("train: status " << t.status << "\n" << reported(t));
    REQUIRE(t.status == 0);
  }
  const std::string pot = slurp("pot.tadah");
  INFO("pot.tadah:\n" << pot);
  CHECK(record_of(pot) == std::vector<std::string>{"DBFILE db.tadah"});
  CHECK(is_balance_table(balance_of(pot)));
  CHECK(record_precedes_keys(pot));
  {
    const CliRun t = run_tadah({"tadah", "train", "-c", "config", "-o",
                                "pot2.tadah", "-v", "0"});
    INFO("train -o -v: status " << t.status << "\n" << reported(t));
    REQUIRE(t.status == 0);
  }
  const std::string pot2 = slurp("pot2.tadah");
  INFO("pot2.tadah:\n" << pot2);
  CHECK(record_of(pot2) ==
        std::vector<std::string>{"DBFILE db.tadah", "VERBOSE 0",
                                 "OUTFILE pot2.tadah"});
  CHECK(balance_of(pot2) == balance_of(pot));
  for (const char* key : {"DBFILE", "VERBOSE", "OUTFILE"}) {
    INFO("key " << key);
    CHECK(line_tokens(pot2, key).empty());
  }

  std::ofstream("bare.tadah") << without_record(pot);
  {
    const CliRun p = run_tadah({"tadah", "predict", "-p", "pot.tadah", "-d",
                                "db.tadah"});
    INFO("predict pot.tadah: status " << p.status << "\n" << reported(p));
    REQUIRE(p.status == 0);
    std::filesystem::rename("energy.pred", "energy_record.pred");
  }
  {
    const CliRun p = run_tadah({"tadah", "predict", "-p", "bare.tadah", "-d",
                                "db.tadah"});
    INFO("predict bare.tadah: status " << p.status << "\n" << reported(p));
    REQUIRE(p.status == 0);
  }
  CHECK_FALSE(slurp("energy.pred").empty());
  CHECK(slurp("energy_record.pred") == slurp("energy.pred"));

  std::ofstream("retrain.train") << as_training_config(pot, "db2.tadah");
  {
    const CliRun t = run_tadah({"tadah", "train", "-c", "retrain.train", "-o",
                                "pot3.tadah"});
    INFO("retrain: status " << t.status << "\n" << reported(t));
    REQUIRE(t.status == 0);
  }
  const std::string pot3 = slurp("pot3.tadah");
  INFO("pot3.tadah:\n" << pot3);
  CHECK(record_of(pot3) ==
        std::vector<std::string>{"DBFILE db2.tadah", "OUTFILE pot3.tadah"});
  CHECK(is_balance_table(balance_of(pot3)));
  CHECK(pot3.find("db.tadah") == std::string::npos);
}

/* Pins: `tadah refit -c config.refit -o refit_pot.tadah` (a dense coarse
 * refit of the small Ta setfl) writes refit_pot.tadah whose training record
 * is "OUTFILE refit_pot.tadah" (its configuration sets none of the other
 * keys; no fit balance table: refit fits curves, no observations); `tadah
 * refit --edit refit_pot.tadah --func phi --add 2.5 -o
 * edited.tadah -v 0` writes edited.tadah whose record is "edited by tadah
 * refit --edit from refit_pot.tadah" and then the input's record, "OUTFILE
 * refit_pot.tadah" (the edit fits nothing: its record is that of the fit
 * whose weights it edits; the edit's own -o and -v are not added).
 * Before the fix: neither potential had a training record.
 * Before the opening line was corrected: it
 * read "# --- training record (not read by tadah) ---": record_of finds no
 * record and the record checks fail. */
TEST_CASE("Subcommand: refit records its run, and refit --edit copies the "
          "record of its input",
          "[TadahCLI][refit][potential-content][training-record]") {
  ScratchCwd cwd("refit_record");
  { std::ofstream os("eam_small.alloy"); os << small_ta_setfl(); }
  std::ofstream("config.refit")
      << "EAM eam_small.alloy\nRHOFORM SIGNED\nREFIT_MODE dense\n"
         "REFIT_BASIS Blip\nREFIT_LEVEL coarse\nREFIT_MAX_ESCALATIONS 0\n";
  {
    const CliRun r = run_tadah({"tadah", "refit", "-c", "config.refit", "-o",
                                "refit_pot.tadah"});
    INFO("refit: status " << r.status << "\n" << reported(r));
    REQUIRE(r.status == 0);
  }
  const std::string refit_pot = slurp("refit_pot.tadah");
  INFO("refit_pot.tadah:\n" << refit_pot);
  CHECK(record_of(refit_pot) ==
        std::vector<std::string>{"OUTFILE refit_pot.tadah"});
  CHECK(balance_of(refit_pot).empty());  // refit fits no observations
  CHECK(record_precedes_keys(refit_pot));
  {
    const CliRun r = run_tadah({"tadah", "refit", "--edit", "refit_pot.tadah",
                                "--func", "phi", "--add", "2.5", "-o",
                                "edited.tadah", "-v", "0"});
    INFO("--edit: status " << r.status << "\n" << reported(r));
    REQUIRE(r.status == 0);
  }
  const std::string edited = slurp("edited.tadah");
  INFO("edited.tadah:\n" << edited);
  CHECK(record_of(edited) ==
        std::vector<std::string>{
            "edited by tadah refit --edit from refit_pot.tadah",
            "OUTFILE refit_pot.tadah"});
  CHECK(line_tokens(edited, "OUTFILE").empty());
}

/* Pins: `tadah refit --edit noend.tadah --func phi --add 2.5 -o e1.tadah`,
 * noend.tadah being the refit_pot.tadah of `tadah refit -c config.refit -o
 * refit_pot.tadah` (the case above) without the end line of its training
 * record, exits 1 without writing e1.tadah, saying "noend.tadah: line 1
 * opens a training record (...) that no end line (...) closes before line
 * 3, the first line after it that is not a comment: a file with the end
 * line after the record, before line 3, or without the record, is
 * accepted." -- line 1 is the opening line and line 3 the first key line,
 * AMPGRIDMB, which every writer puts right after the record -- and not the
 * refusal of a key line inside the record ("is not a comment: every reader
 * ...", whose rule that a line of the record begins with '#' would comment
 * out a key of the potential); the unchanged refit_pot.tadah edits (exit
 * 0).
 * Before the fix of its wording the refusal ended "...: restore the end
 * line after the record, before line 3, or delete the record.", an
 * instruction.
 * Before the fix: exit 1 with "noend.tadah: line 3, inside the training
 * record that line 1 opens, is not a comment: every reader of the potential
 * reads it as a key. Begin each line of the record with '#', or end the
 * record (...) before it."; no potential got the missing-end-line refusal,
 * which came only when the file ended inside the record.
 * Before the opening line was corrected: line 1
 * of refit_pot.tadah read "# --- training record (not read by tadah) ---":
 * REQUIRE(lines[0] == kRecordOpen) stops the case. */
TEST_CASE("Subcommand: refit --edit of a potential whose training record "
          "lost its end line names the missing end line, not its first key "
          "line",
          "[TadahCLI][refit][potential-content][training-record]") {
  ScratchCwd cwd("refit_record_no_end");
  { std::ofstream os("eam_small.alloy"); os << small_ta_setfl(); }
  std::ofstream("config.refit")
      << "EAM eam_small.alloy\nRHOFORM SIGNED\nREFIT_MODE dense\n"
         "REFIT_BASIS Blip\nREFIT_LEVEL coarse\nREFIT_MAX_ESCALATIONS 0\n";
  {
    const CliRun r = run_tadah({"tadah", "refit", "-c", "config.refit", "-o",
                                "refit_pot.tadah"});
    INFO("refit: status " << r.status << "\n" << reported(r));
    REQUIRE(r.status == 0);
  }
  const std::string refit_pot = slurp("refit_pot.tadah");
  INFO("refit_pot.tadah:\n" << refit_pot);
  std::vector<std::string> lines;
  {
    std::istringstream in(refit_pot);
    for (std::string l; std::getline(in, l);)
      if (l != kRecordEnd) lines.push_back(l);
  }
  REQUIRE(lines.size() > 3);
  REQUIRE(lines[0] == kRecordOpen);
  REQUIRE(lines[1] == "# OUTFILE refit_pot.tadah");
  REQUIRE(lines[2].rfind("DESCRIPTOR", 0) == 0);  // the first declaration
  {
    std::ofstream os("noend.tadah");
    for (const std::string& l : lines) os << l << "\n";
  }
  {
    const CliRun r = run_tadah({"tadah", "refit", "--edit", "noend.tadah",
                                "--func", "phi", "--add", "2.5", "-o",
                                "e1.tadah"});
    const std::string rep = reported(r);
    INFO("--edit noend.tadah: status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    CHECK(contains(rep, std::string("noend.tadah: line 1 opens a training "
                                    "record ('") +
                            kRecordOpen + "') that no end line ('" +
                            kRecordEnd +
                            "') closes before line 3, the first line after "
                            "it that is not a comment: a file with the end "
                            "line after the record, before line 3, or "
                            "without the record, is accepted."));
    CHECK_FALSE(contains(rep, "is not a comment: every reader"));
    CHECK_FALSE(std::filesystem::exists("e1.tadah"));
  }
  {
    const CliRun r = run_tadah({"tadah", "refit", "--edit", "refit_pot.tadah",
                                "--func", "phi", "--add", "2.5", "-o",
                                "e2.tadah"});
    INFO("--edit refit_pot.tadah: status " << r.status << "\n"
                                           << reported(r));
    CHECK(r.status == 0);
    CHECK(std::filesystem::exists("e2.tadah"));
  }
}

#ifdef TADAH_ENABLE_NFP
/* Pins: `tadah hpo -c config.tadah --hpotarget config.hpo --validation
 * config.val` (write_hpo_potential_fixture: one evaluation; config.tadah
 * sets DBFILE db.tadah and VERBOSE 0) writes best_pot.tadah whose training
 * record, right above its key lines, is "DBFILE db.tadah", "VALIDATION
 * config.val", "HPOTARGET config.hpo" and "VERBOSE 0" (the configuration
 * file's and the command line's values), then the fit balance table of
 * the evaluation's fit, with no key line of them; the
 * header's training dataset line ("#   [1] 3 structs, ..." after
 * "#   Training ") names no db.tadah, which the record names; and `tadah
 * predict -p best_pot.tadah -d db.tadah` writes energy.pred byte for byte as
 * with best_pot.tadah without its record.
 * Before the fix: best_pot.tadah had no training record, and the header's
 * training dataset line ended with "  db.tadah".
 * Before the opening line was corrected: it
 * read "# --- training record (not read by tadah) ---": record_of finds no
 * record and the record checks fail. */
TEST_CASE("Subcommand: hpo records the run's DBFILE, --validation, "
          "--hpotarget and VERBOSE in best_pot.tadah's training record",
          "[TadahCLI][hpo][potential-content][training-record]") {
  ScratchCwd cwd("hpo_record");
  write_hpo_potential_fixture();
  {
    const CliRun r = run_tadah({"tadah", "hpo", "-c", "config.tadah",
                                "--hpotarget", "config.hpo", "--validation",
                                "config.val"});
    INFO("hpo: status " << r.status << "\n" << reported(r));
    REQUIRE(r.status == 0);
  }
  const std::string best = slurp("best_pot.tadah");
  INFO("best_pot.tadah:\n" << best);
  CHECK(record_of(best) ==
        std::vector<std::string>{"DBFILE db.tadah", "VALIDATION config.val",
                                 "HPOTARGET config.hpo", "VERBOSE 0"});
  CHECK(is_balance_table(balance_of(best)));
  CHECK(record_precedes_keys(best));
  for (const char* key : {"DBFILE", "VALIDATION", "HPOTARGET", "VERBOSE"}) {
    INFO("key " << key);
    CHECK(line_tokens(best, key).empty());
  }
  {
    std::istringstream in(best);
    std::string line, after_training;
    bool seen = false;
    while (std::getline(in, line)) {
      if (seen) {
        after_training = line;
        break;
      }
      seen = line.rfind("#   Training ", 0) == 0;
    }
    INFO("the line after the header's Training line: " << after_training);
    CHECK(seen);
    CHECK(after_training.rfind("#   [1] 3 structs, ", 0) == 0);
    CHECK(after_training.find("db.tadah") == std::string::npos);
  }
  std::ofstream("bare.tadah") << without_record(best);
  {
    const CliRun p = run_tadah({"tadah", "predict", "-p", "best_pot.tadah",
                                "-d", "db.tadah"});
    INFO("predict best_pot.tadah: status " << p.status << "\n"
                                           << reported(p));
    REQUIRE(p.status == 0);
    std::filesystem::rename("energy.pred", "energy_record.pred");
  }
  {
    const CliRun p = run_tadah({"tadah", "predict", "-p", "bare.tadah", "-d",
                                "db.tadah"});
    INFO("predict bare.tadah: status " << p.status << "\n" << reported(p));
    REQUIRE(p.status == 0);
  }
  CHECK_FALSE(slurp("energy.pred").empty());
  CHECK(slurp("energy_record.pred") == slurp("energy.pred"));
}

/* Pins: `tadah hpo -c config.tadah --hpotarget config.hpo --validation
 * config.val` without -v, config.tadah being write_hpo_potential_fixture's
 * without its "VERBOSE 0" line, writes best_pot.tadah whose training record
 * is exactly "DBFILE db.tadah", "VALIDATION config.val" and "HPOTARGET
 * config.hpo": the VERBOSE 1 the run takes is the blueprint default nobody
 * wrote (the hpo task's only record key with a default; hpo_run passes the
 * task's KeyProvenance, whose origin of it is Default), so it is not
 * recorded, and there is no VERBOSE key line either.
 * Before the fix: passes as well (a pin of the provenance rule the HPO
 * CHANGELOG and the Training page state: with HPOTargets built without
 * the provenance, hpo_run_from_startup passing nullptr, the record ends with
 * "VERBOSE 1" and this case fails; the case above sets VERBOSE 0 and
 * passes either way).
 * Before the opening line was corrected: it
 * read "# --- training record (not read by tadah) ---": record_of finds no
 * record and the record check fails. */
TEST_CASE("Subcommand: hpo leaves the VERBOSE nobody wrote out of "
          "best_pot.tadah's training record",
          "[TadahCLI][hpo][potential-content][training-record]") {
  ScratchCwd cwd("hpo_record_defaults");
  write_hpo_potential_fixture();
  {
    std::istringstream in(slurp("config.tadah"));
    std::ostringstream out;
    bool dropped = false;
    for (std::string l; std::getline(in, l);) {
      if (l == "VERBOSE 0") {
        dropped = true;
        continue;
      }
      out << l << "\n";
    }
    REQUIRE(dropped);
    std::ofstream("config.tadah") << out.str();
  }
  {
    const CliRun r = run_tadah({"tadah", "hpo", "-c", "config.tadah",
                                "--hpotarget", "config.hpo", "--validation",
                                "config.val"});
    INFO("hpo: status " << r.status << "\n" << reported(r));
    REQUIRE(r.status == 0);
  }
  const std::string best = slurp("best_pot.tadah");
  INFO("best_pot.tadah:\n" << best);
  CHECK(record_of(best) ==
        std::vector<std::string>{"DBFILE db.tadah", "VALIDATION config.val",
                                 "HPOTARGET config.hpo"});
  CHECK(line_tokens(best, "VERBOSE").empty());
}
#endif  // TADAH_ENABLE_NFP

/* ---- [dataset keys]: the keys that prepared the training data -----------
 *
 * A potential's key lines are only what is needed to use the model -- the
 * model's keys, its error scales, the resolved ESHIFT values and LSCALE.
 * EFILTER, FFILTER, ESHIFT_ATOM, ESHIFT_DBATOM, ZERO_COM_FORCE and WDBFILE
 * prepared the training data: tadah writes them into the training record;
 * tadah predict ignores them in a potential (an older one has them as key
 * lines too), so a filter applies to the data predicted only when predict
 * is given it; --no-eshift compares the model with the reference energies
 * as they are; a retrain from a potential (and tadah hpo -c of one) takes
 * none of them from the potential, so it works with any number of
 * datasets. The retired EWEIGHT_TEMP and WDBFILE_AUTO, which prepared the
 * training data of a potential of an earlier version, are no key of a
 * potential tadah writes either: a load of one that has them gives one
 * notice that lists them. */

namespace {

/* weighted_entry() with the energy @p energy instead of -1.0. */
std::string entry_with_energy(const std::string& label,
                              const std::string& energy,
                              const std::string& second_atom,
                              const std::string& f) {
  std::string e = weighted_entry(label, "1 1 1", second_atom, f, "0");
  const std::string from = "\n-1.0\n";
  const std::size_t at = e.find(from);
  REQUIRE(at != std::string::npos);
  return e.replace(at, from.size(), "\n" + energy + "\n");
}

/* The three dimers of retrain_dataset() with the energy @p energy each. */
std::string dimers_with_energy(const std::string& energy) {
  return entry_with_energy("D1", energy, "2.60 0.40 0.30", "0.12") +
         entry_with_energy("D2", energy, "3.10 0.20 0.50", "-0.07") +
         entry_with_energy("D3", energy, "3.70 0.10 0.20", "0.03");
}

/* The pair configuration of retrain_config() (M_BLR BF_Linear, LAMBDA 0.1,
 * OALGO 4) with DBFILE @p dbfile and the lines @p extra. */
std::string dataset_keys_config(const std::string& dbfile,
                                const std::string& extra) {
  return with_line(retrain_config("M_BLR BF_Linear", "LAMBDA 0.1\nOALGO 4\n"),
                   "DBFILE", "DBFILE " + dbfile) +
         extra;
}

/* The training-only dataset keys. */
const char* const kDatasetKeys[] = {"EFILTER",        "FFILTER",
                                    "ESHIFT_ATOM",    "ESHIFT_DBATOM",
                                    "ZERO_COM_FORCE", "WDBFILE"};

/* The keys retired in Tadah! 1.6.0: no potential tadah writes has a line of
 * them. */
const char* const kRetiredKeys[] = {"EWEIGHT", "FWEIGHT", "SWEIGHT",
                                    "WDBFILE_AUTO", "EWEIGHT_TEMP"};

/* The error scales every potential a fit writes has a line of. */
const char* const kErrorScaleKeys[] = {"ESIGMA", "FSIGMA", "SSIGMA", "ENORM",
                                       "ENERGY"};

/* The key lines of the six and of the retired EWEIGHT_TEMP and
 * WDBFILE_AUTO with the values of a potential of an earlier version trained
 * with them (as the training configuration set them), in this order. */
const char* const kOlderDatasetKeyLines =
    "EFILTER -0.6 -0.4\nFFILTER 1.0\nZERO_COM_FORCE true\n"
    "ESHIFT_DBATOM true\nEWEIGHT_TEMP 300\nWDBFILE 1 0\nWDBFILE_AUTO 1\n";

/* The number of lines of @p text (each ends with a newline). */
std::size_t lines_in(const std::string& text) {
  return static_cast<std::size_t>(std::count(text.begin(), text.end(), '\n'));
}

/* The lines of @p text that contain @p needle. */
std::size_t count_lines_with(const std::string& text,
                             const std::string& needle) {
  std::istringstream in(text);
  std::size_t n = 0;
  for (std::string line; std::getline(in, line);)
    if (line.find(needle) != std::string::npos) ++n;
  return n;
}

}  // namespace

/* Pins: `tadah train -c config` (DBFILE db.tadah: the three dimers of
 * retrain_dataset(), E/N -0.5 eV and forces of 0.2 eV/A at most; with
 * EFILTER -0.6 -0.4, FFILTER 1.0, ZERO_COM_FORCE true and ESHIFT_DBATOM
 * true, which keep them all) writes pot.tadah with no key line of any of
 * the six dataset keys (its ESHIFT line, the shift ESHIFT_DBATOM resolved,
 * stays) or of the retired EWEIGHT, FWEIGHT, SWEIGHT, WDBFILE_AUTO and
 * EWEIGHT_TEMP, a line of each of ESIGMA, FSIGMA, SSIGMA, ENORM and
 * ENERGY, and the training record "DBFILE db.tadah", "EFILTER -0.6 -0.4",
 * "FFILTER 1.0", "ESHIFT_DBATOM true", "ZERO_COM_FORCE true", "VERBOSE 0";
 * `tadah predict
 * -p pot.tadah -d pred.tadah -F`, pred.tadah being the three dimers, LOW
 * (E/N -1.5 eV, outside the EFILTER window) and HOT (forces of 8.7 eV/A,
 * above FFILTER), exits 0, predicts all five structures (energy.pred has 5
 * rows) and prints no "Multiple ESHIFT sources" WARNING; with --efilter
 * -0.6 -0.4 --ffilter 1.0 on the predict command line it predicts the
 * three (the filters apply to the data predicted when predict is given
 * them).
 * Before the fix: the four were key lines of pot.tadah, with ESHIFT_ATOM
 * false and WDBFILE_AUTO 0 (the blueprint defaults of the train task), and
 * its record was "DBFILE db.tadah", "VERBOSE 0" after the opening line
 * "(not read by tadah)"; predict applied the potential's filters: 3 rows,
 * and the WARNING "Multiple ESHIFT sources set; precedence: explicit > ATOM
 * > DBATOM." (ESHIFT_DBATOM true beside the ESHIFT line). Before the error
 * scales the potential had EWEIGHT, FWEIGHT and SWEIGHT lines and no
 * ESIGMA, FSIGMA, SSIGMA, ENORM or ENERGY line. */
TEST_CASE("Subcommand: predict of a potential trained with filters and "
          "ESHIFT_DBATOM predicts every structure it is given",
          "[TadahCLI][predict][dataset-keys][training-record]") {
  ScratchCwd cwd("dataset_keys_predict_new");
  std::ofstream("db.tadah") << retrain_dataset();
  std::ofstream("config") << dataset_keys_config(
      "db.tadah",
      "EFILTER -0.6 -0.4\nFFILTER 1.0\nZERO_COM_FORCE true\n"
      "ESHIFT_DBATOM true\n");
  {
    const CliRun t = run_tadah({"tadah", "train", "-c", "config"});
    INFO("train: status " << t.status << "\n" << reported(t));
    REQUIRE(t.status == 0);
  }
  const std::string pot = slurp("pot.tadah");
  INFO("pot.tadah:\n" << pot);
  CHECK(record_of(pot) ==
        std::vector<std::string>{"DBFILE db.tadah", "EFILTER -0.6 -0.4",
                                 "FFILTER 1.0", "ESHIFT_DBATOM true",
                                 "ZERO_COM_FORCE true", "VERBOSE 0"});
  for (const char* key : kDatasetKeys) {
    INFO("key " << key);
    CHECK(line_tokens(pot, key).empty());
  }
  for (const char* key : kRetiredKeys) {
    INFO("key " << key);
    CHECK(line_tokens(pot, key).empty());
  }
  for (const char* key : kErrorScaleKeys) {
    INFO("key " << key);
    CHECK_FALSE(line_tokens(pot, key).empty());
  }
  CHECK(line_tokens(pot, "ESHIFT").size() == 2u);  // ESHIFT and one value

  std::ofstream("pred.tadah")
      << retrain_dataset()
      << entry_with_energy("LOW", "-3.0", "2.60 0.40 0.30", "0.12")
      << weighted_entry("HOT", "1 1 1", "3.10 0.20 0.50", "5.0", "0");
  {
    const CliRun p = run_tadah({"tadah", "predict", "-p", "pot.tadah", "-d",
                                "pred.tadah", "-F"});
    const std::string rep = reported(p);
    INFO("predict: status " << p.status << "\n" << rep);
    REQUIRE(p.status == 0);
    CHECK(predicted_values("energy.pred").size() == 5u);
    CHECK_FALSE(contains(rep, "Multiple ESHIFT sources"));
  }
  {
    const CliRun p = run_tadah({"tadah", "predict", "-p", "pot.tadah", "-d",
                                "pred.tadah", "-F", "--efilter", "-0.6",
                                "-0.4", "--ffilter", "1.0"});
    INFO("predict --efilter --ffilter: status " << p.status << "\n"
                                                << reported(p));
    REQUIRE(p.status == 0);
    CHECK(predicted_values("energy.pred").size() == 3u);
  }
}

/* Pins: `tadah predict -F` of old.tadah -- bare.tadah (the potential of
 * the plain configuration, without its record) with the key lines of a
 * potential of an earlier version trained with EFILTER -0.6 -0.4, FFILTER
 * 1.0, ZERO_COM_FORCE true, ESHIFT_DBATOM true, EWEIGHT_TEMP 300, WDBFILE
 * 1 0 and WDBFILE_AUTO 1 -- on pred.tadah (the three dimers, LOW and HOT of
 * the case above) writes energy.pred and forces.pred byte for byte as
 * bare.tadah does (five structures, the reference energies unshifted: the
 * ESHIFT_DBATOM of the potential derives no shift from the data predicted),
 * and at -v 2 says so in one INFO line: "tadah predict: old.tadah: EFILTER
 * -0.6 -0.4, FFILTER 1.0, ESHIFT_DBATOM true, ZERO_COM_FORCE true and
 * WDBFILE 1 0 prepared the training data of the potential and are not
 * applied to the data predicted (tadah keeps such keys in a potential's
 * training record, not as key lines); --efilter, --ffilter and
 * --zero-com-force filter the data predicted."; the retired EWEIGHT_TEMP
 * and WDBFILE_AUTO get one notice, which names each with its line ("Retired
 * keys EWEIGHT_TEMP (line <n> of old.tadah) and WDBFILE_AUTO (line <m> of
 * old.tadah) are ignored").
 * Before the fix: predict applied old.tadah's EFILTER and FFILTER (3 rows)
 * and its ESHIFT_DBATOM, which fitted a shift to the data predicted and
 * subtracted it from the reference energies: energy.pred differs. Before
 * the error scales the INFO line named EWEIGHT_TEMP 300 and WDBFILE_AUTO 1
 * among the seven, and no notice named them retired. */
TEST_CASE("Subcommand: predict ignores the keys that prepared the training "
          "data of an older potential",
          "[TadahCLI][predict][dataset-keys]") {
  ScratchCwd cwd("dataset_keys_predict_old");
  std::ofstream("db.tadah") << retrain_dataset();
  std::ofstream("config") << dataset_keys_config("db.tadah", "");
  {
    const CliRun t = run_tadah({"tadah", "train", "-c", "config", "-o",
                                "trained.tadah"});
    INFO("train: status " << t.status << "\n" << reported(t));
    REQUIRE(t.status == 0);
  }
  const std::string bare = without_record(slurp("trained.tadah"));
  std::ofstream("bare.tadah") << bare;
  std::ofstream("old.tadah") << bare << kOlderDatasetKeyLines;
  std::ofstream("pred.tadah")
      << retrain_dataset()
      << entry_with_energy("LOW", "-3.0", "2.60 0.40 0.30", "0.12")
      << weighted_entry("HOT", "1 1 1", "3.10 0.20 0.50", "5.0", "0");
  {
    const CliRun p = run_tadah({"tadah", "predict", "-p", "bare.tadah", "-d",
                                "pred.tadah", "-F"});
    INFO("predict bare.tadah: status " << p.status << "\n" << reported(p));
    REQUIRE(p.status == 0);
    std::filesystem::rename("energy.pred", "energy_bare.pred");
    std::filesystem::rename("forces.pred", "forces_bare.pred");
  }
  const CliRun p = run_tadah({"tadah", "predict", "-p", "old.tadah", "-d",
                              "pred.tadah", "-F", "-v", "2"});
  const std::string rep = reported(p);
  INFO("predict old.tadah: status " << p.status << "\n" << rep);
  REQUIRE(p.status == 0);
  CHECK(predicted_values("energy_bare.pred").size() == 5u);
  CHECK(slurp("energy.pred") == slurp("energy_bare.pred"));
  CHECK(slurp("forces.pred") == slurp("forces_bare.pred"));
  CHECK(contains(rep,
                 "tadah predict: old.tadah: EFILTER -0.6 -0.4, FFILTER 1.0, "
                 "ESHIFT_DBATOM true, ZERO_COM_FORCE true and WDBFILE 1 0 "
                 "prepared the training data of the potential and are not "
                 "applied to the data predicted (tadah keeps such keys in a "
                 "potential's training record, not as key lines); "
                 "--efilter, --ffilter and --zero-com-force filter the data "
                 "predicted."));
  const std::size_t first = lines_in(bare);  // EFILTER is line first + 1
  CHECK(count_lines_with(rep, "Retired keys") == 1u);
  CHECK(contains(rep, "Retired keys EWEIGHT_TEMP (line " +
                          std::to_string(first + 5) +
                          " of old.tadah) and WDBFILE_AUTO (line " +
                          std::to_string(first + 7) +
                          " of old.tadah) are ignored"));
  CHECK_FALSE(contains(rep, "Multiple ESHIFT sources"));
}

/* Pins: `tadah predict --no-eshift -d db.tadah` (the three dimers, E/N -0.5
 * eV) compares the model with the reference energies as they are: the
 * True column of energy.pred is -0.5 for each structure, with
 *   - pot.tadah, trained on db.tadah with ESHIFT_DBATOM true (its ESHIFT
 *     line -0.5), and
 *   - old.tadah, bare.tadah (trained without a shift) with the key lines
 *     "ESHIFT -0.5" and "ESHIFT_DBATOM true" of an earlier version;
 * without --no-eshift the True column of each is 0 (-0.5 - (-0.5)).
 * Before the fix: pot.tadah had ESHIFT_DBATOM true as a key line, and with
 * the ESHIFT removed by --no-eshift the toggle fitted a shift to the data
 * predicted (-0.5 again): the True column of both was 0. */
TEST_CASE("Subcommand: predict --no-eshift compares the model with the "
          "reference energies as they are",
          "[TadahCLI][predict][dataset-keys]") {
  ScratchCwd cwd("dataset_keys_no_eshift");
  std::ofstream("db.tadah") << retrain_dataset();
  std::ofstream("config") << dataset_keys_config("db.tadah",
                                                 "ESHIFT_DBATOM true\n");
  std::ofstream("plain") << dataset_keys_config("db.tadah", "");
  for (const auto& [cfg, out] :
       {std::pair<std::string, std::string>{"config", "pot.tadah"},
        {"plain", "trained.tadah"}}) {
    const CliRun t = run_tadah({"tadah", "train", "-c", cfg, "-o", out});
    INFO("train " << cfg << ": status " << t.status << "\n" << reported(t));
    REQUIRE(t.status == 0);
  }
  {
    const std::vector<std::string> eshift =
        line_tokens(slurp("pot.tadah"), "ESHIFT");
    REQUIRE(eshift.size() == 2u);
    CHECK(std::stod(eshift[1]) == Approx(-0.5).epsilon(1e-12));
  }
  std::ofstream("old.tadah") << without_record(slurp("trained.tadah"))
                             << "ESHIFT -0.5\nESHIFT_DBATOM true\n";
  for (const char* pot : {"pot.tadah", "old.tadah"}) {
    INFO(pot);
    {
      const CliRun p = run_tadah({"tadah", "predict", "-p", pot, "-d",
                                  "db.tadah", "--no-eshift"});
      INFO("--no-eshift: status " << p.status << "\n" << reported(p));
      REQUIRE(p.status == 0);
      const std::vector<double> truth = column_values("energy.pred", 1);
      REQUIRE(truth.size() == 3u);
      for (double t : truth) CHECK(t == Approx(-0.5).margin(1e-12));
    }
    {
      const CliRun p = run_tadah({"tadah", "predict", "-p", pot, "-d",
                                  "db.tadah"});
      INFO("predict: status " << p.status << "\n" << reported(p));
      REQUIRE(p.status == 0);
      const std::vector<double> truth = column_values("energy.pred", 1);
      REQUIRE(truth.size() == 3u);
      for (double t : truth) CHECK(t == Approx(0.0).margin(1e-12));
    }
  }
}

/* Pins: `tadah predict -F -o e.dat` (one file for two outputs) exits 1
 * before it predicts, with one message, "tadah predict writes the energies
 * and the forces (--force): --outfile names 1 file, e.dat, and takes 2, the
 * energy file and the force file, or 3, the energy, force and stress
 * files", no ERROR line before it, and writes no e.dat; `-F -o e.dat f.dat`
 * exits 0 and writes both.
 * Before the fix: an ERROR line "Provide 2 output files: one for energy
 * and one for forces." came first, and the refusal that stopped the run
 * read "Invalid OUTFILE size 1. Expected either 2 for the requested outputs
 * or 3 for the default [energy, force, stress] layout.". */
TEST_CASE("Subcommand: predict -F with one --outfile file is refused in one "
          "message",
          "[TadahCLI][exit-status][predict]") {
  ScratchCwd cwd("predict_outfile_count");
  std::ofstream("db.tadah") << retrain_dataset();
  std::ofstream("config") << dataset_keys_config("db.tadah", "");
  {
    const CliRun t = run_tadah({"tadah", "train", "-c", "config"});
    INFO("train: status " << t.status << "\n" << reported(t));
    REQUIRE(t.status == 0);
  }
  {
    const CliRun p = run_tadah({"tadah", "predict", "-p", "pot.tadah", "-d",
                                "db.tadah", "-F", "-o", "e.dat"});
    const std::string rep = reported(p);
    INFO("-o e.dat: status " << p.status << "\n" << rep);
    CHECK(p.status == 1);
    CHECK(contains(rep, "tadah predict writes the energies and the forces "
                        "(--force): --outfile names 1 file, e.dat, and takes "
                        "2, the energy file and the force file, or 3, the "
                        "energy, force and stress files"));
    CHECK_FALSE(contains(rep, "Provide"));
    CHECK_FALSE(contains(rep, "Invalid OUTFILE size"));
    CHECK_FALSE(std::filesystem::exists("e.dat"));
  }
  {
    const CliRun p = run_tadah({"tadah", "predict", "-p", "pot.tadah", "-d",
                                "db.tadah", "-F", "-o", "e.dat", "f.dat"});
    INFO("-o e.dat f.dat: status " << p.status << "\n" << reported(p));
    CHECK(p.status == 0);
    CHECK(std::filesystem::exists("e.dat"));
    CHECK(std::filesystem::exists("f.dat"));
  }
}

/* Pins: `tadah predict -F` of pot.tadah, trained on db.tadah with LSCALE
 * 1.1 (its LSCALE line), rescales the data predicted by the potential's
 * LSCALE: energy.pred and forces.pred are byte for byte those of the same
 * predict with --lscale 1.1, and the predicted energies differ from those
 * of --no-lscale.
 * Before the fix: predict replaced the potential's LSCALE with 1.0, the
 * predict task's blueprint default (the merge of the potential and the
 * task's context put the task's keys last): the output was that of
 * --no-lscale. */
TEST_CASE("Subcommand: predict applies the LSCALE of the potential",
          "[TadahCLI][predict][dataset-keys][lscale]") {
  ScratchCwd cwd("dataset_keys_lscale");
  std::ofstream("db.tadah") << retrain_dataset();
  std::ofstream("config") << dataset_keys_config("db.tadah", "LSCALE 1.1\n");
  {
    const CliRun t = run_tadah({"tadah", "train", "-c", "config"});
    INFO("train: status " << t.status << "\n" << reported(t));
    REQUIRE(t.status == 0);
  }
  REQUIRE(line_tokens(slurp("pot.tadah"), "LSCALE").size() == 2u);
  const auto predict = [](std::vector<std::string> extra,
                          const std::string& tag) {
    std::vector<std::string> args = {"tadah", "predict", "-p", "pot.tadah",
                                     "-d", "db.tadah", "-F"};
    args.insert(args.end(), extra.begin(), extra.end());
    const CliRun p = run_tadah(args);
    INFO(tag << ": status " << p.status << "\n" << reported(p));
    REQUIRE(p.status == 0);
    std::filesystem::rename("energy.pred", "energy_" + tag + ".pred");
    std::filesystem::rename("forces.pred", "forces_" + tag + ".pred");
  };
  predict({}, "default");
  predict({"--lscale", "1.1"}, "lscale");
  predict({"--no-lscale"}, "none");
  CHECK_FALSE(slurp("energy_default.pred").empty());
  CHECK(slurp("energy_default.pred") == slurp("energy_lscale.pred"));
  CHECK(slurp("forces_default.pred") == slurp("forces_lscale.pred"));
  CHECK(predicted_values("energy_default.pred") !=
        predicted_values("energy_none.pred"));
}

/* Pins: a retrain from a potential takes none of the keys that prepared its
 * training data from it, so it works with any number of datasets:
 *   - `tadah train -c config` (DBFILE a.tadah b.tadah, the dimers at E/N
 *     -0.5 and -0.55 eV, with WDBFILE 1 0.5, EFILTER -0.7 -0.3 and
 *     ESHIFT_DBATOM true) writes pot.tadah (its ESHIFT line e0);
 *   - `tadah train -c retrain.train -o pot2.tadah`, retrain.train being
 *     pot.tadah with "DBFILE a.tadah b.tadah c.tadah" (c.tadah at E/N
 *     -0.45 eV), exits 0; pot2.tadah has the ESHIFT e0 (the model's energy
 *     reference, not re-derived) and the record "DBFILE a.tadah b.tadah
 *     c.tadah", "OUTFILE pot2.tadah";
 *   - rederive.train, retrain.train without its ESHIFT line, with
 *     --eshift-dbatom (the documented way to derive ESHIFT from the data of
 *     the retrain: delete the ESHIFT line and give the run the toggle), gives
 *     pot3.tadah the ESHIFT of a training configuration of the three datasets
 *     with ESHIFT_DBATOM true (pot4.tadah), not e0; the same copy with its
 *     own line "ESHIFT_DBATOM true" and no option (pot7.tadah): the line is
 *     not read (the copy is a potential), so pot7.tadah has no ESHIFT line,
 *     with the WARNING "tadah train: the configuration file
 *     rederive_line.train is a potential (it has WEIGHTS): its ESHIFT_DBATOM
 *     true prepared the training data of the fit that wrote it, not of this
 *     run, and is ignored ... This run takes its own ESHIFT_DBATOM from
 *     --eshift-dbatom on the command line, or from an ESHIFT_DBATOM line in a
 *     configuration file that includes the potential with CONFIG ...";
 *   - old.train, pot.tadah without its record and with the key lines
 *     "WDBFILE 1 0.5", "EFILTER 5 6" and "ESHIFT_DBATOM true" of an earlier
 *     version, and the three datasets, exits 0 (pot5.tadah, ESHIFT e0),
 *     with the WARNING "tadah train: the configuration file old.train is a
 *     potential (it has WEIGHTS): its EFILTER 5 6, ESHIFT_DBATOM true and
 *     WDBFILE 1 0.5 prepared the training data of the fit that wrote it,
 *     ..."; with --wdbfile 1 1 2 (pot6.tadah) the command line's WDBFILE
 *     applies and is recorded: "DBFILE a.tadah b.tadah c.tadah", "WDBFILE 1
 *     1 2", "OUTFILE pot6.tadah".
 * Before the fix: pot.tadah had the key lines "WDBFILE 1 0.5", "EFILTER
 * -0.7 -0.3" and "ESHIFT_DBATOM true", and the retrain with three datasets
 * was refused ("WDBFILE size (2) does not match number of datasets (3).",
 * exit 1, after the WARNING "Multiple ESHIFT sources set; precedence:
 * explicit > ATOM > DBATOM."): the REQUIRE stops the case.
 * Before the fix that reads no ESHIFT_ATOM, ESHIFT_DBATOM or UNCERTAINTY
 * line of a potential: the copy's own ESHIFT_DBATOM true, beside no ESHIFT
 * line, was read and derived the shift (the documented way then), so
 * pot7.tadah had an ESHIFT line and no WARNING named the line: those two
 * checks fail.
 * Before the fix that states the advice in the impersonal voice: it read
 * "Give --eshift-dbatom on the command line, or an ESHIFT_DBATOM line in a
 * configuration file that includes the potential with CONFIG, to apply it to
 * this run": the check of that WARNING fails. */
TEST_CASE("Subcommand: train -c retrains a potential on any number of "
          "datasets, with the potential's ESHIFT",
          "[TadahCLI][train][retrain][dataset-keys][training-record]") {
  ScratchCwd cwd("dataset_keys_retrain");
  std::ofstream("a.tadah") << retrain_dataset();
  std::ofstream("b.tadah") << dimers_with_energy("-1.1");
  std::ofstream("c.tadah") << dimers_with_energy("-0.9");
  std::ofstream("config") << dataset_keys_config(
      "a.tadah b.tadah",
      "WDBFILE 1 0.5\nEFILTER -0.7 -0.3\nESHIFT_DBATOM true\n");
  const auto train = [](std::vector<std::string> args) {
    std::vector<std::string> argv = {"tadah", "train"};
    argv.insert(argv.end(), args.begin(), args.end());
    return run_tadah(argv);
  };
  {
    const CliRun t = train({"-c", "config"});
    INFO("train: status " << t.status << "\n" << reported(t));
    REQUIRE(t.status == 0);
  }
  const std::string pot = slurp("pot.tadah");
  INFO("pot.tadah:\n" << pot);
  const std::vector<std::string> e0 = line_tokens(pot, "ESHIFT");
  REQUIRE(e0.size() == 2u);
  const std::string datasets = "DBFILE a.tadah b.tadah c.tadah";
  const std::string retrain = with_line(pot, "DBFILE", "") + datasets + "\n";
  std::ofstream("retrain.train") << retrain;
  {
    const CliRun t = train({"-c", "retrain.train", "-o", "pot2.tadah"});
    INFO("retrain: status " << t.status << "\n" << reported(t));
    REQUIRE(t.status == 0);
    const std::string pot2 = slurp("pot2.tadah");
    INFO("pot2.tadah:\n" << pot2);
    CHECK(line_tokens(pot2, "ESHIFT") == e0);
    CHECK(record_of(pot2) ==
          std::vector<std::string>{datasets, "OUTFILE pot2.tadah"});
  }
  std::ofstream("rederive.train") << with_line(retrain, "ESHIFT", "");
  std::ofstream("rederive_line.train")
      << with_line(retrain, "ESHIFT", "") << "ESHIFT_DBATOM true\n";
  std::ofstream("fresh.train")
      << dataset_keys_config("a.tadah b.tadah c.tadah", "ESHIFT_DBATOM true\n");
  {
    const CliRun t = train({"-c", "rederive.train", "-o", "pot3.tadah",
                            "--eshift-dbatom"});
    INFO("rederive: status " << t.status << "\n" << reported(t));
    REQUIRE(t.status == 0);
    const CliRun f = train({"-c", "fresh.train", "-o", "pot4.tadah"});
    INFO("fresh: status " << f.status << "\n" << reported(f));
    REQUIRE(f.status == 0);
    const std::vector<std::string> e3 =
        line_tokens(slurp("pot3.tadah"), "ESHIFT");
    REQUIRE(e3.size() == 2u);
    CHECK(e3 == line_tokens(slurp("pot4.tadah"), "ESHIFT"));
    CHECK(e3 != e0);
  }
  {
    const CliRun t = train({"-c", "rederive_line.train", "-o", "pot7.tadah"});
    const std::string rep = reported(t);
    INFO("rederive_line: status " << t.status << "\n" << rep);
    REQUIRE(t.status == 0);
    CHECK(line_tokens(slurp("pot7.tadah"), "ESHIFT").empty());
    CHECK(contains(rep, "tadah train: the configuration file "
                        "rederive_line.train is a potential (it has "
                        "WEIGHTS): its ESHIFT_DBATOM true prepared the "
                        "training data of the fit that wrote it, not of "
                        "this run, and is ignored"));
    CHECK(contains(rep, "This run takes its own ESHIFT_DBATOM from "
                        "--eshift-dbatom on the command line, or from an "
                        "ESHIFT_DBATOM line in a configuration file that "
                        "includes the potential with CONFIG"));
  }
  std::ofstream("old.train")
      << with_line(without_record(pot), "DBFILE", "")
      << "WDBFILE 1 0.5\nEFILTER 5 6\nESHIFT_DBATOM true\n" << datasets
      << "\n";
  {
    const CliRun t = train({"-c", "old.train", "-o", "pot5.tadah"});
    const std::string rep = reported(t);
    INFO("old.train: status " << t.status << "\n" << rep);
    REQUIRE(t.status == 0);
    CHECK(contains(rep, "tadah train: the configuration file old.train is a "
                        "potential (it has WEIGHTS): its EFILTER 5 6, "
                        "ESHIFT_DBATOM true and WDBFILE 1 0.5 prepared the "
                        "training data of the fit that wrote it"));
    CHECK(line_tokens(slurp("pot5.tadah"), "ESHIFT") == e0);
  }
  {
    const CliRun t = train({"-c", "old.train", "-o", "pot6.tadah",
                            "--wdbfile", "1", "1", "2"});
    INFO("old.train --wdbfile: status " << t.status << "\n" << reported(t));
    REQUIRE(t.status == 0);
    CHECK(record_of(slurp("pot6.tadah")) ==
          std::vector<std::string>{datasets, "WDBFILE 1 1 2",
                                   "OUTFILE pot6.tadah"});
  }
}

/* Pins: a retrain that includes a potential with CONFIG in a configuration
 * file applies the keys that prepare the training data its own lines give,
 * and records them; only the potential's lines of them are not read.
 * pot.tadah is `tadah train -c config` (a.tadah b.tadah, WDBFILE 1 0.5);
 * the retrains fit a.tadah b.tadah c.tadah:
 *   - `tadah train -c run.train` ("CONFIG pot.tadah", the three datasets,
 *     "WDBFILE 1 0.001 1", "OUTFILE pot2.tadah") exits 0 with no WARNING
 *     about a potential, and pot2.tadah has the record "DBFILE a.tadah
 *     b.tadah c.tadah", "WDBFILE 1 0.001 1", "OUTFILE pot2.tadah" and the
 *     WEIGHTS of `tadah train -c retrain.train --wdbfile 1 0.001 1`
 *     (pot3.tadah; retrain.train: pot.tadah with the three datasets), which
 *     differ from those of the same file without its WDBFILE line
 *     (none.train, pot4.tadah);
 *   - `tadah train -c outer.train -o pot6.tadah`, outer.train being "CONFIG
 *     pot.tadah", the three datasets and "WDBFILE 1 0.001 1", gives the
 *     WEIGHTS of pot2.tadah and the record "DBFILE a.tadah b.tadah
 *     c.tadah", "WDBFILE 1 0.001 1", "OUTFILE pot6.tadah";
 *   - `tadah train -c old.train`, old.train being "ZERO_COM_FORCE true",
 *     "CONFIG old.tadah", the three datasets, "WDBFILE 1 0.001 1", "EFILTER
 *     -0.7 -0.3", "OUTFILE pot5.tadah", old.tadah being pot.tadah with the
 *     key lines "WDBFILE 1 0.5", "EFILTER 5 6", "FFILTER 0.001" (it would
 *     drop every structure) and "ZERO_COM_FORCE false" of an earlier version,
 *     exits 0; pot5.tadah's record is "DBFILE a.tadah b.tadah c.tadah",
 *     "EFILTER -0.7 -0.3", "ZERO_COM_FORCE true", "WDBFILE 1 0.001 1",
 *     "OUTFILE pot5.tadah" (the line before CONFIG too), and the WARNING
 *     names the one line of the potential the run does not give: "tadah
 *     train: old.tadah, which the configuration file old.train includes with
 *     CONFIG, is a potential (it has WEIGHTS): its FFILTER 0.001 prepared the
 *     training data of the fit that wrote it, not of this run, and is ignored
 *     (tadah keeps such keys in a potential's training record, not as key
 *     lines). This run takes its own FFILTER from --ffilter on the command
 *     line, or from an FFILTER line in old.train (the Training page of the
 *     documentation, Retraining from a potential).".
 * Before the fix: a key of a configuration with WEIGHTS was applied only
 * when it was a command-line option, and the include mixed the potential's
 * lines with the run's: the WDBFILE of the including file was ignored with
 * the WARNING "... is a potential (it has WEIGHTS): its WDBFILE 1 0.001 1
 * prepared the training data of the fit that wrote it ... Give --wdbfile
 * on the command line ...", so pot2.tadah had the record "DBFILE ...",
 * "OUTFILE pot2.tadah" and the WEIGHTS of pot4.tadah; outer.train's
 * WDBFILE likewise; old.train stopped with "Exceeded capacity for key:
 * EFILTER" (exit 1: the potential's two values were there).
 * Before the fix that states the advice in the impersonal voice: the
 * WARNING of old.train read "Give --ffilter on the command line, or an
 * FFILTER line in old.train, to apply it to this run": the check of that
 * WARNING fails. */
TEST_CASE("Subcommand: a retrain that includes a potential with CONFIG "
          "applies the keys of its own lines that prepare the training data",
          "[TadahCLI][train][retrain][dataset-keys]") {
  ScratchCwd cwd("dataset_keys_task_retrain");
  std::ofstream("a.tadah") << retrain_dataset();
  std::ofstream("b.tadah") << dimers_with_energy("-1.1");
  std::ofstream("c.tadah") << dimers_with_energy("-0.9");
  std::ofstream("config") << dataset_keys_config("a.tadah b.tadah",
                                                 "WDBFILE 1 0.5\n");
  {
    const CliRun t = run_tadah({"tadah", "train", "-c", "config"});
    INFO("train: status " << t.status << "\n" << reported(t));
    REQUIRE(t.status == 0);
  }
  const std::string pot = slurp("pot.tadah");
  INFO("pot.tadah:\n" << pot);
  const std::string datasets = "DBFILE a.tadah b.tadah c.tadah";
  const auto run_config = [](const std::string& file,
                              const std::string& body) {
    std::ofstream(file) << body;
    return run_tadah({"tadah", "train", "-c", file});
  };
  {
    const CliRun r = run_config("run.train", "CONFIG pot.tadah\n" +
                                                 datasets +
                                                 "\nWDBFILE 1 0.001 1\n"
                                                 "OUTFILE pot2.tadah\n");
    const std::string rep = reported(r);
    INFO("-c run.train: status " << r.status << "\n" << rep);
    REQUIRE(r.status == 0);
    CHECK_FALSE(contains(rep, "is a potential"));
    const std::string pot2 = slurp("pot2.tadah");
    INFO("pot2.tadah:\n" << pot2);
    CHECK(record_of(pot2) == std::vector<std::string>{datasets,
                                                      "WDBFILE 1 0.001 1",
                                                      "OUTFILE pot2.tadah"});
  }
  {
    const CliRun r = run_config("none.train", "CONFIG pot.tadah\n" +
                                                  datasets +
                                                  "\nOUTFILE pot4.tadah\n");
    INFO("-c none.train: status " << r.status << "\n" << reported(r));
    REQUIRE(r.status == 0);
  }
  std::ofstream("retrain.train") << pot << datasets << "\n";
  {
    const CliRun t = run_tadah({"tadah", "train", "-c", "retrain.train", "-o",
                                "pot3.tadah", "--wdbfile", "1", "0.001", "1"});
    INFO("train -c retrain.train --wdbfile: status " << t.status << "\n"
                                                     << reported(t));
    REQUIRE(t.status == 0);
  }
  const std::vector<std::string> w2 =
      line_tokens(slurp("pot2.tadah"), "WEIGHTS");
  REQUIRE(w2.size() > 1u);
  CHECK(w2 == line_tokens(slurp("pot3.tadah"), "WEIGHTS"));
  CHECK(w2 != line_tokens(slurp("pot4.tadah"), "WEIGHTS"));
  std::ofstream("outer.train") << "CONFIG pot.tadah\n" << datasets
                               << "\nWDBFILE 1 0.001 1\n";
  {
    const CliRun t =
        run_tadah({"tadah", "train", "-c", "outer.train", "-o", "pot6.tadah"});
    const std::string rep = reported(t);
    INFO("train -c outer.train: status " << t.status << "\n" << rep);
    REQUIRE(t.status == 0);
    CHECK_FALSE(contains(rep, "is a potential"));
    const std::string pot6 = slurp("pot6.tadah");
    INFO("pot6.tadah:\n" << pot6);
    CHECK(line_tokens(pot6, "WEIGHTS") == w2);
    CHECK(record_of(pot6) == std::vector<std::string>{datasets,
                                                      "WDBFILE 1 0.001 1",
                                                      "OUTFILE pot6.tadah"});
  }
  std::ofstream("old.tadah") << without_record(pot)
                             << "WDBFILE 1 0.5\nEFILTER 5 6\nFFILTER 0.001\n"
                                "ZERO_COM_FORCE false\n";
  {
    const CliRun r = run_config(
        "old.train", "ZERO_COM_FORCE true\nCONFIG old.tadah\n" + datasets +
                         "\nWDBFILE 1 0.001 1\nEFILTER -0.7 -0.3\n"
                         "OUTFILE pot5.tadah\n");
    const std::string rep = reported(r);
    INFO("-c old.train: status " << r.status << "\n" << rep);
    REQUIRE(r.status == 0);
    CHECK(contains(rep,
                   "tadah train: old.tadah, which the configuration file "
                   "old.train includes with CONFIG, is a potential (it has "
                   "WEIGHTS): its FFILTER 0.001 prepared the training data of "
                   "the fit that wrote it, not of this run, and is ignored "
                   "(tadah keeps such keys in a potential's training record, "
                   "not as key lines). This run takes its own FFILTER from "
                   "--ffilter on the command line, or from an FFILTER line in "
                   "old.train (the Training page of the documentation, "
                   "Retraining from a potential)."));
    const std::string pot5 = slurp("pot5.tadah");
    INFO("pot5.tadah:\n" << pot5);
    CHECK(record_of(pot5) ==
          std::vector<std::string>{datasets, "EFILTER -0.7 -0.3",
                                   "ZERO_COM_FORCE true", "WDBFILE 1 0.001 1",
                                   "OUTFILE pot5.tadah"});
  }
}

/* Pins: `tadah train -c` reads a configuration as the concatenation of its
 * lines, a CONFIG include in place of its line. inc.train is the pair
 * configuration of the [retrain] cases on b.tadah (its line 13 is "BIAS
 * false"):
 *   - run.train "BIAS true", "CONFIG inc.train" exits 1 with "Key BIAS is
 *     set on line 1 of run.train and on line 13 of inc.train (CONFIG on line
 *     2 of run.train): a key of one value is set once in a configuration
 *     file and the files it includes with CONFIG", and writes no pot.tadah;
 *   - below.train "CONFIG inc.train", "BIAS true" exits 1 with "Key BIAS is
 *     set on line 13 of inc.train (CONFIG on line 1 of below.train) and on
 *     line 2 of below.train: ...";
 *   - two.train "DBFILE a.tadah", "CONFIG inc.train", "OUTFILE two.tadah"
 *     exits 0, and the training record of two.tadah lists "DBFILE a.tadah
 *     b.tadah" (both datasets, in line order), "VERBOSE 0" (inc.train's)
 *     and "OUTFILE two.tadah".
 * Before the fix: run.train trained with the include's BIAS false and
 * below.train with BIAS true (exit 0, a potential written), and two.tadah
 * recorded "DBFILE b.tadah": the include's DBFILE replaced a.tadah without
 * a message. */
TEST_CASE("Subcommand: train -c reads a CONFIG include in place of its line: "
          "a key of one value set twice is refused, DBFILE lines add up",
          "[TadahCLI][train][config-include]") {
  ScratchCwd cwd("config_includes");
  std::ofstream("a.tadah") << retrain_dataset();
  std::ofstream("b.tadah") << dimers_with_energy("-1.1");
  std::ofstream("inc.train") << dataset_keys_config("b.tadah", "");
  const std::string once =
      ": a key of one value is set once in a configuration file and the "
      "files it includes with CONFIG";
  {
    std::ofstream("run.train") << "BIAS true\nCONFIG inc.train\n";
    const CliRun r = run_tadah({"tadah", "train", "-c", "run.train"});
    const std::string rep = reported(r);
    INFO("-c run.train: status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    CHECK(contains(rep, "Key BIAS is set on line 1 of run.train and on line "
                        "13 of inc.train (CONFIG on line 2 of run.train)" +
                            once));
    CHECK_FALSE(std::filesystem::exists("pot.tadah"));
  }
  {
    std::ofstream("below.train") << "CONFIG inc.train\nBIAS true\n";
    const CliRun r = run_tadah({"tadah", "train", "-c", "below.train"});
    const std::string rep = reported(r);
    INFO("-c below.train: status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    CHECK(contains(rep, "Key BIAS is set on line 13 of inc.train (CONFIG on "
                        "line 1 of below.train) and on line 2 of below.train" +
                            once));
    CHECK_FALSE(std::filesystem::exists("pot.tadah"));
  }
  std::ofstream("two.train") << "DBFILE a.tadah\nCONFIG inc.train\n"
                                "OUTFILE two.tadah\n";
  const CliRun r = run_tadah({"tadah", "train", "-c", "two.train"});
  INFO("-c two.train: status " << r.status << "\n" << reported(r));
  REQUIRE(r.status == 0);
  const std::string two = slurp("two.tadah");
  INFO("two.tadah:\n" << two);
  CHECK(record_of(two) == std::vector<std::string>{"DBFILE a.tadah b.tadah",
                                                   "VERBOSE 0",
                                                   "OUTFILE two.tadah"});
}

/* A retrain that includes a potential with CONFIG reads a line of the
 * including file of a key the potential's own lines set as a second line
 * of that key. pot.tadah is `tadah train` of the pair configuration of the
 * [retrain] cases on a.tadah (D2_BP, one descriptor; RCUT2B 4.5 on line
 * <r>, BIAS false, MODEL M_BLR BF_Linear):
 *   - bias.train "BIAS true", "CONFIG pot.tadah", "DBFILE a.tadah" exits 1
 *     with "Key BIAS is set on line 1 of bias.train and on line <n> of
 *     pot.tadah (CONFIG on line 2 of bias.train): a key of one value is set
 *     once in a configuration file and the files it includes with CONFIG; a
 *     potential's value is changed by editing its line in a copy of the
 *     potential (the Training page of the documentation, Retraining from a
 *     potential)", <n> the line of BIAS in pot.tadah, and writes no
 *     potential;
 *   - model.train "CONFIG pot.tadah", "DBFILE a.tadah", "MODEL M_BLR
 *     BF_Linear" exits 1 with "Too many values for key MODEL on line 3 of
 *     model.train: MODEL takes at most 3, and its lines read so far give 4,
 *     with line <m> of pot.tadah (CONFIG on line 1 of model.train) before
 *     it; a potential's value is changed ...", <m> the line of MODEL in
 *     pot.tadah;
 *   - larger.train "RCUT2B 5.0", "CONFIG pot.tadah", "DBFILE a.tadah",
 *     "OUTFILE larger.tadah" exits 1 with "TYPE2B names 1 descriptor (D2_BP)
 *     but RCUT2B has 2 values, set on line 1 of larger.train and on line <r>
 *     of pot.tadah (CONFIG on line 2 of larger.train): each descriptor takes
 *     one RCUT2B value, in the order of the TYPE2B lines" and writes no
 *     potential: the line is read as one of a training configuration;
 *   - smaller.train "CONFIG pot.tadah", "DBFILE a.tadah", "RCUT2B 4.0",
 *     "OUTFILE smaller.tadah" exits 1 with "... set on line <r> of pot.tadah
 *     (CONFIG on line 1 of smaller.train) and on line 3 of smaller.train:
 *     ..." and writes no potential.
 * Before the fix: larger.train exited 0 with "WARNING: Part of RCUT2B (4.5)
 * is ignored: TYPE2B D2_BP takes one RCUT2B value, and its cutoff is the
 * largest, 5.0; the potential works unchanged with RCUT2B 5.0 alone" and
 * wrote RCUT2B 5 (the largest cutoff was the model's), and smaller.train
 * exited 0 with the potential's RCUT2B 4.5, the line ignored with a
 * WARNING. Until the refusals of the parser named the lines of the key
 * before a line and the way to change a potential's value, the BIAS
 * refusal ended at the rule and the MODEL one at "... read so far give 4".
 */
TEST_CASE("Subcommand: a retrain that includes a potential with CONFIG reads "
          "a line of the potential's keys as a second line: BIAS, MODEL and "
          "RCUT2B refused",
          "[TadahCLI][train][retrain][config-include]") {
  ScratchCwd cwd("config_include_retrain_keys");
  std::ofstream("a.tadah") << retrain_dataset();
  std::ofstream("config") << dataset_keys_config("a.tadah", "");
  {
    const CliRun t = run_tadah({"tadah", "train", "-c", "config"});
    INFO("train: status " << t.status << "\n" << reported(t));
    REQUIRE(t.status == 0);
  }
  const std::string pot = slurp("pot.tadah");
  INFO("pot.tadah:\n" << pot);
  const std::vector<std::string> rcut = line_tokens(pot, "RCUT2B");
  REQUIRE(rcut.size() == 2u);
  // the number of the line of @p key in pot.tadah
  const auto line_of = [&pot](const std::string& key) {
    std::istringstream in(pot);
    std::size_t n = 0, at = 0;
    for (std::string l; std::getline(in, l);) {
      ++n;
      std::istringstream ls(l);
      std::string k;
      if (ls >> k && k == key) at = n;
    }
    return at;
  };
  const std::size_t bias_line = line_of("BIAS");
  REQUIRE(bias_line > 0u);
  const std::size_t model_line = line_of("MODEL");
  REQUIRE(model_line > 0u);
  const std::string copy_advice =
      "; a potential's value is changed by editing its line in a copy of the "
      "potential (the Training page of the documentation, Retraining from a "
      "potential)";
  const auto run_config = [](const std::string& file,
                             const std::string& body) {
    std::ofstream(file) << body;
    return run_tadah({"tadah", "train", "-c", file});
  };
  {
    const CliRun r =
        run_config("bias.train", "BIAS true\nCONFIG pot.tadah\nDBFILE a.tadah\n"
                                 "OUTFILE bias.tadah\n");
    const std::string rep = reported(r);
    INFO("-c bias.train: status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    CHECK(contains(rep, "Key BIAS is set on line 1 of bias.train and on line " +
                            std::to_string(bias_line) +
                            " of pot.tadah (CONFIG on line 2 of bias.train): "
                            "a key of one value is set once in a "
                            "configuration file and the files it includes "
                            "with CONFIG" + copy_advice));
    CHECK_FALSE(std::filesystem::exists("bias.tadah"));
  }
  {
    const CliRun r = run_config("model.train",
                                "CONFIG pot.tadah\nDBFILE a.tadah\n"
                                "MODEL M_BLR BF_Linear\nOUTFILE model.tadah\n");
    const std::string rep = reported(r);
    INFO("-c model.train: status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    CHECK(contains(rep, "Too many values for key MODEL on line 3 of "
                        "model.train: MODEL takes at most 3, and its lines "
                        "read so far give 4, with line " +
                            std::to_string(model_line) +
                            " of pot.tadah (CONFIG on line 1 of model.train) "
                            "before it" + copy_advice));
    CHECK_FALSE(std::filesystem::exists("model.tadah"));
  }
  const std::size_t rcut_line = line_of("RCUT2B");
  REQUIRE(rcut_line > 0u);
  const std::string pot_rcut =
      "line " + std::to_string(rcut_line) + " of pot.tadah";
  const std::string counts =
      "TYPE2B names 1 descriptor (D2_BP) but RCUT2B has 2 values, set on ";
  const std::string one =
      ": each descriptor takes one RCUT2B value, in the order of the TYPE2B "
      "lines";
  {
    const CliRun r = run_config("larger.train",
                                "RCUT2B 5.0\nCONFIG pot.tadah\nDBFILE a.tadah\n"
                                "OUTFILE larger.tadah\n");
    const std::string rep = reported(r);
    INFO("-c larger.train: status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    CHECK(contains(rep, counts + "line 1 of larger.train and on " + pot_rcut +
                            " (CONFIG on line 2 of larger.train)" + one));
    CHECK_FALSE(std::filesystem::exists("larger.tadah"));
  }
  {
    const CliRun r = run_config("smaller.train",
                                "CONFIG pot.tadah\nDBFILE a.tadah\nRCUT2B 4.0\n"
                                "OUTFILE smaller.tadah\n");
    const std::string rep = reported(r);
    INFO("-c smaller.train: status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    CHECK(contains(rep, counts + pot_rcut +
                            " (CONFIG on line 1 of smaller.train) and on line "
                            "3 of smaller.train" + one));
    CHECK_FALSE(std::filesystem::exists("smaller.tadah"));
  }
}

/* A configuration file of a command that writes one file takes one
 * OUTFILE value, as the option -o does, over its lines and the files they
 * include with CONFIG. inc.train is the pair configuration of the
 * [retrain] cases on b.tadah with "OUTFILE b_out.tadah" on its last line,
 * <o>; plain.train is that configuration without it:
 *   - out.train "OUTFILE a_out.tadah", "CONFIG inc.train": `tadah train -c`
 *     exits 1 with "Key OUTFILE is set on line 1 of out.train and on line
 *     <o> of inc.train (CONFIG on line 2 of out.train): a key of one value
 *     is set once in a configuration file and the files it includes with
 *     CONFIG" and writes neither file;
 *   - one.train "OUTFILE a_out.tadah b_out.tadah", "CONFIG plain.train":
 *     `tadah train -c` exits 1 with "Too many values for key OUTFILE on line
 *     1 of one.train: OUTFILE of tadah train takes at most 1, and its lines
 *     read so far give 2" and writes neither file, and `tadah analysis
 *     suggest_bounds -c` with "... OUTFILE of tadah analysis suggest_bounds
 *     takes at most 1, ...".
 * Before the fix: each run exited 0 and wrote a_out.tadah, b_out.tadah
 * ignored without a message (Tadah! 1.5.0 wrote b_out.tadah for out.train:
 * the include replaced the line above it). */
TEST_CASE("Subcommand: a configuration file takes one OUTFILE value over "
          "its CONFIG includes",
          "[TadahCLI][train][config-include]") {
  ScratchCwd cwd("config_include_outfile");
  std::ofstream("b.tadah") << dimers_with_energy("-1.1");
  const std::string plain = dataset_keys_config("b.tadah", "");
  std::ofstream("plain.train") << plain;
  std::ofstream("inc.train") << plain << "OUTFILE b_out.tadah\n";
  const std::size_t o = lines_in(plain) + 1;
  {
    std::ofstream("out.train") << "OUTFILE a_out.tadah\nCONFIG inc.train\n";
    const CliRun r = run_tadah({"tadah", "train", "-c", "out.train"});
    const std::string rep = reported(r);
    INFO("-c out.train: status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    CHECK(contains(rep, "Key OUTFILE is set on line 1 of out.train and on "
                        "line " + std::to_string(o) +
                            " of inc.train (CONFIG on line 2 of out.train): a "
                            "key of one value is set once in a configuration "
                            "file and the files it includes with CONFIG"));
    CHECK_FALSE(std::filesystem::exists("a_out.tadah"));
    CHECK_FALSE(std::filesystem::exists("b_out.tadah"));
  }
  std::ofstream("one.train")
      << "OUTFILE a_out.tadah b_out.tadah\nCONFIG plain.train\n";
  for (const std::vector<std::string> command :
       {std::vector<std::string>{"train"},
        std::vector<std::string>{"analysis", "suggest_bounds"}}) {
    std::vector<std::string> args{"tadah"};
    std::string words = "tadah";
    for (const std::string& w : command) {
      args.push_back(w);
      words += " " + w;
    }
    args.insert(args.end(), {"-c", "one.train"});
    const CliRun r = run_tadah(args);
    const std::string rep = reported(r);
    INFO(words << " -c one.train: status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    CHECK(contains(rep, "Too many values for key OUTFILE on line 1 of "
                        "one.train: OUTFILE of " + words +
                            " takes at most 1, and its lines read so far "
                            "give 2"));
    CHECK_FALSE(std::filesystem::exists("a_out.tadah"));
    CHECK_FALSE(std::filesystem::exists("b_out.tadah"));
  }
}

/* Pins: the example of the Quick Start (Config File). common.train has
 * "DBFILE eos.tadahdb", "BIAS false"; run.train "CONFIG common.train",
 * "DBFILE elastic.tadahdb", "BIAS true": `tadah train -c run.train` exits 1
 * with the message the page quotes, "Key BIAS is set on line 2 of
 * common.train (CONFIG on line 1 of run.train) and on line 3 of run.train:
 * a key of one value is set once in a configuration file and the files it
 * includes with CONFIG". An unknown key of an included file is named with
 * the CONFIG line that includes it: wrong.train "BIAS false", "CONFIG
 * bad.train", bad.train "FOOBAR 1", "Invalid key FOOBAR on line 1 of
 * bad.train (CONFIG on line 2 of wrong.train) (closest key: ".
 * Before the fix: the first passes as well; the second named bad.train
 * alone ("Invalid key FOOBAR on line 1 of bad.train (closest key: "). */
TEST_CASE("Subcommand: train -c names the lines of a key set twice and the "
          "CONFIG line of an included line, as the Quick Start shows",
          "[TadahCLI][train][config-include]") {
  ScratchCwd cwd("config_include_quickstart");
  std::ofstream("common.train") << "DBFILE eos.tadahdb\nBIAS false\n";
  std::ofstream("run.train")
      << "CONFIG common.train\nDBFILE elastic.tadahdb\nBIAS true\n";
  {
    const CliRun r = run_tadah({"tadah", "train", "-c", "run.train"});
    const std::string rep = reported(r);
    INFO("-c run.train: status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    CHECK(contains(rep, "Key BIAS is set on line 2 of common.train (CONFIG on "
                        "line 1 of run.train) and on line 3 of run.train: a "
                        "key of one value is set once in a configuration "
                        "file and the files it includes with CONFIG"));
  }
  std::ofstream("bad.train") << "FOOBAR 1\n";
  std::ofstream("wrong.train") << "BIAS false\nCONFIG bad.train\n";
  const CliRun r = run_tadah({"tadah", "train", "-c", "wrong.train"});
  const std::string rep = reported(r);
  INFO("-c wrong.train: status " << r.status << "\n" << rep);
  CHECK(r.status == 1);
  CHECK(contains(rep, "Invalid key FOOBAR on line 1 of bad.train (CONFIG on "
                      "line 2 of wrong.train) (closest key: "));
}

/* Pins: `tadah predict -F --zero-com-force` zeroes the net reference force
 * of each structure predicted: pot.tadah trained on db.tadah (the three
 * dimers, both atoms of each with the force f f f, so the net force is 2f
 * per component); the True column of forces.pred is 0 everywhere with
 * --zero-com-force and not without it, and the predicted column is the
 * same in both runs (the option changes the reference forces, not the
 * model).
 * Before the fix: passes as well (a pin: ZERO_COM_FORCE is an option of
 * tadah predict; a removal of every key that prepared the training data
 * from predict's merged context makes it fail). */
TEST_CASE("Subcommand: predict --zero-com-force zeroes the net reference "
          "force of the data predicted",
          "[TadahCLI][predict][dataset-keys]") {
  ScratchCwd cwd("dataset_keys_zero_com_force");
  std::ofstream("db.tadah") << retrain_dataset();
  std::ofstream("config") << dataset_keys_config("db.tadah", "");
  {
    const CliRun t = run_tadah({"tadah", "train", "-c", "config"});
    INFO("train: status " << t.status << "\n" << reported(t));
    REQUIRE(t.status == 0);
  }
  const auto forces = [](std::vector<std::string> extra,
                         const std::string& tag) {
    std::vector<std::string> args = {"tadah", "predict", "-p", "pot.tadah",
                                     "-d", "db.tadah", "-F"};
    args.insert(args.end(), extra.begin(), extra.end());
    const CliRun p = run_tadah(args);
    INFO(tag << ": status " << p.status << "\n" << reported(p));
    REQUIRE(p.status == 0);
    std::filesystem::rename("forces.pred", "forces_" + tag + ".pred");
  };
  forces({}, "plain");
  forces({"--zero-com-force"}, "zeroed");
  const std::vector<double> plain = column_values("forces_plain.pred", 1);
  const std::vector<double> zeroed = column_values("forces_zeroed.pred", 1);
  REQUIRE(plain.size() == 18u);  // 3 structures x 2 atoms x 3 components
  REQUIRE(zeroed.size() == plain.size());
  CHECK(std::any_of(plain.begin(), plain.end(),
                    [](double f) { return f != 0.0; }));
  for (double f : zeroed) CHECK(f == Approx(0.0).margin(1e-12));
  CHECK(column_values("forces_plain.pred", 2) ==
        column_values("forces_zeroed.pred", 2));
}

#ifdef TADAH_ENABLE_NFP
/* Pins: `tadah hpo -c config.tadah --hpotarget config.hpo --validation
 * config.val` (write_hpo_potential_fixture, config.tadah with EFILTER -0.6
 * -0.4, ESHIFT_DBATOM true and WDBFILE 1 added) writes best_pot.tadah with
 * no key line of the three, its ESHIFT line (resolved on the training
 * data), and the training record "DBFILE db.tadah", "EFILTER -0.6 -0.4",
 * "ESHIFT_DBATOM true", "WDBFILE 1", "VALIDATION config.val", "HPOTARGET
 * config.hpo", "VERBOSE 0"; and `tadah hpo -c seed.tadah ...`, seed.tadah
 * being the potential `tadah train -c config.tadah` writes (the fixture's
 * own config.tadah) without its record, with the key lines "WDBFILE 1 1"
 * (two values, one dataset) and "EFILTER 5 6" (it drops every structure)
 * of an earlier version and "DBFILE db.tadah", exits 0 with best_pot.tadah,
 * the journal (hpo_run.log) having the WARNING "tadah hpo: the
 * configuration file seed.tadah is a potential (it has WEIGHTS): its
 * EFILTER 5 6 and WDBFILE 1 1 prepared the training data of the fit that
 * wrote it ...".
 * Before the fix: the three were key lines of best_pot.tadah (with
 * ESHIFT_ATOM false, WDBFILE_AUTO 0 and ZERO_COM_FORCE false, the hpo
 * task's defaults) and its record, after the opening line "(not read by
 * tadah)", listed DBFILE, VALIDATION, HPOTARGET and VERBOSE alone; the
 * seed's WDBFILE and EFILTER applied to the run's training data, and the
 * run was refused at startup ("HPOEngine error: WDBFILE size (2) does not
 * match number of datasets (1).", exit 1). */
TEST_CASE("Subcommand: hpo keeps the keys that prepared the training data in "
          "best_pot.tadah's record and ignores those of a -c potential",
          "[TadahCLI][hpo][dataset-keys][training-record]") {
  ScratchCwd cwd("dataset_keys_hpo");
  write_hpo_potential_fixture();
  const std::string config = slurp("config.tadah");
  std::ofstream("keys.tadah")
      << config << "EFILTER -0.6 -0.4\nESHIFT_DBATOM true\nWDBFILE 1\n";
  {
    const CliRun r = run_tadah({"tadah", "hpo", "-c", "keys.tadah",
                                "--hpotarget", "config.hpo", "--validation",
                                "config.val"});
    INFO("hpo: status " << r.status << "\n" << reported(r));
    REQUIRE(r.status == 0);
  }
  {
    const std::string best = slurp("best_pot.tadah");
    INFO("best_pot.tadah:\n" << best);
    CHECK(record_of(best) ==
          std::vector<std::string>{"DBFILE db.tadah", "EFILTER -0.6 -0.4",
                                   "ESHIFT_DBATOM true", "WDBFILE 1",
                                   "VALIDATION config.val",
                                   "HPOTARGET config.hpo", "VERBOSE 0"});
    for (const char* key : kDatasetKeys) {
      INFO("key " << key);
      CHECK(line_tokens(best, key).empty());
    }
    for (const char* key : kRetiredKeys) {
      INFO("key " << key);
      CHECK(line_tokens(best, key).empty());
    }
    for (const char* key : kErrorScaleKeys) {
      INFO("key " << key);
      CHECK_FALSE(line_tokens(best, key).empty());
    }
    CHECK(line_tokens(best, "ESHIFT").size() == 2u);
  }
  for (const char* f : {"best_pot.tadah", "hpo_run.log"})
    std::filesystem::remove(f);
  {
    const CliRun t = run_tadah({"tadah", "train", "-c", "config.tadah", "-o",
                                "trained.tadah"});
    INFO("train: status " << t.status << "\n" << reported(t));
    REQUIRE(t.status == 0);
  }
  std::ofstream("seed.tadah")
      << without_record(slurp("trained.tadah"))
      << "WDBFILE 1 1\nEFILTER 5 6\nDBFILE db.tadah\n";
  const CliRun r = run_tadah({"tadah", "hpo", "-c", "seed.tadah",
                              "--hpotarget", "config.hpo", "--validation",
                              "config.val"});
  INFO("hpo -c seed.tadah: status " << r.status << "\n" << reported(r));
  REQUIRE(r.status == 0);
  CHECK(std::filesystem::exists("best_pot.tadah"));
  const std::string journal = slurp("hpo_run.log");
  INFO("hpo_run.log:\n" << journal);
  CHECK(contains(journal, "tadah hpo: the configuration file seed.tadah is "
                          "a potential (it has WEIGHTS): its EFILTER 5 6 and "
                          "WDBFILE 1 1 prepared the training data of the fit "
                          "that wrote it"));
}

/* Pins: `tadah hpo -c seed.tadah --hpotarget config.hpo --validation
 * config.val` (write_hpo_potential_fixture), seed.tadah being the potential
 * `tadah train -c config.tadah` writes (no ESHIFT line) without its record,
 * with the key lines "ESHIFT_ATOM false" and "ESHIFT_DBATOM true" of an
 * earlier version and "DBFILE db.tadah", exits 0; neither toggle is read:
 * best_pot.tadah has no ESHIFT line (no shift is derived from the run's
 * data), its training record is "DBFILE db.tadah", "VALIDATION config.val",
 * "HPOTARGET config.hpo", and the journal (hpo_run.log) has the WARNING
 * "tadah hpo: the configuration file seed.tadah is a potential (it has
 * WEIGHTS): its ESHIFT_DBATOM true prepared the training data of the fit that
 * wrote it, not of this run, and is ignored ... This run takes its own
 * ESHIFT_DBATOM from --eshift-dbatom on the command line, or from an
 * ESHIFT_DBATOM line in a configuration file that includes the potential with
 * CONFIG ..." (ESHIFT_ATOM false is at its default).
 * Before the fix: the toggles of a potential without an ESHIFT line were
 * read: ESHIFT_DBATOM derived a shift (best_pot.tadah had an ESHIFT line),
 * the record listed "ESHIFT_ATOM false" and "ESHIFT_DBATOM true" as the
 * run's, and no WARNING named the toggle: the ESHIFT, record and WARNING
 * checks fail.
 * Before the fix that states the advice in the impersonal voice: the journal
 * read "Give --eshift-dbatom on the command line, or an ESHIFT_DBATOM line in
 * a configuration file that includes the potential with CONFIG, to apply it
 * to this run": the journal check fails. */
TEST_CASE("Subcommand: hpo -c reads no ESHIFT_ATOM or ESHIFT_DBATOM line of "
          "a seed potential",
          "[TadahCLI][hpo][dataset-keys][training-record]") {
  ScratchCwd cwd("dataset_keys_hpo_seed_toggles");
  write_hpo_potential_fixture();
  {
    const CliRun t = run_tadah({"tadah", "train", "-c", "config.tadah", "-o",
                                "trained.tadah"});
    INFO("train: status " << t.status << "\n" << reported(t));
    REQUIRE(t.status == 0);
  }
  const std::string trained = slurp("trained.tadah");
  REQUIRE(line_tokens(trained, "ESHIFT").empty());
  std::ofstream("seed.tadah")
      << without_record(trained)
      << "ESHIFT_ATOM false\nESHIFT_DBATOM true\nDBFILE db.tadah\n";
  const CliRun r = run_tadah({"tadah", "hpo", "-c", "seed.tadah",
                              "--hpotarget", "config.hpo", "--validation",
                              "config.val"});
  const std::string rep = reported(r);
  INFO("hpo -c seed.tadah: status " << r.status << "\n" << rep);
  REQUIRE(r.status == 0);
  const std::string journal = slurp("hpo_run.log");
  INFO("hpo_run.log:\n" << journal);
  CHECK(contains(journal, "tadah hpo: the configuration file seed.tadah is "
                          "a potential (it has WEIGHTS): its ESHIFT_DBATOM "
                          "true prepared the training data of the fit that "
                          "wrote it, not of this run, and is ignored"));
  CHECK(contains(journal, "This run takes its own ESHIFT_DBATOM from "
                          "--eshift-dbatom on the command line, or from an "
                          "ESHIFT_DBATOM line in a configuration file that "
                          "includes the potential with CONFIG"));
  const std::string best = slurp("best_pot.tadah");
  INFO("best_pot.tadah:\n" << best);
  CHECK(line_tokens(best, "ESHIFT").empty());
  CHECK(record_of(best) ==
        std::vector<std::string>{"DBFILE db.tadah", "VALIDATION config.val",
                                 "HPOTARGET config.hpo"});
}

/* Pins: `tadah hpo -c seed.tadah` of a seed potential with the UNCERTAINTY
 * line tadah train 1.0.0 to 1.5.0 wrote into every potential ("UNCERTAINTY
 * true", the potential `tadah train -c config.tadah` writes, without its
 * record, with that line and "DBFILE db.tadah") is refused (exit 1) before any
 * evaluation, naming the line as a retired key marked as converted by
 * deleting it ("UNCERTAINTY true (line N of seed.tadah; converted by
 * deleting the line)") and its conversion, and writes no best_pot.tadah.
 * Before the fix: the line was not read (tadah hpo takes no UNCERTAINTY)
 * and the run exited 0. */
TEST_CASE("Subcommand: hpo -c refuses the UNCERTAINTY line of a seed "
          "potential",
          "[TadahCLI][hpo][uncertainty]") {
  ScratchCwd cwd("hpo_seed_uncertainty");
  write_hpo_potential_fixture();
  {
    const CliRun t = run_tadah({"tadah", "train", "-c", "config.tadah", "-o",
                                "trained.tadah"});
    INFO("train: status " << t.status << "\n" << reported(t));
    REQUIRE(t.status == 0);
  }
  std::ofstream("seed.tadah")
      << without_record(slurp("trained.tadah"))
      << "UNCERTAINTY true\nDBFILE db.tadah\n";
  const CliRun r = run_tadah({"tadah", "hpo", "-c", "seed.tadah",
                              "--hpotarget", "config.hpo", "--validation",
                              "config.val"});
  const std::string rep = reported(r) + slurp("hpo_run.log");
  INFO("hpo -c seed.tadah: status " << r.status << "\n" << rep);
  CHECK(r.status == 1);
  CHECK(contains(rep, "UNCERTAINTY true (line "));
  CHECK(contains(rep, " of seed.tadah; converted by deleting the line)"));
  CHECK(contains(rep, "Deleting the option, or the UNCERTAINTY line, "
                      "converts it"));
  CHECK_FALSE(std::filesystem::exists("best_pot.tadah"));
}

/* Pins: the documented route from a refit to tadah hpo says how to give a
 * DM_REAM potential under EMBF0 ZERO a per-atom reference energy, and the
 * advice works. On the outputs of a dense refit of the small Ta setfl
 * (pot.tadah: DM_REAM, EMBF0 ZERO, BIAS false, no ESHIFT, every weight
 * pinned), with config.train being pot.tadah plus "DBFILE db.tadah" (three
 * Ta dimers, retrain_dataset) and t.hpotarget refit.hpotarget with MAXEVAL
 * 1:
 *   - refit.hpotarget begins "# Starter target file of tadah hpo, written
 *     by tadah refit. It runs with:" and has the line "# DFT totals, tadah
 *     hpo takes --eshift-atom or --eshift-dbatom (an" and no "add
 *     ESHIFT_ATOM" (it began "# HPO starter target written by `tadah
 *     refit`. Run:" and the line read "# DFT totals) give tadah hpo ...");
 *   - `tadah train -c config.train -o p2.tadah` exits 0 with the HINT
 *     "WARNING: HINT: EMBF0 ZERO fixes the DM_REAM isolated-atom energy at
 *     0 ... For absolute reference energies, such as DFT totals, ESHIFT, or
 *     ESHIFT_ATOM or ESHIFT_DBATOM for the run (--eshift-atom,
 *     --eshift-dbatom: a potential's own line of them is not read), gives
 *     that constant, all compatible with EAMFAST. BIAS true also adds ..."
 *     (it read "If the reference energies are absolute (e.g. DFT totals),
 *     set ESHIFT or give the run ..., or BIAS true ...");
 *   - `tadah hpo -c config.train --hpotarget t.hpotarget --validation
 *     config.val` exits 0 with the journal HINT "EMBF0 ZERO fixes ... For
 *     absolute reference energies, such as DFT totals, ESHIFT, or
 *     ESHIFT_ATOM or ESHIFT_DBATOM for the run (--eshift-atom,
 *     --eshift-dbatom: a potential's own line of them is not read), gives
 *     that constant, all compatible with EAMFAST. ..." (it was "If
 *     reference energies are absolute (e.g. DFT totals), set ESHIFT or give
 *     the run ESHIFT_ATOM or ESHIFT_DBATOM (...), all compatible with
 *     EAMFAST.");
 *   - with --eshift-dbatom both exit 0 with no EMBF0 HINT, and p3.tadah and
 *     best_pot.tadah have an ESHIFT line and "ESHIFT_DBATOM true" in their
 *     training record (a pin: the run's own toggle applied before the fix
 *     too).
 * Before the fix: the hpotarget said "# DFT totals) add ESHIFT_ATOM,
 * ESHIFT_DBATOM or ESHIFT to <config>." and both HINTs "set ESHIFT_ATOM,
 * ESHIFT_DBATOM or ESHIFT": a toggle line added to config.train, a
 * potential, is not read, so that advice applies no shift; the text CHECKs
 * fail. */
TEST_CASE("Subcommand: refit, then train -c and hpo -c of the refit "
          "potential: the EMBF0 ZERO advice names the run's toggles",
          "[TadahCLI][refit][hpo][embf0][dataset-keys]") {
  ScratchCwd cwd("refit_embf0_eshift_advice");
  { std::ofstream os("eam_small.alloy"); os << small_ta_setfl(); }
  std::ofstream("config.refit")
      << "EAM eam_small.alloy\nRHOFORM SIGNED\nREFIT_MODE dense\n"
         "REFIT_BASIS Blip\nREFIT_LEVEL coarse\nREFIT_MAX_ESCALATIONS 0\n";
  {
    const CliRun fit = run_tadah({"tadah", "refit", "-c", "config.refit"});
    INFO("refit: status " << fit.status << "\n" << reported(fit));
    REQUIRE(fit.status == 0);
  }
  const std::string target = slurp("refit.hpotarget");
  INFO("refit.hpotarget:\n" << target);
  CHECK(contains(target, "# DFT totals, tadah hpo takes --eshift-atom or "
                         "--eshift-dbatom (an\n"));
  CHECK(target.rfind("# Starter target file of tadah hpo, written by tadah "
                     "refit. It runs with:\n", 0) == 0);
  CHECK_FALSE(contains(target, "e.g."));
  CHECK_FALSE(contains(target, "add ESHIFT_ATOM"));
  REQUIRE(contains(target, "MAXEVAL 200\n"));
  std::ofstream("t.hpotarget")
      << replace_key_line(target, "MAXEVAL", "    MAXEVAL 1");
  const std::string pot = slurp("pot.tadah");
  REQUIRE(line_tokens(pot, "ESHIFT").empty());
  REQUIRE(line_tokens(pot, "REFERENCE") ==
          std::vector<std::string>{"REFERENCE", "ZERO"});
  std::ofstream("db.tadah") << retrain_dataset();
  std::ofstream("val.tadah") << retrain_dataset();
  std::ofstream("config.val") << "DBFILE val.tadah\n";
  std::ofstream("config.train") << pot << "DBFILE db.tadah\n";
  const std::string hint = "REFERENCE ZERO fixes the DM_Embed isolated-atom "
                           "energy at 0";
  const std::string toggles =
      "ESHIFT, or ESHIFT_ATOM or ESHIFT_DBATOM for the run (--eshift-atom, "
      "--eshift-dbatom: a potential's own line of them is not read), gives "
      "that constant, all compatible with EAMFAST";
  {
    const CliRun t = run_tadah({"tadah", "train", "-c", "config.train", "-o",
                                "p2.tadah"});
    const std::string rep = reported(t);
    INFO("train: status " << t.status << "\n" << rep);
    REQUIRE(t.status == 0);
    CHECK(contains(rep, "HINT: " + hint));
    CHECK(contains(rep, "For absolute reference energies, such as DFT "
                        "totals, " + toggles +
                        ". BIAS true also adds a per-atom constant, but its "
                        "intercept columns, one per ATOM species, precede "
                        "the descriptor weights (FIXINDEX/FIXWEIGHT indices "
                        "shift) and it disables EAMFAST.\n"));
    CHECK_FALSE(contains(rep, "e.g."));
  }
  {
    const CliRun t = run_tadah({"tadah", "train", "-c", "config.train", "-o",
                                "p3.tadah", "--eshift-dbatom"});
    const std::string rep = reported(t);
    INFO("train --eshift-dbatom: status " << t.status << "\n" << rep);
    REQUIRE(t.status == 0);
    CHECK_FALSE(contains(rep, hint));
    const std::string p3 = slurp("p3.tadah");
    INFO("p3.tadah:\n" << p3);
    CHECK(line_tokens(p3, "ESHIFT").size() == 2u);  // ESHIFT and one value
    const std::vector<std::string> rec = record_of(p3);
    CHECK(std::find(rec.begin(), rec.end(), "ESHIFT_DBATOM true") !=
          rec.end());
  }
  {
    const CliRun h = run_tadah({"tadah", "hpo", "-c", "config.train",
                                "--hpotarget", "t.hpotarget", "--validation",
                                "config.val"});
    INFO("hpo: status " << h.status << "\n" << reported(h));
    REQUIRE(h.status == 0);
    const std::string journal = slurp("hpo_run.log");
    INFO("hpo_run.log:\n" << journal);
    CHECK(contains(journal, hint));
    CHECK(contains(journal, "For absolute reference energies, such as DFT "
                            "totals, ESHIFT, or ESHIFT_ATOM or ESHIFT_DBATOM "
                            "for the run (--eshift-atom, --eshift-dbatom: a "
                            "potential's own line of them is not read), "
                            "gives that constant, all compatible with "
                            "EAMFAST. BIAS true"));
  }
  const CliRun h = run_tadah({"tadah", "hpo", "-c", "config.train",
                              "--hpotarget", "t.hpotarget", "--validation",
                              "config.val", "--eshift-dbatom"});
  INFO("hpo --eshift-dbatom: status " << h.status << "\n" << reported(h));
  REQUIRE(h.status == 0);
  const std::string journal = slurp("hpo_run.log");
  INFO("hpo_run.log:\n" << journal);
  CHECK_FALSE(contains(journal, hint));
  const std::string best = slurp("best_pot.tadah");
  INFO("best_pot.tadah:\n" << best);
  CHECK(line_tokens(best, "ESHIFT").size() == 2u);
  const std::vector<std::string> rec = record_of(best);
  CHECK(std::find(rec.begin(), rec.end(), "ESHIFT_DBATOM true") != rec.end());
}

/* Pins: `tadah hpo -c run.train --hpotarget config.hpo --validation
 * config.val`, run.train being "CONFIG trained.tadah" (the potential
 * `tadah train -c config.tadah` writes, write_hpo_potential_fixture),
 * "DBFILE db.tadah", "WDBFILE 2" and "EFILTER -0.6 -0.4" (which keeps the
 * three dimers), exits 0; best_pot.tadah's training record is "DBFILE
 * db.tadah", "EFILTER -0.6 -0.4", "WDBFILE 2", "VALIDATION config.val",
 * "HPOTARGET config.hpo" (the configuration file's keys applied to the run
 * and recorded), and neither the run's output nor the journal says the
 * configuration file is a potential.
 * Before the fix: hpo_run removed every key of a configuration
 * with WEIGHTS that was not a command-line option: the journal had the
 * WARNING "tadah hpo: ... is a potential (it has WEIGHTS): its EFILTER -0.6
 * -0.4 and WDBFILE 2 prepared the training data of the fit that wrote it
 * ...", and the record had no EFILTER and no WDBFILE. */
TEST_CASE("Subcommand: hpo seeded from a potential with CONFIG applies the "
          "configuration file's keys that prepare the training data",
          "[TadahCLI][hpo][dataset-keys][training-record]") {
  ScratchCwd cwd("dataset_keys_hpo_task");
  write_hpo_potential_fixture();
  {
    const CliRun t = run_tadah({"tadah", "train", "-c", "config.tadah", "-o",
                                "trained.tadah"});
    INFO("train: status " << t.status << "\n" << reported(t));
    REQUIRE(t.status == 0);
  }
  std::ofstream("run.train") << "CONFIG trained.tadah\n"
                                "DBFILE db.tadah\n"
                                "WDBFILE 2\n"
                                "EFILTER -0.6 -0.4\n";
  const CliRun r = run_tadah({"tadah", "hpo", "-c", "run.train",
                              "--hpotarget", "config.hpo", "--validation",
                              "config.val"});
  const std::string rep = reported(r);
  INFO("-c run.train: status " << r.status << "\n" << rep);
  REQUIRE(r.status == 0);
  const std::string journal = slurp("hpo_run.log");
  INFO("hpo_run.log:\n" << journal);
  CHECK_FALSE(contains(rep, "is a potential"));
  CHECK_FALSE(contains(journal, "is a potential"));
  const std::string best = slurp("best_pot.tadah");
  INFO("best_pot.tadah:\n" << best);
  CHECK(record_of(best) ==
        std::vector<std::string>{"DBFILE db.tadah", "EFILTER -0.6 -0.4",
                                 "WDBFILE 2", "VALIDATION config.val",
                                 "HPOTARGET config.hpo"});
}
#endif  // TADAH_ENABLE_NFP

/* ---- an older potential's run settings (completing the training record
 * rule): VALIDATION, HPOTARGET, VERBOSE, NUMERIC, OUTFILE, AUDIT,
 * AUDIT_SAMPLE and HEALTH_LOG were settings of the run that wrote it; a
 * potential of an earlier version has them as key lines (a 1.5.x
 * best_pot.tadah has HPOTARGET, VERBOSE and NUMERIC, and VALIDATION when
 * its run had a validation file). No run that reads the potential, and no
 * program that loads it, takes them. */

/* Pins: `tadah train -c old.train`, old.train being the potential `tadah
 * train -c config.tadah -o trained.tadah` writes (retrain_config) without its
 * record, with "DBFILE db.tadah" and the key lines VALIDATION config.val,
 * HPOTARGET config.hpo, VERBOSE 2, NUMERIC 12, OUTFILE old_out.tadah, AUDIT
 * warn, AUDIT_SAMPLE 2 and HEALTH_LOG full of an earlier version's run, exits
 * 0 and writes pot.tadah (the default -o), not old_out.tadah; prints no INFO
 * line (its own default verbosity) and no NUMERIC WARNING ("NUMERIC 12
 * limits"); pot.tadah's WEIGHTS are those of the same retrain without the
 * eight lines (bare.train -o bare.tadah), token for token (18 digits); its
 * training record is "DBFILE db.tadah"; and the WARNING names VERBOSE 2,
 * NUMERIC 12, OUTFILE old_out.tadah, AUDIT warn and AUDIT_SAMPLE 2 ("were
 * settings of the run that wrote it, not of this run, and are ignored") with
 * the advice "This run takes its own VERBOSE, NUMERIC, OUTFILE, AUDIT or
 * AUDIT_SAMPLE from a VERBOSE, NUMERIC, OUTFILE, AUDIT or AUDIT_SAMPLE line
 * in a configuration file that includes the potential with CONFIG, or
 * VERBOSE or OUTFILE from --verbose or --outfile on the command line" --
 * not VALIDATION, HPOTARGET or HEALTH_LOG, which tadah train does not read;
 * the options are paired with the keys they set.
 * Before the fix: the eight lines were read as the run's: the retrain wrote
 * old_out.tadah at VERBOSE 2 (INFO lines) and NUMERIC 12 (the WARNING
 * "NUMERIC 12 limits ..."), so the REQUIRE on pot.tadah stops the case.
 * Before the fix that states the advice in the impersonal voice: it read
 * "Give --verbose or --outfile on the command line, or a VERBOSE, NUMERIC,
 * OUTFILE, AUDIT or AUDIT_SAMPLE line in a configuration file that includes
 * the potential with CONFIG, to apply one to this run": the check fails.
 * Before the fix that pairs the options with their keys: "... from
 * --verbose or --outfile on the command line, or from a VERBOSE, ...
 * line ...", which offered an option for NUMERIC, AUDIT and AUDIT_SAMPLE:
 * the check fails. */
TEST_CASE("Subcommand: train -c of an older potential reads none of its run "
          "settings",
          "[TadahCLI][train][retrain][training-record][run-settings]") {
  ScratchCwd cwd("old_run_settings_train");
  std::ofstream("db.tadah") << retrain_dataset();
  std::ofstream("config.tadah")
      << retrain_config("M_BLR BF_Linear", "LAMBDA 0.1\nOALGO 4\n");
  {
    const CliRun t = run_tadah({"tadah", "train", "-c", "config.tadah", "-o",
                                "trained.tadah"});
    INFO("train: status " << t.status << "\n" << reported(t));
    REQUIRE(t.status == 0);
  }
  const std::string bare =
      without_record(slurp("trained.tadah")) + "DBFILE db.tadah\n";
  std::ofstream("bare.train") << bare;
  std::ofstream("old.train") << bare
                             << "VALIDATION config.val\nHPOTARGET config.hpo\n"
                                "VERBOSE 2\nNUMERIC 12\nOUTFILE old_out.tadah\n"
                                "AUDIT warn\nAUDIT_SAMPLE 2\nHEALTH_LOG full\n";
  {
    const CliRun t = run_tadah({"tadah", "train", "-c", "bare.train", "-o",
                                "bare.tadah"});
    INFO("bare.train: status " << t.status << "\n" << reported(t));
    REQUIRE(t.status == 0);
  }
  const CliRun r = run_tadah({"tadah", "train", "-c", "old.train"});
  const std::string rep = reported(r);
  INFO("old.train: status " << r.status << "\n" << rep);
  REQUIRE(r.status == 0);
  CHECK_FALSE(std::filesystem::exists("old_out.tadah"));
  REQUIRE(std::filesystem::exists("pot.tadah"));
  const std::string pot = slurp("pot.tadah");
  INFO("pot.tadah:\n" << pot);
  CHECK_FALSE(contains(rep, "INFO:"));
  CHECK_FALSE(contains(rep, "NUMERIC 12 limits"));
  REQUIRE(line_tokens(pot, "WEIGHTS").size() == 5u);
  CHECK(line_tokens(pot, "WEIGHTS") ==
        line_tokens(slurp("bare.tadah"), "WEIGHTS"));
  CHECK(record_of(pot) == std::vector<std::string>{"DBFILE db.tadah"});
  CHECK(contains(rep, "tadah train: the configuration file old.train is a "
                      "potential (it has WEIGHTS): its VERBOSE 2, NUMERIC "
                      "12, OUTFILE old_out.tadah, AUDIT warn and "
                      "AUDIT_SAMPLE 2 were settings of the run that wrote "
                      "it, not of this run, and are ignored"));
  CHECK(contains(rep, "This run takes its own VERBOSE, NUMERIC, OUTFILE, "
                      "AUDIT or AUDIT_SAMPLE from a VERBOSE, NUMERIC, "
                      "OUTFILE, AUDIT or AUDIT_SAMPLE line in a "
                      "configuration file that includes the potential with "
                      "CONFIG, or VERBOSE or OUTFILE from --verbose or "
                      "--outfile on the command line"));
  CHECK_FALSE(contains(rep, "VALIDATION"));
  CHECK_FALSE(contains(rep, "HPOTARGET"));
  CHECK_FALSE(contains(rep, "HEALTH_LOG"));
}

/* Pins: tadah predict takes its verbosity from its own -v alone, whatever
 * VERBOSE line the potential has (a potential of an earlier version: the
 * potential retrain_config trains, without its record): with "VERBOSE 2"
 * and "EFILTER -8 -2" in p2.tadah, `tadah predict -p p2.tadah -d db.tadah`
 * (-v 1, the default) prints no INFO line; with "VERBOSE 0" and the same
 * EFILTER in p0.tadah, `tadah predict -p p0.tadah -d db.tadah -v 2` prints
 * the INFO line "tadah predict: p0.tadah: EFILTER -8 -2 prepared the
 * training data of the potential and is not applied to the data predicted".
 * Before the fix: the parse of the potential set the log level from its
 * VERBOSE line until predict merged in its own: p2.tadah's VERBOSE 2 printed
 * that INFO line at -v 1, and p0.tadah's VERBOSE 0 hid it at -v 2: both
 * checks fail. */
TEST_CASE("Subcommand: predict takes no verbosity from a potential's VERBOSE "
          "line",
          "[TadahCLI][predict][run-settings]") {
  ScratchCwd cwd("old_run_settings_predict");
  std::ofstream("db.tadah") << retrain_dataset();
  std::ofstream("config.tadah")
      << retrain_config("M_BLR BF_Linear", "LAMBDA 0.1\nOALGO 4\n");
  {
    const CliRun t = run_tadah({"tadah", "train", "-c", "config.tadah", "-o",
                                "trained.tadah"});
    INFO("train: status " << t.status << "\n" << reported(t));
    REQUIRE(t.status == 0);
  }
  const std::string body = without_record(slurp("trained.tadah"));
  std::ofstream("p2.tadah") << body << "VERBOSE 2\nEFILTER -8 -2\n";
  std::ofstream("p0.tadah") << body << "VERBOSE 0\nEFILTER -8 -2\n";
  {
    const CliRun r =
        run_tadah({"tadah", "predict", "-p", "p2.tadah", "-d", "db.tadah"});
    const std::string rep = reported(r);
    INFO("p2.tadah: status " << r.status << "\n" << rep);
    REQUIRE(r.status == 0);
    CHECK_FALSE(contains(rep, "INFO:"));
  }
  const CliRun r = run_tadah(
      {"tadah", "predict", "-p", "p0.tadah", "-d", "db.tadah", "-v", "2"});
  const std::string rep = reported(r);
  INFO("p0.tadah -v 2: status " << r.status << "\n" << rep);
  REQUIRE(r.status == 0);
  CHECK(contains(rep, "INFO: tadah predict: p0.tadah: EFILTER -8 -2 prepared "
                      "the training data of the potential and is not applied "
                      "to the data predicted"));
}

/* Pins: `tadah analysis check_bounds -c old_best.tadah`, old_best.tadah
 * being the potential `tadah train -c config.tadah` writes (retrain_config)
 * without its record, with "DBFILE db.tadah" and the key lines HPOTARGET
 * config.hpo, VALIDATION config.val, VERBOSE 2 and NUMERIC 12 of a 1.5.x
 * tadah hpo seed, does not read the seed's run settings:
 *   - without --hpotarget it exits 1, refused "tadah analysis check_bounds:
 *     no HPOTARGET: the target file is given with --hpotarget, or with an
 *     HPOTARGET line in a configuration file without WEIGHTS. old_best.tadah
 *     is a potential (it has WEIGHTS): its HPOTARGET config.hpo line was a
 *     setting of the run that wrote it and is not read.", after the WARNING
 *     "tadah analysis check_bounds: the configuration file old_best.tadah is
 *     a potential (it has WEIGHTS): its HPOTARGET config.hpo and VERBOSE 2
 *     were settings of the run that wrote it, not of this run, and are
 *     ignored (...). This run takes its own HPOTARGET or VERBOSE from
 *     --hpotarget or --verbose on the command line, or from an HPOTARGET or
 *     VERBOSE line in a configuration file that includes the potential with
 *     CONFIG (...).";
 *   - with --hpotarget config.hpo (OPTIM CGRID2B (1) 2.3 2.7) it exits 0,
 *     and the WARNING names VERBOSE 2 alone.
 * Before the fix: no WARNING named a line of old_best.tadah, and without
 * --hpotarget it was refused "analysis check_bounds: HPOTARGET is required
 * (set HPOTARGET in config or pass via the CLI).": the refusal and WARNING
 * CHECKs fail. (On v1-dev the parse stored the seed's HPOTARGET line and
 * the run used it.)
 * Before the fix that states the refusal and the advice in the impersonal
 * voice: the refusal read "tadah analysis check_bounds: no HPOTARGET: name
 * the hpotarget file with --hpotarget, or with an HPOTARGET line in a
 * configuration file without WEIGHTS. ..." and the advice "Give --hpotarget
 * or --verbose on the command line, or an HPOTARGET or VERBOSE line in a
 * configuration file that includes the potential with CONFIG, to apply one to
 * this run": the checks fail. */
TEST_CASE("Subcommand: analysis check_bounds -c of an older seed potential "
          "names its HPOTARGET line, not read, and asks for --hpotarget",
          "[TadahCLI][analysis][check_bounds][run-settings]") {
  ScratchCwd cwd("old_run_settings_check_bounds");
  std::ofstream("db.tadah") << retrain_dataset();
  std::ofstream("config.tadah")
      << retrain_config("M_BLR BF_Linear", "LAMBDA 0.1\nOALGO 4\n");
  std::ofstream("config.hpo") << "OPTIM CGRID2B (1) 2.3 2.7\n";
  {
    const CliRun t = run_tadah({"tadah", "train", "-c", "config.tadah", "-o",
                                "trained.tadah"});
    INFO("train: status " << t.status << "\n" << reported(t));
    REQUIRE(t.status == 0);
  }
  std::ofstream("old_best.tadah")
      << without_record(slurp("trained.tadah"))
      << "DBFILE db.tadah\nHPOTARGET config.hpo\nVALIDATION config.val\n"
         "VERBOSE 2\nNUMERIC 12\n";
  {
    const CliRun r = run_tadah(
        {"tadah", "analysis", "check_bounds", "-c", "old_best.tadah"});
    const std::string rep = reported(r);
    INFO("no --hpotarget: status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    CHECK(contains(rep, "tadah analysis check_bounds: no HPOTARGET: the "
                        "target file is given with --hpotarget, or with an "
                        "HPOTARGET line in a configuration file without "
                        "WEIGHTS. old_best.tadah is a potential (it has "
                        "WEIGHTS): its HPOTARGET config.hpo line was a "
                        "setting of the run that wrote it and is not read."));
    CHECK(contains(rep, "tadah analysis check_bounds: the configuration file "
                        "old_best.tadah is a potential (it has WEIGHTS): its "
                        "HPOTARGET config.hpo and VERBOSE 2 were settings of "
                        "the run that wrote it, not of this run, and are "
                        "ignored"));
    CHECK(contains(rep, "This run takes its own HPOTARGET or VERBOSE from "
                        "--hpotarget or --verbose on the command line, or "
                        "from an HPOTARGET or VERBOSE line in a configuration "
                        "file that includes the potential with CONFIG"));
  }
  const CliRun r =
      run_tadah({"tadah", "analysis", "check_bounds", "-c", "old_best.tadah",
                 "--hpotarget", "config.hpo"});
  const std::string rep = reported(r);
  INFO("--hpotarget config.hpo: status " << r.status << "\n" << rep);
  CHECK(r.status == 0);
  CHECK(contains(rep, "tadah analysis check_bounds: the configuration file "
                      "old_best.tadah is a potential (it has WEIGHTS): its "
                      "VERBOSE 2 was a setting of the run that wrote it"));
  CHECK_FALSE(contains(rep, "HPOTARGET config.hpo"));
}

#ifdef TADAH_ENABLE_NFP
/* Pins: `tadah hpo -c old.tadah --hpotarget config.hpo` (the fixture of
 * write_hpo_potential_fixture: PC_ERMSE, a validation metric), old.tadah
 * being the potential `tadah train -c config.tadah` writes without its
 * record, with "DBFILE db.tadah" and the key lines VALIDATION config.val,
 * HPOTARGET config.hpo, VERBOSE 2 and NUMERIC 12 of an earlier version's
 * best_pot.tadah:
 *   - without --validation the run does not validate on config.val: it exits
 *     1, refused "... no --validation file was given", with no
 *     best_pot.tadah, after the WARNING "tadah hpo: the configuration file
 *     old.tadah is a potential (it has WEIGHTS): its VALIDATION config.val,
 *     VERBOSE 2 and NUMERIC 12 were settings of the run that wrote it, not of
 *     this run, and are ignored (...). This run takes its own VALIDATION,
 *     VERBOSE or NUMERIC from a VALIDATION, VERBOSE or NUMERIC line in a
 *     configuration file that includes the potential with CONFIG, or
 *     VALIDATION or VERBOSE from --validation or --verbose on the command
 *     line (...)." (HPOTARGET, given on the command line, is not named; tadah
 *     hpo has no --numeric);
 *   - with --validation other.val it validates on other.val ("Validation
 *     set        : other.val" in hpo_run.log), exits 0, prints no INFO line
 *     and no NUMERIC WARNING ("NUMERIC 12 limits"), and best_pot.tadah's
 *     record is "DBFILE db.tadah", "VALIDATION other.val", "HPOTARGET
 *     config.hpo".
 * Before the fix: the four lines were the run's: without --validation the
 * run validated on config.val and exited 0 (the status CHECK fails); with
 * --validation other.val it ran at VERBOSE 2 and NUMERIC 12 and its record
 * listed "VERBOSE 2" and "NUMERIC 12": the INFO, NUMERIC and record checks
 * fail.
 * Before the fix that states the advice in the impersonal voice: it read
 * "Give --validation or --verbose on the command line, or a VALIDATION,
 * VERBOSE or NUMERIC line in a configuration file that includes the potential
 * with CONFIG, to apply one to this run": the check fails.
 * Before the fix that pairs the options with their keys: "... from
 * --validation or --verbose on the command line, or from a VALIDATION,
 * VERBOSE or NUMERIC line ...", which offered an option for NUMERIC: the
 * check fails. */
TEST_CASE("Subcommand: hpo -c of an older best_pot.tadah does not validate "
          "on its VALIDATION file unless --validation is given",
          "[TadahCLI][hpo][training-record][run-settings]") {
  ScratchCwd cwd("old_run_settings_hpo");
  write_hpo_potential_fixture();
  std::ofstream("other.val") << "DBFILE val.tadah\n";
  {
    const CliRun t = run_tadah({"tadah", "train", "-c", "config.tadah", "-o",
                                "trained.tadah"});
    INFO("train: status " << t.status << "\n" << reported(t));
    REQUIRE(t.status == 0);
  }
  std::ofstream("old.tadah")
      << without_record(slurp("trained.tadah"))
      << "DBFILE db.tadah\nVALIDATION config.val\nHPOTARGET config.hpo\n"
         "VERBOSE 2\nNUMERIC 12\n";
  {
    const CliRun r = run_tadah({"tadah", "hpo", "-c", "old.tadah",
                                "--hpotarget", "config.hpo"});
    const std::string rep = reported(r);
    INFO("without --validation: status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    CHECK(contains(rep, "no --validation file was given"));
    CHECK_FALSE(std::filesystem::exists("best_pot.tadah"));
    CHECK(contains(rep, "tadah hpo: the configuration file old.tadah is a "
                        "potential (it has WEIGHTS): its VALIDATION "
                        "config.val, VERBOSE 2 and NUMERIC 12 were settings "
                        "of the run that wrote it, not of this run, and are "
                        "ignored"));
    CHECK(contains(rep, "This run takes its own VALIDATION, VERBOSE or "
                        "NUMERIC from a VALIDATION, VERBOSE or NUMERIC line "
                        "in a configuration file that includes the potential "
                        "with CONFIG, or VALIDATION or VERBOSE from "
                        "--validation or --verbose on the command line"));
  }
  const CliRun r = run_tadah({"tadah", "hpo", "-c", "old.tadah",
                              "--hpotarget", "config.hpo", "--validation",
                              "other.val"});
  const std::string rep = reported(r);
  INFO("--validation other.val: status " << r.status << "\n" << rep);
  REQUIRE(r.status == 0);
  const std::string journal = slurp("hpo_run.log");
  INFO("hpo_run.log:\n" << journal);
  CHECK(contains(journal, "Validation set        : other.val"));
  CHECK_FALSE(contains(journal, "config.val"));
  CHECK_FALSE(contains(rep, "INFO:"));
  CHECK_FALSE(contains(rep, "NUMERIC 12 limits"));
  const std::string best = slurp("best_pot.tadah");
  INFO("best_pot.tadah:\n" << best);
  CHECK(record_of(best) ==
        std::vector<std::string>{"DBFILE db.tadah", "VALIDATION other.val",
                                 "HPOTARGET config.hpo"});
}

/* ---- [hpo-check]: tadah hpo --check -----------------------------------
 *
 * `tadah hpo --check` (or CHECK true in a configuration file) makes every
 * startup check of `tadah hpo` and prints a summary, without an evaluation
 * and without a file. Before the feature: `--check` is an unknown option
 * (CLI11 exits 109, "The following argument was not expected: --check")
 * and CHECK an unknown configuration key, so every case below fails. */
namespace {

/* write_hpo_potential_fixture()'s setup, with archives of every potential
 * (DUMP) and of every new best (BDUMP) in config.hpo: a run creates their
 * directories, a check does not. */
void write_hpo_check_fixture() {
  write_hpo_potential_fixture();
  std::ofstream("config.hpo")
      << "OPTIMIZER\nLIB NLOPT\nALGO LN_SBPLX\nMAXEVAL 1\nENDOPTIMIZER\n"
         "LOSS L2\nPC_ERMSE 0 1\nOPTIM CGRID2B (1) 2.3 2.7\n"
         "DUMP 1 pots\nBDUMP 1 best_pots\n";
}

/* The entries of the working directory, the files run_tadah() captures
 * standard output and error in left out. */
std::set<std::string> run_directory_entries() {
  std::set<std::string> out;
  for (const auto& e : std::filesystem::directory_iterator(".")) {
    const std::string name = e.path().filename().string();
    if (name.rfind("cli_", 0) == 0 &&
        (contains(name, ".stdout") || contains(name, ".stderr")))
      continue;
    out.insert(name);
  }
  return out;
}

/* The checks of a check of write_hpo_check_fixture()'s setup: status 0, the
 * summary on standard output, and no file written (`before`: the entries
 * of the working directory before the run). */
void check_hpo_check_passed(const CliRun& r,
                            const std::set<std::string>& before) {
  INFO("status " << r.status << "\n" << reported(r));
  CHECK(r.status == 0);
  CHECK(contains(r.out, "tadah hpo --check: config.hpo passed the startup "
                        "checks"));
  CHECK(contains(r.out, "Search parameters: 1"));
  CHECK(contains(r.out, "CGRID2B[1]  [2.3, 2.7]  LIN"));
  CHECK(contains(r.out, "Optimiser: STRATEGY TYPE PLAIN"));
  CHECK(contains(r.out, "STRATEGY  NLOPT:LN_SBPLX; stops: MAXEVAL 1"));
  /* Each term with its loss function (the heading named the last LOSS
   * line, "Loss terms: LOSS L2", and the rows had none). */
  CHECK(contains(r.out, "Loss terms:\n  PC_ERMSE  target 0, weight 1, loss L2\n"));
  CHECK(contains(r.out, "Data: 3 training structures, 3 validation "
                        "structures"));
  CHECK(contains(r.out, "No candidate was evaluated and no file was "
                        "written."));
  CHECK(run_directory_entries() == before);
  for (const char* f : {"hpo_run.log", "loss.tadah", "params.tadah",
                        "hpo_search_space.tsv", "best_pot.tadah", "pots",
                        "best_pots"}) {
    INFO(f);
    CHECK_FALSE(std::filesystem::exists(f));
  }
}

/* The message of `tadah hpo` on standard error: from "HPOEngine error:" to
 * the end; "" when there is none. */
std::string engine_error(const CliRun& r) {
  const auto p = r.err.find("HPOEngine error:");
  return p == std::string::npos ? std::string() : r.err.substr(p);
}

/* Whether a potential file has a key line of `key`. */
bool has_key_line(const std::string& file, const std::string& key) {
  return !line_tokens(slurp(file), key).empty();
}

}  // namespace

/* Pins: `tadah hpo --check -c config.tadah --hpotarget config.hpo
 * --validation config.val` exits 0 and prints the summary: the search
 * parameter with its box and scale (CGRID2B[1] [2.3, 2.7] LIN), the
 * optimiser with its stops (NLOPT:LN_SBPLX, MAXEVAL 1), the loss term
 * (PC_ERMSE target 0, weight 1) and the data loaded (3 training and 3
 * validation structures). The working directory is as it was: no journal,
 * no result file, no search-space sidecar, no potential, no DUMP or BDUMP
 * directory -- no evaluation was made. */
TEST_CASE("Subcommand: hpo --check passes a valid setup with a summary, "
          "without an evaluation or a file",
          "[TadahCLI][hpo][hpo-check]") {
  ScratchCwd cwd("hpo_check_valid");
  write_hpo_check_fixture();
  const auto before = run_directory_entries();
  const CliRun r = run_tadah({"tadah", "hpo", "--check", "-c", "config.tadah",
                              "--hpotarget", "config.hpo", "--validation",
                              "config.val"});
  check_hpo_check_passed(r, before);
}

/* Pins: in a configuration file without WEIGHTS, CHECK true is --check:
 * `tadah hpo -c check.cfg --hpotarget config.hpo --validation config.val`,
 * check.cfg holding "CONFIG config.tadah" and "CHECK true", exits 0 with
 * the summary and writes no file. CHECK false in such a file (run.cfg),
 * and a `tadah hpo` without --check (CHECK false by default), run: each
 * writes best_pot.tadah and its DUMP and BDUMP archives, and no potential
 * has a CHECK line (CHECK is an instruction to tadah hpo, not a key of the
 * model).
 * Before the fix: passes as well (a pin). */
TEST_CASE("Subcommand: CHECK true in a configuration file checks the setup, "
          "and a run writes no CHECK line into its potentials",
          "[TadahCLI][hpo][hpo-check]") {
  ScratchCwd cwd("hpo_check_task");
  write_hpo_check_fixture();
  std::ofstream("check.cfg") << "CONFIG config.tadah\nCHECK true\n";
  std::ofstream("run.cfg") << "CONFIG config.tadah\nCHECK false\n";
  {
    const auto before = run_directory_entries();
    const CliRun r = run_tadah({"tadah", "hpo", "-c", "check.cfg",
                                "--hpotarget", "config.hpo", "--validation",
                                "config.val"});
    check_hpo_check_passed(r, before);
  }
  const std::vector<std::vector<std::string>> runs = {
      {"tadah", "hpo", "-c", "run.cfg", "--hpotarget", "config.hpo",
       "--validation", "config.val"},
      {"tadah", "hpo", "-c", "config.tadah", "--hpotarget", "config.hpo",
       "--validation", "config.val"}};
  for (const auto& args : runs) {
    for (const char* d : {"pots", "best_pots"})
      std::filesystem::remove_all(d);
    std::filesystem::remove("best_pot.tadah");
    const CliRun r = run_tadah(args);
    INFO(args[1] << " " << args[2] << ": status " << r.status << "\n"
                 << reported(r));
    REQUIRE(r.status == 0);
    REQUIRE(std::filesystem::exists("best_pot.tadah"));
    std::vector<std::string> potentials = {"best_pot.tadah"};
    for (const char* d : {"pots", "best_pots"}) {
      REQUIRE(std::filesystem::is_directory(d));
      for (const auto& e : std::filesystem::directory_iterator(d))
        potentials.push_back(e.path().string());
    }
    REQUIRE(potentials.size() >= 3);
    for (const auto& p : potentials) {
      INFO(p << ":\n" << slurp(p));
      CHECK(has_key_line(p, "WEIGHTS"));
      CHECK_FALSE(has_key_line(p, "CHECK"));
    }
  }
}

/* Pins: `tadah hpo --check` on a setup `tadah hpo` refuses at startup exits
 * with the run's status and message, and writes no file (the run writes its
 * journal): a target file with an unknown key (the parser names it), a
 * training configuration whose DBFILE names a file that does not exist (the
 * message names the file), and a DUMP or a BDUMP line whose directory is in
 * a directory that does not exist (the message names the directory; no
 * directory is created).
 * Before the DUMP and BDUMP directories were checked at their line: the
 * check of such a setup passed (status 0) and the run stopped when it
 * created the directory (status 1). */
TEST_CASE("Subcommand: hpo --check refuses a setup with the message of "
          "tadah hpo, and writes no file",
          "[TadahCLI][hpo][hpo-check]") {
  struct Case {
    const char* name;
    const char* hpo_line;  // appended to config.hpo
    const char* dbfile;    // the DBFILE of config.tadah
    const char* says;      // a part of the message
  };
  const Case cases[] = {
      {"unknown key in the target file", "NOSUCHKEY 1\n", "db.tadah",
       "NOSUCHKEY"},
      {"missing training dataset", "", "absent.tadah", "absent.tadah"},
      {"DUMP directory in a missing directory", "DUMP 1 missing/pots\n",
       "db.tadah", "missing/pots"},
      {"BDUMP directory in a missing directory",
       "BDUMP 1 missing/best_pots\n", "db.tadah", "missing/best_pots"},
  };
  for (const Case& c : cases) {
    INFO("case: " << c.name);
    /* The check first, then the run, in one directory: a message can name
     * a file by its absolute path. */
    ScratchCwd cwd("hpo_check_refused");
    write_hpo_check_fixture();
    std::ofstream("config.hpo", std::ios::app) << c.hpo_line;
    /* Read before the stream that rewrites the file truncates it. */
    const std::string config =
        replace_key_line(slurp("config.tadah"), "DBFILE",
                         std::string("DBFILE ") + c.dbfile);
    std::ofstream("config.tadah") << config;
    const auto before = run_directory_entries();
    const CliRun check = run_tadah({"tadah", "hpo", "--check", "-c",
                                    "config.tadah", "--hpotarget",
                                    "config.hpo", "--validation",
                                    "config.val"});
    CHECK(run_directory_entries() == before);
    const CliRun run = run_tadah({"tadah", "hpo", "-c", "config.tadah",
                                  "--hpotarget", "config.hpo",
                                  "--validation", "config.val"});
    INFO("tadah hpo: status " << run.status << "\n" << reported(run)
         << "\ntadah hpo --check: status " << check.status << "\n"
         << reported(check));
    CHECK(run.status == 1);
    CHECK(check.status == run.status);
    CHECK(contains(engine_error(run), c.says));
    CHECK(engine_error(check) == engine_error(run));
    CHECK_FALSE(std::filesystem::exists("missing"));
  }
}

/* Pins: CHECK is a setting of the run, as HPOTARGET is, and a potential
 * neither carries nor sets it:
 *   - `tadah train -c chk.tadah -o pot.tadah`, chk.tadah the fixture's
 *     config.tadah with "CHECK true", exits 0, and pot.tadah has no CHECK
 *     key line; its training record lists "# CHECK true" (tadah train reads
 *     no CHECK);
 *   - so do `tadah train -c g.train`, CHECK true above its line CONFIG
 *     config.tadah, and `tadah train -c t.train`, CHECK true below it;
 *   - `tadah hpo -c seed.tadah --hpotarget config.hpo --validation
 *     config.val`, seed.tadah pot.tadah with "CHECK true" and "DBFILE
 *     db.tadah" added, runs: status 0, no check summary, best_pot.tadah
 *     (without a CHECK line), and a WARNING names "CHECK true" as a setting
 *     of the run that wrote the potential;
 *   - `tadah hpo --check -c seed.tadah ...` is a check: the summary, and no
 *     file.
 * Before the fix: tadah train wrote the key line "CHECK true" in each of
 * the three, and `tadah hpo -c seed.tadah` read it: it printed the check
 * summary and exited 0 without a run. */
TEST_CASE("Subcommand: CHECK is a setting of the run: tadah train writes no "
          "CHECK line, and hpo -c of a potential with CHECK true runs",
          "[TadahCLI][hpo][hpo-check][retrain]") {
  ScratchCwd cwd("hpo_check_setting");
  write_hpo_check_fixture();
  {
    const std::string config = slurp("config.tadah");
    std::ofstream("chk.tadah") << config << "CHECK true\n";
  }
  std::ofstream("g.train") << "CHECK true\nCONFIG config.tadah\n"
                              "OUTFILE potg.tadah\n";
  std::ofstream("t.train") << "CONFIG config.tadah\nOUTFILE pott.tadah\n"
                              "CHECK true\n";
  const std::vector<std::pair<std::vector<std::string>, std::string>> trains =
      {{{"tadah", "train", "-c", "chk.tadah", "-o", "pot.tadah"}, "pot.tadah"},
       {{"tadah", "train", "-c", "g.train"}, "potg.tadah"},
       {{"tadah", "train", "-c", "t.train"}, "pott.tadah"}};
  for (const auto& [args, potential] : trains) {
    const CliRun t = run_tadah(args);
    INFO(args[1] << " " << args[2] << ": status " << t.status << "\n"
                 << reported(t));
    REQUIRE(t.status == 0);
    REQUIRE(std::filesystem::exists(potential));
    const std::string text = slurp(potential);
    INFO(potential << ":\n" << text);
    CHECK(has_key_line(potential, "WEIGHTS"));
    CHECK_FALSE(has_key_line(potential, "CHECK"));
    CHECK(contains(text, "\n# CHECK true\n"));
  }
  {
    const std::string pot = slurp("pot.tadah");
    std::ofstream("seed.tadah") << pot << "CHECK true\nDBFILE db.tadah\n";
  }
  {
    const CliRun r = run_tadah({"tadah", "hpo", "-c", "seed.tadah",
                                "--hpotarget", "config.hpo", "--validation",
                                "config.val"});
    const std::string rep = reported(r);
    INFO("hpo -c seed.tadah: status " << r.status << "\n" << rep);
    CHECK(r.status == 0);
    CHECK_FALSE(contains(r.out, "passed the startup checks"));
    CHECK(contains(rep, "its CHECK true was a setting of the run that wrote "
                        "it, not of this run, and is ignored"));
    REQUIRE(std::filesystem::exists("best_pot.tadah"));
    CHECK_FALSE(has_key_line("best_pot.tadah", "CHECK"));
  }
  for (const char* d : {"pots", "best_pots"}) std::filesystem::remove_all(d);
  for (const char* f : {"best_pot.tadah", "hpo_run.log"})
    std::filesystem::remove(f);
  {
    const auto before = run_directory_entries();
    const CliRun r = run_tadah({"tadah", "hpo", "--check", "-c", "seed.tadah",
                                "--hpotarget", "config.hpo", "--validation",
                                "config.val"});
    INFO("hpo --check -c seed.tadah: status " << r.status << "\n"
                                              << reported(r));
    CHECK(r.status == 0);
    CHECK(contains(r.out, "tadah hpo --check: config.hpo passed the startup "
                          "checks"));
    CHECK(run_directory_entries() == before);
  }
}

/* Pins: `tadah hpo -h` lists --check with its description, in the third
 * person as every help text ("Checks the setup of tadah hpo without
 * running it: ...").
 * Before the help was in the third person: it read "Check the setup of
 * tadah hpo without running it: make its startup checks, ...". */
TEST_CASE("Subcommand: hpo -h lists --check",
          "[TadahCLI][hpo][hpo-check]") {
  const CliRun r = run_tadah({"tadah", "hpo", "-h"});
  INFO("status " << r.status << "\n" << reported(r));
  CHECK(r.status == 0);
  CHECK(contains(r.out, "--check"));
  CHECK(contains(r.out, "Checks the setup of tadah hpo"));
}
#endif  // TADAH_ENABLE_NFP

/* ---------------------------------------------------------------------------
 * A potential with RCTYPE, RCUT and list values its descriptors do not
 * take, as tadah 1.5.0 wrote them (a joined line of D2_Blip and D2_BP with
 * a third RCTYPE2B and RCUT2B value and a KDEG2B neither reads), is
 * trained from: tadah train -c and tadah hpo -c report the values, one
 * WARNING per key, leave them out and train the potential's model.
 * Before the fix: both refused it ('TYPE2B names 2 descriptors (D2_Blip,
 * D2_BP) but RCTYPE2B has 3 values: ...'), after the WARNINGs.
 * ------------------------------------------------------------------------- */
namespace {

const char* const kRetrainTaDb =
    "bcc_ta_a\n1.0 1.0 1.0\n-0.30 0.0\n"
    "3.301 0.0 0.0\n0.0 3.301 0.0\n0.0 0.0 6.602\n"
    "0.0 0.0 0.0\n0.0 0.0 0.0\n0.0 0.0 0.0\n"
    "Ta 0.05 -0.03 0.02  0.31 -0.12 0.05\n"
    "Ta 1.70 1.62 1.68  -0.22 0.17 -0.09\n"
    "Ta -0.04 0.06 3.33  0.04 0.21 -0.15\n"
    "Ta 1.61 1.69 4.92  -0.13 -0.26 0.19\n"
    "\n"
    "bcc_ta_b\n1.0 1.0 1.0\n-0.24 0.0\n"
    "3.301 0.0 0.0\n0.0 3.301 0.0\n0.0 0.0 6.602\n"
    "0.0 0.0 0.0\n0.0 0.0 0.0\n0.0 0.0 0.0\n"
    "Ta 0.10 0.00 -0.05  -0.27 0.08 0.14\n"
    "Ta 1.66 1.58 1.62  0.19 -0.23 0.06\n"
    "Ta 0.02 -0.08 3.27  0.11 0.16 -0.21\n"
    "Ta 1.69 1.71 5.00  -0.03 -0.01 0.01\n"
    "\n";

/* Trains the joined model into pot.tadah, then writes old_pot.tadah: that
 * potential with the values 1.5.0 kept and a DBFILE line. */
void write_old_potential() {
  { std::ofstream os("train.db"); os << kRetrainTaDb; }
  {
    std::ofstream os("config.tadah");
    os << "DBFILE train.db\nATOM Ta\nWATOM 1\nINIT2B true\nINITMB false\n"
          "TYPE2B D2_mJoin\nTYPE2B D2_Blip 2 2 Ta Ta\nTYPE2B D2_BP 2 2 Ta Ta\n"
          "RCTYPE2B Cut_Cos Cut_Cos\nRCUT2B 5 4.5\n"
          "CGRID2B 2 3 0.5 1.0\nSGRID2B 1 1 2 3\n"
          "BIAS true\nVERBOSE 0\nMODEL M_BLR BF_Linear\nLAMBDA 1e-3\n";
  }
  const CliRun t = run_tadah({"tadah", "train", "-c", "config.tadah", "-o",
                              "pot.tadah"});
  INFO("train stderr:\n" << t.err << t.what);
  REQUIRE(t.status == 0);
  std::ifstream in("pot.tadah");
  std::ofstream os("old_pot.tadah");
  for (std::string line; std::getline(in, line);) {
    std::istringstream ss(line);
    std::string k;
    ss >> k;
    if (k == "RCTYPE2B") line = "RCTYPE2B Cut_Cos Cut_Cos Cut_Tanh";
    else if (k == "RCUT2B") line = "RCUT2B 5 4.5 6";
    else if (k == "DBFILE") continue;
    os << line << "\n";
  }
  os << "KDEG2B 5 5\nDBFILE train.db\n";
}

/* The keys reported once per process (core::report_ignored_key_once): a
 * case run after another in one process sees none again. */
std::vector<bool> reported_before() {
  std::vector<bool> b;
  for (const char* key : {"RCTYPE2B", "RCUT2B", "KDEG2B"})
    b.push_back(tadah::core::ignored_key_reported(key));
  return b;
}

void check_reported(const std::string& err, const std::vector<bool>& before) {
  const char* notices[] = {
      "WARNING: Part of RCTYPE2B (Cut_Tanh) is ignored: TYPE2B names 2 "
      "descriptors (D2_Blip, D2_BP), which take its first 2 values; the "
      "potential works unchanged without it",
      "WARNING: Part of RCUT2B (6) is ignored: TYPE2B names 2 descriptors "
      "(D2_Blip, D2_BP), which take its first 2 values; the potential "
      "works unchanged without it",
      "WARNING: Key KDEG2B (5 5) is ignored: TYPE2B names 2 descriptors "
      "(D2_Blip, D2_BP), which take none of its values; the potential "
      "works unchanged without it"};
  for (std::size_t i = 0; i < 3; ++i) {
    INFO(notices[i]);
    CHECK(contains(err, notices[i]) == !before[i]);
  }
}

}  // namespace

TEST_CASE("Subcommand: train: a potential with values its descriptors do not "
          "take retrains, the values reported and left out",
          "[TadahCLI][train][potential]") {
  ScratchCwd cwd("retrain_old_pot");
  write_old_potential();
  const auto before = reported_before();
  const CliRun r = run_tadah({"tadah", "train", "-c", "old_pot.tadah", "-o",
                              "retrained.tadah"});
  INFO("stderr:\n" << r.err << r.what);
  REQUIRE(r.status == 0);
  check_reported(r.err, before);
  CHECK(pot_key("retrained.tadah", "RCTYPE2B") ==
        std::vector<std::string>{"Cut_Cos", "Cut_Cos"});
  CHECK(pot_key("retrained.tadah", "RCUT2B") ==
        std::vector<std::string>{"5", "4.5"});
  CHECK(pot_key("retrained.tadah", "KDEG2B").empty());
  /* the potential's model, trained on the same data */
  CHECK(pot_key("retrained.tadah", "WEIGHTS") ==
        pot_key("pot.tadah", "WEIGHTS"));
}

#ifdef TADAH_ENABLE_NFP
TEST_CASE("Subcommand: hpo: a -c potential with values its descriptors do not "
          "take is searched from, the values reported and left out",
          "[TadahCLI][hpo][potential]") {
  ScratchCwd cwd("hpo_old_pot");
  write_old_potential();
  { std::ofstream os("config.val"); os << "DBFILE train.db\n"; }
  {
    std::ofstream os("config.hpo");
    os << "OPTIMIZER\nLIB NLOPT\nALGO LN_SBPLX\nMAXEVAL 2\nENDOPTIMIZER\n"
          "LOSS L2\nPC_ERMSE 0 1\nOPTIM CGRID2B (1) 1.5 2.5\n";
  }
  const auto before = reported_before();
  const CliRun r = run_tadah({"tadah", "hpo", "-c", "old_pot.tadah",
                              "--hpotarget", "config.hpo",
                              "--validation", "config.val"});
  INFO("stderr:\n" << r.err << r.what);
  REQUIRE(r.status == 0);
  check_reported(r.err, before);
  REQUIRE(std::filesystem::exists("best_pot.tadah"));
  CHECK(pot_key("best_pot.tadah", "RCTYPE2B") ==
        std::vector<std::string>{"Cut_Cos", "Cut_Cos"});
  CHECK(pot_key("best_pot.tadah", "RCUT2B") ==
        std::vector<std::string>{"5", "4.5"});
  CHECK(pot_key("best_pot.tadah", "KDEG2B").empty());
}
#endif  // TADAH_ENABLE_NFP

/* ---------------------------------------------------------------------------
 * A configuration file that includes a potential with CONFIG is the
 * concatenation of its lines: a line of a descriptor key outside the
 * potential's own lines adds its values to the potential's and is read as
 * a line of a training configuration, by every command that reads the
 * file. The potential's own values that its descriptors do not take are
 * reported and ignored, read alone or included.
 * ------------------------------------------------------------------------- */
namespace {

/* The number of the line of `key` in the file `file` (0: none). */
std::size_t line_of_key(const std::string& file, const std::string& key) {
  std::ifstream in(file);
  std::size_t n = 0, at = 0;
  for (std::string l; std::getline(in, l);) {
    ++n;
    std::istringstream ls(l);
    std::string k;
    if (ls >> k && k == key) at = n;
  }
  return at;
}

/* After write_old_potential(): above.train has "RCUT2B 6.0" above "CONFIG
 * pot.tadah", below.train below it; the refusal each gives. */
struct JoinedIncludes {
  std::string above, below;
};

JoinedIncludes write_joined_includes() {
  std::ofstream("above.train")
      << "RCUT2B 6.0\nCONFIG pot.tadah\nDBFILE train.db\n";
  std::ofstream("below.train")
      << "CONFIG pot.tadah\nDBFILE train.db\nRCUT2B 6.0\n";
  const std::size_t r = line_of_key("pot.tadah", "RCUT2B");
  REQUIRE(r > 0u);
  const std::string pot = "line " + std::to_string(r) + " of pot.tadah";
  const std::string counts =
      "TYPE2B names 2 descriptors (D2_Blip, D2_BP) but RCUT2B has 3 values, "
      "set on ";
  const std::string one =
      ": each descriptor takes one RCUT2B value, in the order of the TYPE2B "
      "lines";
  return {counts + "line 1 of above.train and on " + pot +
              " (CONFIG on line 2 of above.train)" + one,
          counts + pot +
              " (CONFIG on line 1 of below.train) and on line 3 of "
              "below.train" + one};
}

}  // namespace

/* `tadah train -c` of a file that includes the D2_mJoin potential pot.tadah
 * (D2_Blip RCUT2B 5, D2_BP 4.5: write_old_potential) with CONFIG and has
 * "RCUT2B 6.0" above the CONFIG line exits 1 with "TYPE2B names 2
 * descriptors (D2_Blip, D2_BP) but RCUT2B has 3 values, set on line 1 of
 * above.train and on line <r> of pot.tadah (CONFIG on line 2 of
 * above.train): each descriptor takes one RCUT2B value, in the order of the
 * TYPE2B lines" and writes no potential; below the CONFIG line likewise
 * ("set on line <r> of pot.tadah (CONFIG on line 1 of below.train) and on
 * line 3 of below.train"). Pins: old_pot.tadah (1.5.0 values) included by
 * a file of no other line retrains with its values reported and left out,
 * as given with -c.
 * Before the fix: above.train exited 0 and wrote RCUT2B 6 5 (D2_BP's
 * cutoff 5), with "WARNING: Part of RCUT2B (4.5) is ignored: ... the
 * potential works unchanged without it"; below.train exited 0, 6.0
 * ignored with a WARNING. */
TEST_CASE("Subcommand: train: a line of RCUT2B in a file that includes a "
          "D2_mJoin potential is refused naming both lines",
          "[TadahCLI][train][retrain][config-include][potential]") {
  ScratchCwd cwd("joined_include_rcut");
  write_old_potential();
  const JoinedIncludes refusals = write_joined_includes();
  {
    const CliRun r = run_tadah(
        {"tadah", "train", "-c", "above.train", "-o", "above.tadah"});
    const std::string rep = reported(r);
    INFO("-c above.train: status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    CHECK(contains(rep, refusals.above));
    CHECK_FALSE(std::filesystem::exists("above.tadah"));
  }
  {
    const CliRun r = run_tadah(
        {"tadah", "train", "-c", "below.train", "-o", "below.tadah"});
    const std::string rep = reported(r);
    INFO("-c below.train: status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    CHECK(contains(rep, refusals.below));
    CHECK_FALSE(std::filesystem::exists("below.tadah"));
  }
  {
    std::ofstream("old.train") << "CONFIG old_pot.tadah\n";
    const auto before = reported_before();
    const CliRun r = run_tadah(
        {"tadah", "train", "-c", "old.train", "-o", "old_retrained.tadah"});
    INFO("-c old.train: status " << r.status << "\n" << reported(r));
    REQUIRE(r.status == 0);
    check_reported(r.err, before);
    CHECK(pot_key("old_retrained.tadah", "RCUT2B") ==
          std::vector<std::string>{"5", "4.5"});
  }
}

/* Pins: `tadah predict -p` of a potential with values its descriptors do
 * not take (old_pot.tadah without its DBFILE line) exits 0 with the
 * WARNINGs, given once per key and process. Before the fix: passes as
 * well. */
TEST_CASE("Subcommand: predict -p of a potential with values its "
          "descriptors do not take loads with the WARNINGs",
          "[TadahCLI][predict][potential]") {
  ScratchCwd cwd("predict_old_pot");
  write_old_potential();
  std::ofstream("old.tadah") << with_line(slurp("old_pot.tadah"), "DBFILE", "");
  const auto before = reported_before();
  const CliRun r =
      run_tadah({"tadah", "predict", "-p", "old.tadah", "-d", "train.db"});
  INFO("status " << r.status << "\n" << reported(r));
  CHECK(r.status == 0);
  check_reported(r.err, before);
}

/* `tadah analysis check_bounds -c`, `tadah analysis suggest_bounds -c` and
 * `tadah analysis bfunc -c` of above.train (an RCUT2B line above the CONFIG
 * line of the D2_mJoin potential) exit 1 with the refusal of the retrain.
 * Before the fix: each read RCUT2B 6 5 (the line moved D2_BP's cutoff) and
 * went on: check_bounds and suggest_bounds without the refusal,
 * suggest_bounds and bfunc with exit 0. */
TEST_CASE("Subcommand: analysis: a line of RCUT2B in a file that includes a "
          "D2_mJoin potential is refused naming both lines",
          "[TadahCLI][analysis][config-include][potential]") {
  ScratchCwd cwd("joined_include_analysis");
  write_old_potential();
  const JoinedIncludes refusals = write_joined_includes();
  std::ofstream("config.hpo")
      << "OPTIMIZER\nLIB NLOPT\nALGO LN_SBPLX\nMAXEVAL 1\nENDOPTIMIZER\n"
         "LOSS L2\nPC_ERMSE 0 1\nOPTIM CGRID2B (1) 1.5 2.5\n";
  const std::vector<std::vector<std::string>> runs = {
      {"tadah", "analysis", "check_bounds", "-c", "above.train",
       "--hpotarget", "config.hpo"},
      {"tadah", "analysis", "suggest_bounds", "-c", "above.train", "-o",
       "block.hpo"},
      {"tadah", "analysis", "bfunc", "-c", "above.train", "--type", "2b",
       "-r", "1", "5", "5", "-o", "b.out"}};
  for (const auto& args : runs) {
    const CliRun r = run_tadah(args);
    const std::string rep = reported(r);
    INFO(args[2] << ": status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    CHECK(contains(rep, refusals.above));
  }
}

#ifdef TADAH_ENABLE_NFP
/* `tadah hpo -c above.train` and `tadah hpo --check -c below.train` (an
 * RCUT2B line above or below the CONFIG line of the D2_mJoin potential, the
 * seed of the search) exit 1 with the refusal of the retrain and write no
 * best_pot.tadah.
 * Before the fix: the search ran from RCUT2B 6 5 (exit 0, best_pot.tadah
 * with RCUT2B 6 5), and the check passed with 6.0 ignored. */
TEST_CASE("Subcommand: hpo: a line of RCUT2B in a file that includes the "
          "seed potential is refused naming both lines",
          "[TadahCLI][hpo][config-include][potential]") {
  ScratchCwd cwd("joined_include_hpo");
  write_old_potential();
  const JoinedIncludes refusals = write_joined_includes();
  std::ofstream("config.val") << "DBFILE train.db\n";
  std::ofstream("config.hpo")
      << "OPTIMIZER\nLIB NLOPT\nALGO LN_SBPLX\nMAXEVAL 2\nENDOPTIMIZER\n"
         "LOSS L2\nPC_ERMSE 0 1\nOPTIM CGRID2B (1) 1.5 2.5\n";
  {
    const CliRun r = run_tadah({"tadah", "hpo", "-c", "above.train",
                                "--hpotarget", "config.hpo", "--validation",
                                "config.val"});
    const std::string rep = reported(r);
    INFO("hpo -c above.train: status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    CHECK(contains(rep, refusals.above));
    CHECK_FALSE(std::filesystem::exists("best_pot.tadah"));
  }
  {
    const CliRun r = run_tadah({"tadah", "hpo", "--check", "-c",
                                "below.train", "--hpotarget", "config.hpo",
                                "--validation", "config.val"});
    const std::string rep = reported(r);
    INFO("hpo --check -c below.train: status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    CHECK(contains(rep, refusals.below));
  }
}
#endif  // TADAH_ENABLE_NFP

/* ---- [published]: the potentials published with the documentation ------
 *
 * The potentials published with the documentation (docs/source/mlips:
 * Tadah.Kr1, Tadah.Kr2 and tadah.n2; ta-dah.kr1 and ta-dah.kr2 are in the
 * file format of the discontinued Ta-dah!, which no Tadah! reads) were fitted
 * by Tadah! before 1.6.0. Their key lines are those of the model: the lines
 * of the keys that weighted their fit (EWEIGHT, FWEIGHT, SWEIGHT, retired in
 * 1.6.0) and of the run that wrote them (VERBOSE, NUMERIC) are in their
 * training record, the comment block above the key lines that no loader
 * reads. A potential with retired keys as key lines (a potential of an
 * earlier version) keeps its tests on the fixtures of Tadah! 1.5.0. */

namespace {

/* The 1-based numbers of the lines of @p file whose first word is @p key. */
std::vector<std::size_t> key_line_numbers(const std::string& file,
                                          const std::string& key) {
  std::ifstream in(file);
  std::vector<std::size_t> out;
  std::size_t n = 0;
  for (std::string line; std::getline(in, line);) {
    ++n;
    std::istringstream ls(line);
    std::string first;
    if (ls >> first && first == key) out.push_back(n);
  }
  return out;
}

/* The key lines of the training record of the potential file @p file
 * ("KEY v1 v2 ...", without the leading "# "), in order. */
std::vector<std::string> training_record_lines(const std::string& file) {
  std::ifstream in(file);
  std::vector<std::string> out;
  bool inside = false;
  for (std::string line; std::getline(in, line);) {
    if (line.rfind("# --- training record", 0) == 0) {
      inside = true;
    } else if (line.rfind("# --- end of training record", 0) == 0) {
      inside = false;
    } else if (inside && line.rfind("# ", 0) == 0) {
      out.push_back(line.substr(2));
    }
  }
  return out;
}

}  // namespace

/* Pins: for each published potential P (Tadah.Kr1, Tadah.Kr2, tadah.n2,
 * copied into the scratch directory as a user downloads it):
 *  - its training record lists the retired keys of its fit, EWEIGHT,
 *    FWEIGHT and SWEIGHT, and the run settings of the run that wrote it
 *    (VERBOSE 0 for the Kr potentials, NUMERIC 15 for Tadah!N2), and none
 *    of these is a key line;
 *  - `tadah predict -p P -d D` (D a Kr dimer 4 A apart for the Kr
 *    potentials, two N2 molecules for Tadah!N2) exits 0, gives no notice of
 *    a retired key and writes the energies of P' -- P with the lines of its
 *    training record as key lines, the file as published before -- digit
 *    for digit; P' gives one notice of its retired keys;
 *  - the Kr potentials retrain as the Training page says (P with a DBFILE
 *    line added: exit 0 and pot.tadah). (Tadah!N2 is refused by the DIMER
 *    rule of a fit, which another case checks.)
 * Before the fix: P had the EWEIGHT, FWEIGHT, SWEIGHT and VERBOSE or
 * NUMERIC lines as key lines (no training record): predict gave the notice
 * of its retired keys ("Retired keys EWEIGHT (line 7 of Tadah.Kr1), ...")
 * and a retrain was refused with their conversion. */
TEST_CASE("Subcommand: the published potentials load without a notice of "
          "retired keys, their fit's keys in the training record",
          "[TadahCLI][predict][train][published]") {
  ScratchCwd cwd("published_record");
  const std::string dir = TADAH_PUBLISHED_MLIPS_DIR;
  std::ofstream("kr.tadah")
      << "Kr dimer\n1 1 1\n-0.01\n40 0 0\n0 40 0\n0 0 40\n0 0 0\n0 0 0\n"
         "0 0 0\nKr 10 10 10 0 0 0\nKr 14 10 10 0 0 0\n\n";
  std::ofstream("n2.tadah")
      << "N2 pair\n1 1 1\n-0.005\n40 0 0\n0 40 0\n0 0 40\n0 0 0\n0 0 0\n"
         "0 0 0\nN 10 10 9.45 0 0 0\nN 10 10 10.55 0 0 0\n"
         "N 14 10 9.45 0 0 0\nN 14 10 10.55 0 0 0\n\n";
  for (const char* name : {"Tadah.Kr1", "Tadah.Kr2", "tadah.n2"}) {
    INFO(name);
    const bool n2 = std::string(name) == "tadah.n2";
    std::filesystem::copy_file(dir + "/" + name, name);
    const std::string data = n2 ? "n2.tadah" : "kr.tadah";
    const std::vector<std::string> record = training_record_lines(name);
    INFO("training record: " << record.size() << " lines");
    for (const std::string key :
         {std::string("EWEIGHT"), std::string("FWEIGHT"),
          std::string("SWEIGHT"), std::string(n2 ? "NUMERIC" : "VERBOSE")}) {
      INFO(key);
      CHECK(key_line_numbers(name, key).empty());
      CHECK(std::count_if(record.begin(), record.end(),
                          [&](const std::string& l) {
                            return l.rfind(key + " ", 0) == 0;
                          }) == 1);
    }
    // P': the record's lines as key lines.
    {
      std::ofstream old(std::string(name) + ".keys");
      for (const std::string& l : record) old << l << "\n";
      std::istringstream in(slurp(name));
      for (std::string line; std::getline(in, line);)
        if (line.rfind("#", 0) != 0) old << line << "\n";
    }
    std::filesystem::remove("energy.pred");
    const CliRun p = run_tadah({"tadah", "predict", "-p", name, "-d", data,
                                "-o", "new.pred"});
    const std::string rep = reported(p);
    INFO("predict: status " << p.status << "\n" << rep);
    CHECK(p.status == 0);
    CHECK(count_lines_with(rep, "Retired key") == 0u);
    const CliRun q = run_tadah({"tadah", "predict", "-p",
                                std::string(name) + ".keys", "-d", data, "-o",
                                "old.pred"});
    INFO("predict of the record as key lines: status " << q.status << "\n"
                                                       << reported(q));
    CHECK(q.status == 0);
    CHECK(count_lines_with(reported(q), "Retired keys") == 1u);
    REQUIRE(std::filesystem::exists("new.pred"));
    CHECK(slurp("new.pred") == slurp("old.pred"));
    std::filesystem::remove("new.pred");
    std::filesystem::remove("old.pred");
    if (n2) continue;
    std::filesystem::remove("pot.tadah");
    std::ofstream(std::string(name) + ".train")
        << slurp(name) << "DBFILE " << data << "\n";
    const CliRun t = run_tadah({"tadah", "train", "-c",
                                std::string(name) + ".train"});
    INFO("train: status " << t.status << "\n" << reported(t));
    CHECK(t.status == 0);
    CHECK(std::filesystem::exists("pot.tadah"));
  }
}

namespace {

/* The words of a command line of the help (a blueprint example), as a
 * shell splits it: at blanks, a word in double quotes kept whole without
 * the quotes. */
std::vector<std::string> shell_words(const std::string& line) {
  std::vector<std::string> words;
  std::string word;
  bool quoted = false, any = false;
  for (const char ch : line) {
    if (ch == '"') {
      quoted = !quoted;
      any = true;
    } else if ((ch == ' ' || ch == '\t') && !quoted) {
      if (any) words.push_back(word);
      word.clear();
      any = false;
    } else {
      word += ch;
      any = true;
    }
  }
  if (any) words.push_back(word);
  return words;
}

}  // namespace

/* Pins: tadah has no option --task (-t), neither alone nor on a command:
 * `tadah --task run.task`, `tadah -t run.task` and `tadah train --task
 * run.task`, run.task a file of the form task files had (TASK train,
 * CONFIG config.train, OUTFILE pot.tadah), exit non-zero naming the
 * option, and nothing runs (no pot.tadah).
 * Before the fix: each ran the TASK train section of run.task (exit 0,
 * pot.tadah written), the command in front of --task ignored. */
TEST_CASE("Subcommand: tadah and its commands have no option --task",
          "[TadahCLI][exit-status][one_command]") {
  ScratchCwd cwd("no_task_option");
  write_ta_dimer_training();
  std::ofstream("run.task") << "TASK train\nCONFIG config.train\n"
                               "OUTFILE pot.tadah\n";
  for (const std::vector<std::string>& args :
       {std::vector<std::string>{"tadah", "--task", "run.task"},
        std::vector<std::string>{"tadah", "-t", "run.task"},
        std::vector<std::string>{"tadah", "train", "--task", "run.task"}}) {
    std::filesystem::remove("pot.tadah");
    const CliRun r = run_tadah(args);
    const std::string rep = reported(r);
    INFO(args[1] << " " << args[2] << ": status " << r.status << "\n" << rep);
    CHECK(r.status != 0);
    CHECK(contains(rep, args[1] == "train" ? args[2] : args[1]));
    CHECK_FALSE(std::filesystem::exists("pot.tadah"));
  }
}

/* Pins: an argument that no command takes is named first, with CLI11's
 * exit status 109, also when the command line lacks an option, gives one
 * without an option it needs or with one it excludes: `tadah train --foo`
 * (no -c), `tadah predict -d db.tadah --foo` (-d needs -p) and `tadah data
 * sample -d db.tadah -o out.tadah --uniform 2 --random 2 --foo` (--random
 * excludes --uniform) each exit 109 with "The following argument was not
 * expected: --foo", and none says "Exactly 1 option", "requires" or
 * "excludes".
 * Before the fix: CLI11 reported what the command lacked, needed or
 * excluded first: "Exactly 1 option from [-c,--config] is required" (exit
 * 106; "[-t,--task,-c,--config]" while --task was an option), "--dbfile
 * requires --potential" (107) and "--random excludes --uniform" (108),
 * without --foo. */
TEST_CASE("Subcommand: an argument no command takes is named before what "
          "the command lacks, needs or excludes",
          "[TadahCLI][exit-status][one_command]") {
  ScratchCwd cwd("extras_first");
  struct Leg {
    std::vector<std::string> args;
    const char* other;  // what CLI11 reported first before the fix
  };
  const std::vector<Leg> legs = {
      {{"tadah", "train", "--foo"}, "Exactly 1 option"},
      {{"tadah", "predict", "-d", "db.tadah", "--foo"}, "requires"},
      {{"tadah", "data", "sample", "-d", "db.tadah", "-o", "out.tadah",
        "--uniform", "2", "--random", "2", "--foo"},
       "excludes"},
  };
  for (const Leg& leg : legs) {
    std::string line;
    for (const std::string& a : leg.args) line += (line.empty() ? "" : " ") + a;
    const CliRun r = run_tadah(leg.args);
    const std::string rep = reported(r);
    INFO(line << ": status " << r.status << "\n" << rep);
    CHECK(r.status == 109);
    CHECK(contains(rep, "The following argument was not expected: --foo"));
    CHECK_FALSE(contains(rep, leg.other));
  }
}

/* Pins: tadah runs one command per run: a command line with two commands,
 * `tadah train -c config.train -o a.tadah predict -d db.tadah -p
 * a.tadah` and `tadah train -c config.train -o a.tadah train -c
 * config.train -o b.tadah`, exits 105 before either runs (no a.tadah, no
 * b.tadah, no energy.pred), naming the commands it gives: "tadah runs
 * one command per run: the command line gives 2, tadah train and tadah
 * predict" (and "..., tadah train and tadah train"). Each command alone
 * runs.
 * Before the fix: both commands ran, one after the other (exit 0, a.tadah,
 * b.tadah and energy.pred written), the last command's VERBOSE applying to
 * both. */
TEST_CASE("Subcommand: a command line with two commands is refused before "
          "either runs",
          "[TadahCLI][exit-status][one_command]") {
  ScratchCwd cwd("two_commands");
  write_ta_dimer_training();
  const std::vector<std::string> outputs = {"a.tadah", "b.tadah",
                                            "energy.pred"};
  const std::vector<std::pair<std::vector<std::string>, std::string>> cases =
      {{{"tadah", "train", "-c", "config.train", "-o", "a.tadah", "predict",
         "-d", "db.tadah", "-p", "a.tadah"},
        "tadah runs one command per run: the command line gives 2, tadah "
        "train and tadah predict"},
       {{"tadah", "train", "-c", "config.train", "-o", "a.tadah", "train",
         "-c", "config.train", "-o", "b.tadah"},
        "tadah runs one command per run: the command line gives 2, tadah "
        "train and tadah train"}};
  for (const auto& c : cases) {
    for (const auto& f : outputs) std::filesystem::remove(f);
    const CliRun r = run_tadah(c.first);
    const std::string rep = reported(r);
    INFO(c.second << "\nstatus " << r.status << "\n" << rep);
    CHECK(r.status == 105);
    CHECK(contains(rep, c.second));
    for (const auto& f : outputs) {
      INFO("written: " << f);
      CHECK_FALSE(std::filesystem::exists(f));
    }
  }
  const CliRun t = run_tadah({"tadah", "train", "-c", "config.train", "-o",
                              "a.tadah"});
  INFO("train: status " << t.status << "\n" << reported(t));
  CHECK(t.status == 0);
  const CliRun p = run_tadah({"tadah", "predict", "-d", "db.tadah", "-p",
                              "a.tadah"});
  INFO("predict: status " << p.status << "\n" << reported(p));
  CHECK(p.status == 0);
  CHECK(std::filesystem::exists("energy.pred"));
}

/* Pins: a command line that gives a second command is refused before any
 * command runs, with the message of one command per run and exit status
 * 105, in every form CLI11 reads it:
 *  - the command that holds the one CLI11 reads, typed again (`tadah data
 *    print -d db.tadah data split -d db.tadah -o part1.tadah part2.tadah
 *    --even`: "the command line gives 2, tadah data print and tadah data
 *    split");
 *  - a command given before, typed again after another (`tadah predict ...
 *    analysis cutoff ... predict ...`: "gives 3, tadah predict, tadah
 *    analysis cutoff and tadah predict");
 *  - a second command that lacks an option (`tadah predict -d db.tadah -p
 *    a.tadah predict -p a.tadah`, `tadah explain train.verbose train`);
 *  - the same command and its group typed again (`tadah analysis cutoff
 *    ... analysis cutoff ...`);
 *  - a command that groups others, given alone after a command (`tadah data
 *    print -d db.tadah data`, `tadah predict ... analysis predict ...`);
 * and nothing is written (part1.tadah, part2.tadah, energy.pred,
 * cutoff.txt). An argument
 * that names no command keeps CLI11's exit status 109 ("The following
 * argument was not expected: predictx", "... --foo").
 * Before the fix: exit 109 naming the second command's words as arguments
 * no command takes ("The following argument was not expected: data", "...
 * analysis", "The following arguments were not expected: db.tadah -d
 * a.tadah -p predict"), exit 106 for what the second command lacked
 * ("At least 1 option from [-s,--structure,-d,--dbfile] is required",
 * "Exactly 1 option from [-c,--config] is required"), and exit 105 with
 * "tadah analysis takes one of its commands: ..." for the group given
 * alone after predict. */
TEST_CASE("Subcommand: a second command is refused in every form, also "
          "where CLI11 takes its name for an argument",
          "[TadahCLI][exit-status][one_command]") {
  ScratchCwd cwd("second_command_forms");
  write_ta_dimer_training();
  const std::vector<std::string> outputs = {"part1.tadah", "part2.tadah",
                                            "energy.pred", "cutoff.txt"};
  const std::string head =
      "tadah runs one command per run: the command line gives ";
  const std::vector<std::string> cutoff = {
      "analysis", "cutoff", "-o", "cutoff.txt", "-r", "0", "3.14", "100",
      "--type", "Cut_Cos"};
  auto line = [](std::initializer_list<std::vector<std::string>> parts) {
    std::vector<std::string> out = {"tadah"};
    for (const auto &p : parts) out.insert(out.end(), p.begin(), p.end());
    return out;
  };
  const std::vector<std::string> predict = {"predict", "-d", "db.tadah",
                                            "-p", "a.tadah"};
  const std::vector<std::pair<std::vector<std::string>, std::string>> cases =
      {{line({{"data", "print", "-d", "db.tadah", "data", "split", "-d",
               "db.tadah", "-o", "part1.tadah", "part2.tadah", "--even"}}),
        "2, tadah data print and tadah data split"},
       {line({predict, cutoff, predict}),
        "3, tadah predict, tadah analysis cutoff and tadah predict"},
       {line({predict, {"predict", "-p", "a.tadah"}}),
        "2, tadah predict and tadah predict"},
       {line({{"explain", "train.verbose", "train"}}),
        "2, tadah explain and tadah train"},
       {line({cutoff, cutoff}),
        "2, tadah analysis cutoff and tadah analysis cutoff"},
       {line({{"data", "print", "-d", "db.tadah", "data"}}),
        "2, tadah data print and tadah data"},
       {line({predict, {"analysis"}, predict}),
        "3, tadah predict, tadah analysis and tadah predict"}};
  for (const auto& c : cases) {
    for (const auto& f : outputs) std::filesystem::remove(f);
    std::string words;
    for (const std::string& a : c.first) words += (words.empty() ? "" : " ") + a;
    const CliRun r = run_tadah(c.first);
    const std::string rep = reported(r);
    INFO(words << "\nstatus " << r.status << "\n" << rep);
    CHECK(r.status == 105);
    CHECK(contains(rep, head + c.second +
                            " (each runs as a tadah command of its own)"));
    for (const auto& f : outputs) {
      INFO("written: " << f);
      CHECK_FALSE(std::filesystem::exists(f));
    }
  }
  const std::vector<std::pair<std::vector<std::string>, std::string>> extras =
      {{line({predict, {"-A", "predictx"}}), "predictx"},
       {line({{"data", "print", "-d", "db.tadah", "--foo"}}), "--foo"}};
  for (const auto& c : extras) {
    const CliRun r = run_tadah(c.first);
    const std::string rep = reported(r);
    INFO(c.second << ": status " << r.status << "\n" << rep);
    CHECK(r.status == 109);
    CHECK(contains(rep, "The following argument was not expected: " +
                            c.second));
    CHECK_FALSE(contains(rep, head));
  }
}

/* Pins: a command that groups others runs nothing itself: `tadah data`,
 * `tadah data -v 2`, `tadah properties` and `tadah analysis` exit 105
 * naming the commands of the group: "tadah data takes one of its commands:
 * balance, convert, dedup, merge, print, sample, split or write",
 * "tadah properties takes one of its commands: pairwise", "tadah analysis
 * takes one of its commands: bfunc, check_bounds, cutoff, dataset_stats,
 * descriptor or suggest_bounds".
 * Before the fix: `tadah data`, `tadah properties` and `tadah analysis`
 * exited 0 and printed nothing, and `tadah data -v 2` exited 1 with
 * "TaskFactory::createTasksFromInput - taskInput.getTaskName() is not a
 * valid task: DATA". */
TEST_CASE("Subcommand: a command that groups others is refused without one "
          "of its commands",
          "[TadahCLI][exit-status][one_command]") {
  ScratchCwd cwd("group_alone");
  const std::string data =
      "tadah data takes one of its commands: balance, convert, dedup, merge, "
      "print, sample, split or write";
  const std::vector<std::pair<std::vector<std::string>, std::string>> cases =
      {{{"tadah", "data"}, data},
       {{"tadah", "data", "-v", "2"}, data},
       {{"tadah", "properties"},
        "tadah properties takes one of its commands: pairwise"},
       {{"tadah", "analysis"},
        "tadah analysis takes one of its commands: bfunc, check_bounds, "
        "cutoff, dataset_stats, descriptor or suggest_bounds"}};
  for (const auto& c : cases) {
    const CliRun r = run_tadah(c.first);
    const std::string rep = reported(r);
    INFO(c.first.size() << " words: status " << r.status << "\n" << rep);
    CHECK(r.status == 105);
    CHECK(contains(rep, c.second));
  }
}

/* Pins: each example of the help of `tadah analysis cutoff` runs as the
 * help prints it: exit 0, its output file written (`-o cutoff.txt -r 0
 * 3.14 100 --type Cut_Cos --derivative`).
 * Before the fix: the example was `... -t "CutCos" --derivative`: -t was
 * --task, so it exited non-zero ("--range excludes --task"), and CutCos is
 * no cutoff (the name is Cut_Cos). */
TEST_CASE("Subcommand: the examples of analysis cutoff run",
          "[TadahCLI][exit-status][analysis]") {
  ScratchCwd cwd("cutoff_examples");
  const auto& examples = tadah::core::BlueprintParser::getTasksMapRef()
                             .at("ANALYSIS.cutoff")
                             .examples;
  REQUIRE_FALSE(examples.empty());
  for (const std::string& example : examples) {
    std::filesystem::remove("cutoff.txt");
    const CliRun r = run_tadah(shell_words(example));
    INFO(example << "\nstatus " << r.status << "\n" << reported(r));
    CHECK(r.status == 0);
    CHECK(std::filesystem::exists("cutoff.txt"));
  }
}

/* Pins: refit --retarget refuses NUMERIC in its configuration (-c), which
 * has no effect there: on the outputs of a dense refit of the small Ta
 * setfl, `tadah refit -c rt.cfg` with RETARGET refit.hpotarget, OUTFILE
 * rt.hpotarget and NUMERIC 14 exits 1, "tadah refit --retarget: NUMERIC
 * has no effect under --retarget: only a refit of an EAM file and --edit
 * read it; remove NUMERIC (the hpotarget --retarget writes prints its
 * bounds %.8e whatever NUMERIC is)", and writes no rt.hpotarget; without
 * NUMERIC it runs; `tadah refit -c edit.cfg` (EDIT pot.tadah, FUNC phi, ADD
 * 2.5, NUMERIC 14) runs, as --edit writes a potential.
 * Before the fix: --retarget accepted NUMERIC (exit 0, rt.hpotarget
 * written). */
TEST_CASE("Subcommand: refit --retarget refuses NUMERIC in its "
          "configuration",
          "[TadahCLI][exit-status][refit]") {
  ScratchCwd cwd("refit_retarget_numeric");
  { std::ofstream os("eam_small.alloy"); os << small_ta_setfl(); }
  std::ofstream("config.refit")
      << "EAM eam_small.alloy\nRHOFORM SIGNED\nREFIT_MODE dense\n"
         "REFIT_BASIS Blip\nREFIT_LEVEL coarse\nREFIT_MAX_ESCALATIONS 0\n";
  {
    const CliRun fit = run_tadah({"tadah", "refit", "-c", "config.refit"});
    INFO("refit: status " << fit.status << "\n" << reported(fit));
    REQUIRE(fit.status == 0);
    REQUIRE(std::filesystem::exists("pot.tadah"));
    REQUIRE(std::filesystem::exists("refit.hpotarget"));
  }
  const std::string retarget =
      "RETARGET refit.hpotarget\nOUTFILE rt.hpotarget\n";
  {
    std::ofstream("rt.cfg") << retarget << "NUMERIC 14\n";
    const CliRun r = run_tadah({"tadah", "refit", "-c", "rt.cfg"});
    const std::string rep = reported(r);
    INFO("--retarget with NUMERIC: status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    CHECK(contains(rep, "tadah refit --retarget: NUMERIC has no effect "
                        "under --retarget: only a refit of an EAM file and "
                        "--edit read it; remove NUMERIC (the hpotarget "
                        "--retarget writes prints its bounds %.8e whatever "
                        "NUMERIC is)"));
    CHECK_FALSE(std::filesystem::exists("rt.hpotarget"));
  }
  {
    std::ofstream("rt.cfg") << retarget;
    const CliRun r = run_tadah({"tadah", "refit", "-c", "rt.cfg"});
    INFO("--retarget: status " << r.status << "\n" << reported(r));
    CHECK(r.status == 0);
    CHECK(std::filesystem::exists("rt.hpotarget"));
  }
  std::ofstream("edit.cfg") << "EDIT pot.tadah\nFUNC phi\nADD 2.5\n"
                               "NUMERIC 14\n";
  const CliRun e = run_tadah({"tadah", "refit", "-c", "edit.cfg"});
  INFO("--edit with NUMERIC: status " << e.status << "\n" << reported(e));
  CHECK(e.status == 0);
  CHECK(std::filesystem::exists("pot_edited.tadah"));
}

/* ---- [one_command]: the commands are counted from the words ---------- */

/* Pins: the commands of a command line are counted from its words before
 * CLI11 checks any command's options, each option taking the words it
 * takes (a command name after an option that takes one value is that
 * value): `tadah predict -p x train -c y` exits 105 with "the command line
 * gives 2, tadah predict and tadah train", `tadah train -c a predict -p b
 * hpo -c c` with "gives 3, tadah train, tadah predict and tadah hpo",
 * `tadah data print -d a split -d b data` with "gives 3, tadah data print,
 * tadah data split and tadah data" and `tadah data print data split` with
 * "gives 2, tadah data print and tadah data split"; `tadah train -c
 * predict` reads predict as the file of -c (no second command: exit 1, the
 * file does not open).
 * Before the fix: the count followed how far CLI11 had read: the first
 * exited 106 ("At least 1 option from [-s,--structure,-d,--dbfile] is
 * required", predict's requirement, before train was read), the second
 * gave 2 (train and predict, hpo unread), the third 2 (data split absorbed
 * the trailing data) and the fourth "tadah data print and tadah data". */
TEST_CASE("Subcommand: the commands of a command line are counted from its "
          "words",
          "[TadahCLI][exit-status][one_command]") {
  ScratchCwd cwd("command_words");
  const std::string head =
      "tadah runs one command per run: the command line gives ";
  const std::vector<std::pair<std::vector<std::string>, std::string>> cases =
      {{{"tadah", "predict", "-p", "x", "train", "-c", "y"},
        "2, tadah predict and tadah train"},
       {{"tadah", "train", "-c", "a", "predict", "-p", "b", "hpo", "-c", "c"},
        "3, tadah train, tadah predict and tadah hpo"},
       {{"tadah", "data", "print", "-d", "a", "split", "-d", "b", "data"},
        "3, tadah data print, tadah data split and tadah data"},
       {{"tadah", "data", "print", "data", "split"},
        "2, tadah data print and tadah data split"}};
  for (const auto& c : cases) {
    std::string words;
    for (const std::string& a : c.first)
      words += (words.empty() ? "" : " ") + a;
    const CliRun r = run_tadah(c.first);
    const std::string rep = reported(r);
    INFO(words << "\nstatus " << r.status << "\n" << rep);
    CHECK(r.status == 105);
    CHECK(contains(rep, head + c.second +
                            " (each runs as a tadah command of its own)"));
  }
  const CliRun f = run_tadah({"tadah", "train", "-c", "predict"});
  const std::string rf = reported(f);
  INFO("tadah train -c predict: status " << f.status << "\n" << rf);
  CHECK(f.status == 1);
  CHECK_FALSE(contains(rf, head));
  CHECK(contains(rf, "predict"));
}

/* ---- [stream_state]: tadah predict -A and the format of std::cout ----- */

/* Pins: tadah predict -A leaves the format of standard output as it found
 * it: on the three Ta dimers, `tadah predict -d db.tadah -p pot.tadah -A`
 * (exit 0) and then, in the same process, `tadah train -c config.a -v 1`
 * with M_KRR Kern_RBF (MPARAMS 1.0) print the kernel's settings line
 * "Kern_RBF | sigma: 1 | gamma: 1" (the kernel copies the format of
 * std::cout).
 * Before the fix: predict -A left std::cout std::scientific at precision 6,
 * and the line read "Kern_RBF | sigma: 1.000000e+00 | gamma:
 * 1.000000e+00". */
TEST_CASE("Subcommand: tadah predict -A leaves the format of standard output "
          "as it found it",
          "[TadahCLI][kernel_line][stream_state]") {
  ScratchCwd cwd("predict_stream_state");
  write_ta_dimer_training();
  const CliRun t = run_tadah({"tadah", "train", "-c", "config.train"});
  INFO("train: status " << t.status << "\n" << reported(t));
  REQUIRE(t.status == 0);
  const CliRun p = run_tadah(
      {"tadah", "predict", "-d", "db.tadah", "-p", "pot.tadah", "-A"});
  INFO("predict -A: status " << p.status << "\n" << reported(p));
  REQUIRE(p.status == 0);
  REQUIRE(contains(p.out, "Energy RMSE"));
  std::string base = slurp("config.train");
  const std::string linear = "MODEL M_BLR BF_Linear\n";
  const auto at = base.find(linear);
  REQUIRE(at != std::string::npos);
  base.replace(at, linear.size(), "MODEL M_KRR Kern_RBF 1\nSBASIS 3\n");
  std::ofstream("config.a") << base << "MPARAMS 1.0\nOUTFILE pot_a.tadah\n";
  const CliRun k = run_tadah({"tadah", "train", "-c", "config.a", "-v", "1"});
  INFO("Kern_RBF train: status " << k.status << "\n" << reported(k));
  CHECK(k.status == 0);
  CHECK(lines_with(k.out, "Kern_RBF | sigma: 1 | gamma: 1") == 1u);
}

/* ---- [audit]: AUDIT error and the reach of a density ------------------ */

namespace {

/* Writes bcc.tadah, three bcc Ta cells (a = 3.2, 3.3 and 3.4 A, two atoms
 * each), and @p name, a DM_mBlipEnv fit of them (the many-body part of the
 * Ta seed of the HPO tests: CGRIDMB @p cgridmb, SGRIDMB 0.2, RCUTMB 5.6)
 * with AUDIT error, to @p outfile. */
void write_bcc_mblipenv(const std::string& name, const std::string& cgridmb,
                        const std::string& outfile) {
  {
    std::ofstream os("bcc.tadah");
    int n = 0;
    for (const char* a : {"3.2", "3.3", "3.4"}) {
      const double h = std::stod(a) / 2.0;
      os << "bcc_ta_" << ++n << "\n1 1 1\n" << -11.8 + 0.1 * n << "\n"
         << a << " 0 0\n0 " << a << " 0\n0 0 " << a << "\n"
         << "0 0 0\n0 0 0\n0 0 0\n"
         << "Ta 0 0 0 0 0 0\nTa " << h << " " << h << " " << h
         << " 0 0 0\n\n";
    }
  }
  std::ofstream(name)
      << "DBFILE bcc.tadah\nATOM Ta\nWATOM 1\nINIT2B false\nINITMB true\n"
         "TYPEMB DM_mBlipEnv 0 1 1 7 7 Ta Ta\nRCUTENV 5.0\nRCTYPEMB Cut_Poly\n"
         "RCUTMB 5.6\nSGRIDMB 0.2\nCGRIDMB " << cgridmb << "\n"
         "SEMBFUNC 1 1 1 1 1 1 1\nCEMBFUNC 1 2 3 4 5 6 7\n"
         "FORCE false\nSTRESS false\nBIAS false\nNORM false\n"
         "MODEL M_BLR BF_Linear\nLAMBDA 1e-3\nOALGO 1\nAUDIT error\n"
         "OUTFILE " << outfile << "\n";
}

}  // namespace

/* Pins: tadah train with AUDIT error trains the Ta seed of the HPO tests,
 * whose radial GTO is centred at the origin (CGRIDMB 0, SGRIDMB 0.2:
 * exp(-0.2 * 2.77^2) = 0.22 at the nearest neighbour of bcc Ta at a = 3.2
 * A): exit 0 and the potential written.
 * Before the fix: the audit failed the seed ("CGRIDMB=0.000000 is outside
 * the dataset's r_ij range ... ρ will be ~0 for every atom"), and the
 * refusal ended the process with std::exit(EXIT_FAILURE), this test
 * program with it (exit status 1, no test case after it run). */
TEST_CASE("Subcommand: train with AUDIT error trains a Gaussian density "
          "centred at the origin whose width reaches the neighbours",
          "[TadahCLI][exit-status][audit]") {
  ScratchCwd cwd("audit_reach");
  write_bcc_mblipenv("seed.train", "0.0", "seed.tadah");
  const CliRun r = run_tadah({"tadah", "train", "-c", "seed.train"});
  INFO("status " << r.status << "\n" << reported(r));
  CHECK(r.status == 0);
  CHECK(std::filesystem::exists("seed.tadah"));
}

/* Pins: tadah train with AUDIT error refuses a configuration with a FAIL
 * finding with exit status 1 returned to its caller, the program going
 * on: a radial GTO centred 30 A away (CGRIDMB 30) is refused naming the
 * finding ("AUDIT=error", "CGRIDMB"), and no potential is written; this
 * test case runs to its end in the process that ran tadah.
 * Before the fix: TrainEngine called std::exit(EXIT_FAILURE) inside the
 * library, which ended this test program (exit status 1) before any check
 * ran. */
TEST_CASE("Subcommand: train with AUDIT error refuses a FAIL finding with "
          "exit status 1, in the process",
          "[TadahCLI][exit-status][audit]") {
  ScratchCwd cwd("audit_refusal");
  write_bcc_mblipenv("far.train", "30.0", "far.tadah");
  const CliRun r = run_tadah({"tadah", "train", "-c", "far.train"});
  const std::string rep = reported(r);
  INFO("status " << r.status << "\n" << rep);
  CHECK(r.status == 1);
  CHECK(contains(rep, "AUDIT=error"));
  CHECK(contains(rep, "CGRIDMB"));
  CHECK_FALSE(std::filesystem::exists("far.tadah"));
}

/* ---- [command_keys]: a key of another command in a -c file ------------ */

/* Pins: tadah train -c with a line of a key that only another command
 * reads -- ANALYTICS (tadah predict), REFIT_MODE (tadah refit), INDEX
 * (tadah data print and others) -- exits 1 naming the key, its line and
 * file, the command that reads it and tadah train ("Key ANALYTICS on line
 * 18 of extra.train is not accepted here: ... tadah predict ... a
 * configuration file of tadah train"), and writes no potential; a
 * potential written before with an ANALYTICS true line (as tadah train of
 * an earlier version wrote it) is read by tadah predict without the line,
 * with one notice on standard error ("Key ANALYTICS is ignored: ..."), and
 * predicts (exit 0).
 * Before the fix: each training exited 0 and wrote the key into the
 * potential as a key line, where it changed nothing; the prediction gave
 * no notice. */
TEST_CASE("Subcommand: a configuration file with a key of another command "
          "is refused, and a potential's line of one is not read",
          "[TadahCLI][exit-status][command_keys]") {
  ScratchCwd cwd("command_keys");
  write_ta_dimer_training();
  const std::string base = slurp("config.train");
  const std::size_t line =
      static_cast<std::size_t>(std::count(base.begin(), base.end(), '\n')) + 1;
  struct Case {
    const char* text;
    const char* key;
    const char* owner;
  };
  for (const Case& c : {Case{"ANALYTICS true", "ANALYTICS", "tadah predict"},
                        Case{"REFIT_MODE dense", "REFIT_MODE", "tadah refit"},
                        Case{"INDEX 1,3", "INDEX", "tadah data print"}}) {
    std::filesystem::remove("pot.tadah");
    std::ofstream("extra.train") << base << c.text << "\n";
    const CliRun r = run_tadah({"tadah", "train", "-c", "extra.train"});
    const std::string rep = reported(r);
    INFO(c.text << ": status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    CHECK(contains(rep, "Key " + std::string(c.key) + " on line " +
                            std::to_string(line) +
                            " of extra.train is not accepted here"));
    CHECK(contains(rep, c.owner));
    CHECK(contains(rep, "a configuration file of tadah train"));
    CHECK_FALSE(std::filesystem::exists("pot.tadah"));
  }

  std::filesystem::remove("pot.tadah");
  const CliRun t = run_tadah({"tadah", "train", "-c", "config.train"});
  INFO("train: status " << t.status << "\n" << reported(t));
  REQUIRE(t.status == 0);
  std::ofstream("old_pot.tadah") << slurp("pot.tadah") << "ANALYTICS true\n";
  const CliRun p = run_tadah(
      {"tadah", "predict", "-d", "db.tadah", "-p", "old_pot.tadah"});
  const std::string rp = reported(p);
  INFO("predict: status " << p.status << "\n" << rp);
  CHECK(p.status == 0);
  CHECK(lines_with(p.err, "Key ANALYTICS is ignored") == 1u);
  CHECK(contains(p.err, "old_pot.tadah"));
  CHECK(contains(p.err, "tadah predict"));
}

/* Pins: tadah data print leaves the format of standard output as it found
 * it: `tadah data print -s nacl_p1.cif` (exit 0) and then, in the same
 * process, `tadah train -c config.a -v 1` on the three Ta dimers with
 * M_KRR Kern_RBF (MPARAMS 1.0) print the kernel's settings line "Kern_RBF |
 * sigma: 1 | gamma: 1" (the kernel copies the format of std::cout).
 * Before the fix: printing the structure left std::cout std::fixed at the
 * printer's precision, and the line read "Kern_RBF | sigma: 1.000000 |
 * gamma: 1.000000". */
TEST_CASE("Subcommand: tadah data print leaves the format of standard "
          "output as it found it",
          "[TadahCLI][kernel_line][stream_state]") {
  ScratchCwd cwd("print_stream_state");
  { std::ofstream os("nacl_p1.cif"); os << kNaClP1Cif; }
  const CliRun p = run_tadah({"tadah", "data", "print", "-s", "nacl_p1.cif"});
  INFO("data print: status " << p.status << "\n" << reported(p));
  REQUIRE(p.status == 0);
  REQUIRE(contains(p.out, "Volume"));
  write_ta_dimer_training();
  std::string base = slurp("config.train");
  const std::string linear = "MODEL M_BLR BF_Linear\n";
  const auto at = base.find(linear);
  REQUIRE(at != std::string::npos);
  base.replace(at, linear.size(), "MODEL M_KRR Kern_RBF 1\nSBASIS 3\n");
  std::ofstream("config.a") << base << "MPARAMS 1.0\nOUTFILE pot_a.tadah\n";
  const CliRun k = run_tadah({"tadah", "train", "-c", "config.a", "-v", "1"});
  INFO("Kern_RBF train: status " << k.status << "\n" << reported(k));
  CHECK(k.status == 0);
  CHECK(lines_with(k.out, "Kern_RBF | sigma: 1 | gamma: 1") == 1u);
}

namespace {

/* Writes @p name, a fit of bcc.tadah (written by write_bcc_mblipenv) with
 * AUDIT error by TYPEMB DM_mJoin of DM_EAD 0 1 1 (a Gaussian channel,
 * CGRIDMB(1)) and DM_Blip 0 2 2 (two blips, CGRIDMB(2) and (3)), CGRIDMB
 * @p cgridmb, every SGRIDMB 1, RCUTMB 5.6 5.6, to @p outfile. */
void write_bcc_joined_density(const std::string& name,
                              const std::string& cgridmb,
                              const std::string& outfile) {
  std::ofstream(name)
      << "DBFILE bcc.tadah\nATOM Ta\nWATOM 1\nINIT2B false\nINITMB true\n"
         "TYPEMB DM_mJoin\nTYPEMB DM_EAD 0 1 1 Ta Ta\n"
         "TYPEMB DM_Blip 0 2 2 Ta Ta\nRCTYPEMB Cut_Cos Cut_Cos\n"
         "RCUTMB 5.6 5.6\nCGRIDMB " << cgridmb << "\nSGRIDMB 1 1 1\n"
         "FORCE false\nSTRESS false\nBIAS false\nNORM false\n"
         "MODEL M_BLR BF_Linear\nLAMBDA 1e-3\nOALGO 1\nAUDIT error\n"
         "OUTFILE " << outfile << "\n";
}

}  // namespace

/* Pins: tadah train with AUDIT error judges each descriptor of a DM_mJoin
 * line by its own channels: DM_EAD's Gaussian at the origin (CGRIDMB(1) =
 * 0, SGRIDMB 1: 5e-4 at the nearest neighbour of bcc Ta, 2.77 A) joined
 * with DM_Blip's blips at 3 and 3.5 trains (exit 0, the potential
 * written); with the second blip at 30 A the run exits 1 naming
 * "DM_Blip.CGRIDMB" and "CGRIDMB(3) = 30", and nothing of CGRIDMB(1).
 * Before the fix: the first exited 1, DM_Blip judging the Gaussian at the
 * origin as a blip ("[FAIL] DM_Blip.CGRIDMB -- CGRIDMB(1) = 0 with
 * SGRIDMB(1) = 1: the support of the blip ... holds no neighbour
 * distance"), while a lone DM_EAD with that channel trains; the second
 * named CGRIDMB(1) as well. */
TEST_CASE("Subcommand: train with AUDIT error judges each descriptor of a "
          "DM_mJoin line by its own channels",
          "[TadahCLI][exit-status][audit]") {
  ScratchCwd cwd("audit_join");
  write_bcc_mblipenv("unused.train", "0.0", "unused.tadah");
  write_bcc_joined_density("join.train", "0.0 3.0 3.5", "join.tadah");
  const CliRun r = run_tadah({"tadah", "train", "-c", "join.train"});
  INFO("status " << r.status << "\n" << reported(r));
  CHECK(r.status == 0);
  CHECK(std::filesystem::exists("join.tadah"));
  write_bcc_joined_density("far.train", "0.0 3.0 30", "far.tadah");
  const CliRun f = run_tadah({"tadah", "train", "-c", "far.train"});
  const std::string rep = reported(f);
  INFO("far: status " << f.status << "\n" << rep);
  CHECK(f.status == 1);
  CHECK(contains(rep, "DM_Blip.CGRIDMB"));
  CHECK(contains(rep, "CGRIDMB(3) = 30"));
  CHECK_FALSE(contains(rep, "CGRIDMB(1)"));
  CHECK_FALSE(std::filesystem::exists("far.tadah"));
}

namespace {

/* The model lines of a fit of bcc.tadah. */
const char* const kBlockModel =
    "DBFILE bcc.tadah\nATOM Ta\nWATOM 1\n"
    "FORCE false\nSTRESS false\nBIAS false\nNORM false\n"
    "MODEL M_BLR BF_Linear\nLAMBDA 1e-3\nOALGO 1\n";

}  // namespace

/* Pins: tadah train with AUDIT error refuses a D2_Knot5 seed whose cutoff
 * holds no neighbour distance (RCUT2B 2, knots 1.5 and 2, on bcc Ta cells
 * whose nearest neighbours are 2.77 A apart or more): exit 1, the FAIL
 * finding naming D2_Knot5.RCUT2B, and no potential written.
 * Before the fix: D2_Knot5's cutoff had no check: the audit passed, and a
 * potential whose descriptor is 0 for every structure was trained (exit
 * 0). */
TEST_CASE("Subcommand: train with AUDIT error refuses a cutoff that holds "
          "no neighbour distance",
          "[TadahCLI][exit-status][audit]") {
  ScratchCwd cwd("audit_short_cutoff");
  write_bcc_mblipenv("unused.train", "0.0", "unused.tadah");
  std::ofstream("knot.train")
      << kBlockModel
      << "INIT2B true\nINITMB false\nTYPE2B D2_Knot5 2 2 Ta Ta\n"
         "RCTYPE2B Cut_Dummy\nRCUT2B 2.0\nCGRID2B 1.5 2.0\nKDEG2B 5 5\n"
         "AUDIT error\nOUTFILE knot.tadah\n";
  const CliRun r = run_tadah({"tadah", "train", "-c", "knot.train"});
  INFO("status " << r.status << "\n" << reported(r));
  CHECK(r.status == 1);
  CHECK(contains(reported(r), "D2_Knot5.RCUT2B"));
  CHECK_FALSE(std::filesystem::exists("knot.tadah"));
}

namespace {

/* The lines of a DESCRIPTOR block of a DM_Embed component, ta, whose
 * density cutoff has RADIUS @p radius (blips at 0.5, 1 and 1.5 A). */
std::string embed_component_lines(const std::string& radius) {
  return "DESCRIPTOR ta DM_Embed\nPAIRS Ta Ta\nDENSITY\nBASIS Blip5\n"
         "FORM NONNEG\nCENTRES 0.5 1.0 1.5\nETA 1.5 1.5 1.5\n"
         "AMPLITUDES 0.4 0.2 0.1\nCUTOFF\nTYPE Cut_Poly\nRADIUS " +
         radius +
         "\nENDCUTOFF\nENDDENSITY\nEMBEDDING\nBASIS Gaussian\n"
         "CENTRES 0.0 0.5 1.0 1.5\nWIDTHS 0.5 0.5 0.5 0.5\n"
         "REFERENCE ZERO\nENVELOPE\nTYPE Smoothstep2\nCUTOFF 2.0\n"
         "ENDENVELOPE\nENDEMBEDDING\nENDDESCRIPTOR\n";
}

}  // namespace

/* Pins: tadah train with AUDIT error refuses a DM_Embed component whose
 * density cutoff holds no neighbour distance (RADIUS 2 on bcc Ta cells
 * whose nearest neighbours are 2.77 A apart or more): exit 1, the FAIL
 * finding naming TA.DENSITY.CUTOFF.RADIUS, and no potential written; with
 * RADIUS 5.6 the same configuration trains (exit 0, the potential
 * written).
 * Before the fix: DM_Embed's metadata had no cutoff check: the audit
 * passed, and a potential whose density is 0 for every atom was trained
 * (exit 0). */
TEST_CASE("Subcommand: train with AUDIT error refuses a DESCRIPTOR block "
          "whose density cutoff holds no neighbour distance",
          "[TadahCLI][exit-status][audit]") {
  ScratchCwd cwd("audit_embed_cutoff");
  write_bcc_mblipenv("unused.train", "0.0", "unused.tadah");
  std::ofstream("short.train")
      << kBlockModel << "INIT2B false\nINITMB true\n"
      << embed_component_lines("2.0")
      << "AUDIT error\nOUTFILE short.tadah\n";
  const CliRun r = run_tadah({"tadah", "train", "-c", "short.train"});
  INFO("RADIUS 2: status " << r.status << "\n" << reported(r));
  CHECK(r.status == 1);
  CHECK(contains(reported(r), "TA.DENSITY.CUTOFF.RADIUS"));
  CHECK_FALSE(std::filesystem::exists("short.tadah"));
  std::ofstream("wide.train")
      << kBlockModel << "INIT2B false\nINITMB true\n"
      << embed_component_lines("5.6")
      << "AUDIT error\nOUTFILE wide.tadah\n";
  const CliRun w = run_tadah({"tadah", "train", "-c", "wide.train"});
  INFO("RADIUS 5.6: status " << w.status << "\n" << reported(w));
  CHECK(w.status == 0);
  CHECK(std::filesystem::exists("wide.tadah"));
}

#ifdef TADAH_ENABLE_NFP

/* ---- [preflight_audit]: tadah hpo and the critical-parameter audit ---- */

namespace {

/* Writes, in the working directory, val.db (one rattled bcc Ta cell),
 * config.val, fw.train (D2_BP of 5 functions, every weight pinned, no
 * training DBFILE, AUDIT off: the default) and fw.hpo (OPTIM FIXWEIGHT
 * (1) alone), and wide.train (the same model with DBFILE val.db, no
 * FIXINDEX, AUDIT warn) and wide.hpo (OPTIM SGRID2B (1) alone). */
void write_audit_fixture() {
  std::ofstream("val.db")
      << "rattled_bcc_ta_1\n1.0 1.0 1.0\n-0.30 0.0\n"
         "3.301 0.0 0.0\n0.0 3.301 0.0\n0.0 0.0 6.602\n"
         "0.0 0.0 0.0\n0.0 0.0 0.0\n0.0 0.0 0.0\n"
         "Ta 0.05 -0.03 0.02  0.31 -0.12 0.05\n"
         "Ta 1.70 1.62 1.68  -0.22 0.17 -0.09\n"
         "Ta -0.04 0.06 3.33  0.04 0.21 -0.15\n"
         "Ta 1.61 1.69 4.92  -0.13 -0.26 0.19\n\n";
  std::ofstream("config.val") << "DBFILE val.db\n";
  const std::string model =
      "ATOM Ta\nWATOM 1\nINIT2B true\nINITMB false\n"
      "TYPE2B D2_BP 5 5 Ta Ta\n"
      "CGRID2B 0.3 0.6 1.0 1.4 1.8\nSGRID2B 2.0 2.7 3.4 4.2 5.0\n"
      "RCTYPE2B Cut_Cos\nRCUT2B 5.0\nBIAS false\nNORM false\n"
      "MODEL M_BLR BF_Linear\nLAMBDA 1e-3\nOALGO 4\n";
  std::ofstream("fw.train")
      << model << "FIXINDEX 1-5\nFIXWEIGHT -15.0 22.5 -32.5 17.5 -10.0\n";
  std::ofstream("wide.train") << model << "DBFILE val.db\nAUDIT warn\n";
  const std::string head =
      "OPTIMIZER\nLIB NLOPT\nALGO LN_SBPLX\nMAXEVAL 1\nENDOPTIMIZER\n"
      "LOSS L2\nPC_ERMSE 0 1\n";
  std::ofstream("fw.hpo") << head << "OPTIM FIXWEIGHT (1) -30.0 0.0\n";
  std::ofstream("wide.hpo") << head << "OPTIM SGRID2B (1) 1.0 3.0\n";
}

}  // namespace

/* Pins: tadah hpo --check of a search of FIXWEIGHT alone (D2_BP, every
 * weight pinned, AUDIT off: the default) prints no critical-parameter
 * warning (exit 0); with AUDIT warn and a search of SGRID2B alone, the
 * fixed RCUT2B of the searched descriptor is flagged once ("D2_BP.RCUT2B
 * -- RCUT2B(1) is a critical parameter that the seed fixes and no OPTIM
 * line searches ...", one line, naming `OPTIM RCUT2B (1)`).
 * Before the fix: a preview of the audit ran before every run and check,
 * whatever AUDIT said and whatever the search space held: the first check
 * printed the RCUT2B warning once, and the second twice (the preview, then
 * the audit of AUDIT warn); the warning read "Critical hyperparameter is
 * fixed in the seed config and is not in any OPTIM block ...", naming no
 * entry. */
TEST_CASE("Subcommand: hpo --check gives the critical-parameter warnings of "
          "AUDIT warn once, and none for a search of FIXWEIGHT alone",
          "[TadahCLI][exit-status][hpo][preflight_audit]") {
  ScratchCwd cwd("hpo_preflight");
  write_audit_fixture();
  const std::string warning =
      "is a critical parameter that the seed fixes and no OPTIM line "
      "searches";
  const CliRun fw = run_tadah({"tadah", "hpo", "--check", "-c", "fw.train",
                               "--hpotarget", "fw.hpo", "--validation",
                               "config.val"});
  INFO("FIXWEIGHT alone: status " << fw.status << "\n" << reported(fw));
  CHECK(fw.status == 0);
  CHECK(lines_with(reported(fw), warning) == 0u);
  const CliRun wide = run_tadah({"tadah", "hpo", "--check", "-c",
                                 "wide.train", "--hpotarget", "wide.hpo",
                                 "--validation", "config.val"});
  INFO("AUDIT warn: status " << wide.status << "\n" << reported(wide));
  CHECK(wide.status == 0);
  CHECK(lines_with(reported(wide), "D2_BP.RCUT2B") == 1u);
  CHECK(lines_with(reported(wide), warning) == 1u);
  CHECK(lines_with(reported(wide), "RCUT2B(1) " + warning) == 1u);
  CHECK(lines_with(reported(wide), "`OPTIM RCUT2B (1)`") == 1u);
}

/* Pins: tadah hpo --check -c of a training configuration with an
 * ANALYTICS true line (an option of tadah predict) exits 1 naming the key,
 * its line, the file, tadah predict and tadah hpo.
 * Before the fix: the check passed (exit 0) and a run would have written
 * the line into every potential. */
TEST_CASE("Subcommand: hpo -c refuses a key of another command",
          "[TadahCLI][exit-status][hpo][command_keys]") {
  ScratchCwd cwd("hpo_command_keys");
  write_audit_fixture();
  std::ofstream("extra.train") << slurp("fw.train") << "ANALYTICS true\n";
  const std::string base = slurp("fw.train");
  const std::size_t line =
      static_cast<std::size_t>(std::count(base.begin(), base.end(), '\n')) + 1;
  const CliRun r = run_tadah({"tadah", "hpo", "--check", "-c", "extra.train",
                              "--hpotarget", "fw.hpo", "--validation",
                              "config.val"});
  const std::string rep = reported(r);
  INFO("status " << r.status << "\n" << rep);
  CHECK(r.status == 1);
  CHECK(contains(rep, "Key ANALYTICS on line " + std::to_string(line) +
                          " of extra.train is not accepted here"));
  CHECK(contains(rep, "tadah predict"));
  CHECK(contains(rep, "a configuration file of tadah hpo"));
}

/* Pins: the OPTIM block tadah analysis suggest_bounds writes for a seed is
 * accepted by tadah hpo --check with that seed: for D2_Blip (four blips,
 * RCUT2B 5.6) and DM_EAD with Gaussian channels at 2.9, 3.3 and 4.7 A
 * (SGRIDMB 4, RCUTMB 5.6) on three bcc Ta cells, suggest_bounds exits 0,
 * and a target file of an optimiser, PC_ERMSE and the block passes tadah
 * hpo --check (exit 0): every box holds the seed's value.
 * Before the fix: the RCUT2B and RCUTMB boxes ended at the 99th percentile
 * of the distances within the seed's cutoff, below 5.6, and every CGRIDMB
 * box was the first shell's, which 4.7 lies outside: tadah hpo --check
 * exited 1 ("INIT[OUTER] starting point 1: ... lies outside its OPTIM
 * bounds"). */
TEST_CASE("Subcommand: the block of analysis suggest_bounds passes hpo "
          "--check with its seed",
          "[TadahCLI][exit-status][hpo][suggest_bounds]") {
  ScratchCwd cwd("suggest_bounds_hpo");
  write_bcc_mblipenv("unused.train", "0.0", "unused.tadah");
  std::ofstream("seed.train")
      << "DBFILE bcc.tadah\nATOM Ta\nWATOM 1\nINIT2B true\nINITMB true\n"
         "TYPE2B D2_Blip 4 4 Ta Ta\nRCTYPE2B Cut_Cos\nRCUT2B 5.6\n"
         "CGRID2B 2.5 3.0 3.5 4.0\nSGRID2B 1 1 1 1\n"
         "TYPEMB DM_EAD 0 3 3 Ta Ta\nRCTYPEMB Cut_Cos\nRCUTMB 5.6\n"
         "CGRIDMB 2.9 3.3 4.7\nSGRIDMB 4 4 4\n"
         "FORCE false\nSTRESS false\nBIAS false\nNORM false\n"
         "MODEL M_BLR BF_Linear\nLAMBDA 1e-3\nOALGO 1\n";
  std::ofstream("config.val") << "DBFILE bcc.tadah\n";
  const CliRun s = run_tadah({"tadah", "analysis", "suggest_bounds", "-c",
                              "seed.train", "-o", "block.hpo"});
  INFO("suggest_bounds: status " << s.status << "\n" << reported(s));
  REQUIRE(s.status == 0);
  std::ofstream("target.hpo")
      << "OPTIMIZER\nLIB NLOPT\nALGO LN_SBPLX\nMAXEVAL 1\nENDOPTIMIZER\n"
         "LOSS L2\nPC_ERMSE 0 1\n" << slurp("block.hpo");
  const CliRun c = run_tadah({"tadah", "hpo", "--check", "-c", "seed.train",
                              "--hpotarget", "target.hpo", "--validation",
                              "config.val"});
  INFO("hpo --check: status " << c.status << "\n" << reported(c) << "\n"
                              << slurp("target.hpo"));
  CHECK(c.status == 0);
}

namespace {

/* The second value of the line of @p block that starts with @p prefix
 * ("OPTIM RCUT2B (1) "): its high bound; -1 when there is no such line. */
double high_bound(const std::string& block, const std::string& prefix) {
  std::istringstream in(block);
  for (std::string line; std::getline(in, line);) {
    if (line.compare(0, prefix.size(), prefix) != 0) continue;
    std::istringstream values(line.substr(prefix.size()));
    double lo = 0.0, hi = -1.0;
    values >> lo >> hi;
    return hi;
  }
  return -1.0;
}

/* The comment line before the line of @p block that starts with @p prefix
 * ("" when there is none). */
std::string comment_before(const std::string& block,
                           const std::string& prefix) {
  std::istringstream in(block);
  std::string last;
  for (std::string line; std::getline(in, line);) {
    if (line.compare(0, prefix.size(), prefix) == 0) return last;
    if (!line.empty()) last = line;
  }
  return "";
}

}  // namespace

/* Pins: the block tadah analysis suggest_bounds writes for each of these
 * seeds, on three bcc Ta cells, is accepted by tadah hpo --check with that
 * seed (an optimiser, PC_ERMSE and the block; exit 0):
 *  - "ta": the Ta seed of the HPO tests (D2_Blip with LIN grids, RCUT2B
 *    5.6, and DM_mBlipEnv with CGRIDMB 0), whose RCUT2B box ends past 6 A
 *    (at the 99th percentile of the distances up to 7 A, which no distance
 *    within 5.6 A gives), its comment naming the 99th percentile of the
 *    distances and their reach, 1.25 times the largest cutoff of the seed;
 *  - "zbl": D2_ZBL (its TYPE2B values have no OPTIM line);
 *  - "join": DM_mJoin of two DM_mBlipEnv, which share RCUTENV;
 *  - "blip": DM_Blip with a centre at the origin and one at 4.7 A;
 *  - "short": D2_Blip and DM_mBlipEnv with cutoffs of 2 A, which hold no
 *    neighbour distance (the block keeps the fixed ranges of RCUTENV,
 *    CEMBFUNC and SEMBFUNC).
 * Before the fix: "ta" passed tadah hpo --check, but its RCUT2B box was
 * 5.54 to 5.64, below 6 (the 99th percentile of the distances within 5.6
 * to 1.05 times the second peak of the distances, a reversed range the
 * block swapped), its comment the scale and the role of RCUT2B; "join" was
 * refused ("the index is already selected by the OPTIM
 * RCUTENV line"), and "short" too ("the low bound 0 is not > 0", an
 * SGRID2B box widened from (0, 0)). */
TEST_CASE("Subcommand: the block of analysis suggest_bounds passes hpo "
          "--check for the Ta seed, D2_ZBL, a DM_mJoin line, a blip at the "
          "origin and cutoffs that hold no neighbour",
          "[TadahCLI][exit-status][hpo][suggest_bounds]") {
  ScratchCwd cwd("suggest_bounds_seeds");
  write_bcc_mblipenv("unused.train", "0.0", "unused.tadah");
  std::ofstream("config.val") << "DBFILE bcc.tadah\n";
  const std::string mblipenv =
      "TYPEMB DM_mBlipEnv 0 1 1 7 7 Ta Ta\nRCUTENV 5.0\nRCTYPEMB Cut_Poly\n"
      "SGRIDMB 0.2\nCGRIDMB 0.0\nSEMBFUNC 1 1 1 1 1 1 1\n"
      "CEMBFUNC 1 2 3 4 5 6 7\n";
  const std::vector<std::pair<std::string, std::string>> seeds = {
      {"ta",
       "INIT2B true\nINITMB true\nTYPE2B D2_Blip 7 7 Ta Ta\n"
       "RCTYPE2B Cut_Poly\nSGRID2B LIN 7 1 1.2\nCGRID2B LIN 7 0.1 3.0\n"
       "RCUT2B 5.6\nRCUTMB 5.6\n" + mblipenv},
      {"zbl",
       "INIT2B true\nINITMB false\n"
       "TYPE2B D2_ZBL 0.5292 0.88534 0.23 0.23 Ta Ta\nRCTYPE2B Cut_Cos\n"
       "RCUT2B 5.0\n"},
      {"join",
       "INIT2B false\nINITMB true\nTYPEMB DM_mJoin\n"
       "TYPEMB DM_mBlipEnv 0 1 1 3 3 Ta Ta\n"
       "TYPEMB DM_mBlipEnv 0 1 1 3 3 Ta Ta\nRCUTENV 5.0\n"
       "RCTYPEMB Cut_Cos Cut_Cos\nRCUTMB 5.6 5.0\nSGRIDMB 0.2 0.3\n"
       "CGRIDMB 0.0 2.9\nSEMBFUNC 1 1 1 1 1 1\nCEMBFUNC 1 2 3 1 2 3\n"},
      {"blip",
       "INIT2B false\nINITMB true\nTYPEMB DM_Blip 0 2 2 Ta Ta\n"
       "RCTYPEMB Cut_Cos\nRCUTMB 5.6\nCGRIDMB 0 4.7\nSGRIDMB 0.8 1\n"},
      {"short",
       "INIT2B true\nINITMB true\nTYPE2B D2_Blip 2 2 Ta Ta\n"
       "RCTYPE2B Cut_Cos\nRCUT2B 2.0\nCGRID2B 1.0 1.5\nSGRID2B 1 1\n"
       "RCUTMB 2.0\n" + mblipenv},
  };
  for (const auto& [name, lines] : seeds) {
    CAPTURE(name);
    const std::string train = name + ".train";
    const std::string block = name + ".block";
    const std::string target = name + ".hpo";
    std::ofstream(train) << kBlockModel << lines;
    const CliRun s = run_tadah({"tadah", "analysis", "suggest_bounds", "-c",
                                train, "-o", block});
    INFO("suggest_bounds: status " << s.status << "\n" << reported(s));
    REQUIRE(s.status == 0);
    std::ofstream(target)
        << "OPTIMIZER\nLIB NLOPT\nALGO LN_SBPLX\nMAXEVAL 1\nENDOPTIMIZER\n"
           "LOSS L2\nPC_ERMSE 0 1\n" << slurp(block);
    const CliRun c = run_tadah({"tadah", "hpo", "--check", "-c", train,
                                "--hpotarget", target, "--validation",
                                "config.val"});
    INFO("hpo --check: status " << c.status << "\n" << reported(c) << "\n"
                                << slurp(target));
    CHECK(c.status == 0);
    if (name != "ta") continue;
    const std::string text = slurp(block);
    CHECK(high_bound(text, "OPTIM RCUT2B (1) ") > 6.0);
    const std::string why = comment_before(text, "OPTIM RCUT2B (1) ");
    CHECK(contains(why, "99th percentile"));
    CHECK(contains(why, "1.25 times"));
  }
}

/* Pins: the block tadah analysis suggest_bounds writes for a seed of
 * D2_Knot5 (knots 3 4 5, RCUT2B 5) and a knot DM_REAM density (knots 3 4
 * 5, RCUTMB 5), each alone on its TYPE line, on three bcc Ta cells, has no
 * OPTIM RCUT2B or RCUTMB line (and one OPTIM CGRID2B line, the knots), and
 * tadah hpo --check with that seed passes
 * it (exit 0) without the warning that an OPTIM line of a knot cutoff has
 * no effect ("has no effect for the knot basis family").
 * Before the fix: the block had "OPTIM RCUTMB (1) ...", and tadah hpo
 * --check warned "OPTIM RCUTMB has no effect for the knot basis family:
 * ... (a wasted search dimension)". */
TEST_CASE("Subcommand: the block of analysis suggest_bounds for knot "
          "channels passes hpo --check without an OPTIM line of a knot "
          "cutoff",
          "[TadahCLI][exit-status][hpo][suggest_bounds][knot]") {
  ScratchCwd cwd("suggest_bounds_knots");
  write_bcc_mblipenv("unused.train", "0.0", "unused.tadah");
  std::ofstream("config.val") << "DBFILE bcc.tadah\n";
  std::ofstream("knot.train")
      << kBlockModel
      << "INIT2B true\nINITMB true\nTYPE2B D2_Knot5 3 0 Ta Ta\n"
         "RCTYPE2B Cut_Dummy\nRCUT2B 5.0\nCGRID2B 3 4 5\n"
         "TYPEMB DM_REAM 2 3 0 3 3 0 0 0 Ta Ta\nRCTYPEMB Cut_Dummy\n"
         "RCUTMB 5.0\nCGRIDMB 3 4 5\nAMPGRIDMB 0.3 0.2 0.1\n"
         "CEMBFUNC 0 1 2\nRHOFORM SIGNED\nEMBF0 FREE\n";
  const CliRun s = run_tadah({"tadah", "analysis", "suggest_bounds", "-c",
                              "knot.train", "-o", "knot.block"});
  INFO("suggest_bounds: status " << s.status << "\n" << reported(s));
  REQUIRE(s.status == 0);
  const std::string block = slurp("knot.block");
  INFO(block);
  // A line of the block, not its comments, which name the OPTIM lines that
  // tadah hpo warns about.
  auto lines_starting = [&block](const std::string& prefix) {
    std::size_t n = 0;
    std::istringstream in(block);
    for (std::string line; std::getline(in, line);)
      if (line.compare(0, prefix.size(), prefix) == 0) ++n;
    return n;
  };
  CHECK(lines_starting("OPTIM RCUT2B") == 0u);
  CHECK(lines_starting("OPTIM RCUTMB") == 0u);
  CHECK(lines_starting("OPTIM CGRID2B") == 1u);
  std::ofstream("knot.hpo")
      << "OPTIMIZER\nLIB NLOPT\nALGO LN_SBPLX\nMAXEVAL 1\nENDOPTIMIZER\n"
         "LOSS L2\nPC_ERMSE 0 1\n" << block;
  const CliRun c = run_tadah({"tadah", "hpo", "--check", "-c", "knot.train",
                              "--hpotarget", "knot.hpo", "--validation",
                              "config.val"});
  INFO("hpo --check: status " << c.status << "\n" << reported(c));
  CHECK(c.status == 0);
  CHECK(lines_with(reported(c), "has no effect for the knot basis family") ==
        0u);
}

#endif  // TADAH_ENABLE_NFP

/* ---- [inert]: keys that change nothing a fit gives -------------------- */

namespace {

/* A D2_EAM + DM_EAM training configuration on the small Ta setfl, with
 * WATOM @p watom on line 3 ("config"), and its dataset (three bcc cells). */
void write_eam_training(const std::string& watom) {
  { std::ofstream os("eam_small.alloy"); os << small_ta_setfl(); }
  std::ofstream("db.tadah") << bcc_ta_entry("bcc-Ta-3.25", 3.25, -16.2)
                            << bcc_ta_entry("bcc-Ta-3.30", 3.30, -16.3)
                            << bcc_ta_entry("bcc-Ta-3.35", 3.35, -16.1);
  std::ofstream("config")
      << "DBFILE db.tadah\nATOM Ta\nWATOM " << watom << "\n"
      << "INIT2B true\nTYPE2B D2_EAM 0 Ta Ta\nRCTYPE2B Cut_Dummy\n"
         "RCUT2B 4.5\nINITMB true\nTYPEMB DM_EAM 0 Ta Ta\n"
         "RCTYPEMB Cut_Dummy\nRCUTMB 4.5\nSETFL eam_small.alloy\n"
         "FORCE false\nSTRESS false\nBIAS false\nNORM false\nVERBOSE 0\n"
         "MODEL M_BLR BF_Linear\nLAMBDA 1e-3\nOALGO 1\n";
}

}  // namespace

/* Pins: `tadah train -c` refuses, with status 1 and without writing a
 * potential, a configuration with a line that changes nothing a fit gives,
 * naming the key, its values, the line and the file:
 *  - MBLOCK 64, NBLOCK 64, MPIWPCKG 50 (deprecated), EMBF0 FREE and RHOFORM
 *    SIGNED (no DM_REAM block) on line 18 of config.train (a D2_BP fit):
 *    "tadah train: the configuration has a line that changes nothing a fit
 *    gives, and a fit refuses it: MBLOCK 64 (line 18 of config.train): ...";
 *  - WATOM 2.5 (line 3 of config) with D2_EAM and DM_EAM, which take their
 *    functions from the SETFL file unweighted: "WATOM 2.5 (line 3 of
 *    config): no descriptor of the configuration reads it (D2_EAM and
 *    DM_EAM ...)".
 * The same configurations without those lines, and with WATOM 1, train.
 * Before the fix: each trained (exit 0, pot.tadah): MBLOCK, NBLOCK,
 * MPIWPCKG, EMBF0 and RHOFORM with a WARNING, WATOM without one. */
TEST_CASE("Subcommand: train refuses a line that changes nothing a fit "
          "gives, naming the key, the line and the file",
          "[TadahCLI][exit-status][train][inert]") {
  ScratchCwd cwd("train_inert");
  for (const std::string line : {"MBLOCK 64", "NBLOCK 64", "MPIWPCKG 50",
                                 "EMBF0 FREE", "RHOFORM SIGNED"}) {
    INFO(line);
    write_ta_dimer_training();
    std::ofstream("config.train", std::ios::app) << line << "\n";
    std::filesystem::remove("pot.tadah");
    const CliRun r = run_tadah({"tadah", "train", "-c", "config.train"});
    const std::string rep = reported(r);
    INFO("status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    CHECK(contains(rep, "tadah train: the configuration has a line that "
                        "changes nothing a fit gives, and a fit refuses it: " +
                            line + " (line 18 of config.train): "));
    CHECK_FALSE(std::filesystem::exists("pot.tadah"));
  }
  {
    write_eam_training("2.5");
    std::filesystem::remove("pot.tadah");
    const CliRun r = run_tadah({"tadah", "train", "-c", "config"});
    const std::string rep = reported(r);
    INFO("WATOM 2.5: status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    CHECK(contains(rep, "WATOM 2.5 (line 3 of config): no descriptor of the "
                        "configuration reads it (D2_EAM and DM_EAM"));
    CHECK_FALSE(std::filesystem::exists("pot.tadah"));
  }
  {
    write_ta_dimer_training();
    const CliRun r = run_tadah({"tadah", "train", "-c", "config.train"});
    INFO("no inert line: status " << r.status << "\n" << reported(r));
    CHECK(r.status == 0);
    write_eam_training("1");
    const CliRun e = run_tadah({"tadah", "train", "-c", "config"});
    INFO("WATOM 1: status " << e.status << "\n" << reported(e));
    CHECK(e.status == 0);
  }
}

/* Pins: `tadah train` of a D2_EAM + DM_EAM configuration without a WATOM
 * line exits 0, writes a potential without a WATOM line, and says nothing
 * of WATOM; `tadah predict` of that potential says nothing of WATOM either.
 * (WATOM = Z is given only to a configuration a descriptor of which reads
 * WATOM.)
 * Before the fix: the potential had a WATOM 73 line, the WATOM train gives
 * a configuration without one; with the refusal of a WATOM no descriptor
 * reads, train then gave the notice of WATOM 73 and refused it (exit 1). */
TEST_CASE("Subcommand: train of a D2_EAM and DM_EAM configuration without "
          "WATOM gives it no WATOM",
          "[TadahCLI][exit-status][train][inert]") {
  ScratchCwd cwd("train_eam_no_watom");
  write_eam_training("1");
  {
    std::istringstream in(slurp("config"));
    std::ofstream out("config_nowatom");
    for (std::string l; std::getline(in, l);)
      if (l.rfind("WATOM", 0) != 0) out << l << "\n";
  }
  const CliRun t = run_tadah({"tadah", "train", "-c", "config_nowatom", "-o",
                              "pot.tadah"});
  INFO("train: status " << t.status << "\n" << reported(t));
  CHECK(t.status == 0);
  CHECK(count_lines_with(reported(t), "WATOM") == 0u);
  REQUIRE(std::filesystem::exists("pot.tadah"));
  CHECK(pot_key("pot.tadah", "WATOM").empty());
  const CliRun p = run_tadah({"tadah", "predict", "-p", "pot.tadah", "-d",
                              "db.tadah"});
  INFO("predict: status " << p.status << "\n" << reported(p));
  CHECK(p.status == 0);
  CHECK(count_lines_with(reported(p), "WATOM") == 0u);
}

/* Pins: `tadah predict` of a D2_EAM + DM_EAM potential whose WATOM is 2.5
 * (a potential trained with WATOM 1, its WATOM line edited) exits 0 with
 * one notice, "WARNING: Key WATOM (2.5) is ignored: no descriptor of the
 * potential reads it (D2_EAM and DM_EAM ...); the potential works unchanged
 * without it", and predicts the energies of the WATOM 1 potential, digit
 * for digit; the WATOM 1 potential gives no notice of WATOM.
 * Before the fix: no notice (the same energies). */
TEST_CASE("Subcommand: predict of a D2_EAM and DM_EAM potential with a "
          "WATOM other than 1 gives one notice",
          "[TadahCLI][exit-status][predict][inert]") {
  ScratchCwd cwd("predict_inert_watom");
  write_eam_training("1");
  {
    const CliRun t = run_tadah({"tadah", "train", "-c", "config", "-o",
                                "pot1.tadah"});
    INFO("train: status " << t.status << "\n" << reported(t));
    REQUIRE(t.status == 0);
  }
  {
    std::istringstream in(slurp("pot1.tadah"));
    std::ofstream out("pot25.tadah");
    for (std::string l; std::getline(in, l);) {
      std::istringstream ls(l);
      std::string key;
      ls >> key;
      out << (key == "WATOM" ? std::string("WATOM 2.5") : l) << "\n";
    }
  }
  const CliRun one = run_tadah({"tadah", "predict", "-p", "pot1.tadah", "-d",
                                "db.tadah", "-o", "e1.pred"});
  INFO("WATOM 1: status " << one.status << "\n" << reported(one));
  CHECK(one.status == 0);
  CHECK(count_lines_with(reported(one), "WATOM") == 0u);
  const CliRun two = run_tadah({"tadah", "predict", "-p", "pot25.tadah",
                                "-d", "db.tadah", "-o", "e25.pred"});
  const std::string rep = reported(two);
  INFO("WATOM 2.5: status " << two.status << "\n" << rep);
  CHECK(two.status == 0);
  CHECK(count_lines_with(rep, "Key WATOM (2.5) is ignored") == 1u);
  CHECK(contains(rep, "WARNING: Key WATOM (2.5) is ignored: no descriptor of "
                      "the potential reads it (D2_EAM and DM_EAM"));
  CHECK(contains(rep, "the potential works unchanged without it"));
  REQUIRE(std::filesystem::exists("e1.pred"));
  CHECK(slurp("e25.pred") == slurp("e1.pred"));
}

/* Pins: `tadah refit -c` of the small Ta setfl (dense Blip, coarse) refuses
 * with status 1, writing no pot.tadah, a configuration with a retired key
 * (EWEIGHT 1), a deprecated key (MBLOCK 64) or a WATOM other than 1 (WATOM
 * 2; the potential a refit writes has WATOM 1), each on line 7 of
 * config.refit, naming the line ("EWEIGHT 1 (line 7 of config.refit)");
 * with WATOM 1 it refits (exit 0).
 * Before the fix: each refitted (exit 0, pot.tadah): EWEIGHT and MBLOCK
 * with a WARNING, WATOM 2 without one (pot.tadah had WATOM 1). */
TEST_CASE("Subcommand: refit refuses a retired key, a deprecated key and a "
          "WATOM it never reads, naming the line",
          "[TadahCLI][exit-status][refit][inert]") {
  ScratchCwd cwd("refit_inert");
  { std::ofstream os("eam_small.alloy"); os << small_ta_setfl(); }
  auto config = [](const std::string& extra) {
    std::ofstream("config.refit")
        << "EAM eam_small.alloy\nRHOFORM SIGNED\nREFIT_MODE dense\n"
           "REFIT_BASIS Blip\nREFIT_LEVEL coarse\nREFIT_MAX_ESCALATIONS 0\n"
        << extra << "\n";
  };
  for (const std::string line : {"EWEIGHT 1", "MBLOCK 64", "WATOM 2"}) {
    INFO(line);
    config(line);
    std::filesystem::remove("pot.tadah");
    const CliRun r = run_tadah({"tadah", "refit", "-c", "config.refit"});
    const std::string rep = reported(r);
    INFO("status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    CHECK(contains(rep, "tadah refit: "));
    CHECK(contains(rep, line + " (line 7 of config.refit"));
    CHECK_FALSE(std::filesystem::exists("pot.tadah"));
  }
  config("WATOM 1");
  const CliRun ok = run_tadah({"tadah", "refit", "-c", "config.refit"});
  INFO("WATOM 1: status " << ok.status << "\n" << reported(ok));
  CHECK(ok.status == 0);
  CHECK(std::filesystem::exists("pot.tadah"));
}

#ifdef TADAH_ENABLE_NFP
/* Pins: `tadah hpo --check` and the run refuse, with status 1, a training
 * configuration with a line that changes nothing a fit gives: MBLOCK 64 on
 * line 18 of config.train ("tadah hpo: the configuration has a line that
 * changes nothing a fit gives, and a fit refuses it: MBLOCK 64 (line 18 of
 * config.train)"); the run writes no best_pot.tadah.
 * Before the fix: --check exited 0 and the run trained every evaluation,
 * MBLOCK giving a WARNING. */
TEST_CASE("Subcommand: hpo refuses a line that changes nothing a fit gives, "
          "by --check and the run",
          "[TadahCLI][exit-status][hpo][inert]") {
  ScratchCwd cwd("hpo_inert");
  write_ta_dimer_training();
  std::ofstream("config.train", std::ios::app) << "MBLOCK 64\n";
  std::ofstream("config.hpo") << "OPTIMIZER\nLIB NLOPT\nALGO LN_SBPLX\n"
                                 "MAXEVAL 2\nENDOPTIMIZER\nLOSS L2\n"
                                 "PC_ERMSE 0 1\nOPTIM CGRID2B (1) 2.3 2.7\n";
  for (const bool check : {true, false}) {
    INFO((check ? "--check" : "the run"));
    std::vector<std::string> args = {"tadah", "hpo", "-c", "config.train",
                                     "--hpotarget", "config.hpo"};
    if (check) args.push_back("--check");
    const CliRun r = run_tadah(args);
    const std::string rep = reported(r);
    INFO("status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    CHECK(contains(rep, "tadah hpo: the configuration has a line that "
                        "changes nothing a fit gives, and a fit refuses it: "
                        "MBLOCK 64 (line 18 of config.train)"));
    CHECK_FALSE(std::filesystem::exists("best_pot.tadah"));
  }
}

/* Pins: `tadah hpo --check` and the run refuse, with status 1, the target
 * file line `OPTIM WATOM (1) 0.5 2.0` (line 8 of config.hpo) for a D2_EAM +
 * DM_EAM training configuration with WATOM 1, whose descriptors read no
 * WATOM: "OPTIM WATOM (line 8 of the target file) is not a valid search
 * target: no descriptor of the training configuration reads WATOM ..."; the
 * run evaluates nothing and writes no best_pot.tadah.
 * Before the fix: --check exited 0 ("passed the startup checks") and the run
 * exited 0 with best_pot.tadah, every evaluation but the start scoring
 * FAILSCORE (its fit refused the candidate's WATOM, which no descriptor
 * reads). */
TEST_CASE("Subcommand: hpo refuses OPTIM WATOM when no descriptor reads "
          "WATOM, by --check and the run",
          "[TadahCLI][exit-status][hpo][inert]") {
  ScratchCwd cwd("hpo_optim_watom");
  write_eam_training("1");
  std::ofstream("config.val") << "DBFILE db.tadah\n";
  std::ofstream("config.hpo") << "OPTIMIZER\nLIB NLOPT\nALGO LN_SBPLX\n"
                                 "MAXEVAL 3\nENDOPTIMIZER\nLOSS L2\n"
                                 "PC_ERMSE 0 1\nOPTIM WATOM (1) 0.5 2.0\n";
  for (const bool check : {true, false}) {
    INFO((check ? "--check" : "the run"));
    std::vector<std::string> args = {"tadah", "hpo", "-c", "config",
                                     "--hpotarget", "config.hpo",
                                     "--validation", "config.val"};
    if (check) args.push_back("--check");
    const CliRun r = run_tadah(args);
    const std::string rep = reported(r);
    INFO("status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    CHECK(contains(rep, "OPTIM WATOM (line 8 of the target file) is not a "
                        "valid search target: no descriptor of the training "
                        "configuration reads WATOM"));
    CHECK_FALSE(contains(rep, "passed the startup checks"));
    CHECK_FALSE(std::filesystem::exists("best_pot.tadah"));
  }
}
#endif  // TADAH_ENABLE_NFP

/* Pins: the INFO lines (-v 2) of tadah data merge, split, sample, dedup,
 * balance and write, of tadah train, tadah predict, tadah refit and tadah
 * analysis cutoff name no class of the code that runs them (no line has
 * "Engine"); tadah data sample says where it writes ("tadah data sample:
 * the sampled structures are written to s.tadah"), tadah data dedup what it
 * keeps ("tadah data dedup: 3 structures in 1 file"), tadah data balance
 * what it does ("tadah data balance: shifting the energies, E -= sum_Z N_Z
 * eshift[Z]"); an error of tadah data merge and of tadah data convert
 * reaches the program (exit 1, "DBFILE does not exist: nosuch.tadah", the
 * DFT file that does not open) as every other command's does.
 * Before the fix: "Validating DataMergeEngine...", "DataMergeEngine
 * started...", "DataSampleEngine finished. Sampled data written to:
 * s.tadah", "DataDedupEngine finished. 3 structures in 1 files.",
 * "DataBalanceEngine: Applying ESHIFT (E -= sum_Z N_Z * eshift[Z])...",
 * "AnalysisCutoffEngine finished.", and the two errors printed by the
 * command itself as "DataMergeEngine error: ..." and "DataConvertEngine
 * error: ...". */
TEST_CASE("Subcommand: the log lines and errors of a command name the "
          "command, not the code that runs it",
          "[TadahCLI][data][messages]") {
  ScratchCwd cwd("engine_lines");
  std::ofstream("db.tadah") << retrain_dataset();
  std::ofstream("train.cfg") << retrain_config("M_BLR BF_Linear",
                                               "LAMBDA 0.1\nOALGO 4\n");
  std::ofstream("norefit.cfg") << "REFIT_MODE dense\n";
  const std::vector<std::vector<std::string>> runs = {
      {"tadah", "data", "merge", "-d", "db.tadah", "-o", "m.tadah", "-v", "2"},
      {"tadah", "data", "split", "-d", "db.tadah", "-o", "p1.tadah",
       "p2.tadah", "--even", "-v", "2"},
      {"tadah", "data", "sample", "-d", "db.tadah", "-o", "s.tadah",
       "--uniform", "2", "-v", "2"},
      {"tadah", "data", "dedup", "-d", "db.tadah", "-o", "d.tadah",
       "--threshold", "1e-6", "-v", "2"},
      {"tadah", "data", "balance", "-d", "db.tadah", "--eshift", "-1", "-o",
       "b.tadah", "-v", "2"},
      {"tadah", "data", "write", "-d", "db.tadah", "-o", "w.cell", "-f",
       "castep", "-i", "1", "-v", "2"},
      {"tadah", "train", "-c", "train.cfg", "-o", "pot.tadah", "-v", "2"},
      {"tadah", "predict", "-p", "pot.tadah", "-d", "db.tadah", "-v", "2"},
      {"tadah", "analysis", "cutoff", "-o", "c.txt", "-r", "0", "3.14", "10",
       "--type", "Cut_Cos", "-v", "2"}};
  std::string all;
  for (const auto& args : runs) {
    const CliRun r = run_tadah(args);
    INFO(args[1] << " " << args[2] << ": status " << r.status << "\n"
                 << reported(r));
    CHECK(r.status == 0);
    CHECK_FALSE(contains(reported(r), "Engine"));
    all += reported(r);
  }
  CHECK(contains(all, "INFO: tadah data sample: the sampled structures are "
                      "written to s.tadah\n"));
  CHECK(contains(all, "INFO: tadah data dedup: 3 structures in 1 file\n"));
  CHECK(contains(all, "INFO: tadah data balance: shifting the energies, E "
                      "-= sum_Z N_Z eshift[Z]\n"));
  const CliRun refit = run_tadah({"tadah", "refit", "-c", "norefit.cfg",
                                  "-v", "2"});
  INFO("refit: status " << refit.status << "\n" << reported(refit));
  CHECK(refit.status == 1);
  CHECK_FALSE(contains(reported(refit), "Engine"));
  const CliRun merge = run_tadah({"tadah", "data", "merge", "-d",
                                  "nosuch.tadah", "-o", "y.tadah"});
  INFO("merge: status " << merge.status << "\n" << reported(merge));
  CHECK(merge.status == 1);
  CHECK(contains(merge.what, "DBFILE does not exist: nosuch.tadah"));
  CHECK_FALSE(contains(reported(merge), "Engine"));
  const CliRun convert = run_tadah({"tadah", "data", "convert", "--dft-file",
                                    "nosuch.OUTCAR", "-o", "x.tadah"});
  INFO("convert: status " << convert.status << "\n" << reported(convert));
  CHECK(convert.status == 1);
  CHECK(contains(convert.what, "cannot open 'nosuch.OUTCAR'"));
  CHECK_FALSE(contains(reported(convert), "Engine"));
}

/* Pins: tadah data write refuses a format it does not write before reading
 * anything, naming the formats it writes: -f cell (the CASTEP cell file's
 * extension) exits 1 with "tadah data write: --format cell is not a format
 * it writes: castep, lammps or vasp", naming the option (FORMAT is given
 * with -f or --format only), and no file is written; -f castep writes
 * w.cell.
 * Before the fix: StructureWriterSelector::get_writer printed "Unknown
 * type! Returning nullptr." and the run dereferenced the null writer: a
 * segmentation fault (also in Tadah! 1.5.0, whose help gave "-f cell" as
 * the example). Before the fix that names the option: "tadah data write:
 * FORMAT cell is not a format it writes: ...". */
TEST_CASE("Subcommand: data write refuses a format it does not write",
          "[TadahCLI][data][write]") {
  ScratchCwd cwd("write_format");
  std::ofstream("db.tadah") << retrain_dataset();
  const CliRun r = run_tadah({"tadah", "data", "write", "-d", "db.tadah",
                              "-o", "w.cell", "-f", "cell", "-i", "1"});
  INFO("cell: status " << r.status << "\n" << reported(r));
  CHECK(r.status == 1);
  CHECK(r.what == "tadah data write: --format cell is not a format it "
                  "writes: castep, lammps or vasp");
  CHECK_FALSE(std::filesystem::exists("w.cell"));
  const CliRun c = run_tadah({"tadah", "data", "write", "-d", "db.tadah",
                              "-o", "w.cell", "-f", "castep", "-i", "1"});
  INFO("castep: status " << c.status << "\n" << reported(c));
  CHECK(c.status == 0);
  CHECK(std::filesystem::exists("w.cell"));
}
