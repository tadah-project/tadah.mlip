"""Tests for the runs of the documentation's commands:
tests/doc_commands/doc_commands.py (the commands of an rst file's bash code
blocks) and tests/nfp_docs/check_nfp_run.py (the runs the Nested Fitting
manifest lists, tadah_nfp_<id>).

The runs use a stand-in for tadah, a Python script that writes the files
tadah writes and records each call; no tadah build is needed.

Run: python3 -m unittest discover -s docs/tools/tests -v
"""
import contextlib
import csv
import io
import json
import os
import shutil
import signal
import stat
import subprocess
import sys
import tempfile
import textwrap
import time
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, '..', '..', '..'))
sys.path.insert(0, os.path.join(REPO, 'tests', 'doc_commands'))
sys.path.insert(0, os.path.join(REPO, 'tests', 'nfp_docs'))
sys.path.insert(0, os.path.join(REPO, 'tests', 'cli_examples'))
import check_cli_example  # noqa: E402
import check_nfp_run  # noqa: E402
import doc_commands  # noqa: E402
import nfp_files  # noqa: E402

MANIFEST = os.path.join(REPO, 'docs', 'source', 'nfp', 'files', 'manifest.txt')

# The stand-in for tadah. In its working directory it appends its
# arguments to calls.log. An argument of --config must name an existing
# file; the file `exit` sets its exit status. For `train` it writes the
# potential (-o or pot.tadah), with the WEIGHTS line that fake.json's
# weights[<file>] gives (none without). For `hpo` it writes the files of a
# search, as tadah hpo writes them, from the entry of its --hpotarget
# argument in the file `fake.json` (a dictionary; `default` serves every
# other target):
#
#   loss         best loss (default: the content of the file `loss`, else 0.5)
#   status       the "Optimiser status" of hpo_run.log (default
#                "MAXEVAL_REACHED -- maximum evaluations exhausted"); null
#                leaves the line out
#   evaluations  the "Total evaluations" of hpo_run.log (default 3)
#   dims         [label, lo, hi, best, start] per search parameter, in
#                hpo_search_space.tsv (default [["RCUT2B", 4.0, 6.5, 5.0,
#                5.3]]); a sixth entry names its params.tadah column
#   losses       TOTAL_LOSS/N of every evaluation in loss.tadah (default:
#                loss + 1, then loss); points: the search parameters of
#                every evaluation in params.tadah (default: start, then best)
#   outvar       {"names": [...], "start": [...], "best": [...]}:
#                outvar.tadah and best_outvar.tadah
#   journal      more lines of hpo_run.log
#   stdout       lines the search prints on its standard output
#   stderr       lines the search prints on its standard error
#   legacy_best  the best_* files (best_params.tadah, best_outvar.tadah)
#                that lead with the column of tadah 1.5.0, #ITERATION, not
#                with #IMPROVEMENT and EVAL
#
# The history files have the headers of tadah 1.6.0: loss.tadah,
# params.tadah and outvar.tadah a row per evaluation (#ITERATION ...),
# best_loss.tadah, best_params.tadah and best_outvar.tadah a row per new
# best (#IMPROVEMENT EVAL ...): the start (evaluation 1), then the first
# evaluation of the lowest loss when it is a later one.
#
# An entry `<target>#<n>` serves the n-th `hpo` search of the directory
# before the entry of <target>. `hpo --check` writes nothing, and exits with
# the content of the file `check_exit` when there is one.
#
# The file `no_best` makes `hpo` write no best_pot.tadah. For `predict`
# it prints the table of -A, as tadah 1.6.0 does: a row per metric that
# names its unit ("Force RMSE (eV/Å)"), a Mean column and a column per -d
# file. The RMSE of kind k of the i-th -d file is fake.json's
# predict[<file name without .tadahdb>][k] (default 1) times the best loss
# of the best_pot.tadah in the directory; fake.json's predict_units[k]
# replaces the unit of kind k in the table (null: the row names none).
FAKE_TADAH = '''#!{python}
import json, os, sys
sys.stdout.reconfigure(encoding='utf-8')
args = sys.argv[1:]
with open('calls.log', 'a') as fh:
    fh.write(os.getcwd() + ' ' + ' '.join(args) + '\\n')
if '--config' in args and not os.path.isfile(args[args.index('--config') + 1]):
    print('no such configuration'); sys.exit(3)
if os.path.isfile('exit'):
    print('stand-in fails'); sys.exit(int(open('exit').read()))
fake = json.load(open('fake.json')) if os.path.isfile('fake.json') else {{}}
def opt(name):
    return args[args.index(name) + 1] if name in args else None
if args[:1] == ['hpo'] and '--check' in args:
    if os.path.isfile('check_exit'):
        print('stand-in check refuses'); sys.exit(int(open('check_exit').read()))
    print('passed the startup checks; no file was written')
elif args[:1] == ['hpo']:
    calls = [l for l in open('calls.log') if l.split()[1:2] == ['hpo']
             and '--check' not in l.split()]
    target = opt('--hpotarget')
    spec = fake.get('%s#%d' % (target, len(calls)),
                    fake.get(target, fake.get('default', {{}})))
    loss = spec.get('loss', float(open('loss').read()) if os.path.isfile('loss')
                    else 0.5)
    dims = spec.get('dims', [['RCUT2B', 4.0, 6.5, 5.0, 5.3]])
    n = spec.get('evaluations', 3)
    losses = spec.get('losses', [loss + 1.0] + [loss] * (max(n, 1) - 1))
    points = spec.get('points', [[d[4] for d in dims]]
                      + [[d[3] for d in dims]] * (len(losses) - 1))
    if not os.path.isfile('no_best'):
        open('best_pot.tadah', 'w').write(
            '# HPO_BEST_FORMAT 2\\n# HPO_BEST_LOSS %r\\nMODEL M_BLR BF_Linear\\n'
            % loss)
    # The bests: the start, then the first evaluation of the lowest loss.
    best_eval = 1 + losses.index(min(losses))
    bests = [(1, 1, 0)] + ([(2, best_eval, 1)] if best_eval > 1 else [])
    legacy = spec.get('legacy_best', [])
    def head(name):
        return '#ITERATION' if name in legacy else '#IMPROVEMENT EVAL'
    def lead(name, improvement, evaluation):
        return ('%d ' % evaluation if name in legacy
                else '%d %d ' % (improvement, evaluation))
    with open('best_loss.tadah', 'w') as fh:
        fh.write('#IMPROVEMENT EVAL SLOT L(ERMSE)[1] TOTAL_LOSS/N\\n')
        for k, e, _ in bests:
            fh.write('%d %d 1 %.6e %.6e\\n' % (k, e, losses[e - 1],
                                               losses[e - 1]))
    cols = ' '.join(d[5] if len(d) > 5 else d[0] for d in dims)
    with open('hpo_search_space.tsv', 'w') as fh:
        fh.write('# search space\\n#   dim  1-based\\n'
                 'dim\\tlabel  \\tlo\\thi\\torigin\\tx0\\n')
        for i, d in enumerate(dims, 1):
            fh.write('%d\\t%s\\t%.10g\\t%.10g\\tOPTIM\\t%.10g\\n'
                     % (i, d[0], d[1], d[2], d[4]))
    with open('loss.tadah', 'w') as fh:
        fh.write('#ITERATION SLOT L(ERMSE)[1] TOTAL_LOSS/N\\n')
        for i, x in enumerate(losses, 1):
            fh.write('%d 1 %.6e %.6e\\n' % (i, x, x))
    with open('params.tadah', 'w') as fh:
        fh.write('#ITERATION SLOT ' + cols + '\\n')
        for i, p in enumerate(points, 1):
            fh.write('%d 1 ' % i + ' '.join('%.6e' % x for x in p) + '\\n')
    with open('best_params.tadah', 'w') as fh:
        fh.write(head('best_params.tadah') + ' SLOT ' + cols + '\\n')
        for k, e, b in bests:
            fh.write(lead('best_params.tadah', k, e) + '1 '
                     + ' '.join('%.6e' % d[4 - b] for d in dims) + '\\n')
    if 'outvar' in spec:
        ov = spec['outvar']
        names = ' IT-TIME/ms SLOT ' + ' '.join(ov['names']) + '\\n'
        row = lambda v: '400 1 ' + ' '.join('%.6e' % x for x in v) + '\\n'
        values = (ov['start'], ov['best'])
        open('outvar.tadah', 'w').write(
            '#ITERATION' + names + '1 ' + row(ov['start'])
            + '2 ' + row(ov['best']))
        open('best_outvar.tadah', 'w').write(
            head('best_outvar.tadah') + names
            + ''.join(lead('best_outvar.tadah', k, e) + row(values[b])
                      for k, e, b in bests))
    with open('hpo_run.log', 'w') as fh:
        fh.write('  TADAH Hyperparameter Optimisation -- Run Journal\\n')
        for line in spec.get('journal', []):
            fh.write(line + '\\n')
        fh.write('-- DIAGNOSTICS --\\n  Total evaluations       : %d\\n' % n)
        if spec.get('status', '') is not None:
            fh.write('-- RESULT --\\n  Optimiser status        : %s\\n'
                     % spec.get('status', 'MAXEVAL_REACHED -- maximum '
                                'evaluations exhausted'))
    for line in spec.get('stdout', []):
        print(line)
    for line in spec.get('stderr', []):
        print(line, file=sys.stderr)
elif args[:1] == ['train']:
    out = args[args.index('-o') + 1] if '-o' in args else 'pot.tadah'
    weights = fake.get('weights', {{}}).get(out)
    open(out, 'w').write('MODEL M_BLR BF_Linear\\n' + (
        'WEIGHTS ' + ' '.join(repr(w) for w in weights) + '\\n'
        if weights is not None else ''))
elif args[:1] == ['predict']:
    i = args.index('-d') + 1
    data = []
    while i < len(args) and not args[i].startswith('-'):
        data.append(os.path.basename(args[i]).replace('.tadahdb', ''))
        i += 1
    best = 1.0
    for line in open(opt('-p')):
        if line.startswith('# HPO_BEST_LOSS'):
            best = float(line.split()[2])
    table = fake.get('predict', {{}})
    units = {{'energy': 'meV/atom', 'force': 'eV/Å', 'stress': 'GPa'}}
    units.update(fake.get('predict_units', {{}}))
    cells = lambda vs: ''.join('%16.6e' % v for v in vs)
    print('Metric'.ljust(24) + 'Mean'.rjust(16)
          + ''.join(('Val%d' % k).rjust(16) for k in range(1, len(data) + 1)))
    print('-' * (40 + 16 * len(data)))
    for kind in ('Energy', 'Force', 'Stress'):
        vals = [table.get(d, {{}}).get(kind.lower(), 1.0) * best for d in data]
        unit = units[kind.lower()]
        name = lambda m: kind + ' ' + m + (' (%s)' % unit if unit else '')
        print(name('MAE').ljust(24) + cells([0.0] * (len(vals) + 1)))
        print(name('RMSE').ljust(24) + cells([sum(vals) / len(vals)] + vals))
        print((kind + ' R^2').ljust(24) + cells([0.9] * (len(vals) + 1)))
        print()
'''

PAGE = '''Unit 1: First Search
====================

Run, in ``tutorial/unit1``:

.. code-block:: bash
   :caption: the search

   # train once, then search
   tadah train -c ../../data/config.train \\
      -o first.tadah
   tadah hpo --config ../../data/config.train --hpotarget config.hpo

.. code-block:: text

   ls -l
'''


class DocCommandsTest(unittest.TestCase):
    def write(self, text):
        fd, path = tempfile.mkstemp(suffix='.rst')
        self.addCleanup(os.remove, path)
        with os.fdopen(fd, 'w', encoding='utf-8') as fh:
            fh.write(textwrap.dedent(text))
        return path

    def test_commands_of_bash_blocks_with_lines(self):
        path = self.write(PAGE)
        self.assertEqual(doc_commands.bash_commands(path), [
            doc_commands.Command(10, ['tadah', 'train', '-c', '../../data/config.train',
                                      '-o', 'first.tadah']),
            doc_commands.Command(12, ['tadah', 'hpo', '--config',
                                      '../../data/config.train', '--hpotarget',
                                      'config.hpo'])])

    def test_options_comments_and_other_blocks_are_not_commands(self):
        path = self.write('''\
            .. code-block:: bash
               :caption: c
               :emphasize-lines: 1

               ls  # list
               # only a comment
            Text.

               tadah outside
            .. code-block:: none

               tadah not bash
              .. code-block:: bash

                 tadah nested
            ''')
        self.assertEqual([c.argv for c in doc_commands.bash_commands(path)],
                         [['ls'], ['tadah', 'nested']])
        self.assertEqual(doc_commands.tadah_commands(path), [['tadah', 'nested']])

    def test_blocks_are_named_by_the_line_of_their_directive(self):
        # The line Sphinx gives the literal block of a directive, so that
        # the documentation build (docs/tools/nfp_checks.py) can tell the
        # blocks these commands come from.
        path = self.write(PAGE)
        blocks = doc_commands.bash_blocks(path)
        self.assertEqual([b.line for b in blocks], [6])
        self.assertEqual(blocks[0].commands, doc_commands.bash_commands(path))
        path = self.write('''\
            .. code-block:: bash

            .. code:: bash

               tadah code
              .. code-block:: bash

                 tadah nested
            ''')
        self.assertEqual([(b.line, [c.argv for c in b.commands])
                          for b in doc_commands.bash_blocks(path)],
                         [(1, []), (6, [['tadah', 'nested']])])

    def test_the_cli_example_check_reads_tadah_commands_only(self):
        path = self.write(PAGE.replace('   # train once', '   ls\n   # train once'))
        self.assertEqual(check_cli_example.readme_commands(path),
                         [c.argv for c in doc_commands.bash_commands(path)
                          if c.argv[0] == 'tadah'])
        self.assertEqual(len(check_cli_example.readme_commands(path)), 2)


