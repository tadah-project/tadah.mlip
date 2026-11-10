#define CATCH_AED_TYPE_MAIN
#include "catch2/catch.hpp"
#include <tadah/core/aed_type.h>
#include <tadah/core/maths.h>
#include <iterator>
#include <numeric>
#include <cstdlib>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace tadah::core;

TEST_CASE("aed_type default constructor") {
    aed_type vec;
    REQUIRE(vec.size() == 0);
}

TEST_CASE("aed_type parameterized constructor") {
    size_t size = 5;
    aed_type vec(size);
    REQUIRE(vec.size() == size);
    for (size_t i = 0; i < size; ++i) {
        REQUIRE(vec[i] == 0.0);
    }
}

TEST_CASE("aed_type from array constructor") {
    double data[] = {1.1, 2.2, 3.3};
    aed_type vec(data, 3);
    REQUIRE(vec.size() == 3);
    REQUIRE(vec[0] == 1.1);
    REQUIRE(vec[1] == 2.2);
    REQUIRE(vec[2] == 3.3);
}

TEST_CASE("aed_type copy constructor") {
    double data[] = {4.4, 5.5};
    aed_type vec1(data, 2);
    aed_type vec2(vec1);
    REQUIRE(vec2.size() == vec1.size());
    REQUIRE(vec2[0] == vec1[0]);
    REQUIRE(vec2[1] == vec1[1]);
}

TEST_CASE("aed_type move constructor") {
    aed_type vec1(3);
    vec1[0] = 1.1; vec1[1] = 2.2; vec1[2] = 3.3;
    aed_type vec2(std::move(vec1));
    REQUIRE(vec2.size() == 3);
}

TEST_CASE("aed_type arithmetic operations") {
    aed_type vec(3);
    vec[0] = 1.0; vec[1] = 2.0; vec[2] = 3.0;
    vec += 1.0;
    vec *= 2.0;
    vec -= 1.0;

    REQUIRE(vec[0] == 3.0);
    REQUIRE(vec[1] == 5.0);
    REQUIRE(vec[2] == 7.0);

    aed_type vec2 = vec + 3.0;
    REQUIRE(vec2[0] == 6.0);

    vec2 = vec - 1.0;
    REQUIRE(vec2[0] == 2.0);
}

TEST_CASE("aed_type dot product") {
    double data[] = {1.0, 2.0};
    aed_type vec1(data, 2);
    aed_type vec2 = vec1;
    double result = vec1 * vec2;
    REQUIRE(result == 5.0);  // 1*1 + 2*2
}

TEST_CASE("aed_type equality check") {
    double data[] = {1.0, 2.0, 3.0};
    aed_type vec1(data, 3);
    aed_type vec2(vec1);
    REQUIRE(vec1 == vec2);
    REQUIRE_FALSE(vec1 != vec2);
}

TEST_CASE("aed_type set zero and resize") {
    aed_type vec(3);
    vec[0] = 1.0; vec[1] = 2.0; vec[2] = 3.0;
    vec.set_zero();
    for (size_t i = 0; i < vec.size(); ++i) {
        REQUIRE(vec[i] == 0.0);
    }

    vec.resize(5);
    REQUIRE(vec.size() == 5);
    for (size_t i = 0; i < vec.size(); ++i) {
        REQUIRE(vec[i] == 0.0);
    }
}

TEST_CASE("aed_type random generation") {
    aed_type vec(5);
    vec.random();
    bool allZero = true;
    for (size_t i = 0; i < vec.size(); ++i) {
        if (vec[i] != 0.0) {
            allZero = false;
            break;
        }
    }
    REQUIRE_FALSE(allZero);
}

TEST_CASE("aed_type mean and std deviation") {
    double data[] = {1.0, 2.0, 3.0, 4.0};
    aed_type vec(data, 4);

    double mean = vec.mean();
    REQUIRE(mean == Approx(2.5));

    double stddev = vec.std_dev(mean, 4);
    REQUIRE(stddev == Approx(1.11803));
}

TEST_CASE("aed_type copy method") {
    double data[] = {1.0, 2.0, 3.0};
    aed_type vec(data, 3);

    aed_type vec_copy = vec.copy(2);

    REQUIRE(vec_copy.size() == 2);
    REQUIRE(vec_copy[0] == 1.0);
    REQUIRE(vec_copy[1] == 2.0);
}

TEST_CASE("aed_type copy() out-of-range throws", "[aed_type][new]") {
    // copy(size_t nelements, size_t start) checks if (nelements + start > size())
    aed_type vec({10.0, 20.0, 30.0});
    REQUIRE_THROWS_AS(vec.copy(2, 2), std::runtime_error);
    // because 2 + 2 = 4 > size 3 => "Out of range"

    REQUIRE_NOTHROW(vec.copy(3, 0)); // 3 + 0 = 3 => valid
}

