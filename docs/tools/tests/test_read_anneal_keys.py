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
HAVE_DOCUTILS = importlib.util.find_spec('docutils') is not None

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


@unittest.skipIf(not HAVE_TOMLLIB,
                 'tomllib needs Python 3.11 or later, as the docs do')
@unittest.skipIf(not HAVE_DOCUTILS,
                 'docutils is required to verify rendered table structure')
class ReadAnnealKeysTableTest(unittest.TestCase):
    def test_reference_cells_preserve_defaults_values_and_constraints(self):
        from docutils import nodes
        from docutils.core import publish_doctree

        sys.path.insert(0, CONFIG)
        try:
            rak = importlib.import_module('read_anneal_keys')
        finally:
            sys.path.remove(CONFIG)

        kinds = ['serial', 'pt', 'pa']
        data = {
            'key': [
                dict(name='COUNT', type='count', kinds=kinds,
                     group='budget', rank=1, default=4, default_serial=6,
                     min=1, doc='Number of repetitions.'),
                dict(name='SCALE', type='float', kinds=kinds,
                     group='steps', rank=1, default=0.25, min=0, max=1,
                     doc='Proposal scale.'),
                dict(name='MODE', type='enum', kinds=kinds,
                     group='init', rank=1, default='LOCAL',
                     values=['LOCAL', 'GLOBAL'], doc='Initialisation mode.'),
                dict(name='ACTIVE', type='bool', kinds=kinds,
                     group='init', rank=2, default=1,
                     doc='Enables proposal adaptation.'),
            ],
            'rule': [
                dict(type='requires_flag', key='SCALE', flag='ACTIVE'),
                dict(type='conflicts_flag', key='COUNT', flag='ACTIVE'),
                dict(type='unused_when_set', key='ACTIVE', other='COUNT'),
                dict(type='unused_when_flag_off', key='SCALE', flag='ACTIVE'),
                dict(type='requires_enum', key='SCALE',
                     enum_key='MODE', value='LOCAL'),
            ],
        }
        for kind in kinds:
            with self.subTest(kind=kind):
                tree = publish_doctree(rak.render(kind, data),
                                       settings_overrides={'halt_level': 2})
                tables = list(tree.findall(nodes.table))
                self.assertEqual(len(tables), 3)
                actual = {}
                for table in tables:
                    columns = list(table.findall(nodes.colspec))
                    self.assertEqual([c['colwidth'] for c in columns],
                                     [28, 72])
                    rows = list(table.findall(nodes.row))
                    self.assertEqual([c.astext() for c in rows[0].children],
                                     ['Key', 'Reference'])
                    for row in rows[1:]:
                        self.assertEqual(len(row.children), 2)
                        key, reference = row.children
                        self.assertTrue(all(isinstance(p, nodes.paragraph)
                                            for p in reference.children))
                        self.assertNotIn(key.astext(), actual)
                        actual[key.astext()] = [p.astext()
                                                for p in reference.children]
                expected = {
                    'COUNT': [
                        'Default: ' + ('6' if kind == 'serial' else '4'),
                        'Values: >= 1',
                        'Number of repetitions. Refused when ACTIVE is ON.',
                    ],
                    'SCALE': [
                        'Default: 0.25', 'Values: [0, 1]',
                        'Proposal scale. Refused unless ACTIVE is ON. '
                        'Refused unless MODE is LOCAL.',
                    ],
                    'MODE': [
                        'Default: LOCAL', 'Values: LOCAL | GLOBAL',
                        'Initialisation mode.',
                    ],
                    'ACTIVE': [
                        'Default: ON', 'Values: ON | OFF',
                        'Enables proposal adaptation. '
                        'Refused when COUNT is set.',
                    ],
                }
                self.assertEqual(actual, expected)


if __name__ == '__main__':
    unittest.main()
