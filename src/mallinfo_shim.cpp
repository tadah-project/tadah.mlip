// mallinfo_shim.cpp -- link-time interposition of glibc's mallinfo2().
//
// Activated by "-Wl,--wrap=mallinfo2" on the `tadah` target (see CMakeLists.txt).
// The linker rewrites every reference to mallinfo2 as __wrap_mallinfo2, so this
// definition is what the process actually calls; glibc's original stays reachable
// as __real_mallinfo2 (unused here).
//
// WHY THIS EXISTS
// LAMMPS' Output::memory_usage() runs at every `run`/`minimize` setup and calls
// Info::get_memory_info() -> mallinfo2() to build a per-MPI-rank memory report.
// mallinfo2() walks the WHOLE process heap while holding the global malloc lock --
// an O(heap-size) operation. pc_lammps drives many single-rank LAMMPS instances on
// a shared worker pool, all sharing one large (multi-GB) process heap, each issuing
// many `run` commands; the heap-walk then dominates wall-clock and serialises the
// pool. The report is pure waste here: LAMMPS is created with `-screen none
// -log none`, and memory_usage() prints only the class-summed figure -- never the
// mallinfo-derived value. Returning a zeroed struct makes it ~free.
//
// Kept in TADAH, deliberately NOT as a LAMMPS source patch: it needs no LAMMPS
// fork, needs no per-machine setup (allocator preload, env vars), and travels
// inside the tadah binary to every deployment host. mallinfo2 is referenced
// nowhere else in this binary (only LAMMPS' info.cpp), so nothing else is affected.
//
// Linux / glibc >= 2.33 only -- the sole place LAMMPS calls mallinfo2. On older
// glibc LAMMPS falls back to mallinfo(); if TADAH is ever built there, add
// `-Wl,--wrap=mallinfo` and a matching __wrap_mallinfo() returning a zeroed
// `struct mallinfo`.
//
// NOTE ON LTO: the `tadah` build enables -flto, which emits slim-LTO objects
// with no real symbol table -- the linker's --wrap then cannot bind
// __wrap_mallinfo2. This translation unit is therefore compiled with -fno-lto
// (see CMakeLists.txt) so it produces a genuine ELF symbol, and the wrapper is
// marked `used` so it is never discarded as apparently-unreferenced.
#if defined(__linux__)
#include <features.h>   // defines __GLIBC__ / __GLIBC_PREREQ; MUST precede their tests
#if defined(__GLIBC__) && defined(__GLIBC_PREREQ)
#if __GLIBC_PREREQ(2, 33)
#include <malloc.h>
#include <cstring>
extern "C" {
struct mallinfo2 __wrap_mallinfo2(void) __attribute__((used));
struct mallinfo2 __wrap_mallinfo2(void)
{
  struct mallinfo2 mi;
  std::memset(&mi, 0, sizeof(mi));
  return mi;
}
}
#endif
#endif
#endif
