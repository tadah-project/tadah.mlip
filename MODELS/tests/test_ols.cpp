// test_ols.cpp

#include "catch2/catch.hpp"
#include <tadah/models/ols.h>
#include <tadah/models/svd.h>
#include <tadah/models/memory/models_workspace_manager.h>
#include <tadah/core/maths.h>
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <vector>

using namespace tadah::core;
using namespace tadah::models;
TEST_CASE("Testing OLS") {
    using Matrix = Matrix_T<>;
    using Vector = aed_type;

    // Helper function to compare vectors within a given tolerance
    auto vectorsAreClose = [](const Vector& v1, const Vector& v2, double tolerance) {
        REQUIRE(v1.size() == v2.size());
        for (size_t i = 0; i < v1.size(); ++i) {
            REQUIRE(std::abs(v1[i] - v2[i]) < tolerance);
        }
    };

    // Prepare test data
    SECTION("Solve with square matrix") {
        // Initialize A (column-major order)
        Matrix A(3, 3);
        A(0,0) = 1; A(1,0) = 4; A(2,0) = 7;   // First column
        A(0,1) = 2; A(1,1) = 5; A(2,1) = 8;   // Second column
        A(0,2) = 3; A(1,2) = 6; A(2,2) = 10;  // Third column

        Vector expectedWeights = {1, 2, 3};

        // Compute B = A * expectedWeights
        Vector B = A * expectedWeights;

        // Create an instance of OLS
        tadah::models::OLS ols;

        // Test different algorithms
        std::vector<tadah::models::Algorithm> algorithms = {
            tadah::models::Algorithm::GELSD,
            tadah::models::Algorithm::GELS,
            tadah::models::Algorithm::NormalEquations,
            tadah::models::Algorithm::Pseudoinverse
        };

        for (auto algo : algorithms) {
            SECTION("Solve using algorithm: " + to_string_algorithm(algo)) {
                Vector weights(3);
                double rcond = -1.0;
                std::cout << "------------" << std::endl;
                std::cout << A << std::endl;
                std::cout << "expectedWeights: " << expectedWeights << std::endl;
                ols.solve(A, B, weights, rcond, algo);
                std::cout << "weights: " << weights << std::endl;
                Vector B_computed = A * weights;
                std::cout << "B: " << B<< std::endl;
                std::cout << "B_computed: " << B_computed << std::endl;
                vectorsAreClose(weights, expectedWeights, 1e-4);
            }
        }
    }

    SECTION("Solve with overdetermined system (m > n)") {
        int m = 5;
        int n = 3;
        Matrix A(m, n);

        // Initialize A (column-major order)
        A(0,0) = 1;  A(1,0) = 4;  A(2,0) = 7;  A(3,0) = 1;  A(4,0) = 0; // First column
        A(0,1) = 2;  A(1,1) = 5;  A(2,1) = 8;  A(3,1) = 0;  A(4,1) = 1; // Second column
        A(0,2) = 3;  A(1,2) = 6;  A(2,2) = 10; A(3,2) = 0;  A(4,2) = 0; // Third column

        Matrix A_cpy = A;
        Vector expectedWeights = {1, 2, 3};

        // Compute B = A * expectedWeights
        Vector B = A * expectedWeights;
        Vector B_cpy = B;

        tadah::models::OLS ols;

        Vector weights(n);
        ols.solve(A, B, weights);

        // Compute B_computed = A * weights
        Vector B_computed = A_cpy * weights;

        // Check residual
        for (int i = 0; i < m; ++i) {
            REQUIRE(std::abs(B_cpy[i] - B_computed[i]) < 1e-3);
        }
    }

    SECTION("Solve with underdetermined system (m < n)") {
        int m = 3;
        int n = 5;
        Matrix A(m, n);

        // Initialize A (column-major order)
        A(0,0) = 1;  A(1,0) = 6;   A(2,0) = 11;  // First column
        A(0,1) = 2;  A(1,1) = 7;   A(2,1) = 12;  // Second column
        A(0,2) = 3;  A(1,2) = 8;   A(2,2) = 13;  // Third column
        A(0,3) = 4;  A(1,3) = 9;   A(2,3) = 14;  // Fourth column
        A(0,4) = 5;  A(1,4) = 10;  A(2,4) = 15;  // Fifth column

        Matrix A_cpy = A;
        Vector B = {1, 2, 3};
        Vector B_cpy = B;

        tadah::models::OLS ols;

        Vector weights(n);
        ols.solve(A, B, weights);

        // Compute B_computed = A * weights
        Vector B_computed = A_cpy * weights;

        // Check that B_computed matches B
        vectorsAreClose(B_computed, B_cpy, 1e-4);
    }

    SECTION("Solve with rank-deficient matrix") {
        Matrix A(3, 3);

        // Initialize A to be rank-deficient (second column is 2 * first column)
        A(0,0) = 1;  A(1,0) = 2;  A(2,0) = 3; // First column
        A(0,1) = 2;  A(1,1) = 4;  A(2,1) = 6; // Second column (2 * first column)
        A(0,2) = 3;  A(1,2) = 6;  A(2,2) = 9; // Third column

        Matrix A_cpy = A;
        Vector expectedWeights = {1, 0, 3};
        Vector B = A * expectedWeights;
        Vector B_cpy = B;

        tadah::models::OLS ols;

        Vector weights(3);
        ols.solve(A, B, weights);

        // Since the system is rank-deficient, the solution is not unique
        // Verify that A * weights is close to B

        Vector B_computed = A_cpy * weights;
        vectorsAreClose(B_computed, B_cpy, 1e-4);
    }

    SECTION("Testing solveWithPseudoinverse method") {
        Matrix A(3, 3);
        // Initialize A (column-major order)
        A(0,0) = 1; A(1,0) = 4; A(2,0) = 7;   // First column
        A(0,1) = 2; A(1,1) = 5; A(2,1) = 8;   // Second column
        A(0,2) = 3; A(1,2) = 6; A(2,2) = 10;  // Third column

        Vector expectedWeights = {1, 2, 3};

        // Compute B = A * expectedWeights
        Vector B = A * expectedWeights;

        tadah::models::OLS ols;
        tadah::models::SVD svd;

        // Compute SVD of A
        svd.compute(A);

        Vector weights(3);
        ols.solveWithPseudoinverse(A.rows(), A.cols(), B, weights, svd.getSVDWorkspace());

        vectorsAreClose(weights, expectedWeights, 1e-4);
    }

    SECTION("Testing solveWithPseudoinverse with regularization") {
        Matrix A(3, 3);
        // Initialize A (column-major order)
        A(0,0) = 1; A(1,0) = 4; A(2,0) = 7;   // First column
        A(0,1) = 2; A(1,1) = 5; A(2,1) = 8;   // Second column
        A(0,2) = 3; A(1,2) = 6; A(2,2) = 10;  // Third column

        Matrix A_cpy = A;
        Vector expectedWeights = {1, 2, 3};

        // Compute B = A * expectedWeights
        Vector B = A * expectedWeights;
        Vector B_cpy = B;

        tadah::models::OLS ols;
        tadah::models::SVD svd;

        // Compute SVD of A
        svd.compute(A);

        Vector weights(3);
        double rcond = -1.0;
        double lambda = 0.0001; // Regularization parameter

        ols.solveWithPseudoinverse(A.rows(), A.cols(), B, weights, svd.getSVDWorkspace(), rcond, lambda);

        // Since lambda introduces regularization, the weights may not match exactly
        // Compute residual
        Vector B_computed = A_cpy * weights;
        double residualNorm = 0.0;

        for (size_t i = 0; i < A.rows(); ++i) {
            residualNorm += std::pow(B_cpy[i] - B_computed[i], 2);
        }
        residualNorm = std::sqrt(residualNorm);

        REQUIRE(residualNorm < 1e-3);
    }

    SECTION("Testing with workspace manager") {
        Matrix A(3, 3);
        // Initialize A (column-major order)
        A(0,0) = 1; A(1,0) = 4; A(2,0) = 7;   // First column
        A(0,1) = 2; A(1,1) = 5; A(2,1) = 8;   // Second column
        A(0,2) = 3; A(1,2) = 6; A(2,2) = 10;  // Third column

        Vector expectedWeights = {1, 2, 3};
        Vector B = A * expectedWeights;

        // Create a workspace manager
        tadah::models::memory::ModelsWorkspaceManager workspaceManager;

        // Create OLS instance with the workspace manager
        tadah::models::OLS ols(workspaceManager);

        Vector weights(3);
        ols.solve(A, B, weights);

        vectorsAreClose(weights, expectedWeights, 1e-4);

        // Reuse the workspace manager for another computation
        Matrix A2(3, 3);
        A2(0,0) = 2; A2(1,0) = 5; A2(2,0) = 8;
        A2(0,1) = 3; A2(1,1) = 6; A2(2,1) = 9;
        A2(0,2) = 4; A2(1,2) = 7; A2(2,2) = 11;

        Vector expectedWeights2 = {2, 3, 4};
        Vector B2 = A2 * expectedWeights2;

        Vector weights2(3);
        ols.solve(A2, B2, weights2);

        vectorsAreClose(weights2, expectedWeights2, 1e-4);
    }

    SECTION("Testing exception handling with invalid inputs (NormalEquations)") {
        Matrix A(3, 3);
        // Initialize A (column-major order) with zeros to create a singular matrix
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                A(i,j) = 0.0;
            }
        }

        Vector B = {0, 0, 0};
        Vector weights(3);

        tadah::models::OLS ols;

        // Expect an exception due to singular A^T A matrix in NormalEquations algorithm
        REQUIRE_THROWS_AS(ols.solve(A, B, weights, -1.0, tadah::models::Algorithm::NormalEquations), std::runtime_error);
    }

    SECTION("Testing with ill-conditioned matrix") {
        Matrix A(3, 3);

        // Create an ill-conditioned matrix
        A(0,0) = 1e-10; A(1,0) = 0;      A(2,0) = 0;
        A(0,1) = 0;      A(1,1) = 1e-10; A(2,1) = 0;
        A(0,2) = 0;      A(1,2) = 0;      A(2,2) = 1e-10;

        Vector expectedWeights = {1, 2, 3};
        Vector B = A * expectedWeights;

        tadah::models::OLS ols;

        Vector weights(3);
        ols.solve(A, B, weights, -1.0, tadah::models::Algorithm::GELSD);

        vectorsAreClose(weights, expectedWeights, 1e-2);
    }

    SECTION("Testing NormalEquations algorithm with rank-deficient matrix") {
        Matrix A(2, 2);

        // Initialize A (column-major order) with a rank-deficient matrix
        A(0,0) = 1;  A(1,0) = 2;  // First column
        A(0,1) = 2;  A(1,1) = 4;  // Second column (2 * first column)

        Vector B = {5, 10}; // Compute B = A * [1; 2], but it's arbitrary here
        Vector weights(2);

        tadah::models::OLS ols;

        // Expect an exception due to singular A^T A matrix
        REQUIRE_THROWS_AS(ols.solve(A, B, weights, -1.0, tadah::models::Algorithm::NormalEquations), std::runtime_error);
    }

    SECTION("Testing solve with minimum input") {
        Matrix A(1, 1);
        A(0,0) = 2;

        Vector B = {4};
        Vector weights(1);

        tadah::models::OLS ols;

        ols.solve(A, B, weights);

        REQUIRE(weights[0] == Approx(2.0).epsilon(1e-6));
    }

    SECTION("Testing solve with large matrix") {
        int m = 100;
        int n = 50;
        Matrix A(m, n);

        // Initialize A with random values
        for (int j = 0; j < n; ++j) {
            for (int i = 0; i < m; ++i) {
                A(i,j) = static_cast<double>(std::rand()) / RAND_MAX;
            }
        }

        Matrix A_cpy = A;

        Vector expectedWeights(n);
        for (int i = 0; i < n; ++i) {
            expectedWeights[i] = static_cast<double>(std::rand()) / RAND_MAX;
        }

        Vector B = A * expectedWeights;
        Vector B_cpy = B;

        tadah::models::OLS ols;

        Vector weights(n);
        ols.solve(A, B, weights);

        // Compute residual norm
        Vector B_computed = A_cpy * weights;
        double residualNorm = 0.0;
        for (int i = 0; i < m; ++i) {
            residualNorm += std::pow(B_cpy[i] - B_computed[i], 2);
        }
        residualNorm = std::sqrt(residualNorm);

        REQUIRE(residualNorm < 1e-2);
    }
}


