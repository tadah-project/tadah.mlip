"""Sphinx extension: the ``hpo`` domain of the target-file reference.

conf.py lists ``hpotarget_domain`` in ``extensions``. The reference pages
under ``nfp/reference/`` (written by ``config/read_hpo_registry.py`` from
HPO's registries) hold one entry per key, block, option, optimiser and
``PARAM`` key; the roles below cite them from any page.

Entries (written by the generator, not by hand)::

    .. hpo:key:: OPTIMIZER.MAXEVAL
       :names: MAXEVAL
       :text: MAXEVAL

       Body: paragraphs and a field list.

``hpo:key``, ``hpo:block``, ``hpo:opt``, ``hpo:param`` and ``hpo:topic``
declare an entry whose anchor is its id (the argument); a topic is an
entry headed by words, such as a rule, and its heading and the roles that
cite it show those words as text, not as code. ``:names:`` lists the bare
names an entry answers to (without it, only the id reaches the entry),
``:text:`` is the name shown in its heading and by a role that cites it
by id, ``:annotation:`` a few words after the name, ``:also:`` further
ids of the same entry. ``hpo:removed`` declares a name that a target file
no longer takes (``:names:``, ``:hint:``, ``:scope:`` ``key`` or
``param``): its entry is shown, and a role that names it is a warning
with the hint. ``hpo:target`` declares an id without an entry (a
section heading follows it). An id declared twice is a warning.

Roles::

    :hpo:key:`MAXEVAL`            a key, option, form, loss function,
    :hpo:key:`OPTIMIZER.MAXEVAL`  STRATEGY type, INIT method, environment
    :hpo:key:`PC_LAMMPS.--hotvar` variable, rule or section, by bare name
                                  or by id
    :hpo:block:`STRATEGY`         a block
    :hpo:opt:`NLOPT:LN_SBPLX`     an optimiser (LIB:ALGO, ALGO alone, or LIB)
    :hpo:param:`GRID_POINTS`      a PARAM key (PARAM.<optimiser>.<name>)
    :hpo:topic:`RULE.<name>`      a rule or another topic, by id
    :hpo:key:`!MAXEVAL`           the same, not linked, still checked
    :hpo:key:`text <MAXEVAL>`     another text

A role renders inline literal text (a topic: text): the name as written,
or the entry's ``:text:`` when it is cited by id, linked to the entry. An
id is looked up first, then a bare name; a name of one entry that is also
a removed name cites that entry. Each of these is a build warning at the
page and line of the role (so ``sphinx-build -W`` fails): a name no entry
of the role's kind answers to (naming the kind it is when another role
takes it); a name of several entries (naming their ids, each of which
resolves it); a removed name that no entry has (with its hint). The text
is then shown without a link.
"""
import difflib

from docutils import nodes
from docutils.parsers.rst import directives
from sphinx import addnodes
from sphinx.directives import ObjectDescription
from sphinx.domains import Domain, ObjType
from sphinx.roles import XRefRole
from sphinx.util import logging
from sphinx.util.docutils import SphinxDirective
from sphinx.util.nodes import make_refnode

logger = logging.getLogger(__name__)

#: Object types, and the role of each.
KINDS = ('key', 'block', 'opt', 'param', 'topic')
KIND_WORDS = {'key': 'key', 'block': 'block', 'opt': 'optimiser',
              'param': 'PARAM key', 'topic': 'topic'}
ARTICLE = {'key': 'a', 'block': 'a', 'opt': 'an', 'param': 'a',
           'topic': 'a'}
#: The removed names each role checks: the target-file keywords (key and
#: block) and the retired PARAM keys.
REMOVED_SCOPE = {'key': 'key', 'block': 'key', 'opt': None, 'param': 'param',
                 'topic': None}
#: The node of a role's text: a topic is words, the others are names.
TEXT_NODE = {k: nodes.inline if k == 'topic' else nodes.literal
             for k in KINDS}


def _names(text):
    return tuple(text.split()) if text else ()


def _warn(message, node):
    logger.warning(message, location=node, type='hpo', subtype='ref')


