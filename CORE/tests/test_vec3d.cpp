#include "catch2/catch.hpp"
#include <tadah/core/core_types.h>
#include <tadah/core/vec3d.h>
#include <iomanip>

using namespace tadah::core;

TEST_CASE( "Testing Vec3d()", "[maths]" ) {
    REQUIRE_NOTHROW(Vec3d());
    v_type v = {1,2,3};
    REQUIRE_NOTHROW(v);
    REQUIRE_NOTHROW(Vec3d(1,2,3));
}
TEST_CASE( "Testing Vec3d ostream", "[element]" ) {
    Vec3d v(1,2,3);
    std::stringstream ss;
    ss << v;
    REQUIRE_THAT(ss.str(), Catch::Matchers::Equals("1  2  3"));
}
TEST_CASE( "Testing Vec3d operator/(double, Vec3d)", "[maths]" ) {
    Vec3d v(1,2,3);
    Vec3d u = 6.0 / v;                   
    REQUIRE( u[0] == Approx(6.0/1.0) );
    REQUIRE( u[1] == Approx(6.0/2.0) );
    REQUIRE( u[2] == Approx(6.0/3.0) );
}
TEST_CASE("Testing Vec3d constructor from array", "[maths][new]") {
    SECTION("Construct from float array") {
        float arr[3] = {1.1f, 2.2f, 3.3f};
        Vec3d v(arr);
        REQUIRE(v[0] == Approx(1.1));
        REQUIRE(v[1] == Approx(2.2));
        REQUIRE(v[2] == Approx(3.3));
    }

    SECTION("Construct from double array") {
        double arr[3] = {4.5, 5.6, 6.7};
        Vec3d v(arr);
        REQUIRE(v[0] == Approx(4.5));
        REQUIRE(v[1] == Approx(5.6));
        REQUIRE(v[2] == Approx(6.7));
    }
}

TEST_CASE("Testing Vec3d isApprox()", "[maths][new]") {
    SECTION("isApprox with very small epsilon") {
        Vec3d a(1.0000000001, 2.0000000001, 3.0000000001);
        Vec3d b(1.0, 2.0, 3.0);
        REQUIRE_FALSE(a.isApprox(b, 1e-12)); 
        // Because the difference ~1e-10 is bigger than 1e-12
    }

    SECTION("isApprox with reasonable epsilon") {
        Vec3d a(1.000000001, 2.000000001, 3.000000001);
        Vec3d b(1.0, 2.0, 3.0);
        REQUIRE(a.isApprox(b, 1e-6)); 
        // Differences ~1e-9, so 1e-6 is large enough to consider them "approx"
    }
}

TEST_CASE("Testing Vec3d cross edge cases", "[maths][new]") {
    SECTION("Zero-vector cross product") {
        Vec3d zero(0, 0, 0);
        Vec3d any(1, 2, 3);
        Vec3d cross1 = zero.cross(any);
        Vec3d cross2 = any.cross(zero);
        REQUIRE(cross1 == Vec3d(0, 0, 0));
        REQUIRE(cross2 == Vec3d(0, 0, 0));
    }

    SECTION("Parallel vectors cross product") {
        Vec3d p1(1, 1, 1);
        Vec3d p2(2, 2, 2); // parallel to p1 (just scaled)
        Vec3d cr = p1.cross(p2);
        REQUIRE(cr == Vec3d(0, 0, 0));
    }

    SECTION("Anti-parallel vectors cross product") {
        Vec3d a1(1, 1, 1);
        Vec3d a2(-1, -1, -1); // anti-parallel to a1
        Vec3d cr = a1.cross(a2);
        REQUIRE(cr == Vec3d(0, 0, 0));
    }
}

TEST_CASE("Testing Vec3d norm() with edge cases", "[maths][new]") {
    SECTION("Norm of zero vector") {
        Vec3d zero(0, 0, 0);
        REQUIRE(zero.norm() == Approx(0.0));
    }

    SECTION("Norm of negative components") {
        Vec3d neg(-1, -2, -2);
        REQUIRE(neg.norm() == Approx(std::sqrt(1 + 4 + 4)));
    }
}

TEST_CASE("Testing Vec3d operator*(double) additional checks", "[maths][new]") {
    SECTION("Multiplying by zero") {
        Vec3d v(1, -2, 3);
        auto res = v * 0.0;
        REQUIRE(res[0] == Approx(0.0));
        REQUIRE(res[1] == Approx(0.0));
        REQUIRE(res[2] == Approx(0.0));
    }

    SECTION("Multiplying by negative scalar") {
        Vec3d v(1, 2, 3);
        auto res = v * -2.0;
        REQUIRE(res[0] == Approx(-2.0));
        REQUIRE(res[1] == Approx(-4.0));
        REQUIRE(res[2] == Approx(-6.0));
    }
}

TEST_CASE("Testing Vec3d operator/(double) edge cases", "[maths][new]") {
    SECTION("Dividing by negative scalar") {
        Vec3d v(2, -4, 6);
        Vec3d result = v / -2.0;
        REQUIRE(result[0] == Approx(-1.0));
        REQUIRE(result[1] == Approx(2.0));
        REQUIRE(result[2] == Approx(-3.0));
    }

    SECTION("Dividing by very small scalar (not zero)") {
        // Might highlight floating-point precision issues
        Vec3d v(1, 2, 3);
        double small = 1e-12;
        Vec3d result = v / small;
        REQUIRE(result[0] == Approx(1.0 / 1e-12));
        REQUIRE(result[1] == Approx(2.0 / 1e-12));
        REQUIRE(result[2] == Approx(3.0 / 1e-12));
    }

    // The code does not explicitly handle divide-by-zero. We can test
    // how it behaves, but typically this is undefined behavior in
    // standard C++. No exceptions are thrown by default.
    SECTION("Dividing by zero (behavioral check only)") {
        Vec3d v(1, 2, 3);
        double zeroVal = 0.0;
        // This leads to infinity or NaN in most implementations:
        // Though undefined, the test only checks that it compiles and runs:
        [[maybe_unused]] Vec3d result = v / zeroVal;
        // Nothing specific is REQUIREd here due to UB.
        SUCCEED("Division by zero executed, result may be Inf or NaN.");
    }
}

