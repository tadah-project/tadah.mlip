#define CATCH_FD_TYPE_MAIN
#include "catch2/catch.hpp"
#include <tadah/core/fd_type.h>

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace tadah::core;

TEST_CASE("fd_type default constructor") {
    fd_type fd;
    REQUIRE(fd.rows() == 0);
}

TEST_CASE("fd_type parameterized constructor") {
    size_t dim = 5;
    fd_type fd(dim);
    REQUIRE(fd.rows() == dim);

    for (size_t i = 0; i < dim; ++i) {
        for (int j = 0; j < 3; ++j) {
            REQUIRE(fd(i, j) == 0.0);
        }
    }
}

TEST_CASE("fd_type copy constructor") {
    size_t dim = 3;
    fd_type fd1(dim);
    fd1(1, 1) = 5.0;

    fd_type fd2(fd1);
    REQUIRE(fd2.rows() == fd1.rows());
    REQUIRE(fd2(1, 1) == 5.0);
}

TEST_CASE("fd_type move constructor") {
    size_t dim = 3;
    fd_type fd1(dim);
    fd1(1, 1) = 5.0;

    fd_type fd2(std::move(fd1));
    REQUIRE(fd2.rows() == dim);
    REQUIRE(fd2(1, 1) == 5.0);
}

TEST_CASE("fd_type copy assignment operator") {
    size_t dim = 3;
    fd_type fd1(dim);
    fd1(1, 1) = 7.0;

    fd_type fd2;
    fd2 = fd1;
    REQUIRE(fd2.rows() == fd1.rows());
    REQUIRE(fd2(1, 1) == 7.0);
}

TEST_CASE("fd_type move assignment operator") {
    size_t dim = 3;
    fd_type fd1(dim);
    fd1(1, 1) = 9.0;

    fd_type fd2;
    fd2 = std::move(fd1);
    REQUIRE(fd2.rows() == dim);
    REQUIRE(fd2(1, 1) == 9.0);
}

TEST_CASE("fd_type arithmetic operations") {
    size_t dim = 3;
    fd_type fd1(dim);
    fd_type fd2(dim);
    fd1(1, 1) = 3.0;
    fd2(1, 1) = 1.0;

    fd_type fd3 = fd1 + fd2;
    REQUIRE(fd3(1, 1) == 4.0);

    fd3 = fd1 - fd2;
    REQUIRE(fd3(1, 1) == 2.0);

    fd1 *= 2.0;
    REQUIRE(fd1(1, 1) == 6.0);
}

TEST_CASE("fd_type set_zero and random") {
    size_t dim = 3;
    fd_type fd(dim);
    fd(0, 0) = 5.0;

    fd.set_zero();
    for (size_t i = 0; i < fd.rows(); ++i) {
        for (int j = 0; j < 3; ++j) {
            REQUIRE(fd(i, j) == 0.0);
        }
    }

    fd.random();
    bool allZero = true;
    for (size_t i = 0; i < fd.rows(); ++i) {
        for (int j = 0; j < 3; ++j) {
            if (fd(i, j) != 0.0) {
                allZero = false;
                break;
            }
        }
    }
    REQUIRE_FALSE(allZero);
}

TEST_CASE("fd_type equality and approximation") {
    size_t dim = 3;
    fd_type fd1(dim);
    fd_type fd2(dim);

    REQUIRE(fd1 == fd2);
    
    fd1(1, 1) = 0.0000001;
    REQUIRE(fd1.isApprox(fd2, 1e-6));
}


TEST_CASE("fd_type single dimension boundary checks", "[fd_type][new]") {
    // Test with dim=1
    fd_type fd1(1);
    REQUIRE(fd1.rows() == 1);
    
    // Check initial zero fill
    REQUIRE(fd1(0, 0) == 0.0);
    REQUIRE(fd1(0, 1) == 0.0);
    REQUIRE(fd1(0, 2) == 0.0);

    // Verify arithmetic on single row
    fd1(0,0) = 10.0;
    fd1(0,1) = 20.0;
    fd1 *= 2.0;
    REQUIRE(fd1(0,0) == 20.0);
    REQUIRE(fd1(0,1) == 40.0);

    // Another single dimension instance
    fd_type fd2(1);
    fd2(0,0) = 5.0;
    fd2(0,1) = 7.0;

    // Operator +
    fd_type fd3 = fd1 + fd2;
    REQUIRE(fd3(0,0) == 25.0);
    REQUIRE(fd3(0,1) == 47.0);

    // Operator -
    fd3 = fd1 - fd2;
    REQUIRE(fd3(0,0) == 15.0);
    REQUIRE(fd3(0,1) == 33.0);
}

