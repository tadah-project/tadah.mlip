#!/usr/bin/env python3
"""Generate the Nested Fitting reference pages from HPO's registries.

    read_hpo_registry.py [--out DIR]

Sources, in HPO's blueprint/ directory:

    hpotarget.toml and its part files   the target-file registry, loaded
                                        and checked by HPO's own loader,
                                        hpotarget_registry.py
    optimizers_gen.toml                 the optimiser list
    anneal_keys.toml                    the PARAM keys of the annealers

Pages, written into docs/source/nfp/reference/ (or DIR) and ignored by git;
the hub, nfp/reference/index.rst, is committed:

    target_keys.rst      how a line is read, the blocks, the top-level keys
                         (but those of pc_options.rst), the environment
                         variables, the removed keys
    optimizer_keys.rst   the keys of an optimiser, of the STRATEGY, INIT,
                         NOISE and RESTART blocks, the STRATEGY types and
                         INIT methods
    pc_options.rst       the validation metrics, PC_LAMMPS, PC_FAILPROBE and
                         the settings of their scripts, PC_OBSERVABLE, LOSS
                         and the loss functions, OPTIM
    optimisers.rst       every LIB:ALGO of the optimiser list, and the PARAM
                         keys but those of the annealers and of ASA
    anneal_params.rst    the PARAM keys of the annealers
    asa_params.rst       the PARAM keys of ASA:ASA

Every entry of the target-file registry has one entry on these pages, and
a rule each refers to has a place in its page's Rules section; an entry
is a directive of the hpo domain (docs/tools/hpotarget_domain.py) whose
Sphinx target is the registry id, so that the :hpo: roles find it. Its
body is the registry's text and a field list (Syntax, Values, Default,
Where, Repeat, Rules, Override, Example and the like; an empty field is
left out). Code lines are at most CODE_WIDTH characters: a longer
target-file line is continued with a backslash, as the target file
allows. The output depends only on the registries (no date, no path), so
two runs write the same files.

The HPO checkout is found as read_anneal_keys.py finds it
(source_locator.py); a checkout without the registry, a registry the
loader refuses, or a loader of another API stops the build with the
reason, and so does a Python without tomllib (3.10 and older).
"""

from __future__ import annotations

import argparse
import importlib.util
import os
import re
import sys
import textwrap

try:
    import tomllib
except ImportError:
    sys.exit(
        "read_hpo_registry.py needs Python 3.11 or later: it reads HPO's "
        "blueprint/hpotarget.toml with tomllib, which Python "
        f"{sys.version_info.major}.{sys.version_info.minor} does not have "
        f"({sys.executable}); build the documentation with Python 3.11 or "
        "later")

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import source_locator  # noqa: E402
import read_anneal_keys as rak  # noqa: E402

OUT_DIR = os.path.normpath(os.path.join(HERE, '..', 'nfp', 'reference'))
BLUEPRINT = 'blueprint'
LOADER = 'hpotarget_registry.py'
#: The loader API this generator reads (hpotarget_registry.API).
LOADER_API = 1
#: The longest code line the pages hold. Every code block of these pages
#: is in the body of a field list, which at 1024 px (the narrowest window
#: that does not wrap code) holds about 70 characters of code.
CODE_WIDTH = 64
#: The indent of a continued target-file line.
CONTINUATION_INDENT = '    '
PAGES = ('target_keys', 'optimizer_keys', 'pc_options', 'optimisers',
         'anneal_params', 'asa_params')
ANNEALERS = {kind: label for kind, (_f, label) in rak.KINDS.items()}
INT_LIMITS = {'INT_MAX': 2**31 - 1, 'UINT_MAX': 2**32 - 1,
              'UINT64_MAX': 2**64 - 1}
#: The argument kinds whose registry text, when there is one, says what
#: the value is (blueprint/README.md: any word, the rest of the line, a
#: LAMMPS time spec, a loss function).
TEXT_DESCRIBES = ('name', 'tokens', 'timespec', 'loss')


class GenerationError(Exception):
    """The registries cannot be written as reference pages; says why."""


# ------------------------------------------------------------- text ----

# Characters that may stand before and after inline markup in reStructuredText.
_BEFORE = set(' \t-:/\'"<([{')
_AFTER = set(' \t-.,:;!?\\/\'")]}>')


def literal(text):
    """*text* as an inline literal."""
    text = ' '.join(str(text).split())
    if not text or '``' in text or text.endswith('`'):
        raise GenerationError(f'cannot write {text!r} as an inline literal')
    return f'``{text}``'


def _escape(text):
    text = text.replace('\\', '\\\\')
    text = re.sub(r'([*`|])', r'\\\1', text)
    return re.sub(r'_(?=\W|$)', r'\\_', text)


# A `code` span, and an option or a bare -- (which the HTML builder would
# otherwise print as a dash).
_CODE = re.compile(r'`([^`]+)`|(?<![\w-])(--(?:[A-Za-z][\w-]*)?)(?![\w-])')
_LEADING = re.compile(r'([-+*:]|\d+[.)]|[A-Za-z#][.)]|\(\w+\)|\.\.)(\s|$)')


def prose(text):
    """Registry text as reStructuredText: `code` and --options as inline
    literals, every other markup character escaped."""
    text = ' '.join(text.split())
    out, pos = [], 0
    for m in _CODE.finditer(text):
        out.append(_escape(text[pos:m.start()]))
        before = text[m.start() - 1] if m.start() else ' '
        after = text[m.end()] if m.end() < len(text) else ' '
        out.append(('' if before in _BEFORE else '\\ ')
                   + literal(m.group(1) or m.group(2))
                   + ('' if after in _AFTER else '\\ '))
        pos = m.end()
    out.append(_escape(text[pos:]))
    result = ''.join(out)
    return '\\' + result if _LEADING.match(result) else result


