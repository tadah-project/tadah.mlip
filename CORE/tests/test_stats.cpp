// Catch2 v2 unit tests for tadah::core::stats utilities.
// Build glob in CORE/tests/CMakeLists.txt picks this up automatically.

#include "catch2/catch.hpp"
#include <tadah/core/stats/percentiles.h>
#include <tadah/core/stats/histogram.h>
#include <tadah/core/stats/kde.h>
#include <tadah/core/stats/column_stats.h>

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

using namespace tadah::core::stats;

TEST_CASE("percentile linear interpolation matches numpy method=linear") {
    std::vector<double> v{1.0, 2.0, 3.0, 4.0, 5.0};

    SECTION("p0 == min, p100 == max, p50 == median for odd n") {
        REQUIRE(percentile(v, 0.0) == Approx(1.0));
        REQUIRE(percentile(v, 1.0) == Approx(5.0));
        REQUIRE(percentile(v, 0.5) == Approx(3.0));
    }
    SECTION("interpolation between samples") {
        // p25 of {1..5}: h = 0.25 * 4 = 1.0 -> v[1] = 2.0
        REQUIRE(percentile(v, 0.25) == Approx(2.0));
        // p10: h = 0.10 * 4 = 0.4 -> v[0] + 0.4*(v[1]-v[0]) = 1.4
        REQUIRE(percentile(v, 0.10) == Approx(1.4));
    }
    SECTION("rejects empty sample") {
        std::vector<double> e;
        REQUIRE_THROWS_AS(percentile(e, 0.5), std::invalid_argument);
    }
    SECTION("rejects out-of-range q") {
        REQUIRE_THROWS_AS(percentile(v, -0.1), std::invalid_argument);
        REQUIRE_THROWS_AS(percentile(v, 1.5),  std::invalid_argument);
    }
    SECTION("single-element sample returns that element") {
        std::vector<double> one{42.0};
        REQUIRE(percentile(one, 0.0)  == Approx(42.0));
        REQUIRE(percentile(one, 0.99) == Approx(42.0));
    }
}

TEST_CASE("multi_percentile sorts once and returns in the requested order") {
    std::vector<double> v{5.0, 1.0, 4.0, 2.0, 3.0};
    auto pcts = multi_percentile(v, {0.0, 0.5, 1.0});
    REQUIRE(pcts.size() == 3);
    REQUIRE(pcts[0] == Approx(1.0));
    REQUIRE(pcts[1] == Approx(3.0));
    REQUIRE(pcts[2] == Approx(5.0));
}

TEST_CASE("summary_percentiles emits all 11 entries with mean+std") {
    std::vector<double> v(100);
    for (std::size_t i = 0; i < 100; ++i) v[i] = static_cast<double>(i);
    auto s = summary_percentiles(v);
    // p1 ~ 0.99, p50 ~ 49.5, p99 ~ 98.01
    REQUIRE(s[0] == Approx(0.99));            // p1
    REQUIRE(s[2] == Approx(9.9));             // p10
    REQUIRE(s[4] == Approx(49.5));            // p50
    REQUIRE(s[8] == Approx(98.01));           // p99
    REQUIRE(s[9] == Approx(49.5));            // mean
    // sample stddev of 0..99 is sqrt(sum((i-49.5)^2)/99) = sqrt(8416.667) ~ 29.0115
    REQUIRE(s[10] == Approx(29.011491975).margin(1e-6));
}

TEST_CASE("histogram fixed-bin counts samples in [low, high)") {
    std::vector<double> v{0.0, 0.5, 0.99, 1.0, 1.5, 1.99, 2.5};
    auto h = histogram(v, 0.0, 3.0, 3);
    REQUIRE(h.n_bins == 3);
    REQUIRE(h.bin_width() == Approx(1.0));
    // bin 0 = [0,1): 0.0, 0.5, 0.99   -> 3
    // bin 1 = [1,2): 1.0, 1.5, 1.99  -> 3
    // bin 2 = [2,3): 2.5             -> 1
    REQUIRE(h.counts[0] == 3);
    REQUIRE(h.counts[1] == 3);
    REQUIRE(h.counts[2] == 1);
    REQUIRE(h.total_count() == 7);
}

