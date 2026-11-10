// Matrix_Base::row() and col() return views whatever the compiler elides.
//
// This file is compiled with -fno-elide-constructors (GCC and Clang; see
// tests/CMakeLists.txt), which turns off the copy elisions the standard
// allows but does not require (NRVO): only a prvalue returned by value is
// still built in place (C++17's guaranteed elision). A row() that returns a
// named view relies on NRVO, and a move from a view copies its values (an
// aed_type owns nothing to give), so without NRVO the caller gets a copy.
//
// The matrix here is a Matrix_Base over a storage type local to this file,
// so its row() and col() are this file's own instantiations: under LTO an
// inline Matrix::row of another file, compiled with elision, could be the
// one linked.

#include "catch2/catch.hpp"
#include <tadah/core/aed_type.h>
#include <tadah/core/maths.h>

#include <cstddef>
#include <vector>

using namespace tadah::core;

namespace {

// Column-major storage of a Matrix_Base, local to this file.
struct LocalStorage {
  std::vector<double> v;
  double &operator[](std::size_t i) { return v[i]; }
  const double &operator[](std::size_t i) const { return v[i]; }
  std::size_t size() const { return v.size(); }
};

// M(3, 4) with M(r, c) = 10 r + c.
Matrix_Base<LocalStorage> local_matrix() {
  Matrix_Base<LocalStorage> M(LocalStorage{std::vector<double>(12, 0.0)}, 3,
                              4);
  for (std::size_t r = 0; r < 3; ++r)
    for (std::size_t c = 0; c < 4; ++c) M(r, c) = 10.0 * r + c;
  return M;
}

}  // namespace

/* Pins: the non-const Matrix_Base::row(r) and col(c) of a 3 x 4 matrix
 * return views of the matrix's values, also without NRVO (this file is
 * compiled with -fno-elide-constructors under GCC and Clang): the returned
 * aed_type is a view (is_view(), data() at M(r, 0) or M(0, c)), and a write
 * through it, to a temporary (M.row(1)[2] = 5) or to a named result,
 * changes the matrix; the const row() and col() return owning copies,
 * which do not.
 * Before the fix: row() and col() returned a named view (aed_type temp(...);
 * return temp;), which without NRVO the move constructor copied (a move
 * from a view copies): the result was an owner (is_view() false) and every
 * write was lost (M(1, 2) stayed 12). With the default flags the test
 * passes as well, as GCC applies NRVO there. */
TEST_CASE("Matrix: row() and col() return views without copy elision",
          "[aed_type][column_view][no_elision]") {
  Matrix_Base<LocalStorage> M = local_matrix();

  M.row(1)[2] = 5.0;
  CHECK(M(1, 2) == 5.0);
  M.col(3)[0] = 7.0;
  CHECK(M(0, 3) == 7.0);

  aed_type row = M.row(2);
  CHECK(row.is_view());
  CHECK(row.data() == &M(2, 0));
  row[1] = -1.0;
  CHECK(M(2, 1) == -1.0);

  aed_type col = M.col(0);
  CHECK(col.is_view());
  CHECK(col.data() == &M(0, 0));
  col[2] = -2.0;
  CHECK(M(2, 0) == -2.0);

  const Matrix_Base<LocalStorage> &cM = M;
  aed_type crow = cM.row(0);
  CHECK_FALSE(crow.is_view());
  crow[0] = 99.0;
  CHECK(M(0, 0) == 0.0);
  aed_type ccol = cM.col(1);
  CHECK_FALSE(ccol.is_view());
  ccol[0] = 99.0;
  CHECK(M(0, 1) == 1.0);
}
