#!/usr/bin/env python3
"""Generate and validate compatibility pages from legacy_routes.toml.

    legacy_routes.py HTML_OUTPUT [--routes FILE] [--check]

Paths in the table are relative to the HTML output root. Each page has a
title, the name of the page it leads to, a default destination and an
explicit destination for each recorded anchor.
A destination may name a page or a page and an HTML id. No target may be
another redirect, an external URL, or a path outside the output tree.

Generation validates every target before replacing any output. HTML and
inline JavaScript are deterministic, including when the TOML entries are
reordered. Known fragments are percent-decoded once; unknown or malformed
fragments reach the default destination. The no-script fallback, canonical
link and visible link name the same default destination; the page's
title, heading and link text are the entry's title.

As a Sphinx extension, this module generates pages at build-finished. The
``legacy_routes_file`` setting selects the table. Only the flat ``html``
builder writes the pages, whose paths and links are those of its output;
every other builder (``dirhtml``, ``singlehtml``, ``epub``, non-HTML
builders) writes none, and an HTML builder says so in the build log.
"""
import argparse
import html
from html.parser import HTMLParser
import json
from pathlib import Path, PurePosixPath
import posixpath
import re
import sys
import tempfile

# The table is read with tomllib, in the standard library from Python 3.11;
# the documentation build needs Python 3.11 or later, and Sphinx loads this
# extension before conf.py runs the page generators, so an older
# interpreter stops the build here.
try:
    import tomllib
except ImportError:
    sys.exit(
        "legacy_routes.py needs Python 3.11 or later: it reads "
        "docs/tools/legacy_routes.toml with tomllib, which Python "
        f"{sys.version_info.major}.{sys.version_info.minor} does not have "
        f"({sys.executable}); build the documentation with Python 3.11 or "
        "later")

DEFAULT_ROUTES = Path(__file__).with_suffix('.toml')
_PAGE = re.compile(r'(?:[A-Za-z0-9_-]+/)*[A-Za-z0-9_-]+\.html')
_ANCHOR = re.compile(r'[A-Za-z0-9_][A-Za-z0-9_.:-]*')
_SCRIPT = '''(function () {
  var table = JSON.parse(document.getElementById("redirect-routes").textContent);
  var id;
  try {
    id = decodeURIComponent(window.location.hash.slice(1));
  } catch (error) {
    window.location.replace(table["default"]);
    return;
  }
  var target = Object.prototype.hasOwnProperty.call(table.routes, id)
      ? table.routes[id] : table["default"];
  window.location.replace(target);
})();'''


def _destination(value):
    if not isinstance(value, str):
        raise ValueError(f'destination must be a string: {value!r}')
    page, separator, anchor = value.partition('#')
    if not _PAGE.fullmatch(page):
        raise ValueError(f'invalid HTML path: {value!r}')
    if separator and not _ANCHOR.fullmatch(anchor):
        raise ValueError(f'invalid destination fragment: {value!r}')
    return page, anchor


def _validate_table(table):
    if not isinstance(table, dict) or set(table) != {'version', 'pages'}:
        raise ValueError('table needs exactly version and pages')
    if type(table['version']) is not int or table['version'] != 1:
        raise ValueError('routes version must be 1')
    pages = table['pages']
    if not isinstance(pages, dict) or not pages:
        raise ValueError('pages must be a non-empty table')
    for source, entry in pages.items():
        if not isinstance(source, str) or not _PAGE.fullmatch(source):
            raise ValueError(f'invalid redirect path: {source!r}')
        if (not isinstance(entry, dict)
                or set(entry) != {'default', 'title', 'anchors'}):
            raise ValueError(f'{source} needs exactly default, title and anchors')
        title = entry['title']
        if (not isinstance(title, str) or not title or title != title.strip()
                or not title.isprintable()):
            raise ValueError(f'{source}: the title must be one line of text: '
                             f'{title!r}')
        if not isinstance(entry['anchors'], dict):
            raise ValueError(f'{source}: anchors must be a table')
        for anchor in entry['anchors']:
            if not isinstance(anchor, str) or not _ANCHOR.fullmatch(anchor):
                raise ValueError(f'{source}: invalid anchor {anchor!r}')
        for target in [entry['default'], *entry['anchors'].values()]:
            page, _ = _destination(target)
            if page in pages:
                raise ValueError(f'{source}: target {target!r} is a redirect')
    return table


def load_routes(path=DEFAULT_ROUTES):
    """Read a versioned table, refusing unknown fields and invalid paths."""
    with open(path, 'rb') as stream:
        return _validate_table(tomllib.load(stream))


