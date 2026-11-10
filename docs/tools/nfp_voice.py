"""Sphinx extension: a lexical voice check of Nested Fitting prose.

Adding ``nfp_voice`` to conf.py's ``extensions`` checks every page under
``nfp/``, including generated reference pages. Warnings name the source
file and the containing node's line; ``sphinx-build -W`` fails on them.

The check reads resolved docutils trees, not rst source: headings, captions,
tables, lists, quotations, link labels and substituted or included prose
are checked too. Code, program output (literal/doctest blocks), inline
literals, mathematical notation, object names, comments and citation
markers are not prose. Citation bodies are checked, as are reference
topics (their names are headings in prose) and signature annotations.
Only a URL itself is excluded, never an arbitrary label linking to it.

This checks whole words, case-insensitively: you, your, yours, yourself,
we, our, us and let's (with a straight or typographical apostrophe). It
does not recognise imperative sentences or enforce impersonal grammar;
those require review. Source locations identify nodes, not word columns.
"""
import re

from docutils import nodes
from sphinx import addnodes
from sphinx.util import logging

logger = logging.getLogger(__name__)

CHAPTER = 'nfp/'
WORDS = re.compile(r"(?<!\w)(?:you|your|yours|yourself|we|our|us|"
                   r"let['\N{RIGHT SINGLE QUOTATION MARK}]s)(?!\w)",
                   re.IGNORECASE)
URL = re.compile(r'(?:[a-z][a-z0-9+.-]*://|mailto:|www\.)[^\s<>]+',
                 re.IGNORECASE)
NON_PROSE = (nodes.literal, nodes.literal_block, nodes.doctest_block,
             nodes.math, nodes.math_block, nodes.comment,
             nodes.substitution_definition, nodes.system_message,
             nodes.citation_reference, nodes.footnote_reference)
NAMES = (addnodes.desc_name, addnodes.desc_addname)


def _excluded(node):
    if isinstance(node, nodes.Element) and node.get('nfp_voice_citation'):
        return True
    if isinstance(node, nodes.literal) and isinstance(
            node.parent, addnodes.download_reference):
        # Sphinx wraps even an explicit prose label in a literal node.
        # A download's implicit filename remains a name, not prose.
        return not node.parent.get('refexplicit')
    if isinstance(node, NON_PROSE):
        return True
    if isinstance(node, nodes.label) and isinstance(
            node.parent, (nodes.citation, nodes.footnote)):
        return True
    if isinstance(node, NAMES):
        parent = node.parent
        while parent is not None and not isinstance(parent, addnodes.desc):
            parent = parent.parent
        # A topic's signature names a rule in prose, not a key or symbol.
        return not (parent is not None and parent.get('domain') == 'hpo'
                    and parent.get('objtype') == 'topic')
    if isinstance(node, nodes.reference):
        uri = node.get('refuri', '')
        text = node.astext()
        if uri and text in (uri, uri.removeprefix('mailto:')):
            return True
    return False


def mark_citations(app, doctree):
    """Keep the identity of citation markers through reference resolution.

    Sphinx replaces citation references with ordinary reference nodes,
    without a citation class. Only their inline content survives; a link
    that merely resembles a citation must not acquire the exclusion.
    """
    for node in doctree.findall(addnodes.pending_xref):
        if node.get('refdomain') == 'citation':
            for child in node.children:
                if isinstance(child, nodes.Element):
                    child['nfp_voice_citation'] = True


def _location(node, fallback):
    """The closest source and line, including an include's own source."""
    source = line = None
    while node is not None:
        if source is None:
            source = node.source
        if line is None:
            line = node.line
        if source is not None and line is not None:
            break
        node = node.parent
    # A copied reference title can lack source metadata on every inline
    # node. Its enclosing paragraph still has it; synthetic nodes may not.
    return f'{source or fallback}:{line or 1}'


def prose_blocks(doctree):
    """Yield (text, spans) across inline markup, never across block edges.

    Each span is (start, end, node), for locating a match in the original
    tree. Excluded children separate words without hiding adjacent prose.
    """
    parts, spans = [], []
    length = 0

    def flush():
        nonlocal length
        if parts:
            yield ''.join(parts), list(spans)
            parts.clear()
            spans.clear()
            length = 0

    def visit(node):
        nonlocal length
        if _excluded(node):
            parts.append('\0')
            length += 1
            return
        if isinstance(node, nodes.Text):
            # docutils keeps escape markers internally; astext() removes
            # them, as the writer does (e.g. emphasised parts of one word).
            text = node.astext()
            parts.append(text)
            spans.append((length, length + len(text), node))
            length += len(text)
            return
        if isinstance(node, nodes.image):
            # Alternative text is rendered when an image is unavailable,
            # and read by assistive technology. Respect excluded ancestors
            # here too: an unused image substitution is not rendered.
            yield from flush()
            text = node.get('alt', '')
            if text:
                yield text, [(0, len(text), node)]
            return
        block = not isinstance(node, nodes.Inline)
        if block:
            yield from flush()
        for child in node.children:
            yield from visit(child)
        if block:
            yield from flush()

    yield from visit(doctree)
    yield from flush()


def check_doctree(doctree, source):
    """Warn for each prohibited word in a resolved chapter document."""
    for text, spans in prose_blocks(doctree):
        # Keep offsets stable while excluding URL characters only.
        text = URL.sub(lambda m: '\0' * len(m.group()), text)
        for match in WORDS.finditer(text):
            node = next(n for start, end, n in spans
                        if start <= match.start() < end)
            logger.warning('impersonal voice: "%s" in rendered prose',
                           match.group(), location=_location(node, source),
                           type='nfp_voice', subtype='prose')


def check_pages(app, exception):
    """Check all chapter trees after reading and reference resolution.

    A sweep at build-finished also checks unchanged cached pages: Sphinx
    does not emit doctree-resolved for them in an incremental build. No
    findings or resolved trees are stored in the environment or sources.
    """
    if exception is not None:
        return
    for docname in sorted(app.env.found_docs):
        if docname.startswith(CHAPTER):
            tree = app.env.get_and_resolve_doctree(docname, app.builder)
            check_doctree(tree, app.env.doc2path(docname))


def setup(app):
    app.connect('doctree-read', mark_citations)
    app.connect('build-finished', check_pages)
    return {'version': '1', 'env_version': 1,
            'parallel_read_safe': True, 'parallel_write_safe': True}