class ManifestTest(unittest.TestCase):
    def parse(self, text):
        fd, path = tempfile.mkstemp(suffix='.txt')
        self.addCleanup(os.remove, path)
        with os.fdopen(fd, 'w', encoding='utf-8') as fh:
            fh.write(text)
        return check_nfp_run.parse_manifest(path)

    def test_lines_and_comments(self):
        runs = self.parse('# comment\n\n'
                          'unit1 | nfp/tutorial/unit1.rst | tutorial/unit1 | - '
                          '| 120 | best_loss=1e-6\n'
                          ' pt | nfp/examples/pt.rst | examples/pt | slow,anneal '
                          '| 900 | best_loss=abs:1e-9 \n')
        self.assertEqual([(r.id, r.page, r.dir, r.labels, r.timeout, r.line)
                          for r in runs],
                         [('unit1', 'nfp/tutorial/unit1.rst', 'tutorial/unit1',
                           [], 120, 3),
                          ('pt', 'nfp/examples/pt.rst', 'examples/pt',
                           ['slow', 'anneal'], 900, 4)])
        self.assertEqual(runs[0].tolerances,
                         {'best_loss': check_nfp_run.Tolerance(1e-6, False)})
        self.assertEqual(runs[1].tolerances,
                         {'best_loss': check_nfp_run.Tolerance(1e-9, True)})

    def test_errors_name_the_line(self):
        good = 'a | p.rst | d | - | 10 | -'
        cases = {
            'a | p.rst | d | - | 10': '5 fields, expected 6 or 7',
            'a | p.rst | d | - | 10 | - | x.hpo | y': '8 fields',
            'a | p.rst | d | - | 10 | - | ': 'searches "" is not',
            'a | p.rst | d | - | 10 | - | x.hpo,,run 2': 'searches "x.hpo,,run 2" '
                                                     'is not',
            'a-b | p.rst | d | - | 10 | -': 'id "a-b"',
            good + '\n' + good: 'also on line 2',
            'a | p.txt | d | - | 10 | -': 'page "p.txt"',
            'a | p.rst | /abs | - | 10 | -': 'directory "/abs"',
            'a | p.rst | d | Slow | 10 | -': 'label "Slow"',
            'a | p.rst | d | examples | 10 | -': 'label "examples"',
            'a | p.rst | d | - | 0 | -': 'timeout "0"',
            'a | p.rst | d | - | 1.5 | -': 'timeout "1.5"',
            'a | p.rst | d | - | 10 | best_loss': 'is not <quantity>=<tolerance>',
            'a | p.rst | d | - | 10 | loss=1': 'unknown quantity "loss"',
            'a | p.rst | d | - | 10 | best_loss=-1': 'not a number >= 0',
            'a | p.rst | d | - | 10 | best_loss=abs:x': 'not a number >= 0',
            'a | p.rst | d | - | 10 | best_loss=1,best_loss=2': 'given twice',
        }
        for text, message in cases.items():
            with self.subTest(text=text):
                with self.assertRaises(check_nfp_run.ManifestError) as cm:
                    self.parse('# c\n' + text + '\n')
                self.assertIn(message, str(cm.exception))
                self.assertRegex(str(cm.exception), r'\.txt:\d+: ')

    def test_list_prints_id_labels_and_timeout(self):
        fd, path = tempfile.mkstemp(suffix='.txt')
        self.addCleanup(os.remove, path)
        with os.fdopen(fd, 'w', encoding='utf-8') as fh:
            fh.write('u1 | p.rst | d | - | 120 | -\n'
                     'u2 | p.rst | d | slow | 900 | -\n')
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            self.assertEqual(check_nfp_run.main(['--list', path]), 0)
        self.assertEqual(out.getvalue(),
                         'u1|examples|120\nu2|examples,slow|900\n')

    def test_the_shipped_manifest_is_valid(self):
        check_nfp_run.parse_manifest(MANIFEST)

    def test_the_shipped_manifest_runs_every_page_and_directory(self):
        # Every page under docs/source/nfp that runs tadah, and every
        # directory of docs/source/nfp/files with a target file or a
        # training configuration, has a line; every line names a page and
        # a directory that exist.
        self.assertEqual(check_nfp_run.coverage(MANIFEST), [])

    def test_the_shipped_tolerances_hold_on_another_build(self):
        # Pins: every tolerance of the shipped manifest is at least that of
        # the class of its rows (losses 1e-3 relative, lengths 0.01 Å,
        # other quantities 1e-2 relative, counts exact), every quote
        # marker's tol= at least that of the tightest class, and every
        # value of expected.csv is written with at most two digits past
        # its tolerance. Before the fix: check_nfp_run has no
        # precision_problems (AttributeError); with it, the 212 tolerances
        # of rows tighter than their class (losses within 1e-6, lengths
        # within 1e-3 Å, widths and errors within 1e-4 or 1e-3) and the two
        # quotes within 1e-4 are listed.
        self.assertEqual(check_nfp_run.precision_problems(MANIFEST), [])


class CoverageTest(unittest.TestCase):
    '''A docs tree in a scratch directory: source/nfp/{tutorial/unit1.rst,
    target_file.rst} and source/nfp/files/{manifest.txt, data/,
    tutorial/unit1/}.'''

    def setUp(self):
        self.tmp = tempfile.mkdtemp(prefix='tadah_nfp_coverage_')
        self.addCleanup(shutil.rmtree, self.tmp)
        self.source = os.path.join(self.tmp, 'source')
        self.files = os.path.join(self.source, 'nfp', 'files')
        self.put('nfp/tutorial/unit1.rst', PAGE)
        # A guide whose only code is a target-file fragment runs nothing.
        self.put('nfp/target_file.rst', 'The Target File\n===============\n\n'
                 '.. code-block:: none\n\n   OPTIM RCUT2B (1) 4.0 6.5\n')
        self.put('nfp/files/data/ta.tadahdb', 'data\n')
        self.put('nfp/files/tutorial/unit1/config.hpo', 'OPTIM RCUT2B (1) 4.0 6.5\n')
        self.put('nfp/files/tutorial/unit1/config.train', 'DBFILE ta.db\n')
        self.manifest('unit1 | nfp/tutorial/unit1.rst | tutorial/unit1 | - | 60 | -')

    def put(self, rel, text):
        path = os.path.join(self.source, rel)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, 'w', encoding='utf-8') as fh:
            fh.write(text)

    def manifest(self, *lines):
        self.put('nfp/files/manifest.txt', '# runs\n' + ''.join(l + '\n' for l in lines))

    def coverage(self):
        return check_nfp_run.coverage(os.path.join(self.files, 'manifest.txt'))

    def test_a_manifest_that_runs_every_page_and_directory(self):
        self.assertEqual(self.coverage(), [])

    def test_a_page_that_runs_tadah_needs_a_line(self):
        self.put('nfp/examples/pt.rst', 'PT\n==\n\n.. code-block:: bash\n\n'
                 '   tadah hpo -h\n')
        problems = self.coverage()
        self.assertEqual(len(problems), 1, problems)
        self.assertRegex(problems[0], r'nfp/examples/pt\.rst:6: runs tadah, and no '
                         r'line of .*manifest\.txt runs the page')

    def test_a_directory_of_runnable_files_needs_a_line(self):
        self.put('nfp/files/examples/pt/config.hpo', 'OPTIM RCUT2B (1) 4.0 6.5\n')
        self.put('nfp/files/examples/gfs/retrain.train', 'DBFILE ta.db\n')
        problems = self.coverage()
        self.assertEqual(len(problems), 2, problems)
        self.assertRegex(problems[0], r'nfp/files/examples/gfs holds a training '
                         r'configuration \(retrain\.train\), and no line of '
                         r'.*manifest\.txt runs in it')
        self.assertRegex(problems[1], r'nfp/files/examples/pt holds a target file '
                         r'\(config\.hpo\), and no line of .*manifest\.txt runs in it')

    def test_each_line_names_a_page_and_a_directory_that_exist(self):
        self.manifest('unit1 | nfp/tutorial/unit1.rst | tutorial/unit1 | - | 60 | -',
                      'u2 | nfp/tutorial/unit2.rst | tutorial/unit1 | - | 60 | -',
                      'u3 | nfp/tutorial/unit1.rst | tutorial/unit3 | - | 60 | -',
                      'u4 | ../outside.rst | tutorial/unit1 | - | 60 | -',
                      'u5 | nfp/tutorial/unit1.rst | ../../.. | - | 60 | -')
        self.put('../outside.rst', PAGE)
        problems = self.coverage()
        self.assertEqual(len(problems), 4, problems)
        self.assertRegex(problems[0], r'manifest\.txt:3: page nfp/tutorial/unit2\.rst '
                         r'does not exist')
        self.assertRegex(problems[1], r'manifest\.txt:4: directory tutorial/unit3 '
                         r'does not exist')
        self.assertRegex(problems[2], r'manifest\.txt:5: page \.\./outside\.rst is '
                         r'not under ')
        self.assertRegex(problems[3], r'manifest\.txt:6: directory \.\./\.\./\.\. is '
                         r'not under ')

    def test_a_slow_line_needs_a_short_variant(self):
        # The guide: a long run is labelled slow, with a short variant in
        # the default suite (ctest -LE slow).
        self.manifest('unit1 | nfp/tutorial/unit1.rst | tutorial/unit1 | slow | 60 | -')
        problems = self.coverage()
        self.assertEqual(len(problems), 1, problems)
        self.assertRegex(problems[0], r'manifest\.txt:2: the line unit1 is slow, and '
                         r'no line without slow runs nfp/tutorial/unit1\.rst '
                         r'\(a short variant for ctest -LE slow\)')
        self.manifest('unit1 | nfp/tutorial/unit1.rst | tutorial/unit1 | slow | 60 | -',
                      'unit1_short | nfp/tutorial/unit1.rst | tutorial/unit1 | - | '
                      '60 | - | config.hpo')
        self.assertEqual(self.coverage(), [])

    def test_the_searches_of_a_line_are_searches_of_its_page(self):
        self.manifest('unit1 | nfp/tutorial/unit1.rst | tutorial/unit1 | - | 60 | - '
                      '| run 1, other.hpo')
        problems = self.coverage()
        self.assertEqual(len(problems), 1, problems)
        self.assertRegex(problems[0], r'manifest\.txt:2: the line names the search '
                         r'"other\.hpo", which nfp/tutorial/unit1\.rst does not '
                         r'run \(its searches: run 1 \(config\.hpo\)\)')

    def test_a_malformed_manifest_is_reported(self):
        self.manifest('unit1 | nfp/tutorial/unit1.rst | tutorial/unit1')
        problems = self.coverage()
        self.assertEqual(len(problems), 1, problems)
        self.assertIn('3 fields, expected 6 or 7', problems[0])


# The training configuration of the runs: its descriptors set the units of
# the grid parameters (nfp_files.DESCRIPTOR_UNITS).
CONFIG_TRAIN = ('DBFILE ta.db\n'
                'TYPE2B     D2_Blip 9 9 Ta Ta   # a comment\n'
                'TYPEMB     DM_mBlipEnv 0 1 1 11 11 Ta Ta\n')


class RunFixture(unittest.TestCase):
    '''A docs tree in a scratch directory: source/nfp/files/{manifest.txt,
    data/config.train, tutorial/unit1/...} and source/nfp/unit1.rst.'''

    def setUp(self):
        self.tmp = tempfile.mkdtemp(prefix='tadah_nfp_run_')
        self.addCleanup(shutil.rmtree, self.tmp)
        self.files = os.path.join(self.tmp, 'source', 'nfp', 'files')
        self.unit = os.path.join(self.files, 'tutorial', 'unit1')
        os.makedirs(self.unit)
        os.makedirs(os.path.join(self.files, 'data'))
        self.put('data/config.train', CONFIG_TRAIN)
        self.put('tutorial/unit1/config.hpo', 'OPTIM RCUT2B (1) 4.0 6.5\n')
        self.put('tutorial/unit1/expected.csv', 'Quantity,Value\nBest loss,0.5\n')
        self.page(PAGE)
        self.manifest('best_loss=1e-6')
        self.tadah = os.path.join(self.tmp, 'tadah')
        with open(self.tadah, 'w', encoding='utf-8') as fh:
            fh.write(FAKE_TADAH.format(python=sys.executable))
        os.chmod(self.tadah, os.stat(self.tadah).st_mode | stat.S_IXUSR)
        self.work = os.path.join(self.tmp, 'work')

    def put(self, rel, text):
        with open(os.path.join(self.files, rel), 'w', encoding='utf-8') as fh:
            fh.write(text)

    def page(self, text):
        with open(os.path.join(self.tmp, 'source', 'nfp', 'unit1.rst'), 'w',
                  encoding='utf-8') as fh:
            fh.write(text)

    def manifest(self, tolerances, labels='-', timeout=60, searches=None):
        line = (f'unit1 | nfp/unit1.rst | tutorial/unit1 | {labels} | {timeout} '
                f'| {tolerances}')
        if searches is not None:
            line += f' | {searches}'
        self.put('manifest.txt', '# runs\n' + line + '\n')

    def run_check(self, nfp='ON', rid='unit1'):
        out = io.StringIO()
        with contextlib.redirect_stdout(out), \
                contextlib.redirect_stderr(out):
            status = check_nfp_run.main([
                '--manifest', os.path.join(self.files, 'manifest.txt'),
                '--id', rid, '--tadah', self.tadah, '--workdir', self.work,
                '--nfp', nfp])
        return status, out.getvalue()

    def tree(self):
        return sorted(os.path.relpath(os.path.join(d, f), self.files)
                      for d, _, fs in os.walk(self.files) for f in fs)

    def fake(self, spec):
        self.put('tutorial/unit1/fake.json', json.dumps(spec))

    def expect(self, *rows):
        with open(os.path.join(self.unit, 'expected.csv'), 'w', encoding='utf-8',
                  newline='') as fh:
            writer = csv.writer(fh, lineterminator='\n')
            writer.writerow(['Quantity', 'Value'])
            writer.writerows(rows)


