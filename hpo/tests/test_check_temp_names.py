#!/usr/bin/env python3
"""Unit tests of check_temp_names.py (CTest hpo_temp_names_lint_tool).

The check refuses a statement of a test source that makes text from a clock
reading (time_since_epoch) without the process id (getpid), mkdtemp or
mkstemp: a file or directory named so is the same in two test processes
that read the same tick. Synthetic sources, checked as text and through the
command line.
"""
from __future__ import annotations

import importlib.util
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
TOOL = HERE / "check_temp_names.py"

sys.dont_write_bytecode = True   # no __pycache__ in the source tree
_spec = importlib.util.spec_from_file_location("check_temp_names", TOOL)
check = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(check)

STAMP = ("std::to_string(std::chrono::steady_clock::now()\n"
         "                   .time_since_epoch().count())")
TMP = "std::filesystem::temp_directory_path()"


def problems(text: str) -> list:
    return check.check_text(text, "t.cpp")[0]


class Refused(unittest.TestCase):
    def test_a_file_named_by_the_clock_alone(self):
        p = problems("struct Journal {\n  std::filesystem::path path;\n"
                     f'  Journal() : path({TMP} /\n      ("journal_" + {STAMP}'
                     ' + ".log"))\n  { open(path); }\n};\n')
        self.assertEqual(len(p), 1, p)
        self.assertIn("t.cpp:5: a name made from a clock reading alone "
                      "('Journal() : path(std::filesystem::"
                      'temp_directory_path() / ("journal_" + ', p[0])
        self.assertIn("hpo_test::unique_temp_path", p[0])
        self.assertIn("mkdtemp or mkstemp", p[0])

    def test_a_directory_named_in_a_statement_of_a_body(self):
        p = problems(f'void f() {{\n  int a = 0;\n  dir = {TMP} / ("run_" + '
                     f"{STAMP});\n  create_directories(dir);\n}}\n")
        self.assertEqual(len(p), 1, p)
        self.assertIn("t.cpp:4: ", p[0])   # the line of the reading

    def test_a_helper_that_returns_the_stamp(self):
        # The name is joined elsewhere, in the same source or another.
        p = problems("std::string stamp() {\n  return std::to_string(\n"
                     "      std::chrono::steady_clock::now()"
                     ".time_since_epoch().count());\n}\n")
        self.assertEqual(len(p), 1, p)
        self.assertIn("t.cpp:3: ", p[0])

    def test_any_clock_and_a_reading_taken_from_a_time_point(self):
        self.assertEqual(len(problems(
            'auto p = dir / ("x" + std::to_string('
            "system_clock::now().time_since_epoch().count()));\n")), 1)
        self.assertEqual(len(problems(
            "const auto t = Clock::now();\n"
            'auto p = dir / ("x" + std::to_string('
            "t.time_since_epoch().count()));\n")), 1)

    def test_a_name_written_to_a_stream(self):
        self.assertEqual(len(problems(
            'name << "run_" << clock::now().time_since_epoch().count();\n')),
            1)

    def test_a_process_id_in_another_statement_does_not_count(self):
        p = problems("const auto pid = ::getpid();\n"
                     f'auto a = {TMP} / ("a_" + {STAMP});\n'
                     f'auto b = {TMP} / ("b_" + {STAMP});\n')
        self.assertEqual([q.split(": ")[0] for q in p],
                         ["t.cpp:3", "t.cpp:5"])

    def test_a_statement_ends_at_a_brace(self):
        # A process id on the other side of a brace is not in the name:
        # the body of a constructor after its initialiser list, the block
        # around a statement, an initialiser in braces before or after.
        for text in (
                f'Journal() : path({TMP} / ("journal_" + {STAMP}))\n'
                "{ owner = ::getpid(); }\n",
                f'if (::getpid() > 0) {{ p = {TMP} / ("x_" + {STAMP}); }}\n',
                "int ids[] = {::getpid()}, "
                f'n = count({TMP} / ("x_" + {STAMP}));\n',
                f'fs::path p{{{TMP} / ("x_" + {STAMP})}}, '
                "q = std::to_string(::getpid());\n"):
            with self.subTest(text=text):
                self.assertEqual(len(problems(text)), 1)

    def test_two_readings_of_one_statement_are_one_refusal(self):
        self.assertEqual(len(problems(
            f'auto p = {TMP} / ({STAMP} + "_" + {STAMP});\n')), 1)


