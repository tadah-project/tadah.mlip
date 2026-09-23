#!/usr/bin/env python3
"""Check the hpo.html redirect in a built HTML tree.

    check_redirect.py <html-output-dir> [--anchors FILE]

hpo.html (docs/source/_extra/hpo.html) redirects the pre-1.5.1 Nested
Fitting page to the pages it was split into.  Its script sends ``#x`` to
``routes[x]`` verbatim when the routes table names ``x``, else to
``default#x``.  This checker reads that same table (the JSON script
``redirect-routes``) and reports

  * hpo.html missing from the build, or a malformed routes table;
  * a meta refresh, canonical link or visible link that does not point at
    the default page, or a missing ``robots noindex``;
  * a target page missing from the build;
  * a route ``page#y`` whose page has no id ``y``, and a route ``x -> page``
    (no fragment) whose page has no id ``x`` -- the old section must live
    on the page it is sent to;
  * an anchor ``x`` of the old page (hpo_legacy_anchors.txt) that is not
    routed and is not an id on the default page.

It is also a Sphinx extension (conf.py lists ``check_redirect`` in
``extensions``).  After every build with the 'html' builder it turns each
problem into a Sphinx warning, so ``sphinx-build -W`` fails.  The page links
to the flat page names of that builder; when another builder (dirhtml,
singlehtml, epub, ...) copies it into its output, the extension warns once
that the redirect is not checked there instead of passing in silence.

Run standalone, it exits 1 when it reports anything.
"""
import argparse
import html.parser
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REDIRECT_PAGE = 'hpo.html'
DEFAULT_ANCHORS = os.path.join(HERE, 'hpo_legacy_anchors.txt')


