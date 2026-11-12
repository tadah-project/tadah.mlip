// test_signal_handler.cpp
//
// core::SignalHandler turns the first SIGINT, SIGTERM or SIGUSR2 into a stop
// request that long-running loops poll (tadah hpo stops once the
// evaluations in progress finish and writes its results). A second SIGINT
// or SIGTERM, which a user sends when that wait is too long (a LAMMPS
// script's long run, a slow fit), ends the process at once, killed by the
// signal. The tests run in child processes -- this executable re-run on a
// hidden case, which installs the handler in a fresh process and raises
// the signals TADAH_TEST_SIGNALS lists -- because the second signal ends
// the process that receives it.

#include "catch2/catch.hpp"

#include <tadah/core/signal_handler.h>

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

using tadah::core::SignalHandler;

namespace {

int signal_number(const std::string& name)
{
    if (name == "INT") return SIGINT;
    if (name == "TERM") return SIGTERM;
    if (name == "USR2") return SIGUSR2;
    return 0;
}

/* What a child run wrote (standard output and error together) and how it
 * ended. */
struct ChildRun {
    std::string output;
    int status = 0;
};

/* This executable on the hidden case [.signal_handler_child], with
 * TADAH_TEST_SIGNALS=`signals` (comma-separated names: INT, TERM, USR2). */
ChildRun run_child(const std::string& signals)
{
    int fds[2];
    REQUIRE(::pipe(fds) == 0);
    const pid_t pid = ::fork();
    REQUIRE(pid >= 0);
    if (pid == 0) {
        ::dup2(fds[1], STDOUT_FILENO);
        ::dup2(fds[1], STDERR_FILENO);
        ::close(fds[0]);
        ::close(fds[1]);
        ::setenv("TADAH_TEST_SIGNALS", signals.c_str(), 1);
        ::execl("/proc/self/exe", "test_core", "[.signal_handler_child]",
                static_cast<char*>(nullptr));
        ::_exit(127);
    }
    ::close(fds[1]);
    ChildRun run;
    char buf[4096];
    for (ssize_t n; (n = ::read(fds[0], buf, sizeof buf)) > 0;)
        run.output.append(buf, static_cast<std::size_t>(n));
    ::close(fds[0]);
    REQUIRE(::waitpid(pid, &run.status, 0) == pid);
    return run;
}

std::size_t count_of(const std::string& text, const std::string& needle)
{
    std::size_t n = 0;
    for (auto p = text.find(needle); p != std::string::npos;
         p = text.find(needle, p + needle.size()))
        ++n;
    return n;
}

const std::string kStopping =
    " received: stopping once the work in progress can stop and save its "
    "results";

}  // namespace

/* The child: installs the handler, raises each signal of
 * TADAH_TEST_SIGNALS in turn and says after each that it is still running,
 * then names the first signal it recorded. Hidden: it runs only in the
 * children of the cases below. */
TEST_CASE("SignalHandler: child that raises the signals it is given",
          "[.signal_handler_child]")
{
    const char* list = std::getenv("TADAH_TEST_SIGNALS");
    REQUIRE(list != nullptr);
    SignalHandler::install();
    std::string names = list;
    std::size_t start = 0;
    while (start <= names.size()) {
        const std::size_t end = names.find(',', start);
        const std::string name = names.substr(
            start, end == std::string::npos ? std::string::npos : end - start);
        const int signo = signal_number(name);
        REQUIRE(signo != 0);
        ::raise(signo);
        /* written with write(2): what a signal ends is not flushed */
        const std::string line = "still running after SIG" + name + "\n";
        REQUIRE(::write(STDOUT_FILENO, line.data(), line.size()) ==
                static_cast<ssize_t>(line.size()));
        if (end == std::string::npos) break;
        start = end + 1;
    }
    const std::string seen = "first signal seen: " + SignalHandler::reason() +
                             (SignalHandler::stop_requested() ? ", stop requested"
                                                              : "") + "\n";
    REQUIRE(::write(STDOUT_FILENO, seen.data(), seen.size()) ==
            static_cast<ssize_t>(seen.size()));
}