TEST_CASE("fd_type operator[](size_t) and operator()(size_t k)", "[fd_type][new]") {
    fd_type fd(3);
    // Fill each coordinate
    fd(0,0) = 1.1; fd(1,0) = 1.2; fd(2,0) = 1.3;
    fd(0,1) = 2.1; fd(1,1) = 2.2; fd(2,1) = 2.3;
    fd(0,2) = 3.1; fd(1,2) = 3.2; fd(2,2) = 3.3;

    // operator[](size_t k) returns aed_type reference
    aed_type& ref0 = fd[0]; // x-coordinates
    REQUIRE(ref0.size() == 3);
    REQUIRE(ref0(0) == 1.1);
    REQUIRE(ref0(1) == 1.2);
    REQUIRE(ref0(2) == 1.3);

    // operator()(size_t k) const and non-const
    const fd_type& cfd = fd;
    REQUIRE(cfd(0)(2) == 1.3); // read-only usage
    fd_type& mfd = const_cast<fd_type&>(cfd);
    mfd(1)(0) = 9.9; // modifies y-coordinates at index 0
    REQUIRE(fd(0,1) == 9.9);
}

/* Pins: the column accessors fd[k] and fd(k) need only k < 3: a
 * default-constructed fd_type (rows() 0) has three empty columns, size 0
 * through operator[], the non-const operator() and the const operator(), as
 * fd_type(0) has; a copy of it has them too.
 * Before the fix: a default-constructed fd_type held three null column
 * pointers and the accessors dereferenced them: a segmentation fault that
 * ends the test process (fd_type(0) and copies already had columns). */
TEST_CASE("fd_type: a default-constructed fd_type has three empty columns",
          "[fd_type]") {
    fd_type fd;
    const fd_type& cfd = fd;
    for (size_t k = 0; k < 3; ++k) {
        INFO("column " << k);
        CHECK(fd[k].size() == 0);
        CHECK(fd(k).size() == 0);
        CHECK(cfd(k).size() == 0);
    }
    const fd_type copy(fd);
    for (size_t k = 0; k < 3; ++k) CHECK(copy(k).size() == 0);
    fd_type zero(0);
    for (size_t k = 0; k < 3; ++k) CHECK(zero[k].size() == 0);
}

/* Pins: an fd_type moved from, by construction or by assignment, has rows()
 * 0 and three empty columns (size 0 through operator[] and both operator()),
 * and the fd_type moved to keeps its values in its columns: fd2[1](1) is the
 * 5.0 written at fd1(1, 1), and a write through fd2[2] reaches fd2(0, 2).
 * Before the fix: the move left the source's three column pointers null and
 * the accessors dereferenced them: a segmentation fault that ends the test
 * process. */
TEST_CASE("fd_type: a moved-from fd_type has three empty columns",
          "[fd_type]") {
    fd_type fd1(3);
    fd1(1, 1) = 5.0;
    fd_type fd2(std::move(fd1));
    CHECK(fd1.rows() == 0);
    const fd_type& c1 = fd1;
    for (size_t k = 0; k < 3; ++k) {
        INFO("constructed from, column " << k);
        CHECK(fd1[k].size() == 0);
        CHECK(fd1(k).size() == 0);
        CHECK(c1(k).size() == 0);
    }
    REQUIRE(fd2[1].size() == 3);
    CHECK(fd2[1](1) == 5.0);
    fd2[2](0) = 7.0;
    CHECK(fd2(0, 2) == 7.0);

    fd_type fd3;
    fd3 = std::move(fd2);
    CHECK(fd2.rows() == 0);
    for (size_t k = 0; k < 3; ++k) {
        INFO("assigned from, column " << k);
        CHECK(fd2[k].size() == 0);
        CHECK(fd2(k).size() == 0);
    }
    REQUIRE(fd3[2].size() == 3);
    CHECK(fd3[1](1) == 5.0);
    CHECK(fd3[2](0) == 7.0);
}