class _RedirectPage(html.parser.HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.meta = []          # attribute dicts of <meta>
        self.links = []         # attribute dicts of <link>
        self.anchors = []       # href of <a>
        self.routes_json = None
        self._in_routes = False

    def handle_starttag(self, tag, attrs):
        a = {k: (v or '') for k, v in attrs}
        if tag == 'meta':
            self.meta.append(a)
        elif tag == 'link':
            self.links.append(a)
        elif tag == 'a' and 'href' in a:
            self.anchors.append(a['href'])
        elif tag == 'script' and a.get('id') == 'redirect-routes':
            self._in_routes = True
            self.routes_json = ''

    def handle_endtag(self, tag):
        if tag == 'script':
            self._in_routes = False

    def handle_data(self, data):
        if self._in_routes:
            self.routes_json += data


class _Ids(html.parser.HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.ids = set()

    def handle_starttag(self, tag, attrs):
        for k, v in attrs:
            if k == 'id' and v:
                self.ids.add(v)


def page_ids(path):
    '''Return the set of id attributes in the HTML file at *path*.'''
    parser = _Ids()
    with open(path, encoding='utf-8') as fh:
        parser.feed(fh.read())
    return parser.ids


def read_anchors(path):
    '''Read one anchor per line; '#' starts a comment.'''
    anchors = []
    with open(path, encoding='utf-8') as fh:
        for line in fh:
            line = line.split('#', 1)[0].strip()
            if line:
                anchors.append(line)
    return anchors


def _refresh_url(content):
    m = re.match(r'''^\s*\d+\s*;\s*url\s*=\s*['"]?([^'"]+)['"]?\s*$''',
                 content, re.IGNORECASE)
    return m.group(1) if m else None


def check(outdir, anchors_file=DEFAULT_ANCHORS):
    '''Return a list of problems with the redirect in the built *outdir*.'''
    page = os.path.join(outdir, REDIRECT_PAGE)
    if not os.path.isfile(page):
        return [f"{page} is missing (html_extra_path should copy "
                f"docs/source/_extra/{REDIRECT_PAGE})"]
    parsed = _RedirectPage()
    with open(page, encoding='utf-8') as fh:
        parsed.feed(fh.read())

    if parsed.routes_json is None:
        return [f"{REDIRECT_PAGE} has no <script id=\"redirect-routes\"> table"]
    try:
        table = json.loads(parsed.routes_json)
        default = table['default']
        routes = table['routes']
        if not isinstance(default, str) or not isinstance(routes, dict):
            raise TypeError('default must be a string and routes an object')
    except (ValueError, KeyError, TypeError) as err:
        return [f"{REDIRECT_PAGE}: malformed routes table ({err})"]

    problems = []
    refresh = [m for m in parsed.meta
               if m.get('http-equiv', '').lower() == 'refresh']
    if len(refresh) != 1 or _refresh_url(refresh[0].get('content', '')) != default:
        problems.append(f"{REDIRECT_PAGE}: expected one meta refresh to "
                        f"{default}, found {[m.get('content') for m in refresh]}")
    canonical = [l.get('href') for l in parsed.links
                 if 'canonical' in l.get('rel', '').lower().split()]
    if canonical != [default]:
        problems.append(f"{REDIRECT_PAGE}: expected rel=canonical {default}, "
                        f"found {canonical}")
    if not any(m.get('name', '').lower() == 'robots'
               and 'noindex' in m.get('content', '').lower()
               for m in parsed.meta):
        problems.append(f"{REDIRECT_PAGE}: no <meta name=\"robots\" "
                        "content=\"noindex\">")
    if default not in parsed.anchors:
        problems.append(f"{REDIRECT_PAGE}: no visible link to {default}")

    ids = {}
    pages = {default} | {t.split('#', 1)[0] for t in routes.values()}
    for target_page in sorted(pages):
        path = os.path.join(outdir, target_page)
        if os.path.isfile(path):
            ids[target_page] = page_ids(path)
        else:
            problems.append(f"target page {target_page} is missing from the build")

    def resolve(anchor, origin):
        if anchor in routes:
            target_page, _, fragment = routes[anchor].partition('#')
            target = routes[anchor]
            needed = fragment or anchor   # a page-only route: the section lives there
        else:
            target_page, needed = default, anchor
            target = f"{default}#{anchor}"
        if target_page in ids and needed not in ids[target_page]:
            problems.append(f"{origin} #{anchor} is sent to {target}, but "
                            f"{target_page} has no id \"{needed}\"")

    for anchor in routes:
        resolve(anchor, 'routed fragment')
    for anchor in read_anchors(anchors_file):
        resolve(anchor, f"old anchor ({os.path.basename(anchors_file)})")
    return problems


def verify_build(app, exception):
    '''Sphinx 'build-finished' handler: report problems as warnings.'''
    if exception is not None:
        return
    from sphinx.util import logging
    logger = logging.getLogger(__name__)
    if app.builder.name == 'html':
        for problem in check(app.outdir):
            logger.warning('hpo.html redirect: %s', problem)
    elif os.path.isfile(os.path.join(app.outdir, REDIRECT_PAGE)):
        logger.warning("hpo.html redirect: not checked for the '%s' builder; "
                       "the page links to the flat page names of the 'html' "
                       "builder, which this output may not have",
                       app.builder.name)


def setup(app):
    '''Sphinx extension entry point.'''
    app.connect('build-finished', verify_build)
    return {'parallel_read_safe': True, 'parallel_write_safe': True}


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('outdir', help='HTML output directory of sphinx-build')
    ap.add_argument('--anchors', default=DEFAULT_ANCHORS,
                    help='file of old hpo.html anchors (default: %(default)s)')
    args = ap.parse_args(argv)
    problems = check(args.outdir, args.anchors)
    for p in problems:
        print(f"check_redirect: {p}", file=sys.stderr)
    if problems:
        return 1
    print(f"check_redirect: {REDIRECT_PAGE} OK in {args.outdir}")
    return 0


if __name__ == '__main__':
    sys.exit(main())