TEST_CASE("aed_type copy assignment self-assignment", "[aed_type][new]") {
    // Confirm that self-assignment won't break anything
    aed_type vec({1.1, 2.2, 3.3});
    aed_type &vecRef = vec;
    REQUIRE_NOTHROW(vec = vecRef); // self-assignment
    REQUIRE(vec.size() == 3);
    REQUIRE(vec[1] == 2.2);
}

TEST_CASE("aed_type move assignment self-assignment", "[aed_type][new]") {
    aed_type vec({1.1, 2.2, 3.3});
    aed_type &vecRef = vec;
    REQUIRE_NOTHROW(vec = std::move(vecRef)); 
    // Technically self move-assignment is discouraged. We test that it won’t crash 
    // though behavior can be arbitrary. Just for coverage.
}

TEST_CASE("aed_type operator*(T) with size mismatch (potential UB)", "[aed_type][new]") {
    // The code does not do a size check before summation => potential out-of-bounds
    // If using dynamically allocated arrays, this can crash or read random memory.
    // Note it’s undefined behavior (not an exception).
    aed_type vec1({1.0, 2.0});
    aed_type vec2({3.0});
    
    // Because there's no check, we can't guarantee a result.
    // The call goes in a REQUIRE_NOTHROW, as the implementation doesn't throw:
    REQUIRE_NOTHROW(static_cast<void>(vec1 * vec2));
    // The result is likely out-of-bounds for the second element. 
}

TEST_CASE("aed_type operator<<(std::ostream&)", "[aed_type][new]") {
    aed_type vec({5.5, 6.6, 7.7});
    std::stringstream ss;
    ss << vec;
    // We expect something like "5.5  6.6  7.7"
    REQUIRE_FALSE(ss.str().empty());
    REQUIRE(ss.str().find("5.5") != std::string::npos);
    REQUIRE(ss.str().find("6.6") != std::string::npos);
    REQUIRE(ss.str().find("7.7") != std::string::npos);
}

TEST_CASE("aed_type::resize shrink then grow", "[aed_type][resize]") {
    // Regression: shrinking used to call std::fill with first > last
    // (fill range computed from the OLD size on the reallocated,
    // smaller block), writing zeros past the end of the heap buffer.
    // Exercised by the FIXWEIGHT partial-fix path, where the model
    // weight vector shrinks from K to K - |FIXINDEX| between trainings.
    aed_type v(14);
    for (size_t i = 0; i < v.size(); ++i) v[i] = 1.0 + double(i);

    v.resize(12);  // shrink: must not write beyond the new block
    REQUIRE(v.size() == 12);
    for (size_t i = 0; i < 12; ++i) REQUIRE(v[i] == 1.0 + double(i));

    v.resize(14);  // grow again: new tail must be zero-initialised
    REQUIRE(v.size() == 14);
    for (size_t i = 0; i < 12; ++i) REQUIRE(v[i] == 1.0 + double(i));
    REQUIRE(v[12] == 0.0);
    REQUIRE(v[13] == 0.0);

    v.resize(14);  // no-op resize keeps contents
    REQUIRE(v[5] == 6.0);
}

namespace {

// M(3, 4) with M(r, c) = 10 r + c; Matrix::row(2) is a view of M(2, 0 .. 3),
// 4 values 3 apart (M is column-major: M(r, c) is element r + 3 c).
Matrix row_view_matrix() {
  Matrix M(3, 4);
  for (size_t r = 0; r < 3; ++r)
    for (size_t c = 0; c < 4; ++c) M(r, c) = 10.0 * r + c;
  return M;
}

// The values of x as its operator[], data() and begin() to end() give them.
std::vector<double> indexed(const aed_type &x) {
  std::vector<double> v;
  for (size_t i = 0; i < x.size(); ++i) v.push_back(x[i]);
  return v;
}
std::vector<double> stored(const aed_type &x) {
  return std::vector<double>(x.data(), x.data() + x.size());
}
std::vector<double> ranged(const aed_type &x) {
  return std::vector<double>(x.begin(), x.end());
}

}  // namespace

