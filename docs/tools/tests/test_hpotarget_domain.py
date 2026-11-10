"""Tests for docs/tools/hpotarget_domain.py (the :hpo: roles and entries).

Each case builds a small Sphinx project with the extension: a page of
entries, written as the reference generator writes them, and a page that
cites them. Requires Sphinx and sphinx_rtd_theme.

Run: python3 -m unittest discover -s docs/tools/tests -v
"""
import collections
import html as html_lib
import importlib.util
import os
import re
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
TOOLS = os.path.normpath(os.path.join(HERE, '..'))
CONFIG = os.path.normpath(os.path.join(TOOLS, '..', 'source', 'config'))
NFP_DOCS = os.path.normpath(os.path.join(TOOLS, '..', '..', 'tests',
                                         'nfp_docs'))
HPO_VAR = 'TADAH_HPO_SOURCE_DIR'
HAVE_SPHINX = all(importlib.util.find_spec(m) for m in
                  ('sphinx', 'sphinx_rtd_theme'))

# Entries as the generator writes them: two keys of one name in two
# blocks, and a removed keyword of that name; a STRATEGY type whose name is
# a removed keyword too; a key cited by a long id, a block, an optimiser, a
# PARAM key, a removed keyword, a library target before its section, an
# entry with a second id, and a topic (a rule) headed by words.
ENTRIES = '''\
Reference
=========

.. hpo:key:: OPTIMIZER.MAXEVAL
   :names: MAXEVAL
   :text: MAXEVAL

   The evaluation budget.

   :Syntax:
      ``MAXEVAL <n>``

.. hpo:key:: OPTIMIZER.SEED
   :names: SEED
   :text: SEED

   The seed of the optimiser.

.. hpo:key:: INIT.SEED
   :names: SEED
   :text: SEED

   The seed of the starting points.

.. hpo:removed:: REMOVED.SEED
   :names: SEED
   :text: SEED
   :hint: moved into an INIT block.
   :scope: key

.. hpo:key:: STRATEGY.TYPE.PRESTAGE
   :names: PRESTAGE
   :text: PRESTAGE

   An exploration, then a refinement.

.. hpo:removed:: REMOVED.PRESTAGE
   :names: PRESTAGE
   :text: PRESTAGE
   :hint: replaced by TYPE PRESTAGE.
   :scope: key

.. hpo:topic:: RULE.one_optimiser
   :text: A block names one optimiser

   A rule between two keys.

.. hpo:key:: PC_OBSERVABLE.cohesive_energy.--max-wall-time
   :names: --max-wall-time
   :text: --max-wall-time

   The wall-clock limit of a relaxation.

.. hpo:key:: ROOT.OPTIM
   :names: OPTIM
   :text: OPTIM
   :also: OPTIM

   A search parameter.

.. hpo:block:: BLOCK.STRATEGY
   :names: STRATEGY
   :text: STRATEGY

   How the search runs its optimiser.

.. hpo:param:: PARAM.ASA:ASA.Temperature_Ratio_Scale
   :names: Temperature_Ratio_Scale
   :text: Temperature_Ratio_Scale

   The main control of the annealing rate.

.. hpo:removed:: REMOVED.INIT_K
   :names: INIT_K
   :text: INIT_K
   :hint: renamed to K_TOTAL inside an INIT block.
   :scope: key

.. hpo:target:: NLOPT
   :kind: opt
   :text: NLOPT

NLOPT
-----

.. hpo:opt:: NLOPT:LN_SBPLX
   :names: LN_SBPLX
   :text: NLOPT:LN_SBPLX

   A local search.
'''


def page(title, text):
    return f'{title}\n{"=" * len(title)}\n\n{text}\n'


def build(files, extensions=('hpotarget_domain',), strict=False):
    """Build a project of *files* (relative path -> text); returns (exit
    status, warning and error lines with the project path removed, {page:
    html})."""
    with tempfile.TemporaryDirectory(prefix='tadah_hpo_domain_') as tmp:
        src = os.path.join(tmp, 'src')
        toctree = ''.join(f'   {n[:-4]}\n' for n in sorted(files)
                          if n != 'index.rst')
        files = dict({'index.rst': page('Fixture', '.. toctree::\n\n'
                                        + toctree)}, **files)
        conf = (f'import sys\nsys.path.insert(0, {TOOLS!r})\n'
                "project = 'Fixture'\n"
                f'extensions = {list(extensions)!r}\n'
                "html_theme = 'sphinx_rtd_theme'\n")
        for name, text in dict(files, **{'conf.py': conf}).items():
            path = os.path.join(src, name)
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, 'w', encoding='utf-8') as fh:
                fh.write(text)
        out = os.path.join(tmp, 'html')
        cmd = [sys.executable, '-m', 'sphinx', '-b', 'html', '-q']
        if strict:
            cmd += ['-W', '--keep-going']
        proc = subprocess.run(cmd + [src, out], capture_output=True,
                              text=True, timeout=120)
        warnings = [l.replace(src + os.sep, '') for l in proc.stderr.splitlines()
                    if 'WARNING' in l or 'ERROR' in l]
        html = {}
        for name in files:
            path = os.path.join(out, name[:-4] + '.html')
            if os.path.isfile(path):
                with open(path, encoding='utf-8') as fh:
                    html[name[:-4]] = fh.read()
        return proc.returncode, warnings, html


