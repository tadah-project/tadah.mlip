"""Rendered-prose voice checks, exercised through Sphinx (including -W).

Run: python3 -m unittest discover -s docs/tools/tests -p test_nfp_voice.py -v
"""
import importlib.util
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest

TOOLS = str(Path(__file__).resolve().parents[1])
HAVE_SPHINX = importlib.util.find_spec('sphinx') is not None


def page(title, body=''):
    return f'{title}\n{"=" * len(title)}\n\n{body}\n'


def build(body='', *, files=None, title='Search', strict=True, twice=False,
          extensions=()):
    """A local, theme-independent project; return status, warnings and HTML."""
    with tempfile.TemporaryDirectory(prefix='tadah_nfp_voice_') as tmp:
        src = Path(tmp) / 'src'
        out = Path(tmp) / 'html'
        conf = (f'import sys\nsys.path.insert(0, {TOOLS!r})\n'
                f'extensions = {["nfp_voice", *extensions]!r}\n'
                "project = 'Fixture'\nmaster_doc = 'index'\n"
                "exclude_patterns = ['nfp/files/**', '*.inc']\n"
                "smartquotes = True\nshow_warning_types = True\n")
        inputs = {
            'conf.py': conf,
            'index.rst': page('Fixture', '.. toctree::\n\n   nfp/page\n'),
            'nfp/page.rst': page(title, body),
        }
        inputs.update(files or {})
        for name, content in inputs.items():
            path = src / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(content, encoding='utf-8')
        cmd = [sys.executable, '-m', 'sphinx', '-b', 'html', '-q', '-n',
               '-d', str(Path(tmp) / 'doctrees')]
        if strict:
            cmd += ['-W', '--keep-going']
        result = None
        for _ in range(2 if twice else 1):
            proc = subprocess.run(cmd + [str(src), str(out)],
                                  capture_output=True, text=True, timeout=120,
                                  env=dict(os.environ, NO_COLOR='1'))
            # A second build with no changed source must still check prose.
            if twice and result is not None:
                if proc.returncode != result[0]:
                    raise AssertionError(f'incremental status changed: {proc.stderr}')
            html = out / 'nfp/page.html'
            result = (proc.returncode, proc.stderr.replace(str(src) + '/', ''),
                      html.read_text(encoding='utf-8') if html.is_file() else '')
        return result


