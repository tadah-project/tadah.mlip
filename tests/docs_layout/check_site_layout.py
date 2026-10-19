#!/usr/bin/env python3
"""The Nested Fitting pages of the built site need no horizontal scroll bar.

    check_site_layout.py --docs-source <docs/source> --doxyfile <Doxyfile>
                         --workdir <scratch> [--doxygen <doxygen>]
                         [--core-source <CORE>] [--hpo-source <HPO>]
                         [--pages GLOB ...]

Builds the documentation as CONTRIBUTING.md describes (*Building the
documentation*) and checks its pages in a headless browser:

  1. Doxygen runs the build's Doxyfile in <workdir>/doxy and writes the XML
     of the API pages to <workdir>/doxy/docs/xml;
  2. ``sphinx-build -W --keep-going -b html`` (this interpreter's Sphinx)
     builds <docs-source> into <workdir>/html, with TADAH_DOXYGEN_XML_DIR
     naming that XML and TADAH_CORE_SOURCE_DIR and TADAH_HPO_SOURCE_DIR the
     sources the build configured (a source that is not given is looked up
     as the documentation build looks it up);
  3. docs/tools/check_layout.py checks the pages under nfp/
     (``nfp/**/*.html``) at 1280, 1024, 900, 768 and 390 px.

Every page source under <docs-source>/nfp (``.rst`` and ``.md`` outside
nfp/files, generated pages included) must be a built page, and every page
is checked. --pages checks the pages its globs select instead (relative to
the site, repeatable), without that comparison.

The scratch directory is <workdir>. The build writes the generated pages
into <docs-source>, as every documentation build does.

Exit status: 0 when every page fits; 1 when the build fails, a page source
is not built, or a page overflows; 77, which CTest reports as Skipped,
without Doxygen (--doxygen not given or not executable), without a Python
module that docs/requirements.txt lists, without a Chromium-family browser,
or, without --pages, while <docs-source>/nfp holds no page source.
"""
import argparse
import importlib.util
import os
import re
import shutil
import subprocess
import sys

SKIP = 77
PAGE_SUFFIXES = ('.rst', '.md')


def required_modules(docs_source):
    '''The Python modules of docs/requirements.txt: each line's name, before
    a version or an extra, with '-' read as '_'.'''
    path = os.path.join(docs_source, '..', 'requirements.txt')
    modules = []
    with open(path, encoding='utf-8') as fh:
        for line in fh:
            name = re.split(r'[<>=!~\[;\s#]', line.strip(), maxsplit=1)[0]
            if name:
                modules.append(name.replace('-', '_'))
    return modules


def page_sources(docs_source):
    '''The pages under <docs_source>/nfp, outside nfp/files, as paths
    relative to <docs_source> without their suffix, sorted.'''
    nfp = os.path.join(docs_source, 'nfp')
    files = os.path.join(nfp, 'files')
    pages = []
    for top, subdirs, names in os.walk(nfp):
        if os.path.commonpath([top, files]) == files:
            subdirs[:] = []
            continue
        for name in names:
            stem, suffix = os.path.splitext(name)
            if suffix in PAGE_SUFFIXES:
                rel = os.path.relpath(os.path.join(top, stem), docs_source)
                pages.append(rel.replace(os.sep, '/'))
    return sorted(pages)


