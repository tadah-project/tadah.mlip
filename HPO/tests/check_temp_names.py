#!/usr/bin/env python3
"""Check test sources for a file or directory named by a clock reading alone.

  check_temp_names.py <source or directory> [<source or directory> ...]

A directory stands for every *.cpp, *.h and *.hpp under it (subdirectories
included). Comments and the insides of string literals are not code.

CTest runs every test case in a process of its own, many at a time, and all
of them share the temporary directory. A name made from a clock reading,

    temp_directory_path() / ("journal_" + std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count()) + ".log")

is the same in two processes that read the same tick: one then reads the
other's journal, or removes the other's directory.

Rule. A clock reading that can become a name is ``time_since_epoch`` (any
clock, however the reading is spelled). Its statement is the code from the
``;``, ``{`` or ``}`` before it to the one after it. A statement that makes
text from the reading -- it also holds ``to_string``, or ``<<`` and a string
literal -- is refused unless it holds ``getpid``, ``mkdtemp`` or ``mkstemp``
as well: the process id tells two processes that run side by side apart, and
mkdtemp and mkstemp create what they name, so no two callers get one name.
HPO's tests take such a name from hpo_test::unique_temp_path
(tests/scratch_tmpdir.h: the stem, the process id and a count of the names
the process has taken).

Not refused: a reading that is not made into text (an elapsed time, a
deadline, a seed), and a name without a clock reading.

Not seen, each of them a name that two processes can share:

  * a reading kept in a variable that a later statement makes into text;
  * a reading written to a stream in a statement without a string literal
    (``name << prefix; name << clock::now().time_since_epoch().count();``);
  * a reading made into text by anything but ``to_string`` and ``<<``
    (``snprintf``, ``std::format``);
  * a reading inside braces (``std::to_string(Ticks{now()
    .time_since_epoch().count()}.v)``): the braces end the statement;
  * a time taken without ``time_since_epoch``: ``std::time``,
    ``std::clock``, the difference of two time points, a function of
    another name that returns a reading;
  * a name that holds no clock reading at all: a count alone, a random
    number, ``tmpnam``, a fixed name;
  * a process id that is in the statement and not in the name (accepted).

Refused although nothing is wrong: a process id that reaches the name from
another statement, and text made from a reading for another purpose (a
time stamp in a message): this check does not know what the text names.

Exit status: 0 when no statement is refused; 1 with one line per refused
statement; 2 for a usage error, a path that does not exist or no source.
"""

from __future__ import annotations

import pathlib
import re
import sys

sys.dont_write_bytecode = True   # no __pycache__ in the source tree
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from check_mpi_build_tests import SOURCE_SUFFIXES, scan  # noqa: E402

_READING = re.compile(r"\btime_since_epoch\b")
_TEXT = re.compile(r"\bto_string\b")
_UNIQUE = re.compile(r"\b(?:getpid|mkdtemp|mkstemp)\b")


def check_text(text: str, where: str = "<text>"):
    """(problems, readings made into text) of one source."""
    code = scan(text).code
    problems, n_text = [], 0
    done = -1   # the end of the last statement reported on
    for m in _READING.finditer(code):
        if m.start() < done:
            continue
        a = max(code.rfind(c, 0, m.start()) for c in ";{}") + 1
        ends = [e for e in (code.find(c, m.end()) for c in ";{}") if e >= 0]
        b = min(ends) if ends else len(code)
        statement = code[a:b]
        if not (_TEXT.search(statement)
                or ("<<" in statement and '"' in statement)):
            continue
        done = b
        n_text += 1
        if _UNIQUE.search(statement):
            continue
        line = text.count("\n", 0, m.start()) + 1
        shown = " ".join(text[a:b].split())
        if len(shown) > 120:
            shown = shown[:117] + "..."
        problems.append(
            f"{where}:{line}: a name made from a clock reading alone "
            f"('{shown}'): two test processes that read the same tick "
            "choose the same name. Name the file or directory by the "
            "process id and a count (HPO's tests: "
            "hpo_test::unique_temp_path, tests/scratch_tmpdir.h), or "
            "create it with mkdtemp or mkstemp")
    return problems, n_text


def sources(arg: str):
    """The sources @p arg stands for; None when it does not exist."""
    path = pathlib.Path(arg)
    if path.is_dir():
        return sorted(p for p in path.rglob("*")
                      if p.suffix in SOURCE_SUFFIXES and p.is_file())
    return [path] if path.is_file() else None


def main(argv: list) -> int:
    if len(argv) < 2:
        print(__doc__.split("\n\n")[1], file=sys.stderr)
        return 2
    problems, n_files, n_text = [], 0, 0
    for arg in argv[1:]:
        paths = sources(arg)
        if paths is None:
            print(f"check_temp_names: {arg}: no such file or directory",
                  file=sys.stderr)
            return 2
        for path in paths:
            p, n = check_text(path.read_text(encoding="utf-8"), str(path))
            problems += p
            n_files += 1
            n_text += n
    if n_files == 0:
        print("check_temp_names: no source in " + ", ".join(argv[1:]),
              file=sys.stderr)
        return 2
    for p in problems:
        print(p)
    print(f"check_temp_names: {n_files} files, {n_text} clock readings made "
          f"into text, {len(problems)} refused")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
