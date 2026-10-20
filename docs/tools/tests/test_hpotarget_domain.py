"""Tests for docs/tools/hpotarget_domain.py (the :hpo: roles and entries).

Each case builds a small Sphinx project with the extension: a page of
entries, written as the reference generator writes them, and a page that
cites them. Requires Sphinx and sphinx_rtd_theme.

Run: python3 -m unittest discover -s docs/tools/tests -v
"""
import importlib.util
import os
import re
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
TOOLS = os.path.normpath(os.path.join(HERE, '..'))
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


if __name__ == '__main__':
    unittest.main()
