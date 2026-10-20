#!/usr/bin/env python3
"""Generate the Nested Fitting reference pages from HPO's registries.

    read_hpo_registry.py [--out DIR]

Sources, in HPO's blueprint/ directory:

    hpotarget.toml and its part files   the target-file registry, loaded
                                        and checked by HPO's own loader,
                                        hpotarget_registry.py
    optimizers_gen.toml                 the optimiser list
    optimizers_doc.toml                 what each optimiser is (read by the
                                        loader, meta.optimizer_docs)
    anneal_keys.toml                    the PARAM keys of the annealers

Pages, written into docs/source/nfp/reference/ (or DIR) and ignored by git;
the hub, nfp/reference/index.rst, is committed:

    target_keys.rst      how a line is read, the blocks, the top-level keys
                         (but those of pc_options.rst), the environment
                         variables, the removed keys
    optimizer_keys.rst   the keys of an optimiser, of the STRATEGY, INIT,
                         NOISE and RESTART blocks, the STRATEGY types and
                         INIT methods
    pc_options.rst       the validation metrics, PC_LAMMPS, PC_FAILPROBE, the
                         LAMMPS script and the settings of the scripts,
                         PC_OBSERVABLE, LOSS and the loss functions, OPTIM
    optimisers.rst       every LIB:ALGO of the optimiser list, and the PARAM
                         keys but those of the annealers and of ASA
    anneal_params.rst    the PARAM keys of the annealers
    asa_params.rst       the PARAM keys of ASA:ASA

Every entry of the target-file registry has one entry on these pages, and
a rule each refers to has a place in its page's Rules section; an entry
is a directive of the hpo domain (docs/tools/hpotarget_domain.py) whose
Sphinx target is the registry id, so that the :hpo: roles find it. An
entry headed by words (a rule, a section of the registry, the LAMMPS
script) is a topic. Its body is the registry's text (doc, long_doc,
quirk) and a field list (Syntax, Values, Default, Where, Repeat, Rules,
Override, Checks, Example and the like; an empty field is left out). The
Checks field quotes the message of each check of the entry (checks, a
block's requires and close_checks) as program output, captioned with the
check's doc, which says when it comes.

The registry's text becomes reStructuredText here (Writer.text): a name of
one entry of the registry is a role, which links at its first mention in
an entry; a `code` span is an inline literal; an abbreviation of the
registry's table ([abbreviations]) is spelled out at its first use on a
page; "the optimiser list" links to the optimisers page. A quoted message
of tadah hpo is a code block of program output; an example is a code
block, its doc (and the environment it runs with) the block's caption.

Code lines are at most CODE_WIDTH characters: a longer target-file line is
continued with a backslash, as the target file allows. The output depends
only on the registries (no date, no path), so two runs write the same
files.

The HPO checkout is found as read_anneal_keys.py finds it
(source_locator.py); a checkout without the registry, a registry the
loader refuses, or a loader of another API stops the build with the
reason, and so does a Python without tomllib (3.10 and older), a field of
the registry these pages do not account for (FIELDS, SECTION_FIELDS: each
field is shown or listed with the reason it is not), a repeat rule or a
check stage they have no words for, and an optimiser or an annealer key
the registries leave without the text the pages need.
"""

from __future__ import annotations

