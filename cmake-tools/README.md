# Tadah CMake Tools

Common CMake utilities for the *Tadah* family of repositories.

Add to a project with FetchContent:

```cmake
include(FetchContent)
FetchContent_Declare(tadah_cmake
  GIT_REPOSITORY https://github.com/tadah-project/cmake-tools.git
  GIT_TAG        v1-dev)          # the Tadah modules' development pin
FetchContent_MakeAvailable(tadah_cmake)

target_link_libraries(myexe PRIVATE tadah::compile_options)
```

The Tadah modules fetch cmake-tools at `v1-dev` during development. A
release tags cmake-tools (`v1.6.0`) and pins that tag in all seven modules
in the release commit (`Tadah.MLIP/RELEASING.md`).

## Git base address

The Tadah modules fetch cmake-tools and each other as
`${TADAH_GIT_BASE}/<repository>.git`. `TADAH_GIT_BASE` is set by the
block of `testing/tadah_git_base_block.cmake`, which each module's
`CMakeLists.txt` carries unchanged before its first `FetchContent_Declare`
(it is needed before cmake-tools itself is fetched):
`-DTADAH_GIT_BASE=<address>`, else the environment variable of that name
when it is set and not empty, else `https://github.com/tadah-project`.
`tadah_add_git_base_check(<name>)` (`cmake/TadahTesting.cmake`) adds the
test `<name>_git_base` (label `cmake`), which checks the block and the
addresses.

## Branch detection

`tadah_get_git_branch(<var>)` (`cmake/TadahDevelop.cmake`) names the git
ref every Tadah module fetches its sibling modules at
(`GIT_TAG ${TADAH_GIT_BRANCH}`). A value already set, such as
`-DTADAH_GIT_BRANCH=v1.6.0`, is kept. Otherwise the first of these is used:

1. `$CI_COMMIT_REF_NAME` (a CI pipeline that sets it: the branch, or the
   tag of a tag pipeline);
2. `$READTHEDOCS_GIT_IDENTIFIER` (Read the Docs: the branch or tag it
   checked out; its `latest` and `stable` versions are named after no ref),
   then `$READTHEDOCS_VERSION_NAME` and `$READTHEDOCS_VERSION`;
3. the upstream branch of the checkout of the top-level project
   (`origin/v1-dev` gives `v1-dev`), else its local branch. The top-level
   project is the one CMake was started on: `Tadah.MLIP`, a module built on
   its own, or LAMMPS itself for the `pair_style tadah` build, where the
   LAMMPS checkout's branch is not a Tadah branch.

A CI or Read the Docs variable counts only when it is set and not empty; one
that is set but empty names no ref and is skipped, with a status message.
A release tarball or a detached HEAD has no branch: the function warns and
leaves the variable empty. Pass `-DTADAH_GIT_BRANCH=<branch or tag>` (for a
release tarball, its tag, e.g. `v1.6.0`) so that the modules are fetched at
the matching ref. The docs build looks up the CORE and HPO sources with the
same variables, in the same order and by the same rule
(`Tadah.MLIP/docs/source/config/source_locator.py`); Tadah.MLIP's
`tadah_git_branch_*` tests check both.

## Removed build options

`cmake/TadahCompileOptions.cmake` stops the configure step when a removed
option is switched on. Every Tadah module fetches cmake-tools, and so runs
this file, before it declares any target of its own, so the check covers the
umbrella build, a standalone build of any module and the LAMMPS plugin build
alike. Setting a removed option to `OFF` (as older presets do) is accepted.

| Option | Removed in | Use instead |
|---|---|---|
| `TADAH_BUILD_MPI` | 1.6.0 (the legacy ScaLAPACK training build, `tadah_mpi`) | `TADAH_HPO_MPI` for MPI-parallel `TADAH:ANNEAL_PT` / `ANNEAL_PA` in HPO (needs `TADAH_ENABLE_NFP=ON` and an MPI build of LAMMPS); OpenMP threads for training |

## Testing