TEST_CASE("histogram_fd produces a non-trivial binning for a real distribution") {
    std::mt19937 rng(0);
    std::normal_distribution<double> g(0.0, 1.0);
    std::vector<double> v;
    v.reserve(2000);
    for (int i = 0; i < 2000; ++i) v.push_back(g(rng));
    auto h = histogram_fd(v);
    REQUIRE(h.n_bins > 5);              // FD should produce a sensible count
    REQUIRE(h.total_count() == v.size()); // every sample assigned (incl. max)
    // Centred histogram: bin near 0 should be the heaviest in expectation.
    std::size_t arg = 0, top = 0;
    for (std::size_t i = 0; i < h.n_bins; ++i) {
        if (h.counts[i] > top) { top = h.counts[i]; arg = i; }
    }
    const double centre_of_heaviest = h.centre(arg);
    REQUIRE(std::fabs(centre_of_heaviest) < 0.6);   // generous; test stability over RNG seed
}

TEST_CASE("histogram_fd handles constant input and empty input gracefully") {
    SECTION("empty") {
        std::vector<double> e;
        auto h = histogram_fd(e);
        REQUIRE(h.n_bins == 0);
    }
    SECTION("constant sample") {
        std::vector<double> c(50, 7.0);
        auto h = histogram_fd(c);
        REQUIRE(h.n_bins == 1);
        REQUIRE(h.total_count() == 50);
    }
}

TEST_CASE("histogram_peaks finds local maxima with prominence and separation") {
    Histogram h;
    h.low = 0.0;
    h.high = 10.0;
    h.n_bins = 10;
    h.counts = {1, 5, 10, 4, 2, 1, 3, 9, 4, 1};
    auto peaks = histogram_peaks(h, /*prominence=*/4, /*separation=*/0);
    REQUIRE(peaks.size() == 2);
    REQUIRE(peaks[0].bin == 2);
    REQUIRE(peaks[1].bin == 7);
    // Below-threshold peak (count 3 at bin 6) must not appear.
}

TEST_CASE("silverman_bandwidth shrinks with sample size") {
    std::mt19937 rng(0);
    std::normal_distribution<double> g(0.0, 1.0);
    auto draw = [&](std::size_t N) {
        std::vector<double> v(N);
        for (auto& x : v) x = g(rng);
        return v;
    };
    auto v100  = draw(100);
    auto v10000 = draw(10000);
    const double h100   = silverman_bandwidth(v100);
    const double h10000 = silverman_bandwidth(v10000);
    REQUIRE(h100   > 0.0);
    REQUIRE(h10000 > 0.0);
    REQUIRE(h10000 < h100);   // n^(-1/5) decreases with n
}

TEST_CASE("kde_evaluate then kde_peaks recover Gaussian centre") {
    std::mt19937 rng(0);
    std::normal_distribution<double> g(2.5, 0.4);
    std::vector<double> v(2000);
    for (auto& x : v) x = g(rng);
    const double bw = silverman_bandwidth(v);
    REQUIRE(bw > 0.0);
    std::vector<double> grid;
    grid.reserve(401);
    for (int i = 0; i <= 400; ++i) grid.push_back(0.0 + i * 0.025); // 0..10
    auto density = kde_evaluate(v, grid, bw);
    auto peaks = kde_peaks(grid, density);
    REQUIRE(!peaks.empty());
    // The strongest peak should be close to mean = 2.5.
    auto strongest = peaks[0];
    for (const auto& p : peaks) if (p.density > strongest.density) strongest = p;
    REQUIRE(strongest.position == Approx(2.5).margin(0.1));
    // FWHM of N(0, 0.4^2) is 2*sqrt(2*ln 2)*0.4 ~ 0.94
    REQUIRE(strongest.fwhm == Approx(0.94).margin(0.20));
}

TEST_CASE("kde_peaks can resolve a clear bimodal distribution") {
    std::mt19937 rng(1);
    std::normal_distribution<double> g1(0.0, 0.2);
    std::normal_distribution<double> g2(3.0, 0.2);
    std::vector<double> v;
    for (int i = 0; i < 1000; ++i) v.push_back(g1(rng));
    for (int i = 0; i < 1000; ++i) v.push_back(g2(rng));
    const double bw = silverman_bandwidth(v);
    REQUIRE(bw > 0.0);
    std::vector<double> grid;
    for (int i = 0; i <= 500; ++i) grid.push_back(-1.0 + i * 0.01); // -1..4
    auto density = kde_evaluate(v, grid, bw);
    auto peaks = kde_peaks(grid, density);
    REQUIRE(peaks.size() >= 2);
    // Sort peaks by position and check they bracket 0 and 3.
    std::sort(peaks.begin(), peaks.end(),
              [](const KdePeak& a, const KdePeak& b){ return a.position < b.position; });
    REQUIRE(peaks.front().position == Approx(0.0).margin(0.2));
    REQUIRE(peaks.back().position  == Approx(3.0).margin(0.2));
}

