// test_kernel_settings_once.cpp
//
// The line a kernel prints when it is built at VERBOSE > 0 (its label and
// settings, "Kern_RBF | sigma: 1 | gamma: 1"). Every kernel built prints it
// (tadah train, tadah predict and pair_style tadah print the line of each
// model they build), except while a
// KernelSettingsLineScope is open: tadah hpo holds one for each run, which
// builds the model of every evaluation from one configuration, so the run
// prints the line of its first build of each kernel only, and its
// PC_ISOLATION PROCESS workers hold one in which no kernel prints. Each
// scope that opens while none is open starts afresh, so a second run in
// the process prints its own line.
//
// No case depends on what an earlier case of the process printed: a scope
// opened by a case starts its own record.
#include "catch2/catch.hpp"
#include <tadah/core/context.h>
#include <tadah/core/logger.h>
#include <tadah/models/functions/kernels/kern_base.h>
#include <tadah/models/functions/kernels/kern_lq.h>
#include <tadah/models/functions/kernels/kern_polynomial.h>
#include <tadah/models/functions/kernels/kern_quadratic.h>
#include <tadah/models/functions/kernels/kern_rbf.h>
#include <tadah/models/functions/kernels/kern_sigmoid.h>

#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using tadah::core::Context;
using tadah::models::KernelSettingsLineScope;

namespace {

/* Captures std::cout while it lives, and restores the Logger level that a
 * Context's VERBOSE sets (Context::add). */
class CoutCapture {
public:
  CoutCapture()
      : old_(std::cout.rdbuf(out_.rdbuf())),
        level_(tadah::core::Logger::instance().getLogLevel()) {}
  ~CoutCapture() {
    tadah::core::Logger::instance().setLogLevel(level_);
    std::cout.rdbuf(old_);
  }
  CoutCapture(const CoutCapture &) = delete;
  CoutCapture &operator=(const CoutCapture &) = delete;
  std::string str() const { return out_.str(); }

private:
  std::ostringstream out_;
  std::streambuf *old_;
  tadah::core::LogLevel level_;
};

std::size_t occurrences(const std::string &text, const std::string &needle) {
  std::size_t n = 0;
  for (std::size_t p = text.find(needle); p != std::string::npos;
       p = text.find(needle, p + needle.size()))
    ++n;
  return n;
}

/* A kernel context: VERBOSE @p verbose and, when @p mparams is not empty,
 * MPARAMS. */
Context kernel_ctx(int verbose, const std::string &mparams) {
  Context c;
  c.add("VERBOSE", verbose);
  if (!mparams.empty()) c.add("MPARAMS", mparams);
  return c;
}

/* What std::cout receives while @p build runs. */
template <class F>
std::string printed_by(F build) {
  CoutCapture out;
  build();
  return out.str();
}

/* Outside every KernelSettingsLineScope, builds the kernel K once at
 * VERBOSE 0, three times at VERBOSE 1 from @p mparams and, when
 * @p other_mparams is not empty, once at VERBOSE 1 from those, and checks
 * what std::cout received: nothing from the VERBOSE 0 build; @p line (the
 * settings line of @p mparams) three times, the label nowhere else but in
 * the lines of the builds; and @p other_line once. */
template <class K>
void check_every_build_prints(const std::string &label,
                              const std::string &mparams,
                              const std::string &line,
                              const std::string &other_mparams = "",
                              const std::string &other_line = "") {
  const std::string quiet =
      printed_by([&] { K k(kernel_ctx(0, mparams)); });
  const std::string verbose = printed_by([&] {
    for (int n = 0; n < 3; ++n) K k(kernel_ctx(1, mparams));
  });
  INFO("VERBOSE 0 output: '" << quiet << "'");
  INFO("VERBOSE 1 output: '" << verbose << "'");
  CHECK(quiet.empty());
  CHECK(occurrences(verbose, line) == 3u);
  CHECK(occurrences(verbose, label) == 3u);
  if (!other_mparams.empty()) {
    const std::string other =
        printed_by([&] { K k(kernel_ctx(1, other_mparams)); });
    INFO("VERBOSE 1 output of MPARAMS " << other_mparams << ": '" << other
                                        << "'");
    CHECK(occurrences(other, other_line) == 1u);
    CHECK(occurrences(other, label) == 1u);
  }
}

const std::string kRbfLine1 = "\nKern_RBF | sigma: 1 | gamma: 1\n";
const std::string kRbfLine2 = "\nKern_RBF | sigma: 2 | gamma: 0.25\n";
const std::string kPolyLine =
    "\nKern_Polynomial | degree: 2 | gamma: 0.5 | c: 1.5\n";

}  // namespace