class HpoEntry(ObjectDescription):
    """One reference entry: a key, block, optimiser, PARAM key or topic."""

    has_content = True
    required_arguments = 1
    final_argument_whitespace = False
    option_spec = dict(ObjectDescription.option_spec,
                       names=directives.unchanged,
                       text=directives.unchanged,
                       annotation=directives.unchanged,
                       also=directives.unchanged)

    def get_signatures(self):
        return [self.arguments[0].strip()]

    def handle_signature(self, sig, signode):
        text = self.options.get('text') or sig
        signode += addnodes.desc_name(text, text)
        annotation = self.options.get('annotation')
        if annotation:
            signode += addnodes.desc_annotation(' ' + annotation,
                                                ' ' + annotation)
        return sig

    def add_target_and_index(self, ident, sig, signode):
        ids = (ident,) + _names(self.options.get('also'))
        domain = self.env.get_domain('hpo')
        for i, anchor in enumerate(ids):
            signode['ids'].append(anchor)
            self.register(domain, anchor, names=() if i else
                          _names(self.options.get('names')), node=signode)
        self.state.document.note_explicit_target(signode)

    def register(self, domain, ident, names, node):
        domain.note_object(ident, self.objtype, names,
                           self.options.get('text') or ident, node)


class HpoRemoved(HpoEntry):
    """A name the target file no longer takes, with the parser's hint."""

    option_spec = dict(HpoEntry.option_spec, hint=directives.unchanged,
                       scope=lambda v: directives.choice(v, ('key', 'param')))

    def register(self, domain, ident, names, node):
        domain.note_removed(ident, self.options.get('scope', 'key'), names,
                            self.options.get('hint', ''), node)


class HpoTarget(SphinxDirective):
    """An id without an entry of its own: ``.. hpo:target:: NLOPT`` with
    ``:kind:``, ``:names:`` and ``:text:``; the section that follows is
    what it links to."""

    required_arguments = 1
    option_spec = {'kind': lambda v: directives.choice(v, KINDS),
                   'names': directives.unchanged,
                   'text': directives.unchanged}

    def run(self):
        ident = self.arguments[0].strip()
        node = nodes.target('', '', ids=[ident])
        self.set_source_info(node)
        self.state.document.note_explicit_target(node)
        self.env.get_domain('hpo').note_object(
            ident, self.options.get('kind', 'key'),
            _names(self.options.get('names')),
            self.options.get('text') or ident, node)
        return [node]


class HpoRole(XRefRole):
    """A citation of an entry; ``!`` gives the text without a link, still
    checked."""

    innernodeclass = nodes.literal

    def process_link(self, env, refnode, has_explicit_title, title, target):
        # Until the entry is found, an id shows its last part (the name, as
        # in OPTIMIZER.MAXEVAL), so that the checks made while the page is
        # read see the length of the name; resolve_xref then shows the
        # entry's text, or what was written when no entry answers.
        refnode['hpo_written'] = title
        if not has_explicit_title and '.' in target:
            title = target.rsplit('.', 1)[1] or title
        return title, target

    def create_non_xref_node(self):
        if not self.has_explicit_title:
            self.title = self.title[1:]
        self.target = self.target[1:] if self.target.startswith('!') \
            else self.target
        if self.has_explicit_title and self.title.startswith('!'):
            self.title = self.title[1:]
        result, messages = self.create_xref_node()
        for node in result:
            if isinstance(node, addnodes.pending_xref):
                node['hpo_nolink'] = True
        return result, messages