TEST_CASE("column_norm + column_norms agree on a small matrix") {
    // 3 rows, 2 cols, row-major:
    //  [1, 4]
    //  [2, 5]
    //  [3, 6]
    std::vector<double> M{1.0, 4.0, 2.0, 5.0, 3.0, 6.0};
    REQUIRE(column_norm(M.data(), 3, 2, 0) == Approx(std::sqrt(1.0 + 4.0 + 9.0)));
    REQUIRE(column_norm(M.data(), 3, 2, 1) == Approx(std::sqrt(16.0 + 25.0 + 36.0)));
    auto cn = column_norms(M.data(), 3, 2);
    REQUIRE(cn[0] == Approx(std::sqrt(14.0)));
    REQUIRE(cn[1] == Approx(std::sqrt(77.0)));
}

TEST_CASE("block_median_column_norm matches the median of the block") {
    // 1-row matrix, 4 cols: column norms == |entries|
    std::vector<double> M{1.0, 3.0, 5.0, 7.0};
    // block = cols [0..3] -> median of {1, 3, 5, 7} = (3 + 5) / 2 = 4
    REQUIRE(block_median_column_norm(M.data(), 1, 4, 0, 4) == Approx(4.0));
    // block = cols [1..2] -> median of {3, 5} = 4
    REQUIRE(block_median_column_norm(M.data(), 1, 4, 1, 2) == Approx(4.0));
    // block = single col 2 -> 5
    REQUIRE(block_median_column_norm(M.data(), 1, 4, 2, 1) == Approx(5.0));
    REQUIRE_THROWS_AS(block_median_column_norm(M.data(), 1, 4, 0, 0),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(block_median_column_norm(M.data(), 1, 4, 2, 5),
                      std::out_of_range);
}

TEST_CASE("block_norm_ratio_to_max picks the largest block as denominator") {
    std::vector<double> blocks{1.0, 5.0, 3.0};
    REQUIRE(block_norm_ratio_to_max(1.0, blocks) == Approx(0.2));
    REQUIRE(block_norm_ratio_to_max(5.0, blocks) == Approx(1.0));
    REQUIRE(block_norm_ratio_to_max(0.0, blocks) == Approx(0.0));
    SECTION("returns 1.0 when all blocks are zero (degenerate baseline)") {
        std::vector<double> z{0.0, 0.0};
        REQUIRE(block_norm_ratio_to_max(0.0, z) == Approx(1.0));
    }
    SECTION("returns 1.0 on empty input") {
        std::vector<double> e;
        REQUIRE(block_norm_ratio_to_max(0.0, e) == Approx(1.0));
    }
}

TEST_CASE("effective_rank counts singular values above rcond * s0") {
    // s0 = 10, rcond = 1e-3 -> cutoff = 1e-2
    std::vector<double> s{10.0, 5.0, 0.05, 0.005, 1e-6};
    REQUIRE(effective_rank(s, 1e-3) == 3);    // 10, 5, 0.05 above 0.01
    REQUIRE(effective_rank(s, 1.0) == 0);
    REQUIRE(effective_rank(s, 1e-12) == 5);
}

TEST_CASE("block_is_exact_zero distinguishes exact-zero from tiny-but-nonzero") {
    // 2 rows x 3 cols
    std::vector<double> M{0.0, 0.0, 1e-8,
                           0.0, 0.0, 0.0};
    REQUIRE(block_is_exact_zero(M.data(), 2, 3, 0, 2));   // first two cols all zero
    REQUIRE_FALSE(block_is_exact_zero(M.data(), 2, 3, 0, 3));  // last col has 1e-8 > 1e-15
    // A custom floor below 1e-8 must reject as well.
    REQUIRE_FALSE(block_is_exact_zero(M.data(), 2, 3, 0, 3, /*floor=*/1e-9));
    // A custom floor above 1e-8 accepts.
    REQUIRE(block_is_exact_zero(M.data(), 2, 3, 0, 3, /*floor=*/1e-7));
}
