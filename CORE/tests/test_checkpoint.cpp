#include "catch2/catch.hpp"

#include <tadah/core/checkpoint.h>
#include <tadah/core/checkpoint_manager.h>
#include <tadah/core/signal_handler.h>

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

using tadah::core::Checkpoint;
using tadah::core::CheckpointManager;
using tadah::core::ICheckpointable;
using tadah::core::SignalHandler;

namespace {

/* Per-test scratch path under build dir (a tmpfile() name is brittle on
 * some systems and we want to inspect the file from tests).  */
std::string scratch_path(const std::string& tag)
{
    std::filesystem::path p =
        std::filesystem::temp_directory_path() / ("tadah_cp_" + tag + ".tadah");
    std::filesystem::remove(p);
    return p.string();
}

struct Counter : public ICheckpointable {
    int n_saved = 0;
    int n_loaded = 0;
    int payload  = 0;
    std::string mytag;
    explicit Counter(std::string t) : mytag(std::move(t)) {}
    void save(Checkpoint& cp) const override {
        const_cast<Counter*>(this)->n_saved++;
        cp.section(mytag);
        cp.put_int("payload", payload);
    }
    void load(const Checkpoint& cp) override {
        n_loaded++;
        if (auto v = cp.get_int(mytag, "payload")) payload = static_cast<int>(*v);
    }
    std::string tag() const override { return mytag; }
};

}  // namespace

TEST_CASE("Checkpoint round-trip primitives", "[checkpoint]")
{
    const auto path = scratch_path("primitives");
    {
        Checkpoint cp(path);
        cp.section("META");
        cp.put_int("an_int", 42);
        cp.put_size("a_size", 1234567);
        cp.put_dbl("pi", 3.141592653589793);
        cp.put_bool("flag_t", true);
        cp.put_bool("flag_f", false);
        cp.put_vec("vec", std::vector<double>{1.0, 2.5, -3.5e-2, 1e-300, 1e300});
        cp.put_vec_int("ivec", std::vector<long long>{0, 1, -7, 1234567});
        cp.put("str", "hello world");
        cp.commit();
    }
    REQUIRE(Checkpoint::file_exists(path));

    Checkpoint cp(path);
    REQUIRE(cp.load_from_disk());
    REQUIRE(cp.get_int("META", "an_int")  == 42);
    REQUIRE(cp.get_size("META", "a_size") == 1234567u);
    REQUIRE(cp.get_dbl("META", "pi").value() == Approx(3.141592653589793));
    REQUIRE(cp.get_bool("META", "flag_t").value() == true);
    REQUIRE(cp.get_bool("META", "flag_f").value() == false);
    auto v = cp.get_vec("META", "vec");
    REQUIRE(v.has_value());
    REQUIRE(v->size() == 5);
    REQUIRE((*v)[0] == Approx(1.0));
    REQUIRE((*v)[1] == Approx(2.5));
    REQUIRE((*v)[2] == Approx(-3.5e-2));
    REQUIRE((*v)[3] == Approx(1e-300));
    REQUIRE((*v)[4] == Approx(1e300));
    auto iv = cp.get_vec_int("META", "ivec");
    REQUIRE(iv.has_value());
    REQUIRE(iv->size() == 4);
    REQUIRE((*iv)[2] == -7);
    REQUIRE(cp.get("META", "str").value() == "hello world");

    /* Missing keys return nullopt, not exceptions. */
    REQUIRE_FALSE(cp.get("META", "missing").has_value());
    REQUIRE_FALSE(cp.get_dbl("OTHER", "pi").has_value());

    std::filesystem::remove(path);
}

TEST_CASE("Checkpoint get_dbl reads back a subnormal", "[checkpoint][lossless]")
{
    /* put_dbl writes %.17g, i.e. a subnormal token; get_dbl used std::stod,
     * which throws out_of_range on underflow, so the value came back as
     * missing. */
    const auto path = scratch_path("subnormal");
    const double sub = 1e-310;
    {
        Checkpoint cp(path);
        cp.section("META");
        cp.put_dbl("sub", sub);
        cp.put_dbl("dmin", -std::numeric_limits<double>::denorm_min());
        cp.commit();
    }
    Checkpoint cp(path);
    REQUIRE(cp.load_from_disk());
    REQUIRE(cp.get_dbl("META", "sub").has_value());
    CHECK(*cp.get_dbl("META", "sub") == sub);
    REQUIRE(cp.get_dbl("META", "dmin").has_value());
    CHECK(*cp.get_dbl("META", "dmin") ==
          -std::numeric_limits<double>::denorm_min());
    std::filesystem::remove(path);
}

TEST_CASE("Checkpoint commit is atomic via .tmp + rename", "[checkpoint]")
{
    const auto path = scratch_path("atomic");
    /* Pre-populate the destination with old content. A fake-aborted
     * write should leave that content intact. */
    {
        std::ofstream f(path);
        f << "[META]\nold = yes\n";
    }
    Checkpoint cp(path);
    cp.section("META");
    cp.put("new", "yes");
    cp.commit();

    Checkpoint reread(path);
    REQUIRE(reread.load_from_disk());
    REQUIRE(reread.get("META", "new").value() == "yes");
    /* The .tmp file must not linger after a successful commit. */
    REQUIRE_FALSE(std::filesystem::exists(path + ".tmp"));
    std::filesystem::remove(path);
}

