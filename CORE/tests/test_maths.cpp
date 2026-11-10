#include "catch2/catch.hpp"
#include <tadah/core/core_types.h>
#include <tadah/core/maths.h>
#include <iomanip>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using namespace tadah::core;

TEST_CASE("aed_type Constructors") {
    SECTION("Default constructor") {
        aed_type vec;
        REQUIRE(vec.size() == 0);
        REQUIRE(vec.data() == nullptr);
    }

    SECTION("Constructor with size and stride") {
        size_t size = 5;
        size_t stride = 2;
        aed_type vec(size, stride);
        REQUIRE(vec.size() == size);
        REQUIRE(vec.data() != nullptr);
    }
}

TEST_CASE("aed_type Constructors with External Arrays") {
    SECTION("Non-allocating constructor with double*") {
        size_t size = 5;
        double data[] = {1, 2, 3, 4, 5};
        aed_type vec(data, size);

        REQUIRE(vec.size() == size);
        for (size_t i = 0; i < size; ++i) {
            REQUIRE(vec[i] == Approx(data[i])); // CHANGED
        }
    }

    SECTION("Allocating constructor with const double*") {
        size_t size = 5;
        const double data[] = {1, 2, 3, 4, 5};
        aed_type vec(data, size);

        REQUIRE(vec.size() == size);
        for (size_t i = 0; i < size; ++i) {
            REQUIRE(vec[i] == Approx(data[i])); // CHANGED
        }
    }
}

TEST_CASE("aed_type Constructors with 'new' Allocated Arrays") {
    SECTION("Non-allocating constructor with double*") {
        size_t size = 5;
        double* data = new double[size]{1, 2, 3, 4, 5};
        aed_type vec(data, size);

        REQUIRE(vec.size() == size);
        for (size_t i = 0; i < size; ++i) {
            REQUIRE(vec[i] == Approx(data[i])); // CHANGED
        }

        delete[] data; // Clean up
    }

    SECTION("Allocating constructor with const double*") {
        size_t size = 5;
        const double* data = new double[size]{1, 2, 3, 4, 5};
        aed_type vec(data, size);

        REQUIRE(vec.size() == size);
        for (size_t i = 0; i < size; ++i) {
            REQUIRE(vec[i] == Approx(data[i])); // CHANGED
        }

        delete[] data; // Clean up
    }
}

TEST_CASE("Testing copy constructors of aed_type") {
    REQUIRE_NOTHROW(aed_type());
    REQUIRE_NOTHROW(aed_type(5));
    REQUIRE_NOTHROW(aed_type(5,2));
    REQUIRE_NOTHROW(aed_type(6,2));

    // Create an initial object
    aed_type original(3);
    aed_type original2(3);
    original[0] = 0.100000000000001; 
    original[1] = 10; 
    original[2] = 0.0001;

    SECTION("Const copy constructor") {
        const aed_type const_original = original;
        aed_type const_copy = const_original;
        REQUIRE(const_copy != original2);
    }

    SECTION("Non-const copy constructor") {
        aed_type non_const_copy = original;
        REQUIRE(non_const_copy == original);  // uses operator==
        REQUIRE(non_const_copy != original2);
    }
}

TEST_CASE("aed_type Assignment Operator") {
    size_t size = 5;
    aed_type original(size);
    for (size_t i = 0; i < size; ++i) {
        original[i] = static_cast<double>(i);
    }

    aed_type copy;
    copy = original;
    REQUIRE(copy == original); // uses operator==
}

TEST_CASE("aed_type Element Access") {
    size_t size = 3;
    aed_type vec(size);
    vec[0] = 1.0;
    vec[1] = 2.0;
    vec[2] = 3.0;

    REQUIRE(vec[0] == Approx(1.0)); // CHANGED
    REQUIRE(vec[1] == Approx(2.0)); // CHANGED
    REQUIRE(vec[2] == Approx(3.0)); // CHANGED
}

TEST_CASE("aed_type Operations") {
    size_t size = 3;
    aed_type vec(size);
    vec[0] = 1.0;
    vec[1] = 2.0;
    vec[2] = 3.0;

    vec += 1.0;
    REQUIRE(vec[0] == Approx(2.0)); // CHANGED
    REQUIRE(vec[1] == Approx(3.0)); // CHANGED
    REQUIRE(vec[2] == Approx(4.0)); // CHANGED

    vec -= 1.0;
    REQUIRE(vec[0] == Approx(1.0)); // CHANGED
    REQUIRE(vec[1] == Approx(2.0)); // CHANGED
    REQUIRE(vec[2] == Approx(3.0)); // CHANGED

    vec *= 2.0;
    REQUIRE(vec[0] == Approx(2.0)); // CHANGED
    REQUIRE(vec[1] == Approx(4.0)); // CHANGED
    REQUIRE(vec[2] == Approx(6.0)); // CHANGED
}