class RunTest(RunFixture):
    def test_a_run_within_the_tolerance_passes(self):
        before = self.tree()
        status, out = self.run_check()
        self.assertEqual(status, 0, out)
        self.assertIn('ok: Best loss: 0.5, expected 0.5 within relative 1e-06 '
                      '(best_loss)', out)
        self.assertIn('PASS: 2 command(s) of nfp/unit1.rst', out)
        # Both commands ran, in order, in the scratch copy of the
        # directory, where ../../data resolves.
        run_dir = os.path.join(self.work, 'files', 'tutorial', 'unit1')
        with open(os.path.join(run_dir, 'calls.log'), encoding='utf-8') as fh:
            calls = fh.read().splitlines()
        # Before the search, tadah hpo --check with its arguments.
        self.assertEqual(calls, [
            run_dir + ' train -c ../../data/config.train -o first.tadah',
            run_dir + ' hpo --config ../../data/config.train --hpotarget config.hpo '
                      '--check',
            run_dir + ' hpo --config ../../data/config.train --hpotarget config.hpo'])
        # The source tree is not written.
        self.assertEqual(self.tree(), before)

    def test_a_best_loss_outside_the_tolerance_fails(self):
        self.put('tutorial/unit1/loss', '0.5000006')
        status, out = self.run_check()
        self.assertEqual(status, 1, out)
        self.assertIn('FAIL: Best loss: 0.5000006, expected 0.5 within relative '
                      '1e-06 (best_loss)', out)
        self.assertIn('FAIL: outside the tolerance: Best loss', out)
        # An absolute tolerance.
        self.manifest('best_loss=abs:1e-6')
        self.assertEqual(self.run_check()[0], 0)
        self.manifest('best_loss=abs:1e-7')
        self.assertEqual(self.run_check()[0], 1)

    def test_a_command_that_fails_fails_the_run(self):
        self.put('tutorial/unit1/exit', '4')
        status, out = self.run_check()
        self.assertEqual(status, 1, out)
        self.assertIn('-o first.tadah` exited with 4:\nstand-in fails', out)

    def test_a_search_whose_check_fails_does_not_run(self):
        self.put('tutorial/unit1/check_exit', '5')
        status, out = self.run_check()
        self.assertEqual(status, 1, out)
        self.assertIn('--hpotarget config.hpo --check` exited with 5:\nstand-in '
                      'check refuses', out)
        run_dir = os.path.join(self.work, 'files', 'tutorial', 'unit1')
        with open(os.path.join(run_dir, 'calls.log'), encoding='utf-8') as fh:
            searches = [l for l in fh.read().splitlines()
                        if ' hpo ' in l and not l.endswith('--check')]
        self.assertEqual(searches, [])

    def test_a_search_without_a_best_potential_fails(self):
        self.put('tutorial/unit1/no_best', '')
        status, out = self.run_check()
        self.assertEqual(status, 1, out)
        self.assertIn('exited with 0, but best_pot.tadah was not written', out)

    def test_only_tadah_commands_are_run(self):
        self.page(PAGE.replace('   # train once, then search', '   cd tutorial'))
        status, out = self.run_check()
        self.assertEqual(status, 1, out)
        self.assertRegex(out, r'FAIL: .*unit1\.rst:9: "cd tutorial" is not a '
                         r'tadah command')
        self.assertFalse(os.path.exists(self.work))

    def test_a_page_without_commands_fails(self):
        self.page('Unit 1\n======\n\nText.\n')
        status, out = self.run_check()
        self.assertEqual((status, 'has no command in a bash code block' in out),
                         (1, True), out)

    def test_expected_values_and_tolerances_match(self):
        cases = {
            # (expected.csv, tolerances) -> message
            ('Quantity,Value\nBest loss,0.5\n', '-'): 'no tolerance for "best_loss"',
            ('Quantity,Value\n', 'best_loss=1e-6'): 'must hold the "Best loss"',
            ('Quantity,Value\nBest loss,0.5\nRMSE,1\n', 'best_loss=1e-6'):
                'no check measures "RMSE"',
            ('Name,Value\nBest loss,0.5\n', 'best_loss=1e-6'):
                'header row is not "Quantity,Value"',
            ('Quantity,Value\nBest loss,low\n', 'best_loss=1e-6'):
                'is not a number: low',
        }
        for (csv_text, tolerances), message in cases.items():
            with self.subTest(csv=csv_text, tolerances=tolerances):
                self.put('tutorial/unit1/expected.csv', csv_text)
                self.manifest(tolerances)
                status, out = self.run_check()
                self.assertEqual(status, 1, out)
                self.assertIn(message, out)

    def test_a_tolerance_without_its_quantity_fails(self):
        self.page(PAGE.replace('   tadah hpo --config ../../data/config.train '
                               '--hpotarget config.hpo\n', ''))
        self.put('tutorial/unit1/expected.csv', 'Quantity,Value\n')
        status, out = self.run_check()
        self.assertEqual(status, 1, out)
        self.assertIn('tolerance of "best_loss", which', out)
        # A page that runs no search needs no expected value.
        self.manifest('-')
        status, out = self.run_check()
        self.assertEqual(status, 0, out)

    def test_a_search_is_skipped_without_nfp(self):
        status, out = self.run_check(nfp='OFF')
        self.assertEqual(status, check_nfp_run.SKIP, out)
        self.assertIn('SKIP: nfp/unit1.rst runs tadah hpo', out)
        self.assertFalse(os.path.exists(self.work))

    def test_a_run_longer_than_its_timeout_fails(self):
        self.manifest('best_loss=1e-6', timeout=1)
        with open(self.tadah, 'a', encoding='utf-8') as fh:
            fh.write('import time; time.sleep(3)\n')
        status, out = self.run_check()
        self.assertEqual(status, 1, out)
        self.assertIn("did not end within the run's 1 s", out)

    def test_a_relative_tadah_is_taken_from_the_working_directory(self):
        # The commands run in the copy of the files; a relative --tadah
        # (./tadah) names the program from where the script was started.
        cwd = os.getcwd()
        self.addCleanup(os.chdir, cwd)
        os.chdir(self.tmp)
        self.tadah = os.path.join('.', 'tadah')
        status, out = self.run_check()
        self.assertEqual(status, 0, out)

    def test_an_unknown_id_fails(self):
        status, out = self.run_check(rid='unit9')
        self.assertEqual(status, 1, out)
        self.assertIn('has no run "unit9"', out)

    def test_the_script_runs_from_the_command_line(self):
        proc = subprocess.run(
            [sys.executable, os.path.join(REPO, 'tests', 'nfp_docs',
                                          'check_nfp_run.py'),
             '--manifest', os.path.join(self.files, 'manifest.txt'),
             '--id', 'unit1', '--tadah', self.tadah, '--workdir', self.work,
             '--nfp', 'ON'], capture_output=True, text=True, timeout=60)
        self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)



SEARCH_PAGE = """Unit 1
======

.. code-block:: bash

   tadah hpo --config ../../data/config.train --hpotarget config.hpo
"""

STATUS = 'MAXEVAL_REACHED -- maximum evaluations exhausted'


def page_of(*commands):
    """A page with one bash code block of *commands*."""
    return ('Unit 1\n======\n\n.. code-block:: bash\n\n'
            + ''.join(f'   {c}\n' for c in commands))


