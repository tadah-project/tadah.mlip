#!/usr/bin/env python3
"""The LAMMPS package: its writer, and a LAMMPS build from the written tree.

    check_export.py tool --work DIR
    check_export.py configure|build --lammps DIR --work DIR --cmake EXE
                          --generator NAME --jobs N [--cxx EXE]

Both run export/write_package.py of the repository that holds this file.

tool: the writer on a throw-away repository in DIR. A manifest with an entry
of each kind gives exactly the expected tree; a pattern that matches no
file, a file taken in twice, a file left out that is not in, two files with
one place, a line that is not an entry and an output directory that is not
empty each stop it with exit status 1, the problem named, nothing written;
so do a file whose place is below the place of another, a file named twice
by two spellings of its path, and a pattern that reaches a symbolic link.

build: writes the package of this repository into DIR/package, then
configures the LAMMPS sources in --lammps with the package exactly as a
user does (PKG_ML-TADAH, CMAKE_PROJECT_INCLUDE and TADAH_LAMMPS_SOURCE_DIR
name the written tree) in the new build directory DIR/lammps, and builds
the target lammps, the LAMMPS library with the pair style, on N processes.
No other LAMMPS package is on and nothing may be downloaded
(FETCHCONTENT_FULLY_DISCONNECTED). The build type is Debug, which compiles
in the shortest time; the check is that the library builds, not how fast
it runs.

It fails when the tree lacks README.md, LICENSE or CMakeLists.txt at its
top, when the configure or the build step fails, and when either read a
file of this repository outside the written tree: every file the build
wrote but its objects and libraries (the cache, the generated build files,
the dependency files of every object) is searched for paths into the
repository, and each must lie in the written tree or the build directory.
The records are searched after each step. Those of the configure step must
name every source file of the package and its top build file, and those of
the build step must add headers of the package; otherwise the check could
not tell what was read, and fails.

It prints the files and bytes of the package, the times of the two steps,
and the files of the package that the build did not read.

configure: as build, in DIR too, without the build step: the check of what
the configure step read. The headers are read by the build step only.
"""

import argparse
import os
import re
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
REPOSITORY = os.path.dirname(os.path.dirname(HERE))
WRITER = os.path.join(REPOSITORY, "export", "write_package.py")
NOT_SEARCHED = (".o", ".obj", ".a", ".so", ".dylib", ".gch")
PATH_END = rb"[^\s\"';:,()<>|\\\x00]*"


def run(command, **kwargs):
    """(exit status, output) of `command`."""
    done = subprocess.run(command, stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT, **kwargs)
    return done.returncode, done.stdout.decode("utf-8", "replace")


def new_directory(path):
    shutil.rmtree(path, ignore_errors=True)
    os.makedirs(path)
    return path


def files_of(root):
    """The files under `root`, as sorted paths with `/`."""
    found = []
    for folder, _, names in os.walk(root):
        for name in names:
            rel = os.path.relpath(os.path.join(folder, name), root)
            found.append(rel.replace(os.sep, "/"))
    return sorted(found)


# ---------------------------------------------------------------- tool

