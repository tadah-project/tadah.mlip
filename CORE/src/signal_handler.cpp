#include <tadah/core/signal_handler.h>

#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstddef>
#include <cstring>

#include <unistd.h>

namespace tadah {
namespace core {

namespace {

volatile std::sig_atomic_t g_stop_flag    = 0;
volatile std::sig_atomic_t g_signal_seen  = 0;
bool                       g_installed    = false;

/* How many times each watched signal has been delivered. A lock-free
 * atomic is safe in a signal handler, and fetch_add makes "the second" a
 * single delivery even when two threads take the signal at once. */
static_assert(std::atomic<int>::is_always_lock_free,
              "the signal handler needs a lock-free counter");
std::atomic<int> g_sigint_count{0};
std::atomic<int> g_sigterm_count{0};
std::atomic<int> g_sigusr2_count{0};

/* Writes a message to standard error with write(2), the async-signal-safe
 * call; what it returns is of no use in a handler. */
template <std::size_t N>
void say(const char (&text)[N]) noexcept
{
    const ssize_t written = ::write(STDERR_FILENO, text, N - 1);
    (void)written;
}

/* The second signal of its kind: the default action, now. The handler
 * runs with the signal blocked, so it is unblocked before it is raised
 * again (sigprocmask, async-signal-safe: on Linux it is the calling
 * thread's mask), and the process ends killed by it -- the status a shell
 * reports as 128 + signo. _exit is the last resort if it survives. */
void end_now(int signo) noexcept
{
    struct sigaction dfl{};
    dfl.sa_handler = SIG_DFL;
    sigemptyset(&dfl.sa_mask);
    ::sigaction(signo, &dfl, nullptr);
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, signo);
    ::sigprocmask(SIG_UNBLOCK, &set, nullptr);
    ::raise(signo);
    ::_exit(128 + signo);
}

/* Async-signal-safe handler: atomics, write(2), sigaction, sigprocmask,
 * raise and _exit only. */
extern "C" void tadah_signal_handler(int signo) noexcept
{
    const int saved_errno = errno;
    std::atomic<int>& count = signo == SIGINT    ? g_sigint_count
                              : signo == SIGTERM ? g_sigterm_count
                                                 : g_sigusr2_count;
    const int earlier = count.fetch_add(1);
    /* A second SIGINT or SIGTERM: the user does not want to wait for the
     * stop the first one asked for. A second SIGUSR2 (a scheduler's
     * advance warning) and a signal of another kind (SLURM's SIGTERM after
     * its USR2 warning, an MPI launcher's SIGTERM after a terminal's
     * SIGINT) leave that stop to finish. */
    if (earlier > 0 && signo == SIGINT) {
        say("tadah: second SIGINT: ending at once\n");
        end_now(signo);
    }
    if (earlier > 0 && signo == SIGTERM) {
        say("tadah: second SIGTERM: ending at once\n");
        end_now(signo);
    }
    /* Only record the first signal -- later signals don't downgrade the
     * reason text shown to the user. */
    if (g_signal_seen == 0) g_signal_seen = signo;
    g_stop_flag = 1;
    /* The first of its kind is announced at once: the loops notice the
     * flag only at their next safe point, which a long evaluation can put
     * far off. */
    if (earlier == 0) {
        if (signo == SIGINT)
            say("tadah: SIGINT received: stopping once the work in progress "
                "can stop and save its results; a second SIGINT (Ctrl-C) "
                "ends tadah at once, and what is not yet written is lost\n");
        else if (signo == SIGTERM)
            say("tadah: SIGTERM received: stopping once the work in progress "
                "can stop and save its results; a second SIGTERM ends tadah "
                "at once, and what is not yet written is lost\n");
        else if (signo == SIGUSR2)
            say("tadah: SIGUSR2 received: stopping once the work in progress "
                "can stop and save its results\n");
    }
    errno = saved_errno;
}

}   // namespace

void SignalHandler::install()
{
    if (g_installed) return;

    struct sigaction sa{};
    sa.sa_handler = &tadah_signal_handler;
    sigemptyset(&sa.sa_mask);
    /* SA_RESTART so a slow read()/getline() doesn't error out with
     * EINTR; the per-eval poll picks the flag up at the next safe
     * point. */
    sa.sa_flags = SA_RESTART;

    /* Install for the three signals we care about. Failures are
     * non-fatal — the flag-based polling falls through to "no signal
     * seen" which is the expected behaviour without a handler. */
    ::sigaction(SIGINT,  &sa, nullptr);
    ::sigaction(SIGTERM, &sa, nullptr);
    ::sigaction(SIGUSR2, &sa, nullptr);

    g_installed = true;
}

bool SignalHandler::stop_requested()
{
    return g_stop_flag != 0;
}

int SignalHandler::signal_seen()
{
    return static_cast<int>(g_signal_seen);
}

std::string SignalHandler::reason()
{
    int s = static_cast<int>(g_signal_seen);
    switch (s) {
        case 0:        return "no signal";
        case SIGINT:   return "SIGINT";
        case SIGTERM:  return "SIGTERM";
        case SIGUSR2:  return "SIGUSR2";
        default:       return std::string("signal ") + std::to_string(s);
    }
}

}   // namespace core
}   // namespace tadah
