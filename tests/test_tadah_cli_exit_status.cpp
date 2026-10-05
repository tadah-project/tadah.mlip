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

#include "tadah_test_skip.hpp"  // before catch2/catch.hpp
#include "catch2/catch.hpp"
#include <tadah.mlip/cli/tadah_cli.h>
#include <tadah/core/periodic_table.h>
#include <tadah/mlip/structure.h>
#include <tadah/mlip/structure_db.h>
#include <tadah/mlip/structure_readers/vasp_poscar_reader.h>

#include <algorithm>
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
 * Each of these cases runs `tadah hpo` in this process and relies on
 * having the process to itself, as CTest gives it: catch_discover_tests
 * registers every TEST_CASE as its own CTest entry, one test_tadah process
 * each. Two `tadah hpo` runs in one process share the per-thread trainer
 * cache (HPO_Host's tls_trainer_cache_): the second run reuses the first
 * run's trainer and rewrites only the searched parameters, so it trains
 * with the first run's FORCE/STRESS. Run together in one process (for
 * example `test_tadah "[validation-metric]"`), a case that follows another
 * `tadah hpo` run with a different training FORCE fails its best_pot.tadah
 * FORCE check. That is a recorded open issue, not a defect of these cases:
 * see RELEASE-1.6.0-FOLLOWUPS.md, "Two HPO runs in one process share the
 * thread-local trainer cache".
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
 * before the start of the selection (undefined behaviour, a crash) and
 * '-i 5-3' silently selected nothing. */
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