def sentence(text):
    """*text* with a full stop at its end."""
    text = text.rstrip()
    return text if text.endswith(('.', '?', '!')) else text + '.'


def capital(text):
    """*text* with a capital first letter, to start a sentence."""
    return text[:1].upper() + text[1:]


def role(kind, ident):
    return f':hpo:{kind}:`{ident}`'


def number(value):
    """A registry number or limit as the parser's messages write it."""
    if isinstance(value, str):
        return str(INT_LIMITS[value])
    if isinstance(value, float) and value.is_integer():
        return str(int(value))
    return str(value)


def wrap_line(line, width=CODE_WIDTH):
    """A target-file line as physical lines of at most *width* characters,
    each continued with a backslash as the target file allows; the tokens
    of the joined line are those of *line*."""
    if len(line) <= width:
        return [line]
    if '#' in line:
        raise GenerationError(f'a line with a comment is longer than {width} '
                              f'characters: {line!r}')
    indent = line[:len(line) - len(line.lstrip())]
    tokens = line.split()
    out, current = [], indent + tokens[0]
    for token in tokens[1:]:
        if len(current) + 1 + len(token) + 2 <= width:
            current += ' ' + token
        else:
            out.append(current + ' \\')
            current = indent + CONTINUATION_INDENT + token
            if len(current) + 2 > width:
                raise GenerationError(f'a token is too long for a code line '
                                      f'of {width} characters: {token!r}')
    out.append(current)
    return out


def code(text, width=CODE_WIDTH):
    """A target-file fragment as a code block."""
    lines = []
    for line in text.strip('\n').split('\n'):
        lines += wrap_line(line.rstrip(), width)
    return ['.. code-block:: none', ''] + ['   ' + l if l else '' for l in lines]


def annealer_message(kind, text):
    """A message of an annealer's PARAM keys as the annealer prints it,
    after its name (anneal_keys.toml: "prefixed with TADAH:<ALGO>: ")."""
    return f'{ANNEALERS[kind]}: {text}'


def message(text, width=CODE_WIDTH):
    """A message of tadah hpo as a block of program output, its lines
    broken at blanks to at most *width* characters where a word allows."""
    lines = []
    for line in text.rstrip('\n').split('\n'):
        lines += textwrap.wrap(line, width, break_long_words=False,
                               break_on_hyphens=False) or ['']
    return ['.. code-block:: text', ''] + ['   ' + l if l else '' for l in lines]


# ------------------------------------------------------------ entries ----

class Entry:
    """One directive of the hpo domain: an id, its heading, its text and
    its field list."""

    def __init__(self, kind, ident, text, names=(), annotation='', also=(),
                 directive=None, options=()):
        self.kind, self.ident, self.text = kind, ident, text
        self.names, self.annotation, self.also = list(names), annotation, also
        self.directive = directive or kind
        self.options = list(options)
        self.paragraphs, self.fields = [], []

    def para(self, text, raw=False):
        if text and text.strip():
            self.paragraphs.append(text if raw else prose(sentence(text)))
        return self

    def field(self, name, *blocks):
        """A field whose body is *blocks*, each a list of lines; empty
        blocks and fields are left out."""
        blocks = [b for b in blocks if b]
        if blocks:
            self.fields.append((name, blocks))
        return self

    def line(self, name, text):
        return self.field(name, [text] if text else [])

    def items(self, name, items):
        """A field of one item, or a bullet list of several."""
        items = [i for i in items if i]
        if len(items) == 1:
            return self.line(name, items[0])
        return self.field(name, ['- ' + i for i in items])

    def examples(self, cases, width=CODE_WIDTH):
        blocks, seen = [], set()
        for case in cases:
            if case.text in seen:
                continue
            seen.add(case.text)
            if case.doc:
                blocks.append([prose(sentence(case.doc))])
            blocks.append(code(case.text, width))
        return self.field('Example', *blocks)

    def render(self):
        out = [f'.. hpo:{self.directive}:: {self.ident}']
        if self.names:
            out.append(f'   :names: {" ".join(self.names)}')
        out.append(f'   :text: {self.text}')
        if self.annotation:
            out.append(f'   :annotation: {self.annotation}')
        if self.also:
            out.append(f'   :also: {" ".join(self.also)}')
        out += [f'   :{k}: {v}' for k, v in self.options]
        out.append('')
        for p in self.paragraphs:
            out += ['   ' + p, '']
        for name, blocks in self.fields:
            out.append(f'   :{name}:')
            for i, block in enumerate(blocks):
                if i:
                    out.append('')
                out += ['      ' + l if l else '' for l in block]
            out.append('')
        return out


class Page:
    def __init__(self, name, label, title, intro):
        self.name, self.label, self.title = name, label, title
        self.intro = intro
        self.sections = []      # (title, intro, [Entry or raw lines])

    def section(self, title, items, intro='', before=()):
        """A section of *items* (entries, or lists of lines); *before* are
        lines put before its title, such as a target the section answers
        to."""
        if items:
            self.sections.append((title, intro, items, list(before)))

    def entries(self):
        return [e for _t, _i, items, _b in self.sections for e in items
                if isinstance(e, Entry)]

    def render(self):
        out = ['.. Generated by config/read_hpo_registry.py from the '
               "registries in HPO's", '   blueprint directory: do not edit '
               'by hand.', '', f'.. _{self.label}:', '', self.title,
               '=' * len(self.title), '', self.intro, '']
        if len(self.sections) >= 3:
            out += ['.. contents::', '   :local:', '   :depth: 1',
                    '   :backlinks: none', '']
        for title, intro, items, before in self.sections:
            out += before + [title, '-' * len(title), '']
            if intro:
                out += [intro, '']
            for item in items:
                out += item.render() if isinstance(item, Entry) else item
        while out and out[-1] == '':
            out.pop()
        return '\n'.join(out) + '\n'


