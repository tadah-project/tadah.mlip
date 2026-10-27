"""Tests for docs/tools/nfp_checks.py (layout and structure rules).

Each case builds a small Sphinx project with the extension and reads the
warnings of the build. Requires Sphinx and sphinx_rtd_theme.

Run: python3 -m unittest discover -s docs/tools/tests -v
"""
import importlib.util
import json
import os
import re
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
TOOLS = os.path.normpath(os.path.join(HERE, '..'))
SOURCE = os.path.normpath(os.path.join(TOOLS, '..', 'source'))
HAVE_SPHINX = all(importlib.util.find_spec(m) for m in
                  ('sphinx', 'sphinx_rtd_theme'))

LINE_80 = 'OPTIM RCUT2B (1) 4.0 6.5   # ' + 'x' * 51
assert len(LINE_80) == 80


def page(title, *parts, lines=None):
    """An rst page of *parts* (blocks of rst, at the left margin); with
    *lines*, padded with paragraphs to that many source lines."""
    text = f'{title}\n{"=" * len(title)}\n\n' + ''.join(p.rstrip('\n') + '\n\n'
                                                     for p in parts)
    if lines is not None:
        have = len(text.splitlines())
        text += ''.join(f'Line {i}.\n' for i in range(lines - have))
        assert len(text.splitlines()) == lines
    return text


def directive(head, *content, options=()):
    """A directive block: ``.. head`` then options and indented content."""
    text = f'.. {head}\n' + ''.join(f'   {o}\n' for o in options) + '\n'
    return text + ''.join(f'   {line}\n' if line else '\n' for line in content)


def toctree(entries, caption=None, maxdepth=1, titlesonly=False):
    options = [f':maxdepth: {maxdepth}']
    if caption:
        options.append(f':caption: {caption}')
    if titlesonly:
        options.append(':titlesonly:')
    return directive('toctree::', *entries, options=options)


def table(columns):
    lines = []
    for r in range(2):
        for c in range(columns):
            lines.append(('* - ' if c == 0 else '  - ') + f'cell {r}{c}')
    return directive('list-table::', *lines, options=[':header-rows: 1'])


def sections(n):
    return ''.join(f'Section {i}\n---------\n\nText {i}.\n\n' for i in range(1, n + 1))


def build(files, theme_options=None, theme='sphinx_rtd_theme', extra_conf='',
          strict=False):
    """Build a project of *files* (relative path -> text) with nfp_checks;
    returns (exit status, warning lines with the project path removed)."""
    with tempfile.TemporaryDirectory(prefix='tadah_nfp_checks_') as tmp:
        src = os.path.join(tmp, 'src')
        conf = (f'import sys\nsys.path.insert(0, {TOOLS!r})\n'
                "project = 'Fixture'\nextensions = ['nfp_checks']\n"
                f'html_theme = {theme!r}\n'
                f'html_theme_options = {theme_options or {}!r}\n' + extra_conf)
        for name, text in dict(files, **{'conf.py': conf}).items():
            path = os.path.join(src, name)
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, 'w', encoding='utf-8') as fh:
                fh.write(text)
        cmd = [sys.executable, '-m', 'sphinx', '-b', 'html', '-q']
        if strict:
            cmd += ['-W', '--keep-going']
        proc = subprocess.run(cmd + [src, os.path.join(tmp, 'html')],
                              capture_output=True, text=True, timeout=120)
        warnings = [l.replace(src + os.sep, '') for l in proc.stderr.splitlines()
                    if 'WARNING' in l]
        return proc.returncode, warnings


def site_config():
    """extensions, html_theme_options and exclude_patterns of
    docs/source/conf.py, read with small local inputs in place of the CORE
    and HPO checkouts and the Doxygen XML."""
    with tempfile.TemporaryDirectory(prefix='tadah_nfp_conf_') as tmp:
        for name, text in {'CORE/blueprint/blueprint_tadah.toml':
                           '[TADAH]\nversion = "0.0.1"\n',
                           'HPO/blueprint/anneal_keys.toml': '# fixture\n',
                           'xml/index.xml': ''}.items():
            path = os.path.join(tmp, name)
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, 'w', encoding='utf-8') as fh:
                fh.write(text)
        env = dict(os.environ, TADAH_CORE_SOURCE_DIR=os.path.join(tmp, 'CORE'),
                   TADAH_HPO_SOURCE_DIR=os.path.join(tmp, 'HPO'),
                   TADAH_DOXYGEN_XML_DIR=os.path.join(tmp, 'xml'))
        # overrides and tags by keyword: Sphinx 9 requires both, as
        # keywords; Sphinx 7.4 takes either form. A value reads from conf.py
        # once its option is known (html_theme_options is the html
        # builder's, so it is added here). Before the fix: Config.read(confdir)
        # alone, which Sphinx 9.0.4 refuses ('missing 2 required keyword-only
        # arguments'), and Config.init_values(), which Sphinx 7.4 keeps only
        # for compatibility and needs no call here.
        proc = subprocess.run(
            [sys.executable, '-c',
             'import json, sys; from sphinx.config import Config; '
             'from sphinx.util.tags import Tags; '
             'c = Config.read(sys.argv[1], overrides={}, tags=Tags()); '
             'c.add("html_theme_options", {}, "html", (dict,)); '
             'print("CONFIG=" + json.dumps({k: getattr(c, k) for k in '
             '("extensions", "html_theme_options", "exclude_patterns")}))',
             SOURCE], env=env, text=True, capture_output=True, timeout=60)
    if proc.returncode != 0:
        raise AssertionError(f'reading {SOURCE}/conf.py failed:\n{proc.stderr}')
    line = next(l for l in proc.stdout.splitlines() if l.startswith('CONFIG='))
    return json.loads(line[len('CONFIG='):])