class _Ids(HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.ids = set()

    def handle_starttag(self, tag, attrs):
        self.ids.update(value for key, value in attrs if key == 'id' and value)


def _root(outdir):
    root = Path(outdir).resolve(strict=True)
    if not root.is_dir():
        raise ValueError(f'HTML output is not a directory: {outdir}')
    return root


def _output_path(root, source):
    """Refuse symbolic links and non-directory parents before any write."""
    path = root
    parts = PurePosixPath(source).parts
    for index, part in enumerate(parts):
        path /= part
        if path.is_symlink():
            raise ValueError(f'{source}: redirect output has a symbolic link: '
                             f'{path}')
        if path.exists():
            valid = path.is_file() if index == len(parts) - 1 else path.is_dir()
            if not valid:
                raise ValueError(f'{source}: invalid output path component {path}')
    return path


def validate_targets(outdir, table):
    """Return every missing or unsafe destination, including default fragments."""
    try:
        _validate_table(table)
        root = _root(outdir)
        outputs = {(root / name).resolve() for name in table['pages']}
    except (OSError, ValueError, RuntimeError) as error:
        return [str(error)]
    problems, cache = [], {}
    for source, entry in sorted(table['pages'].items()):
        targets = [('default', entry['default']), *sorted(entry['anchors'].items())]
        for anchor, target in targets:
            page, fragment = _destination(target)
            try:
                path = (root / page).resolve()
                if not path.is_relative_to(root):
                    raise ValueError(f'target {page} resolves outside the HTML tree')
                if path in outputs:
                    raise ValueError(f'target {page} resolves to a redirect output')
                if not path.is_file():
                    raise ValueError(f'target page {page} is missing or not a file')
                if path not in cache:
                    parser = _Ids()
                    parser.feed(path.read_text(encoding='utf-8'))
                    cache[path] = parser.ids
                if fragment and fragment not in cache[path]:
                    raise ValueError(f'target {page} has no id "{fragment}"')
            except (OSError, ValueError, RuntimeError) as error:
                problems.append(f'{source} ({anchor}): {error}')
    return problems


def _relative(source, target):
    page, fragment = _destination(target)
    result = posixpath.relpath(page, posixpath.dirname(source) or '.')
    return result + ('#' + fragment if fragment else '')


def render_page(source, entry):
    """Render one validated entry, with destinations relative to its own page."""
    _validate_table({'version': 1, 'pages': {source: entry}})
    default = _relative(source, entry['default'])
    data = {'default': default,
            'routes': {anchor: _relative(source, target)
                       for anchor, target in sorted(entry['anchors'].items())}}
    table = json.dumps(data, indent=2, sort_keys=True, ensure_ascii=True)
    # JSON is data, not script content, even if the accepted grammar expands.
    table = table.replace('&', '\\u0026').replace('<', '\\u003c')
    table = table.replace('>', '\\u003e')
    href = html.escape(default, quote=True)
    title = html.escape(entry['title'], quote=True)
    return f'''<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>{title} — Tadah! documentation</title>
<meta name="robots" content="noindex">
<link rel="canonical" href="{href}">
<script type="application/json" id="redirect-routes">
{table}
</script>
<script>
{_SCRIPT}
</script>
<noscript><meta http-equiv="refresh" content="0; url={href}"></noscript>
</head>
<body>
<h1>{title}</h1>
<p>The documentation is at <a href="{href}">{title}</a>.</p>
</body>
</html>
'''


def generate(outdir, routes_file=DEFAULT_ROUTES):
    """Validate all destinations, then atomically replace each generated page."""
    table = load_routes(routes_file)
    problems = validate_targets(outdir, table)
    if problems:
        raise ValueError('\n'.join(problems))
    root = _root(outdir)
    outputs = [(name, _output_path(root, name), render_page(name, entry))
               for name, entry in sorted(table['pages'].items())]
    staged = []
    try:
        for name, path, text in outputs:
            path.parent.mkdir(parents=True, exist_ok=True)
            with tempfile.NamedTemporaryFile(
                    mode='w', encoding='utf-8', newline='\n',
                    prefix='.redirect-', suffix='.html',
                    dir=path.parent, delete=False) as stream:
                staged.append((Path(stream.name), path))
                stream.write(text)
            # The published files are readable independently of the build user.
            Path(stream.name).chmod(0o644)
        for temporary, path in staged:
            temporary.replace(path)
    finally:
        for temporary, path in staged:
            temporary.unlink(missing_ok=True)
    return [name for name, path, text in outputs]


def check(outdir, routes_file=DEFAULT_ROUTES):
    """Check targets and exact generated output without changing any files."""
    try:
        table = load_routes(routes_file)
        root = _root(outdir)
        problems = validate_targets(root, table)
        for source, entry in sorted(table['pages'].items()):
            try:
                path = _output_path(root, source)
                if not path.is_file():
                    problems.append(f'{source}: generated page is missing')
                elif path.read_bytes() != render_page(source, entry).encode('utf-8'):
                    problems.append(f'{source}: page differs from generated output')
            except (OSError, ValueError, RuntimeError) as error:
                problems.append(str(error))
        return problems
    except (OSError, ValueError, RuntimeError) as error:
        return [str(error)]


def generate_build(app, exception):
    """Sphinx build-finished handler; a target error stops the build.

    Only the flat ``html`` builder writes the pages: the released site has
    its page paths, and the pages link to them. Another HTML builder
    (``dirhtml``, ``singlehtml``, ``epub``) writes none and logs that.
    """
    if exception is not None or app.builder.format != 'html':
        return
    from sphinx.errors import ExtensionError
    if app.builder.name != 'html':
        from sphinx.util import logging
        logging.getLogger(__name__).info(
            'legacy_routes: the html builder writes the compatibility pages; '
            f'{app.builder.name!r} writes none')
        return
    try:
        generate(app.outdir, app.config.legacy_routes_file)
    except (OSError, ValueError, RuntimeError) as error:
        raise ExtensionError(f'compatibility redirects: {error}') from error


def setup(app):
    """Sphinx extension entry point."""
    app.add_config_value('legacy_routes_file', str(DEFAULT_ROUTES), 'env')
    app.connect('build-finished', generate_build)
    return {'parallel_read_safe': True, 'parallel_write_safe': True}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    parser.add_argument('outdir', help='HTML output directory')
    parser.add_argument('--routes', default=DEFAULT_ROUTES, help='TOML routes table')
    parser.add_argument('--check', action='store_true', help='check without writing')
    args = parser.parse_args(argv)
    try:
        if args.check:
            problems = check(args.outdir, args.routes)
            if problems:
                raise ValueError('\n'.join(problems))
        else:
            generate(args.outdir, args.routes)
    except (OSError, ValueError, RuntimeError) as error:
        print(f'legacy_routes: {error}', file=sys.stderr)
        return 1
    print(f'legacy_routes: redirects OK in {args.outdir}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
