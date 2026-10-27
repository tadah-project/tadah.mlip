#!/usr/bin/env python3
"""The example of the *Configuration File* page, section "Common Usage",
against the program it describes.

    check_config_page_example.py --tadah <tadah> --page <read_toml.py>

The page is generated from the text `header` of
docs/source/config/read_toml.py (the source: the page is written at every
docs build). In its section "Common Usage" the check reads

  - the commands: each line of a `.. code-block:: bash` block that runs
    `tadah` (after a shell prompt, `... $ `, if the line has one);
  - the quoted output: every other non-blank line of those blocks and every
    non-blank line of a `.. code-block:: text` block after them; a line that
    ends in "..." is quoted up to it (the rest of the message is elided);
  - the exit status the prose states: "exit status <n>".

It runs each command in a fresh temporary directory holding config.train, a
training configuration that is complete but for DBFILE (the case the
section describes), and fails, naming what differs, unless the section has
a command, quoted output and a stated exit status, each command exits with
that status, and each quoted line is the start of a line of its output
(standard output and error together).
"""
import argparse
import pathlib
import re
import shlex
import subprocess
import sys
import tempfile

CONFIG = """ATOM Ta
INIT2B true
INITMB false
TYPE2B D2_LJ Ta Ta
RCTYPE2B Cut_Cos
RCUT2B 5.0
MODEL M_BLR BF_Linear
"""


def section(page_source):
    """The text of the section "Common Usage" of the header in
    @p page_source, without its title."""
    m = re.search(r"^Common Usage\n-+\n(.*?)(?=^[^\n]+\n-{3,}\n)",
                  page_source, re.S | re.M)
    if not m:
        raise SystemExit("FAIL: the page has no section 'Common Usage' "
                         "followed by another section")
    return m.group(1)


def code_blocks(text):
    """(language, lines) of each code block of @p text, in order: the lines
    indented under `.. code-block:: <language>`, without the indent."""
    blocks = []
    lines = text.split("\n")
    i = 0
    while i < len(lines):
        m = re.match(r"^(\s*)\.\. code-block::\s*(\S+)\s*$", lines[i])
        if not m:
            i += 1
            continue
        indent = len(m.group(1))
        body = []
        i += 1
        while i < len(lines):
            line = lines[i]
            if line.strip() and len(line) - len(line.lstrip()) <= indent:
                break
            body.append(line)
            i += 1
        stripped = [l.strip() for l in body]
        while stripped and not stripped[0]:
            stripped.pop(0)
        while stripped and not stripped[-1]:
            stripped.pop()
        blocks.append((m.group(2), stripped))
    return blocks


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tadah", required=True)
    ap.add_argument("--page", required=True,
                    help="docs/source/config/read_toml.py")
    args = ap.parse_args()

    text = section(pathlib.Path(args.page).read_text())
    commands, quoted = [], []
    seen_command = False
    for language, lines in code_blocks(text):
        if language == "bash":
            for line in lines:
                if not line:
                    continue
                m = re.match(r"^(?:.*\$\s+)?(tadah\s.*)$", line)
                if m:
                    commands.append(m.group(1))
                    seen_command = True
                else:
                    quoted.append(line)
        elif language == "text" and seen_command:
            quoted.extend(l for l in lines if l)
    status = re.search(r"exit status (\d+)", " ".join(text.split()))

    failures = []
    if not commands:
        failures.append("the section runs no tadah command")
    if not quoted:
        failures.append("the section quotes no output")
    if not status:
        failures.append("the section states no exit status ('exit status "
                        "<n>')")
    for command in commands:
        with tempfile.TemporaryDirectory() as tmp:
            (pathlib.Path(tmp) / "config.train").write_text(CONFIG)
            argv = shlex.split(command)
            argv[0] = args.tadah
            run = subprocess.run(argv, cwd=tmp, stdout=subprocess.PIPE,
                                 stderr=subprocess.STDOUT, text=True)
        out_lines = [l.strip() for l in run.stdout.splitlines()]
        print(f"$ {command}\n{run.stdout}(exit status {run.returncode})")
        if status and run.returncode != int(status.group(1)):
            failures.append(f"'{command}' exits with status "
                            f"{run.returncode}, the page says "
                            f"{status.group(1)}")
        for q in quoted:
            want = q[:-3].rstrip() if q.endswith("...") else q
            if not any(l.startswith(want) for l in out_lines):
                failures.append(f"the quoted line '{q}' does not start a "
                                f"line of the output of '{command}'")
    for f in failures:
        print("FAIL: " + f)
    if failures:
        return 1
    print("The example of the section 'Common Usage' is what tadah prints.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