class SearchChecksTest(RunFixture):
    """What every search passes (status, evaluations, the box), and the
    rows of a page with one search."""

    def setUp(self):
        super().setUp()
        self.page(SEARCH_PAGE)

    def test_best_values_status_and_evaluations_pass(self):
        self.expect(('Best loss', '0.5'), ('Best RCUT2B (Å)', '5.0'),
                    ('Status', STATUS), ('Evaluations', '3'))
        self.manifest('best_loss=1e-6, best_rcut2b=abs:0.01, evaluations=abs:0')
        status, out = self.run_check()
        self.assertEqual(status, 0, out)
        self.assertIn(f'run 1 (config.hpo): status "{STATUS}", 3 evaluations',
                      out)
        self.assertIn('ok: Best RCUT2B (Å): 5.0, expected 5.0 within absolute '
                      '0.01 (best_rcut2b)', out)
        self.assertIn(f'ok: Status: "{STATUS}", expected "{STATUS}"', out)
        self.assertIn('ok: Evaluations: 3, expected 3.0 within absolute 0',
                      out)

    def test_a_best_value_outside_its_tolerance_fails(self):
        self.expect(('Best loss', '0.5'), ('Best RCUT2B (Å)', '5.02'))
        self.manifest('best_loss=1e-6, best_rcut2b=abs:0.01')
        status, out = self.run_check()
        self.assertEqual(status, 1, out)
        self.assertIn('FAIL: Best RCUT2B (Å): 5.0, expected 5.02 within absolute '
                      '0.01 (best_rcut2b)', out)
        self.assertIn('outside the tolerance: Best RCUT2B (Å)', out)

    def test_indexed_and_shortened_labels(self):
        # params.tadah shortens a long label with '*' (FIXWEI*[26]).
        self.fake({'default': {'dims': [
            ['CGRID2B[3]', 2.0, 3.0, 2.5, 2.2],
            ['FIXWEIGHT[26]', 0.0, 50.0, 19.9, 23.0, 'FIXWEI*[26]']]}})
        self.expect(('Best loss', '0.5'), ('Best CGRID2B[3] (Å)', '2.5'),
                    ('Best FIXWEIGHT[26] (eV)', '19.9'))
        self.manifest('best_loss=1e-6, best_cgrid2b[3]=1e-6, '
                      'best_fixweight[26]=abs:0.01')
        status, out = self.run_check()
        self.assertEqual(status, 0, out)
        self.assertIn('ok: Best FIXWEIGHT[26] (eV): 19.9', out)

    def test_columns_must_name_the_dimensions_in_order(self):
        self.fake({'default': {'dims': [['RCUT2B', 4.0, 6.5, 5.0, 5.3,
                                         'RCUTMB']]}})
        status, out = self.run_check()
        self.assertEqual(status, 1, out)
        # After IMPROVEMENT, EVAL and SLOT.
        self.assertIn('best_params.tadah: column 4 is RCUTMB, '
                      'hpo_search_space.tsv dimension 1 is RCUT2B', out)

    def test_the_best_files_lead_with_improvement_and_eval(self):
        # best_params.tadah and best_outvar.tadah number their rows by
        # improvement and give the evaluation of each in EVAL: neither is a
        # search parameter nor a column of outvar.tadah.
        self.fake({'default': {'losses': [1.5, 0.7, 0.5],
                               'outvar': {'names': ['ERMSE'], 'start': [17.0],
                                          'best': [13.0]}}})
        self.expect(('Best loss', '0.5'), ('Best RCUT2B (Å)', '5.0'),
                    ('Best ERMSE (meV/atom)', '13.0'))
        self.manifest('best_loss=1e-6, best_rcut2b=1e-6, best_ermse=1e-6')
        status, out = self.run_check()
        self.assertEqual(status, 0, out)
        self.assertIn('ok: Best ERMSE (meV/atom): 13.0', out)
        for name in ('IMPROVEMENT', 'EVAL', 'SLOT'):
            with self.subTest(name=name):
                self.expect(('Best loss', '0.5'), (f'Best {name} (s)', '3'))
                self.manifest(f'best_loss=1e-6, best_{name.lower()}=1e-6')
                status, out = self.run_check()
                self.assertEqual(status, 1, out)
                self.assertIn(f'{name} is neither a search parameter of run '
                              '(search parameters: RCUT2B) nor a column of '
                              'outvar.tadah (columns: ERMSE)', out)

    def test_best_files_without_improvement_and_eval_fail(self):
        # The columns of tadah 1.5.0 (#ITERATION SLOT ...) are not read as
        # those of 1.6.0.
        self.fake({'default': {'legacy_best': ['best_params.tadah']}})
        status, out = self.run_check()
        self.assertEqual(status, 1, out)
        self.assertIn("best_params.tadah: the columns ['ITERATION', 'SLOT', "
                      "'RCUT2B'] are not IMPROVEMENT, EVAL, SLOT and the 1 "
                      'dimension(s) of hpo_search_space.tsv', out)
        self.fake({'default': {'legacy_best': ['best_outvar.tadah'], 'outvar': {
            'names': ['ERMSE'], 'start': [17.0], 'best': [13.0]}}})
        self.expect(('Best loss', '0.5'), ('Best ERMSE (meV/atom)', '13.0'))
        self.manifest('best_loss=1e-6, best_ermse=1e-6')
        status, out = self.run_check()
        self.assertEqual(status, 1, out)
        self.assertIn("best_outvar.tadah: the columns ['ITERATION', "
                      "'IT-TIME/ms', 'SLOT', 'ERMSE'] do not start with "
                      'IMPROVEMENT, EVAL, IT-TIME/ms, SLOT', out)

    def test_a_name_that_is_no_search_parameter_fails(self):
        self.expect(('Best loss', '0.5'), ('Best RCUTMB (Å)', '5.0'))
        self.manifest('best_loss=1e-6, best_rcutmb=abs:0.01')
        status, out = self.run_check()
        self.assertEqual(status, 1, out)
        self.assertIn('RCUTMB is neither a search parameter of run (search '
                      'parameters: RCUT2B) nor a column of outvar.tadah', out)

    def test_a_best_value_outside_its_box_fails(self):
        self.fake({'default': {'dims': [['RCUT2B', 4.0, 6.5, 6.6, 5.3]]}})
        status, out = self.run_check()
        self.assertEqual(status, 1, out)
        self.assertIn('run 1 (config.hpo): the best RCUT2B 6.6 lies outside its '
                      'OPTIM box [4.0, 6.5]', out)
        # On a bound, and within the printed digits of the two files (7 in
        # best_params.tadah, 10 in hpo_search_space.tsv), it is inside.
        for dims in ([['RCUT2B', 4.0, 6.5, 6.5, 5.3]],
                     [['SGRIDMB', 0.1, 0.123456789012, 0.123456789012, 0.11]]):
            with self.subTest(dims=dims):
                self.fake({'default': {'dims': dims}})
                status, out = self.run_check()
                self.assertEqual(status, 0, out)

    def test_a_search_that_did_not_complete_fails(self):
        for text in ('FAILED -- boom', 'SHUTDOWN (SIGTERM)',
                     '(no status recorded)',
                     'STOPPED -- no evaluation produced a usable loss'):
            with self.subTest(status=text):
                self.fake({'default': {'status': text}})
                status, out = self.run_check()
                self.assertEqual(status, 1, out)
                self.assertIn(f'the search did not complete: status "{text}"',
                              out)
        self.fake({'default': {'status': None}})
        status, out = self.run_check()
        self.assertEqual(status, 1, out)
        self.assertIn('hpo_run.log has no "Optimiser status" line', out)
        self.fake({'default': {'evaluations': 0, 'losses': [0.5]}})
        status, out = self.run_check()
        self.assertEqual(status, 1, out)
        self.assertIn('the journal counts 0 evaluations', out)
        # A search that a stop rule ended completed.
        self.fake({'default': {'status': 'STOPPED -- stagnation: no new best '
                               'in 20 evaluations after 3 restart(s)'}})
        status, out = self.run_check()
        self.assertEqual(status, 0, out)

    def test_the_status_a_page_states_is_checked(self):
        self.expect(('Best loss', '0.5'),
                    ('Status', 'TADAH:GRID: all grid points evaluated'))
        status, out = self.run_check()
        self.assertEqual(status, 1, out)
        self.assertIn(f'FAIL: Status: "{STATUS}", expected "TADAH:GRID: all grid '
                      'points evaluated"', out)

    def test_a_check_is_not_a_search(self):
        # tadah hpo --check writes no file: the search after it is run 1.
        self.page(page_of(
            'tadah hpo --check --config ../../data/config.train '
            '--hpotarget config.hpo',
            'tadah hpo --config ../../data/config.train --hpotarget config.hpo'))
        self.expect(('Best loss (run 1)', '0.5'))
        status, out = self.run_check()
        self.assertEqual(status, 0, out)
        self.assertIn('run 1 (config.hpo): status', out)
        self.page(page_of('tadah hpo --check --config ../../data/config.train '
                          '--hpotarget config.hpo'))
        os.remove(os.path.join(self.unit, 'expected.csv'))
        self.manifest('-')
        status, out = self.run_check()
        self.assertEqual(status, 0, out)
        status, out = self.run_check(nfp='OFF')
        self.assertEqual(status, check_nfp_run.SKIP, out)

    def test_rows_that_cannot_be_read_fail(self):
        cases = {
            ('Best RCUT2B (Å)', 'five'): 'is not a number: five',
            ('Best loss (other.hpo)', '0.5'): 'no check measures "Best loss '
                                              '(other.hpo)"',
            ('Best RCUT2B', '5.0'): 'no check measures "Best RCUT2B"',
            ('Best loss (run 2)', '0.5'): 'the page runs 1 search(es), not 2',
            ('Status', ''): 'the status is empty',
        }
        for row, message in cases.items():
            with self.subTest(row=row):
                self.expect(('Best loss', '0.5'), row)
                status, out = self.run_check()
                self.assertEqual(status, 1, out)
                self.assertIn(message, out)
        self.expect(('Best loss', '0.5'), ('Best loss (run 1)', '0.5'))
        status, out = self.run_check()
        self.assertEqual(status, 1, out)
        self.assertIn('"Best loss (run 1)" measures what row 2 measures', out)

    def test_start_and_outvar_rows(self):
        # Ecoh takes its unit from dft_reference.csv.
        self.put('dft_reference.csv', 'quantity,value,unit,source,method\n'
                 'Ecoh,8.11398,eV/atom,OUTCAR,x\n')
        self.fake({'default': {
            'losses': [1.5, 0.5, 0.5],
            'outvar': {'names': ['ERMSE', 'Ecoh'], 'start': [17.0, 8.1],
                       'best': [13.0, 8.12]}}})
        self.expect(('Best loss', '0.5'), ('Start loss', '1.5'),
                    ('Best ERMSE (meV/atom)', '13.0'),
                    ('Start ERMSE (meV/atom)', '17.0'),
                    ('Best Ecoh (eV/atom)', '8.12'), ('Start RCUT2B (Å)', '5.3'))
        self.manifest('best_loss=1e-6, start_loss=1e-6, best_ermse=1e-6, '
                      'start_ermse=1e-6, best_ecoh=1e-6, start_rcut2b=1e-6')
        status, out = self.run_check()
        self.assertEqual(status, 0, out)
        self.assertIn('ok: Start loss: 1.5, expected 1.5', out)
        self.assertIn('ok: Best Ecoh (eV/atom): 8.12, expected 8.12', out)
        # A name that is both is not guessed.
        self.fake({'default': {'outvar': {'names': ['RCUT2B'], 'start': [1.0],
                                          'best': [2.0]}}})
        self.expect(('Best loss', '0.5'), ('Best RCUT2B (Å)', '5.0'))
        self.manifest('best_loss=1e-6, best_rcut2b=1e-6')
        status, out = self.run_check()
        self.assertEqual(status, 1, out)
        self.assertIn('RCUT2B is both a search parameter and a column of '
                      'outvar.tadah', out)

    def test_differences_from_dft(self):
        self.put('dft_reference.csv', 'quantity,value,unit,source,method\n'
                 'Ecoh,8.11398,eV/atom,OUTCAR,"a, b"\n')
        self.fake({'default': {'outvar': {'names': ['Ecoh', 'S'],
                                          'start': [8.1, 0.0],
                                          'best': [8.12, 0.0]}}})
        self.expect(('Best loss', '0.5'), ('Best Ecoh - DFT (eV/atom)', '0.00602'))
        self.manifest('best_loss=1e-6, best_ecoh_dft=abs:1e-9')
        status, out = self.run_check()
        self.assertEqual(status, 0, out)
        cases = {
            ('Best S - DFT (GPa)', 'best_s_dft'):
                'no quantity of dft_reference.csv is mapped to S',
            ('Best RCUT2B - DFT (Å)', 'best_rcut2b_dft'):
                'RCUT2B is a search parameter, not a property with a DFT value',
        }
        for (label, tolerance), message in cases.items():
            with self.subTest(label=label):
                self.expect(('Best loss', '0.5'), (label, '0'))
                self.manifest(f'best_loss=1e-6, {tolerance}=abs:1')
                status, out = self.run_check()
                self.assertEqual(status, 1, out)
                self.assertIn(message, out)

    def test_calibrated_t_hot(self):
        # The journal lines of tadah 1.6.0: the first calibration (pass 1),
        # its ladder, and the second pass after the step warm-up, which
        # rebuilds the ladder from its T_HOT.
        pass1 = ('[16:22:07]  INFO      ANNEAL_PT: T0=9.458e-01 from 20 probes: '
                 'median rise of the loss 2.833e+00 over the 6 probes that '
                 'raised it, T0 = median rise / ln(1/T0_ACCEPT) with '
                 'T0_ACCEPT=5.000e-02  loss range=[1.448e+01, 1.538e+02]  '
                 'std=3.918e+01  anchor=seed')
        ladder = ('[16:22:07]  INFO      ANNEAL_PT ladder: T_HOT=9.458e-01  '
                  'T_COLD=9.458e-04 (T_HOT/LADDER_SPAN)  geometric over 4 '
                  'slots, ratio=10.000 per rung  [static ladder]')
        pass2_t0 = ('[16:22:17]  INFO      ANNEAL_PT calibration pass 2: '
                    'T0=2.734e-01 from 20 probes: median rise of the loss '
                    '8.191e-01 over the 5 probes that raised it, T0 = median '
                    'rise / ln(1/T0_ACCEPT) with T0_ACCEPT=5.000e-02  loss '
                    'range=[1.474e+01, 2.151e+01]  std=2.204e+00  anchor=seed')
        pass2 = ('[16:22:17]  INFO      ANNEAL_PT calibration pass 2: '
                 'T_HOT=2.734e-01 from probes with the proposal scales of the '
                 'hottest slot after the step warm-up, which stopped at its '
                 'budget (pass 1 used the STEP_FRAC scales; T_HOT of pass 2 / '
                 'pass 1 = 0.29); the ladder is rebuilt')
        self.manifest('best_loss=1e-6, calibrated_t_hot=1e-3')
        # Two passes: the T_HOT of the second, which the search ran with.
        self.fake({'default': {'journal': [pass1, ladder, pass2_t0, pass2]}})
        self.expect(('Best loss', '0.5'), ('Calibrated T_HOT', '0.2734'))
        status, out = self.run_check()
        self.assertEqual(status, 0, out)
        self.assertIn('ok: Calibrated T_HOT: 0.2734, expected 0.2734', out)
        # One pass (STEP_MODE FIXED runs no second one): its T0.
        self.fake({'default': {'journal': [pass1.replace('20 probes',
                                                         '1 probe'), ladder]}})
        self.expect(('Best loss', '0.5'), ('Calibrated T_HOT', '0.9458'))
        status, out = self.run_check()
        self.assertEqual(status, 0, out)
        # No calibration: T_HOT given, or a search without ANNEAL_PT.
        given = ('[16:22:07]  INFO      ANNEAL_PT: PARAM T_HOT=0.27 given, so '
                 'no calibration runs')
        for journal in ([given, ladder.replace('9.458e-01', '2.700e-01')], []):
            with self.subTest(journal=journal):
                self.fake({'default': {'journal': journal}})
                status, out = self.run_check()
                self.assertEqual(status, 1, out)
                self.assertIn('0 journal lines "ANNEAL_PT: T0=<x> from <n> '
                              'probes" (the first calibration of T_HOT), not 1',
                              out)
        # A second pass-2 T_HOT is not guessed between.
        self.fake({'default': {'journal': [pass1, ladder, pass2, pass2]}})
        status, out = self.run_check()
        self.assertEqual(status, 1, out)
        self.assertIn('2 journal lines "ANNEAL_PT calibration pass 2: T_HOT=<x>", '
                      'not 0 or 1', out)

    def test_rows_of_multistart_starts(self):
        journal = ['[07:36:38]  INFO      MULTISTART start 1/2 done: 3 '
                   'evaluation(s) consumed, improved global best '
                   '1.7977e+308 -> 2.0',
                   '[07:40:00]  INFO      MULTISTART start 2/2 done: 2 '
                   'evaluation(s) consumed, improved global best 2.0 -> 1.0']
        spec = {'evaluations': 5, 'losses': [3.0, 2.0, 2.5, 1.0, 1.0],
                'points': [[5.3], [5.0], [4.8], [6.0], [6.1]],
                'journal': journal}
        self.fake({'default': spec})
        self.expect(('Best loss', '0.5'), ('Best loss of start 1', '2.0'),
                    ('Best RCUT2B of start 1 (Å)', '5.0'),
                    ('Best loss of start 2', '1.0'),
                    ('Best RCUT2B of start 2 (Å)', '6.0'))
        self.manifest('best_loss=1e-6, best_loss_of_start=1e-6, '
                      'best_rcut2b_of_start=abs:1e-6')
        status, out = self.run_check()
        self.assertEqual(status, 0, out)
        self.assertIn('ok: Best RCUT2B of start 2 (Å): 6.0', out)
        spec['journal'] = [journal[0],
                           journal[1].replace('2 evaluation', '3 evaluation')]
        self.fake({'default': spec})
        status, out = self.run_check()
        self.assertEqual(status, 1, out)
        self.assertIn('the starts count 6 evaluations, loss.tadah has 5 rows', out)
        spec['journal'] = []
        self.fake({'default': spec})
        status, out = self.run_check()
        self.assertEqual(status, 1, out)
        self.assertIn('the journal has no "MULTISTART start i/k done" line', out)

    def test_the_unit_of_a_row_is_the_unit_of_its_quantity(self):
        # Search parameters: nfp_files.PARAMETER_UNITS, and for a grid key
        # DESCRIPTOR_UNITS of the descriptor the training configuration
        # names; outvar.tadah columns: dft_reference.csv, else OUTVAR_UNITS.
        self.put('dft_reference.csv', 'quantity,value,unit,source,method\n'
                 'Ecoh,8.11398,eV/atom,OUTCAR,x\n')
        # The error scales of the fit: ESIGMA in meV/atom (ENORM ATOM, the
        # default), the absolute part of FSIGMA in eV/Å and of SSIGMA in
        # GPa (FSIGMA[1] when the training configuration gives both parts,
        # FSIGMA when it gives one). The validation errors as tadah 1.6.0
        # writes them: FRMSE in eV/Å, SRMSE_GPA in GPa.
        self.fake({'default': {
            'dims': [['RCUT2B', 4.0, 6.5, 5.0, 5.3],
                     ['SGRIDMB', 0.05, 1.0, 0.2, 0.3],
                     ['SGRID2B[2]', 1.0, 3.0, 2.0, 2.5],
                     ['ESIGMA', 0.5, 2.0, 1.0, 2.0],
                     ['FSIGMA[1]', 0.05, 0.2, 0.15, 0.1],
                     ['SSIGMA', 0.1, 1.0, 0.25, 0.5],
                     ['FIXWEIGHT[26]', 0.0, 50.0, 19.9, 23.0, 'FIXWEI*[26]']],
            'outvar': {'names': ['ERMSE', 'FRMSE', 'SRMSE_GPA', 'Ecoh'],
                       'start': [17.0, 0.4, 0.8, 8.1],
                       'best': [13.0, 0.3, 0.7, 8.12]}}})
        rows = [('Best loss', '0.5'), ('Best RCUT2B (Å)', '5.0'),
                ('Start SGRIDMB (Å^-2)', '0.3'), ('Best SGRID2B[2] (Å^-1)', '2.0'),
                ('Best ESIGMA (meV/atom)', '1.0'), ('Best FSIGMA[1] (eV/Å)', '0.15'),
                ('Best SSIGMA (GPa)', '0.25'), ('Best FIXWEIGHT[26] (eV)', '19.9'),
                ('Best ERMSE (meV/atom)', '13.0'), ('Best FRMSE (eV/Å)', '0.3'),
                ('Best SRMSE_GPA (GPa)', '0.7'), ('Best Ecoh (eV/atom)', '8.12'),
                ('Best Ecoh - DFT (eV/atom)', '0.00602')]
        tolerances = ('best_loss=1e-6, best_rcut2b=1e-6, start_sgridmb=1e-6, '
                      'best_sgrid2b[2]=1e-6, best_esigma=1e-6, '
                      'best_fsigma[1]=1e-6, best_ssigma=1e-6, '
                      'best_fixweight[26]=1e-6, best_ermse=1e-6, best_frmse=1e-6, '
                      'best_srmse_gpa=1e-6, best_ecoh=1e-6, '
                      'best_ecoh_dft=abs:1e-6')
        self.expect(*rows)
        self.manifest(tolerances)
        status, out = self.run_check()
        self.assertEqual(status, 0, out)
        self.assertIn('ok: Best FSIGMA[1] (eV/Å): 0.15', out)
        self.assertIn('ok: Best SRMSE_GPA (GPa): 0.7', out)
        cases = {
            'Best RCUT2B (A)': 'RCUT2B is in Å, not A',
            'Start SGRIDMB (A^-2)': 'SGRIDMB is in Å^-2 (DM_mBlipEnv), not A^-2',
            'Best SGRID2B[2] (Å^-2)': 'SGRID2B[2] is in Å^-1 (D2_Blip), not Å^-2',
            'Best ESIGMA (eV)': 'ESIGMA is in meV/atom (ENORM ATOM), not eV',
            'Best FSIGMA[1] (eV/A)': 'FSIGMA[1] is in eV/Å, not eV/A',
            'Best SSIGMA (eV/A^3)': 'SSIGMA is in GPa, not eV/A^3',
            'Best FIXWEIGHT[26] (meV)': 'FIXWEIGHT[26] is in eV, not meV',
            'Best ERMSE (eV/atom)': 'ERMSE is in meV/atom, not eV/atom',
            'Best FRMSE (eV/A)': 'FRMSE is in eV/Å, not eV/A',
            'Best SRMSE_GPA (eV/A^3)': 'SRMSE_GPA is in GPa, not eV/A^3',
            'Best Ecoh (kcal/mol)': 'Ecoh is in eV/atom (dft_reference.csv), not '
                                    'kcal/mol',
            'Best Ecoh - DFT (meV/atom)': 'Ecoh is in eV/atom (dft_reference.csv), '
                                          'not meV/atom',
        }
        for label, message in cases.items():
            with self.subTest(label=label):
                head = label.split(' (')[0]
                self.expect(*[(label, v) if r.split(' (')[0] == head else (r, v)
                              for r, v in rows])
                status, out = self.run_check()
                self.assertEqual(status, 1, out)
                self.assertIn(message, out)

    def test_the_unit_of_esigma_follows_enorm(self):
        # Under ENORM SQRT the energy residual of a structure of N atoms is
        # divided by sqrt(N): ESIGMA is in meV per sqrt(atom). ENORM is read
        # from the training configuration and the files it includes.
        self.fake({'default': {'dims': [['ESIGMA', 0.5, 2.0, 1.0, 2.0]]}})
        self.manifest('best_loss=1e-6, best_esigma=1e-6')
        for config, base in ((CONFIG_TRAIN + 'ENORM SQRT\n', CONFIG_TRAIN),
                             ('CONFIG ../../data/base.train\n',
                              CONFIG_TRAIN + 'ENORM   SQRT   # per sqrt(N)\n')):
            with self.subTest(config=config):
                self.put('data/base.train', base)
                self.put('data/config.train', config)
                self.expect(('Best loss', '0.5'),
                            ('Best ESIGMA (meV/√atom)', '1.0'))
                status, out = self.run_check()
                self.assertEqual(status, 0, out)
                self.expect(('Best loss', '0.5'), ('Best ESIGMA (meV/atom)', '1.0'))
                status, out = self.run_check()
                self.assertEqual(status, 1, out)
                self.assertIn('ESIGMA is in meV/√atom (ENORM SQRT), not '
                              'meV/atom', out)
        self.put('data/config.train', CONFIG_TRAIN + 'ENORM ATOM\n')
        status, out = self.run_check()
        self.assertEqual(status, 0, out)

    def test_the_descriptors_of_an_included_configuration(self):
        # A CONFIG line includes another training configuration (read from
        # the directory tadah runs in) in its place; a FIXWEIGHT needs the
        # descriptors to have a unit.
        self.put('data/base.train', CONFIG_TRAIN)
        self.put('data/config.train', 'CONFIG ../../data/base.train\n'
                 'DBFILE ta.db\n')
        self.fake({'default': {'dims': [
            ['SGRIDMB', 0.05, 1.0, 0.2, 0.3],
            ['FIXWEIGHT[26]', 0.0, 50.0, 19.9, 23.0, 'FIXWEI*[26]']]}})
        self.expect(('Best loss', '0.5'), ('Best SGRIDMB (Å^-2)', '0.2'),
                    ('Best FIXWEIGHT[26] (eV)', '19.9'))
        self.manifest('best_loss=1e-6, best_sgridmb=1e-6, best_fixweight[26]=1e-6')
        status, out = self.run_check()
        self.assertEqual(status, 0, out)
        self.put('data/config.train', 'DBFILE ta.db\n')
        status, out = self.run_check()
        self.assertEqual(status, 1, out)
        self.assertIn('no unit is known for SGRIDMB: the training configuration '
                      'has no TYPEMB', out)
        self.expect(('Best loss', '0.5'), ('Best FIXWEIGHT[26] (eV)', '19.9'))
        self.manifest('best_loss=1e-6, best_fixweight[26]=1e-6')
        status, out = self.run_check()
        self.assertEqual(status, 1, out)
        self.assertIn('no unit is known for FIXWEIGHT[26]: the training '
                      'configuration names no descriptor (TYPE2B, TYPEMB)', out)

    def test_a_quantity_without_a_known_unit_fails(self):
        # A grid key of a descriptor the tables do not list, an outvar.tadah
        # column that is neither a DFT quantity nor a validation error,
        # FIXWEIGHT with a descriptor whose values have a unit, the relative
        # part of FSIGMA (a ratio), and the retired EWEIGHT and SRMSE (eV/A^3)
        # of tadah 1.5.0.
        self.put('data/config.train', 'DBFILE ta.db\nTYPE2B D2_LJ Ta Ta\n')
        self.fake({'default': {
            'dims': [['CGRID2B[1]', 1.0, 3.0, 2.0, 2.5],
                     ['FIXWEIGHT[2]', 0.0, 50.0, 19.9, 23.0, 'FIXWEI*[2]'],
                     ['FSIGMA[2]', 0.0, 0.1, 0.05, 0.02],
                     ['EWEIGHT', 1.0, 1000.0, 86.0, 86.0]],
            'outvar': {'names': ['S', 'SRMSE'], 'start': [1.0, 0.01],
                       'best': [2.0, 0.005]}}})
        cases = {
            ('Best CGRID2B[1] (Å)', 'best_cgrid2b[1]'):
                'no unit is known for CGRID2B of D2_LJ (nfp_files.DESCRIPTOR_UNITS)',
            ('Best FIXWEIGHT[2] (eV)', 'best_fixweight[2]'):
                'no unit is known for FIXWEIGHT[2]: the descriptor D2_LJ is not '
                'one whose values have no unit (nfp_files.UNITLESS_DESCRIPTORS)',
            ('Best FSIGMA[2] (eV/Å)', 'best_fsigma[2]'):
                'no unit is known for FSIGMA[2] (nfp_files.PARAMETER_UNITS)',
            ('Best EWEIGHT (eV^-2)', 'best_eweight'):
                'no unit is known for EWEIGHT (nfp_files.PARAMETER_UNITS)',
            ('Best S (GPa)', 'best_s'):
                'no unit is known for the outvar.tadah column S',
            ('Best SRMSE (eV/A^3)', 'best_srmse'):
                'no unit is known for the outvar.tadah column SRMSE',
        }
        for (label, tolerance), message in cases.items():
            with self.subTest(label=label):
                self.expect(('Best loss', '0.5'), (label, '2.0'))
                self.manifest(f'best_loss=1e-6, {tolerance}=1e-6')
                status, out = self.run_check()
                self.assertEqual(status, 1, out)
                self.assertIn(message, out)

    def test_graded_failures(self):
        self.fake({'default': {'journal': [
            '  Graded failures         : 5  (6.7%)',
            '    A=0 (hot-var out of range)  B=0 (script or observable failed)  '
            'C=0 (did not complete)  D=5 (pot build / gated)']}})
        self.expect(('Best loss', '0.5'), ('Graded failures', '5'))
        self.manifest('best_loss=1e-6, graded_failures=abs:0')
        status, out = self.run_check()
        self.assertEqual(status, 0, out)
        self.assertIn('ok: Graded failures: 5, expected 5.0 within absolute 0',
                      out)
        self.expect(('Best loss', '0.5'), ('Graded failures', '4'))
        status, out = self.run_check()
        self.assertEqual(status, 1, out)
        self.assertIn('FAIL: Graded failures: 5, expected 4.0', out)
        self.fake({'default': {}})
        status, out = self.run_check()
        self.assertEqual(status, 1, out)
        self.assertIn('hpo_run.log has no "Graded failures" line', out)

    def test_the_manifest_takes_the_tolerance_names_of_the_rows(self):
        self.manifest('best_rcut2b=abs:0.02, best_fixweight[26]=0.01, '
                      'start_ermse=1e-4, best_ecoh_dft=abs:1e-3, '
                      'energy_rmse=0.02, calibrated_t_hot=1e-3, '
                      'evaluations=abs:0, best_loss_of_start=5e-3, '
                      'best_rcut2b_of_start=abs:0.05, start_loss=1e-4, '
                      'best_loss=1e-6, graded_failures=abs:0')
        runs = check_nfp_run.parse_manifest(os.path.join(self.files,
                                                         'manifest.txt'))
        self.assertEqual(len(runs[0].tolerances), 12)
        for bad in ('Best_RCUT2B=1', 'loss=1', 'rmse=1'):
            with self.subTest(tolerance=bad):
                self.manifest(bad)
                with self.assertRaises(check_nfp_run.ManifestError) as cm:
                    check_nfp_run.parse_manifest(os.path.join(self.files,
                                                              'manifest.txt'))
                self.assertIn('unknown quantity', str(cm.exception))