# The throw-away repository: path -> content.
TREE = {
    "top.txt": "top\n",
    "a/one.h": "one\n", "a/two.h": "two\n", "a/skip.txt": "skip\n",
    "a/deep/er/three.h": "three\n", "a/deep/four.cpp": "four\n",
    "b/x.cmake": "x\n", "b/y.cmake": "y\n", "b/sub/z.cmake": "z\n",
    "d/one.h": "another one\n",
    "pkg/README.md": "readme\n", "more": "b/sub/*.cmake   # by a second file\n",
}
GOOD = """# a comment
top.txt
a/**/*.h          # at any depth
!a/two.h
a/deep/*.cpp      src
b/*.cmake         build/files
pkg/README.md     .
@more
"""
# place in the package -> path in the repository
EXPECTED = {
    "top.txt": "top.txt", "a/one.h": "a/one.h",
    "a/deep/er/three.h": "a/deep/er/three.h", "src/four.cpp": "a/deep/four.cpp",
    "build/files/x.cmake": "b/x.cmake", "build/files/y.cmake": "b/y.cmake",
    "README.md": "pkg/README.md", "b/sub/z.cmake": "b/sub/z.cmake",
}
# name -> (manifest, what the writer must say)
REFUSED = {
    "no_match": ("top.txt\na/*.hpp\n", "manifest:2: a/*.hpp matches no file"),
    "no_file": ("top.txt\na/three.h\n", "manifest:2: a/three.h matches no file"),
    "twice": ("a/*.h\na/**/*.h\n",
              "manifest:2: a/one.h is taken in already (manifest:1)"),
    "not_in": ("a/*.h\n!a/skip.txt\n",
               "manifest:2: a/skip.txt is left out but no entry above takes it in"),
    "one_place": ("a/one.h\nd/one.h a\n",
                  "manifest:2: a/one.h and d/one.h have the same place in the "
                  "package, a/one.h"),
    "three_words": ("top.txt a b\n", "manifest:1: not an entry: top.txt a b"),
    "outside": ("top.txt ../x\n", "manifest:1: ../x is not a directory of the package"),
    "other_file": ("top.txt\n@more2\n", "more2:1: c/* matches no file"),
    "empty": ("# nothing\n", "manifest: no entry"),
    "below_a_file": ("top.txt\na/one.h top.txt\n",
                     "manifest:2: top.txt/one.h, the place of a/one.h, is below "
                     "top.txt, the place of top.txt"),
    "same_file": ("top.txt\n./top.txt\n",
                  "manifest:2: top.txt is taken in already (manifest:1)"),
    "link_file": ("l/*.h\n", "manifest:1: l/linked.h is a symbolic link"),
    "link_named": ("l/linked.h\n", "manifest:1: l/linked.h is a symbolic link"),
    "link_below": ("l/**/*.h\n", "manifest:1: l/dir is a symbolic link"),
    "link_above": ("ldir/*.h\n", "manifest:1: ldir is a symbolic link"),
    "link_above_named": ("ldir/inner.h\n", "manifest:1: ldir is a symbolic link"),
}


def tool(work):
    source = new_directory(os.path.join(work, "repository"))
    for rel, content in TREE.items():
        path = os.path.join(source, *rel.split("/"))
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w", encoding="utf-8") as f:
            f.write(content)
    with open(os.path.join(source, "more2"), "w", encoding="utf-8") as f:
        f.write("c/*\n")
    # Symbolic links to a file and a directory outside the repository.
    outside = new_directory(os.path.join(work, "outside"))
    for name in ("secret.h", "inner.h"):
        with open(os.path.join(outside, name), "w", encoding="utf-8") as f:
            f.write("outside\n")
    os.makedirs(os.path.join(source, "l"))
    os.symlink(os.path.join(outside, "secret.h"), os.path.join(source, "l", "linked.h"))
    os.symlink(outside, os.path.join(source, "l", "dir"))
    os.symlink(outside, os.path.join(source, "ldir"))
    failures = []

    def write(name, manifest):
        with open(os.path.join(source, "manifest"), "w", encoding="utf-8") as f:
            f.write(manifest)
        output = os.path.join(work, "out_" + name)
        shutil.rmtree(output, ignore_errors=True)
        status, text = run([sys.executable, WRITER, output, "--source", source,
                            "--manifest", "manifest"])
        return output, status, text

    output, status, text = write("good", GOOD)
    got = files_of(output) if os.path.isdir(output) else []
    if status != 0 or got != sorted(EXPECTED):
        failures.append("good: exit %d, tree %s, expected %s\n%s"
                        % (status, got, sorted(EXPECTED), text))
    else:
        for place, origin in EXPECTED.items():
            with open(os.path.join(output, *place.split("/")), encoding="utf-8") as f:
                if f.read() != TREE[origin]:
                    failures.append("good: %s is not a copy of %s" % (place, origin))
        total = sum(len(TREE[o]) for o in EXPECTED.values())
        if not re.search(r"the package\s+%d files\s+%d bytes" % (len(EXPECTED), total),
                         text):
            failures.append("good: no line 'the package %d files %d bytes':\n%s"
                            % (len(EXPECTED), total, text))

    for name, (manifest, said) in REFUSED.items():
        output, status, text = write(name, manifest)
        if status != 1 or said not in text or os.path.exists(output):
            failures.append("%s: expected exit 1, '%s' and no output directory; "
                            "exit %d, directory %s:\n%s"
                            % (name, said, status,
                               "written" if os.path.exists(output) else "absent", text))

    # An output directory that holds a file is refused and left as it is.
    with open(os.path.join(source, "manifest"), "w", encoding="utf-8") as f:
        f.write(GOOD)
    output = new_directory(os.path.join(work, "out_occupied"))
    with open(os.path.join(output, "kept"), "w", encoding="utf-8") as f:
        f.write("kept\n")
    status, text = run([sys.executable, WRITER, output, "--source", source,
                        "--manifest", "manifest"])
    if status != 1 or "is not an empty directory" not in text or files_of(output) != ["kept"]:
        failures.append("occupied: expected exit 1, 'is not an empty directory' "
                        "and the directory untouched; exit %d, %s:\n%s"
                        % (status, files_of(output), text))

    for failure in failures:
        print("FAIL: " + failure)
    if failures:
        return 1
    print("the writer gives the expected tree and refuses the %d wrong inputs"
          % (len(REFUSED) + 1))
    return 0


