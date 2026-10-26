"""Tests for docs/source/config/read_hpo_registry.py (the reference pages).

The cases on the pages read HPO's registries from the HPO checkout the
documentation build uses (source_locator.py): $TADAH_HPO_SOURCE_DIR, the
HPO checkout next to this repository or $TADAH_PATH/HPO. Without one, or
with one that has no target-file registry, they are skipped, saying why;
a $TADAH_HPO_SOURCE_DIR that names no such checkout is a failure.

Run: python3 -m unittest discover -s docs/tools/tests -v
"""
import importlib
import importlib.util
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from fractions import Fraction

HERE = os.path.dirname(os.path.abspath(__file__))
TOOLS = os.path.normpath(os.path.join(HERE, '..'))
SOURCE = os.path.normpath(os.path.join(TOOLS, '..', 'source'))
CONFIG = os.path.join(SOURCE, 'config')
SCRIPT = os.path.join(CONFIG, 'read_hpo_registry.py')

HAVE_TOMLLIB = importlib.util.find_spec('tomllib') is not None
HAVE_DOCUTILS = importlib.util.find_spec('docutils') is not None
HAVE_SPHINX = all(importlib.util.find_spec(m) for m in
                  ('sphinx', 'sphinx_rtd_theme'))
HPO_VAR = 'TADAH_HPO_SOURCE_DIR'
LOADER = os.path.join('blueprint', 'hpotarget_registry.py')
PAGES = ('target_keys', 'optimizer_keys', 'pc_options', 'optimisers',
         'anneal_params', 'asa_params')

# Imports the script with tomllib blocked, as on Python 3.10 and older.
BLOCKED_IMPORT = (
    "import sys\n"
    "sys.modules['tomllib'] = None\n"
    "sys.path.insert(0, {config!r})\n"
    "import read_hpo_registry\n")


def generator():
    sys.path.insert(0, CONFIG)
    try:
        return importlib.import_module('read_hpo_registry')
    finally:
        sys.path.remove(CONFIG)


def hpo_checkout():
    """The HPO checkout with the target-file registry, or None and why."""
    sys.path.insert(0, CONFIG)
    try:
        import source_locator
    finally:
        sys.path.remove(CONFIG)
    named = HPO_VAR in os.environ
    try:
        root = source_locator.locate('HPO')
    except source_locator.SourceNotFound as err:
        if named:
            raise AssertionError(f'{HPO_VAR} names no HPO checkout: {err}')
        return None, f'no HPO checkout: {err}'
    if not os.path.isfile(os.path.join(root, LOADER)):
        if named:
            raise AssertionError(f'{HPO_VAR}={os.environ[HPO_VAR]} has no '
                                 f'{LOADER}')
        return None, f'the HPO checkout {root} has no target-file registry ' \
                     f'({LOADER})'
    return root, None


def run_script(env, *args, cwd=None, bytecode=False):
    """Runs the generator; without *bytecode* no bytecode is written (with
    it, the generator alone keeps it out of the HPO checkout)."""
    env = dict(env)
    if bytecode:
        env.pop('PYTHONDONTWRITEBYTECODE', None)
    else:
        env['PYTHONDONTWRITEBYTECODE'] = '1'
    return subprocess.run([sys.executable, SCRIPT] + list(args), cwd=cwd,
                          env=env, capture_output=True, text=True, timeout=120)


def targets(text):
    """The ids a page declares: entries, their second ids and targets."""
    ids = re.findall(r'^\.\. hpo:(?:key|block|opt|param|topic|removed|'
                     r'target):: (\S+)$', text, re.M)
    for also in re.findall(r'^   :also: (.+)$', text, re.M):
        ids += also.split()
    return ids


def code_blocks(text):
    """The non-blank lines of each code block of a page, without the indent
    of the block and without the options of the directive (:caption:)."""
    def indent(line):
        return len(line) - len(line.lstrip())
    blocks, lines, i = [], text.split('\n'), 0
    while i < len(lines):
        m = re.match(r'^(\s*)\.\. code-block::', lines[i])
        i += 1
        if not m:
            continue
        while i < len(lines) and re.match(r'^\s+:\w+:', lines[i]):
            i += 1
        outer, content = len(m.group(1)), []
        while i < len(lines) and (not lines[i].strip()
                                  or indent(lines[i]) > outer):
            if lines[i].strip():
                content.append(lines[i])
            i += 1
        inner = min(indent(l) for l in content)
        blocks.append([l[inner:] for l in content])
    return blocks


def blocks_of(text, language):
    """The text of each code block of *language* of a page, its lines
    joined by blanks."""
    out, lines, i = [], text.split('\n'), 0
    while i < len(lines):
        m = re.match(r'^(\s*)\.\. code-block:: (\S+)$', lines[i])
        i += 1
        if not m:
            continue
        outer, content = len(m.group(1)), []
        while i < len(lines) and (not lines[i].strip()
                                  or len(lines[i]) - len(lines[i].lstrip())
                                  > outer):
            content.append(lines[i].strip())
            i += 1
        if m.group(2) == language:
            out.append(' '.join(' '.join(content).split()))
    return out


def prose_of(text):
    """A page without its code blocks and the options of its entries (such
    as :hint:), its markup removed and its blanks collapsed: the text a
    reader sees outside the code blocks."""
    out, lines, i = [], text.split('\n'), 0
    while i < len(lines):
        m = re.match(r'^(\s*)\.\. code-block::', lines[i])
        if not m:
            if not re.match(r'^\s+:(names|text|annotation|also|hint|scope|'
                            r'kind):', lines[i]):
                out.append(lines[i])
            i += 1
            continue
        i += 1
        while i < len(lines) and (not lines[i].strip()
                                  or len(lines[i]) - len(lines[i].lstrip())
                                  > len(m.group(1))):
            i += 1
    text = re.sub(r'\\(.)', r'\1', ' '.join(out).replace('``', ''))
    return ' '.join(text.split())


def field_bodies(text, name):
    """The body of each field *name* of a page, its lines joined."""
    out, lines = [], text.split('\n')
    for i, line in enumerate(lines):
        if line.strip() != f':{name}:':
            continue
        outer, body = len(line) - len(line.lstrip()), []
        for nxt in lines[i + 1:]:
            if nxt.strip() and len(nxt) - len(nxt.lstrip()) <= outer:
                break
            body.append(nxt.strip())
        out.append('\n'.join(body).strip())
    return out


#: The name each annealer starts its messages with (anneal_core.h).
ANNEALER = {'serial': 'TADAH:ANNEAL', 'pt': 'TADAH:ANNEAL_PT',
            'pa': 'TADAH:ANNEAL_PA'}

_ENTRY = re.compile(r'^\.\. hpo:(\w+):: (\S+)\n((?:(?:   .*)?\n)*)', re.M)


def entries_of(text):
    """{id: (directive, body)} of the entries of a page; the body is the
    entry's indented lines, the indent of three removed."""
    return {m.group(2): (m.group(1), '\n'.join(
        l[3:] for l in m.group(3).split('\n'))) for m in _ENTRY.finditer(text)}


def field_of(body, name):
    """The body of the field *name* of an entry, its lines joined by
    newlines, or None."""
    found = field_bodies(body, name)
    return found[0] if found else None


def paragraphs_of(body):
    """The paragraphs of an entry body before its field list and after its
    options, each with its lines joined."""
    out, current = [], []
    for line in body.split('\n'):
        if re.match(r'^:[\w ]+:(\s|$)', line):
            if out or current:
                break
            continue
        if not line.strip():
            if current:
                out.append(' '.join(current))
                current = []
            continue
        current.append(line.strip())
    if current:
        out.append(' '.join(current))
    return out