class CoefficientRowsTest(RunFixture):
    """The rows "Coefficient <i> of <file> (<unit>)": the i-th value of the
    WEIGHTS line of the potential that a tadah train command of the line
    writes."""

    def setUp(self):
        super().setUp()
        self.fake({'weights': {'first.tadah': [0.25, -1.5, 0.0, 0.0]}})
        self.manifest('best_loss=1e-6, coefficient=abs:0')

    def test_a_coefficient_of_a_trained_potential_is_measured(self):
        # Pins: the row reads the WEIGHTS line of first.tadah, the potential
        # that the page's tadah train command writes (-o), by its 1-based
        # index, in the unit of a coefficient of the descriptors of that
        # command's training configuration (eV), within the manifest's
        # tolerance "coefficient"; on a page with one search the row names
        # no search. Before the fix: the manifest refuses the tolerance name
        # "coefficient" (unknown quantity).
        self.expect(('Best loss', '0.5'),
                    ('Coefficient 2 of first.tadah (eV)', '-1.5'),
                    ('Coefficient 3 of first.tadah (eV)', '0'),
                    ('Coefficient 4 of first.tadah (eV)', '0'))
        status, out = self.run_check()
        self.assertEqual(status, 0, out)
        self.assertIn('ok: Coefficient 2 of first.tadah (eV): -1.5, expected '
                      '-1.5 within absolute 0.0 (coefficient)', out)
        self.assertIn('ok: Coefficient 4 of first.tadah (eV): 0.0, expected '
                      '0.0 within absolute 0.0 (coefficient)', out)

    def test_a_coefficient_outside_its_tolerance_fails(self):
        # Pins: a coefficient that differs from its row by more than the
        # tolerance fails the run. Before the fix: the manifest is refused
        # first (unknown tolerance name "coefficient").
        self.expect(('Best loss', '0.5'),
                    ('Coefficient 3 of first.tadah (eV)', '0.1'))
        status, out = self.run_check()
        self.assertEqual(status, 1, out)
        self.assertIn('FAIL: Coefficient 3 of first.tadah (eV): 0.0, expected '
                      '0.1 within absolute 0.0 (coefficient)', out)
        self.assertIn('outside the tolerance: Coefficient 3 of first.tadah '
                      '(eV)', out)

    def test_a_coefficient_it_cannot_measure_is_an_error(self):
        # Pins: a file that no tadah train command of the page writes, an
        # index past the WEIGHTS line, a potential without one and a unit
        # other than that of the coefficients are errors that name the
        # row. Before the fix: the manifest is refused first (unknown
        # tolerance name "coefficient"), whatever the row.
        cases = {
            'Coefficient 1 of other.tadah (eV)':
                'other.tadah is not the potential of a tadah train command '
                'of nfp/unit1.rst (first.tadah)',
            'Coefficient 5 of first.tadah (eV)':
                'first.tadah has 4 coefficients (WEIGHTS), not 5',
            'Coefficient 0 of first.tadah (eV)':
                'no check measures "Coefficient 0 of first.tadah (eV)"',
            'Coefficient 1 of first.tadah (meV)':
                'a coefficient of first.tadah is in eV, not meV',
        }
        for label, message in cases.items():
            with self.subTest(label=label):
                self.expect(('Best loss', '0.5'), (label, '0'))
                status, out = self.run_check()
                self.assertEqual(status, 1, out)
                self.assertIn(message, out)
        self.fake({})
        self.expect(('Best loss', '0.5'),
                    ('Coefficient 1 of first.tadah (eV)', '0'))
        status, out = self.run_check()
        self.assertEqual(status, 1, out)
        self.assertIn('first.tadah has no WEIGHTS line', out)

    def test_a_short_variant_does_not_measure_the_coefficients(self):
        # Pins: a line that names its searches runs no tadah train command
        # before them, so it lists the coefficient rows as not measured and
        # takes no tolerance for them. Before the fix: expected.csv has no
        # row kind "Coefficient" ("no check measures").
        self.expect(('Best loss', '0.5'),
                    ('Coefficient 3 of first.tadah (eV)', '0'))
        self.manifest('best_loss=1e-6', searches='config.hpo')
        status, out = self.run_check()
        self.assertEqual(status, 0, out)
        self.assertIn('not measured: Coefficient 3 of first.tadah (eV) (no '
                      'command of this line writes first.tadah)', out)


