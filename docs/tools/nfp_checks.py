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
    (under ``nfp/reference/``, except its hub) have no length limit;
  * a page with three or more sections that does not start with the
    navigation block

        .. contents::
           :local:
           :depth: 1
           :backlinks: none

    before its first section (a block with other options is a warning
    too);
  * a ``literalinclude`` without ``:language:``, and a code block in a
    language other than bash, ini, none and text (``console``, ``Bash``, a
    literal block ``::``, which has none, a doctest block ``>>>``);
  * a bash block that the runs of the pages do not read: the runs
    (tests/nfp_docs/check_nfp_run.py, tests/doc_commands/doc_commands.py)
    read the lines ``.. code-block:: bash`` of the page itself, so a bash
    block written otherwise (``code``, ``sourcecode``, a ``literalinclude``,
    another spacing or letter case, a ``highlight`` that makes a block
    bash) or one from an included file shows commands that no test runs;
  * a ``.. code-block:: bash`` that the runs read and the page does not
    show (inside a comment): the tests would run a command no reader sees.

Quotes of program output, in every hand-written page of the chapter (not
the generated pages under ``nfp/reference/``) and in the section
"Training inside tadah hpo" of ``training.rst`` (label ``training_hpo``):

  * a text code block (``.. code-block:: text``, a ``literalinclude`` with
    ``:language: text``) is a quote that a test checks: a ``.. code-block::
    text`` of the page itself preceded by an output marker, the comment
    ``.. output: <ids> <source> [search=...] [command=...] [tol=...]``,
    which the run of each manifest line it names checks
    (tests/nfp_docs/check_nfp_run.py; the grammar and the match:
    tests/nfp_docs/nfp_files.py), or the refusal of a target-file line
    that ``.. hpo:refusal::`` renders from HPO's registry
    (hpotarget_domain.py). Any other text block is a warning;
  * a literal block of the section ``training_hpo`` follows the language
    rule of the chapter's pages (above), so that program output there is
    a text code block;
  * an output marker on any page of the chapter or of ``training.rst``
    that names an id, a source, a search, a command or a tolerance that
    no line of ``nfp/files/manifest.txt`` has, or that no text block
    follows, is a warning at the marker's line (the same check as
    check_nfp_run.coverage()); an output marker on any other page, whose
    quotes no run reads, is a warning at its line;
  * with ``nfp_message_comments_fail`` (conf.py; default
    MESSAGE_COMMENTS_FAIL), a ``.. message:`` comment, the placeholder of
    a quote still to be written, is a warning at its line.

Every file under ``nfp/files`` but ``manifest.txt`` (which the tests
read) is linked from a page: a ``:download:``, a ``literalinclude``, an
``include`` or the ``:file:`` of a ``csv-table`` names it. The check reads
what Sphinx itself records for these (the environment's ``dlfiles`` and
``dependencies``), so a path in a comment or a code block links nothing.

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
import importlib.util
import os
import re

from docutils import nodes
from docutils.parsers.rst.directives.body import ParsedLiteral
from docutils.parsers.rst.directives.parts import Contents
from sphinx import addnodes
from sphinx.directives.code import LiteralInclude
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
CONTENTS_FROM = 3       # sections from which a page needs the contents block
UNITS = CHAPTER + 'tutorial/'
FILES = CHAPTER + 'files'
NOT_OFFERED = ('manifest.txt',)   # files the tests read, which no page shows
GENERATED = CHAPTER + 'reference/'
MAX_SIDEBAR_LEVEL = 2
LANGUAGES = ('bash', 'ini', 'none', 'text')
#: The page and the section (its label) outside the chapter whose program
#: output is checked as the chapter's is.
QUOTE_DOC = 'training'
QUOTE_SECTION = 'training_hpo'
#: Whether a ".. message:" placeholder is a warning when conf.py does not
#: set nfp_message_comments_fail. Every placeholder of the chapter is
#: replaced by a tested quote, so a new one is a warning.
MESSAGE_COMMENTS_FAIL = True
MESSAGE_COMMENT = re.compile(r'^\s*\.\. message:')


