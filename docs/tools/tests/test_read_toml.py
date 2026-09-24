"""Tests for docs/source/config/read_toml.py (the config-keys page generator).

Run: python3 -m unittest discover -s docs/tools/tests -v
"""
import importlib.util
import os
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
DOCS = os.path.normpath(os.path.join(HERE, '..', '..'))
sys.path.insert(0, os.path.join(DOCS, 'source', 'config'))

HAVE_PYTOMLPP = importlib.util.find_spec('pytomlpp') is not None

REASON = ('Removed in 1.5.1 with the legacy MPI training build. '
          'Delete the line.')


def blueprint(extra_mblock=None):
    """A minimal blueprint: one ordinary key and one deprecated key, both
    inheriting their format and examples from a TYPES table as the real
    blueprint does. The deprecated key clears the inherited examples with
    an empty list, as CORE requires of a deprecated key."""
    mblock = {'INHERIT': ['TYPES.UINT'],
              'description': 'ScaLAPACK row block size.',
              'deprecated': REASON,
              'examples': []}
    mblock.update(extra_mblock or {})
    return {
        'TYPES': {'UINT': {'value_type': ['UINT'],
                           'value_format': ['<unsigned integer>'],
                           'value_N': [1],
                           'examples': ['20']}},
        'ALLOWED_KEYS': {
            'LAMBDA': {'INHERIT': ['TYPES.UINT'],
                       'default_value': ['0'],
                       'description': 'Regularisation.'},
            'MBLOCK': mblock,
        },
    }


@unittest.skipIf(not HAVE_PYTOMLPP, 'pytomlpp is not installed')
class ReadTomlTest(unittest.TestCase):
    def setUp(self):
        import read_toml
        self.rt = read_toml

    def section(self, page, key):
        start = page.index('.. _%s:' % key)
        nxt = page.find('\n.. _', start + 1)
        return page[start:] if nxt < 0 else page[start:nxt]

    def test_import_has_no_side_effects(self):
        # conf.py runs the script; importing it (as these tests do) must not
        # look for CORE or write the generated page into the cwd.
        cwd = os.getcwd()
        with tempfile.TemporaryDirectory() as tmp:
            os.chdir(tmp)
            try:
                import importlib
                importlib.reload(self.rt)
                self.assertEqual(os.listdir(tmp), [])
            finally:
                os.chdir(cwd)

    def test_deprecated_key_is_rendered_as_a_warning_with_its_reason(self):
        page = self.rt.render(blueprint())
        sec = self.section(page, 'MBLOCK')
        self.assertIn('.. warning::\n\n   **Deprecated.** ' + REASON, sec)
        # The warning comes right after the title, before the format line.
        self.assertLess(sec.index('.. warning::'), sec.index('[*UINT*]'))
        # Inherited fields are still rendered.
        self.assertIn('[*UINT*] <unsigned integer>', sec)
        self.assertIn('ScaLAPACK row block size.', sec)
        self.assertNotIn('*Default*', sec)

    def test_deprecated_key_shows_no_example(self):
        # The page must not show how to use a key it says to delete: the
        # key's empty examples list wins over the inherited TYPES examples.
        page = self.rt.render(blueprint())
        self.assertNotIn('*Example*', self.section(page, 'MBLOCK'))
        self.assertIn('*Example* 1::\n\n       20\n',
                      self.section(page, 'LAMBDA'))

    def test_ordinary_key_has_no_deprecation_warning(self):
        sec = self.section(self.rt.render(blueprint()), 'LAMBDA')
        self.assertNotIn('Deprecated', sec)
        self.assertNotIn('.. warning::', sec)
        self.assertIn('*Default*: 0', sec)

    def test_page_intro_explains_deprecated_keys(self):
        page = self.rt.render(blueprint())
        intro = page[:page.index('.. _LAMBDA:')]
        self.assertIn('**Deprecated**', intro)
        self.assertIn('ignores it', intro)
        # The warning goes to standard error, also from pair_style tadah.
        self.assertIn('standard error', intro)
        self.assertIn('pair_style tadah', intro)

    def test_multiline_reason_stays_inside_the_warning(self):
        sec = self.section(
            self.rt.render(blueprint({'deprecated': 'Line one.\nLine two.'})),
            'MBLOCK')
        self.assertIn('   **Deprecated.** Line one.\n   Line two.\n', sec)


if __name__ == '__main__':
    unittest.main()
