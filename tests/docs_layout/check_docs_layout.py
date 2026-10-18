#!/usr/bin/env python3
"""The site's stylesheet keeps pages free of horizontal scrolling.

    check_docs_layout.py --docs-source <docs/source> --workdir <scratch>

Builds a small documentation project with the theme, theme options, static
path and stylesheets of the real configuration (<docs-source>/conf.py) and
checks it with docs/tools/check_layout.py at 1280, 1024, 900, 768 and
390 px. The project holds what the pages hold:

  * code lines of exactly 80 characters (a command, and a run of digits
    that has no place to break) at the top level, in a literal block, in a
    bullet list, in a bullet list inside a bullet list, in a definition
    list, in a numbered list, in a note and from a ``literalinclude``;
  * a two-column table with long inline literals, a paragraph with a
    40-character inline literal, display maths and an image wider than the
    text column.

The stylesheet must fit all of it at every width: 80-character lines are
shown whole on desktop windows (1024 px and wider) and wrap on narrower
ones. One page, wide.html, holds a 120-character line: it must scroll at
1280 and 1024 px (code lines are not wrapped on desktop windows) and fit,
wrapped, at 900, 768 and 390 px -- which also shows that the check sees
an overflow.

The scratch directory is <workdir> (the source tree is never written).
Without Sphinx and sphinx_rtd_theme, or without a Chromium-family browser,
it exits 77, which CTest reports as Skipped.
"""
import argparse
import importlib.util
import json
import os
import shutil
import struct
import subprocess
import sys
import zlib

SKIP = 77
HERE = os.path.dirname(os.path.abspath(__file__))

RULER = ''.join(str(i % 10) for i in range(1, 81))
COMMAND = ('tadah hpo --config config.train --validation config.val '
           '--hpotarget config_a.hpo')
WIDE = ' '.join(['OPTIM', 'RCUT2B', '(1)', '4.0', '6.5'] * 6)[:120]
assert len(RULER) == 80 and len(COMMAND) == 80 and len(WIDE) == 120


def block(lines, indent, language='none'):
    pad = ' ' * indent
    body = '\n'.join(pad + '   ' + line for line in lines)
    return f'{pad}.. code-block:: {language}\n\n{body}\n'


PAGES = {
    'index.rst': '''Layout fixture
==============

.. toctree::
   :maxdepth: 1
   :caption: Pages

   code
   wide
''',
    'code.rst': f'''Code Lines of 80 Characters
===========================

At the top level:

{block([COMMAND], 0, 'bash')}
{block([RULER], 0)}
A literal block::

   {RULER}

* In a bullet list.

{block([RULER], 2)}
  * In a bullet list inside a bullet list.

{block([RULER], 4)}
Term
   In a definition list.

{block([RULER], 3)}
#. In a numbered list.

{block([COMMAND], 3, 'bash')}
.. note::

   In a note.

{block([RULER], 3)}
From a file:

.. literalinclude:: eighty.txt
   :language: none

Other content
-------------

.. list-table:: Two columns
   :header-rows: 1
   :widths: 30 70

   * - Key
     - Meaning
   * - ``PC_SCRIPT_TIMEOUT``, ``PC_ISOLATION``, ``PC_WORKER_TIMEOUT``
     - ``runs/parameter_search/validation_candidates/best_pot_12.tadah``

An inline literal of 40 characters: ``{RULER[:40]}``.

.. math::

   L = \\frac{{1}}{{N}} \\sum_{{i=1}}^{{N}} w_i \\, (m_i - t_i)^2

.. image:: wide.png
   :alt: An image wider than the text column
''',
    'wide.rst': f'''A Code Line of 120 Characters
=============================

{block([WIDE], 0)}''',
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


def site_settings(docs_source, workdir):
    '''html_theme, html_theme_options, html_static_path (absolute) and
    html_css_files of the real configuration, read with small local inputs
    in place of the CORE and HPO checkouts and the Doxygen XML.'''
    inputs = os.path.join(workdir, 'inputs')
    files = {'CORE/blueprint/blueprint_tadah.toml': '[TADAH]\nversion = "0.0.1"\n',
             'HPO/blueprint/anneal_keys.toml': '# fixture\n',
             'xml/index.xml': ''}
    for name, text in files.items():
        path = os.path.join(inputs, name)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, 'w', encoding='utf-8') as fh:
            fh.write(text)
    env = dict(os.environ, TADAH_CORE_SOURCE_DIR=os.path.join(inputs, 'CORE'),
               TADAH_HPO_SOURCE_DIR=os.path.join(inputs, 'HPO'),
               TADAH_DOXYGEN_XML_DIR=os.path.join(inputs, 'xml'))
    script = '''import json, sys
from sphinx.config import Config
c = Config.read(sys.argv[1])
c.add("html_theme_options", {}, "html", (dict,))
c.add("html_static_path", [], "html", (list, tuple))
c.add("html_css_files", [], "html", (list, tuple))
c.add("html_theme", "alabaster", "html", (str,))
c.init_values()
print("SETTINGS=" + json.dumps({k: getattr(c, k) for k in (
    "html_theme", "html_theme_options", "html_static_path", "html_css_files")}))
'''
    proc = subprocess.run([sys.executable, '-c', script, docs_source], env=env,
                          capture_output=True, text=True, timeout=60)
    if proc.returncode != 0:
        raise RuntimeError(f'reading {docs_source}/conf.py failed:\n{proc.stderr}')
    line = next(l for l in proc.stdout.splitlines() if l.startswith('SETTINGS='))
    settings = json.loads(line[len('SETTINGS='):])
    settings['html_static_path'] = [os.path.join(docs_source, p)
                                    for p in settings['html_static_path']]
    return settings