@unittest.skipUnless(HAVE_SPHINX, 'Sphinx is required')
class RenderedVoiceTest(unittest.TestCase):
    def voice_warnings(self, warnings):
        return [line for line in warnings.splitlines()
                if '[nfp_voice.prose]' in line]

    def assert_words(self, warnings, expected):
        lines = self.voice_warnings(warnings)
        found = []
        for line in lines:
            self.assertRegex(line, r'\S+:\d+: WARNING: impersonal voice: ')
            match = re.search(r'impersonal voice: "([^"]+)" in rendered prose', line)
            self.assertIsNotNone(match, line)
            found.append(match.group(1).lower().replace('\N{RIGHT SINGLE QUOTATION MARK}', "'"))
        self.assertCountEqual(found, expected, warnings)

    def test_every_forbidden_word_case_insensitively_makes_W_fail(self):
        status, warnings, html = build(
            "YOU, Your; yours! Yourself? WE / Our / US / LET'S.")
        self.assertEqual(status, 1, warnings)
        self.assert_words(warnings, ['you', 'your', 'yours', 'yourself',
                                    'we', 'our', 'us', "let's"])
        self.assertIn('Search', html)
        self.assertIn('nfp/page.rst:4:', warnings)

    def test_impersonal_prose_and_word_boundaries_pass(self):
        status, warnings, _ = build(
            'The search runs with a fixed budget. Young weavers use the usual '
            'hours. Ourselves and yourselves are distinct words. '
            'The identifiers user_our and we2 are names.')
        self.assertEqual((status, warnings), (0, ''))

    def test_titles_headings_captions_tables_lists_and_quotations_are_prose(self):
        body = '''Our heading
-----------

.. code-block:: text
   :caption: Your output

   you we us

.. figure:: /pixel.svg
   :alt: A square

   Yours appears in a caption.

.. list-table:: We compare

   * - yourself
     - a value

us
   A definition.

.. note:: Your budget matters.

.. rubric:: Our results

.. epigraph::

   We quote prose, not program output.

* Your first item.

A footnote [#]_.

.. [#] Our footnote has prose.
'''
        status, warnings, _ = build(body, title='You and the Search', files={
            'pixel.svg': '<svg xmlns="http://www.w3.org/2000/svg"/>'})
        self.assertEqual(status, 1, warnings)
        self.assert_words(warnings, ['you', 'our', 'your', 'yours', 'we',
                                    'yourself', 'us', 'your', 'our', 'we',
                                    'your', 'our'])

    def test_code_output_math_names_comments_and_unused_substitutions_are_not_prose(self):
        body = '''The key ``YOUR`` and :code:`US` are literals.

.. code-block:: bash

   echo you your yours yourself we our us "let's"

.. code-block:: text

   You supply your file.

.. literalinclude:: files/output.txt
   :language: text

>>> print('you we our')
you we our

The expression :math:`you + we` has symbols.

.. math::

   your + us

.. A comment: you and we.

.. |unused| replace:: Your unused replacement.

.. hpo:key:: ROOT.US
   :names: US
   :text: US

   The key takes a value.

The key :hpo:key:`US` names the setting.
'''
        status, warnings, _ = build(body, extensions=['hpotarget_domain'], files={
            'nfp/files/output.txt': 'you we our us\n'})
        self.assertEqual((status, warnings), (0, ''))

    def test_urls_are_excluded_but_link_labels_and_emphasis_remain_prose(self):
        body = '''https://example.org/you/our/us

`https://example.org/your <https://example.org/your>`_

us@example.org

`Your manual <https://example.org/you>`_ and *our* results.

**We** describe the search; `you <https://example.org/us>`_ can read it.
'''
        status, warnings, _ = build(body)
        self.assertEqual(status, 1, warnings)
        self.assert_words(warnings, ['your', 'our', 'we', 'you'])

    def test_only_citation_markers_are_excluded_not_citation_body_prose(self):
        status, warnings, _ = build('''The cited work [us]_ describes fitting.

.. [us] A. Example. A fitting method.

The other work [We]_ has a description.

.. [We] Our method describes fitting.
''')
        self.assertEqual(status, 1, warnings)
        self.assert_words(warnings, ['our'])

    def test_generated_reference_topics_and_annotations_have_no_blanket_exemption(self):
        body = '''.. hpo:key:: ROOT.US
   :text: US
   :annotation: Your setting

   We describe the generated key.

.. hpo:topic:: RULE.TEST
   :text: Our rule

   The example uses your value.

:hpo:topic:`RULE.TEST` describes the rule.
'''
        status, warnings, _ = build('', extensions=['hpotarget_domain'], files={
            'index.rst': page('Fixture', '.. toctree::\n\n   nfp/reference/keys\n'),
            'nfp/page.rst': ':orphan:\n\n' + page('Search'),
            'nfp/reference/keys.rst': page('Reference', body)})
        self.assertEqual(status, 1, warnings)
        self.assert_words(warnings, ['your', 'we', 'our', 'your', 'our'])
        self.assertRegex(warnings, r'nfp/reference/keys\.rst:\d+: WARNING:')

    def test_resolved_reference_text_is_checked_in_the_chapter_only(self):
        status, warnings, html = build('The title :ref:`outside` names the guide.', files={
            'outside.rst': ':orphan:\n\n.. _outside:\n\n' + page('Your Guide')})
        self.assertEqual(status, 1, warnings)
        self.assert_words(warnings, ['your'])
        self.assertNotRegex(warnings, r'(?m)^outside.rst:.*nfp_voice')
        self.assertIn('Your Guide', html)

    def test_included_and_substituted_prose_is_checked_with_source_locations(self):
        status, warnings, _ = build('''.. include:: /part.inc

The |phrase| is visible.

.. |phrase| replace:: your choice
''', files={'part.inc': 'We describe the included search.\n'})
        self.assertEqual(status, 1, warnings)
        self.assert_words(warnings, ['we', 'your'])
        self.assertIn('part.inc:1: WARNING: impersonal voice:', warnings)
        self.assertRegex(warnings, r'nfp/page\.rst:\d+: WARNING: impersonal voice:')

    def test_inline_boundaries_and_curly_apostrophes_use_rendered_words(self):
        status, warnings, _ = build(
            "**yo**\u200bu is not a pronoun. **you**\\ng is one word. "
            "*we*\\aver is one word. let\N{RIGHT SINGLE QUOTATION MARK}s compare; **your** value.")
        self.assertEqual(status, 1, warnings)
        self.assert_words(warnings, ["let's", 'your'])

    def test_image_alt_and_explicit_download_labels_are_prose(self):
        status, warnings, _ = build('''.. image:: /pixel.svg
   :alt: Your search diagram

The :download:`Our input <files/us.txt>` is available.
''', files={'pixel.svg': '<svg xmlns="http://www.w3.org/2000/svg"/>',
            'nfp/files/us.txt': 'input\n'})
        self.assertEqual(status, 1, warnings)
        self.assert_words(warnings, ['your', 'our'])

    def test_unused_image_substitutions_and_non_html_prose_are_not_rendered(self):
        status, warnings, _ = build('''The search has a budget.

.. |unused| image:: /pixel.svg
   :alt: Your unused diagram

.. only:: latex

   We describe a different output.

The :download:`files/us.txt` is available.
''', files={'pixel.svg': '<svg xmlns="http://www.w3.org/2000/svg"/>',
            'nfp/files/us.txt': 'input\n'})
        self.assertEqual((status, warnings), (0, ''))

    def test_cross_page_citations_do_not_exempt_ordinary_bracketed_links(self):
        status, warnings, _ = build(
            'The work [us]_ is cited; `[We] <https://example.org/paper>`_ '
            'is an ordinary link.', files={
                'bibliography.rst': ':orphan:\n\n' + page(
                    'Bibliography', '.. [us] A. Example. A paper.')})
        self.assertEqual(status, 1, warnings)
        self.assert_words(warnings, ['we'])

    def test_toctree_captions_and_explicit_titles_are_prose(self):
        status, warnings, _ = build('''.. toctree::
   :caption: Our Guides

   Your guide <leaf>
''', files={'nfp/leaf.rst': page('Leaf')})
        self.assertEqual(status, 1, warnings)
        self.assert_words(warnings, ['our', 'your'])

    def test_incremental_build_checks_unchanged_rendered_prose(self):
        status, warnings, _ = build('Your search has a budget.', twice=True)
        self.assertEqual(status, 1, warnings)
        self.assert_words(warnings, ['your'])

    def test_lexical_check_does_not_claim_to_recognise_imperative_grammar(self):
        status, warnings, _ = build('Run the search.', title='Set the Budget')
        self.assertEqual((status, warnings), (0, ''))


if __name__ == '__main__':
    unittest.main()
