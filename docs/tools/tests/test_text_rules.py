"""Wording rules of the documentation that a pattern decides.

The prose of every page under docs/source (not the code, the literal blocks
and the program output) and the help text of the CORE and HPO blueprints
(the `description`, `long_description` and `doc` fields, which the generated
pages and the command-line help show) never address the reader, never use
the filler, process and invented words the documentation rules ban, and
never tell history. No hand-written page carries a version note
(`versionchanged`, `versionadded`, `versionremoved`, `deprecated`): a page
states what the program does, and the changelog holds the difference
between versions. For the same reason no page names a release of Tadah!:
a sentence about files of another release names the property of the file
that the program tests, such as a key line it has.

What the release rule finds in the prose (release_names): a number of three
parts (`1.5.0`, `v1.6.0`, `1.3.0-beta.1`, `1.5.x`), a release series (`1.x`),
and the name of the program or of one of its modules followed by a number of
two parts or more, with or without the word version or release (`Tadah! 2.0`,
`Tadah!MLIP 1.6`, `tadah version 1.5`). What it does not find, and a review
has to: a release named without a number (`an older build`), and a number of
two parts without the name before it (`since 1.6`, `before 2.0`), which no
pattern tells from a quantity. What it reports although it is no release of
Tadah!: a number of three parts of a program that OTHER_PROGRAMS does not
list (`NLopt 2.7.1`), a section number of three parts (`2.1.3`) and a date
written `30.09.26`; such a number is written as a literal, or its program is
added to the list. A number in a command, a literal, a code block or program
output is no prose and is not read.

The prose is read by paragraph, as docutils reads it: an inline literal, a
role or a link may wrap over a line end. The text a role or a link shows
(`text <target>`) is prose; its target is not. The body of a footnote is
prose. The introduction of the configuration-keys page, which
config/read_toml.py holds, is read as a page is. The other text of the
generated pages comes from the blueprints: the cases of the help text
apply the wording rules to it, not the release rule.

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
HAVE_PYTOMLPP = importlib.util.find_spec('pytomlpp') is not None
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

# A release number: three parts of at most three digits (a date such as
# 11.11.2026 is none), the last a number or `x`, with a pre-release tail
# (`1.3.0-beta.1`), or a release series (`1.x`).
RELEASE_NUMBER = re.compile(
    r'(?<![\w.])v?\d{1,3}\.\d{1,3}\.(?:\d{1,3}|x)'
    r'(?:-[A-Za-z]+(?:\.\d+)?)?(?![\w])'
    r'|(?<![\w.])\d{1,3}\.x\b')
# The name of the program, or of a module (Tadah!MLIP, Tadah.LAMMPS), with
# a number of two parts or more.
NAMED_RELEASE = re.compile(
    r'\btadah!?[A-Za-z.]*\s+(?:(?:version|release)\s+)?v?\d+\.\d+', re.I)
# Programs whose release a page may name: the number directly after the
# name, or after its spelled-out name in brackets, also over a line end.
# The DFT codes and LAMMPS, whose files Tadah! reads or which it runs, and
# the two tools every build needs.
OTHER_PROGRAMS = ('VASP', 'CASTEP', 'LAMMPS', 'CMake', 'Python')
OTHER_PROGRAM_BEFORE = re.compile(
    r'\b(?:' + '|'.join(OTHER_PROGRAMS) + r')(?:\s+\([^()]*\))?\s+$')

# Directives whose body is not prose of the page.
OPAQUE = re.compile(
    r'^\s*\.\.\s+(code-block|sourcecode|literalinclude|parsed-literal|'
    r'math|raw|include|toctree|'
    r'only|autodoxygen\w*|doxygen\w*|csv-table)::')
# A comment, label, target or substitution: ".." not followed by a
# directive name and "::", nor by the label of a footnote or citation,
# whose body is prose. The other directives (note, list-table, glossary,
# figure) hold prose, which is checked.
COMMENT = re.compile(
    r'^\s*\.\.(?!\s+\w[\w:+-]*::)(?!\s+\[[^\]]+\](\s|$))(\s|$)')
DIRECTIVE = re.compile(r'^\s*\.\.\s+\w[\w:+-]*::')
# Inline markup, which may wrap over a line end: a literal, a role, a link
# or interpreted text.
INLINE = re.compile(r"``.*?``|:\w+(?::\w+)*:`[^`]*`|`[^`]*`_{0,2}", re.S)
# A role (but math) or a link that shows a text of its own: `text <target>`.
TITLED = re.compile(
    r'(?::(?!math:)\w+(?::\w+)*:)?`(?P<title>[^`<]*[^`<\s])\s*<[^`<>]*>`'
    r'_{0,2}', re.S)


def indent(line):
    return len(line) - len(line.lstrip())


def without_inline(match):
    """What replaces inline markup: a space, the text a role or a link
    shows, and as many line ends as the markup had."""
    markup = match.group(0)
    titled = TITLED.fullmatch(markup)
    shown = titled.group('title') if titled else ''
    return (' ' + shown + ' ' +
            '\n' * (markup.count('\n') - shown.count('\n')))


def source_lines(text):
    """Yields (line number, line) of the lines of an rst source that hold
    prose: without literal blocks, directive bodies that are no prose and
    comments. Inline markup is still in them."""
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
        yield number, line


def prose_paragraphs(text):
    """Yields (number of the first line, text) of the paragraphs of the
    prose of an rst source: the lines of source_lines() that follow one
    another without a blank line, joined, with the inline markup replaced
    over the whole paragraph (without_inline), so that markup which wraps
    over a line end is read as docutils reads it. The line ends are kept:
    line n of the text is line first + n of the source."""
    first, lines = None, []
    for number, line in source_lines(text):
        if lines and (not line.strip() or number != first + len(lines)):
            yield first, INLINE.sub(without_inline, '\n'.join(lines))
            lines = []
        if line.strip():
            if not lines:
                first = number
            lines.append(line)
    if lines:
        yield first, INLINE.sub(without_inline, '\n'.join(lines))


def prose_lines(text):
    """Yields (line number, line) of the prose of an rst source: without
    literal blocks, directive bodies that are no prose, comments and
    inline literals."""
    for first, paragraph in prose_paragraphs(text):
        for offset, line in enumerate(paragraph.split('\n')):
            yield first + offset, line


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


def release_names(text, label):
    """The places where the prose of an rst source names a release of
    Tadah!, as violations."""
    found = []
    for first, paragraph in prose_paragraphs(text):
        spans = []
        for match in RELEASE_NUMBER.finditer(paragraph):
            if not OTHER_PROGRAM_BEFORE.search(paragraph[:match.start()]):
                spans.append(match.span())
        for match in NAMED_RELEASE.finditer(paragraph):
            if not any(start < match.end() and match.start() < end
                       for start, end in spans):
                spans.append(match.span())
        lines = paragraph.split('\n')
        for start, end in sorted(spans):
            # The line of the number, the last word of the match.
            offset = paragraph.count('\n', 0, end - 1)
            name = ' '.join(paragraph[start:end].split())
            found.append(f'{label}:{first + offset}: names a release: '
                         f'"{name}" in: {lines[offset].strip()}')
    return found


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


    def test_no_page_names_a_release_of_tadah(self):
        # The pages of the API reference are hand-written too.
        not_pages = tuple(d for d in NOT_PAGES if d != 'API')
        found = []
        for path in source_pages(not_pages):
            with open(path, encoding='utf-8') as fh:
                found += release_names(fh.read(),
                                       os.path.relpath(path, SOURCE))
        self.assertEqual(found, [], '\n' + '\n'.join(found))


    @unittest.skipUnless(HAVE_PYTOMLPP, 'pytomlpp is not installed')
    def test_the_introduction_of_the_keys_page_follows_the_rules(self):
        # config/read_toml.py holds the hand-written introduction of the
        # generated configuration-keys page.
        sys.path.insert(0, CONFIG)
        try:
            import read_toml
        finally:
            sys.path.remove(CONFIG)
        label = os.path.join('config', 'read_toml.py (header)')
        found = (violations(prose_lines(read_toml.header), label) +
                 release_names(read_toml.header, label))
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

    def test_a_release_of_tadah_named_in_prose_is_found(self):
        text = ('A potential written by Tadah! 1.5.0 has the line.\n\n'
                'Datasets converted before 1.3.0-beta.1 differ, as do\n'
                'those of 1.0.0 to 1.5.x.\n\n'
                'It stays readable by later 1.x releases.\n\n'
                '* - Line of tadah 1.5\n     - The tag v1.6.0.\n\n'
                'The API may change in Tadah! 2.0.\n')
        self.assertEqual([f.split('"')[1]
                          for f in release_names(text, 'x.rst')],
                         ['1.5.0', '1.3.0-beta.1', '1.0.0', '1.5.x', '1.x',
                          'tadah 1.5', 'v1.6.0', 'Tadah! 2.0'])
        self.assertEqual(
            release_names(text, 'x.rst')[0],
            'x.rst:1: names a release: "1.5.0" in: A potential written by '
            'Tadah! 1.5.0 has the line.')

    def test_other_numbers_and_literals_name_no_release(self):
        text = ('The data come from VASP 5.4.4.\n\n'
                'VASP (Vienna Ab initio Simulation Package) 5.4.4 wrote '
                'them.\n\n'
                'A value of 6.241509074e-3, 0.5 or 1.0 1.0 1.0, CMake 3.24,\n'
                'the licence GPL-3.0 and section 2.1 are no release, nor is\n'
                'the date 11.11.2026.\n\n'
                'The tag is ``v1.6.0``::\n\n   git clone -b v1.6.0 url\n\n'
                '.. code-block:: text\n\n   Tadah version  : 1.6.0\n\n'
                'The run of ``tadah hpo`` 2 is the second.\n')
        self.assertEqual(release_names(text, 'x.rst'), [])

    def test_a_number_after_another_program_is_checked_beyond_its_name(self):
        text = ('Converted from a VASP OUTCAR by 1.5.0.\n\n'
                'LAMMPS and Tadah! 1.6.0 agree.\n')
        self.assertEqual([f.split('"')[1]
                          for f in release_names(text, 'x.rst')],
                         ['1.5.0', '1.6.0'])

    def test_a_release_of_a_listed_program_is_none_also_over_a_line_end(self):
        text = ('The build needs CMake 3.24.1 and Python 3.11.4, and CMake\n'
                '3.x reads the presets.\n\n'
                'The reference data were computed with VASP\n'
                '5.4.4 and with CASTEP (Cambridge Serial Total Energy\n'
                'Package) 23.1.1.\n')
        self.assertEqual(release_names(text, 'x.rst'), [])

    def test_the_name_with_a_version_word_or_a_line_end_is_found(self):
        text = ('It differs from Tadah! version 1.5 and from tadah\n'
                'release 1.6, and from the files of Tadah!\n1.4.\n\n'
                'Tadah!MLIP 1.6 and Tadah.LAMMPS 1.7 read it.\n')
        self.assertEqual([f.split(': ', 2)[0] + ' ' + f.split('"')[1]
                          for f in release_names(text, 'x.rst')],
                         ['x.rst:1 Tadah! version 1.5',
                          'x.rst:2 tadah release 1.6', 'x.rst:3 Tadah! 1.4',
                          'x.rst:5 Tadah!MLIP 1.6',
                          'x.rst:5 Tadah.LAMMPS 1.7'])

    def test_what_the_release_rule_cannot_tell(self):
        # Not found: a number of two parts without the name before it, and
        # a release named without a number.
        text = ('Since 1.6 the key is read; it may change before 2.0, and\n'
                'an older build refuses it.\n')
        self.assertEqual(release_names(text, 'x.rst'), [])
        # Reported, though no release of Tadah!: a number of three parts
        # of a program OTHER_PROGRAMS does not list, a section number and
        # a date with a year of two digits. The page writes such a number
        # as a literal.
        text = ('NLopt 2.7.1 runs it (section 2.1.3) since 30.09.26, and\n'
                '``NLopt 2.7.1`` is a literal.\n')
        self.assertEqual([f.split('"')[1]
                          for f in release_names(text, 'x.rst')],
                         ['2.7.1', '2.1.3', '30.09.26'])

    def test_inline_markup_that_wraps_over_a_line_end_is_no_prose(self):
        # The second line begins inside a literal: what follows its end
        # is prose, and what stands before it is not.
        text = ('A potential that ``tadah train`` or ``tadah\n'
                'hpo`` of Tadah! 1.0.0 to 1.5.0 wrote, as you know, has\n'
                '``EWEIGHT`` lines.\n')
        self.assertEqual([f.split(': ', 2)[0] + ' ' + f.split('"')[1]
                          for f in release_names(text, 'x.rst')],
                         ['x.rst:2 1.0.0', 'x.rst:2 1.5.0'])
        found = violations(prose_lines(text), 'x.rst')
        self.assertEqual([f.split(': ')[:2] for f in found],
                         [['x.rst:2', 'addresses the reader']])
        text = ('The release is cloned with ``git clone -b\n'
                'v1.6.0 url`` and :ref:`a label\n<we_1.5.0>`, then ``you\n'
                'simply --run`` it.\n')
        self.assertEqual(release_names(text, 'x.rst'), [])
        self.assertEqual(violations(prose_lines(text), 'x.rst'), [])

    def test_a_paragraph_ends_at_a_blank_line_and_keeps_its_lines(self):
        text = ('One ``literal\n\nNot in it: Tadah! 1.5.0.\n\n'
                '* an item\n* of 1.4.0\n\n'
                '.. note::\n\n   First line.\n   Then 1.3.0 and you.\n')
        self.assertEqual([f.split(' in: ')[0]
                          for f in release_names(text, 'x.rst')],
                         ['x.rst:3: names a release: "1.5.0"',
                          'x.rst:6: names a release: "1.4.0"',
                          'x.rst:11: names a release: "1.3.0"'])
        self.assertEqual([f.split(': ')[:2] for f in
                          violations(prose_lines(text), 'x.rst')],
                         [['x.rst:11', 'addresses the reader']])

    def test_the_text_a_role_or_a_link_shows_is_prose(self):
        text = ('See :ref:`the conversion of 1.5.0 <fit_objective>` and\n'
                '`your notes of Tadah! 1.4 <https://example.org/1.3.0>`_.\n\n'
                'The file :download:`pot.tadah <files/pot_1.5.0.tadah>`,\n'
                ':math:`x <y>` and :ref:`we_1.2.0` are not.\n')
        self.assertEqual([f.split('"')[1]
                          for f in release_names(text, 'x.rst')],
                         ['1.5.0', 'Tadah! 1.4'])
        self.assertEqual([f.split(': ')[:2] for f in
                          violations(prose_lines(text), 'x.rst')],
                         [['x.rst:2', 'addresses the reader']])

    def test_the_body_of_a_footnote_is_prose(self):
        text = ('Text [1]_.\n\n.. [1] Read by Tadah! 1.5.0, as you know.\n'
                '   And by 1.4.0.\n\n.. [Ref2020] Simply cited.\n\n'
                '.. a comment of 1.3.0 for you\n\n.. _label_1.2.0:\n')
        self.assertEqual([f.split('"')[1]
                          for f in release_names(text, 'x.rst')],
                         ['1.5.0', '1.4.0'])
        self.assertEqual([f.split(': ')[1] for f in
                          violations(prose_lines(text), 'x.rst')],
                         ['addresses the reader', 'filler wording'])

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
