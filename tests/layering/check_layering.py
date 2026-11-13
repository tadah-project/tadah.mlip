#!/usr/bin/env python3
"""No file of the prediction side includes a header of what lies above it.

    check_layering.py <source directory>
    check_layering.py --self-test <scratch directory>

The prediction side is four directories, in this order: core, models, md,
lammps. Each may include its own headers and those of the directories before
it. Every other directory of the repository lies above all four: mlip, hpo
and the application (src/, include/ and external/ at the root). A file of
the four that includes a header from above cannot be built without it.

Checked: every C and C++ header and source of the four directories (also a
template of one, *.in), in any subdirectory but <directory>/tests (the tests
of a module may include what they need) and <directory>/external (code of
others, whose includes depend on the platform).

Each #include outside a comment is looked up as a compiler would:
  - a quoted one first beside the including file;
  - then under every include directory of the repository: include, src,
    external, external/<name> and external/<name>/include of each top-level
    directory and of the root, and lammps/ML-TADAH.
A header generated from a template counts as the template (<name>.in). An
include found nowhere in the repository belongs to the compiler, the system
or LAMMPS and is not judged. One found in the including file's directory or
in one before it is allowed, wherever else it is found too. Any other is
reported as
``<file>:<line>: includes <header>: <path found>, of <owner>, above <directory>``
and the exit status is 1; otherwise the files and includes checked are
counted and the status is 0. A directory of the four without a file to check
fails as well.

--self-test writes a small tree with includes of each kind, allowed and not,
and fails unless exactly the planted ones are reported.
"""

import os
import re
import shutil
import sys

LAYERS = ["core", "models", "md", "lammps"]
SUFFIXES = (".h", ".hh", ".hpp", ".hxx", ".c", ".cc", ".cpp", ".cxx",
            ".ipp", ".tpp", ".inl")
NOT_CHECKED = ("tests", "external")