TEST_CASE("aed_type Copy Method with Range") {
    size_t size = 5;
    aed_type vec(size);
    for (size_t i = 0; i < size; ++i) {
        vec[i] = static_cast<double>(i + 1);
    }

    aed_type subVec = vec.copy(3, 1);
    REQUIRE(subVec.size() == 3);
    REQUIRE(subVec[0] == Approx(2.0)); // CHANGED
    REQUIRE(subVec[1] == Approx(3.0)); // CHANGED
    REQUIRE(subVec[2] == Approx(4.0)); // CHANGED
}

TEST_CASE("aed_type Random Initialization") {
    size_t size = 6;
    SECTION("Default bounds") {
        aed_type vec(size);
        vec.random();

        // Check values are within a specified range
        for (size_t i = 0; i < size; ++i) {
            REQUIRE(vec[i] >= -1e4);
            REQUIRE(vec[i] <= 1e4);
        }
    }

    SECTION("Custom bounds") {
        int seed = 501;
        double low = 1e-5;
        double high = 1e-4;
        aed_type vec2(size);
        vec2.random(seed, low, high);

        // Check values are within a specified range
        for (size_t i = 0; i < size; ++i) {
            REQUIRE(vec2[i] >= low);
            REQUIRE(vec2[i] <= high);
        }
    }
}

TEST_CASE("Matrix_T basic functionality", "[matrix][new]") {
    SECTION("Default constructor") {
        Matrix_T<> mat;
        REQUIRE(mat.rows() == 0);
        REQUIRE(mat.cols() == 0);
        REQUIRE(mat.size() == 0);
    }

    SECTION("Constructor with rows and cols") {
        Matrix_T<> mat(3, 4);
        REQUIRE(mat.rows() == 3);
        REQUIRE(mat.cols() == 4);
        REQUIRE(mat.size() == 12); 
    }

    SECTION("Resize") {
        Matrix_T<> mat(2, 2);
        REQUIRE(mat.size() == 4);

        mat.resize(3, 5);
        REQUIRE(mat.rows() == 3);
        REQUIRE(mat.cols() == 5);
        REQUIRE(mat.size() == 15);
    }
}

TEST_CASE("Matrix_T element access and set_zero()", "[matrix][new]") {
    Matrix_T<> mat(2, 3);
    REQUIRE(mat.size() == 6);

    // Fill with some values
    for (size_t i = 0; i < mat.size(); ++i) {
        mat[i] = static_cast<double>(i + 1);
    }
    // Check them
    for (size_t i = 0; i < mat.size(); ++i) {
        REQUIRE(mat[i] == Approx(i + 1.0));
    }

    SECTION("Row-col operator()") {
        REQUIRE(mat(0, 0) == Approx(1.0));
        REQUIRE(mat(0, 1) == Approx(3.0)); // row-major or col-major? (We see col-major in code)
        // Actually, based on the code: m_[i + rows_*j] => col-major
        // So (0,1) => index = 0 + 2*1 = 2 => mat[2] => 3.0
        REQUIRE(mat(0, 1) == Approx(3.0));
        REQUIRE(mat(1, 2) == Approx(6.0));
    }

    SECTION("set_zero()") {
        mat.set_zero();
        for (size_t i = 0; i < mat.size(); ++i) {
            REQUIRE(mat[i] == Approx(0.0));
        }
    }
}

TEST_CASE("Matrix_T swap_columns()", "[matrix][new]") {
    Matrix_T<> mat(3, 3);
    // Populate matrix in col-major order:
    // col 0: 1,2,3
    // col 1: 4,5,6
    // col 2: 7,8,9
    // So underlying container: [1,2,3, 4,5,6, 7,8,9]
    for (size_t c = 0; c < 3; ++c) {
        for (size_t r = 0; r < 3; ++r) {
            mat(r, c) = 1.0 + r + c * 3;
        }
    }
    // Check initial order
    REQUIRE(mat(0, 0) == Approx(1.0));
    REQUIRE(mat(1, 0) == Approx(2.0));
    REQUIRE(mat(2, 0) == Approx(3.0));
    REQUIRE(mat(0, 2) == Approx(7.0));
    REQUIRE(mat(1, 2) == Approx(8.0));
    REQUIRE(mat(2, 2) == Approx(9.0));

    SECTION("Swapping col 0 and col 2") {
        mat.swap_columns(0, 2);
        // Now col 0 <-> col 2
        REQUIRE(mat(0, 0) == Approx(7.0));
        REQUIRE(mat(1, 0) == Approx(8.0));
        REQUIRE(mat(2, 0) == Approx(9.0));
        REQUIRE(mat(0, 2) == Approx(1.0));
        REQUIRE(mat(1, 2) == Approx(2.0));
        REQUIRE(mat(2, 2) == Approx(3.0));
    }
}