def root(nfp_entries, titlesonly=True, other=('other',)):
    return page('Fixture', toctree(other, caption='How to Use Tadah!'),
                toctree(nfp_entries, caption='Nested Fitting',
                        titlesonly=titlesonly))


GOOD = {
    'index.rst': root(['nfp/overview', 'nfp/tutorial/index', 'nfp/reference/index']),
    # Outside the chapter no rule applies.
    'other.rst': page('Other', directive('code-block:: none', LINE_80 + 'x'),
                      table(3), directive('raw:: html', '<b>raw</b>'),
                      lines=300),
    'nfp/overview.rst': page(
        'Overview',
        f"A literal ``{'y' * 40}`` and ``{'z' * 30} {'z' * 30}``.",
        directive('code-block:: none', LINE_80),
        directive('literalinclude:: files/eighty.txt', options=[':language: none']),
        table(2), sections(7), lines=250),
    # A tab expands to the next multiple of 8 columns: 8 + 72 = 80.
    'nfp/files/eighty.txt': LINE_80 + '\n\t' + 't' * 72 + '\n',
    'nfp/tutorial/index.rst': page('Tutorial', 'Units.', toctree(['unit1'])),
    'nfp/tutorial/unit1.rst': page('Unit 1', lines=200),
    'nfp/reference/index.rst': page('Reference', 'Keys.', toctree(['keys'])),
    'nfp/reference/keys.rst': page('Keys', sections(9), lines=400),
}


@unittest.skipUnless(HAVE_SPHINX, 'Sphinx and sphinx_rtd_theme are required')
class ChapterRulesTest(unittest.TestCase):
    def test_a_conforming_chapter_builds_with_W(self):
        status, warnings = build(GOOD, {'navigation_depth': 2}, strict=True)
        self.assertEqual(warnings, [])
        self.assertEqual(status, 0)

    def test_each_rule_is_a_warning_at_its_page(self):
        files = dict(GOOD)
        files['index.rst'] = root(['nfp/overview', 'nfp/tutorial/index',
                                   'nfp/reference/index', 'nfp/bad',
                                   'nfp/long', 'nfp/many', 'nfp/examples/index'],
                                  titlesonly=False)
        files['nfp/bad.rst'] = page(
            'Bad',
            directive('|b| raw:: html', '<b>b</b>'),
            f"Raw by substitution: |b|. A literal ``{'q' * 41}``.",
            directive('code-block:: none', LINE_80 + 'x'),
            directive('literalinclude:: files/long.txt',
                      options=[':language: none']),
            directive('parsed-literal::', 'OPTIM *RCUT2B*'),
            directive('raw:: html', '<b>raw</b>'),
            table(3),
            toctree(['leaf']))
        files['nfp/files/long.txt'] = (LINE_80 + 'xx\n\t'
                                       + 't' * 73 + '\n')
        files['nfp/leaf.rst'] = page('Leaf')
        files['nfp/long.rst'] = page('Long', lines=251)
        files['nfp/many.rst'] = page('Many', sections(8))
        files['nfp/tutorial/index.rst'] = page(
            'Tutorial', 'Units.', toctree(['unit1', 'unit2']))
        files['nfp/tutorial/unit2.rst'] = page('Unit 2', lines=201)
        files['nfp/examples/index.rst'] = page(
            'Examples', toctree(['one'], maxdepth=2), toctree(['two']))
        files['nfp/examples/one.rst'] = page('One')
        files['nfp/examples/two.rst'] = page('Two')
        status, warnings = build(files, {'navigation_depth': 2})
        text = '\n'.join(warnings)
        expected = [
            (r'index\.rst:\d+: WARNING: the "Nested Fitting" toctree has no '
             r':titlesonly:'),
            r'nfp/bad\.rst:\d+: WARNING: line 1 of a code block is 81 characters',
            r'nfp/bad\.rst:\d+: WARNING: line 1 of a code block is 82 characters',
            r'nfp/bad\.rst:\d+: WARNING: line 2 of a code block is 81 characters '
            r'long \(at most 80\): t{50}',
            r'nfp/bad\.rst:\d+: WARNING: parsed-literal block',
            r'nfp/bad\.rst:\d+: WARNING: raw content',
            r'nfp/bad\.rst:\d+: WARNING: inline literal with 41 characters',
            r'nfp/bad\.rst:\d+: WARNING: table with 3 columns',
            r'nfp/bad\.rst:\d+: WARNING: toctree in a page that is not a hub',
            r'nfp/long\.rst: WARNING: the page has 251 source lines \(at most 250',
            r'nfp/many\.rst: WARNING: the page has 8 sections \(at most 7',
            r'nfp/tutorial/unit2\.rst: WARNING: the page has 201 source lines '
            r'\(at most 200',
            r'nfp/examples/index\.rst:\d+: WARNING: a hub holds one toctree; '
            r'this one holds 2',
            r'nfp/examples/index\.rst:\d+: WARNING: the toctree of a hub has '
            r':maxdepth: 1 \(it has 2\)',
        ]
        for pattern in expected:
            self.assertRegex(text, pattern)
        # Two raw nodes (the directive and the substitution) in bad.rst.
        self.assertEqual(len(re.findall(r'nfp/bad\.rst:\d+: WARNING: raw', text)),
                         2, text)
        self.assertEqual(len(warnings), len(expected) + 1, text)
        self.assertNotIn('other.rst', text)

    def test_a_root_document_without_the_chapter_toctree_is_a_warning(self):
        files = {'index.rst': page('Fixture', toctree(['other'], caption='Other')),
                 'other.rst': page('Other')}
        _, warnings = build(files, {'navigation_depth': 2})
        self.assertEqual(len(warnings), 1, warnings)
        self.assertIn('index.rst: WARNING: the root document has no toctree '
                      'captioned "Nested Fitting"', warnings[0])