Every Tadah module builds its Catch2 (v2) unit tests the same way. Tests are
built when `BUILD_TESTING` is ON (default OFF; see `cmake/TadahTesting.cmake`
for `tadah_declare_testing()` and `tadah_add_tests()`). A module's
`tests/CMakeLists.txt`:

```cmake
tadah_setup_tests()             # Catch2 target; testing/ on CMAKE_MODULE_PATH

file(GLOB tests_src CONFIGURE_DEPENDS "*.cpp")
add_executable(test_foo ${tests_src})
target_link_libraries(test_foo PRIVATE Catch2 tadah.foo)

include(Catch)                  # testing/Catch.cmake
catch_discover_tests(test_foo)  # one CTest entry per test case
```

- `tadah_setup_tests()` creates the `Catch2` INTERFACE target (once per
  build) with `testing/` as its include directory, so sources use
  `#include "catch2/catch.hpp"` (Catch2 v2.13.7, shipped here), and puts
  `testing/` on `CMAKE_MODULE_PATH` for `include(Catch)`. Modules do not
  vendor `catch.hpp`, `Catch.cmake` or `CatchAddTests.cmake`.
- `catch_discover_tests()` lists the executable's test cases after every link
  (`--list-test-names-only`) and registers one CTest entry per non-hidden
  case. The discovery script, `testing/CatchAddTests.cmake`, guarantees:
  - names are taken verbatim: `;`, `[`, `]`, `,`, `\`, quotes, leading
    whitespace and a leading `#`, `~` or `-` in a name neither merge nor
    split entries (CRLF output is accepted);
  - each entry's Catch test spec (the name in double quotes, with `\`, `,`,
    `"` and `@` escaped; the last keeps a tag alias `[@name]` in a name from
    being expanded) selects exactly its own test case;
  - each entry passes `--warn NoTests`, so a spec that matches nothing fails
    instead of passing silently. Catch accepts `--warn` only once, so
    `EXTRA_ARGS` containing `--warn` or `-w` stop the build;
  - the listing is checked against Catch's own count (its exit status,
    which the OS truncates modulo 256); a crash or a disagreement stops the
    build, and "no tests" is decided from the parsed names;
  - a name Catch v2 cannot select on its own stops the build with a message
    naming the test case: a leading or trailing `*` (always a wildcard), a
    blank name, or a name that differs from another test case's name only
    in letter case or surrounding whitespace. Hidden test cases count here,
    because a name spec also selects them. Rename the test case.
- `catch_discover_tests()` passes its options to the script exactly (a
  `TEST_PREFIX` may end in a space), and a change to the script relinks the
  test executables so their tests are discovered again.
- Hidden test cases (`[.]`, `[.tag]`, `[!benchmark]`) are not discovered. To
  run them under CTest, add an explicit entry, which also passes
  `--warn NoTests`:

  ```cmake
  add_test(NAME foo_slow COMMAND test_foo "[.slow]" --warn NoTests)
  ```

### Skipped tests: the TADAH-SKIP marker

Catch2 v2 has no skip: a test case that returns early because an optional
input is absent (an external data file, an environment variable) passes,
and CTest counts a check that checked nothing. Such a guard calls
`TADAH_SKIP` from `testing/tadah_test_skip.hpp` (on the `Catch2` target's
include path) before the test case's first check, directly in the
`TEST_CASE` body and outside every `SECTION`:

```cpp
#include "tadah_test_skip.hpp"  // before (or instead of) catch2/catch.hpp

TEST_CASE("uses an optional database", "[db]") {
  std::ifstream in(path);
  if (!in.is_open()) {
    TADAH_SKIP("database not found at " << path);
  }
  ...
}
```

`TADAH_SKIP(<reason>)` records the reason as a Catch warning, records the
skip request and returns from the test case. When the test case ends
without a failed assertion, a Catch listener that the header registers
writes the line `TADAH-SKIP: <reason>` to standard error. Every test that
`catch_discover_tests()` registers has the `SKIP_REGULAR_EXPRESSION`
`TADAH_TEST_SKIP_REGULAR_EXPRESSION` (`testing/TadahTestSkip.cmake`), which
matches that line at the start of a line, so CTest reports the test as
**Skipped** although it exits with 0.