# --------------------------------------------------------------- build

def ninja_paths(data):
    """The paths of Ninja's dependency log (.ninja_deps), one per line. The
    log is "# ninjadeps", a version and records, each after its 4-byte
    size: a path with its 4-byte number, or (top bit of the size set) the
    numbers of an object's dependencies. A path is not always followed by
    a byte that ends it, so the log is not searched as it is."""
    paths, at = [], 16
    while at + 4 <= len(data):
        size = int.from_bytes(data[at:at + 4], "little")
        at += 4
        if size & 0x80000000:
            at += size & 0x7FFFFFFF
        else:
            paths.append(data[at:at + size - 4].rstrip(b"\0"))
            at += size
    return b"\n".join(paths)


def paths_in(build, roots):
    """The paths under one of `roots` that the files of `build` name, each
    with the file that names it; objects and libraries are not read."""
    needles = [re.compile(re.escape(root.encode()) + rb"(?![A-Za-z0-9._+~@%-])"
                          + PATH_END) for root in roots]
    found = {}
    for folder, _, names in os.walk(build):
        for name in names:
            if name.endswith(NOT_SEARCHED):
                continue
            record = os.path.join(folder, name)
            if not os.path.isfile(record):
                continue
            with open(record, "rb") as f:
                data = f.read()
            if name == ".ninja_deps" and data.startswith(b"# ninjadeps\n"):
                data = ninja_paths(data)
            for needle in needles:
                for match in needle.finditer(data):
                    path = os.path.normpath(match.group(0).decode("utf-8", "replace"))
                    found.setdefault(path, record)
    return found


def inside(path, root):
    return path == root or path.startswith(root + os.sep)