TEST_CASE("Matrix_T operator* (matrix-vector)", "[matrix][new]") {
    // 3x2 matrix times aed_type(2) => aed_type(3)
    Matrix_T<> mat(3, 2);
    mat.set_zero();

    // Fill col-major:
    // col0 -> [1, 2, 3]
    // col1 -> [4, 5, 6]
    // => stored in mat.m_ as [1,2,3,4,5,6]
    for (size_t c = 0; c < 2; ++c) {
        for (size_t r = 0; r < 3; ++r) {
            mat(r, c) = (double)(r + 1 + c * 3);
        }
    }

    aed_type vec(2); // Just aed_type with size=2
    vec[0] = 10.0;
    vec[1] = 100.0;

    aed_type result = mat * vec;

    REQUIRE(result.size() == 3);
    // col0 contributes: [1*10, 2*10, 3*10] => [10, 20, 30]
    // col1 contributes: [4*100, 5*100, 6*100] => [400, 500, 600]
    // sum => [410, 520, 630]
    REQUIRE(result[0] == Approx(410.0));
    REQUIRE(result[1] == Approx(520.0));
    REQUIRE(result[2] == Approx(630.0));
}

TEST_CASE("Matrix_T row() and col()", "[matrix][new]") {
    Matrix_T<> mat(2, 3);
    // Fill col-major => col0-> [1,2], col1-> [3,4], col2-> [5,6]
    for (size_t c = 0; c < 3; ++c) {
        for (size_t r = 0; r < 2; ++r) {
            mat(r, c) = (double)(1 + r + c * 2);
        }
    }

    SECTION("row(r) read-only usage") {
        aed_type row0 = mat.row(0);
        REQUIRE(row0.size() == mat.cols());
        // row(0) => [mat(0,0), mat(0,1), mat(0,2)]
        // => [1, 3, 5]
        REQUIRE(row0[0] == Approx(1.0));
        REQUIRE(row0[1] == Approx(3.0));
        REQUIRE(row0[2] == Approx(5.0));
    }

    SECTION("col(c) read-only usage") {
        aed_type col1 = mat.col(1);
        REQUIRE(col1.size() == 2);
        // col(1) => [mat(0,1), mat(1,1)] => [3,4]
        REQUIRE(col1[0] == Approx(3.0));
        REQUIRE(col1[1] == Approx(4.0));
    }
}

TEST_CASE("Matrix_T transpose free function", "[matrix][new]") {
    // Original is 2 x 3
    Matrix_T<> mat(2,3);
    // col0-> [1,2], col1-> [3,4], col2-> [5,6]
    for (size_t c = 0; c < 3; ++c) {
        for (size_t r = 0; r < 2; ++r) {
            mat(r,c) = (double)(1 + r + c*2);
        }
    }
    // Transposed -> 3 x 2
    Matrix_T<> matT = transpose(mat);
    REQUIRE(matT.rows() == 3);
    REQUIRE(matT.cols() == 2);
    // Check a few values
    // mat(0,0) => 1 => should appear as matT(0,0)
    REQUIRE(matT(0,0) == Approx(mat(0,0)));
    // mat(1,2) => 6 => should appear as matT(2,1)
    REQUIRE(matT(2,1) == Approx(mat(1,2)));
}

TEST_CASE("Matrix3d_T basic usage", "[matrix3d][new]") {
    Matrix3d m;
    // By default, m.rows() = 3, m.cols() = 3
    REQUIRE(m.rows() == 3);
    REQUIRE(m.cols() == 3);
    REQUIRE(m.size() == 9);

    SECTION("Check norm() of zero-initialized matrix") {
        REQUIRE(m.norm() == Approx(0.0));
    }

    SECTION("Set and get elements") {
        // col-major => index = i + 3*j
        // Fill in [1..9]
        for (size_t j = 0; j < 3; ++j) {
            for (size_t i = 0; i < 3; ++i) {
                m(i, j) = (double)(1 + i + j * 3);
            }
        }
        REQUIRE(m(0,0) == Approx(1.0));
        REQUIRE(m(2,2) == Approx(9.0));
        REQUIRE(m.norm() == Approx(std::sqrt(1*1 + 2*2 + 3*3 + 4*4 + 5*5 + 6*6 + 7*7 + 8*8 + 9*9)));
    }
}

TEST_CASE("Matrix3d_T Identity()", "[matrix3d][new]") {
    auto I = Matrix3d::Identity();
    REQUIRE(I.rows() == 3);
    REQUIRE(I.cols() == 3);
    // Diagonal = 1, others = 0
    for (size_t j = 0; j < 3; ++j) {
        for (size_t i = 0; i < 3; ++i) {
            double expected = (i == j) ? 1.0 : 0.0;
            REQUIRE(I(i,j) == Approx(expected));
        }
    }
    REQUIRE(I.norm() == Approx(std::sqrt(3.0))); // 3 diagonal ones
}

TEST_CASE("Matrix3d_T transpose() method", "[matrix3d][new]") {
    Matrix3d m;
    // Fill it: col0-> [1,2,3], col1->[4,5,6], col2->[7,8,9]
    for (size_t j = 0; j < 3; ++j) {
        for (size_t i = 0; i < 3; ++i) {
            m(i, j) = (double)(1 + i + j * 3);
        }
    }
    Matrix3d mt = m.transpose();
    for (size_t i = 0; i < 3; ++i) {
        for (size_t j = 0; j < 3; ++j) {
            REQUIRE(mt(i,j) == Approx(m(j,i)));
        }
    }
}