def tail(path, lines=60):
    '''The warnings and errors of the log at *path* (at most 100 lines), or
    its last *lines* lines when it has none.'''
    with open(path, encoding='utf-8', errors='replace') as fh:
        text = fh.readlines()
    flagged = [l for l in text if 'WARNING' in l or 'ERROR' in l]
    return ''.join(flagged[:100] if flagged else text[-lines:])


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--docs-source', required=True)
    ap.add_argument('--doxyfile', required=True)
    ap.add_argument('--workdir', required=True)
    ap.add_argument('--doxygen')
    ap.add_argument('--core-source')
    ap.add_argument('--hpo-source')
    ap.add_argument('--pages', action='append', metavar='GLOB')
    args = ap.parse_args(argv)
    docs_source = os.path.abspath(args.docs_source)
    sys.path.insert(0, os.path.join(docs_source, '..', 'tools'))
    import check_layout

    if not args.doxygen or not shutil.which(args.doxygen):
        print('SKIP: no Doxygen (the build found none when it was configured)')
        return SKIP
    missing = [m for m in required_modules(docs_source)
               if importlib.util.find_spec(m) is None]
    if missing:
        print('SKIP: the Python module(s) ' + ', '.join(missing) + ' of '
              'docs/requirements.txt are not installed')
        return SKIP
    if not check_layout.find_browser():
        print('SKIP: no Chromium-family browser on PATH (tried '
              + ', '.join(check_layout.BROWSERS) + ')')
        return SKIP
    if not args.pages and not page_sources(docs_source):
        print(f'SKIP: {docs_source}/nfp holds no page (.rst or .md outside '
              'nfp/files) to check')
        return SKIP

    workdir = os.path.abspath(args.workdir)
    shutil.rmtree(workdir, ignore_errors=True)
    doxy = os.path.join(workdir, 'doxy')
    os.makedirs(os.path.join(doxy, 'docs'))
    log = os.path.join(workdir, 'doxygen.log')
    print(f'$ (cd {doxy} && {args.doxygen} {args.doxyfile})', flush=True)
    with open(log, 'w', encoding='utf-8') as fh:
        doxygen = subprocess.run([args.doxygen, os.path.abspath(args.doxyfile)],
                                 cwd=doxy, stdout=fh, stderr=subprocess.STDOUT,
                                 timeout=1200)
    xml = os.path.join(doxy, 'docs', 'xml')
    if doxygen.returncode != 0 or not os.path.isfile(os.path.join(xml, 'index.xml')):
        print(f'FAIL: Doxygen exited with {doxygen.returncode} and wrote no '
              f'{xml}/index.xml:\n{tail(log)}')
        return 1

    env = dict(os.environ, TADAH_DOXYGEN_XML_DIR=xml)
    if args.core_source:
        env['TADAH_CORE_SOURCE_DIR'] = os.path.abspath(args.core_source)
    if args.hpo_source:
        env['TADAH_HPO_SOURCE_DIR'] = os.path.abspath(args.hpo_source)
    html = os.path.join(workdir, 'html')
    log = os.path.join(workdir, 'sphinx.log')
    print(f'$ sphinx-build -W --keep-going -b html {docs_source} {html}', flush=True)
    with open(log, 'w', encoding='utf-8') as fh:
        build = subprocess.run([sys.executable, '-m', 'sphinx', '-W', '--keep-going',
                                '-b', 'html', docs_source, html], env=env,
                               stdout=fh, stderr=subprocess.STDOUT, timeout=1800)
    if build.returncode != 0:
        print(f'FAIL: sphinx-build exited with {build.returncode}:\n{tail(log)}')
        return 1

    if args.pages:
        pages = check_layout.select_pages(html, args.pages)
        if not pages:
            print(f'FAIL: no page of {html} matches {args.pages}')
            return 1
    else:
        pages = check_layout.select_pages(html, ['nfp/**/*.html'])
        built = {p[:-len('.html')] for p in pages}
        unbuilt = [s for s in page_sources(docs_source) if s not in built]
        if unbuilt:
            print('FAIL: page source(s) without a built page: '
                  + ', '.join(unbuilt))
            return 1
    print(f'checking {len(pages)} page(s): ' + ', '.join(pages), flush=True)
    try:
        problems = check_layout.check(html, pages)
    except check_layout.LayoutError as err:
        print(f'FAIL: the layout check did not run: {err}')
        return 1
    for p in problems:
        print('PROBLEM: ' + str(p))
    widths = ', '.join(str(w) for w in check_layout.WIDTHS)
    if problems:
        print(f'FAIL: {len(problems)} overflow(s) in {len(pages)} page(s)')
        return 1
    print(f'PASS: {len(pages)} page(s) fit at {widths} px')
    return 0


if __name__ == '__main__':
    sys.exit(main())