import argparse
import dataclasses
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
#: Fields every entry of the registry has, and what the pages make of them.
_ENTRY_FIELDS = {
    'id': 'the target of the entry',
    'part': 'not shown: the file the entry is written in',
    'examples': 'Example: each a code block, its doc the caption',
    'invalid': 'not shown: the conformance test runs them',
    'untested': 'not shown: why no invalid line shows a message',
}
_TEST = 'not shown: how the conformance test runs a case'
#: Every field of every object of the target-file registry (the loader's
#: dataclasses, by class name) and what the pages make of it. A field that
#: is not shown says why: a message whose condition a field of the entry
#: states in words (Repeat, Where, Values, Keys), or a field of the tests.
#: The generator stops on a field or a class not listed here
#: (check_fields), so that a field the registry gains is shown on the
#: pages or listed here with the reason it is not.
FIELDS = {
    'Registry': {
        'directory': 'not shown: where the files are',
        'files': 'not shown: the files read',
        'full': 'not shown: whether every part file was read',
        'meta': 'read: the files of the other registries',
        'lexing': 'topic LEXING', 'reading': 'topic READING',
        'structure': 'topic STRUCTURE', 'optim': 'the entry of OPTIM',
        'lammps_script': 'topic LAMMPS_SCRIPT',
        'groups': 'topics', 'blocks': 'entries', 'keys': 'entries',
        'line_options': 'entries', 'forms': 'entries',
        'removed': 'entries', 'rules': 'topics', 'envs': 'entries',
        'loss_functions': 'entries', 'strategy_types': 'entries',
        'init_methods': 'entries', 'optim_targets': 'entries',
        'params': 'entries', 'param_excluded': 'entries',
        'param_policies': 'entries',
        'wrappers': _TEST, 'contexts': _TEST,
        'fixtures': 'the script of LAMMPS_SCRIPT; the others: ' + _TEST,
        'abbreviations': 'spelled out at their first use on a page',
        'optimizer_docs': 'the first paragraph of an optimiser or library',
        'notes': "not shown: the loader's notes on a part read alone",
    },
    'Meta': {
        'version': 'not shown: checked by the loader',
        'api': 'not shown: checked by the generator (LOADER_API)',
        'parts': 'not shown: the files the loader reads',
        'anneal_keys': 'read: the PARAM keys of the annealers',
        'optimizers': 'read: the optimiser list',
        'optimizer_docs': 'read by the loader',
    },
    'Section': dict(_ENTRY_FIELDS, fields='SECTION_FIELDS',
                    doc='paragraph', long_doc='paragraph'),
    'Group': dict(_ENTRY_FIELDS, name='the scope of its text',
                  blocks='Where', doc='paragraph', long_doc='paragraph',
                  leftover_msg='not shown: the Values of each key say how '
                               'many values it takes'),
    'Block': dict(_ENTRY_FIELDS, name='heading', end='Syntax',
                  parents='Where', repeat='Repeat',
                  repeat_msg='not shown: the Repeat field states it',
                  misplaced_msg='not shown: the Where field states it',
                  unknown_msg='not shown: the Keys field states it',
                  unknown_first='not shown: the order of the checks',
                  aliases='Alias', refuse='Refused',
                  refuse_msg='not shown: the Refused field states it',
                  unreachable_msgs='not shown: no target file reaches them',
                  requires='Checks', close_checks='Checks',
                  doc='paragraph', long_doc='paragraph', quirk='paragraph'),
    'Key': dict(_ENTRY_FIELDS, name='heading', synopsis='Syntax',
                repeat='Repeat',
                repeat_msg='not shown: the Repeat field states it',
                repeat_doc='Repeat', group='Where', blocks='Where',
                args='Values',
                misplaced_msg='not shown: the Where field states it',
                default='Default', alias_of='Alias of', refused='Refused',
                env='Override', checks='Checks', doc='paragraph',
                long_doc='paragraph', quirk='paragraph'),
    'LineOption': dict(_ENTRY_FIELDS, option='heading', lines='Where',
                       synopsis='Syntax', args='Values', repeat='Repeat',
                       repeat_msg='not shown: the Repeat field states it',
                       default='Default', refused='Refused',
                       checks='Checks', doc='paragraph',
                       long_doc='paragraph', quirk='paragraph'),
    'Form': dict(_ENTRY_FIELDS, line='Where', word='heading',
                 synopsis='Syntax', args='Values', refused='Refused',
                 checks='Checks', doc='paragraph', long_doc='paragraph',
                 quirk='paragraph'),
    'Removed': dict(_ENTRY_FIELDS, name='heading', hint='Message',
                    doc='paragraph'),
    'Rule': dict(_ENTRY_FIELDS, title='heading', check='Checked',
                 msg='Message', msg_doc='Message', keys='Keys',
                 doc='paragraph', quirk='paragraph'),
    'Env': dict(_ENTRY_FIELDS, name='heading', key='Overrides',
                values='Values',
                msg='not shown: the Values field and the text state it',
                checks='Checks', doc='paragraph'),
    'LossFunction': dict(_ENTRY_FIELDS, name='heading', params='Values',
                         doc='paragraph'),
    'StrategyType': dict(_ENTRY_FIELDS, name='heading', keys='Keys',
                         checks='Checks', doc='paragraph',
                         quirk='paragraph'),
    'InitMethod': dict(_ENTRY_FIELDS, name='heading', args='Values',
                       refused='Refused', checks='Checks', doc='paragraph',
                       quirk='paragraph'),
    'OptimTarget': dict(_ENTRY_FIELDS, key='heading', status='Status',
                        indices='Indices', msg='Message', condition='When',
                        doc='paragraph'),
    'Param': dict(_ENTRY_FIELDS, name='heading', optimizers='Where',
                  args='Values', default='Default', indexed='Syntax',
                  checks='Checks', doc='paragraph', long_doc='paragraph',
                  quirk='paragraph'),
    'ParamExcluded': dict(_ENTRY_FIELDS, name='heading',
                          optimizers='Where', why='Refused',
                          doc='paragraph'),
    'ParamPolicy': dict(_ENTRY_FIELDS, optimizers='Where',
                        msg='not shown: the text says which keys the '
                            'optimiser takes',
                        checks='Checks', doc='paragraph', quirk='paragraph'),
    'Case': {'text': 'code', 'doc': 'caption', 'env': 'caption',
             'owner': _TEST, 'part': _TEST, 'kind': _TEST, 'index': _TEST,
             'wrap': _TEST, 'check': _TEST, 'context': _TEST,
             'error': _TEST, 'at': _TEST, 'msg_of': _TEST},
    'Check': {'msg': 'Checks: the message', 'doc': 'Checks: the caption'},
    'Untested': {'msg': 'not shown', 'why': 'not shown'},
    'Arg': {'name': 'Values', 'kind': 'Values', 'optional': 'Values',
            'repeat': 'Values', 'min': 'Values', 'max': 'Values',
            'min_open': 'Values', 'integral': 'Values', 'words': 'Values',
            'refused_words': 'Values: the words; their messages not shown',
            'what': 'not shown: a part of the message',
            'msg': 'not shown: the Values field states it',
            'noun': 'Values', 'default': 'Values', 'doc': 'Values'},
    'Default': {'literal': 'Default', 'text': 'Default',
                'unchecked': 'not shown: why the tests check no setting'},
    'Refusal': {'key': 'Refused', 'why': 'Refused'},
    'LossParam': {'name': 'Values', 'default': 'Values', 'gt': 'Values',
                  'doc': 'Values'},
    'Wrapper': {f: _TEST for f in ('name', 'part', 'text', 'doc', 'block',
                                   'optimizer', 'context')},
    'TrainingContext': {f: _TEST for f in ('name', 'part', 'config', 'doc',
                                           'has_train_db',
                                           'has_validation')},
    'Fixture': {'name': 'not shown', 'part': 'not shown',
                'path': 'Script: the caption', 'text': 'Script',
                'doc': 'Script: the caption'},
    'OptimizerDocs': {'libraries': 'the first paragraph of a library',
                      'optimizers': 'the first paragraph of an optimiser'},
}
_READER_MSG = 'not shown: the Values of an argument state its range'
_STRUCTURE_MSG = 'not shown: the text of the section states it'
#: The fields of each section (Section.fields, by section id) and what the
#: pages make of them; check_fields stops on one not listed.
SECTION_FIELDS = {
    'LEXING': {'comment': 'not shown: the doc states it',
               'continuation': 'not shown: the long_doc states it',
               'key_case': 'not shown: the doc states it',
               'error_frame': 'not shown: the long_doc states it',
               'unfinished_msg': _STRUCTURE_MSG,
               'missing_file_msg': 'not shown: a message of the command',
               'bool_true': 'Values of a boolean',
               'bool_false': 'Values of a boolean',
               'timespec_words': 'Values of a time'},
    'READING': {f: _READER_MSG for f in (
        'integer_msg', 'integer_range_msg', 'note_msg', 'finite_msg',
        'positive_msg', 'underflow_note', 'requires_value_msg',
        'single_value_msg', 'bool_msg', 'timespec_msg',
        'script_timeout_msg', 'script_timeout_large_msg')},
    'STRUCTURE': dict({f: _STRUCTURE_MSG for f in (
        'end_without_open_msg', 'end_mismatch_msg', 'end_takes_no_value_msg',
        'open_takes_no_value_msg', 'unclosed_msg', 'unknown_root_msg')},
        check_order='not shown: the order of the checks',
        removed_msg='Message of a removed key'),
    'OPTIM': {'synopsis': 'Syntax of OPTIM',
              'positive_only': 'not shown: the text of OPTIM AUTO names '
                               'these keys',
              'auto_excluded_prefix': 'not shown: the text of OPTIM AUTO '
                                      'states it',
              'checks': 'Checks of OPTIM'},
    'LAMMPS_SCRIPT': {'fixture': 'Script'},
}
#: The ids of the entries headed by words, which are topics.
TOPIC_IDS = ('LEXING', 'READING', 'STRUCTURE', 'LAMMPS_SCRIPT')
TOPIC_PREFIXES = ('RULE.', 'GROUP.')
#: The tolerance keys, in the order of the optimiser list's stops.
TOLERANCES = ('FTOL_REL', 'FTOL_ABS', 'XTOL_REL', 'XTOL_ABS')


class GenerationError(Exception):
    """The registries cannot be written as reference pages; says why."""


def check_fields(root):
    """Stops when *root* (the registry, or any object of it) or an object it
    holds has a field that FIELDS does not list for its class, or a section
    a field that SECTION_FIELDS does not list, naming each as Class.field
    (or SECTION.field)."""
    unknown, seen = set(), set()

    def walk(obj):
        if isinstance(obj, dict):
            for value in obj.values():
                walk(value)
        elif isinstance(obj, (list, tuple)):
            for value in obj:
                walk(value)
        elif dataclasses.is_dataclass(obj) and not isinstance(obj, type):
            if id(obj) in seen:
                return
            seen.add(id(obj))
            name = type(obj).__name__
            listed = FIELDS.get(name, {})
            for f in dataclasses.fields(obj):
                if f.name not in listed:
                    unknown.add(f'{name}.{f.name}')
                walk(getattr(obj, f.name))
            if name == 'Section':
                listed = SECTION_FIELDS.get(obj.id, {})
                unknown.update(f'{obj.id}.{k}' for k in obj.fields
                               if k not in listed)
    walk(root)
    if unknown:
        raise GenerationError(
            'the registry has fields the reference pages do not account '
            'for (FIELDS and SECTION_FIELDS of read_hpo_registry.py): '
            + ', '.join(sorted(unknown)))


