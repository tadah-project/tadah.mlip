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

REASON = ('Removed in 1.6.0 with the legacy MPI training build. '
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
        # An example is a value of its key, shown as a line of the key (it
        # was shown bare, "20", before the examples became values only).
        page = self.rt.render(blueprint())
        self.assertNotIn('*Example*', self.section(page, 'MBLOCK'))
        self.assertIn('*Example* 1::\n\n       LAMBDA   20\n',
                      self.section(page, 'LAMBDA'))

    def test_each_example_line_is_shown_once_after_its_key(self):
        # Pins: an example is a value of its key (CORE's rule for the
        # blueprint), and one of several lines is one value per line; the
        # page shows each line once, as a configuration line of the key,
        # "<KEY>   <value>", as CoreUserConfigParser lists the examples of a
        # refused value. Blank lines and the blanks around a line are left
        # out; examples are numbered in order.
        # Before the fix: examples were shown as written, so a value was
        # shown without its key ("1.0 2.0"), and an example that began with
        # its key -- 64 in the blueprint until CORE made them values -- with
        # it once.
        bp = blueprint()
        bp['TYPES']['DOUBLES'] = {'value_type': ['{DOUBLE}'],
                                  'value_format': ['<double> ...'],
                                  'value_N': [-2147483647],
                                  'examples': ['2.0 -4.65 0.4']}
        bp['ALLOWED_KEYS']['CGRID2B'] = {
            'INHERIT': ['TYPES.DOUBLES'],
            'description': 'Centres.',
            'examples': ['LIN 10 0 6', '  1.0 2.0  \n\n1.5 2.5\n']}
        sec = self.section(self.rt.render(bp), 'CGRID2B')
        self.assertIn('   *Example* 1::\n\n       CGRID2B   LIN 10 0 6\n\n',
                      sec)
        self.assertIn('   *Example* 2::\n\n       CGRID2B   1.0 2.0\n'
                      '       CGRID2B   1.5 2.5\n\n', sec)
        self.assertNotIn('*Example* 3', sec)
        self.assertEqual(sec.count('CGRID2B   '), 3)

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

    def test_allowed_values_are_listed(self):
        # blueprint field `allowed_values` (CORE): the only values of a
        # string key, matched without regard to case; any other value is an
        # error at its line.
        bp = blueprint()
        bp['TYPES']['STRING'] = {'value_type': ['STRING'],
                                 'value_format': ['<string>'],
                                 'value_N': [1],
                                 'examples': ['word']}
        bp['ALLOWED_KEYS']['EMBF0'] = {'INHERIT': ['TYPES.STRING'],
                                       'value_format': ['ZERO|FREE'],
                                       'allowed_values': ['ZERO', 'FREE'],
                                       'default_value': ['ZERO'],
                                       'examples': ['ZERO', 'FREE'],
                                       'description': 'Reference.'}
        sec = self.section(self.rt.render(bp), 'EMBF0')
        self.assertIn('   *Allowed values*: ZERO / FREE (any case; any other '
                      'value is an error)\n', sec)
        self.assertLess(sec.index('*Allowed values*'), sec.index('*Default*'))
        self.assertNotIn('*Allowed values*', self.section(self.rt.render(bp),
                                                          'LAMBDA'))

    def test_exact_in_potential_is_shown(self):
        # blueprint field `exact_in_potential` (CORE): a key whose numbers a
        # potential records exactly whatever NUMERIC is (the cutoff and grid
        # keys, which a load checks exactly). The page says so after the
        # value count; a key without the field (or with it false) says
        # nothing. Before the fix the field was not rendered.
        bp = blueprint()
        bp['TYPES']['DOUBLES'] = {'value_type': ['{DOUBLE}'],
                                  'value_format': ['<double> ...'],
                                  'value_N': [-2147483647],
                                  'examples': ['3.0']}
        bp['ALLOWED_KEYS']['RCUT2B'] = {'INHERIT': ['TYPES.DOUBLES'],
                                        'exact_in_potential': True,
                                        'description': 'Cutoff.'}
        bp['ALLOWED_KEYS']['WEIGHTS'] = {'INHERIT': ['TYPES.DOUBLES'],
                                         'exact_in_potential': False,
                                         'description': 'Weights.'}
        page = self.rt.render(bp)
        sec = self.section(page, 'RCUT2B')
        self.assertIn('   *In a potential file*: written exactly, whatever '
                      'NUMERIC is (a load checks it exactly)\n', sec)
        self.assertLess(sec.index('*Max number of values*'),
                        sec.index('*In a potential file*'))
        self.assertNotIn('*In a potential file*',
                         self.section(page, 'WEIGHTS'))
        self.assertNotIn('*In a potential file*',
                         self.section(page, 'LAMBDA'))

    def test_multiline_reason_stays_inside_the_warning(self):
        sec = self.section(
            self.rt.render(blueprint({'deprecated': 'Line one.\nLine two.'})),
            'MBLOCK')
        self.assertIn('   **Deprecated.** Line one.\n   Line two.\n', sec)


if __name__ == '__main__':
    unittest.main()