# -------------------------------------------------------------- texts ----

KEY_REPEAT = {
    'last_wins': 'A later line replaces the value.',
    'warn_last_wins': 'A later line replaces the value, with a warning.',
    'error': 'Once: a second line is refused.',
    'add': 'Each line adds one more.',
}
OPTION_REPEAT = {
    'last_wins': 'A later one on the line replaces the value.',
    'warn_last_wins': 'A later one on the line replaces the value, with a '
                      'warning.',
    'error': 'Once on a line: a second one is refused.',
    'add': 'Each one adds one more.',
}
BLOCK_REPEAT = {
    'error': 'Once: a second block is refused.',
    'error_per_parent': 'Once in each enclosing block: a second one is '
                        'refused.',
    'merge': 'A second block continues the first.',
}
CHECKED = {'parse': 'when the file is read',
           'startup': 'when tadah hpo starts'}
RANDOM = {'never': 'never',
          'between_sides': 'only to choose between sides of equal length',
          'trial_points': 'for its trial points'}


class Writer:
    """Turns the loaded registries into the six pages."""

    def __init__(self, reg, optimizers, anneal):
        self.reg = reg
        self.by_id = reg.by_id()
        self.optimizers = optimizers
        self.anneal = anneal
        self.placed = {}            # registry id -> page name
        self.rules_of = {}          # entry id -> [rule ids]
        for rule in reg.rules:
            for ident in rule.keys:
                self.rules_of.setdefault(ident, []).append(rule.id)

    # ---- references -----------------------------------------------------
    def cite(self, ident):
        """A role citing the registry entry *ident*."""
        kind = 'block' if ident.startswith('BLOCK.') else (
            'param' if ident.startswith('PARAM.') else 'key')
        return role(kind, ident)

    def blocks(self, names):
        return ', '.join('top level' if n == 'ROOT' else
                         role('block', 'BLOCK.' + n) for n in names)

    def opt(self, name):
        return role('opt', name)

    # ---- values ---------------------------------------------------------
    def arg_text(self, arg):
        kind = arg.kind
        lo = number(arg.min) if arg.min is not None else None
        hi = number(arg.max) if arg.max is not None else None
        if kind in ('integer', 'number'):
            noun = 'an integer' if kind == 'integer' else 'a number'
            if lo is not None and hi is not None:
                text = f'{noun} from {lo} to {hi}'
            elif lo is not None:
                text = f'{noun} >= {lo}'
            elif hi is not None:
                text = f'{noun} <= {hi}'
            else:
                text = noun
        elif kind == 'positive':
            text = f'a positive {arg.noun or "number"}'
        elif kind == 'word':
            text = 'one of ' + ', '.join(literal(w) for w in arg.words)
            if arg.refused_words:
                text += ' (' + ', '.join(
                    literal(w) for w in arg.refused_words) + ' is refused)'
        elif kind == 'bool':
            lx = self.reg.lexing.fields
            text = ('a boolean: ' + ', '.join(literal(w) for w in
                                              lx['bool_true'])
                    + ' or ' + ', '.join(literal(w) for w in
                                         lx['bool_false']))
        elif kind == 'timespec':
            words = self.reg.lexing.fields['timespec_words']
            text = (', '.join(literal(w) for w in words)
                    + ' or a time ' + literal('[[HH:]MM:]SS'))
        elif kind == 'indices':
            text = 'an index list in parentheses, such as ' + literal('(1,3-5)')
        elif kind == 'loss':
            text = 'a loss function, with its parameter for ' + ' and '.join(
                literal(f.name) for f in self.reg.loss_functions if f.params)
        elif kind == 'name':
            text = 'a word'
        else:   # tokens
            text = 'the rest of the line'
        # For these kinds the registry's text says what the value is; for
        # the others it is a sentence after the description of the value.
        if arg.doc and kind in TEXT_DESCRIBES:
            text = ''
        flags = []
        if arg.optional:
            flags.append('optional')
        if arg.repeat:
            flags.append('repeatable')
        if arg.default is not None:
            flags.append('default ' + literal(arg.default))
        doc = prose(sentence(arg.doc)) if arg.doc else ''
        if text:
            out = sentence('; '.join([text] + flags)) + (' ' + doc if doc
                                                          else '')
        else:
            out = doc + (' ' + capital(sentence('; '.join(flags))) if flags
                         else '')
        return f'{literal("<" + arg.name + ">")}: {out}'

    def values(self, entry, args):
        return entry.items('Values', [self.arg_text(a) for a in args])

    @staticmethod
    def default(entry, default):
        if default is None:
            return entry
        if default.literal is not None:
            return entry.line('Default', literal(default.literal))
        return entry.line('Default', prose(sentence(default.text)))

    def rules(self, entry, ident):
        return entry.line('Rules', ', '.join(
            role('key', r) for r in self.rules_of.get(ident, [])))

    def long_texts(self, entry, e):
        entry.para(getattr(e, 'long_doc', ''))
        entry.para(getattr(e, 'quirk', ''))
        return entry

    # ---- registry entries -----------------------------------------------
    def key_entry(self, k, merge_optim=False):
        entry = Entry('key', k.id, k.name, names=[k.name],
                      also=('OPTIM',) if merge_optim else ())
        entry.para(k.doc)
        self.long_texts(entry, k)
        synopsis = [k.synopsis]
        examples = list(k.examples)
        if merge_optim:
            optim = self.reg.optim
            entry.para(optim.doc)
            synopsis = optim.fields['synopsis']
            examples += optim.examples
        entry.items('Syntax', [literal(s) for s in synopsis])
        self.values(entry, k.args)
        self.default(entry, k.default)
        where = self.reg.groups[k.group].blocks if k.group else k.blocks
        entry.line('Where', self.blocks(where))
        entry.line('Repeat', KEY_REPEAT[k.repeat])
        if k.alias_of:
            entry.line('Alias of', self.cite(k.alias_of))
        if k.refused:
            entry.line('Refused', prose(sentence(k.refused)))
        self.rules(entry, k.id)
        if merge_optim:
            self.rules(entry, 'OPTIM')
        if k.env:
            entry.line('Override', role('key', 'ENV.' + k.env))
        return entry.examples(examples)

    def option_entry(self, o):
        lines = ', '.join(self.cite_line(n) for n in o.lines)
        entry = Entry('key', o.id, o.option, names=[o.option],
                      annotation='(' + ', '.join(
                          n.split('.')[-1] for n in o.lines) + ')')
        entry.para(o.doc)
        self.long_texts(entry, o)
        entry.line('Syntax', literal(o.synopsis))
        self.values(entry, o.args)
        self.default(entry, o.default)
        entry.line('Where', 'on ' + lines + (' lines' if len(o.lines) > 1
                                              else ' line'))
        entry.line('Repeat', OPTION_REPEAT[o.repeat])
        if o.refused:
            entry.line('Refused', prose(sentence(o.refused)))
        self.rules(entry, o.id)
        return entry.examples(o.examples)

    def cite_line(self, name):
        """A role citing the line or form *name* of a line option."""
        if name in self.by_id and type(self.by_id[name]).__name__ == 'Form':
            return role('key', name)
        return role('key', 'ROOT.' + name)

    def form_entry(self, f):
        text = f.word or f.id.split('.', 1)[1]
        entry = Entry('key', f.id, text, names=[f.word] if f.word else [],
                      annotation=f'({f.line})')
        entry.para(f.doc)
        self.long_texts(entry, f)
        entry.line('Syntax', literal(f.synopsis))
        self.values(entry, f.args)
        entry.line('Where', role('key', 'ROOT.' + f.line))
        if f.refused:
            entry.line('Refused', prose(sentence(f.refused)))
        options = [o for o in self.reg.line_options if f.id in o.lines]
        entry.line('Options', ', '.join(role('key', o.id) for o in options))
        self.rules(entry, f.id)
        return entry.examples(f.examples)

    def block_entry(self, b):
        entry = Entry('block', b.id, b.name, names=[b.name])
        entry.para(b.doc)
        entry.para(b.quirk)
        entry.line('Syntax', f'{literal(b.name)} … {literal(b.end)}')
        entry.line('Where', self.blocks(b.parents))
        entry.line('Repeat', BLOCK_REPEAT[b.repeat])
        refused = {r.key for r in b.refuse}
        keys = [role('key', k.id) for k in self.reg.keys_in_block(b.name)
                if k.name not in refused]
        entry.line('Keys', ', '.join(keys))
        entry.items('Refused', [f'{literal(r.key)}: {prose(r.why)}'
                                for r in b.refuse])
        entry.items('Alias', [f'{literal(w)} is read as {self.cite(i)}'
                              for w, i in b.aliases.items()])
        children = [c.name for c in self.reg.blocks if b.name in c.parents]
        entry.line('Blocks', self.blocks(children))
        self.rules(entry, b.id)
        return entry.examples(b.examples)

    def section_entry(self, s, text):
        entry = Entry('key', s.id, text)
        entry.para(s.doc)
        return entry.examples(s.examples)

    def group_entry(self, g):
        entry = Entry('key', g.id, 'Optimiser keys')
        entry.para(g.doc)
        entry.line('Where', self.blocks(g.blocks))
        entry.line('Keys', ', '.join(role('key', k.id) for k in self.reg.keys
                                     if k.group == g.name))
        return entry.examples(g.examples)

    def removed_entry(self, r):
        text = (self.reg.structure.fields['removed_msg']
                .replace('{key}', r.name).replace('{hint}', r.hint))
        entry = Entry('key', r.id, r.name, directive='removed',
                      names=[r.name], options=[('hint', ' '.join(
                          r.hint.split())), ('scope', 'key')])
        entry.para(r.doc)
        return entry.field('Message', message(text))

    def env_entry(self, e):
        entry = Entry('key', e.id, e.name, names=[e.name])
        entry.para(e.doc)
        entry.line('Overrides', self.cite(e.key))
        entry.line('Values', ', '.join(literal(v) for v in e.values))
        return entry.examples(e.examples)

    def rule_entry(self, r):
        entry = Entry('key', r.id, r.id[len('RULE.'):])
        entry.para(r.doc)
        entry.para(r.quirk)
        entry.line('Checked', CHECKED[r.check])
        entry.line('Keys', ', '.join(self.cite(i) for i in r.keys))
        if r.msg:
            entry.field('Message', message(r.msg))
        return entry.examples(r.examples)

    def loss_entry(self, f):
        entry = Entry('key', f.id, f.name, names=[f.name])
        entry.para(f.doc)
        syntax = 'LOSS ' + f.name + ''.join(f' [<{p.name}>]' for p in f.params)
        entry.line('Syntax', literal(syntax))
        entry.items('Values', [
            f'{literal("<" + p.name + ">")}: a number'
            + (f' > {number(p.gt)}' if p.gt is not None else '')
            + f'; default {literal(number(p.default))}.'
            + (f' {prose(sentence(p.doc))}' if p.doc else '')
            for p in f.params])
        return entry.examples(f.examples)

    def strategy_type_entry(self, t):
        entry = Entry('key', t.id, t.name, names=[t.name],
                      annotation='(STRATEGY TYPE)')
        entry.para(t.doc)
        entry.para(t.quirk)
        entry.line('Syntax', literal('TYPE ' + t.name))
        entry.line('Keys', ', '.join(role('key', i) for i in t.keys))
        self.rules(entry, t.id)
        return entry.examples(t.examples)

    def init_method_entry(self, m):
        entry = Entry('key', m.id, m.name, names=[m.name],
                      annotation='(INIT METHOD)')
        entry.para(m.doc)
        entry.para(m.quirk)
        entry.line('Syntax', literal(' '.join(
            ['METHOD', m.name] + [f'<{a.name}>' for a in m.args])))
        self.values(entry, m.args)
        if m.refused:
            entry.line('Refused', prose(sentence(m.refused)))
        return entry.examples(m.examples)

    def optim_target_entry(self, t):
        entry = Entry('key', t.id, t.key, annotation='(OPTIM)')
        entry.para(t.doc)
        entry.line('Status', 'refused as a search parameter'
                   if t.status == 'refused' else 'read in its own way')
        entry.line('Indices', ', '.join(str(i) for i in t.indices))
        if t.condition:
            entry.line('When', prose(sentence(t.condition)))
        if t.msg:
            entry.field('Message', message(t.msg))
        return entry.examples(t.examples)

    def param_entry(self, p):
        entry = Entry('param', p.id, p.name, names=[p.name])
        entry.para(p.doc)
        self.long_texts(entry, p)
        values = ' '.join(f'<{a.name}>' for a in p.args) or '<value>'
        syntax = [literal(f'PARAM {p.name} {values}')]
        if p.indexed:
            syntax.append(literal(f'PARAM {p.name}[<i>] {values}'))
        entry.items('Syntax', syntax)
        self.values(entry, p.args)
        self.default(entry, p.default)
        if p.optimizers != ['*']:
            entry.line('Where', ', '.join(self.opt(o) for o in p.optimizers))
        return entry.examples(p.examples)

    def param_excluded_entry(self, p):
        entry = Entry('param', p.id, p.name, names=[p.name])
        entry.para(p.doc)
        entry.line('Where', ', '.join(self.opt(o) for o in p.optimizers))
        entry.field('Refused', message(p.why))
        return entry.examples(p.examples)

    def param_policy_entry(self, p):
        entry = Entry('param', p.id, f"PARAM ({p.id[len('PARAM.'):]})")
        entry.para(p.doc)
        entry.para(p.quirk)
        entry.line('Where', ', '.join(self.opt(o) for o in p.optimizers))
        return entry.examples(p.examples)

    # ---- the optimiser list -----------------------------------------------
    def optimizer_entry(self, o):
        name = f"{o['lib']}:{o['algo']}"
        entry = Entry('opt', name, name, names=[o['algo']])
        if o['status'] == 'refused':
            entry.line('Status', 'refused')
            return entry.line('Reason', prose(sentence(o['reason'])))
        entry.line('Status', 'available')
        entry.line('Search', prose(o['row_text']))
        stops = ', '.join(role('key', 'OPTIMIZER.' + s) for s in o['stops'])
        notes = [prose(capital(sentence(o[n])))
                 for n in ('tolerance_note', 'untested_tolerance_note')
                 if o[n]]
        if notes:
            stops = ' '.join(([sentence(stops)] if stops else []) + notes)
        entry.line('Stop keys', stops)
        need = 'yes' if o['needs_stop'] else 'no'
        if o['stop_note']:
            need += ': ' + prose(o['stop_note'])
        need = sentence(need)
        if o['ignores_forced_stop']:
            need += ' Only ' + role('key', 'OPTIMIZER.MAXEVAL') + \
                ' stops the search.'
        entry.line('Needs a stop', need)
        reads = ', '.join(role('key', 'OPTIMIZER.' + r) for r in o['reads'])
        if o['warns_unread_seed']:
            reads = (reads + '; ' if reads else '') + \
                'a ' + role('key', '!OPTIMIZER.SEED') + ' line gets a warning'
        entry.line('Reads', reads)
        # How the optimiser uses STEP, as the ALGO help says it; when it
        # refuses STEP, the reason its message gives (the message reads
        # "<LIB>:<ALGO> does not use STEP; <reason>").
        if o['step_consumed']:
            entry.line('STEP', prose(sentence(o['step_note']
                                              or o['step_use_text'])))
        elif o['step_note']:
            entry.field('STEP', ['refused, with the reason:'],
                        message(o['step_note']))
        else:
            entry.line('STEP', 'refused.')
        entry.line('Starts at', 'the point the search is given' if
                   o['starts_at_x0'] else prose(sentence(o['start_note'])))
        entry.line('Random draws', RANDOM.get(o.get('random_draws'), ''))
        entry.line('Bounds', prose(sentence(o['bounds_note'])) if
                   o['ignores_bounds'] else 'inside the OPTIM box')
        if o['accepts_inner']:
            inner = 'an ' + role('block', 'BLOCK.INNER') + \
                ' block sets its local search'
            inner += ('; without one, ' + self.opt('NLOPT:' + o['default_inner'])
                      if o['default_inner'] else '; the block is required')
            entry.line('INNER', sentence(inner))
        if o['accepts_multi_seed']:
            entry.line('Start points', 'one run takes several')
        entry.line('PARAM', ', '.join(role('param', i)
                                      for i in self.params_of(name)))
        return entry

    def params_of(self, name):
        lib = name.split(':')[0]
        out = [p.id for p in self.reg.param_policies
               if name in p.optimizers or lib in p.optimizers]
        out += [p.id for p in self.reg.params
                if (name in p.optimizers or lib in p.optimizers)
                and not p.id.startswith('PARAM.ASA:')]
        return out

    # ---- the annealers --------------------------------------------------
    def anneal_entries(self, policy):
        keys = self.anneal.get('key', [])
        rules = self.anneal.get('rule', [])
        renamed = self.anneal.get('renamed', [])
        live = {k['name'] for k in keys}
        examples = {c.text.split()[1]: c for c in policy.examples
                    if c.text.startswith('PARAM ')}
        by_group = {}
        for k in keys:
            rejected = {r['kind'] for r in rules if r['type'] == 'kind_reject'
                        and r['key'] == k['name']}
            kinds = [kd for kd in rak.KINDS if kd in k['kinds']
                     and kd not in rejected]
            entry = Entry('param', 'PARAM.TADAH:ANNEAL.' + k['name'],
                          k['name'], names=[k['name']])
            entry.para(k['doc'])
            if k['type'] == 'enum':
                value = ' | '.join(k['values'])
            elif k['type'] == 'bool':
                value = 'ON | OFF'
            else:
                value = '<value>'
            entry.line('Syntax', literal(f"PARAM {k['name']} {value}"))
            entry.line('Values', rak.range_for(k))
            defaults = [(kd, rak.default_for(k, kd)) for kd in kinds]
            if len({d for _kd, d in defaults}) == 1:
                entry.line('Default', defaults[0][1])
            else:
                entry.field('Default', [f'- {ANNEALERS[kd]}: {d}'
                                        for kd, d in defaults])
            entry.line('Where', ', '.join(self.opt(ANNEALERS[kd])
                                          for kd in kinds))
            said = []
            for kd in kinds:
                kind_keys = {x['name'] for x in keys if kd in x['kinds']}
                for s in rak.constraint_sentences(k['name'], rules,
                                                  kind_keys):
                    if s not in said:
                        said.append(s)
            # The annealers that refuse the key, each with its message.
            refusals = [annealer_message(kd, r['msg']) for kd in rak.KINDS
                        for r in rules if r['type'] == 'kind_reject'
                        and r['key'] == k['name'] and r['kind'] == kd]
            refusals += [annealer_message(kd, r['msg']) for r in renamed
                         if r['old'] == k['name'] for kd in rak.KINDS
                         if kd in r['kinds']]
            entry.line('Rules', ' '.join(said))
            entry.field('Refused', *[message(m) for m in refusals])
            if k['name'] in examples:
                entry.examples([examples[k['name']]])
            by_group.setdefault(k['group'], []).append((k['rank'], k['name'],
                                                        entry))
        groups = []
        for group in rak.GROUP_ORDER:
            rows = sorted(by_group.pop(group, []), key=lambda r: r[:2])
            if rows:
                groups.append((rak.GROUP_TITLES[group],
                               [e for _r, _n, e in rows]))
        if by_group:
            raise GenerationError('anneal_keys.toml: keys of groups without '
                                  'a title: ' + ', '.join(sorted(by_group)))
        retired = []
        for r in renamed:
            if r['old'] in live:
                continue
            by = ', '.join(ANNEALERS[kd] for kd in rak.KINDS
                           if kd in r['kinds'])
            entry = Entry('param', 'PARAM.TADAH:ANNEAL.' + r['old'], r['old'],
                          directive='removed', names=[r['old']],
                          options=[('hint', ' '.join(r['msg'].split())),
                                   ('scope', 'param')])
            entry.line('Where', by)
            entry.field('Message', *[message(annealer_message(kd, r['msg']))
                                     for kd in rak.KINDS if kd in r['kinds']])
            retired.append(entry)
        return groups, retired

    # ---- pages ----------------------------------------------------------
    def place(self, page, entries):
        for e in entries:
            for ident in (e.ident,) + tuple(e.also):
                if ident in self.by_id:
                    if ident in self.placed:
                        raise GenerationError(f'{ident} is on two pages')
                    self.placed[ident] = page.name
        return entries

    def pages(self):
        reg = self.reg
        pc_lines = {'PC_EMAE', 'PC_ERMSE', 'PC_ErRMSE', 'PC_ERSQ', 'PC_SRMSE',
                    'PC_FRMSE', 'PC_LAMMPS', 'PC_FAILPROBE', 'PC_OBSERVABLE',
                    'LOSS', 'OPTIM'}

        def lines_key(k):
            return k.blocks == ['ROOT'] and (
                k.name in pc_lines or k.name.startswith('PC_')
                or (k.alias_of and k.alias_of.split('.')[-1].startswith('PC_')))

        def with_options(keys):
            out = []
            for k in keys:
                out.append(self.key_entry(k, merge_optim=k.id == 'ROOT.OPTIM'))
                out += [self.option_entry(o) for o in reg.line_options
                        if o.lines[0] == k.name]
            return out

        # target_keys ----------------------------------------------------
        target = Page('target_keys', 'nfp_ref_target_keys',
                      'Top-Level Keys and Blocks',
                      'This page lists how a line of a target file is read, '
                      'the eight blocks and the top-level keys, each entry '
                      'with its syntax, values, default and an example. The '
                      'keys inside the blocks are on '
                      ':ref:`nfp_ref_optimizer_keys`; the target lines, the '
                      f"loss functions and the {role('key', 'ROOT.OPTIM')} "
                      'lines are on :ref:`nfp_ref_pc_options`.')
        target.section('Reading a line', self.place(target, [
            self.section_entry(reg.lexing, 'Lines'),
            self.section_entry(reg.reading, 'Numbers and words'),
            self.section_entry(reg.structure, 'Block structure')]))
        target.section('Blocks', self.place(target, [
            self.block_entry(b) for b in reg.blocks]))
        root = [k for k in reg.keys if k.blocks == ['ROOT']
                and not lines_key(k)]
        target.section('Top-level keys', self.place(target,
                                                    with_options(root)))
        target.section('Environment variables', self.place(target, [
            self.env_entry(e) for e in reg.envs]))
        target.section('Removed keys', self.place(target, [
            self.removed_entry(r) for r in reg.removed]))

        # optimizer_keys -------------------------------------------------
        opt = Page('optimizer_keys', 'nfp_ref_optimizer_keys',
                   'OPTIMIZER Block Keys',
                   'This page lists the keys of the blocks inside '
                   f"{role('block', 'BLOCK.OPTIMIZER')}: the keys of an "
                   'optimiser, valid in each block that runs one, and the '
                   f"keys of the {role('block', 'BLOCK.STRATEGY')}, "
                   f"{role('block', 'BLOCK.INIT')}, "
                   f"{role('block', 'BLOCK.NOISE')} and "
                   f"{role('block', 'BLOCK.RESTART')} blocks, with the "
                   'STRATEGY types and the INIT methods. The blocks are on '
                   ':ref:`nfp_ref_target_keys`; the optimisers and their '
                   f"{role('key', 'OPTIMIZER.PARAM')} keys are on "
                   ':ref:`nfp_ref_optimisers`.')
        group = [self.group_entry(g) for g in reg.groups.values()]
        group += [self.key_entry(k) for k in reg.keys
                  if k.group or k.id == 'OPTIMIZER.LOG_HP']
        opt.section('Keys of an optimiser', self.place(opt, group))
        for block, extra in (('STRATEGY', reg.strategy_types),
                             ('INIT', reg.init_methods), ('NOISE', []),
                             ('RESTART', [])):
            keys = [self.key_entry(k) for k in reg.keys if not k.group
                    and k.id != 'OPTIMIZER.LOG_HP' and k.scope == block]
            kinds = [self.strategy_type_entry(t) if block == 'STRATEGY'
                     else self.init_method_entry(t) for t in extra]
            opt.section(f'{block} block', self.place(opt, keys + kinds))

        # pc_options -----------------------------------------------------
        pc = Page('pc_options', 'nfp_ref_pc_options',
                  'Targets, Losses and Search Parameters',
                  'This page lists the lines that make up the loss of an '
                  'evaluation (the validation metrics, the LAMMPS scripts '
                  f"of {role('key', 'ROOT.PC_LAMMPS')} and "
                  f"{role('key', 'ROOT.PC_FAILPROBE')} with their "
                  'options, the observables of '
                  f"{role('key', 'ROOT.PC_OBSERVABLE')}), the loss "
                  f"functions, and the {role('key', 'ROOT.OPTIM')} lines "
                  'that make up the search space.')
        metrics = [k for k in reg.keys if lines_key(k)
                   and k.name in ('PC_EMAE', 'PC_ERMSE', 'PC_ErRMSE',
                                  'PC_ERSQ', 'PC_SRMSE', 'PC_FRMSE')]
        pc.section('Validation metrics', self.place(pc, with_options(metrics)))
        # The script lines first, then the settings of their scripts.
        scripts = sorted((k for k in reg.keys if lines_key(k)
                          and k not in metrics
                          and k.name not in ('PC_OBSERVABLE', 'LOSS',
                                             'OPTIM')),
                         key=lambda k: k.name not in ('PC_LAMMPS',
                                                      'PC_FAILPROBE'))
        pc.section('LAMMPS scripts', self.place(pc, with_options(scripts)))
        observable = [k for k in reg.keys if k.id == 'ROOT.PC_OBSERVABLE']
        obs = with_options(observable)
        for f in reg.forms:
            if f.line == 'PC_OBSERVABLE':
                obs.append(self.form_entry(f))
                obs += [self.option_entry(o) for o in reg.line_options
                        if o.lines == [f.id]]
        pc.section('Observables', self.place(pc, obs))
        loss = [k for k in reg.keys if k.id == 'ROOT.LOSS']
        pc.section('Loss functions', self.place(pc, with_options(loss) + [
            self.loss_entry(f) for f in reg.loss_functions]))
        optim = with_options([k for k in reg.keys if k.id == 'ROOT.OPTIM'])
        for f in reg.forms:
            if f.line == 'OPTIM':
                optim.append(self.form_entry(f))
                optim += [self.option_entry(o) for o in reg.line_options
                          if o.lines == [f.id]]
        pc.section('Search parameters', self.place(pc, optim))
        pc.section('Keys that OPTIM refuses or reads in its own way',
                   self.place(pc, [self.optim_target_entry(t)
                                   for t in reg.optim_targets]))

        # optimisers -----------------------------------------------------
        meta = self.optimizers.get('meta', {})
        optimisers = Page(
            'optimisers', 'nfp_ref_optimisers', 'Optimisers',
            'This page lists every optimiser an '
            f"{role('block', 'BLOCK.OPTIMIZER')} block can name with "
            f"{role('key', 'OPTIMIZER.LIB')} and "
            f"{role('key', 'OPTIMIZER.ALGO')}, as ``tadah explain ALGO`` "
            'lists them, with what each reads and how it searches; the '
            f"{role('opt', 'NLOPT')} algorithms are those of NLopt "
            f"{meta.get('nlopt_version')}. A refused optimiser has the "
            'reason and the optimisers that replace it. The '
            f"{role('key', 'OPTIMIZER.PARAM')} keys of the annealers and "
            f"of {role('opt', 'ASA:ASA')} are on "
            ':ref:`nfp_ref_anneal_params` and :ref:`nfp_ref_asa_params`.')
        listed = self.optimizers.get('optimizer', [])
        for lib in meta.get('libraries', []):
            items = [self.optimizer_entry(o) for o in listed
                     if o['lib'] == lib]
            items += self.place(optimisers, [
                self.param_policy_entry(p) for p in reg.param_policies
                if p.optimizers[0].split(':')[0] == lib
                and p.id not in ('PARAM.ASA:ASA', 'PARAM.TADAH:ANNEAL')])
            items += self.place(optimisers, [
                self.param_entry(p) for p in reg.params
                if p.optimizers != ['*'] and not p.id.startswith('PARAM.ASA:')
                and p.optimizers[0].split(':')[0] == lib])
            optimisers.section(lib, items, before=[
                '.. hpo:target:: ' + lib, '   :kind: opt', '   :text: ' + lib,
                ''])
        optimisers.section('Finite differences', self.place(optimisers, [
            self.param_entry(p) for p in reg.params if p.optimizers == ['*']]),
            intro=f"The {role('key', 'OPTIMIZER.PARAM')} keys below set the "
                  'finite differences of an optimiser that takes them.')

        # anneal_params --------------------------------------------------
        policy = self.by_id['PARAM.TADAH:ANNEAL']
        anneal = Page('anneal_params', 'nfp_ref_anneal_params',
                      'Annealing PARAM Keys',
                      'This page lists the '
                      f"{role('key', 'OPTIMIZER.PARAM')} keys of the "
                      f"annealers {role('opt', 'TADAH:ANNEAL')}, "
                      f"{role('opt', 'TADAH:ANNEAL_PT')} and "
                      f"{role('opt', 'TADAH:ANNEAL_PA')}, grouped by what "
                      'they set. A key '
                      'whose default differs between the annealers lists '
                      'the default of each.')
        groups, retired = self.anneal_entries(policy)
        anneal.section('The annealers', self.place(anneal, [
            self.param_policy_entry(policy)]))
        for title, entries in groups:
            anneal.section(title, entries)
        anneal.section('Retired names', retired)

        # asa_params -----------------------------------------------------
        asa = Page('asa_params', 'nfp_ref_asa_params', 'ASA PARAM Keys',
                   'This page lists the '
                   f"{role('key', 'OPTIMIZER.PARAM')} keys of "
                   f"{role('opt', 'ASA:ASA')}, Adaptive Simulated "
                   'Annealing: the options of ASA under '
                   'their ASA names, and the options of ASA that '
                   f"{role('opt', '!ASA:ASA')} does not take.")
        asa.section('Options', self.place(asa, [
            self.param_policy_entry(self.by_id['PARAM.ASA:ASA'])] + [
            self.param_entry(p) for p in reg.params
            if p.id.startswith('PARAM.ASA:')]))
        asa.section('Options not taken', self.place(asa, [
            self.param_excluded_entry(p) for p in reg.param_excluded]))

        pages = [target, opt, pc, optimisers, anneal, asa]

        # The rules: each on the page of the first entry it refers to that
        # is not a block.
        page_of = {p.name: p for p in pages}
        rules_by_page = {}
        for r in reg.rules:
            home = next((self.placed[i] for i in r.keys
                         if not i.startswith('BLOCK.') and i in self.placed),
                        'target_keys')
            rules_by_page.setdefault(home, []).append(self.rule_entry(r))
        for name in PAGES:
            if name in rules_by_page:
                page_of[name].section('Rules', self.place(
                    page_of[name], rules_by_page[name]))

        missing = [i for i in self.by_id if i not in self.placed]
        if missing:
            raise GenerationError('registry entries without a place on the '
                                  'reference pages: ' + ', '.join(missing))
        return pages


