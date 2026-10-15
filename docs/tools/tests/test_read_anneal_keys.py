"""Tests for docs/source/config/read_anneal_keys.py (the ANNEAL key tables).

Run: python3 -m unittest discover -s docs/tools/tests -v
"""
import importlib
import importlib.util
import os
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
DOCS = os.path.normpath(os.path.join(HERE, '..', '..'))
CONFIG = os.path.join(DOCS, 'source', 'config')

HAVE_TOMLLIB = importlib.util.find_spec('tomllib') is not None

# Imports the script with tomllib blocked, as on Python 3.10 and older: a
# None entry in sys.modules makes `import tomllib` raise ImportError.
BLOCKED_IMPORT = (
    "import sys\n"
    "sys.modules['tomllib'] = None\n"
    "sys.path.insert(0, {config!r})\n"
    "import read_anneal_keys\n")


class ReadAnnealKeysPythonTest(unittest.TestCase):
    def test_python_without_tomllib_stops_with_a_clear_message(self):
        # Pins: without tomllib (Python 3.10 and older) the script -- which
        # conf.py runs for every docs build -- exits non-zero as it is
        # imported, with a message that it needs Python 3.11 or later, that
        # it reads HPO's blueprint/anneal_keys.toml with tomllib, and which
        # Python it is; no traceback, nothing written to the working
        # directory, and no fallback to the third-party toml package
        # (docs/requirements.txt installs it for generate_cli.py).
        # Before the fix: the import fell back to toml and succeeded (exit
        # 0, no message); without toml installed it failed with a traceback,
        # "ModuleNotFoundError: No module named 'toml'".
        with tempfile.TemporaryDirectory() as tmp:
            r = subprocess.run(
                [sys.executable, '-c', BLOCKED_IMPORT.format(config=CONFIG)],
                cwd=tmp, capture_output=True, text=True,
                env=dict(os.environ, PYTHONDONTWRITEBYTECODE='1'))
            self.assertNotEqual(r.returncode, 0, r.stdout + r.stderr)
            self.assertIn('read_anneal_keys.py needs Python 3.11 or later: '
                          "it reads HPO's blueprint/anneal_keys.toml with "
                          'tomllib, which Python %d.%d does not have'
                          % sys.version_info[:2], r.stderr)
            self.assertIn(sys.executable, r.stderr)
            self.assertNotIn('Traceback', r.stderr)
            self.assertEqual(os.listdir(tmp), [])

    @unittest.skipIf(not HAVE_TOMLLIB,
                     'tomllib needs Python 3.11 or later, as the docs do')
    def test_the_registry_is_read_with_tomllib(self):
        # Pins: with tomllib (Python 3.11 and later) the script imports
        # without side effects and reads a TOML file with it.
        # Before the fix: passes as well (a pin; tomllib was the first
        # choice).
        sys.path.insert(0, CONFIG)
        try:
            rak = importlib.import_module('read_anneal_keys')
        finally:
            sys.path.remove(CONFIG)
        import tomllib
        self.assertIs(rak.tomllib, tomllib)
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, 'keys.toml')
            with open(path, 'w') as fh:
                fh.write('[[key]]\nname = "NT"\nrank = 2\n')
            self.assertEqual(rak._load(path),
                             {'key': [{'name': 'NT', 'rank': 2}]})


if __name__ == '__main__':
    unittest.main()