CTest has the `SKIP_REGULAR_EXPRESSION` test property from 3.16; an older
CTest ignores it and reports a skipped test as Passed. The tests therefore
need CMake 3.16: `TadahTestSkip.cmake`, which `include(Catch)` and the
discovery script include, stops the configure step on an older CMake with
a message that says so. A module still builds without its tests
(`BUILD_TESTING` OFF) on the CMake its `cmake_minimum_required()` names
(3.15).

- A skip request never hides a failure of the same test case: a check that
  failed before `TADAH_SKIP`, in another `SECTION`, or after `TADAH_SKIP`
  returned from a helper function or lambda (it returns only from that
  function), leaves the test case *Failed*, and no marker is written.
  Checks that run after the request and pass are not reported: the test
  case is Skipped.
- Include `tadah_test_skip.hpp` before `catch2/catch.hpp`, or instead of it
  (it includes Catch itself): its listener needs Catch's external
  interfaces, which it switches on with `CATCH_CONFIG_EXTERNAL_INTERFACES`.
  Included after a plain `catch2/catch.hpp`, it stops the build with a
  message saying so. It registers its listener once per test binary,
  whatever the number of translation units that include it.
- The line is written through C stdio's `stderr`, which the Catch reporters
  that capture a test case's output (`junit`, `xml`) do not redirect, so
  the skip reaches CTest with any reporter.
- Nothing else may print a line that starts with `TADAH-SKIP: `: CTest
  reports an entry whose output has such a line as Skipped, whatever its
  exit status. The text inside a line, or quoted in a failed assertion,
  does not count.
- `PROPERTIES` of `catch_discover_tests()` that set
  `SKIP_REGULAR_EXPRESSION` would replace the marker's; they stop the
  build.
- An explicit `add_test()` entry that runs exactly **one** test case opts
  in after `include(Catch)`:

  ```cmake
  set_tests_properties(foo_oracle PROPERTIES
    SKIP_REGULAR_EXPRESSION "${TADAH_TEST_SKIP_REGULAR_EXPRESSION}")
  ```

  Never on an entry that runs several test cases (a tag): the marker of one
  would report the whole entry as Skipped, failures of the others included.
- A script entry (not Catch) either exits with a code of its own and sets
  `SKIP_RETURN_CODE`, as HPO's `hpo_strategy_matrix` does with 77, or
  prints a line that its own `SKIP_REGULAR_EXPRESSION` matches.

The regression tests for this machinery live in the umbrella repository,
`Tadah.MLIP/tests/catch_discovery` (`ctest -R tadah_catch`;
`tadah_catch_skip_cmake_minimum` checks the CMake 3.16 refusal).

### Header self-containment: `tadah_add_header_check`

`tadah_add_header_check(<target> [HEADER_DIRS <dir>...])`
(`cmake/TadahTesting.cmake`) checks that every header of a module compiles
on its own: each module's `tests/CMakeLists.txt` calls it once for its
library (Tadah.MLIP for the `tadah` executable).

```cmake
tadah_add_header_check(tadah.foo)    # the headers of include/
tadah_add_header_check(tadah.core HEADER_DIRS
  ${PROJECT_SOURCE_DIR}/include ${PROJECT_BINARY_DIR}/generated)
```

- Every `*.h` and `*.hpp` under the `HEADER_DIRS` (default: the `include/`
  directory next to the `CMakeLists.txt` that defines the target) gets a
  generated translation unit that includes it twice, by its path relative
  to its directory (`#include <tadah/foo/bar.h>`): the header must compile
  with nothing included before it, and the second inclusion must be
  harmless (an include guard or `#pragma once`). A header that uses a type
  without including its header, or has no guard, fails. The headers are
  globbed with `CONFIGURE_DEPENDS`, so a new header is checked without
  being registered; the same relative path in two of the `HEADER_DIRS` is
  a configure error.