def wording(table, value, ident, what):
    """The words *table* has for *value* of the entry *ident*; a value it
    has none for stops the generator."""
    if value not in table:
        raise GenerationError(f'{ident}: the {what} {value!r} has no wording '
                              'on the reference pages; known: '
                              + ', '.join(sorted(table)))
    return table[value]


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


def _join(pieces):
    """Plain text and markup pieces, (is_markup, text), as one string: a
    piece of markup gets an escaped blank where the character next to it
    may not stand beside inline markup."""
    out = []
    for i, (markup, text) in enumerate(pieces):
        if not markup:
            out.append(_escape(text))
            continue
        before = pieces[i - 1][1][-1:] if i and pieces[i - 1][1] else ' '
        after = (pieces[i + 1][1][:1] if i + 1 < len(pieces)
                 and pieces[i + 1][1] else ' ')
        out.append(('' if before in _BEFORE else '\\ ') + text
                   + ('' if after in _AFTER else '\\ '))
    result = ''.join(out)
    return '\\' + result if _LEADING.match(result) else result


def prose(text):
    """Registry text as reStructuredText: `code` and --options as inline
    literals, every other markup character escaped."""
    text = ' '.join(text.split())
    pieces, pos = [], 0
    for m in _CODE.finditer(text):
        pieces.append((False, text[pos:m.start()]))
        pieces.append((True, literal(m.group(1) or m.group(2))))
        pos = m.end()
    pieces.append((False, text[pos:]))
    return _join([p for p in pieces if p[1] or p[0]])


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


def number_range(noun, lo=None, hi=None, lo_open=False, hi_open=False):
    """A number of *noun* ("a number", "an integer") and its range in
    words: from a to b, >= a, above a, <= b, below b, and the two-sided
    forms "above a, up to b", "from a, below b", "above a and below b";
    a range of one value, from a to a, is that value."""
    if lo is not None and hi is not None:
        if not lo_open and not hi_open:
            return lo if lo == hi else f'{noun} from {lo} to {hi}'
        if lo_open and hi_open:
            return f'{noun} above {lo} and below {hi}'
        if lo_open:
            return f'{noun} above {lo}, up to {hi}'
        return f'{noun} from {lo}, below {hi}'
    if lo is not None:
        return f'{noun} above {lo}' if lo_open else f'{noun} >= {lo}'
    if hi is not None:
        return f'{noun} below {hi}' if hi_open else f'{noun} <= {hi}'
    return noun


def words(items, last='or'):
    """Literals joined as a list: A, B or C."""
    items = [literal(i) for i in items]
    return items[0] if len(items) == 1 else (
        ', '.join(items[:-1]) + f' {last} ' + items[-1])


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


def code(text, width=CODE_WIDTH, caption=''):
    """A target-file fragment as a code block, with *caption* (already
    reStructuredText) when there is one."""
    lines = []
    for line in text.strip('\n').split('\n'):
        lines += wrap_line(line.rstrip(), width)
    head = ['.. code-block:: none']
    if caption:
        head.append('   :caption: ' + caption)
    return head + [''] + ['   ' + l if l else '' for l in lines]


def script(text, width=CODE_WIDTH, caption=''):
    """A LAMMPS script as a code block: its lines as they are, each at most
    *width* characters (a LAMMPS script continues a line with &, not \\)."""
    lines = text.strip('\n').split('\n')
    for line in lines:
        if len(line) > width:
            raise GenerationError(f'a line of a LAMMPS script is longer than '
                                  f'{width} characters: {line!r}')
    head = ['.. code-block:: none']
    if caption:
        head.append('   :caption: ' + caption)
    return head + [''] + ['   ' + l if l else '' for l in lines]


def annealer_message(kind, text):
    """A message of an annealer's PARAM keys as the annealer prints it,
    after its name (anneal_keys.toml: "prefixed with TADAH:<ALGO>: ")."""
    return f'{ANNEALERS[kind]}: {text}'


def message(text, width=CODE_WIDTH, caption=''):
    """A message of tadah hpo as a block of program output, its lines
    broken at blanks to at most *width* characters where a word allows,
    with *caption* (already reStructuredText) when there is one."""
    lines = []
    for line in text.rstrip('\n').split('\n'):
        lines += textwrap.wrap(line, width, break_long_words=False,
                               break_on_hyphens=False) or ['']
    head = ['.. code-block:: text']
    if caption:
        head.append('   :caption: ' + caption)
    return head + [''] + ['   ' + l if l else '' for l in lines]


# ------------------------------------------------------------ entries ----

class Entry:
    """One directive of the hpo domain: an id, its heading, its text and
    its field list. *scope* names what its text is about, for the names
    its text cites (Writer.text)."""

    def __init__(self, writer, kind, ident, text, names=(), annotation='',
                 also=(), directive=None, options=(), scope=()):
        self.writer = writer
        self.kind, self.ident, self.text = kind, ident, text
        self.names, self.annotation, self.also = list(names), annotation, also
        self.directive = directive or kind
        self.options = list(options)
        self.scope = set(scope) | set(ident.split('.'))
        self.mentioned = set()
        self.paragraphs, self.fields = [], []

    def rst(self, text, full_stop=True):
        """Registry text as reStructuredText of this entry."""
        return self.writer.text(sentence(text) if full_stop else text, self)

    def para(self, text, raw=False):
        if text and text.strip():
            self.paragraphs.append(text if raw else self.rst(text))
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

    def examples(self, cases, width=CODE_WIDTH, render=code):
        """The Example field: each case a code block, its doc (and the
        environment it runs with) the block's caption."""
        blocks, seen = [], set()
        for case in cases:
            if case.text in seen:
                continue
            seen.add(case.text)
            caption = []
            if getattr(case, 'env', None):
                env = ' '.join(f'{k}={v}' for k, v in sorted(case.env.items()))
                caption.append(f'With {literal(env)} in the environment.')
            if case.doc:
                caption.append(self.rst(case.doc))
            blocks.append(render(case.text, width, ' '.join(caption)))
        return self.field('Example', *blocks)

    def checks(self, checks):
        """The Checks field: the message of each check as program output,
        captioned with its doc, which says when the message comes (a
        placeholder of the message it names is an inline literal)."""
        return self.field('Checks', *[message(c.msg, caption=self.rst(c.doc))
                                      for c in checks])

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
    'error_per_parent': 'Once per optimiser: a second block for the '
                        'optimiser of the enclosing block is refused.',
    'merge': 'A second block continues the first.',
}
CHECKED = {'parse': 'when the file is read',
           'startup': 'when tadah hpo starts'}
RANDOM = {'never': 'never',
          'between_sides': 'only to choose between sides of equal length',
          'trial_points': 'for its trial points'}

