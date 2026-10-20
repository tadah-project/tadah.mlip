"""Tests for tests/cli_examples/check_cli_example.py, the run of a CLI example
the documentation includes (tadah_cli_example_<name>): the files of the
other examples a README's commands read (../<other example>/...), the
potential of `tadah train -o`, and the fit balance tables an example ships
(<potential>.balance.txt), which the run's potentials must have within the
tolerance the script states.

The runs use a stand-in for tadah, a Python script that writes the files
tadah writes and records each call; no tadah build is needed.

Run: python3 -m unittest discover -s docs/tools/tests -v
"""
import contextlib
import io
import os
import shutil
import stat
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, '..', '..', '..'))
sys.path.insert(0, os.path.join(REPO, 'tests', 'doc_commands'))
sys.path.insert(0, os.path.join(REPO, 'tests', 'cli_examples'))
import check_cli_example  # noqa: E402

# The stand-in for tadah. In its working directory it appends its working
# directory and arguments to calls.log; an argument of -c or --config must
# name an existing file; `train` writes the potential (-o or pot.tadah, or
# pot.tadah whatever -o says when the file `ignore_o` exists) with a
# training record that ends with the lines of the file `table` (none
# without it).
FAKE_TADAH = '''#!{python}
import os, sys
args = sys.argv[1:]
with open('calls.log', 'a') as fh:
    fh.write(os.getcwd() + ' ' + ' '.join(args) + '\\n')
for opt in ('-c', '--config'):
    if opt in args and not os.path.isfile(args[args.index(opt) + 1]):
        print('no such configuration: ' + args[args.index(opt) + 1])
        sys.exit(3)
if args[:1] == ['train']:
    out = args[args.index('-o') + 1] if '-o' in args else 'pot.tadah'
    if os.path.isfile('ignore_o'):
        out = 'pot.tadah'
    record = ['DBFILE ../ex_a/data.db']
    if os.path.isfile('table'):
        record += open('table').read().splitlines()
    with open(out, 'w') as fh:
        fh.write('# --- training record (no key is read from it) ---\\n')
        for line in record:
            fh.write('# ' + line + '\\n')
        fh.write('# --- end of training record ---\\nMODEL M_BLR BF_Linear\\n')
'''

README = '''Example B
---------

.. code-block:: bash

   tadah train -c ../ex_a/config.train -o mine.tadah
'''

TABLE = '''Fit balance: LAMBDA 1e-07; 14 coefficients, rank 14
  quantity  dataset      n  sigma     RMSE  unit      r/sigma     share  gamma
  energy    all        100  1       0.1317  meV/atom   0.1317  0.005681  1.073
  force     all      16200  0.1 0  0.01369  eV/Å       0.1369    0.9942  11.93
  prior                                                        1.19e-04  1.001
'''