TEST_CASE("Matrix3d_T inverse()", "[matrix3d][new]") {
    SECTION("Inverse of Identity should be Identity") {
        auto I = Matrix3d::Identity();
        auto invI = I.inverse();
        for (size_t i = 0; i < 3; ++i) {
            for (size_t j = 0; j < 3; ++j) {
                REQUIRE(I(i,j) == Approx(invI(i,j)));
            }
        }
    }

    SECTION("Inverse of a known matrix") {
        // Let A = [1 2 3
        //          0 1 4
        //          5 6 0]
        // A^-1 computed manually or the numeric result checked
        Matrix3d A;
        A(0,0) = 1; A(1,0) = 0; A(2,0) = 5;
        A(0,1) = 2; A(1,1) = 1; A(2,1) = 6;
        A(0,2) = 3; A(1,2) = 4; A(2,2) = 0;
        Matrix3d invA = A.inverse();

        // We can check that invA * A = Identity, within a margin
        [[maybe_unused]] auto checkMat = invA * A.row(0); // but note the operator*(const Vec3d&)
        // or do matrix multiply using some known function. 
        // A simpler approach: a quick identity check entry by entry:
        Matrix3d product;
        for (size_t i = 0; i < 3; ++i) {
            for (size_t j = 0; j < 3; ++j) {
                double sum = 0.0;
                for (size_t k = 0; k < 3; ++k) {
                    sum += invA(i,k) * A(k,j);
                }
                product(i,j) = sum;
            }
        }
        // product should be close to Identity
        for (size_t i = 0; i < 3; ++i) {
            for (size_t j = 0; j < 3; ++j) {
                double expected = (i == j)? 1.0 : 0.0;
                REQUIRE(product(i,j) == Approx(expected).margin(1e-12));
            }
        }
    }

    // Pins: the inverse of a singular matrix (rows linearly dependent,
    // determinant 0) is refused with std::domain_error.
    // Before the fix: it did not throw (this section required that) and
    // returned 1/0 times the cofactors, entries that are not numbers.
    SECTION("Singular matrix (zero determinant)") {
        Matrix3d S;
        // Make rows linearly dependent
        // col-major => row 0: [1,2,3], row1: [2,4,6], row2: [3,6,9]
        // But we fill in code as needed:
        S(0,0) = 1; S(1,0) = 2; S(2,0) = 3;
        S(0,1) = 2; S(1,1) = 4; S(2,1) = 6;
        S(0,2) = 3; S(1,2) = 6; S(2,2) = 9;

        // This matrix is singular -> determinant = 0
        REQUIRE_THROWS_AS(S.inverse(), std::domain_error);
    }
}

TEST_CASE("Matrix3d_T operator*(Vec3d)", "[matrix3d][new]") {
    Matrix3d M;
    // Fill col-major => col0-> [1,2,3], col1-> [4,5,6], col2-> [7,8,9]
    for (size_t j = 0; j < 3; ++j) {
        for (size_t i = 0; i < 3; ++i) {
            M(i, j) = (double)(1 + i + j*3);
        }
    }
    // Vec3d = (10, 100, 1000)
    Vec3d v(10, 100, 1000);

    Vec3d res = M * v;
    // By columns:
    // col0 => multiplies v.x => [1*10, 2*10, 3*10] => [10,20,30]
    // col1 => multiplies v.y => [4*100,5*100,6*100] => [400,500,600]
    // col2 => multiplies v.z => [7*1000,8*1000,9*1000] => [7000,8000,9000]
    // sum => [7410, 8520, 9630]
    REQUIRE(res[0] == Approx(7410.0));
    REQUIRE(res[1] == Approx(8520.0));
    REQUIRE(res[2] == Approx(9630.0));
}

TEST_CASE("Mat6R3C row()", "[mat6r3c][new]") {
    Mat6R3C mat;
    // Fill in row-major for clarity, but we access by (i,j):
    // (i, j) => i * 3 + j
    // we have i in [0..5], j in [0..2]
    double val = 1.0;
    for (size_t i = 0; i < 6; ++i) {
        for (size_t j = 0; j < 3; ++j) {
            mat(i, j) = val++;
        }
    }
    // row(0) => (1,2,3)
    // row(5) => (16,17,18)
    SECTION("Top row") {
        Vec3d r0 = mat.row(0);
        REQUIRE(r0[0] == Approx(1.0));
        REQUIRE(r0[1] == Approx(2.0));
        REQUIRE(r0[2] == Approx(3.0));
    }
    SECTION("Bottom row") {
        Vec3d r5 = mat.row(5);
        REQUIRE(r5[0] == Approx(16.0));
        REQUIRE(r5[1] == Approx(17.0));
        REQUIRE(r5[2] == Approx(18.0));
    }
}

namespace {
/* True when `lhs += rhs` compiles for an lhs of type T. */
template <typename T, typename = void>
struct plus_assignable : std::false_type {};
template <typename T>
struct plus_assignable<T, std::void_t<decltype(std::declval<T>() +=
                                               std::declval<const Vec3d&>())>>
    : std::true_type {};
}  // namespace

