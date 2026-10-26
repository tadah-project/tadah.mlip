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

    def test_page_intro_says_tadah_hpo_reads_the_same_file(self):
        # `tadah hpo --config` takes the training configuration: HPO's run
        # trains every potential of the search from it. The page said that
        # nested fitting did not use the file.
        page = self.rt.render(blueprint())
        intro = page[:page.index('.. _LAMBDA:')]
        self.assertNotRegex(intro, r'(?i)not used by|HPO module')
        paragraphs = [p for p in intro.split('\n\n') if 'tadah hpo' in p]
        self.assertEqual(len(paragraphs), 1, intro)
        sentence = ' '.join(paragraphs[0].split())
        self.assertIn('``--config``', sentence)
        self.assertIn(':ref:`nested_fitting`', sentence)
        self.assertEqual(sentence.count('. '), 0, sentence)
        self.assertNotIn('.. note::', paragraphs[0])

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

    def test_retired_key_keeps_its_label_with_reason_replacement_and_conversion(self):
        # Pins: a retired key (CORE's blueprint field `retired`, a table of
        # reason, replacement and conversion) keeps its section and label
        # (an anchor the published pages cite), after the title a warning
        # "**Retired.**" with the reason, then "Replaced by:" and
        # "Conversion:", before the format line; it shows no example and no
        # default (a retired key has neither).
        # Before the fix: the field was not rendered: the section read as
        # an ordinary key.
        bp = blueprint()
        bp['TYPES']['DOUBLE'] = {'value_type': ['DOUBLE'],
                                 'value_format': ['<double>'],
                                 'value_N': [1],
                                 'examples': ['2.0']}
        bp['ALLOWED_KEYS']['FWEIGHT'] = {
            'INHERIT': ['TYPES.DOUBLE'],
            'description': 'Retired: the force weight.',
            'examples': [],
            'retired': {'reason': 'It weighted each force component.',
                        'replacement': 'FSIGMA, the force error scale',
                        'conversion': 'FSIGMA = 1/sqrt(FWEIGHT) eV/A.'}}
        sec = self.section(self.rt.render(bp), 'FWEIGHT')
        self.assertTrue(sec.startswith('.. _FWEIGHT:\n\nFWEIGHT\n'))
        self.assertIn('.. warning::\n\n   **Retired.** It weighted each force '
                      'component.\n', sec)
        self.assertIn('   Replaced by: FSIGMA, the force error scale\n', sec)
        self.assertIn('   Conversion: FSIGMA = 1/sqrt(FWEIGHT) eV/A.\n', sec)
        self.assertLess(sec.index('**Retired.**'), sec.index('[*DOUBLE*]'))
        self.assertNotIn('*Example*', sec)
        self.assertNotIn('*Default*', sec)
        self.assertNotIn('**Retired.**', self.section(self.rt.render(bp),
                                                      'LAMBDA'))

    def test_page_intro_explains_retired_keys(self):
        # Pins: the page says what a retired key does: a potential that has
        # one loads, with one notice per load, and a fit refuses it with
        # its conversion; and what its entry gives in words that fit every
        # retired key: the reason, what replaces it and the conversion
        # (EWEIGHT_TEMP has no key in its place, and UNCERTAINTY, WCOV and
        # WNOISE weighted no training data).
        # Before the fix: it said "another key replaces it"; then that every
        # retired key weighted the training data of a fit.
        page = self.rt.render(blueprint())
        intro = ' '.join(page[:page.index('.. _LAMBDA:')].split())
        self.assertIn('**Retired**', intro)
        self.assertIn('one notice', intro)
        self.assertIn('refuses', intro)
        self.assertIn('its entry gives the reason, what replaces it and its '
                      'conversion', intro)
        self.assertNotIn('another key replaces it', intro)
        self.assertNotIn('A key marked **Retired** weighted the training data',
                         intro)

    def command_blueprint(self):
        """blueprint() with a key only commands read (ANALYTICS, the option
        --analytics of predict, default false), a key of a nested command's
        configuration file (REFIT_MODE of refit, which reads a
        configuration file, written out in its group), a key no command
        reads (QUANTITY), the positional argument of explain (OPTION) and
        an option that one command retires (THRESHOLD: a retired option of
        data balance, read by data dedup), as CORE marks them
        (command_only)."""
        bp = blueprint()
        bp['ALLOWED_KEYS']['analytics'] = {
            'INHERIT': ['TYPES.UINT'], 'description': 'Performs analytics.',
            'cli_long_flag': '--analytics', 'cli_short_flag': '-A',
            'default_value': ['false'], 'command_only': True}
        bp['ALLOWED_KEYS']['refit_mode'] = {
            'INHERIT': ['TYPES.UINT'], 'description': 'Refit mode.',
            'cli_long_flag': '', 'command_only': True}
        bp['ALLOWED_KEYS']['quantity'] = {
            'INHERIT': ['TYPES.UINT'], 'description': 'Generic quantity.',
            'cli_long_flag': '--quantity', 'command_only': True}
        bp['ALLOWED_KEYS']['option'] = {
            'INHERIT': ['TYPES.UINT'], 'description': 'Positional argument.',
            'cli_long_flag': 'OPTION', 'command_only': True}
        bp['ALLOWED_KEYS']['threshold'] = {
            'INHERIT': ['TYPES.UINT'], 'description': 'Threshold.',
            'cli_long_flag': '--threshold', 'command_only': True}
        retired = {'reason': 'It rescaled the priorities.',
                   'replacement': 'the priorities of the data',
                   'conversion': 'No option converts it.'}
        bp['TASKS'] = {
            'predict': {'description': 'Predicts.', 'long_description': '',
                        'examples': [],
                        'keys': {'INHERIT': ['ALLOWED_KEYS.analytics']}},
            'explain': {'description': 'Explains.', 'long_description': '',
                        'examples': [],
                        'keys': {'INHERIT': ['ALLOWED_KEYS.option']}},
            'data': {'description': 'Data.', 'long_description': '',
                     'examples': [],
                     'print': {'description': 'Prints.',
                               'long_description': '', 'examples': [],
                               'keys': {'INHERIT':
                                        ['ALLOWED_KEYS.analytics']}},
                     'balance': {'description': 'Balances.',
                                 'long_description': '', 'examples': [],
                                 'keys': {'INHERIT':
                                          ['ALLOWED_KEYS.threshold'],
                                          'threshold': {
                                              'description': 'Retired.',
                                              'retired': retired}}},
                     'dedup': {'description': 'Removes duplicates.',
                               'long_description': '', 'examples': [],
                               'keys': {'INHERIT':
                                        ['ALLOWED_KEYS.threshold']}}},
            'refit': {'description': 'Refits.', 'long_description': '',
                      'examples': [],
                      'groups': {'input': {
                          'description': 'One of:',
                          'keys': {'INHERIT': ['ALLOWED_KEYS.config'],
                                   'refit_mode': {}}}}},
        }
        return bp

    def test_command_only_key_names_the_commands_that_read_it(self):
        # Pins: a key that only commands read (blueprint field command_only,
        # CORE) is marked so in its entry, with a link to the rule of such
        # a key (the Quick Start's Config File section, its owner), and
        # named as CORE's messages name it: the option and the commands
        # whose options or keys list it, "the option ``--analytics``
        # (``-A``) of ``tadah data print`` and ``tadah predict``"; a key of
        # a command's configuration file without an option, "a key of
        # ``tadah refit``"; one that no command reads, "read by no command;
        # every configuration file refuses its line". The entry does not
        # restate the rule of every such key. An ordinary key says none of
        # it.
        # Before the fix: each of the 79 entries restated the rule ("a
        # configuration file of another command refuses its line, and a
        # potential that holds it is read without it, with a warning"),
        # and QUANTITY's said that a configuration file of another command
        # refuses its line, of a key no command reads.
        page = self.rt.render(self.command_blueprint())
        sec = ' '.join(self.section(page, 'analytics').split())
        self.assertIn('*Command key* (:ref:`config_file`): the option '
                      '``--analytics`` (``-A``) of ``tadah data print`` and '
                      '``tadah predict``.', sec)
        self.assertNotIn('refuses its line', sec)
        self.assertNotIn('potential', sec)
        refit = ' '.join(self.section(page, 'refit_mode').split())
        self.assertIn('*Command key* (:ref:`config_file`): a key of '
                      '``tadah refit``.', refit)
        self.assertNotIn('refuses its line', refit)
        quantity = ' '.join(self.section(page, 'quantity').split())
        self.assertIn('*Command key* (:ref:`config_file`): read by no '
                      'command; every configuration file refuses its line.',
                      quantity)
        self.assertNotIn('another command', quantity)
        self.assertNotIn('Command key', self.section(page, 'LAMBDA'))

    def test_command_key_note_is_a_paragraph_of_its_own(self):
        # Pins: the note of a command key ends its paragraph, so that the
        # default after it is a paragraph of its own.
        # Before the fix: the note ended with one newline, and "*Default*:
        # false" was rendered inside the note's paragraph.
        sec = self.section(self.rt.render(self.command_blueprint()),
                           'analytics')
        self.assertIn('``tadah predict``.\n\n   *Default*: false\n', sec)

    def test_a_command_that_retires_an_option_does_not_read_it(self):
        # Pins: a command whose table writes the key out as retired (an
        # option it refuses with the reason, such as --threshold of tadah
        # data balance) is not a command that reads it: THRESHOLD is read
        # by tadah data dedup alone, and its entry names that command
        # alone, as CORE's commands_reading_key does.
        # Before the fix: the entry named tadah data balance and tadah
        # data dedup.
        bp = self.command_blueprint()
        self.assertEqual(self.rt.commands_reading(bp)['THRESHOLD'],
                         ['tadah data dedup'])
        sec = ' '.join(self.section(self.rt.render(bp), 'threshold').split())
        self.assertIn('the option ``--threshold`` of ``tadah data dedup``.',
                      sec)
        self.assertNotIn('balance', sec)

    def test_positional_key_is_the_argument_of_its_command(self):
        # Pins: a key whose cli_long_flag does not start with "-" is the
        # positional argument of the commands that read it (OPTION of tadah
        # explain), named so: "the argument of ``tadah explain``", as
        # CORE's messages name it.
        # Before the fix: "A key of ``tadah explain``", of a command that
        # reads no configuration file.
        sec = ' '.join(self.section(self.rt.render(self.command_blueprint()),
                                    'option').split())
        self.assertIn('*Command key* (:ref:`config_file`): the argument of '
                      '``tadah explain``.', sec)
        self.assertNotIn('a key of', sec)

    def test_command_key_of_no_configuration_file_shows_no_example(self):
        # Pins: an example of a key is shown as a line of a configuration
        # file ("<KEY>   <value>"); a command key that no command reading a
        # configuration file reads (an option of tadah predict or tadah
        # data, the argument of tadah explain, a key no command reads) has
        # no such line, as every configuration file refuses it, so its
        # entry shows no example: the reference of the command-line
        # options gives the options of each command with their examples. A
        # command key of a command that reads a configuration file (a key
        # of tadah refit) keeps its examples, as does every other key.
        # Before the fix: ANALYTICS showed the example "analytics   20", a
        # line every configuration file refuses.
        page = self.rt.render(self.command_blueprint())
        for key in ('analytics', 'quantity', 'option', 'threshold'):
            with self.subTest(key=key):
                self.assertNotIn('*Example*', self.section(page, key))
        self.assertIn('*Example* 1::\n\n       refit_mode   20\n',
                      self.section(page, 'refit_mode'))
        self.assertIn('*Example* 1::\n\n       LAMBDA   20\n',
                      self.section(page, 'LAMBDA'))

    def test_page_intro_explains_command_only_keys(self):
        # Pins: the page says what a key marked Command key is, that its
        # entry names the commands and shows an example line only when one
        # of them reads a configuration file, that a configuration file of
        # another command refuses its line (one clause and a link to the
        # Quick Start's rule, its owner), and that a potential is read
        # without it, with one warning per key that goes where the warning
        # of a deprecated key goes: standard error, or log.lammps under
        # pair_style tadah.
        # Before the fix: it said that the line "is refused at its line"
        # and that the warning of a potential goes to standard error,
        # which pair_style tadah writes to its log instead.
        page = self.rt.render(self.command_blueprint())
        intro = ' '.join(page[:page.index('.. _LAMBDA:')].split())
        self.assertIn('only some commands read', intro)
        self.assertIn('Its entry names the commands, and shows an example '
                      'line only when one of them reads a configuration '
                      'file.', intro)
        self.assertIn('A configuration file of another command refuses its '
                      'line (:ref:`config_file`).', intro)
        self.assertIn('one warning per key, which goes where the warning of '
                      'a deprecated key goes (standard error, or '
                      '``log.lammps`` under ``pair_style tadah``)', intro)
        self.assertNotIn('is refused at its line', intro)


if __name__ == '__main__':
    unittest.main()
