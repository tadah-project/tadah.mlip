#include <tadah/core/checkpoint_manager.h>

#include <tadah/core/checkpoint.h>

#include <algorithm>
#include <stdexcept>

namespace tadah {
namespace core {

CheckpointManager& CheckpointManager::instance()
{
    static CheckpointManager inst;
    return inst;
}

void CheckpointManager::set_path(std::string p)
{
    std::lock_guard<std::mutex> lg(mtx_);
    path_ = std::move(p);
}

void CheckpointManager::set_period_seconds(double s)
{
    std::lock_guard<std::mutex> lg(mtx_);
    period_secs_ = (s > 0.0) ? s : 0.0;
}

void CheckpointManager::set_period_evals(std::size_t n)
{
    std::lock_guard<std::mutex> lg(mtx_);
    period_evals_ = n;
}

void CheckpointManager::register_(ICheckpointable* p)
{
    if (!p) return;
    std::lock_guard<std::mutex> lg(mtx_);
    if (std::find(participants_.begin(), participants_.end(), p) ==
        participants_.end()) {
        participants_.push_back(p);
    }
}

void CheckpointManager::unregister(ICheckpointable* p)
{
    if (!p) return;
    std::lock_guard<std::mutex> lg(mtx_);
    auto it = std::find(participants_.begin(), participants_.end(), p);
    if (it != participants_.end()) participants_.erase(it);
}

void CheckpointManager::request_flush()
{
    flush_request_.store(true, std::memory_order_relaxed);
}

void CheckpointManager::tick_eval(std::size_t global_eval)
{
    /* Fast-path test without taking the mutex. */
    if (period_evals_ == 0 && period_secs_ <= 0.0 &&
        !flush_request_.load(std::memory_order_relaxed)) return;

    bool fire = flush_request_.load(std::memory_order_relaxed);
    if (period_evals_ != 0) {
        std::size_t prev = last_save_eval_.load(std::memory_order_relaxed);
        if (global_eval >= prev + period_evals_) fire = true;
    }
    if (period_secs_ > 0.0) {
        /* time check needs the mutex anyway because last_save_t_ is
         * not atomic-friendly; do it inline below. */
        std::lock_guard<std::mutex> lg(mtx_);
        auto now = Clock::now();
        std::chrono::duration<double> elapsed = now - last_save_t_;
        if (elapsed.count() >= period_secs_) fire = true;
    }
    if (!fire) return;

    /* save_now takes the mutex itself. */
    save_now();
    last_save_eval_.store(global_eval, std::memory_order_relaxed);
    flush_request_.store(false, std::memory_order_relaxed);
}

void CheckpointManager::save_now()
{
    /* Snapshot participants under the lock (so registrations don't
     * race), then run save() outside the lock to keep the critical
     * section short. */
    std::vector<ICheckpointable*> snap;
    std::string path;
    {
        std::lock_guard<std::mutex> lg(mtx_);
        snap = participants_;
        path = path_;
        last_save_t_ = Clock::now();
    }

    Checkpoint cp(path);
    for (auto* p : snap) p->save(cp);
    cp.commit();
}

bool CheckpointManager::try_load()
{
    std::vector<ICheckpointable*> snap;
    std::string path;
    {
        std::lock_guard<std::mutex> lg(mtx_);
        snap = participants_;
        path = path_;
    }

    Checkpoint cp(path);
    if (!cp.load_from_disk()) {
        resumed_ = false;
        return false;
    }
    for (auto* p : snap) p->load(cp);
    resumed_ = true;
    return true;
}

void CheckpointManager::reset_for_testing()
{
    std::lock_guard<std::mutex> lg(mtx_);
    participants_.clear();
    path_         = "checkpoint.tadah";
    period_secs_  = 0.0;
    period_evals_ = 0;
    last_save_t_  = Clock::now();
    last_save_eval_.store(0);
    flush_request_.store(false);
    resumed_      = false;
}

}   // namespace core
}   // namespace tadah