def links(html):
    """(href, text) of each link to an entry: an <a> around a <code>."""
    return [(href, re.sub(r'<[^>]+>', '', inner)) for href, inner in
            re.findall(r'<a class="reference internal" href="([^"]+)"[^>]*>'
                       r'(<code[^>]*>.*?</code>)</a>', html)]


def literals(html):
    """The text of each inline literal the page holds."""
    return [re.sub(r'<[^>]+>', '', m) for m in
            re.findall(r'<code class="[^"]*literal[^"]*">(.*?)</code>', html)]


@unittest.skipUnless(HAVE_SPHINX, 'Sphinx and sphinx_rtd_theme are required')
class RoleTest(unittest.TestCase):
    def test_a_unique_name_or_an_id_links_to_its_entry(self):
        # Pins: a bare name of one entry, an id, an option cited by its
        # long id, a block, a bare ALGO, a library, a PARAM key and the
        # second id of an entry each link to their entry and show the name
        # as inline literal text; the build has no warning.
        # Before the fix: the hpo roles do not exist; every role is an
        # error ("Unknown interpreted text role").
        status, warnings, html = build({
            'reference.rst': ENTRIES,
            'guide.rst': page('Guide', '\n\n'.join([
                'A budget :hpo:key:`MAXEVAL`.',
                'By id :hpo:key:`OPTIMIZER.MAXEVAL`.',
                'An option :hpo:key:`PC_OBSERVABLE.cohesive_energy.--max-wall-time`.',
                'A block :hpo:block:`STRATEGY`.',
                'An algorithm :hpo:opt:`LN_SBPLX` of :hpo:opt:`NLOPT`.',
                'A PARAM key :hpo:param:`PARAM.ASA:ASA.Temperature_Ratio_Scale`.',
                'The OPTIM lines :hpo:key:`OPTIM`.',
                'Another text :hpo:key:`the budget <MAXEVAL>`.']))},
            strict=True)
        self.assertEqual((status, warnings), (0, []))
        self.assertEqual(links(html['guide']), [
            ('reference.html#OPTIMIZER.MAXEVAL', 'MAXEVAL'),
            ('reference.html#OPTIMIZER.MAXEVAL', 'MAXEVAL'),
            ('reference.html#PC_OBSERVABLE.cohesive_energy.--max-wall-time',
             '--max-wall-time'),
            ('reference.html#BLOCK.STRATEGY', 'STRATEGY'),
            ('reference.html#NLOPT:LN_SBPLX', 'LN_SBPLX'),
            ('reference.html#NLOPT', 'NLOPT'),
            ('reference.html#PARAM.ASA:ASA.Temperature_Ratio_Scale',
             'Temperature_Ratio_Scale'),
            ('reference.html#OPTIM', 'OPTIM'),
            ('reference.html#OPTIMIZER.MAXEVAL', 'the budget')])
        # The library target names its section, not the entry after it.
        self.assertRegex(html['reference'], r'<section id="nlopt">\s*'
                         r'<span id="NLOPT"></span>\s*<h2>NLOPT')

    def test_the_no_link_form_shows_the_name_without_a_link(self):
        # Pins: '!' gives the inline literal without a link and without the
        # xref class, also when the entry is cited by id.
        status, warnings, html = build({
            'reference.rst': ENTRIES,
            'guide.rst': page('Guide', 'Later :hpo:key:`!MAXEVAL` and '
                              ':hpo:key:`!OPTIMIZER.MAXEVAL` and '
                              ':hpo:block:`!STRATEGY`.')}, strict=True)
        self.assertEqual((status, warnings), (0, []))
        self.assertEqual(links(html['guide']), [])
        self.assertEqual(
            [l for l in literals(html['guide'])],
            ['MAXEVAL', 'MAXEVAL', 'STRATEGY'])
        self.assertNotIn('xref', re.search(r'<p>Later.*?</p>', html['guide'],
                                           re.S).group(0))

    def test_a_live_entry_wins_over_a_removed_name(self):
        # Pins: a name of one entry that is also a removed keyword cites the
        # entry (TYPE PRESTAGE, the removed top-level PRESTAGE).
        # Before the fix: a warning listed STRATEGY.TYPE.PRESTAGE and
        # REMOVED.PRESTAGE, and the second, as advised, warned again.
        status, warnings, html = build({
            'reference.rst': ENTRIES,
            'guide.rst': page('Guide', 'A type :hpo:key:`PRESTAGE`.')},
            strict=True)
        self.assertEqual((status, warnings), (0, []))
        self.assertEqual(links(html['guide']), [
            ('reference.html#STRATEGY.TYPE.PRESTAGE', 'PRESTAGE')])

    def test_a_topic_is_shown_by_its_title_in_words(self):
        # Pins: an entry headed by words (a rule) is a topic; cited by id
        # with :hpo:topic:, it shows its title as text, not as code, and its
        # entry carries the class topic, which the stylesheet sets in the
        # font of the text.
        # Before the fix: a rule was a key, shown as code.
        status, warnings, html = build({
            'reference.rst': ENTRIES,
            'guide.rst': page('Guide', 'See :hpo:topic:`RULE.one_optimiser`'
                              ' and :hpo:topic:`!RULE.one_optimiser`.')},
            strict=True)
        self.assertEqual((status, warnings), (0, []))
        m = re.search(r'<a class="reference internal" href="reference.html'
                      r'#RULE.one_optimiser"[^>]*>(.*?)</a>', html['guide'])
        self.assertIn('A block names one optimiser', m.group(1))
        self.assertNotIn('<code', m.group(1))
        self.assertEqual(html['guide'].count('A block names one optimiser'),
                         2)
        self.assertRegex(html['reference'], r'<dl class="hpo topic"')

    def test_an_ambiguous_name_is_a_warning_naming_the_candidates(self):
        # Pins: the warning lists the entries of the name, each of which
        # resolves it, and not a removed keyword of that name.
        # Before the fix: it listed REMOVED.SEED too.
        status, warnings, html = build({
            'reference.rst': ENTRIES,
            'guide.rst': page('Guide', 'Line three.\n\nThe :hpo:key:`SEED`.')},
            strict=True)
        self.assertNotEqual(status, 0)
        self.assertEqual(len(warnings), 1, warnings)
        self.assertRegex(warnings[0], r"^guide\.rst:6: WARNING: hpo:key: 'SEED' "
                         r"names 2 entries: INIT\.SEED, OPTIMIZER\.SEED; write "
                         r"the id of one")
        self.assertEqual(links(html['guide']), [])
        self.assertIn('SEED', literals(html['guide']))

    def test_an_unknown_name_is_a_warning_at_its_page_and_line(self):
        status, warnings, _ = build({
            'reference.rst': ENTRIES,
            'guide.rst': page('Guide', 'One :hpo:key:`MAXEVL`.\n\n'
                              'Two :hpo:key:`!NOSUCHKEY`.\n\n'
                              'Three :hpo:key:`STRATEGY`.\n\n'
                              'Four :hpo:opt:`NLOPT:LN_NOSUCH`.')},
            strict=True)
        self.assertNotEqual(status, 0)
        self.assertEqual(len(warnings), 4, warnings)
        self.assertRegex(warnings[0], r"^guide\.rst:4: WARNING: hpo:key: "
                         r"'MAXEVL' is not a key of the target-file reference "
                         r"\(close: MAXEVAL")
        # The no-link form is checked too.
        self.assertRegex(warnings[1], r"^guide\.rst:6: WARNING: hpo:key: "
                         r"'NOSUCHKEY' is not a key")
        # A name of another kind says which role takes it.
        self.assertRegex(warnings[2], r"^guide\.rst:8: WARNING: hpo:key: "
                         r"'STRATEGY' is not a key of the target-file "
                         r"reference \(it is a block: :hpo:block:\)")
        self.assertRegex(warnings[3], r"^guide\.rst:10: WARNING: hpo:opt: "
                         r"'NLOPT:LN_NOSUCH' is not an optimiser of the "
                         r"target-file reference")

    def test_a_removed_name_is_a_warning_with_its_hint(self):
        status, warnings, html = build({
            'reference.rst': ENTRIES,
            'guide.rst': page('Guide', 'Old :hpo:key:`INIT_K`.\n\n'
                              'By id :hpo:key:`REMOVED.INIT_K`.')},
            strict=True)
        self.assertNotEqual(status, 0)
        self.assertEqual(len(warnings), 2, warnings)
        for line, warning in zip((4, 6), warnings):
            self.assertIn(f"guide.rst:{line}: WARNING: hpo:key: '", warning)
            self.assertIn("' was removed from the target file "
                          "(REMOVED.INIT_K): renamed to K_TOTAL inside an INIT "
                          "block.", warning)
        # A removed name is no entry of a role: the text has no link.
        self.assertEqual(links(html['guide']), [])

    def test_a_name_declared_twice_is_a_warning(self):
        status, warnings, _ = build({
            'reference.rst': ENTRIES,
            'more.rst': page('More', '.. hpo:key:: OPTIMIZER.MAXEVAL\n'
                             '   :names: MAXEVAL\n\n   Again.')})
        self.assertEqual(len(warnings), 1, warnings)
        self.assertIn('hpo: entry OPTIMIZER.MAXEVAL is declared twice',
                      warnings[0])

    def test_a_long_id_is_not_a_long_literal_for_the_layout_rules(self):
        # Pins: with nfp_checks, a page of the Nested Fitting chapter may cite
        # an entry by an id longer than 40 characters: the literal the
        # rules see while the page is read is the name, as on the page.
        status, warnings, _ = build({
            'nfp/reference.rst': ENTRIES,
            'nfp/guide.rst': page(
                'Guide', 'A limit :hpo:key:`PC_OBSERVABLE.cohesive_energy.'
                '--max-wall-time` and :hpo:key:`!PC_OBSERVABLE.cohesive_'
                'energy.--max-wall-time`.')},
            extensions=('hpotarget_domain', 'nfp_checks'))
        # No warning about the citing page (the others are about the root
        # document of the fixture, which has no Nested Fitting toctree).
        self.assertEqual(status, 0, warnings)
        self.assertEqual([w for w in warnings if w.startswith('nfp/guide')],
                         [])