class Accepted(unittest.TestCase):
    def test_the_process_id_in_the_statement(self):
        self.assertEqual(problems(
            f'dir = {TMP} / ("run_" + std::to_string(::getpid()) + "_" +\n'
            f"      {STAMP});\n"), [])

    def test_mkdtemp_and_mkstemp_in_the_statement(self):
        for call in ("mkdtemp", "::mkstemp"):
            with self.subTest(call=call):
                self.assertEqual(problems(
                    f'int r = {call}(buffer_of({TMP} / ("run_" + {STAMP} + '
                    '"_XXXXXX")));\n'), [])

    def test_a_name_without_a_clock_reading(self):
        self.assertEqual(check.check_text(
            'path = hpo_test::unique_temp_path("journal_", ".log");\n'
            f'std::string t = ({TMP} / "run_XXXXXX").string();\n'
            "::mkdtemp(t.data());\n"), ([], 0))

    def test_a_reading_that_is_not_made_into_text(self):
        self.assertEqual(check.check_text(
            "const auto t0 = std::chrono::steady_clock::now();\n"
            "const double s = std::chrono::duration<double>(\n"
            "    std::chrono::steady_clock::now() - t0).count();\n"
            'INFO("took " << s << " s, " << std::to_string(s));\n'
            "const auto deadline = std::chrono::steady_clock::now() + "
            "std::chrono::seconds(5);\n"
            "std::mt19937 rng(std::chrono::system_clock::now()"
            ".time_since_epoch().count());\n"
            "auto seed = (clock::now().time_since_epoch().count() << 16) ^ "
            "salt;\n"
            "fd.random(clock::now().time_since_epoch().count(), lo, hi);\n"),
            ([], 0))

    def test_comments_and_literals_are_not_code(self):
        self.assertEqual(check.check_text(
            "// p = dir / std::to_string(now().time_since_epoch().count());\n"
            "/* std::to_string(now().time_since_epoch().count()) */\n"
            'const char* doc = "to_string(now().time_since_epoch().count())"'
            ";\n"), ([], 0))


class NotSeen(unittest.TestCase):
    """What the docstring of the check lists as not seen: each is a name
    two processes can share, and none is refused."""

    def test_the_listed_forms_pass(self):
        for text in (
                "const auto t = clock::now().time_since_epoch().count();\n"
                'auto p = dir / ("x_" + std::to_string(t));\n',
                "name << prefix;\n"
                "name << clock::now().time_since_epoch().count();\n",
                'std::snprintf(buf, sizeof buf, "x_%lld", static_cast<long '
                "long>(clock::now().time_since_epoch().count()));\n",
                'auto p = dir / ("x_" + std::to_string(Ticks{clock::now()'
                ".time_since_epoch().count()}.v));\n",
                'auto p = dir / ("x_" + std::to_string(std::time(nullptr)));'
                "\n",
                'auto p = dir / ("x_" + std::to_string(std::clock()));\n',
                'auto p = dir / ("x_" + std::to_string((clock::now() - t0)'
                ".count()));\n",
                'auto p = dir / ("x_" + std::to_string(counter++));\n',
                'auto p = dir / ("x_" + std::to_string(rng()));\n',
                "auto p = std::tmpnam(nullptr);\n",
                'auto p = dir / "journal.log";\n',
                'auto p = dir / ("x_" + std::to_string(clock::now()'
                ".time_since_epoch().count() + 0 * ::getpid()));\n"):
            with self.subTest(text=text):
                self.assertEqual(problems(text), [])

    def test_the_parent_process_id_is_not_the_process_id(self):
        self.assertEqual(len(problems(
            'auto p = dir / ("x_" + std::to_string(::getppid()) + '
            f"{STAMP});\n")), 1)


class CommandLine(unittest.TestCase):
    def run_tool(self, *args):
        return subprocess.run([sys.executable, str(TOOL), *map(str, args)],
                              capture_output=True, text=True, check=False)

    def test_sources_directories_and_the_exit_status(self):
        with tempfile.TemporaryDirectory() as tmp:
            d = Path(tmp)
            good = d / "good.cpp"
            good.write_text(f'dir = {TMP} / ("run_" + std::to_string('
                            f"::getpid()) + {STAMP});\n")
            r = self.run_tool(good)
            self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
            self.assertIn("check_temp_names: 1 files, 1 clock readings made "
                          "into text, 0 refused", r.stdout)
            (d / "sub").mkdir()
            bad = d / "sub" / "helper.h"
            bad.write_text(f'inline auto p = {TMP} / ("x_" + {STAMP});\n')
            (d / "notes.txt").write_text(f'p = {TMP} / ("x_" + {STAMP});\n')
            r = self.run_tool(bad)
            self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
            self.assertIn(f"{bad}:2: a name made from a clock reading alone",
                          r.stdout)
            # A directory: its sources, subdirectories included.
            r = self.run_tool(d)
            self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
            self.assertIn("check_temp_names: 2 files, 2 clock readings made "
                          "into text, 1 refused", r.stdout)
            r = self.run_tool(good, d / "sub")
            self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
            self.assertIn("2 files", r.stdout)

    def test_nothing_to_read_is_an_error(self):
        with tempfile.TemporaryDirectory() as tmp:
            d = Path(tmp)
            self.assertEqual(self.run_tool().returncode, 2)
            r = self.run_tool(d / "missing.cpp")
            self.assertEqual(r.returncode, 2)
            self.assertIn("missing.cpp: no such file or directory", r.stderr)
            (d / "notes.txt").write_text("nothing\n")
            r = self.run_tool(d)
            self.assertEqual(r.returncode, 2)
            self.assertIn("check_temp_names: no source in", r.stderr)


if __name__ == "__main__":
    unittest.main()
