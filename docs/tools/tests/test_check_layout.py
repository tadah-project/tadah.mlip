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
    'fields.html': html('<div role="main"><dl class="field-list" style="display: '
                        'grid; grid-template-columns: max-content 90%">'
                        '<dt>A long field name</dt><dd>Body.</dd></dl></div>'),
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
                          'image.html', 'math.html', 'ok.html',
                          'sub/nested.html', 'table.html'])

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
