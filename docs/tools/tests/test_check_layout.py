"""Tests for docs/tools/check_layout.py (overflow in a headless browser).

The cases that render pages need a local Chromium-family browser and skip
without one. The browser reads only small HTML files written here.

Run: python3 -m unittest discover -s docs/tools/tests -v
"""
import contextlib
import io
import os
import shutil
import struct
import sys
import tempfile
import unittest
from unittest import mock
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.normpath(os.path.join(HERE, '..')))
import check_layout  # noqa: E402

BROWSER = check_layout.find_browser()

STYLE = '''<style>
body { margin: 0; font: 16px sans-serif; }
.wy-nav-side { position: fixed; left: 0; top: 0; width: 100px; }
[role="main"] { margin-left: 110px; width: 260px; }
pre { overflow: auto; font: 12px monospace; }
.wy-table-responsive { overflow: auto; }
div.math { overflow: auto; }
</style>'''


def html(body, style=STYLE):
    return (f'<!DOCTYPE html><html><head><meta charset="utf-8">{style}</head>'
            f'<body>{body}</body></html>')


SIDE = ('<div class="wy-nav-side"><pre>' + 'n' * 200 + '</pre>'
        '<img src="logo.png" width="900" height="5"></div>')


def theme(delay):
    '''A stand-in for the Read the Docs theme's script. The theme defines
    window.SphinxRtdTheme and puts each table.docutils in a
    div.wy-table-responsive from a document-ready handler, which runs after
    the load event. Here the handler runs *delay* ms after the load event,
    as it can on a busy machine; with *delay* None it never runs.'''
    wrap = ('for (const t of document.querySelectorAll("table.docutils")) {'
            ' const box = document.createElement("div");'
            ' box.className = "wy-table-responsive";'
            ' t.replaceWith(box); box.appendChild(t); }')
    late = ('' if delay is None else
            'window.addEventListener("load", () => setTimeout(() => {'
            f' {wrap} }}, {delay}));')
    return f'<script>window.SphinxRtdTheme = {{}}; {late}</script>'


# After the load event, a script widens a block by 25 px in each animation
# frame up to 2000 px (80 frames, about 1.3 s): past the window from frame 47.
GROW = ('<script>window.addEventListener("load", () => {'
        ' const el = document.querySelector(".grow");'
        ' const step = () => { const w = parseInt(el.style.width) + 25;'
        ' el.style.width = w + "px"; if (w < 2000) requestAnimationFrame(step); };'
        ' requestAnimationFrame(step); });</script>')

PAGES = {
    'ok.html': html(SIDE + '<div role="main"><section id="intro"><p>Text.</p>'
                    '<pre>short line</pre></section></div>'),
    'code.html': html('<div role="main"><section id="first"><pre>fits</pre>'
                      '</section><section id="second"><pre>'
                      + 'OPTIM RCUT2B ' + 'x' * 100 + '\nshort</pre>'
                      '</section></div>'),
    'document.html': html('<div role="main"><section id="sec">'
                          '<div class="wide" style="width: 2000px">w</div>'
                          '</section></div>'),
    'image.html': html('<div role="main"><section id="fig">'
                       '<img src="pic.png" alt="pic">'
                       '</section></div>'),
    'table.html': html('<div role="main"><div class="wy-table-responsive">'
                       '<table><tr><td style="white-space: nowrap">'
                       + 'cell ' * 100 + '</td></tr></table></div></div>'),
    'math.html': html('<div role="main"><div class="math">'
                      '<span style="white-space: nowrap">' + 'x+' * 200
                      + '</span></div></div>'),
    'word.html': html('<div role="main"><section id="w"><p>'
                      + 'x' * 300 + '</p></section></div>'),
    # Code blocks that scroll their own long lines, then the element that
    # widens the document.
    'scrolls.html': html('<div role="main"><section id="sec">'
                         + ('<pre>' + 'y' * 200 + '</pre>') * 3
                         + '<div class="wide" style="width: 2000px">w</div>'
                         '</section></div>'),
    'fields.html': html('<div role="main"><dl class="field-list" style="display: '
                        'grid; grid-template-columns: max-content 90%">'
                        '<dt>A long field name</dt><dd>Body.</dd></dl></div>'),
    # The theme puts the table in its box (which scrolls it) 1.5 s after the
    # load event; outside the box the table widens the document.
    'theme_late.html': html(theme(1500) + '<div role="main"><section id="tab">'
                            '<table class="docutils"><tr>'
                            '<td style="white-space: nowrap">' + 'cell ' * 100
                            + '</td></tr></table></section></div>'),
    # The theme never puts the table in its box; it leaves a field list's
    # table out.
    'theme_never.html': html(theme(None) + '<div role="main">'
                             '<table class="docutils"><tr><td>Table.</td></tr>'
                             '</table><table class="docutils field-list"><tr>'
                             '<td>Fields.</td></tr></table></div>'),
    # Without the theme a table stays where it is.
    'no_theme.html': html('<div role="main"><section id="t">'
                          '<table class="docutils"><tr><td>Fits.</td></tr>'
                          '</table></section></div>'),
    'grows.html': html('<div role="main"><section id="sec">'
                       '<div class="grow" style="width: 0px">g</div>'
                       '</section></div>' + GROW),
    'sub/nested.html': html('<div role="main"><p>Nested.</p></div>'),
    '_static/skipped.html': html('<div role="main"><p>Static.</p></div>'),
}


