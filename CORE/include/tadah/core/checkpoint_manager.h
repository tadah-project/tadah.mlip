#ifndef TADAH_CORE_CHECKPOINT_MANAGER_H
#define TADAH_CORE_CHECKPOINT_MANAGER_H

/*
 *  CheckpointManager — singleton coordinator for checkpoint participants.
 *
 *  No Tadah module registers participants at present: HPO's checkpoint
 *  resume was removed in 1.6.0 together with the BASINS strategies that
 *  used it. The intended lifecycle for a host program:
 *    1. main() calls SignalHandler::install().
 *    2. main() calls instance().set_path(<file>) and
 *       set_period_seconds(...) / set_period_evals(...) from the parsed
 *       config.
 *    3. Each long-running subsystem registers itself via register_(this).
 *       Order matters only insofar as load() is replayed in registration
 *       order — register lower-level state first (e.g. a journal's
 *       counters) before higher-level state (e.g. an optimiser's round
 *       cursor).
 *    4. main() calls try_load() exactly once. If a file existed, it
 *       parses it and replays load() on every registered participant.
 *    5. The evaluation loop calls tick_eval(global_eval) after every
 *       evaluation. The manager fires save_now() on schedule.
 *    6. SignalHandler-driven shutdown calls request_flush(); the next
 *       safe point (tick_eval / explicit save_now) writes the final
 *       checkpoint, then the loop throws ShutdownRequested.
 *
 *  Thread-safety: register/unregister/save guarded by a mutex.
 *  tick_eval is called from worker threads and uses an atomic counter
 *  to avoid taking the mutex on the fast path; only when the period
 *  trigger fires does it serialise on the mutex.
 */

#include <atomic>
#include <chrono>
#include <cstddef>
#include <mutex>
#include <string>
#include <vector>

namespace tadah {
namespace core {

class ICheckpointable;

class CheckpointManager {
public:
    static CheckpointManager& instance();

    /* Configuration -------------------------------------------------- */
    void set_path(std::string path);
    void set_period_seconds(double s);   // 0 disables
    void set_period_evals(std::size_t n);// 0 disables
    /* Whether to allow the constructor of a participant to immediately
     * snapshot itself (off by default; the manager only triggers saves
     * on schedule or on explicit save_now()). */

    /* Participants --------------------------------------------------- */
    void register_(ICheckpointable* p);
    void unregister(ICheckpointable* p);

    /* Per-eval tick (called from worker threads). Cheap on the fast
     * path; only takes the mutex when a save period elapses. */
    void tick_eval(std::size_t global_eval);

    /* Force a save at the next safe point (i.e. set the flag; actual
     * writing happens at the next tick_eval / save_now / save_if_due
     * boundary). Async-signal-safe-ish: writes one std::atomic<bool>.*/
    void request_flush();

    /* Synchronous save. Runs save() on every participant in
     * registration order, then commits the file atomically. */
    void save_now();

    /* If a saved checkpoint exists at the configured path, parse it
     * and replay load() on every registered participant in
     * registration order. Returns false if no file was found.
     * Throws std::runtime_error on parse error.
     *
     * MUST be called after all participants have registered. */
    bool try_load();

    /* Becomes true after a successful try_load(). */
    bool was_resumed() const { return resumed_; }

    /* Test-only: drop registrations and reset state. Lets the unit
     * tests run without process-global pollution. */
    void reset_for_testing();

    /* Inspect path (for logging). */
    const std::string& path() const { return path_; }

private:
    CheckpointManager() = default;
    CheckpointManager(const CheckpointManager&) = delete;
    CheckpointManager& operator=(const CheckpointManager&) = delete;

    using Clock = std::chrono::steady_clock;

    std::string                        path_         = "checkpoint.tadah";
    std::vector<ICheckpointable*>      participants_;   // non-owning
    std::mutex                         mtx_;

    /* Period configuration. */
    double                             period_secs_  = 0.0;
    std::size_t                        period_evals_ = 0;

    /* State for trigger logic. */
    Clock::time_point                  last_save_t_  = Clock::now();
    std::atomic<std::size_t>           last_save_eval_ {0};
    std::atomic<bool>                  flush_request_ {false};
    bool                               resumed_      = false;
};

}   // namespace core
}   // namespace tadah

#endif