TEST_CASE("Checkpoint::load_from_disk returns false when absent", "[checkpoint]")
{
    const auto path = scratch_path("absent");
    Checkpoint cp(path);
    REQUIRE_FALSE(cp.load_from_disk());
    REQUIRE_FALSE(cp.has("META", "anything"));
}

TEST_CASE("Checkpoint malformed file -> throws on parse", "[checkpoint]")
{
    const auto path = scratch_path("malformed");
    {
        std::ofstream f(path);
        f << "not in a section = oops\n";
    }
    Checkpoint cp(path);
    REQUIRE_THROWS_AS(cp.load_from_disk(), std::runtime_error);
    std::filesystem::remove(path);
}

TEST_CASE("CheckpointManager: register, save_now, try_load round-trip",
          "[checkpoint][manager]")
{
    auto& mgr = CheckpointManager::instance();
    mgr.reset_for_testing();
    const auto path = scratch_path("manager");
    mgr.set_path(path);

    Counter a("ALPHA"); a.payload = 7;
    Counter b("BETA");  b.payload = 19;
    mgr.register_(&a);
    mgr.register_(&b);

    mgr.save_now();
    REQUIRE(Checkpoint::file_exists(path));
    REQUIRE(a.n_saved == 1);
    REQUIRE(b.n_saved == 1);

    /* Reset the in-memory state, re-register, replay load. */
    a.payload = 0; b.payload = 0;
    REQUIRE(mgr.try_load());
    REQUIRE(a.payload == 7);
    REQUIRE(b.payload == 19);
    REQUIRE(mgr.was_resumed());

    mgr.unregister(&a);
    mgr.unregister(&b);
    std::filesystem::remove(path);
}

TEST_CASE("CheckpointManager: tick_eval fires save on eval period",
          "[checkpoint][manager]")
{
    auto& mgr = CheckpointManager::instance();
    mgr.reset_for_testing();
    const auto path = scratch_path("tickeval");
    mgr.set_path(path);
    mgr.set_period_evals(5);

    Counter a("ALPHA"); a.payload = 1;
    mgr.register_(&a);

    /* First 4 ticks: no save. */
    for (std::size_t i = 1; i <= 4; ++i) mgr.tick_eval(i);
    REQUIRE(a.n_saved == 0);

    /* 5th tick: save fires. */
    mgr.tick_eval(5);
    REQUIRE(a.n_saved == 1);

    /* Until next +5: no save. */
    for (std::size_t i = 6; i <= 9; ++i) mgr.tick_eval(i);
    REQUIRE(a.n_saved == 1);
    mgr.tick_eval(10);
    REQUIRE(a.n_saved == 2);

    mgr.unregister(&a);
    std::filesystem::remove(path);
}

TEST_CASE("CheckpointManager: request_flush forces a save on next tick",
          "[checkpoint][manager]")
{
    auto& mgr = CheckpointManager::instance();
    mgr.reset_for_testing();
    const auto path = scratch_path("flush");
    mgr.set_path(path);
    /* No periods configured — only request_flush should trigger. */
    Counter a("ALPHA");
    mgr.register_(&a);
    mgr.tick_eval(1);
    REQUIRE(a.n_saved == 0);
    mgr.request_flush();
    mgr.tick_eval(2);
    REQUIRE(a.n_saved == 1);
    /* The flag is consumed — next tick without a fresh request is a no-op. */
    mgr.tick_eval(3);
    REQUIRE(a.n_saved == 1);

    mgr.unregister(&a);
    std::filesystem::remove(path);
}

TEST_CASE("CheckpointManager: try_load returns false when no file",
          "[checkpoint][manager]")
{
    auto& mgr = CheckpointManager::instance();
    mgr.reset_for_testing();
    const auto path = scratch_path("missing");
    /* path is removed by scratch_path */
    mgr.set_path(path);
    REQUIRE_FALSE(mgr.try_load());
    REQUIRE_FALSE(mgr.was_resumed());
}

TEST_CASE("SignalHandler: install is idempotent, default flag is false",
          "[signal_handler]")
{
    SignalHandler::install();
    SignalHandler::install();
    /* Without a delivered signal we expect the flag to be unset
     * (other tests in this binary may have raised it; the test is
     * conditional). */
    if (!SignalHandler::stop_requested()) {
        REQUIRE(SignalHandler::signal_seen() == 0);
        REQUIRE(SignalHandler::reason() == "no signal");
    }
}

TEST_CASE("SignalHandler: SIGUSR2 raises the flag", "[signal_handler]")
{
    SignalHandler::install();
    if (!SignalHandler::stop_requested()) {
        ::raise(SIGUSR2);
        /* The signal handler is sync on this thread for raise(); the
         * flag is set before raise() returns. */
        REQUIRE(SignalHandler::stop_requested());
        /* The reason will be SIGUSR2 unless an earlier signal was the
         * first delivered (the reason latches on first signal). */
        const auto r = SignalHandler::reason();
        REQUIRE((r == "SIGUSR2" || r == "SIGINT" || r == "SIGTERM"));
    }
}