TEST_CASE("fd_type operator*(const T&) with small vector", "[fd_type][new]") {
    // Suppose we multiply each column vector by a small coefficient vector.
    // fd_type is 3 rows -> each row's x,y,z. Multiplied by a 3-element vector.
    fd_type fd(3);
    // Fill columns:
    // X-coordinates
    fd(0,0) = 1.0;  fd(1,0) = 2.0;  fd(2,0) = 3.0;
    // Y-coordinates
    fd(0,1) = 10.0; fd(1,1) = 20.0; fd(2,1) = 30.0;
    // Z-coordinates
    fd(0,2) = 100.0;fd(1,2) = 200.0;fd(2,2) = 300.0;

    // Multiply with a 3-element vector: v2 = [1, 1, 1]
    // The result is a Vec3d with components:
    //  x-component = sum over i: fd(i,x) * v2[i], i=0..2
    //  similarly for y, z
    std::vector<double> v2 = {1.0, 1.0, 1.0};
    Vec3d product = fd * v2; 
    // The x-component of product = (1.0*1.0 + 2.0*1.0 + 3.0*1.0) = 6 
    // The y-component = (10.0+20.0+30.0) = 60 
    // The z-component = (100.0+200.0+300.0) = 600

    REQUIRE(product(0) == 6.0);
    REQUIRE(product(1) == 60.0);
    REQUIRE(product(2) == 600.0);

    // Another example with different v2
    std::vector<double> v3 = {0.0, 1.0, 2.0};
    // x-component = 1*0 + 2*1 + 3*2 = 2 + 6 = 8
    // y-component = 10*0 + 20*1 + 30*2 = 20 + 60 = 80
    // z-component = 100*0 + 200*1 + 300*2 = 200 + 600 = 800
    Vec3d product2 = fd * v3;
    REQUIRE(product2(0) == 8.0);
    REQUIRE(product2(1) == 80.0);
    REQUIRE(product2(2) == 800.0);
}

TEST_CASE("fd_type operator<<(std::ostream& os, const fd_type&)", "[fd_type][new]") {
    fd_type fd(2);
    fd(0,0) = 1.1; fd(1,0) = 2.2;
    fd(0,1) = 3.3; fd(1,1) = 4.4;
    fd(0,2) = 5.5; fd(1,2) = 6.6;

    std::stringstream ss;
    ss << fd;

    // The operator<< prints each aed_type in a row. 
    // By default, aed_type uses indices in the same order as stored.
    // Checks only that the stream is non-empty and has numeric data.
    REQUIRE_FALSE(ss.str().empty());

    // Basic pattern check. 
    // Typically each coordinate line might appear as "1.1 2.2", then next line "3.3 4.4", then "5.5 6.6".
    REQUIRE(ss.str().find("1.1") != std::string::npos);
    REQUIRE(ss.str().find("6.6") != std::string::npos);
}

/* Pins: the whole-object operations of fd_type take operands of its size
 * (the per-element accessors fd(i, k), fd[k] and fd(k) stay unchecked: their
 * preconditions i < rows() and k < 3 are the caller's):
 *  - fd_types of 4 and of 2 rows are not equal, whichever is compared with
 *    which, by == or by isApprox with any tolerance;
 *  - big - small and big + small throw std::invalid_argument naming both
 *    row counts ("fd_type: operator- of fd_types of 4 and 2 rows");
 *  - small * (a vector of 6 values) throws std::invalid_argument naming the
 *    2 rows and the 6 values.
 * Before the fix: small == big and small.isApprox(big, 1.0) compared the
 * first 6 of the 12 zeros of big with the 6 of small, and fd_type() ==
 * small compared none, so all three were true; nothing threw; big == small,
 * big.isApprox(small, 1.0), big - small, big + small and the product read
 * past the end of the smaller buffer (undefined behaviour: the two
 * comparisons happened to be false in the run recorded). This replaces
 * 'fd_type out-of-range checks', which asserted nothing in any build
 * (REQUIRE(true), or a WARN under NDEBUG). */