TEST_CASE("Mat6R3C row() is a read-only copy; set_row() writes a row",
          "[mat6r3c]") {
    /* row() returns a copy of the row. When the copy was a plain Vec3d,
     * `delM.row(n) = v` compiled and assigned to the temporary, leaving delM
     * unchanged: the MLIP DIMER descriptors were computed from a zero
     * matrix that way. The copy is const, so neither the row nor one of its
     * elements can be assigned to; set_row() is the way to write a row. */
    using Row = decltype(std::declval<Mat6R3C&>().row(0));
    STATIC_REQUIRE_FALSE(std::is_assignable<Row, const Vec3d&>::value);
    STATIC_REQUIRE_FALSE(std::is_assignable<Row, Vec3d&&>::value);
    STATIC_REQUIRE_FALSE(plus_assignable<Row>::value);
    STATIC_REQUIRE_FALSE(
        std::is_assignable<decltype(std::declval<Mat6R3C&>().row(0)[0]),
                           double>::value);
    /* Reading a row is unaffected. */
    STATIC_REQUIRE(std::is_constructible<Vec3d, Row>::value);

    Mat6R3C m;
    m.set_row(4, Vec3d(1.5, -2.0, 3.25));
    for (size_t i = 0; i < 6; ++i) {
        for (size_t j = 0; j < 3; ++j) {
            INFO("element (" << i << ", " << j << ")");
            const double want =
                i != 4 ? 0.0 : (j == 0 ? 1.5 : (j == 1 ? -2.0 : 3.25));
            CHECK(m(i, j) == want);
        }
    }
    const Vec3d r4 = m.row(4);
    CHECK(r4[0] == 1.5);
    CHECK(r4[1] == -2.0);
    CHECK(r4[2] == 3.25);
    CHECK(m.row(4) * m.row(4) == Approx(1.5 * 1.5 + 2.0 * 2.0 + 3.25 * 3.25));

    m.set_row(4, -m.row(4));
    CHECK(m(4, 0) == -1.5);
    CHECK(m(4, 1) == 2.0);
    CHECK(m(4, 2) == -3.25);
}

TEST_CASE("Matrix3d row() and col() are read-only copies", "[matrix3d]") {
    /* Same hazard as Mat6R3C::row(): a cell vector assigned through row()
     * or col() would be lost without a word. */
    using Row = decltype(std::declval<Matrix3d&>().row(0));
    using Col = decltype(std::declval<Matrix3d&>().col(0));
    STATIC_REQUIRE_FALSE(std::is_assignable<Row, const Vec3d&>::value);
    STATIC_REQUIRE_FALSE(std::is_assignable<Col, const Vec3d&>::value);
    STATIC_REQUIRE_FALSE(plus_assignable<Row>::value);
    STATIC_REQUIRE_FALSE(plus_assignable<Col>::value);
    STATIC_REQUIRE_FALSE(
        std::is_assignable<decltype(std::declval<Matrix3d&>().row(0)[0]),
                           double>::value);

    Matrix3d m;
    m.load(1, 2, 3, 4, 5, 6, 7, 8, 9);
    const Vec3d r = m.row(1);
    const Vec3d c = m.col(1);
    CHECK(r[0] == m(1, 0));
    CHECK(r[1] == m(1, 1));
    CHECK(r[2] == m(1, 2));
    CHECK(c[0] == m(0, 1));
    CHECK(c[1] == m(1, 1));
    CHECK(c[2] == m(2, 1));
}

namespace {
/* True when `lhs -= rhs` compiles for a Vec3d lhs and an rhs of type T. */
template <typename T, typename = void>
struct vec3d_minus_assignable : std::false_type {};
template <typename T>
struct vec3d_minus_assignable<T, std::void_t<decltype(std::declval<Vec3d&>() -=
                                                      std::declval<T>())>>
    : std::true_type {};
/* True when `lhs += rhs` compiles for a Vec3d lhs and an rhs of type T. */
template <typename T, typename = void>
struct vec3d_plus_assignable : std::false_type {};
template <typename T>
struct vec3d_plus_assignable<T, std::void_t<decltype(std::declval<Vec3d&>() +=
                                                     std::declval<T>())>>
    : std::true_type {};
/* True when `s * m` compiles for a Matrix3d m and an s of type T. */
template <typename T, typename = void>
struct left_scalable : std::false_type {};
template <typename T>
struct left_scalable<T, std::void_t<decltype(std::declval<T>() *
                                             std::declval<const Matrix3d&>())>>
    : std::true_type {};
}  // namespace