/* Pins: outside every KernelSettingsLineScope, a Kern_RBF built at
 * VERBOSE 0 prints nothing, three built at VERBOSE 1 from MPARAMS 1.0 print
 * "\nKern_RBF | sigma: 1 | gamma: 1\n" three times, and one built from
 * MPARAMS 2.0 then prints "\nKern_RBF | sigma: 2 | gamma: 0.25\n"
 * (check_every_build_prints): each model built prints the settings of its
 * kernel, as the released code did.
 * Before the fix: once per process for each kernel (print_once keyed on
 * the label): one line among the three (none when the process had printed
 * it before), and none for MPARAMS 2.0, so a second training in the
 * process printed no line; KernelSettingsLineScope does not exist (the
 * file does not compile). */
TEST_CASE("Kern_RBF prints its settings line at every build at VERBOSE > 0",
          "[kernel][settings_line]") {
  check_every_build_prints<tadah::models::Kern_RBF>(
      "Kern_RBF", "1.0", kRbfLine1, "2.0", kRbfLine2);
}

/* Pins: as for Kern_RBF, Kern_Polynomial (MPARAMS 2 0.5 1.5): nothing at
 * VERBOSE 0, kPolyLine at each of three VERBOSE 1 builds, and
 * "\nKern_Polynomial | degree: 3 | gamma: 1 | c: 0\n" for MPARAMS 3 1 0.
 * Before the fix: one line among the three (or none), none for MPARAMS
 * 3 1 0. */
TEST_CASE("Kern_Polynomial prints its settings line at every build at "
          "VERBOSE > 0",
          "[kernel][settings_line]") {
  check_every_build_prints<tadah::models::Kern_Polynomial>(
      "Kern_Polynomial", "2 0.5 1.5", kPolyLine, "3 1 0",
      "\nKern_Polynomial | degree: 3 | gamma: 1 | c: 0\n");
}

/* Pins: as for Kern_RBF, Kern_Sigmoid (MPARAMS 0.25 -1): nothing at
 * VERBOSE 0, "\nKern_Sigmoid | gamma: 0.25 | c: -1\n" at each of three
 * VERBOSE 1 builds, and "\nKern_Sigmoid | gamma: 2 | c: 0.5\n" for MPARAMS
 * 2 0.5.
 * Before the fix: one line among the three (or none), none for MPARAMS
 * 2 0.5. */
TEST_CASE("Kern_Sigmoid prints its settings line at every build at "
          "VERBOSE > 0",
          "[kernel][settings_line]") {
  check_every_build_prints<tadah::models::Kern_Sigmoid>(
      "Kern_Sigmoid", "0.25 -1", "\nKern_Sigmoid | gamma: 0.25 | c: -1\n",
      "2 0.5", "\nKern_Sigmoid | gamma: 2 | c: 0.5\n");
}

/* Pins: as for Kern_RBF, Kern_Quadratic: nothing at VERBOSE 0, and
 * "\nKern_Quadratic\n" at each of three VERBOSE 1 builds.
 * Before the fix: one line among the three (or none). */
TEST_CASE("Kern_Quadratic prints its label at every build at VERBOSE > 0",
          "[kernel][settings_line]") {
  check_every_build_prints<tadah::models::Kern_Quadratic>(
      "Kern_Quadratic", "", "\nKern_Quadratic\n");
}

/* Pins: as for Kern_RBF, Kern_LQ: nothing at VERBOSE 0, and "\nKern_LQ\n"
 * at each of three VERBOSE 1 builds.
 * Before the fix: one line among the three (or none). */
TEST_CASE("Kern_LQ prints its label at every build at VERBOSE > 0",
          "[kernel][settings_line]") {
  check_every_build_prints<tadah::models::Kern_LQ>("Kern_LQ", "",
                                                   "\nKern_LQ\n");
}

/* Pins: while a KernelSettingsLineScope is open, each kernel prints its
 * settings line at its first VERBOSE 1 build only, with that build's
 * settings, and a scope opened after it closed starts afresh:
 *   - in a scope: a Kern_RBF built at VERBOSE 0 prints nothing and does not
 *     use up the line; then three Kern_RBF from MPARAMS 1.0, one from 2.0
 *     and a Kern_Polynomial (2 0.5 1.5) print kRbfLine1 once, kRbfLine2
 *     never, "Kern_RBF" once in all, and kPolyLine once;
 *   - in a second scope, opened after the first closed: a Kern_RBF from
 *     MPARAMS 2.0 prints kRbfLine2 once (a second hpo run in the process
 *     prints its own line), and a second one nothing;
 *   - after it closed: two Kern_RBF from MPARAMS 2.0 print kRbfLine2 twice.
 * Before the fix: KernelSettingsLineScope does not exist (the file does
 * not compile); the line was printed by the first kernel of each kind in
 * the process and by no later one, so a second run printed none. */