/* Pins: a second SIGINT, or a second SIGTERM, ends the process at once,
 * killed by that signal (WIFSIGNALED, WTERMSIG -- the shell's status 130 or
 * 143), after the first one asked for the stop: the child says it still
 * runs after the first and not after the second. The first is announced on
 * standard error at once, with what a second one does: "tadah: SIGINT
 * received: stopping once the work in progress can stop and save its
 * results; a second SIGINT (Ctrl-C) ends tadah at once, and what is not yet
 * written is lost" (SIGTERM: "a second SIGTERM ends tadah at once, ..."),
 * and the second with "tadah: second SIGINT: ending at once".
 * Before the fix: the second signal only set the stop flag again, so the
 * child ran on, said so after both signals and exited 0 (WIFSIGNALED is
 * false), and nothing was written at either signal. */
TEST_CASE("SignalHandler: a second SIGINT or SIGTERM ends the process at "
          "once, killed by the signal",
          "[signal_handler][escalation]")
{
    for (const auto& [name, ctrl_c] :
         std::vector<std::pair<std::string, std::string>>{
             {"INT", " (Ctrl-C)"}, {"TERM", ""}}) {
        const ChildRun run = run_child(name + "," + name);
        INFO("SIG" << name << ", output:\n" << run.output);
        REQUIRE(WIFSIGNALED(run.status));
        CHECK(WTERMSIG(run.status) == signal_number(name));
        CHECK(count_of(run.output, "still running after SIG" + name) == 1u);
        CHECK(count_of(run.output, "first signal seen") == 0u);
        CHECK(count_of(run.output, "tadah: SIG" + name + kStopping +
                                       "; a second SIG" + name + ctrl_c +
                                       " ends tadah at once, and what is not "
                                       "yet written is lost\n") == 1u);
        CHECK(count_of(run.output,
                       "tadah: second SIG" + name + ": ending at once\n") ==
              1u);
        /* the announcement comes before the child goes on */
        CHECK(run.output.find("tadah: SIG" + name) <
              run.output.find("still running after SIG" + name));
    }
}

/* Pins: signals of different kinds do not end the process, nor does a
 * second SIGUSR2 (a scheduler's advance warning: SLURM's
 * --signal=B:USR2@<seconds> may be followed by its SIGTERM, and an MPI
 * launcher may send SIGTERM after a terminal's SIGINT, and the graceful
 * stop must go on): SIGINT, SIGTERM, SIGUSR2, SIGUSR2 leave the child
 * running to its end (exit 0), the first signal recorded (SIGINT) and the
 * stop requested; the first of each kind is announced once, SIGUSR2's
 * without a second-signal clause ("tadah: SIGUSR2 received: stopping once
 * the work in progress can stop and save its results\n").
 * Before the fix: the child ran to its end as well (a pin for that part),
 * and nothing was announced: the count checks fail. */
TEST_CASE("SignalHandler: signals of different kinds, and a second SIGUSR2, "
          "do not end the process",
          "[signal_handler][escalation]")
{
    const ChildRun run = run_child("INT,TERM,USR2,USR2");
    INFO("output:\n" << run.output);
    REQUIRE(WIFEXITED(run.status));
    CHECK(WEXITSTATUS(run.status) == 0);
    CHECK(count_of(run.output, "still running after SIG") == 4u);
    CHECK(count_of(run.output,
                   "first signal seen: SIGINT, stop requested\n") == 1u);
    CHECK(count_of(run.output, "tadah: SIGINT" + kStopping) == 1u);
    CHECK(count_of(run.output, "tadah: SIGTERM" + kStopping +
                                   "; a second SIGTERM ends tadah at once") ==
          1u);
    CHECK(count_of(run.output, "tadah: SIGUSR2" + kStopping + "\n") == 1u);
    CHECK(count_of(run.output, "tadah: second") == 0u);
}
