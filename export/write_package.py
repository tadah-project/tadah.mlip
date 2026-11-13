#!/usr/bin/env python3
"""Writes the LAMMPS package of Tadah! from the manifest.

    write_package.py <output directory> [--source DIR] [--manifest FILE]

The package is the pair style `tadah` with the parts of core, models and md
it is built from: a tree that a LAMMPS configure step is given in place of
the directory lammps of this repository. Every file of it is a copy of a
file of this repository; the manifest (export/manifest) lists them. The
package is written anew for each release and never edited.

<output directory> must not exist or must be empty. --source is the
repository (default: the one that holds this script), --manifest the list
(default: export/manifest of the repository).

The manifest: one entry per line; `#` starts a comment.

    PATTERN          the files that match keep their path in the package
    PATTERN DIR      the files that match go into the directory DIR of the
                     package under their own names (`.` is its top)
    !PATTERN         the files that match are left out, though an entry
                     above them takes them in
    @FILE            the entries of FILE, a path of the repository

A pattern is a path of the repository in which `*` stands for any run of
characters within one name and `**` for any run of characters, `/`
included (`a/**/*.h`: the .h files under a/ at any depth).

The script stops, with exit status 1 and every problem named by its line
of the manifest, when a pattern matches no file, a file is taken in by two
entries, an entry leaves out a file that is not in, two files have the
same place in the package, the place of a file is below the place of
another, a pattern reaches a symbolic link (the package holds copies of
files of the repository only: a link is not followed), or a line is not an
entry. Nothing is written then. Otherwise it writes the tree and prints
the files and bytes of each top-level name of the package and of the whole.
"""

import argparse
import os
import re
import shutil
import sys


class ManifestError(Exception):
    """The problems of a manifest, one per line."""


def pattern_regex(pattern):
    """The regular expression of a manifest pattern."""
    out = []
    i = 0
    while i < len(pattern):
        if pattern.startswith("**/", i):
            out.append("(?:.*/)?")
            i += 3
        elif pattern.startswith("**", i):
            out.append(".*")
            i += 2
        elif pattern[i] == "*":
            out.append("[^/]*")
            i += 1
        else:
            out.append(re.escape(pattern[i]))
            i += 1
    return re.compile("".join(out) + r"\Z")


def may_hold(parts, folders):
    """Whether a file below the directory `folders` (its names) can match
    the pattern of the names `parts`."""
    if not parts:
        return False
    if "**" in parts[0] or not folders:
        return True
    return (bool(pattern_regex(parts[0]).match(folders[0]))
            and may_hold(parts[1:], folders[1:]))


def matches(source, pattern):
    """(files, links) of `source` for `pattern`, as sorted paths with `/`:
    the files that match, and the symbolic links that match or that lead
    to what could match. A link is not followed."""
    parts = pattern.split("/")
    fixed = []
    for part in parts:
        if "*" in part:
            break
        fixed.append(part)
    for depth in range(1, len(fixed) + 1):
        if os.path.islink(os.path.join(source, *fixed[:depth])):
            return [], ["/".join(fixed[:depth])]
    if len(fixed) == len(parts):
        return ([pattern] if os.path.isfile(os.path.join(source, *parts)) else []), []
    regex = pattern_regex(pattern)
    found, links = [], []
    for folder, dirs, names in os.walk(os.path.join(source, *fixed)):
        rel = os.path.relpath(folder, source).replace(os.sep, "/")
        below = (lambda name: name) if rel == "." else (lambda name: rel + "/" + name)
        for name in dirs:
            if (os.path.islink(os.path.join(folder, name))
                    and may_hold(parts, below(name).split("/"))):
                links.append(below(name))
        dirs[:] = sorted(d for d in dirs if d != ".git")
        for name in names:
            if regex.match(below(name)):
                if os.path.islink(os.path.join(folder, name)):
                    links.append(below(name))
                elif os.path.isfile(os.path.join(folder, name)):
                    found.append(below(name))
    return sorted(found), sorted(links)


def entries(source, manifest, seen=()):
    """(where, words) of each entry of `manifest`, a path of `source`, with
    the entries of the files it names in their place."""
    if manifest in seen:
        raise ManifestError("%s: names itself" % manifest)
    try:
        with open(os.path.join(source, *manifest.split("/")), encoding="utf-8") as f:
            lines = f.read().splitlines()
    except OSError as e:
        raise ManifestError("%s: cannot be read (%s)" % (manifest, e.strerror))
    for number, line in enumerate(lines, 1):
        words = re.sub(r"(^|\s)#.*", "", line).split()
        if not words:
            continue
        where = "%s:%d" % (manifest, number)
        if words[0].startswith("@") and len(words) == 1:
            for entry in entries(source, words[0][1:], seen + (manifest,)):
                yield entry
        else:
            yield where, words


