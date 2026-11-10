"""Wording rules of the documentation that a pattern decides.

The prose of every page under docs/source (not the code, the literal blocks
and the program output) and the help text of the CORE and HPO blueprints
(the `description`, `long_description` and `doc` fields, which the generated
pages and the command-line help show) never address the reader, never use
the filler, process and invented words the documentation rules ban, and
never tell history. No hand-written page carries a version note
(`versionchanged`, `versionadded`, `versionremoved`, `deprecated`): a page
states what the program does, and the changelog holds the difference
between versions.

The blueprint cases read the CORE and HPO checkouts the documentation build
uses (source_locator.py) and are skipped, saying why, without one; a
$TADAH_CORE_SOURCE_DIR or $TADAH_HPO_SOURCE_DIR that names no checkout is a
failure.

Run: python3 -m unittest discover -s docs/tools/tests -v
"""
import importlib.util
import os
import re
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
SOURCE = os.path.normpath(os.path.join(HERE, '..', '..', 'source'))
CONFIG = os.path.join(SOURCE, 'config')
HAVE_TOMLLIB = importlib.util.find_spec('tomllib') is not None
HELP_FIELDS = ('description', 'long_description', 'doc')

READER = re.compile(r"\b(you|your|yours|yourself|we|our|us|let's)\b", re.I)
FILLER = re.compile(
    r"\b(simply|easily|in order to|note that|utilis\w+|utiliz\w+|"
    r"leverag\w+|facilitat\w+|comprehensive|crucial|pivotal|vital|"
    r"streamlin\w+|empower\w*|unlock\w*|harness\w*|journey|realm|myriad|"
    r"plethora|boasts|elevate|foster\w*|holistic|intricate|nuanced|"
    r"meticulous|dive into|in summary|in conclusion)\b", re.I)
PROCESS = re.compile(
    r"\b(TODO|FIXME|TBD|WIP|next pass|follow-up|deferred|as requested|"
    r"we decided|for now)\b", re.I)
JARGON = re.compile(
    r"\b(outer-search file|root directives?|subblocks?|run policy|"
    r"layout template|composite driver|result contract|stanza|posture|"
    r"knobs?)\b", re.I)
HISTORY = re.compile(
    r"\b(no longer|previously|used to|legacy|former|earlier versions)\b",
    re.I)
RULES = (('addresses the reader', READER), ('filler wording', FILLER),
         ('process wording', PROCESS), ('invented jargon', JARGON),
         ('tells history', HISTORY))
# A version note, also as the first thing of a list item or a table cell.
# docutils reads directive names in any case.
VERSION_NOTE = re.compile(
    r'^\s*(?:(?:[-*+]|#\.|\d+\.)\s+)*\.\.\s+'
    r'(?:version(?:changed|added|removed)|deprecated)::', re.I)

# Directives whose body is not prose of the page.
OPAQUE = re.compile(
    r'^\s*\.\.\s+(code-block|sourcecode|literalinclude|parsed-literal|'
    r'math|raw|include|toctree|'
    r'only|autodoxygen\w*|doxygen\w*|csv-table)::')
# A comment, label, target, substitution or footnote: ".." not followed by
# a directive name and "::". The other directives (note, list-table,
# glossary, figure) hold prose, which is checked.
COMMENT = re.compile(r'^\s*\.\.(?!\s+\w[\w:+-]*::)(\s|$)')
DIRECTIVE = re.compile(r'^\s*\.\.\s+\w[\w:+-]*::')
INLINE = re.compile(r"``.*?``|:\w+(?::\w+)*:`[^`]*`|`[^`]*`_{0,2}")


def indent(line):
    return len(line) - len(line.lstrip())


def prose_lines(text):
    """Yields (line number, line) of the prose of an rst source: without
    literal blocks, directive bodies that are no prose, comments and
    inline literals."""
    skip_above = None
    previous = ''
    for number, line in enumerate(text.splitlines(), 1):
        if skip_above is not None:
            if not line.strip() or indent(line) > skip_above:
                continue
            skip_above = None
        if OPAQUE.match(line) or COMMENT.match(line):
            skip_above = indent(line)
            continue
        if (previous.rstrip().endswith('::') and not line.strip()
                and not DIRECTIVE.match(previous)):
            skip_above = indent(previous)
            previous = line
            continue
        previous = line
        yield number, INLINE.sub(' ', line)