def plain_prose(text):
    """The prose of a page as prose_of gives it, without its roles, its
    references, its directive lines and its section titles: the words a
    reader sees outside links and literals."""
    lines, out = text.split('\n'), []
    for i, line in enumerate(lines):
        if line.startswith('.. '):
            continue
        if i + 1 < len(lines) and re.match(r'^[=\-~]{3,}$', lines[i + 1]):
            continue
        out.append(line)
    text = re.sub(r'``[^`]*``', ' ', '\n'.join(out))
    text = re.sub(r':(?:hpo:\w+|ref|term):`[^`]*`', ' ', text)
    return prose_of(text)


def words_of(rst):
    """The words a reader sees of a piece of reStructuredText: a role shows
    its title, or its target without a title."""
    rst = re.sub(r':[\w:]+:`!?([^`<]*?)\s*<[^>]*>`', r'\1', rst)
    rst = re.sub(r':[\w:]+:`!?([^`]*)`', r'\1', rst)
    rst = re.sub(r'\\(.)', r'\1', rst.replace('``', ''))
    return ' '.join(rst.split())


def field_items(body, name):
    """The top-level items of the field *name* of an entry body: each code
    block or paragraph, its lines as they are, or None without the
    field."""
    lines = body.split('\n')
    for i, line in enumerate(lines):
        if line.strip() != f':{name}:':
            continue
        outer = len(line) - len(line.lstrip())
        rest = []
        for nxt in lines[i + 1:]:
            if nxt.strip() and len(nxt) - len(nxt.lstrip()) <= outer:
                break
            rest.append(nxt)
        inner = min(len(l) - len(l.lstrip()) for l in rest if l.strip())
        items, current = [], []
        for l in rest:
            if l.strip() and len(l) - len(l.lstrip()) == inner and current \
                    and not current[-1].strip():
                items.append('\n'.join(current).strip())
                current = []
            current.append(l)
        if current:
            items.append('\n'.join(current).strip())
        return items
    return None


def number_admits(phrase, value):
    """Whether the Values phrase of a number admits *value*: an integer or
    a number, with its range (from a to b, >= a, <= b, above a, below b,
    and the two-sided forms "above a, up to b" and "from a, below b"), or
    the one value a range of one value is."""
    if re.fullmatch(r'-?[0-9][0-9.e+-]*', phrase):
        return Fraction(value) == Fraction(phrase)
    m = re.match(r'(an integer|a number|a positive(?: [a-z]+)* number)\b(.*)',
                 phrase)
    if not m:
        return True
    noun, rest = m.group(1), m.group(2)
    value = Fraction(value)
    if noun == 'an integer' and value.denominator != 1:
        return False
    if noun.startswith('a positive') and value <= 0:
        return False
    num = r'(-?[0-9][0-9.e+-]*)'
    checks = [(r'from ' + num + r' to ' + num, lambda a, b: a <= value <= b),
              (r'above ' + num + r', up to ' + num,
               lambda a, b: a < value <= b),
              (r'from ' + num + r', below ' + num,
               lambda a, b: a <= value < b),
              (r'above ' + num + r' and below ' + num,
               lambda a, b: a < value < b),
              (r'>= ' + num, lambda a: value >= a),
              (r'<= ' + num, lambda a: value <= a),
              (r'above ' + num, lambda a: value > a),
              (r'below ' + num, lambda a: value < a)]
    for pattern, test in checks:
        mm = re.match(r'\s*' + pattern, rest)
        if mm:
            return test(*(Fraction(g) for g in mm.groups()))
    return True


def logical_lines(text):
    """The logical lines of a target-file fragment as the parser reads them
    (blueprint/hpotarget.toml, [lexing]): a comment runs from # to the end
    of the line, trailing blanks are removed and a line ending with a
    backslash continues on the next one, joined without a separator."""
    out, pending = [], ''
    for line in text.split('\n'):
        line = line.split('#', 1)[0].rstrip()
        if line.endswith('\\'):
            pending += line[:-1]
            continue
        out.append(pending + line)
        pending = ''
    if pending:
        out.append(pending)
    return [l.split() for l in out if l.strip()]


class GeneratorPythonTest(unittest.TestCase):
    def test_python_without_tomllib_stops_with_a_clear_message(self):
        # Pins: without tomllib (Python 3.10 and older) the script stops as
        # it is imported, saying what it needs and which Python it is; no
        # traceback, nothing written.
        # Before the fix: the script does not exist.
        with tempfile.TemporaryDirectory() as tmp:
            r = subprocess.run(
                [sys.executable, '-c', BLOCKED_IMPORT.format(config=CONFIG)],
                cwd=tmp, capture_output=True, text=True,
                env=dict(os.environ, PYTHONDONTWRITEBYTECODE='1'))
            self.assertNotEqual(r.returncode, 0, r.stdout + r.stderr)
            self.assertIn('read_hpo_registry.py needs Python 3.11 or later: '
                          "it reads HPO's blueprint/hpotarget.toml with "
                          'tomllib, which Python %d.%d does not have'
                          % sys.version_info[:2], r.stderr)
            self.assertIn(sys.executable, r.stderr)
            self.assertNotIn('Traceback', r.stderr)
            self.assertEqual(os.listdir(tmp), [])


@unittest.skipIf(not HAVE_TOMLLIB,
                 'tomllib needs Python 3.11 or later, as the docs do')
class MissingHpoTest(unittest.TestCase):
    """The generator stops, naming what it looked for, as
    read_anneal_keys.py does."""

    def run_in(self, hpo, bytecode=False):
        with tempfile.TemporaryDirectory() as tmp:
            out = os.path.join(tmp, 'out')
            r = run_script(dict(os.environ, **{HPO_VAR: hpo}), '--out', out,
                           bytecode=bytecode)
            self.assertNotEqual(r.returncode, 0, r.stdout + r.stderr)
            self.assertNotIn('Traceback', r.stderr)
            self.assertFalse(os.path.exists(out))
            return r.stderr

    def test_no_hpo_checkout(self):
        with tempfile.TemporaryDirectory() as hpo:
            err = self.run_in(hpo)
        self.assertIn(f'read_hpo_registry.py: {HPO_VAR}={hpo} does not contain '
                      'blueprint/anneal_keys.toml', err)

    def test_an_hpo_checkout_without_the_registry(self):
        with tempfile.TemporaryDirectory() as hpo:
            os.makedirs(os.path.join(hpo, 'blueprint'))
            with open(os.path.join(hpo, 'blueprint', 'anneal_keys.toml'),
                      'w') as fh:
                fh.write('# fixture\n')
            err = self.run_in(hpo)
        self.assertIn(f'read_hpo_registry.py: HPO checkout {hpo} has no '
                      f'{LOADER}', err)

    def test_a_loader_of_another_api(self):
        with tempfile.TemporaryDirectory() as hpo:
            os.makedirs(os.path.join(hpo, 'blueprint'))
            for name, text in (('anneal_keys.toml', '# fixture\n'),
                               ('hpotarget_registry.py', 'API = 2\n')):
                with open(os.path.join(hpo, 'blueprint', name), 'w') as fh:
                    fh.write(text)
            err = self.run_in(hpo, bytecode=True)
            self.assertFalse(os.path.isdir(os.path.join(hpo, 'blueprint',
                                                        '__pycache__')))
        self.assertIn(f'read_hpo_registry.py: {hpo}/{LOADER} has API 2; this '
                      'generator reads API 1', err)


@unittest.skipIf(not HAVE_TOMLLIB,
                 'tomllib needs Python 3.11 or later, as the docs do')