class SeveralSearchesTest(RunFixture):
    """Pages with more than one search, and tadah predict after a search."""

    def setUp(self):
        super().setUp()
        os.makedirs(os.path.join(self.files, 'tutorial', 'unit2'))
        self.put('tutorial/unit2/config.hpo', 'OPTIM RCUT2B (1) 4.0 6.5\n')
        # Two target files with the same base name: a search is named by its
        # --hpotarget argument as the page writes it.
        self.page(page_of(
            'tadah hpo --config ../../data/config.train \\',
            '    --hpotarget ../unit2/config.hpo',
            'tadah hpo --config ../../data/config.train --hpotarget config.hpo'))
        self.fake({'../unit2/config.hpo': {'loss': 0.7, 'dims': [
                       ['RCUT2B', 4.0, 6.5, 6.0, 5.3]]},
                   'config.hpo': {'loss': 0.4, 'dims': [
                       ['RCUT2B', 4.0, 6.5, 4.5, 5.3]]}})

    def test_each_row_is_measured_after_its_search(self):
        self.expect(('Best loss (../unit2/config.hpo)', '0.7'),
                    ('Best loss (config.hpo)', '0.4'),
                    ('Best RCUT2B (Å) (../unit2/config.hpo)', '6.0'),
                    ('Best RCUT2B (Å) (config.hpo)', '4.5'))
        self.manifest('best_loss=1e-6, best_rcut2b=abs:1e-6')
        status, out = self.run_check()
        self.assertEqual(status, 0, out)
        self.assertIn('ok: Best loss (../unit2/config.hpo): 0.7', out)
        self.assertIn('ok: Best RCUT2B (Å) (config.hpo): 4.5', out)
        with open(os.path.join(self.work, 'snap', '1', 'best_pot.tadah'),
                  encoding='utf-8') as fh:
            self.assertIn('HPO_BEST_LOSS 0.7', fh.read())

    def test_a_row_names_its_search(self):
        self.expect(('Best loss', '0.4'))
        status, out = self.run_check()
        self.assertEqual(status, 1, out)
        self.assertIn('the page runs 2 searches (run 1 (../unit2/config.hpo), '
                      'run 2 (config.hpo)), and the row names none', out)

    def test_run_numbers_name_searches_of_one_target(self):
        self.page(page_of(
            'tadah hpo --config ../../data/config.train --hpotarget config.hpo',
            'tadah hpo --config ../../data/config.train --hpotarget config.hpo'))
        self.fake({'config.hpo#1': {'loss': 0.7}, 'config.hpo#2': {'loss': 0.4}})
        self.expect(('Best loss (run 1)', '0.7'), ('Best loss (run 2)', '0.4'))
        status, out = self.run_check()
        self.assertEqual(status, 0, out)
        self.expect(('Best loss (config.hpo)', '0.4'))
        status, out = self.run_check()
        self.assertEqual(status, 1, out)
        self.assertIn('runs 1, 2 both use config.hpo: name one as " (run <n>)"',
                      out)

    def test_rows_of_predict_tables(self):
        self.page(page_of(
            'tadah hpo --config ../../data/config.train \\',
            '    --hpotarget ../unit2/config.hpo',
            'tadah predict -p best_pot.tadah -F -S -A \\',
            '    -d ../../data/a.tadahdb ../../data/b.tadahdb',
            'tadah hpo --config ../../data/config.train --hpotarget config.hpo',
            'tadah predict -p best_pot.tadah -A -d ../../data/a.tadahdb'))
        with open(os.path.join(self.unit, 'fake.json'), encoding='utf-8') as fh:
            spec = json.load(fh)
        spec['predict'] = {'a': {'energy': 10.0, 'force': 0.3, 'stress': 0.005},
                           'b': {'energy': 20.0}}
        self.fake(spec)
        # The table of tadah 1.6.0: each row names its unit, and the Mean
        # column comes before Val1 (a: 7, b: 14, mean 10.5).
        self.expect(('Best loss (config.hpo)', '0.4'),
                    ('Energy RMSE a (meV/atom) (../unit2/config.hpo)', '7.0'),
                    ('Force RMSE a (eV/Å) (../unit2/config.hpo)', '0.21'),
                    ('Stress RMSE a (GPa) (../unit2/config.hpo)', '0.0035'),
                    ('Energy RMSE b (meV/atom) (../unit2/config.hpo)', '14.0'),
                    ('Energy RMSE a (meV/atom) (config.hpo)', '4.0'))
        self.manifest('best_loss=1e-6, energy_rmse=1e-6, force_rmse=1e-6, '
                      'stress_rmse=1e-6')
        status, out = self.run_check()
        self.assertEqual(status, 0, out)
        self.assertIn('ok: Energy RMSE a (meV/atom) (../unit2/config.hpo): 7.0',
                      out)
        self.assertIn('ok: Energy RMSE b (meV/atom) (../unit2/config.hpo): 14.0',
                      out)
        self.assertIn('ok: Force RMSE a (eV/Å) (../unit2/config.hpo): 0.21', out)
        self.assertIn('ok: Stress RMSE a (GPa) (../unit2/config.hpo): 0.0035',
                      out)
        self.assertIn('ok: Energy RMSE a (meV/atom) (config.hpo): 4.0', out)
        # The unit of a row is the one the table names.
        cases = {
            'Energy RMSE a (eV/atom) (config.hpo)':
                'tadah predict prints the energy RMSE in meV/atom, not eV/atom',
            'Force RMSE a (eV/A) (config.hpo)':
                'tadah predict prints the force RMSE in eV/Å, not eV/A',
            'Stress RMSE a (eV/A^3) (config.hpo)':
                'tadah predict prints the stress RMSE in GPa, not eV/A^3',
            'Energy RMSE b (meV/atom) (config.hpo)':
                'no tadah predict command after run 2 (config.hpo) reads b',
        }
        for label, message in cases.items():
            with self.subTest(label=label):
                tolerance = label.split()[0].lower() + '_rmse'
                self.expect(('Best loss (config.hpo)', '0.4'), (label, '1'))
                self.manifest(f'best_loss=1e-6, {tolerance}=1e-6')
                status, out = self.run_check()
                self.assertEqual(status, 1, out)
                self.assertIn(message, out)
        self.manifest('best_loss=1e-6, force_rmse=1e-6')
        self.expect(('Best loss (config.hpo)', '0.4'),
                    ('Force RMSE a (eV/Å) (config.hpo)', '0.12'))
        status, out = self.run_check()
        self.assertEqual(status, 0, out)
        spec['predict_units'] = {'force': 'eV/Ang'}
        self.fake(spec)
        status, out = self.run_check()
        self.assertEqual(status, 1, out)
        self.assertIn('tadah predict prints the force RMSE in eV/Ang, not eV/Å',
                      out)
        # A row that names no unit is not read.
        spec['predict_units'] = {'force': None}
        self.fake(spec)
        status, out = self.run_check()
        self.assertEqual(status, 1, out)
        self.assertIn('the Force RMSE row of the table of the tadah predict '
                      'command on line 11 names no unit', out)

    def calls(self):
        run_dir = os.path.join(self.work, 'files', 'tutorial', 'unit1')
        with open(os.path.join(run_dir, 'calls.log'), encoding='utf-8') as fh:
            return [l[len(run_dir) + 1:] for l in fh.read().splitlines()]

    def test_a_line_runs_the_searches_it_names(self):
        # A short variant: the line names the searches it runs; each runs
        # with the commands after it up to the next search, and only the
        # rows of the searches it runs are measured.
        self.page(page_of(
            'tadah hpo --config ../../data/config.train \\',
            '    --hpotarget ../unit2/config.hpo',
            'tadah predict -p best_pot.tadah -A -d ../../data/a.tadahdb',
            'tadah hpo --config ../../data/config.train --hpotarget config.hpo',
            'tadah predict -p best_pot.tadah -A -d ../../data/b.tadahdb'))
        self.expect(('Best loss (../unit2/config.hpo)', '0.7'),
                    ('Energy RMSE a (meV/atom) (../unit2/config.hpo)', '0.7'),
                    ('Best loss (config.hpo)', '0.4'),
                    ('Best RCUT2B (Å) (config.hpo)', '4.5'))
        self.manifest('best_loss=1e-6, best_rcut2b=abs:1e-6', searches='config.hpo')
        status, out = self.run_check()
        self.assertEqual(status, 0, out)
        self.assertEqual(self.calls(), [
            'hpo --config ../../data/config.train --hpotarget config.hpo --check',
            'hpo --config ../../data/config.train --hpotarget config.hpo',
            'predict -p best_pot.tadah -A -d ../../data/b.tadahdb'])
        self.assertIn('ok: Best RCUT2B (Å) (config.hpo): 4.5', out)
        self.assertIn('not measured: Best loss (../unit2/config.hpo) (run 1 '
                      '(../unit2/config.hpo) is not run by this line)', out)
        self.assertIn('PASS: 2 command(s) of nfp/unit1.rst', out)
        # By number, with the rows of its search.
        self.manifest('best_loss=1e-6, energy_rmse=1e-6', searches='run 1')
        status, out = self.run_check()
        self.assertEqual(status, 0, out)
        self.assertEqual([c.split(' --')[0] for c in self.calls()],
                         ['hpo', 'hpo', 'predict -p best_pot.tadah -A -d '
                          '../../data/a.tadahdb'])
        self.assertIn('ok: Energy RMSE a (meV/atom) (../unit2/config.hpo): 0.7',
                      out)
        # A tolerance only the rows of a search the line does not run use.
        self.manifest('best_loss=1e-6, energy_rmse=1e-6, best_rcut2b=abs:1e-6',
                      searches='run 1')
        status, out = self.run_check()
        self.assertEqual(status, 1, out)
        self.assertIn('tolerance of "best_rcut2b", which no row of the searches '
                      'of the line uses', out)
        # A search the page does not run.
        self.manifest('best_loss=1e-6', searches='run 3')
        status, out = self.run_check()
        self.assertEqual(status, 1, out)
        self.assertIn('the line names the search "run 3", which nfp/unit1.rst '
                      'does not run', out)

    def test_a_predict_row_reads_the_best_potential_of_its_search(self):
        self.put('tutorial/unit1/other.tadah', '# HPO_BEST_LOSS 2.0\n')
        self.page(page_of(
            'tadah hpo --config ../../data/config.train --hpotarget config.hpo',
            'tadah predict -p other.tadah -A -d ../../data/a.tadahdb'))
        self.expect(('Best loss', '0.4'), ('Energy RMSE a (meV/atom)', '2.0'))
        self.manifest('best_loss=1e-6, energy_rmse=1e-6')
        status, out = self.run_check()
        self.assertEqual(status, 1, out)
        self.assertIn('reads other.tadah, not the best_pot.tadah of run 1', out)



# --------------------------------------------------------------------------
# Quotes of program output (nfp_files: the marker, the scan, the match;
# check_nfp_run: the static check of the markers and the check in a run)
# --------------------------------------------------------------------------

# A journal as tadah hpo writes it: a timestamp column, a level column,
# a continuation line under the time column, sections, dates, durations.
JOURNAL = """\
================================================================================
  Tadah! nested fitting (tadah hpo) -- Run Journal
================================================================================

  Tadah version  : 1.6.0
  Started        : 2026-11-02 22:56:41

-- PROGRESS ------------------------------------------------------------
[22:56:41]  INFO      GRID search: 11 points in each dimension (PARAM GRID_POINTS; spacing = width/10), 1 dimension.
[22:56:41]  BEST     * eval=     1  loss=5.0095e-01  ERMSE=0.71 meV/at  (14 ms)
            BEST       top: ERMSE_global=5.01e-01(100.0%)
[22:56:41]  BEST     * eval=     6  loss=2.3946e-01  ERMSE=0.49 meV/at  (10 ms)
            BEST       top: ERMSE_global=2.39e-01(100.0%)

-- PERFORMANCE ---------------------------------------------------------
  Total wall time         : 0.2s
  Throughput              : 3649.1 evals/min

-- RESULT --------------------------------------------------------------
  Optimiser status        : TADAH:GRID: all grid points evaluated
  Best total loss         : 2.3946e-01  (eval 6)
  Best potential saved    : best_pot.tadah
  Finished                : 2026-11-02 22:56:41
"""


def quote(text):
    """The lines of a quote as a page writes them (dedented)."""
    return textwrap.dedent(text).strip('\n').split('\n')


class MarkerTest(unittest.TestCase):
    """The grammar of ``.. output: <ids> <source> [key=value ...]``."""

    def test_markers_of_the_grammar(self):
        m = nfp_files.parse_marker('quickstart hpo_run.log')
        self.assertEqual((m.ids, m.source, m.search, m.command, m.tol),
                         (('quickstart',), 'hpo_run.log', None, None, None))
        m = nfp_files.parse_marker('unit7,unit7_short stdout command=3 '
                                   'tol=energy_rmse')
        self.assertEqual((m.ids, m.source, m.command, m.tol),
                         (('unit7', 'unit7_short'), 'stdout', 3,
                          'energy_rmse'))
        m = nfp_files.parse_marker('unit6 hpo_run.log '
                                   'search=anneal_pt.hpo tol=1e-3')
        self.assertEqual((m.search, m.tol), ('anneal_pt.hpo', '1e-3'))
        m = nfp_files.parse_marker("unit7 stderr search='run 2' tol=abs:0.01")
        self.assertEqual((m.source, m.search, m.tol),
                         ('stderr', 'run 2', 'abs:0.01'))

    def test_a_marker_outside_the_grammar_is_refused(self):
        for text, message in (
                ('', 'an output marker names <id> and <source>'),
                ('quickstart', 'an output marker names <id> and <source>'),
                ('quick-start hpo_run.log', 'id "quick-start" is not'),
                ('quickstart run.log', 'the source "run.log" is not stdout, '
                 'stderr or a file of a search (hpo_run.log, '),
                ('quickstart stdout lines=3', 'unknown option "lines"'),
                ('quickstart stdout command=0', 'command=0 is not a whole '
                 'number from 1'),
                ('quickstart stdout command=2 command=3',
                 'option "command" given twice'),
                ('quickstart hpo_run.log command=2', 'command= names a '
                 'command, whose source is stdout or stderr'),
                ('quickstart stdout command=2 search=config.hpo',
                 'command= and search= both name'),
                ('quickstart hpo_run.log tol=-1', 'tol=-1 is not'),
                ('quickstart hpo_run.log tol=abs:x', 'tol=abs:x is not'),
                ('quickstart hpo_run.log stray', 'stray" is not '
                 '<option>=<value>'),
                ("quickstart hpo_run.log search='run 2", 'cannot be read')):
            with self.subTest(text=text):
                with self.assertRaises(nfp_files.FilesError) as cm:
                    nfp_files.parse_marker(text)
                self.assertIn(message, str(cm.exception))


class QuoteScanTest(unittest.TestCase):
    def write(self, text):
        fd, path = tempfile.mkstemp(suffix='.rst')
        self.addCleanup(os.remove, path)
        with os.fdopen(fd, 'w', encoding='utf-8') as fh:
            fh.write(textwrap.dedent(text))
        return path

    def test_marked_blocks_with_their_lines(self):
        path = self.write("""\
            Title
            =====

            .. output: quickstart hpo_run.log

            .. code-block:: text
               :caption: the result

                 Optimiser status        : TADAH:GRID: all grid points

               ...
               Best total loss : 2.3946e-01

            .. code-block:: text

               unmarked
            - A list item:

              .. output: unit1 stdout
              .. code-block:: text

                 [tadah hpo] TADAH:GRID   dim=1
            """)
        blocks = nfp_files.quote_blocks(path)
        self.assertEqual([(b.marker_line, b.line, b.error) for b in blocks],
                         [(4, 6, None), (19, 20, None)])
        self.assertEqual(blocks[0].marker.ids, ('quickstart',))
        # The content dedented as docutils dedents it, without the options
        # and the blank lines around it; a blank line inside is kept.
        self.assertEqual(blocks[0].lines, [
            '  Optimiser status        : TADAH:GRID: all grid points', '',
            '...', 'Best total loss : 2.3946e-01'])
        self.assertEqual(blocks[1].lines, ['[tadah hpo] TADAH:GRID   dim=1'])
        self.assertEqual(nfp_files.text_blocks(path), [6, 14, 20])

    def test_a_marker_without_its_block_or_outside_the_grammar(self):
        path = self.write("""\
            .. output: quickstart hpo_run.log

            Text.

            .. output: quickstart nosuch.log
            .. code-block:: text

               x

            .. output: quickstart hpo_run.log
               more words
            .. code-block:: text

               y
            """)
        blocks = nfp_files.quote_blocks(path)
        self.assertEqual([b.marker_line for b in blocks], [1, 5, 10])
        self.assertIn('an output marker is followed by "Text.", not by '
                      '".. code-block:: text"', blocks[0].error)
        self.assertIn('the source "nosuch.log" is not', blocks[1].error)
        self.assertIsNone(blocks[1].marker)
        self.assertEqual(blocks[1].line, 6)
        self.assertIn('an output marker is one line', blocks[2].error)


class NormaliseTest(unittest.TestCase):
    def test_times_dates_durations_and_blanks_are_masked(self):
        n = nfp_files.normalise_output
        self.assertEqual(n('  Started        : 2026-11-02 22:56:41  '),
                         'Started : <date> <time>')
        self.assertEqual(n('[22:56:41]  BEST     * eval=     6  loss=2.3946e-01'
                           '  (10 ms)'),
                         '[<time>] BEST * eval= 6 loss=2.3946e-01 (<duration>)')
        self.assertEqual(n('Total wall time : 0.2s'),
                         'Total wall time : <duration>')
        self.assertEqual(n('took 1.5 min, then 2 h and 3e-3 s, 7 us'),
                         'took <duration>, then <duration> and <duration>, '
                         '<duration>')
        self.assertEqual(n('Throughput : 3649.1 evals/min'),
                         'Throughput : <rate>')
        self.assertEqual(n('time 9:05:01.25 of 12 searches in 3 slots'),
                         'time <time> of 12 searches in 3 slots')

    def test_a_duration_with_minutes_is_one_duration(self):
        # Pins: a duration that the journal writes in minutes and seconds,
        # with or without hours (HPOLog::format_duration, fmt_millis of
        # hpo_log.cpp: "1m 5s", "1h 2m 3s"), after a "~" or a "=" as well,
        # is masked whole, so a quote of such a line matches a run that
        # took another time. Before the fix: only the seconds are masked,
        # "Total wall time : 1m <duration>", and the minutes stay a number
        # that has to be the run's.
        n = nfp_files.normalise_output
        self.assertEqual(n('Total wall time : 1m 5s'),
                         'Total wall time : <duration>')
        self.assertEqual(n('Total wall time : 1h 0m 12s'),
                         'Total wall time : <duration>')
        self.assertEqual(n('~3 epoch(s) / ~12m 40s to MAXEVAL'),
                         '~3 epoch(s) / ~<duration> to MAXEVAL')
        self.assertEqual(n('step_wall=2m 7s  epoch_wall=59.5s'),
                         'step_wall=<duration> epoch_wall=<duration>')
        # A number of minutes alone is no duration the journal writes.
        self.assertEqual(n('every 5m'), 'every 5m')


