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
    ids = re.findall(r'^\.\. hpo:(?:key|block|opt|param|removed|target):: '
                     r'(\S+)$', text, re.M)
    for also in re.findall(r'^   :also: (.+)$', text, re.M):
        ids += also.split()
    return ids


def code_blocks(text):
    """The non-blank lines of each code block of a page, without the indent
    of the block."""
    def indent(line):
        return len(line) - len(line.lstrip())
    blocks, lines, i = [], text.split('\n'), 0
    while i < len(lines):
        m = re.match(r'^(\s*)\.\. code-block::', lines[i])
        i += 1
        if not m:
            continue
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
        with tempfile.TemporaryDirectory(prefix='tadah_hpo_pages_') as tmp:
            src = os.path.join(tmp, 'src')
            ref = os.path.join(src, 'nfp', 'reference')
            os.makedirs(ref)
            shutil.copy(os.path.join(SOURCE, 'nfp', 'reference', 'index.rst'),
                        ref)
            for name in PAGES:
                with open(os.path.join(ref, name + '.rst'), 'w',
                          encoding='utf-8') as fh:
                    fh.write(self.pages[name])
            files = {
                'index.rst': 'Fixture\n=======\n\n.. toctree::\n'
                             '   :caption: Nested Fitting\n   :titlesonly:\n\n'
                             '   overview\n   nfp/reference/index\n',
                'overview.rst': '.. _nested_fitting:\n\nOverview\n========\n',
                'conf.py': (f'import sys\nsys.path.insert(0, {TOOLS!r})\n'
                            "project = 'Fixture'\n"
                            "extensions = ['hpotarget_domain', 'nfp_checks']\n"
                            "html_theme = 'sphinx_rtd_theme'\n"
                            "html_theme_options = {'navigation_depth': 2}\n")}
            for name, text in files.items():
                with open(os.path.join(src, name), 'w', encoding='utf-8') as fh:
                    fh.write(text)
            proc = subprocess.run(
                [sys.executable, '-m', 'sphinx', '-b', 'html', '-q', '-W',
                 '--keep-going', src, os.path.join(tmp, 'html')],
                capture_output=True, text=True, timeout=600)
        warnings = [l for l in proc.stderr.splitlines() if 'WARNING' in l]
        self.assertEqual(warnings, [])
        self.assertEqual(proc.returncode, 0, proc.stderr[-2000:])


if __name__ == '__main__':
    unittest.main()