- The translation units compile with what a consumer of the library gets:
  they link the target, so its `PUBLIC` and `INTERFACE` include
  directories, definitions and options apply, and its `PRIVATE` ones do
  not. An executable cannot be linked; its own include directories,
  definitions and options are used instead. The definitions of the calling
  directory (`add_compile_definitions` of the module) and the target's C++
  standard apply too. Each translation unit also has its own header's
  `HEADER_DIRS` entry as an include directory, searched first, and no
  other entry: a header that needs a directory the target keeps `PRIVATE`
  fails, even when that directory is another of the `HEADER_DIRS` (CORE's
  `generated/`, whose blueprint header users of `tadah.core` in the build
  tree cannot include).
- They form the object library `<target>_header_check`, which is not part
  of `all`: a header that fails the check fails a test, not the build. The
  CTest entry `<target>_headers` (label `headers`, so `ctest -L headers`
  runs every module's) builds it with `cmake --build`, keeping going past
  a failed header with the Makefile and Ninja generators, so its output
  names every header that fails. It builds on as many processes as the
  machine has physical cores, or on `TADAH_HEADER_CHECK_JOBS` (a whole
  number, 1 or more, set when configuring:
  `-DTADAH_HEADER_CHECK_JOBS=4`), which is also its CTest `PROCESSORS`;
  another value is a configure error. The object files stay in the build
  tree: a rerun compiles only the headers that changed or include one that
  changed.

The installed tree is not checked: a header can still need a third-party
header that the build has and the installation does not.
`Tadah.MLIP/tests/header_check` tests the function (`ctest -R
tadah_header_check`), its refusals included.

### Text check: `tadah_add_text_check`

`tadah_add_text_check(<name> [ROOT <dir>])` (`cmake/TadahTesting.cmake`)
adds the CTest entry `<name>_text` (label `text`, so `ctest -L text` runs
every module's), which reads the text files of a repository and fails on
each line that carries a person's initials, a reference to co-workers, a
decision attributed to the code's owner, the name or state directory of an
AI tool, a note kept outside the repository, filler wording, or the
address of the development host, naming the file, the rule and the line.
In published text (the `.rst`, `.md` and `.txt` files under `docs/` and
`examples/`, and every `README*`, `CHANGELOG.md`, `CITATION.*`,
`CONTRIBUTING.md`, `CODE_OF_CONDUCT.md` and `SECURITY.md`) the name of the
development host's software is refused as well. Each module's `tests/CMakeLists.txt` calls it
once for its repository (`ROOT` defaults to the project's source
directory); Tadah.MLIP also checks cmake-tools.

```cmake
tadah_add_text_check(tadah.foo)                          # this repository
tadah_add_text_check(cmake-tools ROOT ${tadah_cmake_SOURCE_DIR})
```

- `testing/tadah_text_check.cmake` holds the rules (each pattern writes one
  letter in brackets, so the script does not match itself), the file names
  it reads (sources, headers, CMake, Markdown, reStructuredText, TOML,
  Python, shell, configuration and potential files, `.gitignore`, and files
  without an extension of at most 1 MiB) and the directories it skips at
  any depth (`.git`, `external`, `third_party`, `_deps`, `catch2`, `build*`,
  `cmake-build-*`).
- When CMake finds git and the root is the top of a git work tree (a clone,
  a FetchContent checkout), the files read are those git tracks or would
  track (`git ls-files --cached --others --exclude-standard`): what a build
  or a run writes into the tree and git ignores is not read. Otherwise (a
  source tarball, a directory inside a work tree) every file under the root
  is.
- The function refuses an unexpected argument, `ROOT` without a value and a
  `ROOT` that is not a directory.

`Tadah.MLIP/tests/text_check` tests the function and the script (`ctest -R
tadah_text_check_tool`), its refusals included; `Tadah.MLIP/tests/git_base`
tests the git base address check (`ctest -R tadah_git_base_check_tool`).