class CliExampleTest(unittest.TestCase):
    '''Examples ex_a (config.train, data.db) and ex_b (README.rst) in a
    scratch examples/CLI directory.'''

    def setUp(self):
        self.tmp = tempfile.mkdtemp(prefix='tadah_cli_example_')
        self.addCleanup(shutil.rmtree, self.tmp)
        self.cli = os.path.join(self.tmp, 'examples', 'CLI')
        for ex in ('ex_a', 'ex_b'):
            os.makedirs(os.path.join(self.cli, ex))
        self.put('ex_a/config.train', 'DBFILE data.db\n')
        self.put('ex_a/data.db', 'data\n')
        self.put('ex_b/README.rst', README)
        self.tadah = os.path.join(self.tmp, 'tadah')
        with open(self.tadah, 'w', encoding='utf-8') as fh:
            fh.write(FAKE_TADAH.format(python=sys.executable))
        os.chmod(self.tadah, os.stat(self.tadah).st_mode | stat.S_IXUSR)
        self.work = os.path.join(self.tmp, 'work')

    def put(self, rel, text):
        with open(os.path.join(self.cli, rel), 'w', encoding='utf-8') as fh:
            fh.write(text)

    def run_check(self, example='ex_b'):
        out = io.StringIO()
        with contextlib.redirect_stdout(out), \
                contextlib.redirect_stderr(out):
            status = check_cli_example.main([
                '--tadah', self.tadah,
                '--example', os.path.join(self.cli, example),
                '--workdir', self.work, '--nfp', 'ON'])
        return status, out.getvalue()

    def tree(self):
        return sorted(os.path.relpath(os.path.join(d, f), self.cli)
                      for d, _, fs in os.walk(self.cli) for f in fs)

    def test_the_files_of_another_example_are_read_where_they_are(self):
        before = self.tree()
        status, out = self.run_check()
        self.assertEqual(status, 0, out)
        run_dir = os.path.join(self.work, 'ex_b')
        with open(os.path.join(run_dir, 'calls.log'), encoding='utf-8') as fh:
            calls = fh.read().splitlines()
        self.assertEqual(calls, [
            run_dir + ' train -c ../ex_a/config.train -o mine.tadah'])
        self.assertTrue(os.path.isfile(os.path.join(run_dir, 'mine.tadah')))
        # The source tree is not written.
        self.assertEqual(self.tree(), before)

    def test_the_potential_of_the_outfile_option_must_be_written(self):
        self.put('ex_b/ignore_o', '')
        status, out = self.run_check()
        self.assertEqual(status, 1, out)
        self.assertIn('mine.tadah was not written', out)

    def test_a_balance_table_within_its_tolerance_passes(self):
        self.put('ex_b/mine.balance.txt', TABLE)
        # Two units of the last digit of each number (4 significant digits,
        # 3 in exponent form) are within the tolerance.
        self.put('ex_b/table', TABLE.replace('0.1317  meV', '0.1319  meV')
                 .replace('1.19e-04', '1.21e-04').replace('11.93', '11.91'))
        status, out = self.run_check()
        self.assertEqual(status, 0, out)
        self.assertIn('ok: the fit balance table of mine.tadah is '
                      'mine.balance.txt', out)

    def test_a_balance_table_outside_its_tolerance_fails(self):
        self.put('ex_b/mine.balance.txt', TABLE)
        cases = {
            ('0.1317  meV', '0.1320  meV'): 'RMSE 0.1320, expected 0.1317',
            ('1.19e-04', '1.22e-04'): 'share 1.22e-04, expected 1.19e-04',
            ('16200', '16201'): 'n 16201, expected 16200',
            ('0.1 0  0.01369', '0.2 0  0.01369'): 'sigma 0.2 0, expected 0.1 0',
            ('rank 14', 'rank 13'): 'rank 13',
            ('eV/Å ', 'GPa  '): 'unit GPa, expected eV/Å',
            ('  force     all', '  stress    all'): 'stress',
        }
        for (old, new), message in cases.items():
            with self.subTest(old=old, new=new):
                self.assertIn(old, TABLE)
                self.put('ex_b/table', TABLE.replace(old, new))
                status, out = self.run_check()
                self.assertEqual(status, 1, out)
                self.assertIn('FAIL', out)
                self.assertIn(message, out)

    def test_a_table_of_other_rows_fails(self):
        self.put('ex_b/mine.balance.txt', TABLE)
        lines = TABLE.splitlines()
        self.put('ex_b/table', '\n'.join(lines[:3] + lines[4:]) + '\n')
        status, out = self.run_check()
        self.assertEqual(status, 1, out)
        self.assertIn('has 4 lines, mine.balance.txt 5', out)

    def test_a_potential_without_a_table_fails(self):
        self.put('ex_b/mine.balance.txt', TABLE)
        status, out = self.run_check()
        self.assertEqual(status, 1, out)
        self.assertIn('mine.tadah has no fit balance table', out)

    def test_an_expected_table_no_command_writes_fails(self):
        self.put('ex_b/other.balance.txt', TABLE)
        self.put('ex_b/table', TABLE)
        status, out = self.run_check()
        self.assertEqual(status, 1, out)
        self.assertIn('other.balance.txt: no tadah train command of the '
                      'README writes other.tadah', out)


if __name__ == '__main__':
    unittest.main()