# What the build generates (git ignores it) or what is no page.
# The generated pages of nfp/reference stand beside its hand-written
# index.rst.
GENERATED = ('config',)
GENERATED_BESIDE = {os.path.join('nfp', 'reference'): 'index.rst'}
NOT_PAGES = ('API', '_extra', '_static', 'images', 'mlips')


def source_pages(not_pages=NOT_PAGES):
    for dirpath, dirs, files in os.walk(SOURCE):
        relative = os.path.relpath(dirpath, SOURCE)
        dirs[:] = sorted(d for d in dirs if d not in not_pages and
                         os.path.normpath(os.path.join(relative, d))
                         not in GENERATED)
        for name in sorted(files):
            generated = (name == 'cli_auto_gen.rst' or
                         (relative in GENERATED_BESIDE and
                          name != GENERATED_BESIDE[relative]))
            if name.endswith('.rst') and not generated:
                yield os.path.join(dirpath, name)


def version_notes(text, label):
    """The version notes of an rst source, as violations. A note quoted in
    a literal block or an inline literal is none."""
    return [f'{label}:{number}: version note: {line.strip()}'
            for number, line in prose_lines(text)
            if VERSION_NOTE.match(line)]


def violations(lines, label):
    found = []
    for number, line in lines:
        for rule, pattern in RULES:
            for match in pattern.finditer(line):
                found.append(f'{label}:{number}: {rule}: '
                             f'"{match.group(0)}" in: {line.strip()}')
    return found


def checkout(repo):
    """The CORE or HPO checkout, or None and why."""
    sys.path.insert(0, CONFIG)
    try:
        import source_locator
    finally:
        sys.path.remove(CONFIG)
    variable = source_locator.REPOS[repo][0]
    try:
        return source_locator.locate(repo), None
    except source_locator.SourceNotFound as err:
        if variable in os.environ:
            raise AssertionError(f'{variable} names no {repo} checkout: '
                                 f'{err}')
        return None, f'no {repo} checkout: {err}'


def help_texts(path):
    import tomllib
    with open(path, 'rb') as fh:
        data = tomllib.load(fh)
    found = []

    def walk(node, where):
        if isinstance(node, dict):
            for key, value in node.items():
                if key in HELP_FIELDS and isinstance(value, str):
                    found.append((f'{where}.{key}', value))
                else:
                    walk(value, f'{where}.{key}')
        elif isinstance(node, list):
            for value in node:
                walk(value, where)

    walk(data, '')
    return found


class ProseRulesTest(unittest.TestCase):
    def test_pages_follow_the_wording_rules(self):
        found = []
        for path in source_pages():
            with open(path, encoding='utf-8') as fh:
                found += violations(prose_lines(fh.read()),
                                    os.path.relpath(path, SOURCE))
        self.assertEqual(found, [], '\n' + '\n'.join(found))

    def test_no_hand_written_page_carries_a_version_note(self):
        # The pages of the API reference are hand-written too.
        not_pages = tuple(d for d in NOT_PAGES if d != 'API')
        found = []
        for path in source_pages(not_pages):
            with open(path, encoding='utf-8') as fh:
                found += version_notes(fh.read(),
                                       os.path.relpath(path, SOURCE))
        self.assertEqual(found, [], '\n' + '\n'.join(found))


@unittest.skipUnless(HAVE_TOMLLIB, 'tomllib needs Python 3.11')
class HelpTextRulesTest(unittest.TestCase):
    def check_blueprints(self, repo):
        root, why = checkout(repo)
        if root is None:
            self.skipTest(why)
        directory = os.path.join(root, 'blueprint')
        found = []
        for name in sorted(os.listdir(directory)):
            if not name.endswith('.toml'):
                continue
            for where, text in help_texts(os.path.join(directory, name)):
                one_line = [(1, INLINE.sub(' ', text))]
                found += violations(one_line, f'{repo}/blueprint/{name}'
                                              f'{where}')
        self.assertEqual(found, [], '\n' + '\n'.join(found))

    def test_core_help_text_follows_the_wording_rules(self):
        self.check_blueprints('CORE')

    def test_hpo_help_text_follows_the_wording_rules(self):
        self.check_blueprints('HPO')


