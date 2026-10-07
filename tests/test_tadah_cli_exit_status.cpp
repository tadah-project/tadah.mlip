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
// validation predicts forces because PC_FRMSE asks for them, the journal
// says where FORCE/STRESS came from, and FORCE in the --validation file is
// refused at its line.
//
// The [data] cases run the `tadah data` tools on small structure files
// (RELEASE-1.6.0-FOLLOWUPS.md, "Known-issues session ... decisions", D,
// "Data tools"): `data convert` on a file that is not DFT output, or that
// does not exist, exits non-zero with a message naming the file (and the
// recognised formats) instead of crashing; `data print -s` and `data write
// -s` read a structure file; `data dedup` needs --threshold, keeps the first
// of each set of equal structures, and matches atoms independently of their
// order and periodically. They need no HPO and run in every build.
//
// The [coincident] cases (report_leftovers.md section 3, option A): `tadah
// predict`, `tadah train` and `tadah hpo` refuse a dataset holding a
// structure with two atoms at the same position before they predict, train
// or evaluate, naming the structure and the atoms (NNFinder's refusal). The
// predict and train cases run in every build, the hpo case needs NFP.
//
// The [task-file] case runs `tadah --task` on a task file whose TASK lines
// carry an inline comment; it runs in every build.
//
// The [nonlinear] cases run `tadah train`, `tadah predict` (with and
// without --error) and `tadah hpo` with every non-linear model (M_KRR
// Kern_RBF, Kern_Polynomial, Kern_Quadratic, Kern_LQ, Kern_Sigmoid; M_BLR
// BF_Polynomial2), which the design matrix's column-layout check refused
// (RELEASE-1.6.0-FOLLOWUPS.md, "Training"), and `tadah predict` with a KRR
// potential whose basis does not fit. The hpo case needs NFP.

#include "tadah_test_skip.hpp"  // before catch2/catch.hpp
#include "catch2/catch.hpp"
#include <tadah.mlip/cli/tadah_cli.h>
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
#include <fstream>
#include <iostream>
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

