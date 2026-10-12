#!/usr/bin/env python3
"""No CMake file of Tadah.MLIP needs a CMake newer than the one it runs on.

    check_cmake_features.py <Tadah.MLIP source directory>

Every CMake file of the umbrella project -- CMakeLists.txt, cmake/*.cmake,
and under tests/ every CMakeLists.txt, *.cmake and *.cmake.in -- is checked
for two things.

1. It uses nothing newer than its minimum: the version of its own
   cmake_minimum_required() (the highest, when it has several: a script may
   check the version before it declares the one its body needs), else that
   of the nearest CMakeLists.txt in its directory or above it that has one
   (Tadah.MLIP/CMakeLists.txt: 3.15; tests/CMakeLists.txt: 3.16). A command,
   sub-command, command-line option or test property that CMake added later
   fails there, or is ignored, and nothing says why ("Unknown argument
   --fresh"; a SKIP_REGULAR_EXPRESSION that CTest 3.15 ignores reports a
   skipped test Passed). The table below lists those of them, with the
   version that added them; a use in a comment does not count, nor does a
   command or property named inside a quoted argument (a message's text); a
   command-line option counts quoted or not. Keyword arguments that an older
   CMake does not read are not listed (FetchContent_Declare's
   DOWNLOAD_EXTRACT_TIMESTAMP, 3.24: the projects here always give the
   source directory, FETCHCONTENT_SOURCE_DIR_<name>, so nothing reads it).

2. It accepts the CMake of what runs it: its first cmake_minimum_required()
   is at most the tests' minimum (that of tests/CMakeLists.txt) for a file
   under tests/, and at most the umbrella's for the others
   (tests/CMakeLists.txt itself declares the tests' minimum, and the
   umbrella the build's). A file that declares more stops on a CMake its
   build accepts ("CMake 3.18 or higher is required"), which does not say
   why; a script whose body needs more declares the CMake of what runs it
   first, checks the version and says why it skips or stops, and declares
   the version its body needs after that (header_check/check_header_check.cmake).

Each use above the file's minimum is reported as
``<path>:<line>: <feature> needs CMake <v>; the file's minimum is <m> (<from>)``,
each first declaration above what runs the file as
``<path>:<line>: cmake_minimum_required(VERSION <v>) is above the <m> of <from>``,
and the exit status is 1; otherwise the files checked are counted and the
status is 0 (a run that finds no file fails).
"""

import os
import re
import sys

# (pattern, CMake version that added it, what it is). A pattern that starts
# with '-' or '$' is a command-line option or a generator expression, looked
# for in quoted arguments too; the others are commands, looked for outside
# them.
FEATURES = [
    (r"--fresh\b", (3, 24), "cmake --fresh"),
    (r"--test-dir\b", (3, 20), "ctest --test-dir"),
    (r"--toolchain\b", (3, 21), "cmake --toolchain"),
    (r"--output-junit\b", (3, 21), "ctest --output-junit"),
    (r"--no-tests\b", (3, 17), "ctest --no-tests"),
    (r"\bstring\s*\(\s*JSON\b", (3, 19), "string(JSON)"),
    (r"\bstring\s*\(\s*HEX\b", (3, 18), "string(HEX)"),
    (r"\bcmake_path\s*\(", (3, 20), "cmake_path()"),
    (r"\bfile\s*\(\s*REAL_PATH\b", (3, 19), "file(REAL_PATH)"),
    (r"\bfile\s*\(\s*CHMOD(_RECURSE)?\b", (3, 19), "file(CHMOD)"),
    (r"\bfile\s*\(\s*CONFIGURE\b", (3, 18), "file(CONFIGURE)"),
    (r"\bfile\s*\(\s*ARCHIVE_(CREATE|EXTRACT)\b", (3, 18), "file(ARCHIVE_*)"),
    (r"\bcmake_language\s*\(", (3, 18), "cmake_language()"),
    (r"\bmessage\s*\(\s*CHECK_(START|PASS|FAIL)\b", (3, 17), "message(CHECK_*)"),
    (r"\bZIP_LISTS\b", (3, 17), "foreach(ZIP_LISTS)"),
    (r"\bCOMMAND_ERROR_IS_FATAL\b", (3, 19), "execute_process(COMMAND_ERROR_IS_FATAL)"),
    (r"\bblock\s*\(", (3, 25), "block()"),
    (r"\breturn\s*\(\s*PROPAGATE\b", (3, 25), "return(PROPAGATE)"),
    (r"\bPATH_EQUAL\b", (3, 24), "if(PATH_EQUAL)"),
    (r"\$<PATH:", (3, 24), "$<PATH:...>"),
    (r"-E\s+rm\b", (3, 17), "cmake -E rm"),
    (r"-E\s+cat\b", (3, 18), "cmake -E cat"),
    (r"-E\s+(true|false)\b", (3, 16), "cmake -E true/false"),
    (r"\bSKIP_REGULAR_EXPRESSION\b", (3, 16), "test property SKIP_REGULAR_EXPRESSION"),
    (r"\bRESOURCE_GROUPS\b", (3, 16), "test property RESOURCE_GROUPS"),
    (r"--log-level\b", (3, 16), "cmake --log-level"),
    (r"\bCMAKE_CURRENT_FUNCTION(_LIST_DIR|_LIST_FILE|_LIST_LINE)?\b", (3, 17),
     "CMAKE_CURRENT_FUNCTION*"),
    (r"--repeat\b", (3, 17), "ctest --repeat"),
    (r"\bECHO_(OUTPUT|ERROR)_VARIABLE\b", (3, 18), "execute_process(ECHO_*_VARIABLE)"),
    (r"\bCOMPARE\s+NATURAL\b", (3, 18), "list(SORT COMPARE NATURAL)"),
    (r"--stop-on-failure\b", (3, 18), "ctest --stop-on-failure"),
    (r"-E\s+create_hardlink\b", (3, 19), "cmake -E create_hardlink"),
]