class HpoDomain(Domain):
    """The entries of the target-file reference and the roles citing them."""

    name = 'hpo'
    label = 'tadah hpo target file'
    object_types = {k: ObjType(KIND_WORDS[k], k) for k in KINDS}
    directives = dict({k: HpoEntry for k in KINDS}, removed=HpoRemoved,
                      target=HpoTarget)
    roles = {k: HpoRole(innernodeclass=TEXT_NODE[k]) for k in KINDS}
    initial_data = {'objects': {}, 'removed': {}}
    data_version = 1

    # objects: id -> (docname, kind, names, text)
    # removed: id -> (docname, scope, names, hint, text)

    def note_object(self, ident, kind, names, text, node):
        self._check_unique(ident, node)
        self.data['objects'][ident] = (self.env.docname, kind, tuple(names),
                                       text)

    def note_removed(self, ident, scope, names, hint, node):
        self._check_unique(ident, node)
        self.data['removed'][ident] = (self.env.docname, scope, tuple(names),
                                       hint, ident)

    def _check_unique(self, ident, node):
        for table in ('objects', 'removed'):
            if ident in self.data[table]:
                other = self.data[table][ident][0]
                logger.warning(f'hpo: entry {ident} is declared twice (also '
                               f'in {other})', location=node, type='hpo',
                               subtype='duplicate')

    def clear_doc(self, docname):
        for table in ('objects', 'removed'):
            for ident, record in list(self.data[table].items()):
                if record[0] == docname:
                    del self.data[table][ident]

    def merge_domaindata(self, docnames, otherdata):
        for table in ('objects', 'removed'):
            for ident, record in otherdata[table].items():
                if record[0] in docnames:
                    self.data[table][ident] = record

    def get_objects(self):
        for ident, (docname, kind, _names_, text) in sorted(
                self.data['objects'].items()):
            yield ident, text, kind, docname, ident, 1

    # ---------------------------------------------------------- lookup ----
    def lookup(self, role, target):
        """(status, detail) for *target* cited by *role*: ('found', id),
        ('ambiguous', ids), ('removed', [(id, hint)]) or ('unknown',
        hint): an id of an entry of the role's kind, else a bare name."""
        objects, removed = self.data['objects'], self.data['removed']
        scope = REMOVED_SCOPE[role]
        if target in objects and objects[target][1] == role:
            return 'found', target
        if target in removed and removed[target][1] == scope:
            return 'removed', [(target, removed[target][3])]
        found = sorted(i for i, r in objects.items()
                       if r[1] == role and target in r[2])
        gone = sorted(i for i, r in removed.items()
                      if r[1] == scope and target in r[2])
        # An entry of the name wins over a removed name: the name is in the
        # target file's vocabulary.
        if len(found) == 1:
            return 'found', found[0]
        if len(found) > 1:
            return 'ambiguous', found
        if gone:
            return 'removed', [(i, removed[i][3]) for i in gone]
        others = sorted({r[1] for i, r in objects.items()
                         if r[1] != role and (target in r[2] or i == target)})
        known = {n for i, r in objects.items() if r[1] == role
                 for n in r[2] + (i,)}
        close = difflib.get_close_matches(target, sorted(known), n=3)
        hint = ''
        if others:
            hint += ' (it is ' + ' and '.join(
                f'{ARTICLE[k]} {KIND_WORDS[k]}: :hpo:{k}:' for k in others) + ')'
        if close:
            hint += ' (close: ' + ', '.join(close) + ')'
        return 'unknown', hint

    def resolve_xref(self, env, fromdocname, builder, typ, target, node,
                     contnode):
        role = f'hpo:{typ}:'
        status, detail = self.lookup(typ, target)
        if status == 'found':
            docname, _kind, _names_, text = self.data['objects'][detail]
            if not node.get('refexplicit') and target == detail:
                contnode = TEXT_NODE[typ](text, text,
                                          classes=contnode['classes'])
            if node.get('hpo_nolink'):
                contnode['classes'] = [c for c in contnode['classes']
                                       if c != 'xref']
                return contnode
            return make_refnode(builder, fromdocname, docname, detail,
                                contnode, detail)
        if status == 'ambiguous':
            _warn(f"{role} '{target}' names {len(detail)} entries: "
                  f"{', '.join(detail)}; write the id of one", node)
        elif status == 'removed':
            for ident, hint in detail:
                _warn(f"{role} '{target}' was removed from the target file "
                      f"({ident}): {hint}", node)
        else:
            _warn(f"{role} '{target}' is not {ARTICLE[typ]} "
                  f"{KIND_WORDS[typ]} of the target-file reference{detail}",
                  node)
        written = node.get('hpo_written', target)
        return TEXT_NODE[typ](written, written, classes=[
            c for c in contnode['classes'] if c != 'xref'])

    def resolve_any_xref(self, env, fromdocname, builder, target, node,
                         contnode):
        results = []
        for kind in KINDS:
            status, ident = self.lookup(kind, target)
            if status == 'found':
                docname = self.data['objects'][ident][0]
                results.append((f'hpo:{kind}', make_refnode(
                    builder, fromdocname, docname, ident, contnode, ident)))
        return results


def setup(app):
    app.add_domain(HpoDomain)
    return {'version': '1', 'env_version': 1, 'parallel_read_safe': True,
            'parallel_write_safe': True}