TEST_CASE("Testing Vec3d assignment from different typed array", "[maths][new]") {
    SECTION("Assign from int array") {
        int arr[3] = {7, 8, 9};
        Vec3d v;
        REQUIRE_NOTHROW(v = arr); 
        REQUIRE(v[0] == Approx(7.0));
        REQUIRE(v[1] == Approx(8.0));
        REQUIRE(v[2] == Approx(9.0));
    }

    SECTION("Assign from float array") {
        float arr[3] = {7.05f, 8.05f, 9.05f};
        Vec3d v;
        REQUIRE_NOTHROW(v = arr);
        REQUIRE(v[0] == Approx(7.05f));
        REQUIRE(v[1] == Approx(8.05f));
        REQUIRE(v[2] == Approx(9.05f));
    }
}
TEST_CASE( "Testing Vec3d op", "[maths]" ) {
    v_type v1_ = {1,2,3};
    v_type v2_ = {2,3,4};

    Vec3d v1(v1_);
    Vec3d v2(v2_);
    Vec3d v3 = v2 - v1;

    REQUIRE(v3[0] == Approx(1.0));
    REQUIRE(v3[1] == Approx(1.0));
    REQUIRE(v3[2] == Approx(1.0));

    REQUIRE(v3[0] == Approx(v3(0)));
    REQUIRE(v3[1] == Approx(v3(1)));
    REQUIRE(v3[2] == Approx(v3(2)));

    const Vec3d v4 = v2 - v1;
    REQUIRE(v4[0] == Approx(1.0));
    REQUIRE(v4[1] == Approx(1.0));
    REQUIRE(v4[2] == Approx(1.0));

    REQUIRE(v4[0] == Approx(v4(0)));
    REQUIRE(v4[1] == Approx(v4(1)));
    REQUIRE(v4[2] == Approx(v4(2)));

    Vec3d v5 = v2 + v1;
    REQUIRE(v5[0] == Approx(3.0));
    REQUIRE(v5[1] == Approx(5.0));
    REQUIRE(v5[2] == Approx(7.0));

    v5 += v1;
    REQUIRE(v5[0] == Approx(4.0));
    REQUIRE(v5[1] == Approx(7.0));
    REQUIRE(v5[2] == Approx(10.0));

    v5 -= v1;
    REQUIRE(v5[0] == Approx(3.0));
    REQUIRE(v5[1] == Approx(5.0));
    REQUIRE(v5[2] == Approx(7.0));

    v5 *= 2;
    REQUIRE(v5[0] == Approx(6.0));
    REQUIRE(v5[1] == Approx(10.0));
    REQUIRE(v5[2] == Approx(14.0));

    v5 = -v5;
    REQUIRE(v5[0] == Approx(-6.0));
    REQUIRE(v5[1] == Approx(-10.0));
    REQUIRE(v5[2] == Approx(-14.0));

    REQUIRE((v1 * v1) == Approx(14.0));

    Vec3d v6 = v1 * 2;
    Vec3d v7 = 2 * v1;
    REQUIRE(v6 == v7);  // uses operator== which does an approximate comparison

    v7.set_zero();
    REQUIRE(v7[0] == Approx(0.0));
    REQUIRE(v7[1] == Approx(0.0));
    REQUIRE(v7[2] == Approx(0.0));
    REQUIRE(v7 != v6);

    v7 = v6;
    REQUIRE(v6 == v7);

    v6(1,2,3);
    REQUIRE(v1 == v6);

    // Previously: !WithinAbs(...) check was suspicious. Now correctly approximating:
    REQUIRE(v1.norm() == Approx(std::sqrt(14.0)).margin(1e-12));

    v6(4,5,6);
    v7 = v1.cross(v6);
    REQUIRE(v7[0] == Approx(-3.0));
    REQUIRE(v7[1] == Approx(6.0));
    REQUIRE(v7[2] == Approx(-3.0));
}

// --------------------------------------------------------------------------
// Updated Test: "Testing Vec3d operator/=(double)"
// --------------------------------------------------------------------------
TEST_CASE( "Testing Vec3d operator/=(double)", "[maths]" ) {
    Vec3d v(4,8,12);
    v /= 4.0;
    REQUIRE(v[0] == Approx(1.0));
    REQUIRE(v[1] == Approx(2.0));
    REQUIRE(v[2] == Approx(3.0));
}

// --------------------------------------------------------------------------
// Updated Test: "Testing Vec3d dot()"
// --------------------------------------------------------------------------
TEST_CASE( "Testing Vec3d dot()", "[maths]" ) {
    Vec3d a(1,2,3);
    Vec3d b(4,5,6);

    // a.dot(b) and a*b should match, but we ensure approximate float comparison:
    REQUIRE(a.dot(b) == Approx(a * b));
    REQUIRE(a.dot(b) == Approx(1*4 + 2*5 + 3*6)); // 32
    REQUIRE(a.dot(b) == Approx(b.dot(a)));       // symmetry
}