class MatchTest(unittest.TestCase):
    """nfp_files.match_quote(lines, output, tolerance): None, or why the
    quote is not the output."""

    def match(self, text, output=JOURNAL, tolerance=None):
        return nfp_files.match_quote(quote(text), output, tolerance)

    def test_lines_in_order_with_any_blanks_and_times(self):
        self.assertIsNone(self.match("""\
            -- RESULT --------------------------------------------------------------
              Optimiser status   :   TADAH:GRID: all grid points evaluated
              Best total loss         : 2.3946e-01  (eval 6)
            """))
        # Another clock time, date and duration than the run's.
        self.assertIsNone(self.match("""\
            [10:00:00]  BEST     * eval=     6  loss=2.3946e-01  ERMSE=0.49 meV/at  (99 ms)
            """))
        self.assertIsNone(self.match('Finished : 1999-01-01 00:00:00'))

    def test_a_wall_time_in_minutes_matches_any_duration(self):
        # Pins: a quote that gives the wall time of a run in minutes and
        # seconds matches a journal whose run took a time in seconds or in
        # hours. Before the fix: the minutes of the quote, "2m", are text
        # the journal line does not have ('line 1 of the quote').
        self.assertIsNone(self.match('Total wall time : 2m 5s'))
        output = JOURNAL.replace('Total wall time         : 0.2s',
                                 'Total wall time         : 1h 3m 7s')
        self.assertIsNone(self.match('Total wall time : 2m 5s', output))

    def test_consecutive_lines_must_be_consecutive_in_the_output(self):
        message = self.match("""\
              Optimiser status        : TADAH:GRID: all grid points evaluated
              Best potential saved    : best_pot.tadah
            """)
        self.assertIsNotNone(message)
        self.assertIn('line 2 of the quote', message)
        self.assertIn('Best potential saved : best_pot.tadah', message)

    def test_a_line_of_dots_elides_lines(self):
        self.assertIsNone(self.match("""\
              Tadah version  : 1.6.0
            ...
              Optimiser status        : TADAH:GRID: all grid points evaluated
            ...
              Best potential saved    : best_pot.tadah
            """))
        # In order only.
        message = self.match("""\
              Best potential saved    : best_pot.tadah
            ...
              Tadah version  : 1.6.0
            """)
        self.assertIn('line 3 of the quote', message)

    def test_the_first_differing_line_and_the_closest_output_line(self):
        message = self.match("""\
            -- RESULT --------------------------------------------------------------
              Optimiser status        : TADAH:GRID: some grid points evaluated
            """)
        self.assertIn('line 2 of the quote', message)
        self.assertIn('"Optimiser status : TADAH:GRID: some grid points '
                      'evaluated"', message)
        self.assertIn('closest output line 20: "  Optimiser status        : '
                      'TADAH:GRID: all grid points evaluated"', message)

    def test_numbers_within_two_units_of_their_last_digit(self):
        line = '  Best total loss         : {}  (eval {})'
        self.assertIsNone(self.match(line.format('2.3948e-01', 6)))
        self.assertIsNone(self.match(line.format('2.3944e-01', 6)))
        self.assertIsNotNone(self.match(line.format('2.3949e-01', 6)))
        # An integer is exact.
        self.assertIsNotNone(self.match(line.format('2.3946e-01', 7)))
        # 0.49 within 0.02; 100.0 within 0.2.
        self.assertIsNone(self.match(
            'BEST top: ERMSE_global=2.39e-01(100.2%)'))
        self.assertIsNotNone(self.match(
            'BEST top: ERMSE_global=2.39e-01(100.3%)'))
        # A word with digits is text: 1.6.0, RCUT2B.
        self.assertIsNotNone(self.match('  Tadah version  : 1.6.1'))

    def test_a_stated_tolerance_widens_the_number_check(self):
        line = '  Best total loss         : 2.3970e-01  (eval 6)'
        self.assertIsNotNone(self.match(line))
        tol = check_nfp_run.Tolerance(1e-2, False)
        self.assertIsNone(self.match(line, tolerance=tol))
        self.assertIsNotNone(self.match(
            '  Best total loss         : 2.3970e-01  (eval 7)', tolerance=tol))
        self.assertIsNone(self.match(line, tolerance=check_nfp_run.Tolerance(
            3e-4, True)))
        self.assertIsNotNone(self.match(line, tolerance=check_nfp_run.Tolerance(
            1e-4, True)))

    def test_the_time_column_may_be_left_out(self):
        self.assertIsNone(self.match("""\
            BEST     * eval=     6  loss=2.3946e-01  ERMSE=0.49 meV/at  (10 ms)
                        BEST       top: ERMSE_global=2.39e-01(100.0%)
            """))
        # The level column may not.
        self.assertIsNotNone(self.match(
            '* eval=     6  loss=2.3946e-01  ERMSE=0.49 meV/at  (10 ms)'))

    def test_a_long_line_wrapped_with_a_continuation_indent(self):
        self.assertIsNone(self.match("""\
            [22:56:41]  INFO      GRID search: 11 points in each dimension
                (PARAM GRID_POINTS; spacing = width/10), 1 dimension.
            [22:56:41]  BEST     * eval=     1  loss=5.0095e-01  ERMSE=0.71
                meV/at  (14 ms)
            """))
        # A continuation line is indented four blanks more than the first
        # line of its output line.
        self.assertIsNotNone(self.match("""\
            [22:56:41]  INFO      GRID search: 11 points in each dimension
                  (PARAM GRID_POINTS; spacing = width/10), 1 dimension.
            """))
        # An output line that is itself indented is quoted as it is.
        output = 'Params:\n    GRID_POINTS : 11\nStop criteria:\n'
        self.assertIsNone(self.match("""\
            Params:
                GRID_POINTS : 11
            Stop criteria:
            """, output=output))

    def test_the_wrap_of_a_long_line_joins_back(self):
        text = ('the line ends with \\, which continues it on the next line, '
                'and the file has no next line; a second sentence of words '
                'follows it')
        lines = nfp_files.wrap_output(text)
        self.assertGreater(len(lines), 1)
        self.assertTrue(all(len(l) <= 80 for l in lines))
        self.assertTrue(all(l.startswith('    ') for l in lines[1:]))
        self.assertIsNone(nfp_files.match_quote(lines, text))
        self.assertEqual(nfp_files.wrap_output('short'), ['short'])
        # Each line of a text of several lines is wrapped on its own, its
        # continuation indented four blanks more than it.
        two = '  ' + 'x ' * 50 + '\nnext'
        lines = nfp_files.wrap_output(two)
        self.assertEqual(lines[-1], 'next')
        self.assertTrue(lines[1].startswith('      x'))
        self.assertIsNone(nfp_files.match_quote(lines, two))

    def test_the_wrap_breaks_only_lines_longer_than_eighty(self):
        eighty = 'w ' * 39 + 'ww'
        self.assertEqual(len(eighty), 80)
        self.assertEqual(nfp_files.wrap_output(eighty), [eighty])
        lines = nfp_files.wrap_output(eighty + 'w')
        self.assertEqual(lines, ['w ' * 38 + 'w', '    www'])
        self.assertIsNone(nfp_files.match_quote(lines, eighty + 'w'))
        # A word longer than a line is not broken: the line stays longer
        # than 80 characters, which the width check of the pages refuses.
        word = 'x' * 90
        self.assertEqual(nfp_files.wrap_output('a ' + word),
                         ['a', '    ' + word])

    def test_a_continuation_that_is_dots_is_not_an_elision(self):
        # An output line broken before its last word, "...": the
        # continuation line is "    ..." and joins the line before it.
        text = ' '.join(['word'] * 15) + ' abcde ...'
        lines = nfp_files.wrap_output(text)
        self.assertEqual(lines, [text[:-4], '    ...'])
        self.assertIsNone(nfp_files.match_quote(lines, text))
        self.assertIsNotNone(nfp_files.match_quote(
            lines, text[:-4] + '\nother\n'))
        # A line "..." at the indent of the line before it elides.
        self.assertIsNone(nfp_files.match_quote(
            ['  first', '  ...', '  last'], '  first\n  middle\n  last\n'))

    def test_a_number_is_written_as_the_program_writes_it(self):
        line = '  Best total loss         : {}  (eval 6)'
        # Fewer digits, another notation, another exponent width: another
        # text than the program's, though the value is within tolerance.
        for written in ('2.395e-01', '0.23946', '2.3946e-1', '2.39460e-01'):
            with self.subTest(written=written):
                self.assertIsNotNone(self.match(line.format(written)))
        self.assertIsNotNone(self.match(
            line.format('2.395e-01'),
            tolerance=check_nfp_run.Tolerance(1e-2, False)))
        # 100.0 is not 100, and 0.71 is not +0.71.
        output = 'ratio 100 of 0.71\n'
        self.assertIsNotNone(self.match('ratio 100.0 of 0.71', output))
        self.assertIsNotNone(self.match('ratio 100 of +0.71', output))
        self.assertIsNone(self.match('ratio 100 of 0.72', output))
        # A sign the program writes may change with the value.
        self.assertIsNone(self.match('shift -0.01', 'shift +0.01\n',
                                     check_nfp_run.Tolerance(0.05, True)))

    def test_a_full_precision_number_may_end_at_another_digit(self):
        # Pins: a number that the quote and the output both write with 15
        # or more significant digits (a double written in full, whose last
        # digits a rounding changes, and with them their count) matches
        # whatever its count of digits after the point, when its value is
        # within the marker's tol= (without one, within two units of the
        # last digit, which such a change exceeds). A number with fewer
        # digits keeps the program's form (above). Before the fix: the
        # quote 17.519476792114382 is not the output 17.51947679210062
        # whatever the tolerance, as the two write 15 and 14 digits after
        # the point ('line 1 of the quote').
        line = 'the PC_FAILPROBE value {} is above its --gate 0'
        output = line.format('17.51947679210062') + '\n'
        quoted = line.format('17.519476792114382')
        tol = check_nfp_run.Tolerance(1e-2, False)
        self.assertIsNone(self.match(quoted, output, tol))
        # In exponent form as well, with an exponent of as many digits.
        self.assertIsNone(self.match(line.format('3.5756347686298473e-05'),
                                     line.format('3.57563476862985e-05')
                                     + '\n', tol))
        # Without a tol=, or outside it, the values differ.
        self.assertIsNotNone(self.match(quoted, output))
        self.assertIsNotNone(self.match(line.format('17.719476792114382'),
                                        output, tol))
        # Another notation, or fewer than 15 significant digits on either
        # side, is another form.
        self.assertIsNotNone(self.match(line.format('1.7519476792114382e+01'),
                                        output, tol))
        self.assertIsNotNone(self.match(line.format('17.5194767921'),
                                        output, tol))

    def test_a_quote_with_elisions_that_fails_ends_in_time(self):
        # Each "..." lets the next lines start anywhere after the lines
        # before them; on an output of repeated lines a quote whose last
        # line has no place ends in time, not after trying every order.
        output = 'INFO repeated\n' * 400
        quoted = ['INFO repeated', '...'] * 5 + ['nothing here']
        signal.signal(signal.SIGALRM, _too_slow)
        signal.alarm(20)
        try:
            started = time.monotonic()
            message = nfp_files.match_quote(quoted, output)
            elapsed = time.monotonic() - started
        finally:
            signal.alarm(0)
        self.assertIn('line 11 of the quote, "nothing here"', message)
        self.assertLess(elapsed, 5.0)


def _too_slow(signum, frame):
    raise AssertionError('match_quote took longer than 20 s')


# A page whose search the guide pages quote.
QUOTED_PAGE = page_of(
    'tadah hpo --config ../../data/config.train --hpotarget config.hpo',
    'tadah predict -p best_pot.tadah -d ../../data/ta.tadahdb -A')


def marked(marker, *lines):
    """A marked quote: the marker and its text code block."""
    return (f'.. output: {marker}\n\n.. code-block:: text\n\n'
            + ''.join(f'   {l}\n' if l else '\n' for l in lines))


class QuoteRunTest(RunFixture):
    """The run of a manifest line checks every quote that names its id,
    on any page under nfp/ and in training.rst."""

    def setUp(self):
        super().setUp()
        self.page(QUOTED_PAGE)
        self.fake({'default': {
            'journal': JOURNAL.splitlines(),
            'stdout': ['[tadah hpo] TADAH:GRID   dim=1',
                       '[tadah hpo] best_pot.tadah:'],
            'stderr': ['WARNING: a stand-in warning on the error stream']}})

    def source_page(self, rel, text):
        path = os.path.join(self.tmp, 'source', rel)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, 'w', encoding='utf-8') as fh:
            fh.write(text)

    def test_quotes_of_files_and_streams_on_any_page(self):
        self.source_page('nfp/guide.rst', 'Guide\n=====\n\n' + marked(
            'unit1 hpo_run.log',
            '-- RESULT --------------------------------------------------------------',
            '  Optimiser status        : TADAH:GRID: all grid points evaluated')
            + '\n' + marked('unit1 stdout',
                            '[tadah hpo] TADAH:GRID   dim=1')
            + '\n' + marked('unit1 stderr',
                            'WARNING: a stand-in warning on the error stream')
            + '\n' + marked('unit1 stdout command=2',
                            'Energy RMSE (meV/atom)      5.000000e-01'
                            '    5.000000e-01')
            # Another id: not checked by this line.
            + '\n' + marked('other hpo_run.log', 'not in the journal'))
        self.source_page('training.rst', 'Training\n========\n\n' + marked(
            'unit1 best_pot.tadah', '# HPO_BEST_LOSS 0.5'))
        status, out = self.run_check()
        self.assertEqual(status, 0, out)
        for line in (6, 13, 19, 25):
            self.assertIn(f'ok: quote nfp/guide.rst:{line} ', out)
        self.assertIn('ok: quote training.rst:6 (best_pot.tadah of run 1 '
                      '(config.hpo))', out)
        self.assertNotIn('nfp/guide.rst:31', out)

    def test_a_quote_that_differs_fails_the_run(self):
        self.source_page('nfp/guide.rst', 'Guide\n=====\n\n' + marked(
            'unit1 hpo_run.log',
            '  Optimiser status        : TADAH:GRID: no grid point evaluated'))
        status, out = self.run_check()
        self.assertEqual(status, 1, out)
        self.assertIn('FAIL: quote nfp/guide.rst:6 (hpo_run.log of run 1 '
                      '(config.hpo)): line 1 of the quote', out)
        self.assertIn('closest output line', out)
        self.assertIn('TADAH:GRID: all grid points evaluated', out)

    def test_a_quote_of_the_error_stream_is_not_on_standard_output(self):
        self.source_page('nfp/guide.rst', 'Guide\n=====\n\n' + marked(
            'unit1 stdout', 'WARNING: a stand-in warning on the error stream'))
        status, out = self.run_check()
        self.assertEqual(status, 1, out)
        self.assertIn('FAIL: quote nfp/guide.rst:6 (stdout of', out)

    def test_a_stated_tolerance_of_the_line(self):
        self.manifest('best_loss=1e-2')
        self.source_page('nfp/guide.rst', 'Guide\n=====\n\n' + marked(
            'unit1 hpo_run.log tol=best_loss',
            '  Best total loss         : 2.3750e-01  (eval 6)'))
        status, out = self.run_check()
        self.assertEqual(status, 0, out)
        # The quote is compared, within the line's best_loss tolerance:
        # 2.3946e-01 is 0.8 % above 2.3750e-01 and 1.9 % above 2.3500e-01.
        self.assertIn('ok: quote nfp/guide.rst:6 (hpo_run.log of run 1 '
                      '(config.hpo))', out)
        self.source_page('nfp/guide.rst', 'Guide\n=====\n\n' + marked(
            'unit1 hpo_run.log tol=best_loss',
            '  Best total loss         : 2.3500e-01  (eval 6)'))
        status, out = self.run_check()
        self.assertEqual(status, 1, out)
        self.assertIn('FAIL: quote nfp/guide.rst:6 (hpo_run.log of run 1 '
                      '(config.hpo)): line 1 of the quote', out)