def package(source, manifest):
    """{place in the package: path in the repository} of `manifest`."""
    problems = []
    taken = {}        # path in the repository -> (place in the package, where)
    for where, words in entries(source, manifest):
        pattern = words[0].lstrip("!")
        leaves_out = words[0].startswith("!")
        absolute = pattern.startswith("/")
        # One spelling of a path: `./a//b` is `a/b`.
        pattern = "/".join(p for p in pattern.split("/") if p not in ("", "."))
        if (len(words) > 2 or (leaves_out and len(words) > 1) or not pattern
                or absolute or ".." in pattern.split("/")
                or (len(words) == 2 and "**" in pattern)):
            problems.append("%s: not an entry: %s" % (where, " ".join(words)))
            continue
        directory = None
        if len(words) == 2:
            directory = os.path.normpath(words[1]).replace(os.sep, "/")
            if directory.startswith("/") or directory.split("/")[0] == "..":
                problems.append("%s: %s is not a directory of the package"
                                % (where, words[1]))
                continue
        found, links = matches(source, pattern)
        for link in links:
            problems.append("%s: %s is a symbolic link" % (where, link))
        if not found and not links:
            problems.append("%s: %s matches no file" % (where, pattern))
        for path in found:
            if leaves_out:
                if taken.pop(path, None) is None:
                    problems.append("%s: %s is left out but no entry above "
                                    "takes it in" % (where, path))
            elif path in taken:
                problems.append("%s: %s is taken in already (%s)"
                                % (where, path, taken[path][1]))
            elif directory is None:
                taken[path] = (path, where)
            else:
                name = path.rsplit("/", 1)[-1]
                taken[path] = (name if directory == "." else directory + "/" + name,
                               where)
    places = {}
    for path, (place, where) in sorted(taken.items()):
        if place in places:
            problems.append("%s: %s and %s have the same place in the package, %s"
                            % (where, places[place], path, place))
        places[place] = path
    for place, path in sorted(places.items()):
        names = place.split("/")
        for depth in range(1, len(names)):
            above = "/".join(names[:depth])
            if above in places:
                problems.append("%s: %s, the place of %s, is below %s, the place of %s"
                                % (taken[path][1], place, path, above, places[above]))
    if problems:
        raise ManifestError("\n".join(problems))
    if not places:
        raise ManifestError("%s: no entry" % manifest)
    return places


def write(source, places, output):
    """Copies the files of `places` to `output`; (top-level name, files,
    bytes) for each top-level name of the package."""
    totals = {}
    for place, path in sorted(places.items()):
        target = os.path.join(output, *place.split("/"))
        os.makedirs(os.path.dirname(target), exist_ok=True)
        origin = os.path.join(source, *path.split("/"))
        shutil.copyfile(origin, target)
        shutil.copymode(origin, target)
        top = place.split("/")[0]
        files, size = totals.get(top, (0, 0))
        totals[top] = (files + 1, size + os.path.getsize(target))
    return [(top,) + totals[top] for top in sorted(totals)]


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    parser = argparse.ArgumentParser(
        description="Writes the LAMMPS package of Tadah! from the manifest.")
    parser.add_argument("output", metavar="<output directory>")
    parser.add_argument("--source", default=os.path.dirname(here),
                        help="the repository (default: the one of this script)")
    parser.add_argument("--manifest", default="export/manifest",
                        help="the list, a path of the repository "
                             "(default: export/manifest)")
    args = parser.parse_args()
    source = os.path.abspath(args.source)
    output = os.path.abspath(args.output)
    if os.path.exists(output) and (not os.path.isdir(output) or os.listdir(output)):
        print("write_package: %s exists and is not an empty directory" % output,
              file=sys.stderr)
        return 1
    try:
        places = package(source, args.manifest)
    except ManifestError as e:
        print(e, file=sys.stderr)
        print("write_package: nothing is written", file=sys.stderr)
        return 1
    totals = write(source, places, output)
    for top, files, size in totals:
        print("%-16s %5d files %9d bytes" % (top, files, size))
    print("%-16s %5d files %9d bytes" % ("the package", sum(t[1] for t in totals),
                                         sum(t[2] for t in totals)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