/* Pins: a copy of a strided view, Matrix::row(2) of a 3 x 4 matrix (4
 * values 3 apart), is an owner of the row's 4 values one after another
 * (stride 1): its operator[], data()[0 .. 3] and begin() to end() are
 * {20, 21, 22, 23}, for the copy constructor, the copy assignment into an
 * owner, the move constructor (std::vector::push_back of the row, and
 * aed_type(std::move(view))), the move assignment into an owner
 * (x = M.row(2)) and the copy that const Matrix::row(2) gives; writing to
 * a copy leaves the matrix as it was, and the moved-from view still views
 * M(2, 0 .. 3). A copy of an owner made with a stride (aed_type(3, 2))
 * holds its values one after another as well.
 * Before the fix: every copy kept the stride (3) and copied size * stride
 * = 12 doubles from M(2, 0), element 2 of M's 12, so it read 2 doubles
 * past the end of M's storage (AddressSanitizer: heap-buffer-overflow,
 * READ of size 96, in the copy and move constructors, the copy
 * assignment and aed_type(const double*, size, stride)), and data() and
 * begin() to end() gave M's elements 2 to 5, {20, 1, 11, 21}, not the row;
 * the copy of the strided owner kept its stride 2 (data() {1, 0, 2}). The
 * move paths from a view were new on this branch (v1-dev's moves took the
 * view's pointer); the copies and const Matrix::row read past the end on
 * v1-dev too. */
TEST_CASE("aed_type: a copy of a strided view holds its values one after "
          "another", "[aed_type][column_view]") {
  Matrix M = row_view_matrix();
  const std::vector<double> row2{20.0, 21.0, 22.0, 23.0};
  auto check_copy = [&](const aed_type &x, const char *what) {
    INFO(what);
    CHECK_FALSE(x.is_view());
    CHECK(x.size() == 4);
    CHECK(indexed(x) == row2);
    CHECK(stored(x) == row2);
    CHECK(ranged(x) == row2);
  };

  aed_type view = M.row(2);
  REQUIRE(view.is_view());
  REQUIRE(view.data() == &M(2, 0));

  aed_type copied(view);
  check_copy(copied, "copy constructor");
  aed_type assigned({9.0});
  assigned = view;
  check_copy(assigned, "copy assignment into an owner");
  std::vector<aed_type> rows;
  rows.push_back(M.row(2));
  check_copy(rows[0], "move constructor (push_back of M.row(2))");
  aed_type move_assigned;
  move_assigned = M.row(2);
  check_copy(move_assigned, "move assignment into an owner");
  aed_type moved(std::move(view));
  check_copy(moved, "move constructor from a named view");
  CHECK(view.is_view());
  CHECK(view.data() == &M(2, 0));
  CHECK(indexed(view) == row2);
  const Matrix &cM = M;
  aed_type const_row = cM.row(2);
  check_copy(const_row, "const Matrix::row");

  // The copies own their values.
  copied[0] = -1.0;
  rows[0][1] = -1.0;
  move_assigned[2] = -1.0;
  moved[3] = -1.0;
  const_row[0] = -1.0;
  CHECK(M(2, 0) == 20.0);
  CHECK(M(2, 1) == 21.0);
  CHECK(M(2, 2) == 22.0);
  CHECK(M(2, 3) == 23.0);

  aed_type strided(3, 2);
  strided[0] = 1.0;
  strided[1] = 2.0;
  strided[2] = 3.0;
  const std::vector<double> s{1.0, 2.0, 3.0};
  aed_type s_copy(strided);
  CHECK(indexed(s_copy) == s);
  CHECK(stored(s_copy) == s);
  aed_type s_assigned;
  s_assigned = strided;
  CHECK(stored(s_assigned) == s);
}

/* Pins: an owner assigned from a view of its own values (the non-owning
 * pointer constructor over x.data()) keeps them: x = view leaves x
 * {1, 2, 3}, an owner of a buffer of its own.
 * Before the fix: the copy assignment freed x's buffer before it copied
 * the view's values, which were in that buffer, so it read freed memory
 * (AddressSanitizer: heap-use-after-free, READ of size 24, in the copy
 * assignment); x was {0, 0, 0} in the Release build. On v1-dev as well. */
TEST_CASE("aed_type: an owner assigned from a view of its own values keeps "
          "them", "[aed_type][column_view]") {
  aed_type x({1.0, 2.0, 3.0});
  aed_type view(x.data(), 3);
  REQUIRE(view.is_view());
  x = view;
  CHECK_FALSE(x.is_view());
  CHECK(indexed(x) == std::vector<double>{1.0, 2.0, 3.0});
}