class TextTest(unittest.TestCase):
    def test_a_long_line_is_continued_as_the_target_file_allows(self):
        rhr = generator()
        line = ('PC_OBSERVABLE cohesive_energy --species H --target 2.0 '
                '--weight 1 --hotvar a0 1.5 2.5 --unconverged force-scaled 10')
        wrapped = rhr.wrap_line(line, 40)
        self.assertGreater(len(wrapped), 1)
        self.assertTrue(all(len(l) <= 40 for l in wrapped), wrapped)
        self.assertTrue(all(l.endswith(' \\') for l in wrapped[:-1]))
        self.assertEqual(logical_lines('\n'.join(wrapped)), [line.split()])
        self.assertEqual(rhr.wrap_line('MAXEVAL 200', 40), ['MAXEVAL 200'])
        with self.assertRaisesRegex(rhr.GenerationError, 'with a comment'):
            rhr.wrap_line(line + ' # why', 40)

    def test_a_range_of_one_value_is_that_value(self):
        # Pins: a range whose minimum is its maximum reads as that value
        # (ASA's User_Tangents takes 0 only); the other ranges keep their
        # words.
        # Before the fix: "an integer from 0 to 0".
        rhr = generator()
        self.assertEqual(rhr.number_range('an integer', '0', '0'), '0')
        self.assertEqual(rhr.number_range('an integer', '0', '1'),
                         'an integer from 0 to 1')

    @unittest.skipIf(not HAVE_DOCUTILS, 'docutils is required')
    def test_markup_characters_are_kept_as_text(self):
        rhr = generator()
        from docutils.core import publish_doctree
        for text, literal in (
                ('d = max(p|x0|, a) and [x0/f, x0*f]', []),
                ('Writes <directory>/pot_<i>.tadah, a name_ alone.', []),
                ('- a leading dash, then --hotvar and (--maxtime).',
                 ['--hotvar', '--maxtime']),
                ('Set as LAMMPS `timer timeout` after every clear.',
                 ['timer timeout']),
                ('A line that ends with \\ continues.', []),
                ('1. a leading number', [])):
            with self.subTest(text=text):
                tree = publish_doctree(rhr.prose(text), settings_overrides={
                    'report_level': 1, 'halt_level': 5})
                from docutils import nodes
                messages = list(tree.findall(nodes.system_message))
                self.assertEqual(messages, [])
                self.assertEqual(tree.astext(), text.replace('`', ''))
                self.assertEqual([n.astext() for n in tree.findall(
                    nodes.literal)], literal)


@unittest.skipIf(not HAVE_TOMLLIB,
                 'tomllib needs Python 3.11 or later, as the docs do')
class PagesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.hpo, reason = hpo_checkout()
        if cls.hpo is None:
            raise unittest.SkipTest(reason)
        cls.rhr = generator()
        cls.reg, cls.optimizers, cls.anneal = cls.rhr.read_sources(cls.hpo)
        cls.pages = cls.rhr.render(cls.reg, cls.optimizers, cls.anneal)

    def test_the_six_pages(self):
        self.assertEqual(sorted(self.pages), sorted(PAGES))

    def test_every_registry_id_has_one_entry(self):
        # Pins: each id of the target-file registry is the target of one
        # entry (so that a role finds it), as many as the loader has; the
        # other targets are the optimisers and libraries of the optimiser
        # list and the annealers' PARAM keys, each once.
        found = [i for name in PAGES for i in targets(self.pages[name])]
        registry = list(self.reg.by_id())
        self.assertEqual(len(found), len(set(found)),
                         sorted(i for i in found if found.count(i) > 1))
        self.assertEqual(len([i for i in found if i in set(registry)]),
                         len(registry))
        self.assertEqual(set(registry) - set(found), set())
        listed = self.optimizers['optimizer']
        others = {f"{o['lib']}:{o['algo']}" for o in listed}
        others |= set(self.optimizers['meta']['libraries'])
        keys = {k['name'] for k in self.anneal['key']}
        others |= {'PARAM.TADAH:ANNEAL.' + k for k in keys}
        others |= {'PARAM.TADAH:ANNEAL.' + r['old']
                   for r in self.anneal.get('renamed', [])
                   if r['old'] not in keys}
        self.assertEqual(set(found) - set(registry), others)

    def test_every_removed_name_is_declared_with_its_hint(self):
        removed = {r.name: r.hint for r in self.reg.removed}
        text = self.pages['target_keys']
        for name, hint in removed.items():
            with self.subTest(name=name):
                self.assertIn(f'.. hpo:removed:: REMOVED.{name}\n'
                              f'   :names: {name}\n', text)
                self.assertIn(f'   :hint: {" ".join(hint.split())}\n', text)

    def test_code_lines_are_at_most_80_characters(self):
        widest = 0
        for name in PAGES:
            for block in code_blocks(self.pages[name]):
                for line in block:
                    widest = max(widest, len(line.expandtabs(8)))
                    self.assertLessEqual(len(line.expandtabs(8)),
                                         self.rhr.CODE_WIDTH, (name, line))
        self.assertLessEqual(self.rhr.CODE_WIDTH, 80)
        self.assertGreater(widest, 40)   # the blocks were found

    def test_the_pages_hold_no_table(self):
        # A table has at most two columns on these pages; they hold none.
        for name in PAGES:
            text = self.pages[name]
            with self.subTest(page=name):
                self.assertNotRegex(text, r'(?m)^\s*\.\. (list-table|'
                                          r'csv-table|table)::')
                self.assertNotRegex(text, r'(?m)^\s*\+[-=+]+\+\s*$')
                self.assertNotRegex(text, r'(?m)^\s*=+( +=+)+\s*$')

    def test_messages_are_quoted_as_program_output(self):
        # Pins: a message of tadah hpo is a block of program output, as the
        # program prints it, and not prose of the page (its wording, such
        # as "remove STEP", is the program's): the refusals of an
        # annealer's PARAM key, each with the name of the annealer that
        # prints it and that annealer's own message, and the reason an
        # optimiser gives when it refuses STEP.
        # Before the fix: the annealers' messages are inline literals of
        # prose without the annealer's name, and a STEP refusal is prose.
        rules = self.anneal.get('rule', [])
        expected = [f"{ANNEALER[r['kind']]}: {r['msg']}" for r in rules
                    if r['type'] == 'kind_reject']
        expected += [f"{ANNEALER[kind]}: {r['msg']}"
                     for r in self.anneal.get('renamed', [])
                     for kind in r['kinds']]
        notes = [o['step_note'] for o in self.optimizers['optimizer']
                 if o['status'] != 'refused' and not o['step_consumed']
                 and o['step_note']]
        self.assertTrue(expected and notes)
        for page, messages in (('anneal_params', expected),
                               ('optimisers', notes)):
            blocks = blocks_of(self.pages[page], 'text')
            prose = prose_of(self.pages[page])
            for msg in messages:
                with self.subTest(page=page, message=msg[:60]):
                    msg = ' '.join(msg.split())
                    self.assertTrue(any(msg in b for b in blocks))
                    self.assertNotIn(msg.split(': ', 1)[-1][:50], prose)

    def test_the_values_read_as_sentences(self):
        # Pins: the description of an argument (``<name>``: ...) is a
        # fragment and the registry's text a sentence after it; each ends
        # with a full stop; an argument of any word is "a word" (a file, a
        # variable, a value), not "a name". (The annealers' values are a
        # range, as the annealing tables write it.)
        # Before the fix: "a number; The tolerance, ..." and "<value>: a
        # name".
        count = 0
        for name in PAGES:
            for body in field_bodies(self.pages[name], 'Values'):
                items = [i[2:] if i.startswith('- ') else i
                         for i in body.split('\n- ')]
                for item in [i for i in items if i.startswith('``<')]:
                    count += 1
                    with self.subTest(page=name, item=item[:60]):
                        item = ' '.join(item.split())
                        self.assertNotRegex(item, r';\s+(The|A|An)\b')
                        self.assertTrue(item.endswith('.'), item)
        self.assertGreater(count, 100)
        invar = self.pages['pc_options'].split(
            '.. hpo:key:: PC_LAMMPS.--invar\n', 1)[1].split('\n.. hpo:', 1)[0]
        self.assertIn('``<value>``: a word.', invar)

    def test_examples_read_as_the_registry_writes_them(self):
        # Pins: an example longer than a code line is continued with a
        # backslash; the parser reads the same tokens from it.
        wrapped = 0
        for case in self.reg.cases():
            if case.kind != 'example':
                continue
            block = self.rhr.code(case.text)[2:]
            lines = [l[3:] for l in block]
            wrapped += len(lines) > len(case.text.strip('\n').split('\n'))
            self.assertEqual(logical_lines('\n'.join(lines)),
                             logical_lines(case.text), case.text)
        self.assertGreater(wrapped, 0)

    @unittest.skipIf(not HAVE_DOCUTILS, 'docutils is required')
    def test_the_registry_text_is_kept_as_written(self):
        from docutils import nodes
        from docutils.core import publish_doctree
        texts = []
        for e in self.reg.entries():
            for f in ('doc', 'long_doc', 'quirk', 'condition', 'refused'):
                texts.append(getattr(e, f, '') or '')
            texts += [a.doc for a in getattr(e, 'args', [])]
            texts += [c.doc for c in e.examples]
            default = getattr(e, 'default', None)
            texts.append(default.text if default and default.text else '')
        texts += [k['doc'] for k in self.anneal['key']]
        for text in filter(None, texts):
            with self.subTest(text=text[:60]):
                tree = publish_doctree(self.rhr.prose(text), settings_overrides={
                    'report_level': 1, 'halt_level': 5})
                self.assertEqual(list(tree.findall(nodes.system_message)), [])
                self.assertEqual(tree.astext(),
                                 ' '.join(text.split()).replace('`', ''))

    def test_two_runs_write_the_same_files(self):
        with tempfile.TemporaryDirectory() as tmp:
            outs = []
            for seed in ('1', '2'):
                out = os.path.join(tmp, seed)
                r = run_script(dict(os.environ, **{HPO_VAR: self.hpo,
                                                   'PYTHONHASHSEED': seed}),
                               '--out', out)
                self.assertEqual(r.returncode, 0, r.stderr)
                outs.append(out)
            for name in PAGES:
                with open(os.path.join(outs[0], name + '.rst'), 'rb') as a, \
                        open(os.path.join(outs[1], name + '.rst'), 'rb') as b:
                    self.assertEqual(a.read(), b.read(), name)
                with open(os.path.join(outs[0], name + '.rst'),
                          encoding='utf-8') as fh:
                    self.assertEqual(fh.read(), self.pages[name], name)

    def test_a_registry_the_loader_refuses_stops_the_generator(self):
        with tempfile.TemporaryDirectory() as tmp:
            hpo = os.path.join(tmp, 'HPO')
            shutil.copytree(os.path.join(self.hpo, 'blueprint'),
                            os.path.join(hpo, 'blueprint'),
                            ignore=shutil.ignore_patterns('__pycache__'))
            main = os.path.join(hpo, 'blueprint', 'hpotarget.toml')
            with open(main, encoding='utf-8') as fh:
                text = fh.read()
            with open(main, 'w', encoding='utf-8') as fh:
                fh.write(text.replace('[meta]\n', '[meta]\nflavour = 1\n', 1))
            out = os.path.join(tmp, 'out')
            r = run_script(dict(os.environ, **{HPO_VAR: hpo}), '--out', out)
        self.assertNotEqual(r.returncode, 0)
        self.assertIn("read_hpo_registry.py: the registry in "
                      f"{hpo}/blueprint does not pass HPO's loader:", r.stderr)
        self.assertIn("unknown field 'flavour'", r.stderr)
        self.assertNotIn('Traceback', r.stderr)

    @unittest.skipUnless(HAVE_SPHINX, 'Sphinx and sphinx_rtd_theme are required')
    def test_the_pages_build_without_a_warning(self):
        # Pins: with the hpo domain and the layout rules of the chapter, the
        # hub and the six pages build with -W: every role of the pages
        # finds its entry, no code line is longer than 80 characters, no
        # table is wider than two columns, no inline literal is too long.
        status, warnings, _html = build_pages(self.pages)
        self.assertEqual(warnings, [])
        self.assertEqual(status, 0)

    # ---- the entries ------------------------------------------------------
    def page_entries(self):
        out = {}
        for name in PAGES:
            for ident, (kind, body) in entries_of(self.pages[name]).items():
                out[ident] = (name, kind, body)
        return out

    def test_every_validation_metric_is_under_validation_metrics(self):
        # Pins: every top-level key whose registry doc says it is a loss
        # term on a metric of the validation datasets ("A loss term on the
        # ... of the validation datasets") has its entry in the section
        # "Validation metrics" of pc_options, and the section holds no
        # other key's entry; PC_SRMSE_GPA, the stress metric in GPa, is one
        # of them.
        # Before the fix: the generator listed the metrics by name, with
        # PC_SRMSE, so PC_SRMSE_GPA, which replaces it, was placed under
        # "LAMMPS scripts".
        text = self.pages['pc_options']
        heads = [(m.start(), m.group(1)) for m in re.finditer(
            r'^([^\n]+)\n-{3,}$', text, re.M)]
        names = [h for _, h in heads]
        self.assertIn('Validation metrics', names)
        start = heads[names.index('Validation metrics')][0]
        end = heads[names.index('Validation metrics') + 1][0]
        section = text[start:end]
        declared = set(re.findall(r'^\.\. hpo:key:: ROOT\.(\S+)$', section,
                                  re.M))
        metrics = {k.name for k in self.reg.keys
                   if k.blocks == ['ROOT'] and not k.alias_of
                   and re.match(r'A loss term on the .* of the validation '
                                r'datasets', ' '.join(k.doc.split()))}
        self.assertIn('PC_SRMSE_GPA', metrics)
        self.assertEqual(declared, metrics)

    def test_where_leaves_out_a_block_that_refuses_the_key(self):
        # Pins: the Where field of a key of the optimiser group cites each
        # block of the group but one that refuses the key (an INNER block
        # refuses SEED, STOPVAL and GRAD_STRATEGY).
        # Before the fix: it cited BLOCK.INNER for those three.
        entries = self.page_entries()
        refused = [(b, r.key) for b in self.reg.blocks for r in b.refuse]
        self.assertTrue(refused)
        for block, name in refused:
            key = next(k for k in self.reg.keys
                       if k.group and k.name == name)
            with self.subTest(key=key.id, block=block.name):
                where = field_of(entries[key.id][2], 'Where')
                self.assertNotIn(f'BLOCK.{block.name}`', where)
                for other in self.reg.groups[key.group].blocks:
                    if other != block.name:
                        self.assertIn(f'BLOCK.{other}`', where)

    def test_rule_entries_show_their_title(self):
        # Pins: a rule's entry is a topic, headed by the rule's title, and
        # a field that names rules cites them with :hpo:topic:, so that the
        # page shows the titles, in words, and no rule id.
        # Before the fix: the heading and the links showed the id after
        # RULE. (inner_block_required) as code.
        entries = self.page_entries()
        for rule in self.reg.rules:
            with self.subTest(rule=rule.id):
                _page, kind, body = entries[rule.id]
                self.assertEqual(kind, 'topic')
                self.assertIn(f':text: {rule.title}\n', body)
        for name in PAGES:
            text = self.pages[name]
            self.assertNotRegex(text, r':hpo:key:`RULE\.')
            prose = plain_prose(text)
            for rule in self.reg.rules:
                self.assertNotIn(rule.id[len('RULE.'):], prose, name)

    def test_entry_text_holds_no_placeholder(self):
        # Pins: no paragraph or field outside a quoted message holds a
        # {name} placeholder of a message (a rule says what its message
        # holds in words, after the message).
        # Before the fix: rule paragraphs read "{list} names those ...".
        for name in PAGES:
            with self.subTest(page=name):
                self.assertNotRegex(prose_of(self.pages[name]),
                                    r'(?<!\$)\{[A-Za-z_]+\}')

    def test_no_shorthand_or_registry_field_in_the_text(self):
        # Pins: the text of the pages names no field of the registry's
        # schema (positive_only) and has no developer shorthand or class of
        # a library (per-dim, rhobeg, an OUTER STEP, GradientProblemSolver).
        # Before the fix: OPTIM AUTO read "the keys of positive_only", and
        # the STEP and bounds notes of the optimisers had the shorthand.
        loader = self.rhr.load_loader(os.path.join(self.hpo, 'blueprint'))
        fields = {f for table in loader.SCHEMA.values() for f in table
                  if '_' in f.strip('_')}
        self.assertIn('positive_only', fields)
        shorthand = re.compile(r'per-dim\b|\bdims?\b|rhobeg|rhoend|'
                               r'\bOUTER\b|GradientProblemSolver|'
                               r'(?<!Ceres )\bSolver\b|-grad\b|'
                               r'DIAGNOSTICS counts')
        for name in PAGES:
            prose = prose_of(self.pages[name])
            with self.subTest(page=name):
                self.assertEqual([f for f in sorted(fields)
                                  if re.search(r'\b' + f + r'\b', prose)],
                                 [])
                self.assertIsNone(shorthand.search(prose))

    def test_the_lammps_script_entry(self):
        # Pins: pc_options holds the entry LAMMPS_SCRIPT, what the script of
        # a PC_LAMMPS line provides, with the fixture the registry names as
        # its script, and the entries of PC_LAMMPS and PC_FAILPROBE cite it.
        # Before the fix: no page said how a script takes the candidate
        # potential (pair_coeff * * EXTERN).
        entries = self.page_entries()
        page, kind, body = entries['LAMMPS_SCRIPT']
        self.assertEqual((page, kind), ('pc_options', 'topic'))
        self.assertIn('EXTERN', body)
        fixture = self.reg.fixtures[
            self.reg.lammps_script.fields['fixture']].text
        self.assertIn([l.strip() for l in fixture.strip().split('\n')],
                      code_blocks(body))
        for key in ('ROOT.PC_LAMMPS', 'ROOT.PC_FAILPROBE'):
            self.assertIn(':hpo:topic:`LAMMPS_SCRIPT`',
                          field_of(entries[key][2], 'Script') or '', key)

    def test_seed_of_an_optimiser_that_draws_no_random_numbers(self):
        # Pins: an optimiser that draws no random numbers does not list SEED
        # among what it reads, and its entry says SEED has no effect; the
        # SEED entry says which optimisers refuse SEED, without the claim
        # that such an optimiser refuses it.
        # Before the fix: DIRECT, StoGO and AGS showed "Reads: SEED" and
        # "Random draws: never", and SEED said they refuse it.
        entries = self.page_entries()
        never = [f"{o['lib']}:{o['algo']}"
                 for o in self.optimizers['optimizer']
                 if o['status'] == 'registered'
                 and o.get('random_draws') == 'never']
        self.assertTrue(never)
        for name in never:
            body = entries[name][2]
            with self.subTest(optimiser=name):
                self.assertNotIn('OPTIMIZER.SEED', field_of(body, 'Reads')
                                 or '')
                self.assertIn('SEED', field_of(body, 'Random draws'))
                self.assertIn('no effect', field_of(body, 'Random draws'))
        seed = ' '.join(paragraphs_of(entries['OPTIMIZER.SEED'][2]))
        self.assertNotIn('draws no random numbers is refused', seed)

    def test_an_nlopt_entry_names_the_parameters_its_algorithm_reads(self):
        # Pins: the PARAM field of an NLopt algorithm's entry names the
        # PARAM keys NLopt reads for it (algorithm_params of the optimiser
        # list), as literals: tolg for LD_LBFGS, magic_eps for GN_DIRECT_L;
        # an algorithm that reads none says so (LN_SBPLX). Another
        # library's entry adds nothing of the kind.
        # Before the fix: the field named the NLopt PARAM policy alone, and
        # the generator stopped on the optimiser list of the refusal of
        # unread PARAM keys and SEED ("KeyError: 'warns_unread_seed'").
        entries = self.page_entries()
        param = {name: field_of(entries[name][2], 'PARAM') or ''
                 for name in ('NLOPT:LD_LBFGS', 'NLOPT:GN_DIRECT_L',
                              'NLOPT:LN_SBPLX', 'DLIB:BFGS')}
        self.assertIn('NLopt reads ``tolg``', param['NLOPT:LD_LBFGS'])
        self.assertIn('NLopt reads ``magic_eps``', param['NLOPT:GN_DIRECT_L'])
        self.assertIn('NLopt reads no parameter of this algorithm',
                      param['NLOPT:LN_SBPLX'])
        self.assertNotIn('NLopt reads', param['DLIB:BFGS'])

    def test_values_admit_no_refused_value(self):
        # Pins: the Values field of an argument does not admit a number
        # that an invalid case of the entry refuses for its range or for a
        # fraction: an exclusive minimum is "above", a whole number "an
        # integer". Refusals of one optimiser (a group key's message that
        # names it) and those of the context are not ranges of the key.
        # Before the fix: nine ASA options read "a number >= 0" and refused
        # 0; GRID_POINTS and the integer ASA options took fractions.
        entries = self.page_entries()
        refusal = re.compile(r'must be (an integer|a positive|>=? ?-?\d|'
                             r'<=? ?-?\d)|requires PARAM \S+ > 0|'
                             r'to be an integer|must be a finite ratio >=')
        checked = 0
        for e in self.reg.entries():
            args = [a for a in getattr(e, 'args', [])
                    if a.kind in ('integer', 'number', 'positive')]
            if len(args) != 1 or e.id not in entries:
                continue
            values = field_of(entries[e.id][2], 'Values') or ''
            m = re.search(r'``<' + re.escape(args[0].name) + r'>``: '
                          r'(.*?)(?:;|\.(?:\s|$))', values, re.S)
            if not m:
                continue
            for case in e.invalid:
                toks = case.text.split()
                if toks and toks[0] == 'PARAM':
                    toks = toks[1:]
                if (len(toks) != 2 or not refusal.search(case.error)
                        or not re.fullmatch(r'-?\d+(\.\d+)?', toks[1])):
                    continue
                if getattr(e, 'group', '') and re.search(
                        r'[A-Z]+:[A-Z_0-9]+', case.error):
                    continue
                checked += 1
                with self.subTest(entry=e.id, text=case.text):
                    self.assertFalse(number_admits(m.group(1), toks[1]),
                                     m.group(1))
        self.assertGreater(checked, 30)

    def test_annealer_entries(self):
        # Pins: each annealer key has an example of the registry; its
        # values read as those of the other pages (no bare "float", "> 0",
        # "(0, 1]" or "|"); each rule of the key reads as a refusal and its
        # message is quoted, with the annealer's name, as program output.
        # Before the fix: 40 of 44 had no example, the values were
        # shorthand, and "Unused when T_INIT is set" hid a refusal.
        text = self.pages['anneal_params']
        entries = entries_of(text)
        rules = self.anneal['rule']
        for k in self.anneal['key']:
            body = entries['PARAM.TADAH:ANNEAL.' + k['name']][1]
            with self.subTest(key=k['name']):
                self.assertIsNotNone(field_of(body, 'Example'))
                values = field_of(body, 'Values')
                self.assertNotRegex(values, r'^float|^integer|^[(\[<>]|\|')
                for r in rules:
                    if r['key'] != k['name']:
                        continue
                    kinds = [kd for kd in ANNEALER if kd in k['kinds']]
                    self.assertTrue(any(
                        f"{ANNEALER[kd]}: {r['msg']}" in b
                        for b in blocks_of(body, 'text') for kd in kinds),
                        r['msg'])
                self.assertNotIn('Unused', field_of(body, 'Rules') or '')

    def test_optimiser_entries_describe_the_algorithm(self):
        # Pins: the entry of each available optimiser, and the section of
        # each library, starts with the description of the registry's
        # optimiser descriptions; an abbreviation of the registry's table is
        # spelled out at its first use on each page.
        # Before the fix: the entries had capabilities only, and MLSL,
        # AUGLAG, StoGO, AGS or DIRECT were never spelled out.
        docs = self.reg.optimizer_docs
        entries = entries_of(self.pages['optimisers'])
        spelled = [f' ({s})' for s in self.reg.abbreviations.values()]
        for name, doc in docs.optimizers.items():
            with self.subTest(optimiser=name):
                first = words_of(paragraphs_of(entries[name][1])[0])
                for s in spelled:
                    first, doc = first.replace(s, ''), doc.replace(s, '')
                self.assertEqual(first.split()[:4], doc.split()[:4])
        for lib, doc in docs.libraries.items():
            m = re.search(r'\n' + lib + r'\n-+\n\n(.+?)\n\n',
                          self.pages['optimisers'], re.S)
            self.assertEqual(words_of(m.group(1)).split()[:4],
                             doc.split()[:4], lib)
        for name in PAGES:
            prose = re.sub(r'\s+', ' ', plain_prose(self.pages[name]))
            for abbr, spelled in self.reg.abbreviations.items():
                m = re.search(r'(?<![\w-])' + re.escape(abbr) + r'(?![\w-])',
                              prose)
                if not m:
                    continue
                around = prose[max(0, m.start() - len(spelled) - 3):
                               m.end() + len(spelled) + 3].lower()
                with self.subTest(page=name, abbreviation=abbr):
                    self.assertIn(spelled.lower()[:12], around)

    def test_examples_are_captioned_code(self):
        # Pins: in an Example field each code block after the first has its
        # caption (the registry's doc), and no paragraph stands between two
        # blocks; an example run with an environment variable says so in
        # its caption.
        # Before the fix: a doc was a paragraph before its block, so it read
        # as the caption of the block above it, and the TADAH_OPTIM_SCALE
        # example showed "OPTIM_SCALE AUTO" alone.
        for name in PAGES:
            for ident, (_kind, body) in entries_of(self.pages[name]).items():
                parts = field_items(body, 'Example')
                if not parts:
                    continue
                with self.subTest(entry=ident):
                    self.assertTrue(all(p.startswith('.. code-block::')
                                        for p in parts), parts)
                    for b in parts[1:]:
                        self.assertIn(':caption:', b)
        env = entries_of(self.pages['target_keys'])['ENV.TADAH_OPTIM_SCALE']
        self.assertIn('TADAH_OPTIM_SCALE=none',
                      ' '.join(field_items(env[1], 'Example')))

    def test_a_registry_field_the_pages_do_not_account_for_stops_them(self):
        # Pins: the generator accounts for every field of every object of
        # the registry (an entry, an example, a check, an argument, ...) and
        # of every section: the registry passes, the msg_of of its invalid
        # lines included, and a field it does not list stops it, naming the
        # class (or section) and the field, wherever the object is held; so
        # does an object of a class it does not know.
        # Before the fix: only the fields of an example were checked, and
        # msg_of stopped the generator.
        import copy
        import dataclasses
        self.assertTrue(any(c.msg_of for c in self.reg.cases()))
        self.rhr.check_fields(self.reg)
        error = self.rhr.GenerationError

        def wider(obj, name):
            cls = type(obj)
            new = dataclasses.make_dataclass(
                cls.__name__, [(name, str, dataclasses.field(default='x'))],
                bases=(cls,))
            out = copy.copy(obj)
            out.__class__ = new
            return out
        case = wider(next(iter(self.reg.cases())), 'files')
        key = copy.copy(self.reg.keys[0])
        key.examples = [case]
        with self.assertRaisesRegex(error, r'\bCase\.files\b'):
            self.rhr.check_fields(key)
        with self.assertRaisesRegex(error, r'\bKey\.flavour\b'):
            self.rhr.check_fields(wider(self.reg.keys[0], 'flavour'))
        check = next(c for k in self.reg.keys for c in k.checks)
        with self.assertRaisesRegex(error, r'\bCheck\.when\b'):
            self.rhr.check_fields(wider(check, 'when'))
        lexing = copy.copy(self.reg.lexing)
        lexing.fields = dict(lexing.fields, flavour='x')
        with self.assertRaisesRegex(error, r'\bLEXING\.flavour\b'):
            self.rhr.check_fields(lexing)
        widget = dataclasses.make_dataclass('Widget', [('name', str)])
        with self.assertRaisesRegex(error, r'\bWidget\.name\b'):
            self.rhr.check_fields(widget('x'))
        # The generator itself stops on such a registry.
        reg = copy.copy(self.reg)
        reg.keys = [key] + list(self.reg.keys[1:])
        with self.assertRaisesRegex(error, r'\bCase\.files\b'):
            self.rhr.render(reg, self.optimizers, self.anneal)

    def test_an_unknown_value_of_a_rule_field_stops_the_generator(self):
        # Pins: a repeat rule or a check stage the generator has no words
        # for stops it with the entry and the value, not a KeyError.
        # Before the fix: a KeyError and a traceback.
        import copy
        reg = copy.copy(self.reg)
        block = copy.copy(self.reg.blocks[0])
        block.repeat = 'twice'
        reg.blocks = [block] + list(self.reg.blocks[1:])
        with self.assertRaisesRegex(self.rhr.GenerationError,
                                    rf"{block.id}: .*'twice'"):
            self.rhr.render(reg, self.optimizers, self.anneal)
        reg = copy.copy(self.reg)
        key = copy.copy(self.reg.keys[0])
        key.repeat = 'twice'
        reg.keys = [key] + list(self.reg.keys[1:])
        with self.assertRaisesRegex(self.rhr.GenerationError,
                                    rf"{re.escape(key.id)}: .*'twice'"):
            self.rhr.render(reg, self.optimizers, self.anneal)

    def test_the_long_docs_of_sections_blocks_and_groups(self):
        # Pins: the long_doc of each section ([lexing], [reading],
        # [structure], [optim], [lammps_script]), of each block and of the
        # optimiser group is a paragraph of its entry, as the registry
        # writes it ([optim]'s in the entry of OPTIM).
        # Before the fix: none of them was shown; the LAMMPS script entry
        # lost its long_doc when the registry took it out of the section's
        # fields.
        entries = self.page_entries()
        spelled = [f' ({s})' for s in self.reg.abbreviations.values()]
        sections = [self.reg.lexing, self.reg.reading, self.reg.structure,
                    self.reg.optim, self.reg.lammps_script]
        items = [s for s in sections if s is not None] + list(
            self.reg.blocks) + list(self.reg.groups.values())
        checked = 0
        for e in items:
            if not e.long_doc:
                continue
            checked += 1
            ident = 'ROOT.OPTIM' if e.id == 'OPTIM' else e.id
            with self.subTest(entry=e.id):
                text = ' '.join(words_of(p) for p in
                                paragraphs_of(entries[ident][2]))
                for s in spelled:
                    text = text.replace(s, '')
                self.assertIn(' '.join(e.long_doc.replace('`', '').split()),
                              text)
        self.assertGreaterEqual(checked, 12)

    def test_checks_are_quoted_with_when_they_come(self):
        # Pins: each check of an entry (its checks; a block's requires and
        # close_checks; the checks of [optim], in the entry of OPTIM; the
        # checks of an environment variable) is a code block of program
        # output in the entry's Checks field, its caption the check's doc,
        # in which a placeholder of the message is an inline literal.
        # Before the fix: no check was shown, among them the limit of
        # TADAH_PC_WORKER_TIMEOUT and every check of [optim].
        entries = self.page_entries()
        spelled = [f' ({s})' for s in self.reg.abbreviations.values()]
        checked = 0
        for e in self.reg.entries():
            name = type(e).__name__
            if name == 'Section':
                checks = list(e.fields.get('checks', []))
            elif name == 'Block':
                checks = list(e.requires) + list(e.close_checks)
            else:
                checks = list(getattr(e, 'checks', []))
            if not checks:
                continue
            ident = 'ROOT.OPTIM' if e.id == 'OPTIM' else e.id
            items = field_items(entries[ident][2], 'Checks') or []
            shown = []
            for item in items:
                m = re.match(r'\.\. code-block:: text\n\s+:caption: (.*)\n'
                             r'\n((?:.*\n?)*)$', item)
                self.assertIsNotNone(m, (ident, item[:80]))
                shown.append((m.group(1), ' '.join(m.group(2).split())))
            for c in checks:
                checked += 1
                with self.subTest(entry=ident, message=c.msg[:60]):
                    msg = ' '.join(c.msg.split())
                    captions = [cap for cap, text in shown if text == msg]
                    self.assertTrue(captions, msg)
                    doc = ' '.join(c.doc.replace('`', '').split())
                    found = []
                    for cap in captions:
                        words = words_of(cap)
                        for s in spelled:
                            words = words.replace(s, '')
                        found.append(words)
                        self.assertIsNone(re.search(
                            r'(?<!\$)\{\w+\}', re.sub(r'``[^`]*``', '', cap)),
                            cap)
                    self.assertIn(doc, found)
        self.assertGreater(checked, 300)
        env = self.reg.by_id()['ENV.TADAH_PC_WORKER_TIMEOUT']
        self.assertTrue(env.checks)
        body = entries['ENV.TADAH_PC_WORKER_TIMEOUT'][2]
        self.assertIn('9.2e15', ' '.join(field_items(body, 'Checks')))

    def test_a_block_given_once_per_optimiser(self):
        # Pins: the Repeat field of a block the parser takes once for each
        # optimiser (INNER: the STRATEGY and REFINE blocks share one, and so
        # do two EXPLORE blocks) says so.
        # Before the fix: "Once in each enclosing block", which an INNER
        # block in REFINE after one in STRATEGY contradicts.
        entries = self.page_entries()
        blocks = [b for b in self.reg.blocks
                  if b.repeat == 'error_per_parent']
        self.assertTrue(blocks)
        for b in blocks:
            with self.subTest(block=b.id):
                repeat = field_of(entries[b.id][2], 'Repeat')
                self.assertIn('once per optimiser', repeat.lower())
                self.assertNotIn('each enclosing block', repeat)

    #: For each field of [lexing] and [structure] that the pages leave out
    #: because the text of the section states it (SECTION_FIELDS), what the
    #: text of its topic says.
    STATED = {
        ('LEXING', 'comment'):
            r'everything from # to the end of a line is a comment',
        ('LEXING', 'continuation'):
            r'A line that ends with \\ continues on the next line',
        ('LEXING', 'key_case'): r'its key, matched as written',
        ('LEXING', 'error_frame'): r'Each error names the line it is about',
        ('LEXING', 'unfinished_msg'):
            r'A file whose last line ends with \\ is refused',
        ('STRUCTURE', 'end_without_open_msg'):
            r'An END line without an open block',
        ('STRUCTURE', 'end_mismatch_msg'):
            r'an END line closes the innermost open block',
        ('STRUCTURE', 'end_takes_no_value_msg'):
            r'A line that opens or closes a block holds its key alone',
        ('STRUCTURE', 'open_takes_no_value_msg'):
            r'A line that opens or closes a block holds its key alone',
        ('STRUCTURE', 'unclosed_msg'):
            r'every block is closed before the file ends',
        ('STRUCTURE', 'unknown_root_msg'):
            r'An unknown key is refused, at the top level and in every '
            r'block',
    }

    def test_fields_left_out_are_stated_in_the_text(self):
        # Pins: each field of [lexing] and [structure] that the pages leave
        # out because the doc, the long_doc or the text of the section
        # states it is stated by the text of its topic, and a field given
        # that reason has a phrase here. Guard: the keys of [optim]'s
        # positive_only and its auto_excluded_prefix are not shown as
        # fields; the entry of OPTIM AUTO states both in its text.
        # Before the fix: no page said that a file whose last line ends
        # with a backslash is refused ('unfinished line N'), nor that an
        # END line without an open block or an unknown key is refused,
        # and the reason of each said the text states it.
        claimed = {(section, name)
                   for section, fields in self.rhr.SECTION_FIELDS.items()
                   for name, why in fields.items()
                   if re.search(r'the (doc|long_doc|text of the section) '
                                r'states it', why)}
        self.assertEqual(claimed, set(self.STATED))
        entries = self.page_entries()
        for (section, name), phrase in sorted(self.STATED.items()):
            with self.subTest(field=f'{section}.{name}'):
                body = ' '.join(words_of(p) for p in
                                paragraphs_of(entries[section][2]))
                self.assertRegex(body, phrase)
        optim = self.reg.optim.fields
        body = ' '.join(words_of(p) for p in paragraphs_of(
            self.page_entries()['OPTIM.AUTO'][2]))
        for name in optim.get('positive_only', []):
            self.assertRegex(body, r'\b' + re.escape(name) + r'\b')
        prefix = optim.get('auto_excluded_prefix')
        if prefix:
            self.assertIn(f'starts with {prefix}', body)

    def test_values_placeholders_are_in_the_syntax(self):
        # Pins: each <name> the Values field describes is in the Syntax of
        # the entry (an argument of words only lists its words); a refused
        # entry has no Repeat field; STEP's Repeat is its own rule.
        # Before the fix: 22 entries named a placeholder Syntax does not
        # show (NOISE AUTO ``<on>``), and --ncpu had a Repeat field.
        checked = 0
        for name in PAGES:
            for ident, (_kind, body) in entries_of(self.pages[name]).items():
                values = field_of(body, 'Values')
                syntax = field_of(body, 'Syntax')
                if not values or not syntax:
                    continue
                for ph in re.findall(r'``(<[^>]+>)``:', values):
                    checked += 1
                    with self.subTest(entry=ident, placeholder=ph):
                        self.assertIn(ph, syntax)
                if field_of(body, 'Refused') is not None:
                    self.assertIsNone(field_of(body, 'Repeat'), ident)
        self.assertGreater(checked, 100)
        step = entries_of(self.pages['optimizer_keys'])['OPTIMIZER.STEP'][1]
        self.assertIn('adds an override', field_of(step, 'Repeat'))

    def test_the_hub_and_the_finite_difference_keys(self):
        # Pins: the hub promises no field an entry lacks; the keys of the
        # finite differences say where they are read, the gradient-based
        # optimisers, and those cite them in their PARAM field.
        # Before the fix: "Each entry gives the syntax, the values, the
        # default, where the key may appear and an example", and fd_step
        # had no Where.
        with open(os.path.join(SOURCE, 'nfp', 'reference', 'index.rst'),
                  encoding='utf-8') as fh:
            hub = ' '.join(fh.read().split())
        self.assertNotIn('Each entry gives the syntax', hub)
        entries = entries_of(self.pages['optimisers'])
        gradient = [f"{o['lib']}:{o['algo']}"
                    for o in self.optimizers['optimizer']
                    if o['status'] == 'registered' and o['gradient_based']]
        self.assertTrue(gradient)
        for p in self.reg.params:
            if p.optimizers != ['*']:
                continue
            where = field_of(entries[p.id][1], 'Where') or ''
            for name in gradient:
                with self.subTest(param=p.id, optimiser=name):
                    self.assertIn(f'`{name}`', where)
                    self.assertIn(f'`{p.id}`',
                                  field_of(entries[name][1], 'PARAM'))

    def test_key_names_in_the_text_are_roles(self):
        # Pins: a name of one entry (a key, block, option, PARAM key,
        # optimiser LIB:ALGO, ...) in the text of the pages is a role, a
        # link at its first mention in an entry; "the optimiser list" links
        # to the optimisers page.
        # Before the fix: FAILSCORE's text named FAILSCORE_GRADING and
        # PC_LAMMPS as plain text, and "the optimiser list" linked nowhere.
        names = {}
        for name in PAGES:
            for m in re.finditer(r'^\.\. hpo:(\w+):: (\S+)\n((?:   :\w+:.*\n)*)',
                                 self.pages[name], re.M):
                if m.group(1) == 'removed':
                    continue
                nm = re.search(r'   :names: (.+)\n', m.group(3))
                for n in (nm.group(1).split() if nm else []):
                    names.setdefault(n, set()).add(m.group(2))
        unique = {n for n, ids in names.items() if len(ids) == 1
                  and (n.startswith('-') or '_' in n or n.isupper())
                  and not any(':' in i and i.split(':')[1] == n
                              and '_' not in n for i in ids)}
        self.assertIn('FAILSCORE_GRADING', unique)
        for name in PAGES:
            prose = plain_prose(self.pages[name])
            bare = sorted({n for n in unique if re.search(
                r'(?<![\w.:-])' + re.escape(n) + r'(?![\w:-])', prose)})
            with self.subTest(page=name):
                self.assertEqual(bare, [])
        keys = self.pages['optimizer_keys']
        self.assertIn(':ref:`the optimiser list <nfp_ref_optimisers>`', keys)
        self.assertNotIn('the optimiser list', plain_prose(keys))

    @unittest.skipUnless(HAVE_SPHINX, 'Sphinx and sphinx_rtd_theme are required')
    def test_bare_names_resolve_to_the_generated_entries(self):
        # Pins: a role that cites a bare name of the real registry links to
        # the entry of that name: a key, a block, an algorithm, a library,
        # a PARAM key of NLopt's grid, of an annealer and of ASA, an option,
        # a STRATEGY type that is also a removed keyword (the live entry
        # wins), and a rule by its id, shown as its title. A name of
        # several entries is one warning that lists them, and a removed
        # name one warning with its hint.
        # Before the fix: removing the :names: of every entry still passed
        # every test; PRESTAGE warned, listing REMOVED.PRESTAGE as an id
        # that resolves it.
        rule = self.reg.rules[0]
        cite = {
            ':hpo:key:`MAXEVAL`': 'OPTIMIZER.MAXEVAL',
            ':hpo:block:`STRATEGY`': 'BLOCK.STRATEGY',
            ':hpo:opt:`LN_SBPLX`': 'NLOPT:LN_SBPLX',
            ':hpo:opt:`NLOPT`': 'NLOPT',
            ':hpo:param:`GRID_POINTS`': 'PARAM.TADAH:GRID.GRID_POINTS',
            ':hpo:param:`T_HOT`': 'PARAM.TADAH:ANNEAL.T_HOT',
            ':hpo:param:`Temperature_Ratio_Scale`':
                'PARAM.ASA:ASA.Temperature_Ratio_Scale',
            ':hpo:key:`--skip-above`': 'PC_EMAE.--skip-above',
            ':hpo:key:`PRESTAGE`': 'STRATEGY.TYPE.PRESTAGE',
            f':hpo:topic:`{rule.id}`': rule.id,
        }
        warned = [':hpo:key:`SEED`', ':hpo:key:`--hotvar`',
                  ':hpo:key:`INIT_K`', ':hpo:param:`L`']
        guide = 'Guide\n=====\n\n' + '\n\n'.join(
            f'Line {r}.' for r in list(cite) + warned) + '\n'
        status, warnings, html = build_pages(
            self.pages, {'nfp/guide.rst': guide}, toctree=['nfp/guide'])
        self.assertNotEqual(status, 0)
        page = html['nfp/guide']
        found = re.findall(r'<a class="reference internal" href="'
                           r'reference/\w+\.html#([^"]+)"', page)
        self.assertEqual(found, list(cite.values()))
        self.assertIn(f'>{rule.title}<', page)
        self.assertEqual(len(warnings), 4, warnings)
        live = ['INIT.SEED', 'OPTIMIZER.SEED', 'RHOBARGATE.SEED',
                'RHOGATE.SEED']
        self.assertIn("hpo:key: 'SEED' names 4 entries: " + ', '.join(live),
                      warnings[0])
        self.assertIn("hpo:key: '--hotvar' names 2 entries", warnings[1])
        self.assertIn("hpo:key: 'INIT_K' was removed from the target file",
                      warnings[2])
        self.assertIn("hpo:param: 'L' was removed from the target file",
                      warnings[3])