MINIMUM = re.compile(r"\bcmake_minimum_required\s*\(\s*VERSION\s+([0-9]+(?:\.[0-9]+)*)")


def code_of(line):
    """The line without its comment: from a '#' outside a quoted argument,
    at the start of the line or after a blank (a '#' inside an unquoted
    argument, as in a regular expression, is kept)."""
    quoted = False
    prev = " "
    for i, ch in enumerate(line):
        if ch == '"' and prev != "\\":
            quoted = not quoted
        elif ch == "#" and not quoted and prev in " \t":
            return line[:i]
        prev = ch
    return line


def unquoted(code):
    """`code` with the text of its quoted arguments blanked out."""
    out = []
    quoted = False
    prev = " "
    for ch in code:
        if ch == '"' and prev != "\\":
            quoted = not quoted
            out.append(ch)
        else:
            out.append(" " if quoted else ch)
        prev = ch
    return "".join(out)


def version(text):
    return tuple(int(x) for x in text.split("."))


def shown(v):
    return ".".join(str(x) for x in v)


def declarations(path):
    """The (line number, version) of each cmake_minimum_required() of the
    file, in order."""
    found = []
    with open(path, encoding="utf-8") as f:
        for number, line in enumerate(f, start=1):
            m = MINIMUM.search(code_of(line))
            if m:
                found.append((number, version(m.group(1))))
    return found


def own_minimum(path):
    """The highest cmake_minimum_required() VERSION of the file, or None."""
    found = [v for _, v in declarations(path)]
    return max(found) if found else None


def cmake_files(root):
    files = [os.path.join(root, "CMakeLists.txt")]
    cmake_dir = os.path.join(root, "cmake")
    for dirpath, _, names in os.walk(cmake_dir):
        files += [os.path.join(dirpath, n) for n in names if n.endswith(".cmake")]
    for dirpath, _, names in os.walk(os.path.join(root, "tests")):
        for n in names:
            if n == "CMakeLists.txt" or n.endswith(".cmake") or n.endswith(".cmake.in"):
                files.append(os.path.join(dirpath, n))
    return sorted(set(files))


def governing_minimum(path, root, own_first=True):
    """(version, the file that declares it) for `path`: its own minimum,
    else (or with own_first False, always) that of the nearest
    CMakeLists.txt in its directory or above it, other than itself, that
    declares one."""
    own = own_minimum(path) if own_first else None
    if own is not None:
        return own, path
    d = os.path.dirname(path)
    while True:
        lists = os.path.join(d, "CMakeLists.txt")
        if lists != path and os.path.isfile(lists):
            v = own_minimum(lists)
            if v is not None:
                return v, lists
        if os.path.samefile(d, root):
            break
        d = os.path.dirname(d)
    raise SystemExit("check_cmake_features.py: no cmake_minimum_required() "
                     "governs " + path)


def runner_minimum(path, root):
    """(version, the file that declares it) of what runs `path`: the tests'
    minimum (tests/CMakeLists.txt's, else the umbrella's) for a file under
    tests/; the umbrella's for the others; None for the two files that
    declare these, the umbrella's CMakeLists.txt and tests/CMakeLists.txt."""
    umbrella = os.path.join(root, "CMakeLists.txt")
    tests_dir = os.path.join(root, "tests")
    tests_lists = os.path.join(tests_dir, "CMakeLists.txt")
    for declaring in (umbrella, tests_lists):
        if os.path.isfile(declaring) and os.path.samefile(path, declaring):
            return None
    if os.path.isfile(tests_lists) and \
            os.path.commonpath([path, tests_dir]) == tests_dir:
        return governing_minimum(tests_lists, root)
    return governing_minimum(umbrella, root)


def main():
    if len(sys.argv) != 2:
        raise SystemExit("usage: check_cmake_features.py <Tadah.MLIP source directory>")
    root = os.path.abspath(sys.argv[1])
    files = cmake_files(root)
    if not os.path.isfile(files[0]) or len(files) < 2:
        print("FAIL: no CMake files found under " + root)
        return 1
    problems = []
    for path in files:
        minimum, origin = governing_minimum(path, root)
        rel = os.path.relpath(path, root)
        source = os.path.relpath(origin, root)
        runner = runner_minimum(path, root)
        decls = declarations(path)
        if runner is not None and decls and decls[0][1] > runner[0]:
            problems.append(
                "%s:%d: cmake_minimum_required(VERSION %s) is above the %s of %s"
                % (rel, decls[0][0], shown(decls[0][1]), shown(runner[0]),
                   os.path.relpath(runner[1], root)))
        with open(path, encoding="utf-8") as f:
            for number, line in enumerate(f, start=1):
                code = code_of(line)
                bare = unquoted(code)
                for pattern, needs, what in FEATURES:
                    text = code if pattern[0] in "-$" else bare
                    if needs > minimum and re.search(pattern, text):
                        problems.append(
                            "%s:%d: %s needs CMake %s; the file's minimum is %s (%s)"
                            % (rel, number, what, shown(needs), shown(minimum),
                               "its own" if source == rel else source))
    for p in problems:
        print(p)
    if problems:
        print("FAIL: %d use(s) of CMake newer than the file's minimum, or "
              "declaration(s) above what runs the file" % len(problems))
        return 1
    print("checked %d CMake files: none uses CMake newer than its minimum, "
          "and none declares more than what runs it" % len(files))
    return 0


if __name__ == "__main__":
    sys.exit(main())
