"""Wording rules of the documentation that a pattern decides.

The prose of every page under docs/source (not the code, the literal blocks,
the program output and the version directives, which keep history) and the
help text of the CORE and HPO blueprints (the `description`,
`long_description` and `doc` fields, which the generated pages and the
command-line help show) never address the reader, never use the filler,
process and invented words the documentation rules ban, and never tell
history outside a version directive.

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
         ('history outside a version directive', HISTORY))

# Directives whose body is not prose of the page, or keeps history.
OPAQUE = re.compile(
    r'^\s*\.\.\s+(code-block|sourcecode|literalinclude|parsed-literal|'
    r'versionchanged|versionadded|deprecated|math|raw|include|toctree|'
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


def source_pages():
    for dirpath, dirs, files in os.walk(SOURCE):
        relative = os.path.relpath(dirpath, SOURCE)
        dirs[:] = sorted(d for d in dirs if d not in NOT_PAGES and
                         os.path.normpath(os.path.join(relative, d))
                         not in GENERATED)
        for name in sorted(files):
            generated = (name == 'cli_auto_gen.rst' or
                         (relative in GENERATED_BESIDE and
                          name != GENERATED_BESIDE[relative]))
            if name.endswith('.rst') and not generated:
                yield os.path.join(dirpath, name)


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
                         ['addresses the reader', 'filler wording'])

    def test_literal_blocks_after_a_double_colon_are_skipped(self):
        text = 'Run it:\n\n::\n\n   we simply go\n\nBack to prose.\n'
        self.assertEqual(violations(prose_lines(text), 'x.rst'), [])

    def test_prose_inside_admonitions_and_tables_is_checked(self):
        text = ('.. note::\n\n   You can run it.\n\n.. list-table::\n'
                '   :header-rows: 1\n\n   * - Key\n     - Previously 0.\n\n'
                '..  warning::\n\n   It is simply run.\n')
        found = violations(prose_lines(text), 'x.rst')
        self.assertEqual([f.split(': ')[1] for f in found],
                         ['addresses the reader',
                          'history outside a version directive',
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
