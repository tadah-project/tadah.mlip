"""Sphinx extension: layout and structure rules of the documentation.

conf.py lists ``nfp_checks`` in ``extensions``. Every rule is a build
warning, so ``sphinx-build -W`` fails on a page that breaks one.

For every document of the Nested Fitting chapter (under ``nfp/``):

  * a line of a code block (``code-block``, ``literalinclude``, a literal
    block, a doctest block) longer than 80 characters, tabs expanded to
    every 8th column;
  * an inline literal with a run of more than 40 characters without a
    space (it cannot break between words);
  * a table with more than two columns;
  * a ``parsed-literal`` block or raw content (the ``raw`` directive, a
    raw role or a substitution defined with ``raw``);
  * a toctree in a page that is not a hub (a hub is a file named
    ``index.rst``), and in a hub more than one toctree or a toctree whose
    ``:maxdepth:`` is not 1;
  * a page longer than its limit: 250 source lines and seven sections (the
    sections directly under the page title), 200 source lines for a
    tutorial unit (under ``nfp/tutorial/``). The generated reference pages
    (under ``nfp/reference/``, except its hub) have no length limit.

In the root document, the toctree captioned "Nested Fitting" must have
``:titlesonly:``, so that the sidebar shows the chapter's pages and not
their sections; a root document without that toctree is a warning too.

After a build with the ``html`` or ``dirhtml`` builder, for every page of
the site: an entry of the navigation sidebar deeper than two levels under
its caption (an ``li`` of class ``toctree-l3`` or deeper), with the
caption and the entry it is under. A build in which no page has a sidebar
is a warning as well: the check would pass without having run.
"""
import html.parser
import os
import re

from docutils import nodes
from docutils.parsers.rst.directives.body import ParsedLiteral
from sphinx import addnodes
from sphinx.util import logging

logger = logging.getLogger(__name__)

CHAPTER = 'nfp/'
CAPTION = 'Nested Fitting'
MAX_CODE_LINE = 80
MAX_INLINE_RUN = 40
MAX_TABLE_COLUMNS = 2
MAX_PAGE_LINES = 250
MAX_UNIT_LINES = 200
MAX_SECTIONS = 7
UNITS = CHAPTER + 'tutorial/'
GENERATED = CHAPTER + 'reference/'
MAX_SIDEBAR_LEVEL = 2


def _warn(message, location, subtype):
    logger.warning(message, location=location, type='nfp_checks',
                   subtype=subtype)


def is_hub(docname):
    return docname.rsplit('/', 1)[-1] == 'index'


class CheckedParsedLiteral(ParsedLiteral):
    """docutils' parsed-literal, with its block marked so that the checks
    can tell it from a literal block."""

    def run(self):
        result = super().run()
        for node in result:
            if isinstance(node, nodes.literal_block):
                node['nfp_parsed_literal'] = True
        return result


def check_code_lines(node):
    for number, line in enumerate(node.astext().splitlines(), 1):
        width = len(line.expandtabs(8))
        if width > MAX_CODE_LINE:
            _warn(f'line {number} of a code block is {width} characters long '
                  f'(at most {MAX_CODE_LINE}): {line.strip()[:50]}...',
                  node, 'code_line')


def check_doctree(app, doctree):
    docname = app.env.docname
    if docname == app.config.root_doc:
        check_root_toctree(doctree, docname)
    if not docname.startswith(CHAPTER):
        return
    for node in doctree.findall(nodes.literal_block):
        check_code_lines(node)
        if node.get('nfp_parsed_literal'):
            _warn('parsed-literal block (use a code-block)', node,
                  'parsed_literal')
    for node in doctree.findall(nodes.doctest_block):
        check_code_lines(node)
    for node in doctree.findall(nodes.literal):
        if node.parent is not None and isinstance(node.parent,
                                                  nodes.literal_block):
            continue
        for run in re.findall(r'\S{%d,}' % (MAX_INLINE_RUN + 1), node.astext()):
            _warn(f'inline literal with {len(run)} characters without a '
                  f'space (at most {MAX_INLINE_RUN}; put it in a code '
                  f'block): {run[:50]}', node, 'inline_literal')
    for node in doctree.findall(nodes.tgroup):
        cols = int(node.get('cols', 0))
        if cols > MAX_TABLE_COLUMNS:
            _warn(f'table with {cols} columns (at most {MAX_TABLE_COLUMNS}; '
                  'use a definition list or a field list)',
                  node.parent, 'table')
    for node in doctree.findall(nodes.raw):
        if isinstance(node.parent, nodes.substitution_definition):
            continue   # reported where the substitution is used
        _warn('raw content (raw directive, role or substitution)', node, 'raw')
    check_toctrees(doctree, docname)
    check_length(app, doctree, docname)


