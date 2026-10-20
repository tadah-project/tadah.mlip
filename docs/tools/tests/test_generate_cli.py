"""Tests for docs/source/cli/generate_cli.py (the CLI reference generator).

Run: python3 -m unittest discover -s docs/tools/tests -v

The CLI page shows each command in a parsed-literal block, so a key's
long_description, which is RST (the configuration-keys page renders it as
such), is shown there as text: these cases pin how.
"""
import importlib
import importlib.util
import os
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
DOCS = os.path.normpath(os.path.join(HERE, '..', '..'))
sys.path.insert(0, os.path.join(DOCS, 'source', 'cli'))

HAVE_TOML = importlib.util.find_spec('toml') is not None


@unittest.skipIf(not HAVE_TOML, 'the toml module is not installed')
class GenerateCliTest(unittest.TestCase):
    def setUp(self):
        # Pins: importing the generator reads no blueprint and writes no page
        # (conf.py runs it as a script). Before the fix: the import ran it,
        # and failed outside docs/source ('No such file or directory:
        # config/blueprint_tadah.toml').
        cwd = os.getcwd()
        with tempfile.TemporaryDirectory() as tmp:
            os.chdir(tmp)
            try:
                sys.modules.pop('generate_cli', None)
                self.gc = importlib.import_module('generate_cli')
                self.assertEqual(os.listdir(tmp), [])
            finally:
                os.chdir(cwd)

    def description(self, long_description):
        """The lines build_key_info() writes under **Description**: for a
        key with this long_description."""
        bk = self.gc.BlueprintKey()
        bk.keyName = 'refit.optimizer'
        bk.value_type = ['STRING']
        bk.value_format = ['<block>']
        bk.description = 'Optimiser sub-block.'
        bk.longDescription = long_description
        out = self.gc.build_key_info(bk)
        start = out.index('    **Description**:') + 1
        end = len(out) - 1   # build_key_info ends a key with an empty line
        self.assertEqual(out[end], '')
        return out[start:end]

    def test_a_layout_keeps_its_indentation_and_the_marker_shows_as_rst_does(self):
        # Pins: the lines of a long description keep their indentation
        # relative to its least indented line (here the blueprint's own
        # OPTIMIZER text), under the key's six blanks; a blank line inside it
        # is empty and the blank lines around it are left out; and
        # "example::", the RST marker of the literal block that follows,
        # shows as "example:", as on the configuration-keys page.
        # Before the fix: every line was stripped, so REFINE, OPTIMIZER and
        # the keys inside it stood at one indentation, after
        # "for example::" (and a line of six blanks before it).
        self.assertEqual(self.description(
            '\nWritten inside a stage block, for example::\n'
            '\n'
            '    REFINE\n'
            '      OPTIMIZER\n'
            '        LIB CERES  ALGO LM  MAXEVAL 1000\n'
            '      ENDOPTIMIZER\n'
            '    ENDREFINE\n'), [
            '      Written inside a stage block, for example:',
            '',
            '          REFINE',
            '            OPTIMIZER',
            '              LIB CERES  ALGO LM  MAXEVAL 1000',
            '            ENDOPTIMIZER',
            '          ENDREFINE'])

    def test_a_text_indented_as_a_whole_loses_that_indentation(self):
        # Pins: the indentation every line shares is removed, the rest kept.
        # Before the fix: the nested line lost its own two blanks too.
        self.assertEqual(self.description(
            '    First line,\n'
            '    second line:\n'
            '      - nested item\n'), [
            '      First line,',
            '      second line:',
            '        - nested item'])

    def test_the_other_forms_of_the_marker_show_as_rst_does(self):
        # Pins: "text ::" ending a paragraph shows as "text", and a paragraph
        # that is "::" alone is not shown (nor the blank line after it), as
        # RST renders them; "::" is text inside a literal block ("std::"
        # under "KEY 1") and where it does not end a paragraph ("std::" then
        # "mt19937_64" on the next line).
        # Before the fix: each "::" was shown as written.
        self.assertEqual(self.description(
            'A layout ::\n'
            '\n'
            '    KEY 1\n'
            '    std::\n'
            '\n'
            'Another layout.\n'
            '\n'
            '::\n'
            '\n'
            '    KEY 2\n'
            '\n'
            'Drawn with std::\n'
            'mt19937_64.\n'), [
            '      A layout',
            '',
            '          KEY 1',
            '          std::',
            '',
            '      Another layout.',
            '',
            '          KEY 2',
            '',
            '      Drawn with std::',
            '      mt19937_64.'])


    def test_a_retired_option_gives_its_conversion(self):
        # Pins: a retired key (CORE's blueprint field `retired`) whose
        # command-line option is kept to give the conversion is listed with
        # "**Retired**:" and its reason, "**Replaced by**:" and
        # "**Conversion**:", and no default or examples; parse_blueprint
        # reads the field.
        # Before the fix: the field was not read, and the option read as an
        # ordinary one.
        bk = self.gc.BlueprintKey()
        bk.keyName = 'train.wdbfile_auto'
        bk.value_type = ['DOUBLE']
        bk.value_format = ['<double>']
        bk.description = 'Retired: the dataset balance.'
        bk.cli_long_flag = '--wdbfile-auto'
        bk.retired = {'reason': 'It weighted each dataset by n_d^-a.',
                      'replacement': 'WDBFILE',
                      'conversion': 'WDBFILE n_d^-a, one per dataset.'}
        out = self.gc.build_key_info(bk)
        self.assertEqual(out[0], '  --wdbfile-auto  <double>')
        self.assertIn('    **Retired**: It weighted each dataset by n_d^-a.',
                      out)
        self.assertIn('    **Replaced by**: WDBFILE', out)
        self.assertIn('    **Conversion**: WDBFILE n_d^-a, one per dataset.',
                      out)
        self.assertFalse(any('**Default**' in l or '**Examples**' in l
                             for l in out))
        data = {'value_type': ['DOUBLE'], 'value_format': ['<double>'],
                'value_N': [1], 'examples': [],
                'retired': dict(bk.retired)}
        parsed = self.gc.build_blueprint_key('train.wdbfile_auto', data)
        self.assertEqual(parsed.retired, bk.retired)
        plain = self.gc.build_blueprint_key(
            'train.lambda', {'value_type': ['DOUBLE'],
                             'value_format': ['<double>'], 'value_N': [1]})
        self.assertEqual(plain.retired, {})


if __name__ == '__main__':
    unittest.main()
