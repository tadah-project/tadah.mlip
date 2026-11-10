// test_svd.cpp

#include "catch2/catch.hpp"
#include <algorithm>
#include <cmath>
#include <tadah/core/maths.h>
#include <tadah/models/memory/models_workspace_manager.h>
#include <tadah/models/svd.h>

using namespace tadah::core;
using namespace tadah::models;
TEST_CASE("Testing SVD") {
  using Matrix = Matrix_T<>;
  /*using Vector = aed_type;*/

  // Test data

  SECTION("SVD of a rectangular matrix (m > n)") {
    int m = 5;
    int n = 3;
    Matrix A(m, n);
    // Initialize A with arbitrary values
    A(0, 0) = 1;
    A(0, 1) = 2;
    A(0, 2) = 3;
    A(1, 0) = 4;
    A(1, 1) = 5;
    A(1, 2) = 6;
    A(2, 0) = 7;
    A(2, 1) = 8;
    A(2, 2) = 9;
    A(3, 0) = 10;
    A(3, 1) = 11;
    A(3, 2) = 12;
    A(4, 0) = 13;
    A(4, 1) = 14;
    A(4, 2) = 15;

    int minMN = std::min(m, n);

    tadah::models::SVD svd;
    svd.compute(A);

    // For this test, we can check that the singular values are non-negative and
    // in descending order
    double *S_computed = svd.getS();
    for (int i = 0; i < minMN - 1; ++i) {
      REQUIRE(S_computed[i] >= S_computed[i + 1]);
      REQUIRE(S_computed[i] >= 0);
    }
  }

  SECTION("SVD of a rectangular matrix (m < n)") {
    int m = 3;
    int n = 5;
    Matrix A(m, n);
    // Initialize A with arbitrary values
    A(0, 0) = 1;
    A(0, 1) = 2;
    A(0, 2) = 3;
    A(0, 3) = 4;
    A(0, 4) = 5;
    A(1, 0) = 6;
    A(1, 1) = 7;
    A(1, 2) = 8;
    A(1, 3) = 9;
    A(1, 4) = 10;
    A(2, 0) = 11;
    A(2, 1) = 12;
    A(2, 2) = 13;
    A(2, 3) = 14;
    A(2, 4) = 15;

    int minMN = std::min(m, n);

    tadah::models::SVD svd;
    svd.compute(A);

    // Check that singular values are non-negative and in descending order
    double *S_computed = svd.getS();
    for (int i = 0; i < minMN - 1; ++i) {
      REQUIRE(S_computed[i] >= S_computed[i + 1]);
      REQUIRE(S_computed[i] >= 0);
    }
  }

  SECTION("SVD of a rank-deficient matrix") {
    Matrix A(4, 4);
    // Create a rank-deficient matrix (rank 2)
    A(0, 0) = 1;
    A(0, 1) = 2;
    A(0, 2) = 3;
    A(0, 3) = 4;
    A(1, 0) = 2;
    A(1, 1) = 4;
    A(1, 2) = 6;
    A(1, 3) = 8;
    A(2, 0) = 3;
    A(2, 1) = 6;
    A(2, 2) = 9;
    A(2, 3) = 12;
    A(3, 0) = 4;
    A(3, 1) = 8;
    A(3, 2) = 12;
    A(3, 3) = 16;

    tadah::models::SVD svd;
    svd.compute(A);

    // Singular values should have two zeros (or very close to zero)
    double *S_computed = svd.getS();
    int minMN = std::min(A.rows(), A.cols());

    int zero_count = 0;
    double tolerance = 1e-6;
    for (int i = 0; i < minMN; ++i) {
      if (S_computed[i] < tolerance) {
        zero_count++;
      }
    }

    REQUIRE(zero_count >=
            2); // At least two singular values should be near zero
  }

  SECTION("SVD of an identity matrix") {
    int size = 5;
    Matrix A(size, size);
    // Initialize A as an identity matrix
    for (int i = 0; i < size; ++i) {
      for (int j = 0; j < size; ++j) {
        A(i, j) = (i == j) ? 1.0 : 0.0;
      }
    }

    tadah::models::SVD svd;
    svd.compute(A);

    double *S_computed = svd.getS();

    // All singular values should be 1
    for (int i = 0; i < size; ++i) {
      REQUIRE(std::abs(S_computed[i] - 1.0) < 1e-6);
    }

    // U and VT should be identity matrices (up to sign)
    double *U = svd.getU();
    double *VT = svd.getVT();

    // Verify that U is approximately an identity matrix
    for (int i = 0; i < size; ++i) {
      for (int j = 0; j < size; ++j) {
        double expected = (i == j) ? 1.0 : 0.0;
        REQUIRE(std::abs(std::abs(U[i * size + j]) - expected) < 1e-6);
      }
    }

    // Verify that VT is approximately an identity matrix
    for (int i = 0; i < size; ++i) {
      for (int j = 0; j < size; ++j) {
        double expected = (i == j) ? 1.0 : 0.0;
        REQUIRE(std::abs(std::abs(VT[i * size + j]) - expected) < 1e-6);
      }
    }
  }

  SECTION("Access to SVDWorkspace") {
    Matrix A(3, 3);
    A(0, 0) = 1;
    A(0, 1) = 0;
    A(0, 2) = 0;
    A(1, 0) = 0;
    A(1, 1) = 2;
    A(1, 2) = 0;
    A(2, 0) = 0;
    A(2, 1) = 0;
    A(2, 2) = 3;

    tadah::models::SVD svd;
    svd.compute(A);

    tadah::models::memory::SVDWorkspace &svdWorkspace = svd.getSVDWorkspace();

    // Verify that the workspace dimensions match
    REQUIRE(svdWorkspace.m == 3);
    REQUIRE(svdWorkspace.n == 3);

    // Verify that the singular values match the expected values
    double *S_computed = svd.getS();
    REQUIRE(std::abs(S_computed[0] - 3.0) < 1e-6);
    REQUIRE(std::abs(S_computed[1] - 2.0) < 1e-6);
    REQUIRE(std::abs(S_computed[2] - 1.0) < 1e-6);
  }

  SECTION("Test with workspace manager") {
    // Create a workspace manager
    tadah::models::memory::ModelsWorkspaceManager workspaceManager;

    // Create an SVD instance with the workspace manager
    tadah::models::SVD svd(workspaceManager);

    Matrix A(4, 4);
    // Initialize A with arbitrary values
    for (size_t i = 0; i < A.rows(); ++i) {
      for (size_t j = 0; j < A.cols(); ++j) {
        A(i, j) = static_cast<double>(i + j + 1);
      }
    }

    svd.compute(A);

    // Perform another computation to test workspace reuse
    Matrix B(4, 4);
    for (size_t i = 0; i < B.rows(); ++i) {
      for (size_t j = 0; j < B.cols(); ++j) {
        B(i, j) = static_cast<double>((i + 1) * (j + 1));
      }
    }

    svd.compute(B);

    // If no errors occur, the test passes
    REQUIRE(true);
  }

  // Additional tests can be added to cover more cases, such as:
  // - Matrices with complex entries (if supported)
  // - Very large matrices to test performance and memory management
  // - Matrices with known singular values to validate accuracy
}