THREE_LEVELS = {
    'index.rst': root(['nfp/overview']),
    'other.rst': page('Other', 'Section\n-------\n\nSub\n~~~\n\nText.'),
    'nfp/overview.rst': page('Overview'),
}


@unittest.skipUnless(HAVE_SPHINX, 'Sphinx and sphinx_rtd_theme are required')
class SidebarTest(unittest.TestCase):
    def test_entries_deeper_than_two_levels_are_a_warning_on_each_page(self):
        # The theme's default navigation_depth is 4: the subsection of
        # other.rst is the third level under its caption.
        _, warnings = build(THREE_LEVELS)
        self.assertEqual(len(warnings), 1, warnings)
        self.assertRegex(warnings[0], r'^other\.rst: WARNING: the sidebar shows '
                         r'1 entries deeper than 2 levels: level 3 under '
                         r'"How to Use Tadah!": "Sub"')

    def test_two_levels_pass(self):
        status, warnings = build(THREE_LEVELS, {'navigation_depth': 2},
                                 strict=True)
        self.assertEqual((status, warnings), (0, []))

    def test_a_build_without_any_sidebar_is_a_warning(self):
        _, warnings = build(THREE_LEVELS, theme='alabaster',
                            extra_conf="html_sidebars = {'**': []}\n")
        self.assertEqual(len(warnings), 1, warnings)
        self.assertIn('no page has a navigation sidebar', warnings[0])

    def test_the_site_configuration_shows_two_levels(self):
        config = site_config()
        self.assertIn('nfp_checks', config['extensions'])
        options = config['html_theme_options']
        status, warnings = build(THREE_LEVELS, options, strict=True)
        self.assertEqual((status, warnings), (0, []), options)


@unittest.skipUnless(HAVE_SPHINX, 'Sphinx and sphinx_rtd_theme are required')
class ShippedFilesTest(unittest.TestCase):
    def test_files_under_nfp_files_are_not_pages(self):
        # A source file among the shipped files (an example's README.rst)
        # is a file to download or include, not a page: with the
        # exclude_patterns of docs/source/conf.py the build reads it as
        # neither a page outside every toctree nor a Nested Fitting page
        # with a code line longer than 80 characters. Files included from
        # there still are (nfp/overview.rst includes files/eighty.txt).
        exclude = site_config()['exclude_patterns']
        files = dict(GOOD)
        files['nfp/files/examples/pt/README.rst'] = page(
            'Readme', directive('code-block:: none', LINE_80 + 'x'))
        status, warnings = build(files, {'navigation_depth': 2},
                                 extra_conf=f'exclude_patterns = {exclude!r}\n',
                                 strict=True)
        self.assertEqual((status, warnings), (0, []), exclude)


class SiteRootTest(unittest.TestCase):
    def test_the_nested_fitting_toctree_lists_titles_only(self):
        # docs/source/index.rst: the sidebar shows the chapter's pages under
        # "Nested Fitting", not their sections.
        with open(os.path.join(SOURCE, 'index.rst'), encoding='utf-8') as fh:
            text = fh.read()
        blocks = re.findall(r'^\.\. toctree::\n((?:[ \t]+:[^\n]*\n)+)', text,
                            re.MULTILINE)
        nfp = [b for b in blocks if re.search(r':caption:\s*Nested Fitting\s*$',
                                              b, re.MULTILINE)]
        self.assertEqual(len(nfp), 1, blocks)
        self.assertRegex(nfp[0], r'(?m)^\s+:titlesonly:\s*$')


if __name__ == '__main__':
    unittest.main()