/* Pins: OLS::solve refuses an underdetermined system (m < n) for an
 * algorithm that does not solve one, naming the algorithms that do:
 * "The chosen algorithm (GELS) does not solve an underdetermined system (m
 * < n); GELSD and Pseudoinverse do." (std::invalid_argument).
 * Before the fix: "The chosen algorithm (GELS) is not suitable for
 * underdetermined systems (m < n). Please use GELSD or Pseudoinverse.". */
TEST_CASE("OLS refuses an underdetermined system for GELS, naming the "
          "algorithms that solve one",
          "[ols]") {
  using Matrix = Matrix_T<>;
  using Vector = aed_type;
  Matrix A(2, 3);
  for (int i = 0; i < 2; ++i)
    for (int j = 0; j < 3; ++j) A(i, j) = 1.0 + i + 2.0 * j;
  Vector B = {1, 2};
  Vector weights(3);
  tadah::models::OLS ols;
  CHECK_THROWS_AS(
      ols.solve(A, B, weights, 1e-8, tadah::models::Algorithm::GELS),
      std::invalid_argument);
  CHECK_THROWS_WITH(
      ols.solve(A, B, weights, 1e-8, tadah::models::Algorithm::GELS),
      "The chosen algorithm (GELS) does not solve an underdetermined system "
      "(m < n); GELSD and Pseudoinverse do.");
}
