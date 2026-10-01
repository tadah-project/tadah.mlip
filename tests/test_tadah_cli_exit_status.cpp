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

#include "catch2/catch.hpp"
#include <tadah.mlip/cli/tadah_cli.h>

#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
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
 * fitted: FORCE/STRESS cannot change the weights, only what validation
 * would predict today. `switches` holds the FORCE/STRESS lines, if any.
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

bool contains(const std::string& s, const std::string& what) {
  return s.find(what) != std::string::npos;
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
 * tolerance is rel 1e-6 (today's -F run of this fixture: 1.7e-7). */
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

}  // namespace

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

/* Pins: PC_FRMSE works with FORCE false written in the -c file. Validation
 * predicts forces because PC_FRMSE asks for them, whatever FORCE the
 * potential was fitted with: exit 0, a finite FRMSE equal to `tadah predict
 * -F` on best_pot.tadah. best_pot.tadah keeps the training FORCE false and
 * STRESS false. The journal's POTENTIAL SETUP says "Fit forces (FORCE) : no
 * (configuration file config_ffalse.tadah)", the same for STRESS, and
 * "Validation predicts : energies, forces (PC_FRMSE)".
 * Today: validation predicts with the model's FORCE false, so no forces are
 * predicted and Analytics::calc_f_rmse reads past the empty predicted atoms:
 * SIGSEGV at the first evaluation (the test process dies). */
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
 * Today: the same SIGSEGV as with FORCE false written in the file. */
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
 * Today: the run succeeds and the FRMSE is right, but POTENTIAL SETUP has no
 * "Fit forces (FORCE)" line: the REQUIRE on it fails. */
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
 * Today: the key is silently ignored and the run exits 0. */
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