TEST_CASE("fd_type: operations on fd_types of different sizes are refused",
          "[fd_type]") {
    fd_type big(4), small(2);
    CHECK_FALSE(small == big);
    CHECK_FALSE(big == small);
    CHECK_FALSE(small.isApprox(big, 1.0));
    CHECK_FALSE(big.isApprox(small, 1.0));
    CHECK_FALSE(fd_type() == small);
    CHECK_THROWS_AS(big - small, std::invalid_argument);
    CHECK_THROWS_AS(big + small, std::invalid_argument);
    CHECK_THROWS_AS(small * std::vector<double>(6, 1.0), std::invalid_argument);
    CHECK_THROWS_WITH(big - small,
                      "fd_type: operator- of fd_types of 4 and 2 rows");
    CHECK_THROWS_WITH(small + big,
                      "fd_type: operator+ of fd_types of 2 and 4 rows");
    CHECK_THROWS_WITH(small * std::vector<double>(6, 1.0),
                      "fd_type: operator* of an fd_type of 2 rows and a "
                      "vector of 6 values");
}

/* Pins: on fd_types of one size the whole-object operations give their
 * elementwise results: with a = [[1, 0, 0], [0, 0, 3]] and
 * b = [[0.5, 0, 0], [0, 0, -1]] (rows), a + b and a - b are
 * [[1.5, 0, 0], [0, 0, 2]] and [[0.5, 0, 0], [0, 0, 4]], a * (2, 3) is
 * (2, 0, 9), a == a copy of a, a != b, a.isApprox(b, 4) and not
 * a.isApprox(b, 3.9) (the largest difference is 4), and two fd_types of 0
 * rows are equal.
 * Before the fix: passes as well (a pin of the results the size checks
 * must leave unchanged). */
TEST_CASE("fd_type: operations on fd_types of one size are elementwise",
          "[fd_type]") {
    fd_type a(2), b(2);
    a(0, 0) = 1.0;
    a(1, 2) = 3.0;
    b(0, 0) = 0.5;
    b(1, 2) = -1.0;
    const fd_type s = a + b;
    const fd_type d = a - b;
    REQUIRE(s.rows() == 2);
    REQUIRE(d.rows() == 2);
    for (size_t i = 0; i < 2; ++i) {
        for (size_t k = 0; k < 3; ++k) {
            INFO("(" << i << ", " << k << ")");
            const double ws = i == 0 && k == 0 ? 1.5 : i == 1 && k == 2 ? 2.0 : 0.0;
            const double wd = i == 0 && k == 0 ? 0.5 : i == 1 && k == 2 ? 4.0 : 0.0;
            CHECK(s(i, k) == ws);
            CHECK(d(i, k) == wd);
        }
    }
    const Vec3d p = a * std::vector<double>{2.0, 3.0};
    CHECK(p[0] == 2.0);
    CHECK(p[1] == 0.0);
    CHECK(p[2] == 9.0);
    const fd_type a2(a);
    CHECK(a == a2);
    CHECK_FALSE(a == b);
    CHECK(a.isApprox(b, 4.0));
    CHECK_FALSE(a.isApprox(b, 3.9));
    CHECK(fd_type() == fd_type());
}

TEST_CASE("fd_type dimension changes: copy from different sized fd_type", "[fd_type][improved]") {
    // Created with dimension=5, then dimension=3, and copied.
    fd_type fdBig(5);
    fdBig(4, 2) = 99.0; // use last row, z-col

    fd_type fdSmall(2);
    fdSmall(1, 1) = 1.23;

    // Copy small -> big
    fdBig = fdSmall;
    REQUIRE(fdBig.rows() == 2);   // dimension changed from 5 to 2
    REQUIRE(fdBig(1, 1) == 1.23); // data is from fdSmall now

    // Next copy big -> small
    // Re-fill fdBig to dimension=4 so it’s not the same as fdSmall
    fd_type fdMid(4);
    fdMid(3, 2) = 7.89;
    fdSmall = fdMid;
    REQUIRE(fdSmall.rows() == 4);
    REQUIRE(fdSmall(3, 2) == 7.89);
}