/* Pins: begin() to end() of an aed_type visit its size() values through
 * its stride, as operator[] does: a range-for, std::accumulate, the vector
 * of begin() to end(), std::distance, end()[-1], begin()[2] and a reverse
 * walk over Matrix::row(r) of a 3 x 4 matrix (4 values 3 apart) give
 * M(r, 0 .. 3) for every row r, through the const and the non-const
 * iterators; a range-for that writes through a row view writes the row
 * (M(1, c) = -c) and nothing else; an owner made with a stride
 * (aed_type(3, 2)) visits its 3 values; an empty aed_type has begin() ==
 * end().
 * Before the fix: begin() and end() were data() and data() + size(), so
 * they walked size() consecutive doubles from the row's first value, the
 * column-major neighbours: row 0 gave {0, 10, 20, 1}, row 1 {10, 20, 1, 11}
 * and row 2 {20, 1, 11, 21}; the writing loop set M(1, 0), M(2, 0),
 * M(0, 1) and M(1, 1); the strided owner gave {1, 0, 2}. */
TEST_CASE("aed_type: iteration visits the values through the stride",
          "[aed_type][column_view]") {
  Matrix M = row_view_matrix();
  const Matrix &cM = M;
  for (size_t r = 0; r < 3; ++r) {
    INFO("row " << r);
    std::vector<double> row;
    for (size_t c = 0; c < 4; ++c) row.push_back(10.0 * r + c);

    aed_type view = M.row(r);
    REQUIRE(view.is_view());
    std::vector<double> walked;
    for (double v : view) walked.push_back(v);
    CHECK(walked == row);
    CHECK(ranged(view) == row);
    const aed_type &cview = view;
    std::vector<double> cwalked;
    for (double v : cview) cwalked.push_back(v);
    CHECK(cwalked == row);
    CHECK(std::accumulate(view.begin(), view.end(), 0.0) ==
          40.0 * r + 6.0);
    CHECK(std::distance(view.begin(), view.end()) == 4);
    CHECK(view.end()[-1] == row[3]);
    CHECK(view.begin()[2] == row[2]);
    CHECK(*(cview.begin() + 1) == row[1]);
    std::vector<double> reversed;
    for (auto it = view.end(); it != view.begin();) reversed.push_back(*--it);
    CHECK(reversed == std::vector<double>(row.rbegin(), row.rend()));
  }

  aed_type row1 = M.row(1);
  double c = 0.0;
  for (double &v : row1) v = -(c++);
  for (size_t r = 0; r < 3; ++r)
    for (size_t k = 0; k < 4; ++k) {
      INFO("M(" << r << ", " << k << ")");
      CHECK(cM(r, k) == (r == 1 ? -double(k) : 10.0 * r + k));
    }

  aed_type strided(3, 2);
  strided[0] = 1.0;
  strided[1] = 2.0;
  strided[2] = 3.0;
  CHECK(ranged(strided) == std::vector<double>{1.0, 2.0, 3.0});

  aed_type empty;
  CHECK(empty.begin() == empty.end());
  const aed_type &cempty = empty;
  CHECK(cempty.begin() == cempty.end());
}

/* Pins: the adopting constructor aed_type(arr, size, stride, true) owns a
 * buffer of size * stride doubles from malloc (resize() reallocates size *
 * stride, the destructor frees it): one with values reads and writes them
 * through the stride and frees the buffer; a null buffer for size > 0
 * cannot hold them and is refused (std::invalid_argument naming the size
 * and the stride); a null buffer of no values is an empty owner.
 * Before the fix: aed_type(nullptr, 3, 2, true) was constructed without
 * an error (data() null, size() 3), so the message check failed against an
 * empty string; the owner checks and the null buffer of no values passed
 * as well (a pin of the buffer contract). */
TEST_CASE("aed_type: the adopting constructor owns a malloc'ed buffer of "
          "size * stride values", "[aed_type]") {
  double *buf = static_cast<double *>(malloc(3 * 2 * sizeof(double)));
  for (size_t i = 0; i < 6; ++i) buf[i] = double(i);
  {
    aed_type owner(buf, 3, 2, true);
    CHECK_FALSE(owner.is_view());
    CHECK(owner.size() == 3);
    CHECK(indexed(owner) == std::vector<double>{0.0, 2.0, 4.0});
    owner[1] = -1.0;
    CHECK(buf[2] == -1.0);
    owner.resize(4);
    CHECK(owner.size() == 4);
    CHECK(owner[3] == 0.0);
    CHECK(owner[1] == -1.0);
  }  // frees the buffer (AddressSanitizer: no leak, no double free)

  std::string what;
  try {
    aed_type bad(static_cast<double *>(nullptr), 3, 2, true);
  } catch (const std::invalid_argument &e) {
    what = e.what();
  }
  CHECK(what == "aed_type: cannot own a null buffer of 3 values, 2 apart "
                "(the adopting constructor takes a buffer of size * stride "
                "doubles from malloc)");
  CHECK_NOTHROW(aed_type(static_cast<double *>(nullptr), 0, 1, true));
}