class QuoteCoverageTest(CoverageTest):
    """A marker that names an unknown id, source, search or command is a
    problem of the static check (coverage)."""

    def guide(self, *markers):
        self.put('nfp/guide.rst', 'Guide\n=====\n\n' + '\n'.join(
            marked(m, 'output') for m in markers))

    def test_markers_that_name_what_the_line_runs_pass(self):
        self.guide('unit1 hpo_run.log', 'unit1 stdout command=1',
                   'unit1 stdout', "unit1 loss.tadah search='run 1'",
                   'unit1 hpo_run.log search=config.hpo tol=1e-3')
        self.put('training.rst', marked('unit1 stderr command=2', 'x'))
        self.assertEqual(self.coverage(), [])
        # Every marker is read, and each resolves on the line it names.
        manifest = os.path.join(self.files, 'manifest.txt')
        guide = os.path.join(self.source, 'nfp', 'guide.rst')
        self.assertEqual(len(nfp_files.quote_blocks(guide)), 5)
        self.assertEqual(check_nfp_run.quote_problems(manifest), [])

    def test_unknown_ids_sources_searches_and_commands(self):
        self.manifest('unit1 | nfp/tutorial/unit1.rst | tutorial/unit1 | - | 60 '
                      '| best_loss=1e-3')
        self.guide('nosuch hpo_run.log', 'unit1 out.log',
                   'unit1 stdout command=3', 'unit1 hpo_run.log search=x.hpo',
                   'unit1 hpo_run.log tol=start_loss')
        self.put('training.rst', '.. output: unit1 stdout\n\nText.\n')
        problems = self.coverage()
        self.assertEqual(len(problems), 6, problems)
        self.assertRegex(problems[0], r'^nfp/guide\.rst:4: the output marker '
                         r'names "nosuch", which is not an id of .*manifest\.txt')
        self.assertRegex(problems[1], r'^nfp/guide\.rst:10: the source '
                         r'"out\.log" is not stdout, stderr or a file of a '
                         r'search')
        self.assertRegex(problems[2], r'^nfp/guide\.rst:16: command=3, and '
                         r'nfp/tutorial/unit1\.rst has 2 tadah command\(s\) '
                         r'\(the line unit1\)')
        self.assertRegex(problems[3], r'^nfp/guide\.rst:22: .*names the search '
                         r'"x\.hpo", which nfp/tutorial/unit1\.rst does not run')
        self.assertRegex(problems[4], r'^nfp/guide\.rst:28: tol=start_loss, '
                         r'for which the line unit1 gives no tolerance '
                         r'\(its tolerances: best_loss\)')
        self.assertRegex(problems[5], r'^training\.rst:1: an output marker is '
                         r'followed by "Text\.", not by "\.\. code-block:: '
                         r'text"')

    def test_a_source_of_a_search_needs_one(self):
        # A line that runs two searches: a file or the output of a search
        # names one of them; a line that runs no search has no such file.
        self.put('nfp/tutorial/unit1.rst', page_of(
            'tadah hpo --config c.train --hpotarget config.hpo',
            'tadah hpo --config c.train --hpotarget other.hpo'))
        self.guide('unit1 hpo_run.log', 'unit1 stdout',
                   'unit1 hpo_run.log search=other.hpo')
        problems = self.coverage()
        self.assertEqual(len(problems), 2, problems)
        for p in problems:
            self.assertIn('names no search, and the line unit1 runs 2 '
                          '(run 1 (config.hpo), run 2 (other.hpo)): add '
                          'search=', p)

    def test_a_line_that_names_its_searches_runs_only_their_commands(self):
        self.put('nfp/tutorial/unit1.rst', page_of(
            'tadah hpo --config c.train --hpotarget config.hpo',
            'tadah hpo --config c.train --hpotarget other.hpo'))
        self.manifest('unit1 | nfp/tutorial/unit1.rst | tutorial/unit1 | - | 60 '
                      '| - | other.hpo')
        self.guide('unit1 stdout command=1', 'unit1 hpo_run.log search=config.hpo',
                   'unit1 hpo_run.log')
        problems = self.coverage()
        self.assertEqual(len(problems), 2, problems)
        self.assertIn('command=1, which the line unit1 does not run (it runs '
                      'the searches other.hpo)', problems[0])
        self.assertIn('search=config.hpo, which the line unit1 does not run',
                      problems[1])


class ToleranceClassTest(CoverageTest):
    """check_nfp_run.precision_problems(): the tolerances of the manifest
    and of the quote markers against the classes of what they check, and
    the digits of the values of expected.csv against their tolerances."""

    def setUp(self):
        super().setUp()
        self.put('nfp/files/dft_reference.csv',
                 'quantity,value,unit\nEcoh,8.11398,eV/atom\na0,3.32011,Å\n')

    def expected(self, *rows):
        self.put('nfp/files/tutorial/unit1/expected.csv',
                 'Quantity,Value\n' + ''.join(f'{q},{v}\n' for q, v in rows))

    def line(self, tolerances):
        self.manifest('unit1 | nfp/tutorial/unit1.rst | tutorial/unit1 | - | 60 '
                      f'| {tolerances}')

    def problems(self):
        return check_nfp_run.precision_problems(
            os.path.join(self.files, 'manifest.txt'))

    ROWS = (('Best loss', '0.23946'), ('Start loss', '0.50095'),
            ('Best RCUT2B (Å)', '5.000'), ('Best SGRIDMB (Å^-2)', '0.1510'),
            ('Best ERMSE (meV/atom)', '0.489'),
            ('Energy RMSE val (meV/atom)', '10.89'),
            ('Coefficient 2 of first.tadah (eV)', '-1.505'),
            ('Coefficient 3 of first.tadah (eV)', '0'),
            ('Calibrated T_HOT', '0.3088'), ('Evaluations', '177'),
            ('Best a0 (Å)', '3.321'), ('Best Ecoh - DFT (eV/atom)', '0.00'),
            ('Best a0 - DFT (Å)', '0.001'))
    CLASS = ('best_loss=1e-3, start_loss=1e-3, best_rcut2b=abs:0.01, '
             'best_sgridmb=0.01, best_ermse=0.01, energy_rmse=0.01, '
             'coefficient=0.01, calibrated_t_hot=0.01, evaluations=abs:0, '
             'best_a0=abs:0.01, best_ecoh_dft=abs:0.09, best_a0_dft=abs:0.01')

    def test_tolerances_of_their_class_pass(self):
        # Pins: a loss within 1e-3, a length (RCUT2B, a0) within 0.01 Å, a
        # width, an error, a coefficient and a temperature within 1e-2, a
        # count exactly, and a "- DFT" row within the class of its
        # property (Ecoh: 1e-2 of 8.114 eV/atom, a0: 0.01 Å) pass; so do a
        # looser tolerance and a length within a relative tolerance that
        # is 0.01 Å or more. Before the fix: check_nfp_run has no
        # precision_problems (AttributeError).
        self.expected(*self.ROWS)
        self.line(self.CLASS)
        self.assertEqual(self.problems(), [])
        self.line(self.CLASS.replace('best_loss=1e-3', 'best_loss=0.01')
                  .replace('best_rcut2b=abs:0.01', 'best_rcut2b=0.002'))
        self.assertEqual(self.problems(), [])

    def test_a_tolerance_tighter_than_its_class_is_refused(self):
        # Pins: each tolerance tighter than the class of its row is a
        # problem that names the manifest line, the tolerance, the row, the
        # limit it gives and the class: a loss (1e-4 < 1e-3), a length
        # absolute (1e-3 Å) or relative (1e-3 of 5.0 Å = 0.005 Å < 0.01 Å),
        # a width, an outvar column, an error, a coefficient and a
        # temperature (1e-3 < 1e-2) and a "- DFT" row (abs:0.05 < 1e-2 of
        # the property, Ecoh = 0.00 + 8.11398 eV/atom). Before the fix:
        # check_nfp_run has no precision_problems (AttributeError).
        self.expected(*self.ROWS)
        tight = {'best_loss': '1e-4', 'start_loss': '1e-4',
                 'best_rcut2b': 'abs:1e-3', 'best_sgridmb': '1e-3',
                 'best_ermse': '1e-3', 'energy_rmse': '1e-3',
                 'coefficient': '1e-3', 'calibrated_t_hot': '1e-3',
                 'best_a0': 'abs:0.005', 'best_ecoh_dft': 'abs:0.05',
                 'best_a0_dft': 'abs:1e-3'}
        for name, value in tight.items():
            with self.subTest(tolerance=name):
                self.line(', '.join(
                    f'{n}={value}' if n == name else f'{n}={v}'
                    for n, _, v in (i.partition('=')
                                    for i in self.CLASS.split(', '))))
                problems = self.problems()
                self.assertEqual(len(problems), 1, problems)
                self.assertRegex(problems[0], r'manifest\.txt:2: ' + name.replace(
                    '[', r'\[') + '=' + value + ' gives ".*" a tolerance of .*, '
                    r'tighter than its class \(')
        self.line(self.CLASS.replace('best_rcut2b=abs:0.01',
                                     'best_rcut2b=1e-3'))
        problems = self.problems()
        self.assertEqual(len(problems), 1, problems)
        self.assertIn('best_rcut2b=1e-3 gives "Best RCUT2B (Å)" a tolerance of '
                      '0.005 Å, tighter than its class (0.01 Å, a length)',
                      problems[0])
        self.line(self.CLASS.replace('best_ecoh_dft=abs:0.09',
                                     'best_ecoh_dft=abs:0.05'))
        problems = self.problems()
        self.assertEqual(len(problems), 1, problems)
        self.assertIn('best_ecoh_dft=abs:0.05 gives "Best Ecoh - DFT (eV/atom)" '
                      'a tolerance of 0.05 eV/atom, tighter than its class '
                      '(1e-2 of the property, 0.0811 eV/atom)', problems[0])

    def test_a_value_with_more_than_two_digits_past_its_tolerance(self):
        # Pins: a value of expected.csv written with more than two digits
        # past its tolerance (the digits below the leading digit of the
        # limit the tolerance gives it) is a problem, in plain or
        # exponent form; one or two digits past, fewer digits, a count and
        # an exact zero are not. Before the fix: check_nfp_run has no
        # precision_problems (AttributeError).
        self.line('best_loss=1e-3, best_rcut2b=abs:0.01, evaluations=abs:0, '
                  'coefficient=0.01')
        cases = {
            ('0.2394619', '5.000'): ['"Best loss" 0.2394619: 3 digits past '
                                     'its tolerance (0.000239)'],
            ('2.394619e-01', '5.0'): ['"Best loss" 2.394619e-01: 3 digits '
                                      'past its tolerance (0.000239)'],
            ('0.239462', '5.00000'): ['"Best RCUT2B (Å)" 5.00000: 3 digits '
                                      'past its tolerance (0.01)'],
            ('0.23946', '5'): [],
        }
        for (loss, rcut), want in cases.items():
            with self.subTest(loss=loss, rcut=rcut):
                self.expected(('Best loss', loss), ('Best RCUT2B (Å)', rcut),
                              ('Evaluations', '177'),
                              ('Coefficient 3 of first.tadah (eV)', '0'))
                problems = self.problems()
                self.assertEqual(len(problems), len(want), problems)
                for p, w in zip(problems, want):
                    self.assertIn(w, p)
                    self.assertRegex(p, r'tutorial/unit1/expected\.csv: row')

    def test_a_quote_tolerance_tighter_than_every_class_is_refused(self):
        # Pins: a tol= of a quote marker that is a number is at least the
        # tolerance of the tightest class, 1e-3 relative or 0.01 absolute;
        # a named tol= takes the line's tolerance, which the manifest
        # check judges. Before the fix: check_nfp_run has no
        # precision_problems (AttributeError).
        self.expected(('Best loss', '0.23946'))
        self.line('best_loss=1e-3')
        self.put('nfp/guide.rst', 'Guide\n=====\n\n' + '\n'.join(
            marked(m, 'output') for m in (
                'unit1 hpo_run.log tol=1e-3', 'unit1 hpo_run.log tol=abs:0.01',
                'unit1 hpo_run.log tol=best_loss', 'unit1 hpo_run.log tol=1e-4',
                'unit1 hpo_run.log tol=abs:0.005')))
        problems = self.problems()
        self.assertEqual(len(problems), 2, problems)
        self.assertRegex(problems[0], r'^nfp/guide\.rst:22: tol=1e-4 is tighter '
                         r'than every tolerance class \(1e-3 relative, 0\.01 '
                         r'absolute\)')
        self.assertRegex(problems[1], r'^nfp/guide\.rst:28: tol=abs:0\.005 is '
                         r'tighter than every tolerance class')

    def test_a_quote_tolerance_is_judged_on_each_number_it_covers(self):
        # Pins: the tolerance of a quote marker, a number or the name of a
        # tolerance of the line (judged by the value the line gives it),
        # gives each number of the quote that has a point or an exponent
        # at least 1e-3 of its value, the tightest class: abs:0.01 of the
        # line's best_rcutmb is 2e-4 of the share 48.0 and is refused,
        # named or written as a number; tol=2e-2, the line's best_loss
        # (1e-3) and abs:0.1 are not, and an integer (the evaluation 100)
        # is text a quote gives exactly. Before the fix: a named tol= is
        # left to the manifest check, which judges best_rcutmb only by the
        # row Best RCUTMB, and abs:0.01 passes as at least 0.01 absolute:
        # no problem is listed.
        self.expected(('Best loss', '0.23946'), ('Best RCUTMB (Å)', '6.988'))
        self.line('best_loss=1e-3, best_rcutmb=abs:0.01')
        text = ('BEST top: FRMSE_global=2.94e+00(48.0%)',
                'Bound occupancy of the final best (eval 100)')
        self.put('nfp/guide.rst', 'Guide\n=====\n\n' + '\n'.join(
            marked(m, *text) for m in (
                'unit1 hpo_run.log tol=best_rcutmb',
                'unit1 hpo_run.log tol=abs:0.01',
                'unit1 hpo_run.log tol=2e-2', 'unit1 hpo_run.log tol=best_loss',
                'unit1 hpo_run.log tol=abs:0.1')))
        problems = self.problems()
        self.assertEqual(len(problems), 2, problems)
        self.assertRegex(problems[0], r'^nfp/guide\.rst:4: tol=best_rcutmb '
                         r'\(abs:0\.01 of the line unit1\) gives 48\.0 a '
                         r'tolerance of 0\.01, below 1e-3 of the number, the '
                         r'tightest tolerance class$')
        self.assertRegex(problems[1], r'^nfp/guide\.rst:11: tol=abs:0\.01 '
                         r'gives 48\.0 a tolerance of 0\.01, below 1e-3 of the '
                         r'number, the tightest tolerance class$')


if __name__ == '__main__':
    unittest.main()