def registry():
    """(the target-file registry of the HPO checkout the build finds, None)
    or (None, why there is none). A $TADAH_HPO_SOURCE_DIR that names no
    checkout with the registry is a failure."""
    sys.path.insert(0, CONFIG)
    try:
        import read_hpo_registry
        import source_locator
    finally:
        sys.path.remove(CONFIG)
    try:
        hpo = read_hpo_registry.locate_hpo()
    except source_locator.SourceNotFound as err:
        if HPO_VAR in os.environ:
            raise AssertionError(f'{HPO_VAR} names no HPO checkout with the '
                                 f'registry: {err}')
        return None, f'no HPO checkout with the registry: {err}'
    blueprint = os.path.join(hpo, 'blueprint')
    loader = read_hpo_registry.load_loader(blueprint)
    return loader.load(blueprint), None


def nfp_files():
    sys.path.insert(0, NFP_DOCS)
    try:
        import nfp_files as module
    finally:
        sys.path.remove(NFP_DOCS)
    return module


def code_blocks(html, language):
    """The text of each code block of *language* the page shows."""
    return [html_lib.unescape(re.sub(r'<[^>]+>', '', m)) for m in re.findall(
        rf'<div class="highlight-{language} notranslate"><div '
        r'class="highlight"><pre>(.*?)</pre>', html, re.S)]