TEST_CASE("fd_type random distribution bounds check", "[fd_type][improved]") {
    fd_type fd(5);
    double lower = -10.0;
    double upper = 10.0;
    fd.random(std::chrono::system_clock::now().time_since_epoch().count(), lower, upper);

    // Confirm all data is within [lower, upper]
    for (size_t i = 0; i < fd.rows(); ++i) {
      for (int k = 0; k < 3; ++k) {
        double val = fd(i, k);
        REQUIRE(val >= lower);
        REQUIRE(val <= upper);
      }
    }
}

TEST_CASE("fd_type borderline differences for isApprox()", "[fd_type][improved]") {
    size_t dim = 2;
    fd_type fd1(dim);
    fd_type fd2(dim);

    // Make them almost identical
    fd1(0,0) = 1.0;   fd2(0,0) = 1.0;
    fd1(1,2) = 10.0;  fd2(1,2) = 10.0;

    // They should be approx
    REQUIRE(fd1.isApprox(fd2, 1e-9) == true);

    // Now exceed the EPS a bit
    fd2(1,2) = 10.00001; 
    REQUIRE(fd1.isApprox(fd2, 1e-12) == false); 
    // because difference is 1e-5 which is >> 1e-12
}

namespace {

/* The message f throws, or "no exception". */
template <class F> std::string fd_refusal(F f) {
  try {
    f();
  } catch (const std::exception &e) {
    return e.what();
  }
  return "no exception";
}

}  // namespace

/* Pins: a column of an fd_type is a view of the fd_type's values, and an
 * assignment to it writes through: fd[k] = a (a copy), fd[k] = a * 2.0 (a
 * move from a temporary) and fd(k) = a set fd(i, k) for every i, the
 * column stays the view (fd[k].data() is fd.ptr() + k * rows()), the other
 * columns are unchanged, and the fd_type's copy has the values. Assigning a
 * vector of another size throws std::invalid_argument naming both sizes and
 * changes nothing. Moving a column out (aed_type x(std::move(fd[k])), x =
 * std::move(fd[k])) copies its values and leaves the column in place. A
 * view is not resized: resize to another size throws; to its size is a
 * no-op. A default-constructed fd_type's columns are empty views (an
 * assignment of a non-empty vector throws).
 * Before the fix: the assignment made the column own a new buffer with the
 * values (move: the temporary's), so fd(i, k), ptr() and a copy of the
 * fd_type kept the old values (0) and fd[k].data() was not fd.ptr() + k *
 * rows(); a size mismatch replaced the column with a vector of another
 * size; a move out left the column a null data() with its size 3 (a
 * dangling column); resize gave the column a buffer of its own, and a
 * default fd_type's column took the assigned values. */