def check_root_toctree(doctree, docname):
    captioned = [t for t in doctree.findall(addnodes.toctree)
                 if t.get('caption') == CAPTION]
    if not captioned:
        _warn(f'the root document has no toctree captioned "{CAPTION}"',
              docname, 'titlesonly')
    for toctree in captioned:
        if not toctree.get('titlesonly'):
            _warn(f'the "{CAPTION}" toctree has no :titlesonly: (the sidebar '
                  'would list the sections of its pages)', toctree,
                  'titlesonly')


def check_toctrees(doctree, docname):
    toctrees = list(doctree.findall(addnodes.toctree))
    if not toctrees:
        return
    if not is_hub(docname):
        for toctree in toctrees:
            _warn('toctree in a page that is not a hub (only an index.rst '
                  'holds a toctree)', toctree, 'toctree')
        return
    if len(toctrees) > 1:
        _warn(f'a hub holds one toctree; this one holds {len(toctrees)}',
              toctrees[1], 'toctree')
    for toctree in toctrees:
        if toctree.get('maxdepth') != 1:
            _warn('the toctree of a hub has :maxdepth: 1 (it has '
                  f'{toctree.get("maxdepth")})', toctree, 'toctree')


def check_length(app, doctree, docname):
    if docname.startswith(GENERATED) and not is_hub(docname):
        return
    path = app.env.doc2path(docname)
    with open(path, encoding='utf-8') as fh:
        lines = len(fh.read().splitlines())
    limit = (MAX_UNIT_LINES if docname.startswith(UNITS) and not is_hub(docname)
             else MAX_PAGE_LINES)
    if lines > limit:
        _warn(f'the page has {lines} source lines (at most {limit}; split '
              'it by topic)', docname, 'length')
    titles = [n for n in doctree.children if isinstance(n, nodes.section)]
    sections = sum(1 for title in titles for n in title.children
                   if isinstance(n, nodes.section))
    if sections > MAX_SECTIONS:
        _warn(f'the page has {sections} sections (at most {MAX_SECTIONS}; '
              'split it by topic)', docname, 'length')


class _Sidebar(html.parser.HTMLParser):
    """Entries of the navigation sidebar deeper than MAX_SIDEBAR_LEVEL."""

    CONTAINERS = ('wy-menu-vertical', 'sphinxsidebarwrapper')

    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.found = False
        self.depth = 0          # open <div>s inside the sidebar container
        self.caption = None
        self.in_caption = False
        self.deep = []          # (caption, level, entry text)
        self.entry = None       # level of the li whose link text is read
        self.text = ''

    def handle_starttag(self, tag, attrs):
        classes = (dict(attrs).get('class') or '').split()
        if tag == 'div':
            if self.depth:
                self.depth += 1
            elif any(c in classes for c in self.CONTAINERS):
                self.found = True
                self.depth = 1
            return
        if not self.depth:
            return
        if tag == 'p' and 'caption' in classes:
            self.in_caption = True
            self.caption = ''
        elif tag == 'li':
            for c in classes:
                m = re.fullmatch(r'toctree-l(\d+)', c)
                if m and int(m.group(1)) > MAX_SIDEBAR_LEVEL:
                    self.entry = int(m.group(1))
                    self.text = ''

    def handle_endtag(self, tag):
        if not self.depth:
            return
        if tag == 'div':
            self.depth -= 1
        elif tag == 'p' and self.in_caption:
            self.in_caption = False
        elif tag == 'a' and self.entry is not None:
            self.deep.append((self.caption, self.entry, self.text.strip()))
            self.entry = None

    def handle_data(self, data):
        if self.in_caption:
            self.caption += data
        elif self.entry is not None:
            self.text += data


def sidebar_problems(path):
    """(found, problems): whether the page at *path* has a sidebar, and a
    description of each of its entries deeper than two levels."""
    parser = _Sidebar()
    with open(path, encoding='utf-8') as fh:
        parser.feed(fh.read())
    return parser.found, [
        f'level {level} under "{(caption or "(no caption)").strip()}": '
        f'"{text}"' for caption, level, text in parser.deep]


def check_sidebars(app, exception):
    if exception is not None or app.builder.name not in ('html', 'dirhtml'):
        return
    with_sidebar = 0
    for docname in sorted(app.env.found_docs):
        path = os.path.join(app.outdir, app.builder.get_outfilename(docname))
        if not os.path.isfile(path):
            continue
        found, problems = sidebar_problems(path)
        with_sidebar += found
        if problems:
            shown = '; '.join(problems[:3])
            more = f' (and {len(problems) - 3} more)' if len(problems) > 3 else ''
            _warn(f'the sidebar shows {len(problems)} entries deeper than '
                  f'{MAX_SIDEBAR_LEVEL} levels: {shown}{more}', docname,
                  'sidebar')
    if app.env.found_docs and not with_sidebar:
        _warn('no page has a navigation sidebar: the sidebar depth was not '
              'checked', None, 'sidebar')


def setup(app):
    app.add_directive('parsed-literal', CheckedParsedLiteral, override=True)
    app.connect('doctree-read', check_doctree)
    app.connect('build-finished', check_sidebars)
    return {'parallel_read_safe': True, 'parallel_write_safe': True}