class RuleSelfTest(unittest.TestCase):
    def test_prose_outside_literals_is_checked(self):
        text = ('Intro.\n\nYou can run it.\n\n.. code-block:: bash\n\n'
                '   you --simply\n\n.. versionchanged:: 1.6.0\n\n'
                '   Previously it differed.\n\nThe ``we`` key is simply '
                'listed.\n')
        found = violations(prose_lines(text), 'x.rst')
        self.assertEqual([f.split(': ')[1] for f in found],
                         ['addresses the reader', 'tells history',
                          'filler wording'])

    def test_a_version_note_is_found_at_any_indentation(self):
        text = ('Intro.\n\n.. versionchanged:: 1.6.0\n   It differed.\n\n'
                '* item\n\n  .. versionadded:: 1.6.0\n     New.\n\n'
                '``.. versionchanged::`` in a literal is no note.\n\n'
                '.. deprecated:: 1.6.0\n   A key.\n')
        self.assertEqual(version_notes(text, 'x.rst'), [
            'x.rst:3: version note: .. versionchanged:: 1.6.0',
            'x.rst:8: version note: .. versionadded:: 1.6.0',
            'x.rst:13: version note: .. deprecated:: 1.6.0'])

    def test_every_kind_of_version_note_is_found(self):
        text = ('.. versionremoved:: 2.0\n   Gone.\n\n'
                '.. VersionChanged:: 1.6.0\n   It differed.\n\n'
                '* - .. versionadded:: 1.6.0\n       New.\n\n'
                '#. .. deprecated:: 1.6.0\n      Old.\n')
        self.assertEqual([f.split(': ', 2)[2]
                          for f in version_notes(text, 'x.rst')],
                         ['.. versionremoved:: 2.0',
                          '.. VersionChanged:: 1.6.0',
                          '* - .. versionadded:: 1.6.0',
                          '#. .. deprecated:: 1.6.0'])

    def test_a_note_shown_in_a_literal_block_is_no_note(self):
        text = ('The syntax::\n\n   .. versionchanged:: 1.6.0\n\n'
                '.. code-block:: rst\n\n   .. deprecated:: 1.6.0\n\nProse.\n')
        self.assertEqual(version_notes(text, 'x.rst'), [])

    def test_the_body_of_a_deprecation_note_is_prose(self):
        text = '.. deprecated:: 1.6.0\n   Previously you used it.\n'
        found = violations(prose_lines(text), 'x.rst')
        self.assertEqual([f.split(': ')[1] for f in found],
                         ['addresses the reader', 'tells history'])

    def test_literal_blocks_after_a_double_colon_are_skipped(self):
        text = 'Run it:\n\n::\n\n   we simply go\n\nBack to prose.\n'
        self.assertEqual(violations(prose_lines(text), 'x.rst'), [])

    def test_prose_inside_admonitions_and_tables_is_checked(self):
        text = ('.. note::\n\n   You can run it.\n\n.. list-table::\n'
                '   :header-rows: 1\n\n   * - Key\n     - Previously 0.\n\n'
                '..  warning::\n\n   It is simply run.\n')
        found = violations(prose_lines(text), 'x.rst')
        self.assertEqual([f.split(': ')[1] for f in found],
                         ['addresses the reader', 'tells history',
                          'filler wording'])

    def test_comments_labels_and_substitutions_are_skipped(self):
        text = ('.. _we_label:\n\n.. a comment that says you\n   and we\n\n'
                '.. |we| replace:: you\n\n..\n   you again\n\nProse.\n')
        self.assertEqual(violations(prose_lines(text), 'x.rst'), [])

    def test_hand_written_pages_beside_generated_ones_are_checked(self):
        pages = [os.path.relpath(path, SOURCE) for path in source_pages()]
        reference = os.path.join('nfp', 'reference')
        self.assertIn(os.path.join(reference, 'index.rst'), pages)
        self.assertNotIn(os.path.join(reference, 'target_keys.rst'), pages)
        self.assertNotIn(os.path.join('config', 'config_keys.rst'), pages)


if __name__ == '__main__':
    unittest.main()