TEST_CASE("fd_type: an assignment to a column writes into the fd_type",
          "[fd_type][column_view]") {
  const aed_type a({1.0, 2.0, 3.0});
  fd_type fd(3);
  fd[1] = a;
  fd[2] = a * 2.0;
  for (size_t i = 0; i < 3; ++i) {
    INFO("row " << i);
    CHECK(fd(i, 0) == 0.0);
    CHECK(fd(i, 1) == a[i]);
    CHECK(fd(i, 2) == 2.0 * a[i]);
  }
  for (size_t k = 0; k < 3; ++k) {
    CHECK(fd[k].data() == fd.ptr() + 3 * k);
    CHECK(fd[k].size() == 3);
  }
  const fd_type copy(fd);
  CHECK(copy(2, 1) == 3.0);
  CHECK(copy(2, 2) == 6.0);
  fd(0) = a;
  CHECK(fd(1, 0) == 2.0);
  CHECK(fd(0).data() == fd.ptr());

  // Another size: refused, nothing changed.
  CHECK(fd_refusal([&] { fd[0] = aed_type(4); }) ==
        "aed_type: cannot assign 4 values to a view of 3 values (a column of "
        "an fd_type or aeds_type2 is a view of its values)");
  CHECK(fd_refusal([&] { fd[0] = aed_type(); }) ==
        "aed_type: cannot assign 0 values to a view of 3 values (a column of "
        "an fd_type or aeds_type2 is a view of its values)");
  CHECK(fd(0, 0) == 1.0);
  CHECK(fd[0].data() == fd.ptr());

  // Moving a column out copies it.
  aed_type moved(std::move(fd[1]));
  CHECK(moved.size() == 3);
  CHECK(moved[2] == 3.0);
  CHECK(moved.data() != fd.ptr() + 3);
  aed_type assigned;
  assigned = std::move(fd[2]);
  CHECK(assigned.size() == 3);
  CHECK(assigned[2] == 6.0);
  for (size_t k = 1; k < 3; ++k) {
    CHECK(fd[k].data() == fd.ptr() + 3 * k);
    CHECK(fd[k].size() == 3);
  }
  CHECK(fd(2, 1) == 3.0);
  CHECK(fd(2, 2) == 6.0);
  moved[0] = 99.0;  // a copy: the fd_type is not changed
  CHECK(fd(0, 1) == 1.0);

  // A view keeps its size.
  CHECK(fd_refusal([&] { fd[0].resize(5); }) ==
        "aed_type: cannot resize a view of 3 values to 5 (a column of an "
        "fd_type or aeds_type2 is a view of its values)");
  CHECK_NOTHROW(fd[0].resize(3));
  CHECK(fd[0].data() == fd.ptr());

  // A default-constructed fd_type has empty columns that are views.
  fd_type empty;
  CHECK(fd_refusal([&] { empty[0] = a; }) ==
        "aed_type: cannot assign 3 values to a view of 0 values (a column of "
        "an fd_type or aeds_type2 is a view of its values)");
  CHECK(empty[0].size() == 0);
}

/* Pins: an owning aed_type keeps its value semantics: copy assignment of
 * another size replaces its values (a buffer of its own, not shared), move
 * assignment takes the other's buffer (b.data() is the moved-from c's old
 * buffer; what is left in c: the next case), and assigning a column view
 * to an owner copies the values.
 * Before the fix: passes as well (a pin of what the fix keeps). */
TEST_CASE("aed_type: an owning vector's assignment replaces its values",
          "[fd_type][column_view]") {
  aed_type b(2);
  const aed_type a({1.0, 2.0, 3.0});
  b = a;
  CHECK(b.size() == 3);
  CHECK(b[2] == 3.0);
  CHECK(b.data() != a.data());
  aed_type c({4.0, 5.0});
  const double *cdata = c.data();
  b = std::move(c);
  CHECK(b.size() == 2);
  CHECK(b.data() == cdata);
  fd_type fd(3);
  fd(1, 0) = 7.0;
  aed_type d;
  d = fd[0];
  CHECK(d.size() == 3);
  CHECK(d[1] == 7.0);
  CHECK(d.data() != fd.ptr());
}

/* Pins: a moved-from owning aed_type is empty: after `b = std::move(c)`,
 * c.size() is 0 and c.data() is null, and after `aed_type x(std::move(o))`
 * the same holds for o; b and x hold the moved values.
 * Before the fix: c.size() was 2 and o.size() 3, with data() null, so
 * c[0], c.begin() to c.end() or a loop to c.size() read through a null
 * pointer. */
TEST_CASE("aed_type: a moved-from owner is empty", "[fd_type][column_view]") {
  aed_type b(3);
  aed_type c({4.0, 5.0});
  b = std::move(c);
  CHECK(b.size() == 2);
  CHECK(b[1] == 5.0);
  CHECK(c.size() == 0);
  CHECK(c.data() == nullptr);

  aed_type o({1.0, 2.0, 3.0});
  aed_type x(std::move(o));
  CHECK(x.size() == 3);
  CHECK(x[2] == 3.0);
  CHECK(o.size() == 0);
  CHECK(o.data() == nullptr);
}