def png(width, height):
    '''A grey PNG image of the given size.'''
    def chunk(kind, data):
        return (struct.pack('>I', len(data)) + kind + data
                + struct.pack('>I', zlib.crc32(kind + data) & 0xffffffff))
    rows = b''.join(b'\0' + b'\x80' * width for _ in range(height))
    return (b'\x89PNG\r\n\x1a\n'
            + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 0, 0, 0, 0))
            + chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b''))


class FixtureTree(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.mkdtemp(prefix='tadah_check_layout_')
        self.addCleanup(shutil.rmtree, self.dir)
        for name, text in PAGES.items():
            path = os.path.join(self.dir, name)
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, 'w', encoding='utf-8') as fh:
                fh.write(text)
        with open(os.path.join(self.dir, 'pic.png'), 'wb') as fh:
            fh.write(png(400, 10))


class SelectPagesTest(FixtureTree):
    def test_every_page_but_underscore_directories_by_default(self):
        self.assertEqual(check_layout.select_pages(self.dir),
                         ['code.html', 'document.html', 'fields.html',
                          'grows.html', 'image.html', 'math.html',
                          'no_theme.html', 'ok.html', 'scrolls.html',
                          'sub/nested.html', 'table.html', 'theme_late.html',
                          'theme_never.html', 'word.html'])

    def test_globs_select_pages(self):
        self.assertEqual(check_layout.select_pages(self.dir, ['sub/**/*.html',
                                                              'ok.html']),
                         ['ok.html', 'sub/nested.html'])
        # Named explicitly, a page under an underscore directory is checked.
        self.assertEqual(check_layout.select_pages(self.dir, ['_static/*.html']),
                         ['_static/skipped.html'])

    def test_no_matching_page_is_a_usage_error(self):
        err = io.StringIO()
        with contextlib.redirect_stderr(err):
            status = check_layout.main([self.dir, '--pages', 'none/*.html'])
        self.assertEqual(status, 2)
        self.assertIn('no page of', err.getvalue())

    def test_widths_are_checked(self):
        self.assertEqual(check_layout.parse_widths('1280,390'), (1280, 390))
        for bad in ('wide', '1280,100', ''):
            with self.assertRaises(Exception):
                check_layout.parse_widths(bad)

    def test_an_unknown_browser_is_an_error(self):
        with self.assertRaises(check_layout.LayoutError):
            check_layout.find_browser('no-such-browser-tadah')

    def test_problem_lines_name_page_element_and_width(self):
        p = check_layout.Problem('nfp/a.html', 'pre[2]#s', 1024, 'too wide')
        self.assertEqual(str(p), 'nfp/a.html:pre[2]#s:1024px: too wide')


