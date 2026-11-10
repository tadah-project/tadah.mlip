#include "catch2/catch.hpp"

#include <tadah/tadah.lammps/lammps_tadah.h>

#include <cstddef>
#include <vector>

/* Struct-level coverage of the IFT-adjoint snapshot. Full pair_tadah
 * accumulation correctness is exercised end-to-end in HPO's
 * Analytical-vs-Numerical parity test (it has the LAMMPS-runner test
 * fixture). The `capture_adjoint_state` member is verified to default-init
 * to false in the AdjointSnapshot's owning LammpsTadah by inspection of
 * lammps_tadah.h (struct default member initializers); constructing a full
 * LammpsTadah here would require the Function_Base / M_MD_Base factories
 * to be registered in this binary's link line, which they currently are
 * not. */

TEST_CASE("AdjointSnapshot starts empty", "[adjoint_snapshot]") {
    tadah::AdjointSnapshot s;
    CHECK(s.n_atoms == 0);
    CHECK(s.n_desc  == 0);
    CHECK(s.dF_dW.empty());
    CHECK_FALSE(s.valid);
}

TEST_CASE("AdjointSnapshot::reset sizes + zeroes + clears valid",
          "[adjoint_snapshot]") {
    tadah::AdjointSnapshot s;
    s.dF_dW.assign(3, 7.0);    // dirty state — reset must overwrite.
    s.valid = true;

    s.reset(/*n_atoms*/ 4, /*n_desc*/ 5);
    REQUIRE(s.n_atoms == 4);
    REQUIRE(s.n_desc  == 5);
    REQUIRE(s.dF_dW.size() == 3 * 4 * 5);
    for (std::size_t i = 0; i < s.dF_dW.size(); ++i) {
        REQUIRE(s.dF_dW[i] == 0.0);
    }
    CHECK_FALSE(s.valid);
}

TEST_CASE("AdjointSnapshot row_ptr addresses match (3*atom+axis) layout",
          "[adjoint_snapshot]") {
    tadah::AdjointSnapshot s;
    s.reset(/*n_atoms*/ 3, /*n_desc*/ 4);

    // Write distinct sentinel patterns via row_ptr; read back via raw indexing
    // to confirm the row stride is (3*atom + axis) * n_desc.
    for (std::size_t a = 0; a < 3; ++a) {
        for (std::size_t k = 0; k < 3; ++k) {
            double* row = s.row_ptr(a, k);
            for (std::size_t c = 0; c < 4; ++c) {
                row[c] = static_cast<double>(100 * a + 10 * k + c);
            }
        }
    }
    for (std::size_t a = 0; a < 3; ++a) {
        for (std::size_t k = 0; k < 3; ++k) {
            for (std::size_t c = 0; c < 4; ++c) {
                const std::size_t flat = (3 * a + k) * 4 + c;
                REQUIRE(s.dF_dW[flat] ==
                        Approx(static_cast<double>(100 * a + 10 * k + c)));
            }
        }
    }
}

TEST_CASE("AdjointSnapshot const row_ptr matches mutable variant",
          "[adjoint_snapshot]") {
    tadah::AdjointSnapshot s;
    s.reset(2, 3);
    s.row_ptr(1, 2)[0] = 42.0;
    const tadah::AdjointSnapshot& cs = s;
    REQUIRE(cs.row_ptr(1, 2)[0] == Approx(42.0));
}

TEST_CASE("AdjointSnapshot::reset re-zeroes a previously populated buffer",
          "[adjoint_snapshot]") {
    tadah::AdjointSnapshot s;
    s.reset(2, 3);
    s.row_ptr(0, 0)[1] = 1.23;
    s.row_ptr(1, 2)[2] = -4.56;
    s.valid = true;

    s.reset(2, 3);
    for (std::size_t i = 0; i < s.dF_dW.size(); ++i) {
        REQUIRE(s.dF_dW[i] == 0.0);
    }
    CHECK_FALSE(s.valid);
}