/* The values after `key` on the first non-comment line of a potential
 * file whose first token is `key`; empty when there is none. */
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
   * no DBFILE and no ATOM key -> "the ATOM key is required". */
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
 * evaluation now trains a model of its own configuration ("Task file: a
 * second TASK hpo evaluates its own configuration" pins it).
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
 * unrecognised file, names the recognised formats (the decision: "data
 * convert on an unknown or missing file gives an error naming the
 * recognised formats").
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
 * three options and writes nothing; a task file with two selections
 * (UNIFORM 2 and RANDOM 2) exits 1 naming both.
 * Before the fix: no selection wrote an empty dataset with exit 0; with two,
 * the first (UNIFORM, then RANDOM, then INDEX) ran and the other was ignored
 * silently. */
TEST_CASE("Subcommand: data sample needs exactly one selection",
          "[TadahCLI][exit-status][data][sample]") {
  ScratchCwd cwd("sample_selection");
  write_numbered_dataset("db.tadah", 5);
  {
    const CliRun r = run_tadah({"tadah", "data", "sample", "-d", "db.tadah", "-o", "out.tadah"});
    const std::string rep = reported(r);
    INFO("status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    CHECK(contains(rep, "--uniform"));
    CHECK(contains(rep, "--random"));
    CHECK(contains(rep, "--index"));
    CHECK_FALSE(std::filesystem::exists("out.tadah"));
  }
  {
    std::ofstream("two.task") << "TASK data sample\nDBFILE db.tadah\nOUTFILE out.tadah\n"
                                 "UNIFORM 2\nRANDOM 2\n";
    const CliRun r = run_tadah({"tadah", "data", "sample", "-t", "two.task"});
    const std::string rep = reported(r);
    INFO("status " << r.status << "\n" << rep);
    CHECK(r.status == 1);
    CHECK(contains(rep, "UNIFORM"));
    CHECK(contains(rep, "RANDOM"));
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

/* Pins: the index list of `tadah data sample -i` (and INDEX in a task
 * file) is one list however it is spaced, as the command-line schema
 * accepts it: on S1 ... S5, '-i "1, 3"' and '-i 1, 3' give S1, S3,
 * '-i "1 - 3"' S1, S2, S3, '-i "1-5 : 2"' S1, S3, S5, and a task file
 * 'INDEX 1, 3, 5' S1, S3, S5.
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
      {{"1-5 : 2"}, numbered_labels({1, 3, 5})}};
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
  std::filesystem::remove("out.tadah");
  std::ofstream("idx.task") << "TASK data sample\nDBFILE db.tadah\nOUTFILE out.tadah\nINDEX 1, 3, 5\n";
  const CliRun r = run_tadah({"tadah", "data", "sample", "-t", "idx.task"});
  INFO("status " << r.status << "\n" << reported(r));
  REQUIRE(r.status == 0);
  CHECK(dataset_labels("out.tadah") == numbered_labels({1, 3, 5}));
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

/* Pins: `tadah predict -F -S -A` leaves a structure stored with force and
 * stress weight 0 (weights line `1 0 0`) out of the force and stress
 * metrics: on S1, W, S3, where W's reference forces (50 eV/A) and stress
 * (0.5 eV/A^3) are far from the prediction, every Force and Stress MAE,
 * RMSE and R^2 line equals the one for S1, S3 alone.
 * Before the fix: W was included, so the force metrics were off by O(10)
 * eV/A and the stress metrics by O(0.1) eV/A^3. */
TEST_CASE("Subcommand: predict -A leaves structures with weight 0 out of the "
          "force and stress metrics",
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

/* The whitespace-separated fields of `line`. */
std::vector<std::string> fields_of(const std::string& line) {
  std::istringstream is(line);
  std::vector<std::string> out;
  for (std::string f; is >> f;) out.push_back(f);
  return out;
}

/* Pins (MK, 2026-10-06): `tadah predict -A` measures a dataset whose
 * structures all have force weight 0 (stress weight 0) in no force (stress)
 * metric: with -F -A on S.tadah (weights 1) and nof.tadah (force weights 0)
 * it exits 0, prints nan for nof.tadah in each Force line, a mean equal to
 * S.tadah's value, and a line naming nof.tadah as not measured; the same
 * with -S for nos.tadah (stress weights 0). With only the file without the
 * data it exits 1 before predicting, naming it and the weight. The other
 * weight refuses nothing (nof.tadah -S -A and nos.tadah -F -A, and -A
 * alone, exit 0).
 * Before the fix: the run with both files was refused. */
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
  struct Case { std::string file, flag, metric, weight; };
  for (const Case& c : {Case{"nof.tadah", "-F", "Force", "force weight 0"},
                        Case{"nos.tadah", "-S", "Stress", "stress weight 0"}}) {
    INFO(c.file);
    {
      const CliRun r = run_tadah({"tadah", "predict", "-p", "pot.tadah", "-d", "S.tadah",
                                  c.file, c.flag, "-A"});
      const std::string rep = reported(r);
      INFO("both files: status " << r.status << "\n" << rep);
      CHECK(r.status == 0);
      for (const std::string kind : {" MAE", " RMSE", " R^2"}) {
        const std::vector<std::string> f = fields_of(metric_line(r.out, c.metric + kind));
        INFO(c.metric + kind << ": " << metric_line(r.out, c.metric + kind));
        REQUIRE(f.size() == 5u);  // name (2 words), mean, Val1, Val2
        CHECK(f[2] == f[3]);
        CHECK((f[4] == "nan" || f[4] == "-nan"));  // as printed (no isnan: fast-math)
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
  // The other weight refuses nothing: nof.tadah has stress weights 1 and
  // nos.tadah force weights 1.
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

/* Pins: `tadah predict --error` predicts the same energies, forces and
 * stresses as `tadah predict`, whatever the weights: on two structures with
 * weights `0.25 0.36 0.49` and a potential with EWEIGHT 4, FWEIGHT 9 and
 * SWEIGHT 16 (and the error matrix SIGMA = I), energy.pred, forces.pred and
 * stress.pred agree to 1e-12, and so do the -A metric lines.
 * Before the fix: the --error path predicted from the weighted design
 * matrix, so each value was multiplied by the square root of its weight
 * times the global weight (energies by 1, forces by 1.8, stresses by 2.8). */
TEST_CASE("Subcommand: predict --error predicts what predict predicts",
          "[TadahCLI][predict][weight0]") {
  ScratchCwd cwd("predict_error_weights");
  std::ofstream("pot.tadah") << kPairPotential
                             << "EWEIGHT     4\nFWEIGHT     9\nSWEIGHT     16\n"
                                "SIGMA     4     1 0 0 0     0 1 0 0     0 0 1 0     0 0 0 1\n";
  std::ofstream("d.tadah")
      << weighted_entry("S1", "0.25 0.36 0.49", "1.40 1.30 1.20", "0.1", "0.001")
      << weighted_entry("S3", "0.25 0.36 0.49", "1.60 1.25 1.30", "-0.1", "0.002");
  std::vector<std::vector<double>> values[2];
  std::string out[2];
  for (int with_error = 0; with_error < 2; ++with_error) {
    std::vector<std::string> args = {"tadah", "predict", "-p", "pot.tadah", "-d", "d.tadah",
                                     "-F", "-S", "-A"};
    if (with_error) args.push_back("--error");
    const CliRun r = run_tadah(args);
    INFO("--error " << with_error << ": status " << r.status << "\n" << reported(r));
    REQUIRE(r.status == 0);
    for (const char* f : {"energy.pred", "forces.pred", "stress.pred"})
      values[with_error].push_back(predicted_values(f));
    out[with_error] = r.out;
  }
  const char* files[3] = {"energy.pred", "forces.pred", "stress.pred"};
  for (int k = 0; k < 3; ++k) {
    INFO(files[k]);
    REQUIRE(values[1][k].size() == values[0][k].size());
    REQUIRE_FALSE(values[0][k].empty());
    for (size_t i = 0; i < values[0][k].size(); ++i) {
      INFO("row " << i);
      CHECK(values[1][k][i] == Approx(values[0][k][i]).epsilon(1e-12).margin(1e-14));
    }
  }
  for (const char* metric : {"Force RMSE", "Stress RMSE"})
    CHECK(metric_line(out[1], metric) == metric_line(out[0], metric));
}

/* Pins: `tadah predict -A` refuses, before predicting, a dataset that
 * loading leaves without structures (here every structure is dropped by
 * --ffilter 0.01, below its reference forces of 0.1 eV/A), naming the file
 * and saying so, with or without -F.
 * Before the fix: with -F it said every structure had force weight 0 (their
 * force weights are 1); without -F it printed energy metrics of no
 * structure. */
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
}

/* Pins: `tadah data dedup --threshold 1e-6` on T1 = T2 = T3 and a different
 * T4 (Cl at x = 2.0 instead of 2.5; report_leftovers.md section 2) keeps the
 * first of the three equal structures and T4: exactly T1, T4, in that order.
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

/* Pins: `tadah data balance --rescale -F -S` keeps a weight of 0: it marks
 * a quantity the structure has no data for (the DFT readers write sweight 0
 * for a structure without a stress tensor) or that is excluded, so
 * rescaling leaves it 0. Of B1 (weights 1 1 1) and B2 (weights 1 0 0, no
 * forces or stress data), B2 keeps fweight 0 and sweight 0 while B1's are
 * rescaled.
 * Before the fix: every weight was rescaled, so B2's zero stress and forces
 * were fitted again (fweight and sweight 1 below the thresholds). */
TEST_CASE("Subcommand: data balance --rescale keeps a weight of 0",
          "[TadahCLI][exit-status][data]") {
  ScratchCwd cwd("balance_zero_weight");
  {
    std::ofstream os("db.tadah");
    os << "B1\n1 1 1\n-11.5\n3.3 0 0\n0 3.3 0\n0 0 3.3\n0.01 0 0\n0 0.01 0\n0 0 0.01\n"
          "Ta 0 0 0 0.1 0 0\nTa 1.65 1.65 1.65 -0.1 0 0\n\n"
       << "B2\n1 0 0\n-11.4\n3.31 0 0\n0 3.31 0\n0 0 3.31\n0 0 0\n0 0 0\n0 0 0\n"
          "Ta 0 0 0 0 0 0\nTa 1.655 1.655 1.655 0 0 0\n\n";
  }
  const CliRun r = run_tadah({"tadah", "data", "balance", "-d", "db.tadah",
                              "-o", "out.tadah", "--rescale", "-F", "-S"});
  INFO("status " << r.status << "\n" << reported(r));
  REQUIRE(r.status == 0);
  tadah::core::PeriodicTable::initialize();
  tadah::mlip::StructureDB db;
  db.add("out.tadah");
  REQUIRE(db.size() == 2);
  CHECK(db(0).fweight == Approx(1.0 / 0.2));
  CHECK(db(0).sweight == Approx(1.0 / std::sqrt(3 * 0.01 * 0.01)));
  CHECK(db(1).fweight == 0.0);
  CHECK(db(1).sweight == 0.0);
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
 * differ (report_leftovers.md section 2).
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
 * structure and the atoms (report_leftovers.md section 3, option A). `tadah
 * predict`, `tadah train` and `tadah hpo` build their neighbour lists before
 * they predict, train or evaluate, so each stops there with that message. */

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
 * it lists exactly the pairs of structures 2 and 3 (report_leftovers.md
 * sections 3.6 E and 9 item 5).
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

/* ---- [task-file]: `tadah --task` ------------------------------------- */

/* Pins: `tadah --task` runs a task file whose TASK lines carry an inline
 * comment, as the Quick Start's task file may ("Comments start with # and
 * run to the end of the line"): "TASK       train          # First task:
 * training" (after blanks) and "TASK       predict\t# Second task:
 * prediction" (after a tab) train a D2_BP pair potential on three Ta dimers
 * and predict with it: exit 0, pot.tadah written with its WEIGHTS, and
 * energy.pred with one prediction per structure.
 * Before the fix: exit 1 with 'Unknown task section
 * "TRAIN.#.first.task:.training"' (the words of the comment joined to the
 * task name) before any task ran, so the status and message CHECKs failed
 * and the REQUIRE on pot.tadah stopped the case. */
TEST_CASE("Task file: tadah --task runs TASK lines that carry an inline comment",
          "[TadahCLI][exit-status][task-file]") {
  ScratchCwd cwd("task_comment");
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
  std::ofstream("run.task")
      << "VERBOSE    0                  # global default\n"
         "TASK       train          # First task: training\n"
         "  CONFIG     config.train       # the training setup\n"
         "TASK       predict\t# Second task: prediction\n"
         "  DBFILE     db.tadah           # prediction dataset\n"
         "  POTENTIAL  pot.tadah          # same as CLI -p/--potential\n";
  const CliRun r = run_tadah({"tadah", "--task", "run.task"});
  const std::string rep = reported(r);
  INFO("status " << r.status << "\n" << rep);
  CHECK(r.status == 0);
  CHECK_FALSE(contains(rep, "Unknown task section"));
  REQUIRE(std::filesystem::exists("pot.tadah"));
  CHECK(line_tokens(slurp("pot.tadah"), "WEIGHTS").size() == 5u);  // key + 4
  REQUIRE(std::filesystem::exists("energy.pred"));
  CHECK(predicted_values("energy.pred").size() == 3u);
}

/* ---- [eam]: D2_EAM and DM_EAM read the setfl file named by SETFL ------ */

namespace {

/* A single-element (Ta) setfl file with analytic tables, cutoff 4.5 A:
 * F(rho) = -sqrt(rho) on rho in [0, 50], rho(r) = (4.5 - r)^2 and
 * phi(r) = 0.5 (4.5 - r)^3 (z2r = r phi) below the cutoff, 0 beyond. */
std::string small_ta_setfl() {
  const int nrho = 501, nr = 501;
  const double drho = 0.1, dr = 0.01, rc = 4.5;
  std::ostringstream os;
  os.precision(17);
  os << "Small Ta setfl of the DM_EAM CLI test\nanalytic tables\n\n"
     << "1 Ta\n"
     << nrho << " " << drho << " " << nr << " " << dr << " " << rc << "\n"
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
 *   'eam_small.alloy' (SETFL); set RCUTMB to the file's cutoff, 4.5.
 * and write no potential.
 * Before the fix: both exited 0 and wrote a potential that records the
 * file's cutoff (RCUTMB 4.5, RCUT2B 4.5), while the weights had been fitted
 * with the descriptor evaluated at 3.5, so `tadah predict` on that file
 * evaluated a model that was never fitted. */
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
                            "file 'eam_small.alloy' (SETFL); set " + key +
                            " to the file's cutoff, 4.5."));
    CHECK_FALSE(std::filesystem::exists("pot.tadah"));
  }
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
 * 5 x 3 (5 x 4 with BIAS) BASIS values; a second training writes the same
 * file byte for byte (the basis draw is fixed); `tadah predict -p
 * pot.tadah -d db.tadah -F -S` exits 0 with 6 energies and 63 force
 * components, each a number (|value| < 1e3).
 * Before the fix: the first training (M_KRR Kern_RBF, BIAS false) exited
 * 1 with "ColumnLayout: total_cols=3 disagrees with design-matrix width=5"
 * and wrote no potential (the REQUIRE stops there; the check refused every
 * non-linear model: width 6 for BF_Polynomial2, and with BIAS true
 * total_cols=4 against 5 or 10). */
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
      for (const char* f : {"pot.tadah", "pot2.tadah", "energy.pred", "forces.pred"})
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
      const CliRun t2 = run_tadah({"tadah", "train", "-c", "config", "-o",
                                   "pot2.tadah", "-F", "-S"});
      REQUIRE(t2.status == 0);
      CHECK(slurp("pot2.tadah") == pot);

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

/* Pins: for each non-linear model trained with OALGO 3 (which stores the
 * covariance SIGMA), with NORM false and NORM true, `tadah predict
 * --error` exits 0 and predicts the energies, forces and stresses `tadah
 * predict` predicts (1e-10 relative, 1e-12 absolute; both from the same
 * weights, through the design-matrix rows and through the model).
 * Before the fix: the trainings exited 1 (the ColumnLayout check); with a
 * potential at hand, `predict --error` built the same design matrix and
 * exited 1 with that message too. */
TEST_CASE("Subcommand: predict --error predicts what predict predicts for "
          "every non-linear model",
          "[TadahCLI][predict][nonlinear]") {
  ScratchCwd cwd("nonlinear_error");
  std::ofstream("db.tadah") << nonlinear_dataset();
  for (const NonlinearModel& m : kNonlinearModels) {
    for (const char* norm : {"false", "true"}) {
      INFO(m.model << ", NORM " << norm);
      std::ofstream("config") << nonlinear_config(
          m, std::string("BIAS false\nNORM ") + norm + "\nLAMBDA 0\nOALGO 3\n");
      const CliRun t = run_tadah({"tadah", "train", "-c", "config", "-o",
                                  "pot.tadah", "-F", "-S"});
      INFO("train: status " << t.status << "\n" << reported(t));
      REQUIRE(t.status == 0);
      REQUIRE_FALSE(line_tokens(slurp("pot.tadah"), "SIGMA").empty());
      std::vector<std::vector<double>> values[2];
      for (int with_error = 0; with_error < 2; ++with_error) {
        std::vector<std::string> args = {"tadah", "predict", "-p", "pot.tadah",
                                         "-d", "db.tadah", "-F", "-S"};
        if (with_error) args.push_back("--error");
        const CliRun r = run_tadah(args);
        INFO("--error " << with_error << ": status " << r.status << "\n"
                        << reported(r));
        REQUIRE(r.status == 0);
        for (const char* f : {"energy.pred", "forces.pred", "stress.pred"})
          values[with_error].push_back(predicted_values(f));
      }
      const char* files[3] = {"energy.pred", "forces.pred", "stress.pred"};
      for (int k = 0; k < 3; ++k) {
        INFO(files[k]);
        REQUIRE(values[1][k].size() == values[0][k].size());
        REQUIRE_FALSE(values[0][k].empty());
        for (std::size_t i = 0; i < values[0][k].size(); ++i) {
          INFO("row " << i);
          CHECK(values[1][k][i] ==
                Approx(values[0][k][i]).epsilon(1e-10).margin(1e-12));
        }
      }
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
    std::istringstream ml(metric_line(p.out, "Energy RMSE"));
    std::string w1, w2;
    double mean = 0;  // meV/atom, as stats.tadah
    REQUIRE(ml >> w1 >> w2 >> mean);
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

/* ---- [retrain]: a potential used as the training configuration -------- */

namespace {

/* Three Ta dimers with reference forces (FORCE true: 21 rows for the 4
 * weights of D2_BP 4). */
std::string retrain_dataset() {
  return weighted_entry("R1", "1 1 1", "2.60 0.40 0.30", "0.12", "0") +
         weighted_entry("R2", "1 1 1", "3.10 0.20 0.50", "-0.07", "0") +
         weighted_entry("R3", "1 1 1", "3.70 0.10 0.20", "0.03", "0");
}

/* The D2_BP pair configuration of the [task-file] case with FORCE true,
 * the model `model` and the fit lines `fit`, training on db.tadah. */
std::string retrain_config(const std::string& model, const std::string& fit) {
  return "DBFILE db.tadah\nATOM Ta\nWATOM 73\nINIT2B true\nINITMB false\n"
         "TYPE2B D2_BP 4 4 Ta Ta\nCGRID2B 2.5 3.0 3.5 4.0\n"
         "SGRID2B 0.5 0.5 0.5 0.5\nRCTYPE2B Cut_Cos\nRCUT2B 4.5\n"
         "FORCE true\nSTRESS false\nBIAS false\nNORM false\nVERBOSE 0\n"
         "MODEL " + model + "\n" + fit;
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

/* Pins: `tadah train -c pot1.tadah`, pot1.tadah the potential of an OALGO 3
 * fit (LAMBDA 1e-3), writes a potential with one SIGMA of N*N+1 = 17
 * values for its 4 weights, the same tokens as pot1.tadah's (the same data
 * and fit), and so does a third train from that potential; `tadah predict
 * --error` with it exits 0 and writes energy.pred.
 * Before the fix: the retrain wrote 34 SIGMA values (each fit appended its
 * SIGMA to the one of the configuration), the REQUIRE on the count stopped
 * the case; `predict --error` on that potential read 34 values into a
 * 17-value buffer (heap corruption: SIGABRT, or SIGSEGV). */
TEST_CASE("Subcommand: train -c with an OALGO 3 potential writes the SIGMA of "
          "its own fit",
          "[TadahCLI][train][retrain][sigma]") {
  ScratchCwd cwd("retrain_sigma");
  std::ofstream("db.tadah") << retrain_dataset();
  std::ofstream("config.train")
      << retrain_config("M_BLR BF_Linear", "LAMBDA 1e-3\nOALGO 3\n");
  const CliRun t1 = run_tadah({"tadah", "train", "-c", "config.train", "-o", "pot1.tadah"});
  INFO("train: status " << t1.status << "\n" << reported(t1));
  REQUIRE(t1.status == 0);
  const auto sigma1 = line_tokens(slurp("pot1.tadah"), "SIGMA");
  REQUIRE(sigma1.size() == 1 + 17);
  std::string from = "pot1.tadah";
  for (const char* to : {"pot2.tadah", "pot3.tadah"}) {
    INFO("train -c " << from << " -o " << to);
    const CliRun t = run_tadah({"tadah", "train", "-c", from, "-o", to});
    INFO("status " << t.status << "\n" << reported(t));
    REQUIRE(t.status == 0);
    const auto sigma = line_tokens(slurp(to), "SIGMA");
    REQUIRE(sigma.size() == 1 + 17);
    CHECK(sigma == sigma1);
    from = to;
  }
  const CliRun p = run_tadah({"tadah", "predict", "-p", "pot3.tadah", "-d", "db.tadah",
                              "--error"});
  INFO("predict --error: status " << p.status << "\n" << reported(p));
  CHECK(p.status == 0);
  CHECK(predicted_values("energy.pred").size() == 3u);
}

/* Pins: `tadah train -c` with an OALGO 3 potential edited to OALGO 1 (a fit
 * that computes no covariance) writes a potential without SIGMA, and
 * `tadah predict --error` with it exits 1 saying the covariance was not
 * computed ("Sigma matrix is not computed").
 * Before the fix: the potential kept the 17 SIGMA values of the OALGO 3
 * fit beside the OALGO 1 weights, and predict --error exited 0 with them. */
TEST_CASE("Subcommand: train -c with an OALGO 3 potential and OALGO 1 writes "
          "no SIGMA",
          "[TadahCLI][train][retrain][sigma]") {
  ScratchCwd cwd("retrain_nosigma");
  std::ofstream("db.tadah") << retrain_dataset();
  std::ofstream("config.train")
      << retrain_config("M_BLR BF_Linear", "LAMBDA 1e-3\nOALGO 3\n");
  const CliRun t1 = run_tadah({"tadah", "train", "-c", "config.train", "-o", "pot1.tadah"});
  INFO("train: status " << t1.status << "\n" << reported(t1));
  REQUIRE(t1.status == 0);
  REQUIRE(line_tokens(slurp("pot1.tadah"), "SIGMA").size() == 1 + 17);
  std::ofstream("pot1_oalgo1.tadah") << replace_key_line(slurp("pot1.tadah"), "OALGO", "OALGO 1");
  const CliRun t2 = run_tadah({"tadah", "train", "-c", "pot1_oalgo1.tadah", "-o",
                               "pot2.tadah"});
  INFO("retrain: status " << t2.status << "\n" << reported(t2));
  REQUIRE(t2.status == 0);
  CHECK(line_tokens(slurp("pot2.tadah"), "SIGMA").empty());
  CHECK(line_tokens(slurp("pot2.tadah"), "WEIGHTS").size() == 1 + 4);
  const CliRun p = run_tadah({"tadah", "predict", "-p", "pot2.tadah", "-d", "db.tadah",
                              "--error"});
  const std::string rep = reported(p);
  INFO("predict --error: status " << p.status << "\n" << rep);
  CHECK(p.status == 1);
  CHECK(contains(rep, "Sigma matrix is not computed"));
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

#ifdef TADAH_ENABLE_NFP
/* Pins: a task file with two `TASK hpo` blocks (one evaluation each, at
 * the CONFIG seed of OPTIM CGRID2B (1); PC_ERMSE and PC_FRMSE), A with
 * LAMBDA 1e-4 and FWEIGHT 1, then B with LAMBDA 0.1 and FWEIGHT 100: exit
 * 0, and the best_pot.tadah the second run writes is B's: LAMBDA 0.1,
 * FWEIGHT 100, and the weights of `tadah train -c` with B's configuration
 * (1e-10 relative: the same fit at the seed).
 * Before the fix: the second run evaluated the first run's model (the
 * thread's trainer of A, with only CGRID2B rewritten): best_pot.tadah had
 * LAMBDA 1e-4, FWEIGHT 1 and A's weights. */
TEST_CASE("Task file: a second TASK hpo evaluates its own configuration",
          "[TadahCLI][hpo][task-file][retrain]") {
  ScratchCwd cwd("two_hpo_tasks");
  std::ofstream("db.tadah") << retrain_dataset();
  { std::ofstream os("config.val"); os << "DBFILE db.tadah\n"; }
  std::ofstream("configA.tadah")
      << retrain_config("M_BLR BF_Linear", "LAMBDA 1e-4\nOALGO 4\nFWEIGHT 1\n");
  std::ofstream("configB.tadah")
      << retrain_config("M_BLR BF_Linear", "LAMBDA 0.1\nOALGO 4\nFWEIGHT 100\n");
  {
    std::ofstream os("config.hpo");
    os << "OPTIMIZER\nLIB NLOPT\nALGO LN_SBPLX\nMAXEVAL 1\nENDOPTIMIZER\n"
          "LOSS L2\nPC_ERMSE 0 1\nPC_FRMSE 0 1\n"
          "OPTIM CGRID2B (1) 2.3 2.7\n";
  }
  std::ofstream("run.task")
      << "TASK hpo\n  CONFIG configA.tadah\n  HPOTARGET config.hpo\n"
         "  VALIDATION config.val\n"
         "TASK hpo\n  CONFIG configB.tadah\n  HPOTARGET config.hpo\n"
         "  VALIDATION config.val\n";
  const CliRun r = run_tadah({"tadah", "--task", "run.task"});
  INFO("status " << r.status << "\n" << reported(r));
  REQUIRE(r.status == 0);
  REQUIRE(std::filesystem::exists("best_pot.tadah"));
  const std::string best = slurp("best_pot.tadah");
  INFO("best_pot.tadah:\n" << best);
  const auto lambda = line_tokens(best, "LAMBDA");
  const auto fweight = line_tokens(best, "FWEIGHT");
  REQUIRE(lambda.size() == 2);
  REQUIRE(fweight.size() == 2);
  CHECK(std::strtod(lambda[1].c_str(), nullptr) == 0.1);
  CHECK(std::strtod(fweight[1].c_str(), nullptr) == 100.0);

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