@unittest.skipUnless(BROWSER, 'a Chromium-family browser is required')
class RenderTest(FixtureTree):
    def check(self, *pages, widths=(1280, 390)):
        return check_layout.check(self.dir, list(pages), widths)

    def test_a_page_that_fits_has_no_problem(self):
        # The sidebar's wide code block and image are not measured.
        self.assertEqual(self.check('ok.html', 'sub/nested.html'), [])

    def test_a_code_block_that_scrolls(self):
        problems = self.check('code.html')
        self.assertEqual([(p.element, p.width) for p in problems],
                         [('pre[2]#second', 1280), ('pre[2]#second', 390)])
        self.assertRegex(problems[0].detail,
                         r'^scrollWidth \d+ > clientWidth 26\d "OPTIM RCUT2B x+\.\.\."$')

    def test_a_document_wider_than_the_window(self):
        problems = self.check('document.html', widths=(1280,))
        self.assertEqual(len(problems), 1, problems)
        self.assertEqual(problems[0].element, 'document')
        self.assertRegex(problems[0].detail,
                         r'^scrollWidth 2110 > window 1280; div\.wide#sec '
                         r'\(right edge 2110\)$')

    def test_a_word_that_does_not_break_is_named(self):
        # The paragraph's box fits the column; its text reaches past the
        # window.
        problems = self.check('word.html', widths=(1280,))
        self.assertEqual(len(problems), 1, problems)
        self.assertRegex(problems[0].detail,
                         r'^scrollWidth (\d+) > window 1280; p#w '
                         r'\(right edge \1\)$')

    def test_a_code_block_that_scrolls_does_not_widen_the_document(self):
        # Its lines are clipped to its box: the problem names the element
        # that reaches past the window, and each code block only as a block
        # that scrolls.
        problems = self.check('scrolls.html', widths=(1280,))
        document = [p for p in problems if p.element == 'document']
        self.assertEqual(len(document), 1, problems)
        self.assertRegex(document[0].detail,
                         r'^scrollWidth 2110 > window 1280; div\.wide#sec '
                         r'\(right edge 2110\)$')
        self.assertEqual(sorted(p.element for p in problems if p is not document[0]),
                         ['pre[1]#sec', 'pre[2]#sec', 'pre[3]#sec'])

    def test_an_image_wider_than_the_text_column(self):
        problems = self.check('image.html', widths=(1280,))
        self.assertEqual(len(problems), 1, problems)
        self.assertEqual(problems[0].element, 'img[1]#fig')
        self.assertEqual(problems[0].detail,
                         'right edge 510 > text column 370 "pic.png"')

    def test_a_table_and_display_maths_that_scroll(self):
        problems = self.check('table.html', 'math.html', widths=(900,))
        self.assertEqual([(p.page, p.element) for p in problems],
                         [('table.html', 'wy-table-responsive[1]'),
                          ('math.html', 'div.math[1]')])

    def test_a_field_list_wider_than_its_box(self):
        # The body takes 90% of the list's width, the name the rest and more.
        problems = self.check('fields.html', widths=(1280,))
        self.assertEqual([p.element for p in problems], ['dl.field-list[1]'])
        self.assertRegex(problems[0].detail,
                         r'^scrollWidth \d+ > clientWidth 260 "A long field')

    def test_tables_are_measured_in_the_box_the_theme_puts_them_in(self):
        # The theme's script runs well after the load event: the table is
        # measured in its box, which scrolls it, as a reader sees it.
        # Measured before, it would widen the document.
        problems = self.check('theme_late.html', widths=(1280,))
        self.assertEqual([p.element for p in problems],
                         ['wy-table-responsive[1]#tab'], problems)
        self.assertRegex(problems[0].detail,
                         r'^scrollWidth \d+ > clientWidth 260 "cell cell')

    def test_without_the_theme_a_table_is_measured_where_it_is(self):
        self.assertEqual(self.check('no_theme.html', widths=(1280,)), [])

    def test_a_page_is_measured_once_its_layout_stops_changing(self):
        problems = self.check('grows.html', widths=(1280,))
        self.assertEqual(len(problems), 1, problems)
        self.assertEqual(problems[0].element, 'document')
        self.assertRegex(problems[0].detail,
                         r'^scrollWidth 2110 > window 1280; div\.grow#sec '
                         r'\(right edge 2110\)$')

    def test_a_page_that_does_not_become_ready_is_an_error(self):
        err = io.StringIO()
        with mock.patch.object(check_layout, 'READY_TIMEOUT', 1, create=True), \
                contextlib.redirect_stdout(io.StringIO()), \
                contextlib.redirect_stderr(err):
            status = check_layout.main([self.dir, '--pages', 'theme_never.html',
                                        '--widths', '1280'])
        self.assertEqual(status, 2, err.getvalue())
        self.assertRegex(err.getvalue(),
                         r'^check_layout: file://\S+/theme_never\.html: not '
                         r'ready after 1 s: the theme has not put 1 table\(s\) '
                         r'in a div\.wy-table-responsive\n$')

    def test_main_reports_each_problem_and_exits_1(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out), \
                contextlib.redirect_stderr(io.StringIO()):
            status = check_layout.main([self.dir, '--pages', 'code.html',
                                        '--pages', 'ok.html',
                                        '--widths', '1024'])
        self.assertEqual(status, 1)
        lines = out.getvalue().splitlines()
        self.assertEqual(len(lines), 1, lines)
        self.assertRegex(lines[0], r'^code\.html:pre\[2\]#second:1024px: '
                         r'scrollWidth \d+ > clientWidth \d+ "OPTIM')
        with contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(check_layout.main([self.dir, '--pages', 'ok.html']), 0)


if __name__ == '__main__':
    unittest.main()