REG, NO_REGISTRY = (registry() if HAVE_SPHINX else (None, 'no Sphinx'))


@unittest.skipUnless(HAVE_SPHINX, 'Sphinx and sphinx_rtd_theme are required')
@unittest.skipIf(REG is None, NO_REGISTRY or '')
class RefusalTest(unittest.TestCase):
    """``.. hpo:refusal::`` quotes a refusal of the target file from the
    invalid lines of HPO's registry, which its conformance test checks
    against the parser: the line as a ``none`` block and the message as a
    ``text`` block, wrapped to 80 characters."""

    @classmethod
    def setUpClass(cls):
        invalid = [c for c in REG.cases() if c.kind == 'invalid']
        by_text = collections.defaultdict(list)
        for c in invalid:
            by_text[c.text.strip()].append(c)
        # A one-line invalid text of one entry, with a message longer than
        # a code line; and a text that several entries refuse differently.
        cls.single = next(
            v[0] for k, v in by_text.items() if len(v) == 1
            and '\n' not in k and '\n' not in v[0].error
            and 100 < len(v[0].error) < 400 and len(k) < 60)
        cls.ambiguous = next(
            v for k, v in by_text.items() if '\n' not in k
            and len({c.error for c in v}) > 1
            and len({c.wrap for c in v}) == len(v))

    def build_guide(self, text):
        return build({'nfp/guide.rst': page('Guide', text)},
                     extensions=('hpotarget_domain', 'nfp_checks'))

    def guide_warnings(self, warnings):
        # The fixture's root document has no Nested Fitting toctree, which
        # nfp_checks reports; only the guide's warnings count here.
        return [w for w in warnings if w.startswith('nfp/guide')]

    def test_an_invalid_line_shows_the_message_of_its_entry(self):
        c = self.single
        status, warnings, html = self.build_guide(
            f'.. hpo:refusal::\n\n   {c.text.strip()}\n')
        self.assertEqual(self.guide_warnings(warnings), [])
        self.assertEqual(code_blocks(html['nfp/guide'], 'none'),
                         [c.text.strip() + '\n'])
        shown = code_blocks(html['nfp/guide'], 'text')
        self.assertEqual(len(shown), 1, shown)
        lines = shown[0].rstrip('\n').split('\n')
        files = nfp_files()
        self.assertEqual(lines, files.wrap_output(c.error))
        self.assertGreater(len(lines), 1)
        self.assertTrue(all(len(l) <= 80 for l in lines), lines)
        # The convention of the quotes joins the lines into the message.
        self.assertIsNone(files.match_quote(lines, c.error))

    def test_a_line_of_several_entries_names_its_wrapper(self):
        cases = self.ambiguous
        text = cases[0].text.strip()
        status, warnings, html = self.build_guide(
            f'Line 3.\n\n.. hpo:refusal::\n\n   {text}\n')
        mine = self.guide_warnings(warnings)
        self.assertEqual(len(mine), 1, warnings)
        self.assertRegex(mine[0], r'^nfp/guide\.rst:6: WARNING: hpo:refusal: '
                         rf'"{re.escape(text)}" is an invalid line of '
                         rf'{len(cases)} entries with different messages')
        for c in cases:
            self.assertIn(f':wrap: {c.wrap} ({c.owner})', mine[0])
        c = cases[1]
        status, warnings, html = self.build_guide(
            f'.. hpo:refusal::\n   :wrap: {c.wrap}\n\n   {text}\n')
        self.assertEqual(self.guide_warnings(warnings), [])
        self.assertEqual(code_blocks(html['nfp/guide'], 'text'),
                         ['\n'.join(nfp_files().wrap_output(c.error)) + '\n'])

    def test_the_file_option_shows_the_line_in_its_wrapper(self):
        c = self.single
        status, warnings, html = self.build_guide(
            f'.. hpo:refusal::\n   :file:\n\n   {c.text.strip()}\n')
        self.assertEqual(self.guide_warnings(warnings), [])
        shown = code_blocks(html['nfp/guide'], 'none')
        self.assertEqual(len(shown), 1, shown)
        self.assertIn(c.text.strip(), shown[0])
        if c.wrap != 'none':
            self.assertGreater(len(shown[0].strip().split('\n')), 1)

    def test_a_line_no_entry_refuses_is_a_warning_with_the_closest(self):
        c = self.single
        wrong = c.text.strip() + ' extra'
        status, warnings, html = self.build_guide(
            f'Line 3.\n\n.. hpo:refusal::\n\n   {wrong}\n')
        mine = self.guide_warnings(warnings)
        self.assertEqual(len(mine), 1, warnings)
        self.assertRegex(mine[0], r'^nfp/guide\.rst:6: WARNING: hpo:refusal: '
                         rf'no invalid line of the target-file registry is '
                         rf'"{re.escape(wrong)}" \(closest: ')
        self.assertIn(f'"{c.text.strip()}" (:wrap: {c.wrap}, {c.owner})',
                      mine[0])
        self.assertEqual(code_blocks(html['nfp/guide'], 'text'), [])
        status, warnings, html = self.build_guide(
            f'.. hpo:refusal::\n   :wrap: nosuchwrapper\n\n   '
            f'{c.text.strip()}\n')
        mine = self.guide_warnings(warnings)
        self.assertEqual(len(mine), 1, warnings)
        self.assertIn('with :wrap: nosuchwrapper', mine[0])

    def test_a_refusal_is_a_tested_quote_for_the_lint(self):
        # nfp_checks accepts the text block of the directive as a quote
        # that a test checks; a text block of the page without a marker is
        # a warning.
        c = self.single
        status, warnings, html = self.build_guide(
            f'.. hpo:refusal::\n\n   {c.text.strip()}\n\n'
            '.. code-block:: text\n\n   unmarked\n')
        mine = self.guide_warnings(warnings)
        self.assertEqual(len(mine), 1, warnings)
        self.assertRegex(mine[0], r'^nfp/guide\.rst:8: WARNING: program output')


if __name__ == '__main__':
    unittest.main()