TEST_CASE("In a KernelSettingsLineScope a kernel prints its settings line "
          "at its first build only",
          "[kernel][settings_line]") {
  using tadah::models::Kern_Polynomial;
  using tadah::models::Kern_RBF;
  std::string first;
  {
    KernelSettingsLineScope scope;
    first = printed_by([] {
      Kern_RBF quiet(kernel_ctx(0, "1.0"));
      for (int n = 0; n < 3; ++n) Kern_RBF k(kernel_ctx(1, "1.0"));
      Kern_RBF other(kernel_ctx(1, "2.0"));
      Kern_Polynomial p(kernel_ctx(1, "2 0.5 1.5"));
    });
  }
  INFO("first scope: '" << first << "'");
  CHECK(occurrences(first, kRbfLine1) == 1u);
  CHECK(occurrences(first, kRbfLine2) == 0u);
  CHECK(occurrences(first, "Kern_RBF") == 1u);
  CHECK(occurrences(first, kPolyLine) == 1u);
  std::string second;
  {
    KernelSettingsLineScope scope;
    second = printed_by([] {
      for (int n = 0; n < 2; ++n) Kern_RBF k(kernel_ctx(1, "2.0"));
    });
  }
  INFO("second scope: '" << second << "'");
  CHECK(occurrences(second, kRbfLine2) == 1u);
  CHECK(occurrences(second, "Kern_RBF") == 1u);
  const std::string after = printed_by([] {
    for (int n = 0; n < 2; ++n) Kern_RBF k(kernel_ctx(1, "2.0"));
  });
  INFO("after the scopes: '" << after << "'");
  CHECK(occurrences(after, kRbfLine2) == 2u);
}

/* Pins: while a KernelSettingsLineScope of Lines::None is open, no kernel
 * prints its settings line, inside another scope too, and its builds do
 * not use up the line of that scope:
 *   - a None scope alone: two Kern_RBF and a Kern_LQ at VERBOSE 1 print
 *     nothing;
 *   - a None scope opened inside a scope: a Kern_RBF built while both are
 *     open prints nothing, and once the None scope has closed, the first
 *     Kern_RBF built prints kRbfLine1 once and the next one nothing.
 * Before the fix: KernelSettingsLineScope does not exist (the file does
 * not compile); a PC_ISOLATION PROCESS worker printed the line once more. */
TEST_CASE("In a KernelSettingsLineScope of Lines::None no kernel prints its "
          "settings line",
          "[kernel][settings_line]") {
  using tadah::models::Kern_LQ;
  using tadah::models::Kern_RBF;
  const std::string alone = printed_by([] {
    KernelSettingsLineScope none(KernelSettingsLineScope::Lines::None);
    for (int n = 0; n < 2; ++n) Kern_RBF k(kernel_ctx(1, "1.0"));
    Kern_LQ lq(kernel_ctx(1, ""));
  });
  INFO("None scope alone: '" << alone << "'");
  CHECK(alone.empty());
  std::string inside, after;
  {
    KernelSettingsLineScope scope;
    inside = printed_by([] {
      KernelSettingsLineScope none(KernelSettingsLineScope::Lines::None);
      Kern_RBF k(kernel_ctx(1, "1.0"));
    });
    after = printed_by([] {
      for (int n = 0; n < 2; ++n) Kern_RBF k(kernel_ctx(1, "1.0"));
    });
  }
  INFO("None scope inside a scope: '" << inside << "'");
  INFO("the outer scope after it: '" << after << "'");
  CHECK(inside.empty());
  CHECK(occurrences(after, kRbfLine1) == 1u);
  CHECK(occurrences(after, "Kern_RBF") == 1u);
}

/* Pins: a KernelSettingsLineScope covers the kernels built by every thread
 * of the process: in a scope opened by this thread, eight threads that
 * each build a Kern_RBF (MPARAMS 1.0, VERBOSE 1) at the same time print
 * kRbfLine1 exactly once between them (tadah hpo builds the evaluations'
 * models in the threads of its script executor).
 * Before the fix: KernelSettingsLineScope does not exist (the file does
 * not compile). */
TEST_CASE("A KernelSettingsLineScope covers the kernels every thread builds",
          "[kernel][settings_line]") {
  KernelSettingsLineScope scope;
  const std::string out = printed_by([] {
    /* The contexts are made here: Context::add of VERBOSE sets the
     * Logger's level, which the threads must not race on. */
    const std::vector<Context> ctx(8, kernel_ctx(1, "1.0"));
    std::vector<std::thread> threads;
    for (const Context &c : ctx)
      threads.emplace_back([&c] { tadah::models::Kern_RBF k(c); });
    for (auto &t : threads) t.join();
  });
  INFO("output: '" << out << "'");
  CHECK(occurrences(out, kRbfLine1) == 1u);
  CHECK(occurrences(out, "Kern_RBF") == 1u);
}