# ------------------------------------------------------------- driver ----

def load_loader(blueprint):
    """HPO's hpotarget_registry module, imported from *blueprint* without
    writing bytecode into the HPO checkout."""
    path = os.path.join(blueprint, LOADER)
    spec = importlib.util.spec_from_file_location('hpotarget_registry', path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    dont_write = sys.dont_write_bytecode
    sys.dont_write_bytecode = True
    try:
        spec.loader.exec_module(module)
    finally:
        sys.dont_write_bytecode = dont_write
    api = getattr(module, 'API', None)
    if api != LOADER_API:
        raise GenerationError(f'{path} has API {api}; this generator reads '
                              f'API {LOADER_API}')
    return module


def read_sources(hpo_root):
    """(registry, optimiser list, anneal keys) of the HPO checkout at
    *hpo_root*."""
    blueprint = os.path.join(hpo_root, BLUEPRINT)
    loader = load_loader(blueprint)
    try:
        reg = loader.load(blueprint)
    except loader.RegistryError as err:
        raise GenerationError(f'the registry in {blueprint} does not pass '
                              f"HPO's loader:\n{err}") from None
    with open(os.path.join(blueprint, reg.meta.optimizers), 'rb') as fh:
        optimizers = tomllib.load(fh)
    with open(os.path.join(blueprint, reg.meta.anneal_keys), 'rb') as fh:
        anneal = tomllib.load(fh)
    return reg, optimizers, anneal


def render(reg, optimizers, anneal):
    """{page name: text} for the six reference pages."""
    pages = Writer(reg, optimizers, anneal).pages()
    return {p.name: p.render() for p in pages}


def locate_hpo():
    """The HPO checkout, which must hold the registry loader."""
    path = source_locator.locate_file('HPO', os.path.join(BLUEPRINT, LOADER))
    return os.path.dirname(os.path.dirname(path))


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--out', default=OUT_DIR,
                    help='the directory of the pages (default: '
                         'docs/source/nfp/reference)')
    args = ap.parse_args(argv)
    hpo = locate_hpo()
    pages = render(*read_sources(hpo))
    os.makedirs(args.out, exist_ok=True)
    for name in PAGES:
        path = os.path.join(args.out, name + '.rst')
        with open(path, 'w', encoding='utf-8') as fh:
            fh.write(pages[name])
    print(f'read_hpo_registry.py: wrote {len(PAGES)} pages from {hpo} into '
          f'{args.out}')


if __name__ == '__main__':
    try:
        main()
    except source_locator.SourceNotFound as err:
        sys.exit(f'read_hpo_registry.py: {err}')
    except GenerationError as err:
        sys.exit(f'read_hpo_registry.py: {err}')
