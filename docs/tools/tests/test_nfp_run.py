"""Tests for the runs of the documentation's commands:
tests/doc_commands/doc_commands.py (the commands of an rst file's bash code
blocks) and tests/nfp_docs/check_nfp_run.py (the runs the Nested Fitting
manifest lists, tadah_nfp_<id>).

The runs use a stand-in for tadah, a Python script that writes the files
tadah writes and records each call; no tadah build is needed.

Run: python3 -m unittest discover -s docs/tools/tests -v
"""
import contextlib
import io
import os
import shutil
import stat
import subprocess
import sys
import tempfile
import textwrap
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, '..', '..', '..'))
sys.path.insert(0, os.path.join(REPO, 'tests', 'doc_commands'))
sys.path.insert(0, os.path.join(REPO, 'tests', 'nfp_docs'))
sys.path.insert(0, os.path.join(REPO, 'tests', 'cli_examples'))
import check_cli_example  # noqa: E402
import check_nfp_run  # noqa: E402
import doc_commands  # noqa: E402

MANIFEST = os.path.join(REPO, 'docs', 'source', 'nfp', 'files', 'manifest.txt')

# The stand-in for tadah. In its working directory it appends its
# arguments to calls.log and, for `hpo`, writes best_pot.tadah (with the
# loss of the file `loss`, else 0.5), best_loss.tadah and hpo_run.log;
# for `train`, the potential (-o or pot.tadah). An argument of --config
# must name an existing file. The file `exit` sets its exit status, and
# the file `no_best` makes `hpo` write no best_pot.tadah.
FAKE_TADAH = '''#!{python}
import os, sys
args = sys.argv[1:]
with open('calls.log', 'a') as fh:
    fh.write(os.getcwd() + ' ' + ' '.join(args) + '\\n')
if '--config' in args and not os.path.isfile(args[args.index('--config') + 1]):
    print('no such configuration'); sys.exit(3)
if os.path.isfile('exit'):
    print('stand-in fails'); sys.exit(int(open('exit').read()))
if args[:1] == ['hpo']:
    loss = open('loss').read().strip() if os.path.isfile('loss') else '0.5'
    if not os.path.isfile('no_best'):
        open('best_pot.tadah', 'w').write('# HPO_BEST_FORMAT 2\\n# HPO_BEST_LOSS '
                                          + loss + '\\nMODEL M_BLR BF_Linear\\n')
    open('best_loss.tadah', 'w').write('#IMPROVEMENT EVAL TOTAL\\n1 1 ' + loss + '\\n')
    open('hpo_run.log', 'w').write('log\\n')
elif args[:1] == ['train']:
    out = args[args.index('-o') + 1] if '-o' in args else 'pot.tadah'
    open(out, 'w').write('MODEL M_BLR BF_Linear\\n')
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
            'a | p.rst | d | - | 10': '5 fields',
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

    def test_a_malformed_manifest_is_reported(self):
        self.manifest('unit1 | nfp/tutorial/unit1.rst | tutorial/unit1')
        problems = self.coverage()
        self.assertEqual(len(problems), 1, problems)
        self.assertIn('3 fields, expected 6', problems[0])


class RunTest(unittest.TestCase):
    '''A docs tree in a scratch directory: source/nfp/files/{manifest.txt,
    data/config.train, tutorial/unit1/...} and source/nfp/unit1.rst.'''

    def setUp(self):
        self.tmp = tempfile.mkdtemp(prefix='tadah_nfp_run_')
        self.addCleanup(shutil.rmtree, self.tmp)
        self.files = os.path.join(self.tmp, 'source', 'nfp', 'files')
        self.unit = os.path.join(self.files, 'tutorial', 'unit1')
        os.makedirs(self.unit)
        os.makedirs(os.path.join(self.files, 'data'))
        self.put('data/config.train', 'DBFILE ta.db\n')
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

    def manifest(self, tolerances, labels='-', timeout=60):
        self.put('manifest.txt', '# runs\nunit1 | nfp/unit1.rst | tutorial/unit1 '
                 f'| {labels} | {timeout} | {tolerances}\n')

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

    def test_a_run_within_the_tolerance_passes(self):
        before = self.tree()
        status, out = self.run_check()
        self.assertEqual(status, 0, out)
        self.assertIn('ok: best_loss 0.5, expected 0.5 within relative 1e-06', out)
        self.assertIn('PASS: 2 command(s) of nfp/unit1.rst', out)
        # Both commands ran, in order, in the scratch copy of the
        # directory, where ../../data resolves.
        run_dir = os.path.join(self.work, 'files', 'tutorial', 'unit1')
        with open(os.path.join(run_dir, 'calls.log'), encoding='utf-8') as fh:
            calls = fh.read().splitlines()
        self.assertEqual(calls, [
            run_dir + ' train -c ../../data/config.train -o first.tadah',
            run_dir + ' hpo --config ../../data/config.train --hpotarget config.hpo'])
        # The source tree is not written.
        self.assertEqual(self.tree(), before)

    def test_a_best_loss_outside_the_tolerance_fails(self):
        self.put('tutorial/unit1/loss', '0.5000006')
        status, out = self.run_check()
        self.assertEqual(status, 1, out)
        self.assertIn('FAIL: best_loss 0.5000006, expected 0.5 within relative '
                      '1e-06', out)
        self.assertIn('FAIL: outside the tolerance: best_loss', out)
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


if __name__ == '__main__':
    unittest.main()