# A raw string, a comment, a string or a character literal: comments are
# blanked, the others kept (an include's quoted name is a string).
TOKEN = re.compile(
    r'R"([^()\\\s]{0,16})\(.*?\)\1"|//[^\n]*|/\*.*?\*/'
    r'|"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'', re.S)
INCLUDE = re.compile(r'^[ \t]*#[ \t]*include(?:_next)?[ \t]*([<"])([^>"\n]+)[>"]',
                     re.M)


def without_comments(text):
    """`text` with each comment replaced by its line breaks."""
    def blank(match):
        token = match.group(0)
        if token.startswith("//") or token.startswith("/*"):
            return " " + "\n" * token.count("\n")
        return token
    return TOKEN.sub(blank, text)


def include_directories(root):
    """The directories an include is looked up under."""
    found = []
    tops = [""] + sorted(d for d in os.listdir(root)
                         if not d.startswith(".")
                         and os.path.isdir(os.path.join(root, d)))
    for top in tops:
        base = os.path.join(root, top)
        found += [os.path.join(base, "include"), os.path.join(base, "src")]
        external = os.path.join(base, "external")
        found.append(external)
        if os.path.isdir(external):
            for name in sorted(os.listdir(external)):
                found += [os.path.join(external, name),
                          os.path.join(external, name, "include")]
    found.append(os.path.join(root, "lammps", "ML-TADAH"))
    return [d for d in found if os.path.isdir(d)]


def checked_files(root, layer):
    """The headers and sources of `layer` that the rule covers."""
    files = []
    for folder, dirs, names in os.walk(os.path.join(root, layer)):
        if os.path.relpath(folder, root) == layer:
            dirs[:] = [d for d in dirs if d not in NOT_CHECKED]
        dirs.sort()
        for name in sorted(names):
            stem = name[:-3] if name.endswith(".in") else name
            if stem.endswith(SUFFIXES):
                files.append(os.path.join(folder, name))
    return files


def owner(path, root):
    """The top-level directory `path` lies in: "" for a file directly in
    the root, None for one outside the repository."""
    rel = os.path.relpath(path, root)
    if rel.startswith(os.pardir):
        return None
    return rel.split(os.sep)[0] if os.sep in rel else ""


def check(root):
    """(problems, files checked, includes checked) of the tree at `root`."""
    roots = include_directories(root)
    problems, n_files, n_includes = [], 0, 0
    for rank, layer in enumerate(LAYERS):
        allowed = LAYERS[:rank + 1]
        files = checked_files(root, layer)
        if not files:
            problems.append("%s: no header or source to check" % layer)
        n_files += len(files)
        for path in files:
            with open(path, encoding="utf-8", errors="replace") as f:
                text = without_comments(f.read())
            for match in INCLUDE.finditer(text):
                n_includes += 1
                quote, header = match.group(1), match.group(2).strip()
                places = [os.path.dirname(path)] if quote == '"' else []
                found = []
                for place in places + roots:
                    candidate = os.path.normpath(os.path.join(place, header))
                    if os.path.isfile(candidate) or os.path.isfile(candidate + ".in"):
                        found.append(candidate)
                owners = [owner(c, root) for c in found]
                above = [(c, o) for c, o in zip(found, owners)
                         if o is not None and o not in allowed]
                if above and not any(o in allowed for o in owners):
                    line = text.count("\n", 0, match.start(1)) + 1
                    problems.append(
                        "%s:%d: includes %s: %s, of %s, above %s"
                        % (os.path.relpath(path, root), line, header,
                           os.path.relpath(above[0][0], root),
                           above[0][1] or "the root", layer))
    return problems, n_files, n_includes


# The self-test's tree: path -> content. EXPECTED: the includes it must
# report, as "<file>:<line>: includes <header>".
TREE = {
    "core/include/tadah/core/a.h": "#pragma once\n#include <tadah/models/m.h>\n",
    "core/src/a.cpp": '#include "tadah/core/a.h"\n#include <vector>\n'
                      "// #include <tadah/mlip/t.h>\n/* a note\n"
                      "#include <tadah/mlip/t.h>\n*/\n#include <shared.h>\n"
                      'const char* s = R"x(/*)x";\n#include <tadah/hpo/h.h>\n',
    "core/tests/t.cpp": "#include <tadah/mlip/t.h>\n",
    "core/external/x/shared.h": "#include <tadah/mlip/t.h>\n",
    "models/include/tadah/models/m.h": "#include <tadah/core/a.h>\n",
    "models/src/m.cpp": '#include "local.h"\n  #  include <tadah/mlip/t.h>\n',
    "models/src/local.h": "",
    "md/include/tadah/md/d.h": "#include <tadah/models/m.h>\n",
    "md/src/d.cpp": '#include <tadah/md/d.h>\n'
                    '#include "../../hpo/include/tadah/hpo/h.h"\n',
    "lammps/ML-TADAH/p.cpp": '#include "atom.h"\n#include <tadah/md/d.h>\n'
                             "#include <tadah.mlip/cli/c.h>\n#include <gen.hpp>\n"
                             "#include <shared.h>\n#include <only_above.h>\n",
    "mlip/include/tadah/mlip/t.h": "", "mlip/include/tadah/mlip/atom.h": "",
    "mlip/external/x/shared.h": "", "mlip/external/y/only_above.h": "",
    "hpo/include/tadah/hpo/h.h": "", "hpo/src/gen.hpp.in": "",
    "include/tadah.mlip/cli/c.h": "",
}
EXPECTED = [
    "core/include/tadah/core/a.h:2: includes tadah/models/m.h",
    "core/src/a.cpp:9: includes tadah/hpo/h.h",
    "models/src/m.cpp:2: includes tadah/mlip/t.h",
    "md/src/d.cpp:2: includes ../../hpo/include/tadah/hpo/h.h",
    "lammps/ML-TADAH/p.cpp:3: includes tadah.mlip/cli/c.h",
    "lammps/ML-TADAH/p.cpp:4: includes gen.hpp",
    "lammps/ML-TADAH/p.cpp:6: includes only_above.h",
]


def self_test(scratch):
    tree = os.path.join(os.path.abspath(scratch), "tree")
    shutil.rmtree(tree, ignore_errors=True)
    for rel, content in TREE.items():
        path = os.path.join(tree, *rel.split("/"))
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w", encoding="utf-8") as f:
            f.write(content)
    problems, n_files, _ = check(tree)
    for p in problems:
        print(p)
    reported = [": ".join(p.split(": ")[:2]) for p in problems]
    if sorted(reported) != sorted(EXPECTED) or n_files != 8:
        print("FAIL: the self-test expects 8 files checked and exactly:")
        for e in EXPECTED:
            print("  " + e)
        return 1
    shutil.rmtree(os.path.join(tree, "md"))
    if "md: no header or source to check" not in check(tree)[0]:
        print("FAIL: a directory without a file to check is not reported")
        return 1
    print("self-test: the %d planted includes are reported, and no other"
          % len(EXPECTED))
    return 0


def main():
    args = sys.argv[1:]
    if len(args) == 2 and args[0] == "--self-test":
        return self_test(args[1])
    if len(args) != 1 or args[0].startswith("-"):
        raise SystemExit("usage: check_layering.py <source directory> | "
                         "--self-test <scratch directory>")
    root = os.path.abspath(args[0])
    problems, n_files, n_includes = check(root)
    for p in problems:
        print(p)
    if problems:
        print("FAIL: %d problem(s): a file of %s includes a header of a "
              "directory above its own" % (len(problems), ", ".join(LAYERS)))
        return 1
    print("checked %d includes of %d files of %s: none is of a directory "
          "above the including file's" % (n_includes, n_files, ", ".join(LAYERS)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