def _load_doc_commands():
    """tests/doc_commands/doc_commands.py: what the runs of the pages read
    as commands."""
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..',
                        'tests', 'doc_commands', 'doc_commands.py')
    spec = importlib.util.spec_from_file_location('nfp_doc_commands', path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


doc_commands = _load_doc_commands()


def _load_check_nfp_run():
    """tests/nfp_docs/check_nfp_run.py: the static check of the output
    markers, and (its nfp_files) the scan of the quotes of a page."""
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..',
                        'tests', 'nfp_docs', 'check_nfp_run.py')
    spec = importlib.util.spec_from_file_location('nfp_check_nfp_run', path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


check_nfp_run = _load_check_nfp_run()


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


class CheckedContents(Contents):
    """docutils' contents, with its options kept on the block (the
    contents transform replaces the pending node that holds them)."""

    def run(self):
        result = super().run()
        for node in result:
            if isinstance(node, nodes.topic):
                node['nfp_contents'] = dict(self.options)
        return result


class CheckedLiteralInclude(LiteralInclude):
    """Sphinx' literalinclude, with its blocks marked when it sets no
    :language:."""

    def run(self):
        result = super().run()
        if 'language' not in self.options:
            for node in result:
                for block in node.findall(nodes.literal_block):
                    block['nfp_no_language'] = True
        return result


def check_code_lines(node):
    for number, line in enumerate(node.astext().splitlines(), 1):
        width = len(line.expandtabs(8))
        if width > MAX_CODE_LINE:
            _warn(f'line {number} of a code block is {width} characters long '
                  f'(at most {MAX_CODE_LINE}): {line.strip()[:50]}...',
                  node, 'code_line')


def check_language(node):
    """A literal block of a chapter page (or of the section QUOTE_SECTION)
    is a code block in one of LANGUAGES."""
    if node.get('nfp_parsed_literal'):
        _warn('parsed-literal block (use a code-block)', node,
              'parsed_literal')
    elif node.get('nfp_no_language'):
        _warn('literalinclude without :language: (bash, ini, none or '
              'text)', node, 'language')
    elif 'language' not in node:
        _warn('literal block "::" without a language (a chapter page '
              'uses a code-block in bash, ini, none or text)', node,
              'language')
    elif node['language'] not in LANGUAGES:
        _warn(f'code block in the language "{node["language"]}" (a '
              'chapter page uses bash, ini, none or text)', node,
              'language')


def check_doctree(app, doctree):
    docname = app.env.docname
    if docname == app.config.root_doc:
        check_root_toctree(doctree, docname)
    check_quotes(app, doctree, docname)
    if not docname.startswith(CHAPTER):
        return
    for node in doctree.findall(nodes.literal_block):
        check_code_lines(node)
        check_language(node)
    for node in doctree.findall(nodes.doctest_block):
        check_code_lines(node)
        _warn('doctest block (">>>"; a chapter page uses a code-block in '
              'bash, ini, none or text)', node, 'language')
    check_bash_blocks(app, doctree, docname)
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
    check_contents(doctree, docname)


def check_bash_blocks(app, doctree, docname):
    """Each bash block the page renders is a block the runs of the pages
    read (doc_commands.bash_blocks of the page's file, by the line of its
    directive), and each block they read is rendered."""
    path = os.path.normpath(str(app.env.doc2path(docname)))
    read = {b.line for b in doc_commands.bash_blocks(path)}
    shown = set()
    for node in doctree.findall(nodes.literal_block):
        if node.get('language') != 'bash':
            continue
        source = os.path.normpath(node.source) if node.source else None
        if source == path and node.line in read:
            shown.add(node.line)
            continue
        _warn('bash code block that the runs of the pages do not read: a '
              'command block is a ".. code-block:: bash" line of the page '
              'itself (tests/doc_commands/doc_commands.py), so no test runs '
              'these commands', node, 'bash_block')
    for line in sorted(read - shown):
        _warn('the runs of the pages read this ".. code-block:: bash", which '
              'the page does not show (inside a comment?): the tests would run '
              'commands no reader sees', (docname, line), 'bash_block')


def _in_section(node, name):
    """Whether *node* lies in a section whose names include *name*."""
    parent = node.parent
    while parent is not None:
        if isinstance(parent, nodes.section) and name in parent['names']:
            return True
        parent = parent.parent
    return False


def check_quotes(app, doctree, docname):
    """The quotes of program output of a hand-written page of the chapter
    and of the section QUOTE_SECTION of QUOTE_DOC, the output markers of
    the chapter and of QUOTE_DOC, and the ".. message:" placeholders."""
    chapter = docname.startswith(CHAPTER) and not (
        docname.startswith(GENERATED) and not is_hub(docname))
    path = os.path.normpath(str(app.env.doc2path(docname)))
    quotes = check_nfp_run.nfp_files.quote_blocks(path)
    if not docname.startswith(CHAPTER) and docname != QUOTE_DOC:
        # check_nfp_run.quote_pages(): no run reads the markers of a page
        # outside them.
        for q in quotes:
            _warn('an output marker on a page whose quotes no run checks '
                  '(the runs read the markers of the pages under nfp/ and of '
                  'training.rst)', (docname, q.marker_line), 'quote')
        return
    if not chapter and docname != QUOTE_DOC:
        return

    def in_scope(node):
        return chapter or _in_section(node, QUOTE_SECTION)

    if quotes:
        manifest = os.path.join(app.srcdir, FILES, 'manifest.txt')
        try:
            if not os.path.isfile(manifest):
                raise check_nfp_run.ManifestError(
                    f'{manifest} does not exist: no run checks the quote')
            problems = check_nfp_run.quote_problems(manifest, pages=[path])
        except check_nfp_run.ManifestError as err:
            problems = [(path, q.marker_line, str(err)) for q in quotes]
        for _, line, message in problems:
            _warn(message, (docname, line), 'quote')
    marked = {q.line for q in quotes if q.line is not None}
    for node in doctree.findall(nodes.literal_block):
        if not chapter and in_scope(node):
            # The section follows the language rule of the chapter's pages
            # (check_doctree), so that no output hides in another block.
            check_language(node)
        if node.get('language') != 'text' or node.get('nfp_refusal'):
            continue
        if not in_scope(node):
            continue
        source = os.path.normpath(node.source) if node.source else None
        if source == path and node.line in marked:
            continue
        _warn('program output (a text code block) that no test checks: '
              'precede a ".. code-block:: text" with an output marker '
              '(".. output: <id> <source>", which names the run that checks '
              'it), or quote a refusal of a target-file line with '
              '".. hpo:refusal::"', node, 'quote')
    if not app.config.nfp_message_comments_fail:
        return
    with open(path, encoding='utf-8') as fh:
        lines = [n for n, line in enumerate(fh.read().splitlines(), 1)
                 if MESSAGE_COMMENT.match(line)]
    comments = [n for n in doctree.findall(nodes.comment)
                if n.astext().startswith('message:')]
    for i, node in enumerate(comments):
        if not in_scope(node):
            continue
        # The comments of the page itself, in order; one of an included
        # file is reported at its node.
        where = ((docname, lines[i]) if len(lines) == len(comments)
                 else node)
        _warn('a ".. message:" placeholder: replace it with the quote of the '
              'program output it names (an output marker and its text code '
              'block, or ".. hpo:refusal::")', where, 'message')


def check_files_linked(app, env):
    """Every shipped file but NOT_OFFERED is a download or a dependency of
    a page."""
    root = os.path.join(app.srcdir, FILES)
    if not os.path.isdir(root):
        return
    srcdir = os.path.abspath(app.srcdir)

    def rel(p):
        p = os.fspath(p)
        return os.path.normpath(os.path.relpath(p, srcdir) if os.path.isabs(p)
                                else p)
    linked = {rel(p) for p in env.dlfiles}
    for deps in env.dependencies.values():
        linked.update(rel(p) for p in deps)
    for top, subdirs, names in os.walk(root):
        subdirs.sort()
        for name in sorted(names):
            path = os.path.relpath(os.path.join(top, name), srcdir)
            if os.path.relpath(path, FILES) in NOT_OFFERED:
                continue
            if os.path.normpath(path) not in linked:
                _warn(f'{path}: no page links it (:download:, literalinclude, '
                      'include or the :file: of a csv-table)', None, 'unlinked')


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


def check_contents(doctree, docname):
    for title in (n for n in doctree.children if isinstance(n, nodes.section)):
        children = list(title.children)
        firsts = [i for i, n in enumerate(children)
                  if isinstance(n, nodes.section)]
        if len(firsts) < CONTENTS_FROM:
            continue
        blocks = [(i, n) for i, n in enumerate(children[:firsts[0]])
                  if isinstance(n, nodes.topic) and 'nfp_contents' in n]
        if not blocks:
            _warn(f'a page with {len(firsts)} sections starts with '
                  '".. contents::" (:local:, :depth: 1, :backlinks: none) '
                  'before its first section', docname, 'contents')
            continue
        options = blocks[0][1]['nfp_contents']
        wrong = []
        if 'local' not in options:
            wrong.append('no :local:')
        if options.get('depth') != 1:
            wrong.append(f':depth: {options.get("depth", "(none)")}')
        # docutils reads ":backlinks: none" as None.
        if 'backlinks' not in options or options['backlinks'] is not None:
            wrong.append('no :backlinks: none')
        if wrong:
            _warn(f'the contents block has {", and ".join(wrong)} (it takes '
                  ':local:, :depth: 1, :backlinks: none)', blocks[0][1],
                  'contents')


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
    app.add_config_value('nfp_message_comments_fail', MESSAGE_COMMENTS_FAIL,
                         'env', [bool])
    app.add_directive('parsed-literal', CheckedParsedLiteral, override=True)
    app.add_directive('contents', CheckedContents, override=True)
    app.add_directive('literalinclude', CheckedLiteralInclude, override=True)
    app.connect('doctree-read', check_doctree)
    app.connect('env-check-consistency', check_files_linked)
    app.connect('build-finished', check_sidebars)
    return {'parallel_read_safe': True, 'parallel_write_safe': True}