def expected(problem):
    '''The overflow wide.html must show: its code line scrolls on desktop
    windows.'''
    return (problem.page == 'wide.html' and problem.element.startswith('pre[1]')
            and problem.width in (1280, 1024))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--docs-source', required=True)
    ap.add_argument('--workdir', required=True)
    args = ap.parse_args()
    docs_source = os.path.abspath(args.docs_source)
    sys.path.insert(0, os.path.join(docs_source, '..', 'tools'))
    import check_layout

    missing = [m for m in ('sphinx', 'sphinx_rtd_theme')
               if importlib.util.find_spec(m) is None]
    if missing:
        print(f'SKIP: the Python module(s) {", ".join(missing)} are not installed')
        return SKIP
    if not check_layout.find_browser():
        print('SKIP: no Chromium-family browser on PATH (tried '
              + ', '.join(check_layout.BROWSERS) + ')')
        return SKIP

    workdir = os.path.abspath(args.workdir)
    shutil.rmtree(workdir, ignore_errors=True)
    src = os.path.join(workdir, 'src')
    os.makedirs(src)
    settings = site_settings(docs_source, workdir)
    print('site settings: ' + json.dumps(settings))
    with open(os.path.join(src, 'conf.py'), 'w', encoding='utf-8') as fh:
        fh.write("project = 'Layout fixture'\n")
        for key, value in settings.items():
            fh.write(f'{key} = {value!r}\n')
    for name, text in PAGES.items():
        with open(os.path.join(src, name), 'w', encoding='utf-8') as fh:
            fh.write(text)
    with open(os.path.join(src, 'eighty.txt'), 'w', encoding='utf-8') as fh:
        fh.write(RULER + '\n' + COMMAND + '\n')
    with open(os.path.join(src, 'wide.png'), 'wb') as fh:
        fh.write(png(1600, 40))

    out = os.path.join(workdir, 'html')
    build = subprocess.run([sys.executable, '-m', 'sphinx', '-W', '-b', 'html',
                            src, out], capture_output=True, text=True, timeout=300)
    if build.returncode != 0:
        print(f'FAIL: the fixture project does not build:\n{build.stdout}{build.stderr}')
        return 1

    problems = check_layout.check(out, ['code.html', 'index.html', 'wide.html'])
    for p in problems:
        print(('expected: ' if expected(p) else 'PROBLEM: ') + str(p))
    unexpected = [p for p in problems if not expected(p)]
    seen = {p.width for p in problems if expected(p)}
    if seen != {1280, 1024}:
        print('FAIL: the 120-character line of wide.html must scroll at 1280 '
              f'and 1024 px; it scrolls at {sorted(seen) or "no width"}')
        return 1
    if unexpected:
        print(f'FAIL: {len(unexpected)} overflow(s) the stylesheet must prevent')
        return 1
    print('PASS: 80-character code lines fit at 1280 and 1024 px, code wraps '
          'at 900, 768 and 390 px, and nothing else overflows')
    return 0


if __name__ == '__main__':
    sys.exit(main())