# A token of registry text: a `code` span, an --option, the phrase that
# names the optimisers page, or a word (a name, LIB:ALGO, a dotted path,
# an abbreviation with - or +).
_TOKEN = re.compile(
    r'`(?P<code>[^`]+)`'
    r'|(?<![\w-])(?P<opt>--(?:[A-Za-z][\w-]*)?)(?![\w-])'
    r'|(?P<list>the optimiser list)'
    r'|(?<!\$)(?P<placeholder>\{[A-Za-z_][A-Za-z0-9_]*\})'
    r'|(?<![\w.:$\-])(?P<word>[A-Za-z][A-Za-z0-9_]*'
    r'(?:[.:][A-Za-z][A-Za-z0-9_]*)*(?:[-+][A-Za-z0-9]+)*)(?![\w-])')


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
        self.listed = {f"{o['lib']}:{o['algo']}": o
                       for o in optimizers.get('optimizer', [])}
        self.registered = {n: o for n, o in self.listed.items()
                           if o['status'] == 'registered'}
        self.page = None
        self.spelled = set()        # abbreviations spelled out on the page
        self.names = self.name_index()

    # ---- the names the text cites ---------------------------------------
    def name_index(self):
        """{name: [(role kind, id)]}: every name of the registry, the
        optimiser list and the annealers that a role may cite (removed
        names left out). A bare ALGO without an underscore (AUGLAG, GRID)
        names a family as often as one optimiser, so it is left out too;
        LIB:ALGO and the libraries are in."""
        reg, out = self.reg, {}

        def add(name, kind, ident):
            if name:
                out.setdefault(name, []).append((kind, ident))
        for k in reg.keys:
            add(k.name, 'key', k.id)
        for b in reg.blocks:
            add(b.name, 'block', b.id)
        for o in reg.line_options:
            add(o.option, 'key', o.id)
        for f in reg.forms:
            add(f.word, 'key', f.id)
        for kind in ('loss_functions', 'strategy_types', 'init_methods'):
            for e in getattr(reg, kind):
                add(e.name, 'key', e.id)
        for e in reg.envs:
            add(e.name, 'key', e.id)
        for p in list(reg.params) + list(reg.param_excluded):
            add(p.name, 'param', p.id)
        for k in self.anneal.get('key', []):
            add(k['name'], 'param', 'PARAM.TADAH:ANNEAL.' + k['name'])
        for name, o in self.listed.items():
            add(name, 'opt', name)
            if '_' in o['algo']:
                add(o['algo'], 'opt', name)
        for lib in self.optimizers.get('meta', {}).get('libraries', []):
            add(lib, 'opt', lib)
        return out

    def scope_of(self, ident):
        """The words an id's entry is about: the parts of its id, the group
        of a block of the optimiser group, the blocks of a key, the lines
        of an option and the scopes of a rule's entries."""
        e = self.by_id.get(ident)
        out = set(ident.split('.'))
        if ident.startswith(('PARAM.', 'BLOCK.')) or ident in self.listed:
            out.add('OPTIMIZER')
        if e is None:
            return out
        for g in self.reg.groups.values():
            if ident.split('.')[-1] in g.blocks or getattr(e, 'group', '') \
                    == g.name:
                out.add(g.name)
        out |= set(getattr(e, 'blocks', []))
        for line in getattr(e, 'lines', []):
            out |= set(line.split('.'))
        for key in getattr(e, 'keys', []):
            out |= set(key.split('.')[:-1])
        return out

    def resolve(self, name, entry, prev):
        """(kind, id) of the entry *name* cites in the text of *entry*, or
        None: a name of one entry, or of several of which one is closest to
        the entry's scope (the words of its id, its block, its group, the
        word before the name)."""
        found = self.names.get(name)
        if not found:
            return None
        if len(found) == 1:
            return found[0]
        best, score = [], -1
        for kind, ident in found:
            path = set(ident.split('.')[:-1]) if '.' in ident else set()
            s = len(path & entry.scope) + (1 if prev in path else 0)
            if s > score:
                best, score = [(kind, ident)], s
            elif s == score:
                best.append((kind, ident))
        return best[0] if len(best) == 1 and score > 0 else None

    def cite_name(self, name, entry, prev):
        """A role for *name* in the text of *entry*, or a literal: the first
        mention of an entry links, a later one and the entry's own name do
        not."""
        hit = self.resolve(name, entry, prev)
        if hit is None:
            return literal(name)
        kind, ident = hit
        bang = '!' if ident in entry.mentioned or ident == entry.ident else ''
        entry.mentioned.add(ident)
        if ident == name:
            return f':hpo:{kind}:`{bang}{name}`'
        return f':hpo:{kind}:`{bang}{name} <{ident}>`'

    def abbreviation(self, word, text, start, end):
        """The words that spell out *word* after it, at its first use on
        the page, or ''. A use that its spelled-out form already introduces
        ("... (MLSL)" or "MLSL (...)") counts as the first use."""
        spelled = self.reg.abbreviations.get(word)
        if not spelled or word in self.spelled:
            return ''
        self.spelled.add(word)
        if text[end:end + 2] == ' (' or (text[start - 1:start] == '('
                                         and text[end:end + 1] in ',)'):
            return ''
        return f' ({spelled})'

    def text(self, text, entry):
        """Registry *text* as reStructuredText of *entry* (Entry or a
        scope): names as roles, `code` as literals, abbreviations spelled
        out at their first use on the page, the optimiser list linked."""
        text = ' '.join(text.split())
        pieces, pos, prev = [], 0, ''
        for m in _TOKEN.finditer(text):
            pieces.append((False, text[pos:m.start()]))
            pos = m.end()
            if m.group('code') is not None:
                inner = m.group('code').strip()
                pieces.append((True, self.cite_name(inner, entry, prev)
                               if inner in self.names else literal(inner)))
            elif m.group('opt') is not None:
                opt = m.group('opt')
                pieces.append((True, self.cite_name(opt, entry, prev)
                               if opt in self.names else literal(opt)))
            elif m.group('placeholder') is not None:
                # A placeholder of a message, which a check's doc names.
                pieces.append((True, literal(m.group('placeholder'))))
            elif m.group('list') is not None:
                pieces.append((True, ':ref:`the optimiser list '
                                     '<nfp_ref_optimisers>`'))
            else:
                word = m.group('word')
                after = self.abbreviation(word, text, m.start(), m.end())
                if word in self.names:
                    pieces.append((True, self.cite_name(word, entry, prev)))
                elif '.' in word and re.search(r'[A-Z_]', word):
                    # A key path or an id as a message writes it
                    # (INIT.METHOD, H.DENSITY.CENTRES).
                    pieces.append((True, literal(word)))
                else:
                    pieces.append((False, word))
                if after:
                    pieces.append((False, after))
                prev = word
                continue
            prev = ''
        pieces.append((False, text[pos:]))
        return _join([p for p in pieces if p[1] or p[0]])

    def start_page(self, name):
        """Starts the text of the page *name*: its abbreviations are spelled
        out again."""
        self.page, self.spelled = name, set()

    def entry(self, *args, **kw):
        return Entry(self, *args, **kw)

    # ---- references -----------------------------------------------------
    def cite(self, ident):
        """A role citing the registry entry *ident*."""
        if ident.startswith('BLOCK.'):
            kind = 'block'
        elif ident.startswith('PARAM.'):
            kind = 'param'
        elif ident in TOPIC_IDS or ident.startswith(TOPIC_PREFIXES):
            kind = 'topic'
        else:
            kind = 'key'
        return role(kind, ident)

    def blocks(self, names):
        return ', '.join('top level' if n == 'ROOT' else
                         role('block', 'BLOCK.' + n) for n in names)

    def opt(self, name):
        return role('opt', name)

    # ---- values ---------------------------------------------------------
    def arg_text(self, arg, entry, synopsis=''):
        kind = arg.kind
        lo = number(arg.min) if arg.min is not None else None
        hi = number(arg.max) if arg.max is not None else None
        name = literal('<' + arg.name + '>') + ': '
        if kind in ('integer', 'number'):
            noun = 'an integer' if kind == 'integer' or \
                getattr(arg, 'integral', False) else 'a number'
            text = number_range(noun, lo, hi, getattr(arg, 'min_open', False))
        elif kind == 'positive':
            text = f'a positive {arg.noun or "number"}'
        elif kind == 'word':
            text = 'one of ' + ', '.join(literal(w) for w in arg.words)
            if f'<{arg.name}>' not in ' '.join(synopsis):
                # The syntax writes the words, not a placeholder.
                name, text = '', words(arg.words)
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
            wds = self.reg.lexing.fields['timespec_words']
            text = (', '.join(literal(w) for w in wds)
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
        doc = entry.rst(arg.doc) if arg.doc else ''
        if text:
            out = sentence('; '.join([text] + flags)) + (' ' + doc if doc
                                                          else '')
        else:
            out = doc + (' ' + capital(sentence('; '.join(flags))) if flags
                         else '')
        return name + out

    def values(self, entry, args, synopsis=''):
        synopsis = [synopsis] if isinstance(synopsis, str) else synopsis
        return entry.items('Values', [self.arg_text(a, entry, synopsis)
                                      for a in args])

    @staticmethod
    def default(entry, default):
        if default is None:
            return entry
        if default.literal is not None:
            return entry.line('Default', literal(default.literal))
        return entry.line('Default', entry.rst(default.text))

    def rules(self, entry, ident):
        return entry.items('Rules', [role('topic', r)
                                     for r in self.rules_of.get(ident, [])])

    def long_texts(self, entry, e):
        entry.para(getattr(e, 'long_doc', ''))
        entry.para(getattr(e, 'quirk', ''))
        return entry

    def repeat(self, entry, rule, table, refused, own=''):
        """The Repeat field: the key's own rule when it has one; none for a
        refused key or option. A rule the table has no words for stops the
        generator in any case."""
        words = wording(table, rule, entry.ident, 'repeat rule')
        if refused:
            return entry
        if own:
            return entry.line('Repeat', entry.rst(own))
        return entry.line('Repeat', words)

    # ---- registry entries -----------------------------------------------
    def key_entry(self, k, merge_optim=False):
        entry = self.entry('key', k.id, k.name, names=[k.name],
                           also=('OPTIM',) if merge_optim else (),
                           scope=self.scope_of(k.id))
        entry.para(k.doc)
        self.long_texts(entry, k)
        synopsis = [k.synopsis]
        examples = list(k.examples)
        if merge_optim:
            optim = self.reg.optim
            entry.para(optim.doc)
            entry.para(optim.long_doc)
            synopsis = optim.fields['synopsis']
            examples += optim.examples
        entry.items('Syntax', [literal(s) for s in synopsis])
        self.values(entry, k.args, synopsis)
        self.default(entry, k.default)
        if k.group:
            # The blocks of the group, but a block that refuses the key.
            where = [b for b in self.reg.groups[k.group].blocks
                     if k.name not in {r.key for r in self.reg.block(b).refuse}]
        else:
            where = k.blocks
        entry.line('Where', self.blocks(where))
        self.repeat(entry, k.repeat, KEY_REPEAT, k.refused,
                    getattr(k, 'repeat_doc', ''))
        if k.alias_of:
            entry.line('Alias of', self.cite(k.alias_of))
        if k.refused:
            entry.line('Refused', entry.rst(k.refused))
        if k.id in ('ROOT.PC_LAMMPS', 'ROOT.PC_FAILPROBE') and \
                self.reg.lammps_script is not None:
            entry.line('Script', role('topic', 'LAMMPS_SCRIPT'))
        self.rules(entry, k.id)
        if merge_optim:
            self.rules(entry, 'OPTIM')
        if k.env:
            entry.line('Override', role('key', 'ENV.' + k.env))
        checks = list(k.checks)
        if merge_optim:
            checks += self.reg.optim.fields.get('checks', [])
        entry.checks(checks)
        return entry.examples(examples)

    def option_entry(self, o):
        lines = ', '.join(self.cite_line(n) for n in o.lines)
        entry = self.entry('key', o.id, o.option, names=[o.option],
                           annotation='(' + ', '.join(
                               n.split('.')[-1] for n in o.lines) + ')',
                           scope=self.scope_of(o.id))
        entry.para(o.doc)
        self.long_texts(entry, o)
        entry.line('Syntax', literal(o.synopsis))
        self.values(entry, o.args, o.synopsis)
        self.default(entry, o.default)
        entry.line('Where', 'on ' + lines + (' lines' if len(o.lines) > 1
                                              else ' line'))
        self.repeat(entry, o.repeat, OPTION_REPEAT, o.refused)
        if o.refused:
            entry.line('Refused', entry.rst(o.refused))
        self.rules(entry, o.id)
        entry.checks(o.checks)
        return entry.examples(o.examples)

    def cite_line(self, name):
        """A role citing the line or form *name* of a line option."""
        if name in self.by_id and type(self.by_id[name]).__name__ == 'Form':
            return role('key', name)
        return role('key', 'ROOT.' + name)

    def form_entry(self, f):
        text = f.word or f.id.split('.', 1)[1]
        entry = self.entry('key', f.id, text,
                           names=[f.word] if f.word else [],
                           annotation=f'({f.line})', scope=self.scope_of(f.id))
        entry.para(f.doc)
        self.long_texts(entry, f)
        entry.line('Syntax', literal(f.synopsis))
        self.values(entry, f.args, f.synopsis)
        entry.line('Where', role('key', 'ROOT.' + f.line))
        if f.refused:
            entry.line('Refused', entry.rst(f.refused))
        options = [o for o in self.reg.line_options if f.id in o.lines]
        entry.line('Options', ', '.join(role('key', o.id) for o in options))
        self.rules(entry, f.id)
        entry.checks(f.checks)
        return entry.examples(f.examples)

    def block_entry(self, b):
        entry = self.entry('block', b.id, b.name, names=[b.name],
                           scope=self.scope_of(b.id))
        entry.para(b.doc)
        entry.para(b.long_doc)
        entry.para(b.quirk)
        entry.line('Syntax', f'{literal(b.name)} … {literal(b.end)}')
        entry.line('Where', self.blocks(b.parents))
        entry.line('Repeat', wording(BLOCK_REPEAT, b.repeat, b.id,
                                     'repeat rule'))
        refused = {r.key for r in b.refuse}
        keys = [role('key', k.id) for k in self.reg.keys_in_block(b.name)
                if k.name not in refused]
        entry.line('Keys', ', '.join(keys))
        entry.items('Refused', [f'{literal(r.key)}: {entry.rst(r.why, False)}'
                                for r in b.refuse])
        entry.items('Alias', [f'{literal(w)} is read as {self.cite(i)}'
                              for w, i in b.aliases.items()])
        children = [c.name for c in self.reg.blocks if b.name in c.parents]
        entry.line('Blocks', self.blocks(children))
        self.rules(entry, b.id)
        entry.checks(list(b.requires) + list(b.close_checks))
        return entry.examples(b.examples)

    def section_entry(self, s, text):
        entry = self.entry('topic', s.id, text, scope=self.scope_of(s.id))
        entry.para(s.doc)
        entry.para(s.long_doc)
        return entry.examples(s.examples)

    def group_entry(self, g):
        entry = self.entry('topic', g.id, 'Optimiser keys', scope={g.name})
        entry.para(g.doc)
        entry.para(g.long_doc)
        entry.line('Where', self.blocks(g.blocks))
        entry.line('Keys', ', '.join(role('key', k.id) for k in self.reg.keys
                                     if k.group == g.name))
        return entry.examples(g.examples)

    def lammps_script_entry(self, s):
        """What the LAMMPS script of a PC_LAMMPS line provides, with the
        registry's fixture as the script it shows."""
        entry = self.entry('topic', s.id, 'The LAMMPS script',
                           scope={'PC_LAMMPS', 'PC_FAILPROBE'})
        entry.para(s.doc)
        entry.para(s.long_doc)
        fixture = self.reg.fixtures[s.fields['fixture']]
        caption = (f'`{fixture.path}`: {fixture.doc[:1].lower()}'
                   f'{fixture.doc[1:]}' if fixture.doc else '')
        entry.field('Script', script(fixture.text, caption=entry.rst(caption)
                                     if caption else ''))
        return entry.examples(s.examples)

    def removed_entry(self, r):
        text = (self.reg.structure.fields['removed_msg']
                .replace('{key}', r.name).replace('{hint}', r.hint))
        entry = self.entry('key', r.id, r.name, directive='removed',
                           names=[r.name], options=[('hint', ' '.join(
                               r.hint.split())), ('scope', 'key')])
        entry.para(r.doc)
        return entry.field('Message', message(text))

    def env_entry(self, e):
        entry = self.entry('key', e.id, e.name, names=[e.name],
                           scope=self.scope_of(e.key))
        entry.para(e.doc)
        entry.line('Overrides', self.cite(e.key))
        entry.line('Values', ', '.join(literal(v) for v in e.values))
        entry.checks(e.checks)
        return entry.examples(e.examples)

    def rule_entry(self, r):
        entry = self.entry('topic', r.id, r.title, scope=self.scope_of(r.id))
        entry.para(r.doc)
        entry.para(r.quirk)
        entry.line('Checked', wording(CHECKED, r.check, r.id, 'check'))
        entry.line('Keys', ', '.join(self.cite(i) for i in r.keys))
        if r.msg:
            entry.field('Message', message(r.msg),
                        [entry.rst(r.msg_doc)] if r.msg_doc else [])
        return entry.examples(r.examples)

    def loss_entry(self, f):
        entry = self.entry('key', f.id, f.name, names=[f.name],
                           scope=self.scope_of(f.id))
        entry.para(f.doc)
        syntax = 'LOSS ' + f.name + ''.join(f' [<{p.name}>]' for p in f.params)
        entry.line('Syntax', literal(syntax))
        entry.items('Values', [
            f'{literal("<" + p.name + ">")}: '
            + number_range('a number', number(p.gt) if p.gt is not None
                           else None, lo_open=True)
            + f'; default {literal(number(p.default))}.'
            + (f' {entry.rst(p.doc)}' if p.doc else '')
            for p in f.params])
        return entry.examples(f.examples)

    def strategy_type_entry(self, t):
        entry = self.entry('key', t.id, t.name, names=[t.name],
                           annotation='(STRATEGY TYPE)',
                           scope=self.scope_of(t.id))
        entry.para(t.doc)
        entry.para(t.quirk)
        entry.line('Syntax', literal('TYPE ' + t.name))
        entry.line('Keys', ', '.join(role('key', i) for i in t.keys))
        self.rules(entry, t.id)
        entry.checks(t.checks)
        return entry.examples(t.examples)

    def init_method_entry(self, m):
        entry = self.entry('key', m.id, m.name, names=[m.name],
                           annotation='(INIT METHOD)',
                           scope=self.scope_of(m.id))
        entry.para(m.doc)
        entry.para(m.quirk)
        synopsis = ' '.join(['METHOD', m.name] + [f'<{a.name}>'
                                                  for a in m.args])
        entry.line('Syntax', literal(synopsis))
        self.values(entry, m.args, synopsis)
        if m.refused:
            entry.line('Refused', entry.rst(m.refused))
        entry.checks(m.checks)
        return entry.examples(m.examples)

    def optim_target_entry(self, t):
        entry = self.entry('key', t.id, t.key, annotation='(OPTIM)',
                           scope=self.scope_of(t.id))
        entry.para(t.doc)
        entry.line('Status', 'refused as a search parameter'
                   if t.status == 'refused' else 'read in its own way')
        entry.line('Indices', ', '.join(str(i) for i in t.indices))
        if t.condition:
            entry.line('When', entry.rst(t.condition))
        if t.msg:
            entry.field('Message', message(t.msg))
        return entry.examples(t.examples)

    def param_entry(self, p):
        entry = self.entry('param', p.id, p.name, names=[p.name],
                           scope=self.scope_of(p.id))
        entry.para(p.doc)
        self.long_texts(entry, p)
        values = ' '.join(f'<{a.name}>' for a in p.args) or '<value>'
        syntax = [f'PARAM {p.name} {values}']
        if p.indexed:
            syntax.append(f'PARAM {p.name}[<i>] {values}')
        entry.items('Syntax', [literal(s) for s in syntax])
        self.values(entry, p.args, syntax)
        self.default(entry, p.default)
        if p.optimizers != ['*']:
            entry.line('Where', ', '.join(self.opt(o) for o in p.optimizers))
        else:
            entry.line('Where', self.finite_difference_users())
        entry.checks(p.checks)
        return entry.examples(p.examples)

    def finite_difference_users(self):
        """Where the keys of the finite differences are read: the
        gradient-based optimisers, and an NLopt MLSL or AUGLAG with a
        gradient-based local search."""
        grad = [n for n, o in self.registered.items() if o['gradient_based']]
        local = [n for n, o in self.registered.items()
                 if o['accepts_inner'] and not o['gradient_based']]
        text = 'the gradient-based optimisers, ' + ', '.join(
            self.opt(n) for n in grad)
        if local:
            text += ('; with a gradient-based local search, '
                     + ', '.join(self.opt(n) for n in local))
        return text

    def param_excluded_entry(self, p):
        entry = self.entry('param', p.id, p.name, names=[p.name],
                           scope=self.scope_of(p.id))
        entry.para(p.doc)
        entry.line('Where', ', '.join(self.opt(o) for o in p.optimizers))
        entry.field('Refused', message(p.why))
        return entry.examples(p.examples)

    def param_policy_entry(self, p, examples=True):
        entry = self.entry('param', p.id, f"PARAM ({p.id[len('PARAM.'):]})",
                           scope=self.scope_of(p.id))
        entry.para(p.doc)
        entry.para(p.quirk)
        entry.line('Where', ', '.join(self.opt(o) for o in p.optimizers))
        entry.checks(p.checks)
        return entry.examples(p.examples) if examples else entry

    # ---- the optimiser list -----------------------------------------------
    def optimizer_entry(self, o):
        name = f"{o['lib']}:{o['algo']}"
        entry = self.entry('opt', name, name, names=[o['algo']],
                           scope=self.scope_of(name) | {o['lib']})
        if o['status'] == 'refused':
            entry.line('Status', 'refused')
            return entry.line('Reason', entry.rst(o['reason']))
        doc = (self.reg.optimizer_docs.optimizers.get(name)
               if self.reg.optimizer_docs else None)
        if not doc:
            raise GenerationError(f'the optimiser {name} has no description '
                                  'in the optimiser descriptions '
                                  '(meta.optimizer_docs)')
        entry.para(doc)
        entry.line('Status', 'available')
        entry.line('Search', entry.rst(o['row_text'], False))
        stops = ', '.join(role('key', 'OPTIMIZER.' + s) for s in o['stops'])
        notes = []
        if o['tolerance_note']:
            notes.append(entry.rst(capital(o['tolerance_note'])))
        if o['untested_tolerance_note']:
            # A tolerance that is neither a stop nor read as a setting is
            # refused; the note says why, of one tolerance ("it").
            refused = [t for t in TOLERANCES if t not in o['stops']
                       and t not in o['reads']]
            note = o['untested_tolerance_note']
            if refused:
                keys = [role('key', 'OPTIMIZER.' + t) for t in refused]
                head = (keys[0] + ' is refused' if len(keys) == 1 else
                        'Each of ' + ', '.join(keys[:-1]) + ' and '
                        + keys[-1] + ' is refused')
                notes.append(head + ': ' + entry.rst(note))
            else:
                notes.append(entry.rst(capital(note)))
        if notes:
            stops = ' '.join(([sentence(stops)] if stops else []) + notes)
        entry.line('Stop keys', stops)
        need = 'yes' if o['needs_stop'] else 'no'
        if o['stop_note']:
            need += ': ' + entry.rst(o['stop_note'], False)
        need = sentence(need)
        if o['ignores_forced_stop']:
            need += ' Only ' + role('key', 'OPTIMIZER.MAXEVAL') + \
                ' stops the search.'
        entry.line('Needs a stop', need)
        never = o.get('random_draws') == 'never'
        reads = [r for r in o['reads'] if not (never and r == 'SEED')]
        text = ', '.join(role('key', 'OPTIMIZER.' + r) for r in reads)
        if o['warns_unread_seed']:
            text = (text + '; ' if text else '') + \
                'a ' + role('key', '!OPTIMIZER.SEED') + ' line gets a warning'
        entry.line('Reads', text)
        # How the optimiser uses STEP, as the ALGO help says it; when it
        # refuses STEP, the reason its message gives (the message reads
        # "<LIB>:<ALGO> does not use STEP; <reason>").
        if o['step_consumed']:
            entry.line('STEP', entry.rst(capital(o['step_note']
                                                 or o['step_use_text'])))
        elif o['step_note']:
            entry.field('STEP', ['refused, with the reason:'],
                        message(o['step_note']))
        else:
            entry.line('STEP', 'refused.')
        entry.line('Starts at', 'the point the search is given' if
                   o['starts_at_x0'] else entry.rst(o['start_note']))
        draws = RANDOM.get(o.get('random_draws'), '')
        if never and 'SEED' in o['reads']:
            draws += ': a ' + role('key', '!OPTIMIZER.SEED') + \
                ' line is accepted and has no effect'
        entry.line('Random draws', sentence(draws) if draws else '')
        entry.line('Bounds', entry.rst(o['bounds_note']) if
                   o['ignores_bounds'] else entry.rst('inside the OPTIM box',
                                                      False))
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
        """The PARAM entries an optimiser reads: its policy, its keys and,
        for one that takes finite-difference gradients, theirs."""
        lib = name.split(':')[0]
        o = self.registered.get(name, {})
        out = [p.id for p in self.reg.param_policies
               if name in p.optimizers or lib in p.optimizers]
        out += [p.id for p in self.reg.params
                if (name in p.optimizers or lib in p.optimizers)
                and not p.id.startswith('PARAM.ASA:')]
        if o.get('gradient_based') or o.get('accepts_inner'):
            out += [p.id for p in self.reg.params if p.optimizers == ['*']]
        return out

    # ---- the annealers --------------------------------------------------
    def anneal_values(self, k, entry):
        """The values of an annealer key, in the words of the other pages."""
        tokens = [t for t, f in (('AUTO', 'accepts_auto'),
                                 ('OFF', 'accepts_off')) if f in k]
        if k['type'] == 'enum':
            return 'one of ' + ', '.join(literal(v) for v in k['values']) + '.'
        if k['type'] == 'bool':
            return words(['ON', 'OFF']) + '.'
        if k.get('custom_range'):
            if not k.get('range_doc'):
                raise GenerationError(f"anneal_keys.toml: {k['name']} has a "
                                      'custom range and no range_doc')
            return entry.rst(k['range_doc'])
        noun = 'an integer' if k['type'] == 'count' else 'a number'
        lo = rak._num(k['min']) if 'min' in k else (
            '0' if k['type'] == 'count' else None)
        hi = rak._num(k['max']) if 'max' in k else None
        text = number_range(noun, lo, hi, bool(k.get('min_open')),
                            bool(k.get('max_open')))
        if tokens:
            text += ', or ' + words(tokens)
        return text + '.'

    def anneal_entries(self, policy):
        keys = self.anneal.get('key', [])
        rules = self.anneal.get('rule', [])
        renamed = self.anneal.get('renamed', [])
        live = {k['name'] for k in keys}
        # The example of a key: an example of the annealers' PARAM policy
        # whose last line sets the key (the lines before it set what the
        # key needs).
        examples = {}
        for c in policy.examples:
            last = c.text.strip().split('\n')[-1].split()
            if len(last) > 1 and last[0] == 'PARAM':
                examples.setdefault(last[1], c)
        by_group = {}
        for k in keys:
            rejected = {r['kind'] for r in rules if r['type'] == 'kind_reject'
                        and r['key'] == k['name']}
            kinds = [kd for kd in rak.KINDS if kd in k['kinds']
                     and kd not in rejected]
            ident = 'PARAM.TADAH:ANNEAL.' + k['name']
            entry = self.entry('param', ident, k['name'], names=[k['name']],
                               scope={'PARAM', 'TADAH:ANNEAL', 'OPTIMIZER'})
            entry.para(k['doc'])
            entry.para(k.get('long_doc', ''))
            if k['type'] == 'enum':
                value = ' | '.join(k['values'])
            elif k['type'] == 'bool':
                value = 'ON | OFF'
            else:
                value = '<value>'
            entry.line('Syntax', literal(f"PARAM {k['name']} {value}"))
            entry.line('Values', self.anneal_values(k, entry))
            defaults = [(kd, rak.default_for(k, kd)) for kd in kinds]
            if len({d for _kd, d in defaults}) == 1:
                entry.line('Default', defaults[0][1])
            else:
                entry.field('Default', [f'- {ANNEALERS[kd]}: {d}'
                                        for kd, d in defaults])
            entry.line('Where', ', '.join(self.opt(ANNEALERS[kd])
                                          for kd in kinds))
            said, refusals = [], []
            for kd in kinds:
                kind_keys = {x['name'] for x in keys if kd in x['kinds']}
                for s in rak.constraint_sentences(k['name'], rules,
                                                  kind_keys):
                    if s not in said:
                        said.append(s)
                for r in rules:
                    ref = r.get('flag') or r.get('other') or r.get('enum_key')
                    if (r['key'] == k['name'] and r['type'] != 'kind_reject'
                            and (ref is None or ref in kind_keys)):
                        refusals.append(annealer_message(kd, r['msg']))
            # The annealers that refuse the key, each with its message.
            refusals += [annealer_message(kd, r['msg']) for kd in rak.KINDS
                         for r in rules if r['type'] == 'kind_reject'
                         and r['key'] == k['name'] and r['kind'] == kd]
            refusals += [annealer_message(kd, r['msg']) for r in renamed
                         if r['old'] == k['name'] for kd in rak.KINDS
                         if kd in r['kinds']]
            # The sentences of read_anneal_keys.py, their keys as roles.
            entry.line('Rules', ' '.join(entry.rst(s.replace('``', '`'))
                                         for s in said))
            entry.field('Refused', *[message(m) for m in refusals])
            if k['name'] not in examples:
                raise GenerationError(
                    f"the annealer key {k['name']} has no example among the "
                    'examples of the PARAM policy PARAM.TADAH:ANNEAL')
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
            entry = self.entry('param', 'PARAM.TADAH:ANNEAL.' + r['old'],
                               r['old'], directive='removed',
                               names=[r['old']],
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
        check_fields(reg)
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

        # The rules: each on the page of the first entry it refers to that
        # is not a block (the first of its keys the pages hold, in the order
        # of the pages), written in the page's Rules section.
        home = {}
        for r in reg.rules:
            home[r.id] = next((self.page_of(i) for i in r.keys
                               if not i.startswith('BLOCK.')
                               and self.page_of(i)), 'target_keys')

        def rules_section(page):
            page.section('Rules', self.place(page, [
                self.rule_entry(r) for r in reg.rules
                if home[r.id] == page.name]))

        # target_keys ----------------------------------------------------
        self.start_page('target_keys')
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
        rules_section(target)

        # optimizer_keys -------------------------------------------------
        self.start_page('optimizer_keys')
        opt = Page('optimizer_keys', 'nfp_ref_optimizer_keys',
                   'OPTIMIZER Block Keys',
                   'This page lists the keys of the blocks inside '
                   f"{role('block', 'BLOCK.OPTIMIZER')}: the keys of an "
                   'optimiser, valid in each block that runs one, and the '
                   f"keys of the {role('block', 'BLOCK.STRATEGY')}, "
                   f"{role('block', 'BLOCK.INIT')}, "
                   f"{role('block', 'BLOCK.NOISE')} and "
                   f"{role('block', 'BLOCK.RESTART')} blocks, with the "
                   f"types of {role('key', 'STRATEGY.TYPE')} and the "
                   f"methods of {role('key', 'INIT.METHOD')}. The blocks are on "
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
        rules_section(opt)

        # pc_options -----------------------------------------------------
        self.start_page('pc_options')
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
        # The script lines first, then what a script provides, then the
        # settings of the scripts.
        scripts = sorted((k for k in reg.keys if lines_key(k)
                          and k not in metrics
                          and k.name not in ('PC_OBSERVABLE', 'LOSS',
                                             'OPTIM')),
                         key=lambda k: k.name not in ('PC_LAMMPS',
                                                      'PC_FAILPROBE'))
        heads = [k for k in scripts if k.name in ('PC_LAMMPS', 'PC_FAILPROBE')]
        script_items = with_options(heads)
        if reg.lammps_script is not None:
            script_items.append(self.lammps_script_entry(reg.lammps_script))
        script_items += with_options([k for k in scripts if k not in heads])
        pc.section('LAMMPS scripts', self.place(pc, script_items))
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
        rules_section(pc)

        # optimisers -----------------------------------------------------
        self.start_page('optimisers')
        meta = self.optimizers.get('meta', {})
        optimisers = Page(
            'optimisers', 'nfp_ref_optimisers', 'Optimisers',
            'This page lists every optimiser an '
            f"{role('block', 'BLOCK.OPTIMIZER')} block can name with "
            f"{role('key', 'OPTIMIZER.LIB')} and "
            f"{role('key', 'OPTIMIZER.ALGO')}, as ``tadah explain ALGO`` "
            'lists them, with what each is, what it reads and how it '
            f"searches; the {role('opt', 'NLOPT')} algorithms are those "
            f"of NLopt {meta.get('nlopt_version')}. A refused optimiser has "
            'the reason and the optimisers that replace it. The '
            f"{role('key', 'OPTIMIZER.PARAM')} keys of the annealers and "
            f"of {role('opt', 'ASA:ASA')} are on "
            ':ref:`nfp_ref_anneal_params` and :ref:`nfp_ref_asa_params`.')
        listed = self.optimizers.get('optimizer', [])
        docs = reg.optimizer_docs
        for lib in meta.get('libraries', []):
            items = []
            libdoc = docs.libraries.get(lib) if docs else None
            if not libdoc:
                raise GenerationError(f'the library {lib} has no description '
                                      'in the optimiser descriptions '
                                      '(meta.optimizer_docs)')
            items.append([self.text(sentence(libdoc), self.entry(
                'opt', lib, lib, scope={lib, 'OPTIMIZER'})), ''])
            items += [self.optimizer_entry(o) for o in listed
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
        rules_section(optimisers)

        # anneal_params --------------------------------------------------
        self.start_page('anneal_params')
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
        # Each key shows its own example; the policy shows none.
        anneal.section('The annealers', self.place(anneal, [
            self.param_policy_entry(policy, examples=False)]))
        groups, retired = self.anneal_entries(policy)
        for title, entries in groups:
            anneal.section(title, entries)
        anneal.section('Retired names', retired)
        rules_section(anneal)

        # asa_params -----------------------------------------------------
        self.start_page('asa_params')
        asa = Page('asa_params', 'nfp_ref_asa_params', 'ASA PARAM Keys',
                   'This page lists the '
                   f"{role('key', 'OPTIMIZER.PARAM')} keys of "
                   f"{role('opt', 'ASA:ASA')}, Adaptive Simulated "
                   'Annealing (ASA): the options of ASA under '
                   'their ASA names, and the options of ASA that '
                   f"{role('opt', '!ASA:ASA')} does not take.")
        self.spelled.add('ASA')
        asa.section('Options', self.place(asa, [
            self.param_policy_entry(self.by_id['PARAM.ASA:ASA'])] + [
            self.param_entry(p) for p in reg.params
            if p.id.startswith('PARAM.ASA:')]))
        asa.section('Options not taken', self.place(asa, [
            self.param_excluded_entry(p) for p in reg.param_excluded]))
        rules_section(asa)

        missing = [i for i in self.by_id if i not in self.placed]
        if missing:
            raise GenerationError('registry entries without a place on the '
                                  'reference pages: ' + ', '.join(missing))
        return [target, opt, pc, optimisers, anneal, asa]

    def page_of(self, ident):
        """The page that holds the entry *ident* (the rules come after
        every other entry, so this is known by the time they are placed),
        else None."""
        return self.placed.get(ident) or self.home_of(ident)

    def home_of(self, ident):
        """The page an entry that is not placed yet goes to, by its kind."""
        e = self.by_id.get(ident)
        if e is None:
            return None
        name = type(e).__name__
        if name in ('Param', 'ParamExcluded', 'ParamPolicy'):
            if ident.startswith('PARAM.ASA:'):
                return 'asa_params'
            if ident.startswith('PARAM.TADAH:ANNEAL'):
                return 'anneal_params'
            return 'optimisers'
        if name in ('LineOption', 'Form', 'LossFunction', 'OptimTarget') or \
                ident in ('ROOT.PC_LAMMPS', 'ROOT.PC_FAILPROBE',
                          'ROOT.PC_OBSERVABLE', 'ROOT.LOSS', 'ROOT.OPTIM',
                          'LAMMPS_SCRIPT') or \
                ident.split('.')[-1].startswith('PC_') or ident == 'OPTIM':
            return 'pc_options'
        if name in ('StrategyType', 'InitMethod') or (
                name == 'Key' and (e.group or e.scope in
                                   ('STRATEGY', 'INIT', 'NOISE', 'RESTART'))):
            return 'optimizer_keys'
        if name == 'Key' and e.id == 'OPTIMIZER.LOG_HP':
            return 'optimizer_keys'
        return 'target_keys'


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