TEST_CASE("Matrix3d load() reads row by row; storage is column-major",
          "[matrix3d][cell_convention]") {
    /* A cell is loaded as a0x a0y a0z  a1x a1y a1z  a2x a2y a2z, so row i is
     * the lattice vector a_i. The storage behind operator[] is column-major. */
    Matrix3d m;
    m.load(1, 2, 3, 4, 5, 6, 7, 8, 9);
    for (size_t i = 0; i < 3; ++i)
        for (size_t j = 0; j < 3; ++j) {
            INFO("element (" << i << ", " << j << ")");
            CHECK(m(i, j) == double(3 * i + j + 1));
            CHECK(m[i + 3 * j] == m(i, j));
        }
    CHECK(m.row(1)[0] == 4);
    CHECK(m.row(1)[1] == 5);
    CHECK(m.row(1)[2] == 6);
    CHECK(m.col(1)[0] == 2);
    CHECK(m.col(1)[1] == 5);
    CHECK(m.col(1)[2] == 8);
    /* Matrix3d * Vec3d is the product with a column vector:
     * cell^T * f = f0 a0 + f1 a1 + f2 a2. */
    const Vec3d f(0.5, 0.25, 1.0);
    const Vec3d r = m.transpose() * f;
    CHECK(r[0] == Approx(0.5 * 1 + 0.25 * 4 + 1.0 * 7));
    CHECK(r[1] == Approx(0.5 * 2 + 0.25 * 5 + 1.0 * 8));
    CHECK(r[2] == Approx(0.5 * 3 + 0.25 * 6 + 1.0 * 9));
}

TEST_CASE("A Matrix3d does not convert to a Vec3d", "[matrix3d][vec3d][cell_convention]") {
    /* Vec3d's converting constructor and assignment take any indexable
     * type, so `Vec3d v = st.cell;` once compiled and gave v = storage
     * elements 0..2, i.e. column 0 = (a0x, a1x, a2x) -- not a lattice
     * vector. A row or column is read with row() or col(). */
    CHECK_FALSE(std::is_constructible<Vec3d, Matrix3d&>::value);
    CHECK_FALSE(std::is_constructible<Vec3d, const Matrix3d&>::value);
    CHECK_FALSE(std::is_assignable<Vec3d&, const Matrix3d&>::value);
    CHECK_FALSE(std::is_assignable<Vec3d&, Matrix3d&>::value);
    CHECK(plus_assignable<Vec3d&>::value);  // Vec3d += Vec3d stays
    CHECK_FALSE(vec3d_minus_assignable<const Matrix3d&>::value);
    CHECK_FALSE(vec3d_minus_assignable<Matrix&>::value);
    CHECK_FALSE(vec3d_plus_assignable<const Matrix3d&>::value);
    CHECK_FALSE(vec3d_plus_assignable<Matrix&>::value);
    /* Vectors keep converting. */
    CHECK(std::is_constructible<Vec3d, aed_type&>::value);
    CHECK(std::is_constructible<Vec3d, std::vector<double>&>::value);
    CHECK(vec3d_minus_assignable<const Vec3d&>::value);
    CHECK(vec3d_plus_assignable<const Vec3d&>::value);
}

TEST_CASE("Matrix3d scalar operators take arithmetic scalars only", "[matrix3d]") {
    /* The friend `s * m` and Vec3d's `v * s` accepted any type, so `A * B`
     * of two matrices, or `v * A`, picked them and failed inside their
     * bodies with an unrelated error. */
    CHECK(left_scalable<double>::value);
    CHECK(left_scalable<int>::value);
    CHECK_FALSE(left_scalable<const Vec3d&>::value);
    /* `A * B` of two matrices compiles since the matrix product exists; it
     * is that product, not the scalar friend: see "Matrix3d * Matrix3d is
     * the row-by-column product". */

    Matrix3d m;
    m.load(1, 2, 3, 4, 5, 6, 7, 8, 9);
    const Matrix3d m2 = 2.0 * m;
    const Matrix3d mh = m / 2;
    Matrix3d m3 = m;
    m3 *= 3;
    m3 /= 1.5;
    for (size_t i = 0; i < 9; ++i) {
        CHECK(m2[i] == 2 * m[i]);
        CHECK(mh[i] == 0.5 * m[i]);
        CHECK(m3[i] == Approx(2 * m[i]));
    }
}

