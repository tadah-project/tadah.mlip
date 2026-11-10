#ifndef TADAH_CORE_SIGNAL_HANDLER_H
#define TADAH_CORE_SIGNAL_HANDLER_H

/*
 *  SignalHandler — async-signal-safe stop flag for long-running loops.
 *
 *  install() registers handlers for SIGINT, SIGTERM, and SIGUSR2:
 *    - SIGINT  : ctrl+C from interactive shells.
 *    - SIGTERM : default `kill` and SLURM cancellation.
 *    - SIGUSR2 : SLURM advance warning when configured with
 *                `--signal=B:USR2@<seconds>`.
 *
 *  The first of these signals raises the stop flag, which long-running
 *  loops poll via stop_requested() (tadah hpo stops once the evaluations
 *  in progress finish and writes its results), and the first of each kind
 *  is announced at once on standard error, as the stop itself can be far
 *  off: "tadah: SIGINT received: stopping once the work in progress can
 *  stop and save its results; a second SIGINT (Ctrl-C) ends tadah at
 *  once, and what is not yet written is lost" (SIGTERM likewise; SIGUSR2's
 *  line has no second-signal clause).
 *
 *  A second SIGINT, or a second SIGTERM, ends the process at once: the
 *  handler writes "tadah: second SIGINT: ending at once", restores the
 *  default action and re-raises the signal, so the process is killed by
 *  it (a shell reports 128 + signo: 130, 143). Signals of different kinds
 *  do not escalate -- SLURM's SIGTERM after its --signal=USR2 warning, an
 *  MPI launcher's SIGTERM after a terminal's SIGINT -- nor does a second
 *  SIGUSR2: the stop the first asked for goes on. The PC_ISOLATION
 *  workers of tadah hpo ignore the three signals and die with the
 *  process (PR_SET_PDEATHSIG); under MPI the launcher ends the other
 *  ranks of a rank killed this way.
 *
 *  The handler itself is async-signal-safe: lock-free atomic counters, a
 *  std::sig_atomic_t flag and the signal number, write(2), and for a
 *  second signal sigaction, sigprocmask, raise and _exit. All "what
 *  happened" reasoning (reason text, dispatch to participants) runs in
 *  normal context.
 *
 *  Once raised, the flag is never lowered. Re-entering main() in a
 *  fresh process is the only way to reset, which is intentional —
 *  partially-honored shutdowns are confusing.
 */

#include <string>

namespace tadah {
namespace core {

class SignalHandler {
public:
    /* Install handlers for SIGINT/SIGTERM/SIGUSR2. Idempotent — repeat
     * calls are no-ops. Keeps any previous SIGCHLD/SIGPIPE/etc.
     * handlers untouched. */
    static void install();

    /* True once any of the watched signals has been delivered. Polled
     * by the HPO per-eval loop (HPO_Host::operator()). A second SIGINT or
     * SIGTERM does not return here: it ends the process. */
    static bool stop_requested();

    /* Numeric signal that triggered the stop (0 if none yet). */
    static int  signal_seen();

    /* Human-readable description ("SIGINT", "SIGTERM", "SIGUSR2",
     * or "no signal"). Safe to call any time. */
    static std::string reason();

private:
    SignalHandler() = delete;
};

}   // namespace core
}   // namespace tadah

#endif