def build_pages(pages, extra=None, toctree=()):
    """Builds the hub and the generated *pages* with the hpo domain and the
    layout rules (-W --keep-going), with *extra* pages (path -> text) in
    the toctree *toctree*: (exit status, warning lines, {page: html})."""
    with tempfile.TemporaryDirectory(prefix='tadah_hpo_pages_') as tmp:
        src = os.path.join(tmp, 'src')
        ref = os.path.join(src, 'nfp', 'reference')
        os.makedirs(ref)
        shutil.copy(os.path.join(SOURCE, 'nfp', 'reference', 'index.rst'),
                    ref)
        for name in PAGES:
            with open(os.path.join(ref, name + '.rst'), 'w',
                      encoding='utf-8') as fh:
                fh.write(pages[name])
        entries = ''.join(f'   {t}\n' for t in toctree)
        files = dict({
            'index.rst': 'Fixture\n=======\n\n.. toctree::\n'
                         '   :caption: Nested Fitting\n   :titlesonly:\n\n'
                         '   overview\n   nfp/reference/index\n' + entries,
            'overview.rst': '.. _nested_fitting:\n\nOverview\n========\n',
            'conf.py': (f'import sys\nsys.path.insert(0, {TOOLS!r})\n'
                        "project = 'Fixture'\n"
                        "extensions = ['hpotarget_domain', 'nfp_checks']\n"
                        "html_theme = 'sphinx_rtd_theme'\n"
                        "html_theme_options = {'navigation_depth': 2}\n")},
            **(extra or {}))
        for name, text in files.items():
            path = os.path.join(src, name)
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, 'w', encoding='utf-8') as fh:
                fh.write(text)
        out = os.path.join(tmp, 'html')
        proc = subprocess.run(
            [sys.executable, '-m', 'sphinx', '-b', 'html', '-q', '-W',
             '--keep-going', src, out],
            capture_output=True, text=True, timeout=600)
        html = {}
        for name in extra or {}:
            path = os.path.join(out, name[:-4] + '.html')
            if os.path.isfile(path):
                with open(path, encoding='utf-8') as fh:
                    html[name[:-4]] = fh.read()
    warnings = [l.replace(src + os.sep, '') for l in proc.stderr.splitlines()
                if 'WARNING' in l]
    return proc.returncode, warnings, html

if __name__ == '__main__':
    unittest.main()