def build(args, steps):
    work = os.path.abspath(args.work)
    new_directory(work)
    package = os.path.join(work, "package")
    lammps = os.path.join(work, "lammps")

    status, text = run([sys.executable, WRITER, package])
    print(text, end="")
    if status != 0:
        print("FAIL: export/write_package.py, exit %d" % status)
        return 1
    missing = [name for name in ("README.md", "LICENSE", "CMakeLists.txt")
               if not os.path.isfile(os.path.join(package, name))]
    if missing:
        print("FAIL: the package has no %s at its top" % ", ".join(missing))
        return 1

    configure = [
        args.cmake, "-S", os.path.join(args.lammps, "cmake"), "-B", lammps,
        "-G", args.generator,
        "-DCMAKE_BUILD_TYPE=Debug",
        "-DBUILD_MPI=OFF", "-DBUILD_OMP=OFF", "-DBUILD_SHARED_LIBS=OFF",
        "-DBUILD_TESTING=OFF", "-DWITH_JPEG=OFF", "-DWITH_PNG=OFF",
        "-DWITH_GZIP=OFF", "-DWITH_CURL=OFF", "-DWITH_FFMPEG=OFF",
        "-DDOWNLOAD_POTENTIALS=OFF", "-DFETCHCONTENT_FULLY_DISCONNECTED=ON",
        "-DPKG_ML-TADAH=ON",
        "-DCMAKE_PROJECT_INCLUDE=" + os.path.join(
            package, "integration", "TadahLAMMPSProjectInclude.cmake"),
        "-DTADAH_LAMMPS_SOURCE_DIR=" + package,
    ]
    if args.cxx:
        configure.append("-DCMAKE_CXX_COMPILER=" + args.cxx)
    commands = {"configure": configure,
                "build": [args.cmake, "--build", lammps, "--target", "lammps",
                          "--parallel", str(args.jobs)]}
    allowed = {package, os.path.realpath(package), lammps, os.path.realpath(lammps)}
    packages = {package, os.path.realpath(package)}
    repositories = {REPOSITORY, os.path.realpath(REPOSITORY)}
    held = files_of(package)
    read = set()
    for what in steps:
        start = time.time()
        status, text = run(commands[what])
        if status != 0:
            print(text)
            print("FAIL: the %s step of LAMMPS with the package, exit %d"
                  % (what, status))
            return 1
        print("%s: %.0f s%s" % (what, time.time() - start,
                                " on %d processes" % args.jobs
                                if what == "build" else ""))

        # What the step read, from every record the build wrote so far.
        before, read, outside = read, set(), []
        named = paths_in(lammps, repositories | packages)
        for path, record in sorted(named.items()):
            owner = [root for root in packages if inside(path, root)]
            if owner:
                if os.path.isfile(path):
                    read.add(os.path.relpath(path, owner[0]).replace(os.sep, "/"))
            elif not any(inside(path, root) for root in allowed):
                outside.append((path, record))
        for path, record in outside:
            print("FAIL: %s, outside the package, is named by %s" % (path, record))
        # The records of the configure step name the build files and the
        # sources; those of the build step add the headers the compiler read.
        if what == "configure":
            unseen = [f for f in held if f not in read
                      and (f.endswith(".cpp") or f == "CMakeLists.txt")]
        else:
            headers = [f for f in read - before if f.endswith((".h", ".hpp"))]
            unseen = [] if headers else ["a header"]
        for f in unseen:
            print("FAIL: no record of the %s step names %s of the package"
                  % (what, f))
        if outside or unseen:
            return 1
    if "build" not in steps:
        print("the configure step read no file of the repository outside the "
              "package (%d files)" % len(held))
        return 0
    not_read = [f for f in held if f not in read]
    print("the build read %d of the %d files of the package and no file of the "
          "repository outside it; not read: %s"
          % (len(held) - len(not_read), len(held), ", ".join(not_read) or "none"))
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("case", choices=["tool", "configure", "build"])
    parser.add_argument("--work", required=True)
    parser.add_argument("--lammps")
    parser.add_argument("--cmake")
    parser.add_argument("--generator")
    parser.add_argument("--jobs", type=int)
    parser.add_argument("--cxx")
    args = parser.parse_args()
    if args.case == "tool":
        return tool(os.path.abspath(args.work))
    for name in ("lammps", "cmake", "generator", "jobs"):
        if not getattr(args, name):
            parser.error(args.case + " needs --" + name)
    return build(args, ["configure", "build"] if args.case == "build"
                 else ["configure"])


if __name__ == "__main__":
    sys.exit(main())