namespace {
/* True when `m += s` / `m -= s` compiles for an lvalue m of type M and an s
 * of type S. */
template <typename M, typename S, typename = void>
struct has_plus_assign : std::false_type {};
template <typename M, typename S>
struct has_plus_assign<M, S, std::void_t<decltype(std::declval<M&>() +=
                                                  std::declval<S>())>>
    : std::true_type {};
template <typename M, typename S, typename = void>
struct has_minus_assign : std::false_type {};
template <typename M, typename S>
struct has_minus_assign<M, S, std::void_t<decltype(std::declval<M&>() -=
                                                   std::declval<S>())>>
    : std::true_type {};
/* True when `a * b` compiles for two const M. */
template <typename M, typename = void>
struct has_matrix_product : std::false_type {};
template <typename M>
struct has_matrix_product<M, std::void_t<decltype(std::declval<const M&>() *
                                                  std::declval<const M&>())>>
    : std::true_type {};

/* The value checks of the product. A function template, so that `A * B` is
 * only compiled where the product exists (if constexpr discards it today and
 * the case fails at run time instead of failing to build). */
template <typename M>
void check_matrix_product() {
    if constexpr (has_matrix_product<M>::value) {
        CHECK(std::is_same<std::decay_t<decltype(std::declval<const M&>() *
                                                 std::declval<const M&>())>,
                           M>::value);
        M A;
        A.load(1.0, 2.0, 3.0,
               0.0, -1.0, 4.0,
               5.0, 0.5, -2.0);
        M B;
        B.load(2.0, -1.0, 0.0,
               1.0, 3.0, -2.0,
               0.5, 4.0, 1.0);
        /* (A B)_ij = sum_k A_ik B_kj, by hand. All entries are dyadic
         * rationals, so the products and sums are exact. A^T B, A B^T and
         * B A each differ from A B in at least the (0, 0) entry. */
        const double AB[3][3] = {{5.5, 17.0, -1.0},
                                 {1.0, 13.0, 6.0},
                                 {9.5, -11.5, -3.0}};
        const double BA[3][3] = {{2.0, 5.0, 2.0},
                                 {-9.0, -2.0, 19.0},
                                 {5.5, -2.5, 15.5}};
        const M C = A * B;
        const M D = B * A;
        const M AI = A * M::Identity();
        const M IA = M::Identity() * A;
        for (size_t i = 0; i < 3; ++i)
            for (size_t j = 0; j < 3; ++j) {
                INFO("element (" << i << ", " << j << ")");
                CHECK(C(i, j) == AB[i][j]);
                CHECK(D(i, j) == BA[i][j]);
                CHECK(AI(i, j) == A(i, j));
                CHECK(IA(i, j) == A(i, j));
            }
        /* The operands are unchanged. */
        CHECK(A(2, 1) == 0.5);
        CHECK(B(1, 2) == -2.0);
    } else {
        FAIL("Matrix3d * Matrix3d (the matrix product) does not exist");
    }
}
}  // namespace

// Pins the CORE Matrix3d arithmetic of the stress-convention work: the
// scalar compound assignments `m += s` and `m -= s` (double, int and every
// other arithmetic type) do not exist, while `Matrix3d += Matrix3d` and
// `-= Matrix3d` keep adding and subtracting element by element. A scalar
// added to a stress or cell matrix went into all nine entries, off-diagonals
// included (the CASTEP .md reader's `stress -= N kB T / V` removed the
// kinetic term from the shear components too), which is never the intent.
// Before the fix: Matrix3d had operator+=/-=(double) and (int), which added
// the scalar to all nine entries, so has_plus_assign<Matrix3d, double> and
// <Matrix3d, int> were true (and the unconstrained template operator+= made
// the float, long and size_t forms compile as well).
TEST_CASE("Matrix3d has no scalar += and -=; Matrix3d += Matrix3d stays",
          "[matrix3d][stress_convention]") {
    CHECK_FALSE(has_plus_assign<Matrix3d, double>::value);
    CHECK_FALSE(has_minus_assign<Matrix3d, double>::value);
    CHECK_FALSE(has_plus_assign<Matrix3d, int>::value);
    CHECK_FALSE(has_minus_assign<Matrix3d, int>::value);
    CHECK_FALSE(has_plus_assign<Matrix3d, float>::value);
    CHECK_FALSE(has_minus_assign<Matrix3d, float>::value);
    CHECK_FALSE(has_plus_assign<Matrix3d, long>::value);
    CHECK_FALSE(has_minus_assign<Matrix3d, long>::value);
    CHECK_FALSE(has_plus_assign<Matrix3d, std::size_t>::value);
    CHECK_FALSE(has_minus_assign<Matrix3d, std::size_t>::value);

    CHECK(has_plus_assign<Matrix3d, const Matrix3d&>::value);
    CHECK(has_minus_assign<Matrix3d, const Matrix3d&>::value);

    Matrix3d a;
    a.load(1.0, 2.0, 3.0,
           0.0, -1.0, 4.0,
           5.0, 0.5, -2.0);
    Matrix3d b;
    b.load(2.0, -1.0, 0.0,
           1.0, 3.0, -2.0,
           0.5, 4.0, 1.0);
    const double sum[3][3] = {{3.0, 1.0, 3.0},
                              {1.0, 2.0, 2.0},
                              {5.5, 4.5, -1.0}};
    const double diff[3][3] = {{-1.0, 3.0, 3.0},
                               {-1.0, -4.0, 6.0},
                               {4.5, -3.5, -3.0}};
    Matrix3d s = a;
    s += b;
    Matrix3d d = a;
    d -= b;
    for (size_t i = 0; i < 3; ++i)
        for (size_t j = 0; j < 3; ++j) {
            INFO("element (" << i << ", " << j << ")");
            CHECK(s(i, j) == sum[i][j]);
            CHECK(d(i, j) == diff[i][j]);
        }
}

