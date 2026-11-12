/* other_c_library.c -- for tests: a C library that makes other choices than
 * the one of this build where the C standard and POSIX leave the choice to
 * the library. A test that passes with one C library and fails with another
 * that conforms as well has an expectation the program does not promise.
 * Four CTest entries (tests/CMakeLists.txt, "Tests whose expectations held
 * with one C library only") run such tests with this file loaded before
 * the C library (LD_PRELOAD), and TADAH_TEST_C_LIBRARY names what differs:
 *
 *   strerror    the text of every error number: "error <n> in the words of
 *               another C library" (the texts are the library's own: glibc
 *               says "Too many levels of symbolic links" for ELOOP, musl
 *               "Symbolic link loop");
 *   libm        sin, cos and exp: the double next above the C library's
 *               result (the C standard does not fix the last bit, and C
 *               libraries round some arguments one ulp apart);
 *   sort_other  qsort_r: sorted, the elements that compare equal in the
 *               reverse of their input order (their order is unspecified);
 *   sort_keep   qsort_r: sorted, the elements that compare equal in their
 *               input order (what glibc's merge sort gives).
 *
 * With any other value, or none, every function is the C library's.
 * other_c_library.h tells a test which choice its entry asked for and
 * whether this file is loaded (the symbol tadah_test_other_c_library). */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Present in a process that has this file loaded (other_c_library.h). */
const int tadah_test_other_c_library = 1;

enum { kNone, kStrerror, kLibm, kSortOther, kSortKeep };

static int choice(void)
{
    static int known = -1;
    if (known < 0) {
        const char *v = getenv("TADAH_TEST_C_LIBRARY");
        int c = kNone;
        if (v && !strcmp(v, "strerror")) c = kStrerror;
        if (v && !strcmp(v, "libm")) c = kLibm;
        if (v && !strcmp(v, "sort_other")) c = kSortOther;
        if (v && !strcmp(v, "sort_keep")) c = kSortKeep;
        known = c;
    }
    return known;
}

/* Puts into the function pointer at @p slot the function of that name in
 * the C library (the next object), once. */
static void next(const char *name, void *slot)
{
    void *found;
    memcpy(&found, slot, sizeof found);
    if (found) return;
    found = dlsym(RTLD_NEXT, name);
    if (!found) {
        fprintf(stderr, "other_c_library: the C library has no %s\n", name);
        abort();
    }
    memcpy(slot, &found, sizeof found);
}

char *strerror(int number)
{
    static _Thread_local char text[64];
    static char *(*library)(int);
    if (choice() != kStrerror) {
        next("strerror", &library);
        return library(number);
    }
    snprintf(text, sizeof text, "error %d in the words of another C library",
             number);
    return text;
}

static double one_ulp_up(const char *name, double (**library)(double),
                         double x)
{
    next(name, library);
    const double y = (*library)(x);
    return choice() == kLibm ? nextafter(y, DBL_MAX) : y;
}

double sin(double x)
{
    static double (*library)(double);
    return one_ulp_up("sin", &library, x);
}

double cos(double x)
{
    static double (*library)(double);
    return one_ulp_up("cos", &library, x);
}

double exp(double x)
{
    static double (*library)(double);
    return one_ulp_up("exp", &library, x);
}

typedef int (*compare_fn)(const void *, const void *, void *);

void qsort_r(void *base, size_t count, size_t size, compare_fn compare,
             void *arg)
{
    static void (*library)(void *, size_t, size_t, compare_fn, void *);
    const int c = choice();
    if (c != kSortOther && c != kSortKeep) {
        next("qsort_r", &library);
        library(base, count, size, compare, arg);
        return;
    }
    if (count < 2 || size == 0) return;
    char *a = base;
    char *held = malloc(size);
    if (!held) abort();
    /* Insertion sort: an element moves left past the larger ones only, so
     * those that compare equal keep their input order. */
    for (size_t i = 1; i < count; ++i) {
        size_t j = i;
        memcpy(held, a + i * size, size);
        while (j > 0 && compare(a + (j - 1) * size, held, arg) > 0) {
            memcpy(a + j * size, a + (j - 1) * size, size);
            --j;
        }
        memcpy(a + j * size, held, size);
    }
    if (c == kSortOther) {
        /* Every run of elements that compare equal, reversed. */
        for (size_t first = 0; first < count;) {
            size_t last = first;
            while (last + 1 < count &&
                   compare(a + last * size, a + (last + 1) * size, arg) == 0)
                ++last;
            for (size_t lo = first, hi = last; lo < hi; ++lo, --hi) {
                memcpy(held, a + lo * size, size);
                memcpy(a + lo * size, a + hi * size, size);
                memcpy(a + hi * size, held, size);
            }
            first = last + 1;
        }
    }
    free(held);
}