// Pins Matrix3d * Matrix3d: the product exists, is a Matrix3d, and is the
// row-by-column product (A B)_ij = sum_k A_ik B_kj for two non-symmetric
// matrices (so that B A, A^T B or A B^T cannot pass), with the identity as
// its unit. It replaces MLIP's local matMul.
// Before the fix: `A * B` of two matrices did not compile (no operator*
// takes two Matrix3d), so has_matrix_product<Matrix3d> was false and the
// case failed with "Matrix3d * Matrix3d (the matrix product) does not
// exist".
TEST_CASE("Matrix3d * Matrix3d is the row-by-column product",
          "[matrix3d][stress_convention]") {
    CHECK(has_matrix_product<Matrix3d>::value);
    check_matrix_product<Matrix3d>();
}

/* Pins: Matrix_Base::load takes exactly rows() x cols() values: a Matrix3d
 * given 8, 10 or 11 values throws std::invalid_argument naming the count
 * ("8 values", "a 3 x 3 matrix takes 9") and keeps every element; 9 values
 * load as before. (12 or more values, which wrote past the array of the
 * Matrix3d, are refused by the same check; a call with them is not run, as
 * before the fix it writes out of bounds.)
 * Before the fix: no count was checked: 8 values loaded the first eight
 * elements and left element (2,2) as it was, and the 10th and 11th values
 * overwrote elements (0,1) and (0,2) (load's index i past 8 wrapped back
 * into the array), all without an error. */
TEST_CASE("Matrix3d load() refuses a number of values other than 9",
          "[matrix3d]") {
    auto unchanged = [](const Matrix3d &m) {
        for (size_t i = 0; i < 3; ++i)
            for (size_t j = 0; j < 3; ++j) {
                INFO("element (" << i << ", " << j << ")");
                CHECK(m(i, j) == -1.0);
            }
    };
    auto filled = []() {
        Matrix3d m;
        for (size_t k = 0; k < 9; ++k) m[k] = -1.0;
        return m;
    };
    {
        Matrix3d m = filled();
        CHECK_THROWS_AS(m.load(1, 2, 3, 4, 5, 6, 7, 8), std::invalid_argument);
        CHECK_THROWS_WITH(m.load(1, 2, 3, 4, 5, 6, 7, 8),
                          Catch::Contains("8 values") &&
                              Catch::Contains("a 3 x 3 matrix takes 9"));
        unchanged(m);
    }
    {
        Matrix3d m = filled();
        CHECK_THROWS_AS(m.load(1, 2, 3, 4, 5, 6, 7, 8, 9, 10),
                        std::invalid_argument);
        CHECK_THROWS_WITH(m.load(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11),
                          Catch::Contains("11 values"));
        unchanged(m);
    }
    Matrix3d m = filled();
    m.load(1, 2, 3, 4, 5, 6, 7, 8, 9);
    CHECK(m(2, 2) == 9.0);
    CHECK(m(1, 0) == 4.0);
}

/* Pins: Matrix3d::inverse throws std::domain_error, naming the
 * determinant, for a matrix whose determinant is 0 (rows 1 2 3, 4 5 6,
 * 7 8 9; the zero matrix), is not a finite number (an entry NaN or
 * infinite) or whose reciprocal is not (1e-110 on the diagonal: det 1e-330
 * is 0 in double); the inverse of an invertible matrix is as before (diag
 * 2 4 8: diag 0.5 0.25 0.125, exactly).
 * Before the fix: none threw: the inverse of a singular matrix was
 * 1/0 = inf times the cofactors (inf, -inf and NaN entries). */
TEST_CASE("Matrix3d inverse() refuses a matrix whose determinant is 0 or "
          "not finite", "[matrix3d]") {
    std::vector<std::pair<std::string, Matrix3d>> bad;
    {
        Matrix3d m;
        m.load(1, 2, 3, 4, 5, 6, 7, 8, 9);
        bad.emplace_back("rows 1 2 3, 4 5 6, 7 8 9", m);
    }
    {
        Matrix3d m;
        m.set_zero();
        bad.emplace_back("the zero matrix", m);
    }
    {
        Matrix3d m = Matrix3d::Identity();
        m(1, 2) = std::numeric_limits<double>::quiet_NaN();
        bad.emplace_back("an entry NaN", m);
    }
    {
        Matrix3d m = Matrix3d::Identity();
        m(0, 0) = std::numeric_limits<double>::infinity();
        bad.emplace_back("an entry infinite", m);
    }
    {
        Matrix3d m;
        m.set_zero();
        m(0, 0) = m(1, 1) = m(2, 2) = 1e-110;
        bad.emplace_back("diag 1e-110", m);
    }
    for (const auto &b : bad) {
        INFO(b.first);
        CHECK_THROWS_AS(b.second.inverse(), std::domain_error);
        CHECK_THROWS_WITH(b.second.inverse(), Catch::Contains("determinant"));
    }
    Matrix3d d;
    d.set_zero();
    d(0, 0) = 2.0;
    d(1, 1) = 4.0;
    d(2, 2) = 8.0;
    const Matrix3d inv = d.inverse();
    CHECK(inv(0, 0) == 0.5);
    CHECK(inv(1, 1) == 0.25);
    CHECK(inv(2, 2) == 0.125);
    CHECK(inv(0, 1) == 0.0);
}
