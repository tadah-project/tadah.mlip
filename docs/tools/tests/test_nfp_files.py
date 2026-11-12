"""Tests for the checks of the files the Nested Fitting pages ship:
tests/nfp_docs/nfp_files.py (the DFT values of a target file and of
dft_reference.csv) and tests/nfp_docs/check_nfp_files.py (CTest
tadah_nfp_files_static and tadah_nfp_files_check).

The check of target files runs a stand-in for tadah that records its
calls; no tadah build is needed.

Run: python3 -m unittest discover -s docs/tools/tests -v
"""
import contextlib
import io
import os
import shutil
import stat
import sys
import tempfile
import textwrap
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, '..', '..', '..'))
sys.path.insert(0, os.path.join(REPO, 'tests', 'doc_commands'))
sys.path.insert(0, os.path.join(REPO, 'tests', 'nfp_docs'))
import check_nfp_files  # noqa: E402
import nfp_files  # noqa: E402

REFERENCE = textwrap.dedent('''\
    quantity,value,unit,source,method
    Ecoh,8.11398,eV/atom,OUTCAR,"-(F_bulk/atom - F_atom), MP 18^3"
    a0,3.32011,A,OUTCAR,relaxation
    Evf,2.9873,eV,OUTCAR,vacancy
    C11,264.2,GPa,OUTCAR,stress-strain
    C12,161.5,GPa,OUTCAR,as C11
    C44,74.8,GPa,OUTCAR,as C11
    ''')

TARGET = textwrap.dedent('''\
    # Target file (--hpotarget).
    OPTIM      RCUT2B (1) 6.0 7.5
    PC_LAMMPS  --script ../../lammps/ecoh_a0.in --hotvar a0 3.1 3.5 3.32011 \\
               --varloss Ecoh 8.11398 100 --varloss a0 3.32011 1e4
    # Evf about 0.3 eV (10 %).
    PC_LAMMPS  --script ../../lammps/vacancy.in --invar a0_dft 3.32011 \\
               --invar steps 1000 --varloss Evf 2.9873 10
    PC_FAILPROBE --script ../../lammps/failprobe.in --outvar S --gate 0
    PC_OBSERVABLE cohesive_energy --species Ta --target 8.11398 \\
                  --weight 2.5e5   # a comment after a value

    OPTIMIZER
        LIB      NLOPT
        ALGO     LN_BOBYQA
        THREADS  1
    ENDOPTIMIZER
    ''')


def write(root, rel, text):
    path = os.path.join(root, rel)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w', encoding='utf-8') as fh:
        fh.write(text)
    return path


class TargetFileTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp(prefix='tadah_nfp_files_')
        self.addCleanup(shutil.rmtree, self.tmp)

    def test_lines_join_continuations_and_drop_comments(self):
        path = write(self.tmp, 'config.hpo', TARGET)
        lines = nfp_files.target_lines(path)
        self.assertEqual([l.number for l in lines], [2, 3, 6, 8, 9, 12, 13, 14, 15, 16])
        self.assertEqual(lines[1].words[-3:], ['a0', '3.32011', '1e4'])
        self.assertEqual(lines[4].words[-2:], ['--weight', '2.5e5'])

    def test_the_dft_values_of_a_target_file(self):
        # The --hotvar initial value and the --invar value of a variable
        # with a DFT quantity are DFT values too; the --invar of a variable
        # without one (steps) is an input of the script.
        path = write(self.tmp, 'config.hpo', TARGET)
        self.assertEqual(
            [(t.line, t.where, t.quantity, t.value)
             for t in nfp_files.dft_targets(path)],
            [(3, '--hotvar a0 (initial value)', 'a0', 3.32011),
             (3, '--varloss Ecoh', 'Ecoh', 8.11398),
             (3, '--varloss a0', 'a0', 3.32011),
             (6, '--invar a0_dft', 'a0', 3.32011),
             (6, '--varloss Evf', 'Evf', 2.9873),
             (9, 'PC_OBSERVABLE cohesive_energy --target', 'Ecoh', 8.11398)])
        # A --hotvar without an initial value starts from none.
        path = write(self.tmp, 'other.hpo',
                     'PC_LAMMPS --script e.in --hotvar a0 3.1 3.5 --varloss a0 '
                     '3.32011 1\n')
        self.assertEqual([t.where for t in nfp_files.dft_targets(path)],
                         ['--varloss a0'])

    def test_the_seeds_of_a_target_file(self):
        path = write(self.tmp, 'config.hpo', textwrap.dedent('''\
            OPTIM RCUT2B (1) 6.0 7.5
            OPTIMIZER
                INIT
                    METHOD WARM best_pot.tadah   # the best of the search before
                    METHOD SOBOL 4
                ENDINIT
                LIB NLOPT
            ENDOPTIMIZER
            '''))
        self.assertEqual(nfp_files.warm_seeds(path), [(4, 'best_pot.tadah')])

    def test_the_threads_of_the_run(self):
        # OPTIMIZER, STRATEGY and REFINE set the STRATEGY-level optimiser,
        # whose THREADS sets the workers of the run; the THREADS of an
        # EXPLORE block does not.
        path = write(self.tmp, 'config.hpo', textwrap.dedent('''\
            OPTIM RCUT2B (1) 6.0 7.5
            OPTIMIZER
                STRATEGY
                    TYPE PRESTAGE
                    EXPLORE
                        LIB ASA
                        ALGO ASA
                        THREADS 4   # the points asked for at once
                    ENDEXPLORE
                    REFINE
                        LIB NLOPT
                        THREADS 1
                    ENDREFINE
                ENDSTRATEGY
                THREADS 2
            ENDOPTIMIZER
            '''))
        self.assertEqual(nfp_files.run_threads(path), (2, [12, 15]))
        for text, expected in (
                ('OPTIMIZER\nLIB NLOPT\nENDOPTIMIZER\n', (1, [])),
                ('OPTIMIZER\nSTRATEGY\nTHREADS 1\nENDSTRATEGY\n'
                 'ENDOPTIMIZER\n', (1, [3])),
                ('OPTIMIZER\nSTRATEGY\nTYPE PRESTAGE\nEXPLORE\nTHREADS 4\n'
                 'ENDEXPLORE\nENDSTRATEGY\nENDOPTIMIZER\n', (1, [])),
                ('OPTIMIZER\nSTRATEGY\nINNER\nTHREADS 4\nENDINNER\n'
                 'ENDSTRATEGY\nENDOPTIMIZER\n', (1, [])),
                ('PC_ERMSE 0 1\nTHREADS 1\n', (None, []))):
            with self.subTest(text=text):
                path = write(self.tmp, 'other.hpo', text)
                self.assertEqual(nfp_files.run_threads(path), expected)

    def test_an_unmapped_variable_or_observable_is_an_error(self):
        for line, message in (
                ('PC_LAMMPS --script s.in --varloss B 195.7 1\n',
                 'no quantity of dft_reference.csv is mapped to the variable "B"'),
                ('PC_OBSERVABLE lattice_constant --target 3.32\n',
                 'PC_OBSERVABLE lattice_constant --target: no quantity'),
                ('PC_LAMMPS --script s.in --varloss Ecoh x 1\n',
                 '"x" is not a number')):
            with self.subTest(line=line):
                path = write(self.tmp, 'bad.hpo', '# c\n' + line)
                with self.assertRaises(nfp_files.FilesError) as cm:
                    nfp_files.dft_targets(path)
                self.assertIn(message, str(cm.exception))
                self.assertIn('bad.hpo:2:', str(cm.exception))

    def test_the_reference_table(self):
        path = write(self.tmp, 'dft_reference.csv', REFERENCE)
        ref = nfp_files.read_dft_reference(path)
        self.assertEqual(ref['Ecoh'], nfp_files.Reference(8.11398, 'eV/atom', 2))
        self.assertEqual(sorted(ref), ['C11', 'C12', 'C44', 'Ecoh', 'Evf', 'a0'])
        for text, message in (
                ('q,v\n', 'does not start with "quantity,value,unit"'),
                (REFERENCE + 'a0,3.3,A,x,y\n', 'quantity "a0" is also on line 3'),
                (REFERENCE + 'B,big,GPa,x,y\n', 'the value of "B" is not a number')):
            with self.subTest(text=text[-20:]):
                write(self.tmp, 'dft_reference.csv', text)
                with self.assertRaises(nfp_files.FilesError) as cm:
                    nfp_files.read_dft_reference(path)
                self.assertIn(message, str(cm.exception))


class StaticTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp(prefix='tadah_nfp_static_')
        self.addCleanup(shutil.rmtree, self.tmp)
        self.files = os.path.join(self.tmp, 'files')
        write(self.files, 'manifest.txt', '# runs\n')
        write(self.files, 'dft_reference.csv', REFERENCE)
        write(self.files, 'tutorial/unit4/config.hpo', TARGET)

    def run_static(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            status = check_nfp_files.main(['static', '--files', self.files])
        return status, out.getvalue()

    def test_values_equal_to_the_reference_pass(self):
        status, out = self.run_static()
        self.assertEqual(status, 0, out)
        self.assertIn('PASS: 3 file(s) of at most 1 MiB with LF line endings; 6 '
                      'DFT value(s) of target files equal dft_reference.csv; 0 '
                      'file(s) match their SHA256SUMS; 1 target file(s) that '
                      'run a search set THREADS', out)

    def test_an_initial_or_input_value_other_than_the_reference_fails(self):
        write(self.files, 'examples/pt/config.hpo',
              'PC_LAMMPS --script e.in --hotvar a0 3.1 3.5 3.3 \\\n'
              '          --invar a0_dft 3.32 --varloss a0 3.32011 1\n')
        status, out = self.run_static()
        self.assertEqual(status, 1, out)
        self.assertIn('FAIL: examples/pt/config.hpo:1: --hotvar a0 (initial '
                      'value) 3.3: dft_reference.csv gives a0 3.32011 (line 3)',
                      out)
        self.assertIn('FAIL: examples/pt/config.hpo:1: --invar a0_dft 3.32: '
                      'dft_reference.csv gives a0 3.32011 (line 3)', out)

    def test_data_files_match_their_checksums(self):
        import hashlib
        a = write(self.files, 'data/a.tadahdb', 'Ta 1\n')
        b = write(self.files, 'data/b.tadahdb', 'Ta 2\n')
        write(self.files, 'data/README.md', '# data\n')
        sums = ''.join(hashlib.sha256(open(p, 'rb').read()).hexdigest()
                       + '  ' + os.path.basename(p) + '\n' for p in (a, b))
        write(self.files, 'data/SHA256SUMS', sums)
        status, out = self.run_static()
        self.assertEqual(status, 0, out)
        self.assertIn('2 file(s) match their SHA256SUMS', out)
        # A changed file, a listed file that is gone, a data file that is
        # not listed, a line that is not "<sha256>  <file>".
        write(self.files, 'data/a.tadahdb', 'Ta 1\nTa 3\n')
        os.remove(b)
        write(self.files, 'data/c.tadahdb', 'Ta 4\n')
        write(self.files, 'data/SHA256SUMS', sums + 'abc  d.tadahdb\n')
        status, out = self.run_static()
        self.assertEqual(status, 1, out)
        for message in (
                'FAIL: data/a.tadahdb: its SHA-256 is not the one '
                'data/SHA256SUMS gives (line 1)',
                'FAIL: data/SHA256SUMS:2: b.tadahdb does not exist',
                'FAIL: data/SHA256SUMS:3: "abc  d.tadahdb" is not '
                '"<sha256>  <file>"',
                'FAIL: data/c.tadahdb: data/SHA256SUMS does not list it'):
            self.assertIn(message, out)
        self.assertEqual(out.count('FAIL'), 4, out)

    def test_a_text_file_with_cr_line_endings_fails(self):
        with open(os.path.join(self.files, 'dft_reference.csv'), 'w',
                  encoding='utf-8', newline='\r\n') as fh:
            fh.write(REFERENCE)
        with open(os.path.join(self.files, 'pot.bin'), 'wb') as fh:
            fh.write(b'\x00\x01\r\n')
        status, out = self.run_static()
        self.assertEqual(status, 1, out)
        self.assertIn('FAIL: dft_reference.csv: CR line endings (LF only)', out)
        self.assertNotIn('pot.bin', out)

    def test_a_search_without_threads_fails(self):
        # A target file that runs a search states the workers of the run;
        # the THREADS of an EXPLORE block does not set them, and a file
        # without an OPTIMIZER block runs no search.
        write(self.files, 'examples/a/config.hpo', textwrap.dedent('''\
            # A search on the default workers.
            OPTIM RCUT2B (1) 6.0 7.5
            OPTIMIZER
                LIB NLOPT
                ALGO LN_BOBYQA
            ENDOPTIMIZER
            '''))
        write(self.files, 'examples/b/config.hpo', textwrap.dedent('''\
            OPTIMIZER
                STRATEGY
                    TYPE PRESTAGE
                    EXPLORE
                        LIB DLIB
                        ALGO GFS
                        THREADS 4
                    ENDEXPLORE
                ENDSTRATEGY
            ENDOPTIMIZER
            '''))
        write(self.files, 'examples/c/config.hpo', textwrap.dedent('''\
            OPTIMIZER
                STRATEGY
                    TYPE MULTISTART
                    THREADS 2   # an example about threads
                ENDSTRATEGY
            ENDOPTIMIZER
            '''))
        write(self.files, 'examples/d/part.hpo', 'PC_ERMSE 0 1\n')
        status, out = self.run_static()
        self.assertEqual(status, 1, out)
        for rel, line in (('examples/a/config.hpo', 3),
                          ('examples/b/config.hpo', 1)):
            self.assertIn(f'FAIL: {rel}:{line}: the OPTIMIZER block sets no '
                          'THREADS (in OPTIMIZER, STRATEGY or REFINE): a '
                          'shipped search states its workers, THREADS 1 '
                          'unless its example is about threads', out)
        self.assertEqual(out.count('FAIL'), 2, out)
        for rel in ('examples/a/config.hpo', 'examples/b/config.hpo'):
            os.remove(os.path.join(self.files, rel))
        status, out = self.run_static()
        self.assertEqual(status, 0, out)
        self.assertIn('; 2 target file(s) that run a search set THREADS', out)

    def test_a_value_other_than_the_reference_fails(self):
        write(self.files, 'examples/pt/config.hpo',
              'PC_LAMMPS --script e.in --varloss C11 264 1e-2 \\\n'
              '          --varloss C44 74.8 1e-2\n')
        status, out = self.run_static()
        self.assertEqual(status, 1, out)
        self.assertIn('FAIL: examples/pt/config.hpo:1: --varloss C11 264.0: '
                      'dft_reference.csv gives C11 264.2 (line 5)', out)
        self.assertNotIn('C44', out)

    def test_a_quantity_without_a_reference_fails(self):
        write(self.files, 'dft_reference.csv',
              REFERENCE.replace('Evf,2.9873,eV,OUTCAR,vacancy\n', ''))
        status, out = self.run_static()
        self.assertEqual(status, 1, out)
        self.assertIn('--varloss Evf 2.9873: dft_reference.csv has no Evf', out)
        os.remove(os.path.join(self.files, 'dft_reference.csv'))
        status, out = self.run_static()
        self.assertEqual(status, 1, out)
        self.assertIn('dft_reference.csv does not exist, and target files give '
                      'DFT values', out)

    def test_a_file_larger_than_1_mib_fails(self):
        with open(os.path.join(self.files, 'big.tadahdb'), 'wb') as fh:
            fh.write(b'x' * (1024 * 1024 + 1))
        status, out = self.run_static()
        self.assertEqual(status, 1, out)
        self.assertIn('FAIL: big.tadahdb: 1048577 bytes, more than 1 MiB', out)

    def test_an_empty_tree_passes(self):
        shutil.rmtree(self.files)
        write(self.files, 'manifest.txt', '# runs\n')
        status, out = self.run_static()
        self.assertEqual(status, 0, out)
        self.assertIn('PASS: 1 file(s) of at most 1 MiB with LF line endings; 0 '
                      'DFT value(s)', out)


# Datasets in the format of tadah (a label line, the optional priorities,
# the energy, three cell lines, three stress lines and a line per atom;
# a blank line ends a structure).
def dataset(*atoms_per_structure):
    blocks = []
    for n, atoms in enumerate(atoms_per_structure, 1):
        blocks.append(f'cell {n}\n1 1 1\n-8.1\n3.3 0 0\n0 3.3 0\n0 0 3.3\n'
                      '0 0 0\n0 0 0\n0 0 0\n'
                      + ''.join(f'Ta {i}.0 0 0 0 0 0\n' for i in range(atoms)))
    return '\n'.join(blocks) + '\n'


DATA_PAGE = textwrap.dedent("""\
    The Ta data
    -----------

    Files
    ~~~~~

    :download:`ta_a.tadahdb <../files/data/ta_a.tadahdb>`
       Set A, training: 2 cells, 3 atoms.
    :download:`ta_b.tadahdb <../files/data/ta_b.tadahdb>`
       Set B: 1 cell, 2 atoms.
    :download:`README.md <../files/data/README.md>`
       Credit and licence.

    Reference values
    ~~~~~~~~~~~~~~~~

    Lattice constant ``a0``: 3.32011 A
       Relaxation of the 2-atom cell.
    Elastic constants ``C11``: 264.2 GPa, ``C12``: 161.5 GPa
       Central differences of the stress.
    """)


class DataPageTest(unittest.TestCase):
    """static --data-page: the counts of the datasets and the reference
    values that the page of the Ta data gives."""

    def setUp(self):
        self.tmp = tempfile.mkdtemp(prefix='tadah_nfp_data_page_')
        self.addCleanup(shutil.rmtree, self.tmp)
        self.files = os.path.join(self.tmp, 'files')
        write(self.files, 'manifest.txt', '# runs\n')
        write(self.files, 'dft_reference.csv', REFERENCE)
        write(self.files, 'data/ta_a.tadahdb', dataset(1, 2))
        # Without the line of priorities.
        write(self.files, 'data/ta_b.tadahdb',
              'one cell\n-16.2\n3.3 0 0\n0 3.3 0\n0 0 3.3\n0 0 0\n0 0 0\n'
              '0 0 0\nTa 0 0 0 0 0 0\nTa 1.6 1.6 1.6 0 0 0\n')
        write(self.files, 'data/README.md', '# data\n')
        self.page = write(self.tmp, 'tutorial/index.rst', DATA_PAGE)

    def run_static(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            status = check_nfp_files.main(['static', '--files', self.files,
                                           '--data-page', self.page])
        return status, out.getvalue()

    def test_the_counts_and_reference_values_of_the_page_pass(self):
        # Pins: with --data-page, the cells and atoms the page gives for
        # each dataset (the line after its :download: link) are those of
        # the file, and every ``<quantity>``: <value> <unit> of a term of
        # the page equals the value and unit of dft_reference.csv; the PASS
        # line counts both. Before the fix: static refuses the option
        # --data-page (exit 2).
        status, out = self.run_static()
        self.assertEqual(status, 0, out)
        self.assertIn('; the cells and atoms of 2 dataset(s) and 3 reference '
                      'value(s) that tutorial/index.rst gives match the files',
                      out)

    def test_a_count_other_than_the_dataset_fails(self):
        # Pins: a count of cells or atoms that the dataset does not have
        # fails, naming the file, the page line and both counts. Before the
        # fix: static refuses the option --data-page (exit 2).
        write(self.files, 'data/ta_a.tadahdb', dataset(1, 2, 2))
        status, out = self.run_static()
        self.assertEqual(status, 1, out)
        self.assertIn('FAIL: tutorial/index.rst:8: data/ta_a.tadahdb has 3 '
                      'cells and 5 atoms, not 2 cells and 3 atoms', out)
        self.assertEqual(out.count('FAIL'), 1, out)

    def test_a_dataset_without_a_count_and_a_count_without_a_dataset_fail(self):
        # Pins: every dataset of the data directory has its counts on the
        # page, and every dataset the page counts exists. Before the fix:
        # static refuses the option --data-page (exit 2).
        write(self.files, 'data/ta_c.tadahdb', dataset(4))
        os.remove(os.path.join(self.files, 'data/ta_b.tadahdb'))
        status, out = self.run_static()
        self.assertEqual(status, 1, out)
        self.assertIn('FAIL: data/ta_c.tadahdb: tutorial/index.rst gives no '
                      'count of its cells and atoms', out)
        self.assertIn('FAIL: tutorial/index.rst:10: data/ta_b.tadahdb does not '
                      'exist', out)
        self.assertEqual(out.count('FAIL'), 2, out)

    def test_a_reference_value_other_than_the_table_fails(self):
        # Pins: a value or a unit other than that of dft_reference.csv, and
        # a quantity it does not have (``B``, the term of an added entry),
        # fail, naming the page line; a quantity in the description of an
        # entry (``Ev``) is not read. Before the fix: static refuses the
        # option --data-page (exit 2); a checker without the branch of a
        # quantity that dft_reference.csv lacks writes no FAIL line for
        # ``B`` and fails the assertion of that line.
        write(self.tmp, 'tutorial/index.rst', DATA_PAGE.replace(
            '``a0``: 3.32011 A', '``a0``: 3.3201 A').replace(
            '``C12``: 161.5 GPa', '``C12``: 161.5 kbar').replace(
            'Relaxation of', '``Ev``: 3.1 eV from a relaxation of')
            + 'Bulk modulus ``B``: 195.7 GPa\n'
              '   From the elastic constants.\n')
        status, out = self.run_static()
        self.assertEqual(status, 1, out)
        self.assertIn('FAIL: tutorial/index.rst:17: a0 3.3201 A: '
                      'dft_reference.csv gives a0 3.32011 A (line 3)', out)
        self.assertIn('FAIL: tutorial/index.rst:19: C12 161.5 kbar: '
                      'dft_reference.csv gives C12 161.5 GPa (line 6)', out)
        self.assertIn('FAIL: tutorial/index.rst:21: B 195.7 GPa: '
                      'dft_reference.csv has no B', out)
        self.assertEqual(out.count('FAIL'), 3, out)
        # Only the term of an entry gives a reference value: its
        # description may name other numbers.
        self.assertNotIn('Ev 3.1', out)

    def test_a_page_without_counts_fails(self):
        # Pins: a page that gives no count of a dataset fails for each
        # dataset of the directory, so a reworded list is not passed over.
        # Before the fix: static refuses the option --data-page (exit 2).
        write(self.tmp, 'tutorial/index.rst', 'The Ta data\n-----------\n')
        status, out = self.run_static()
        self.assertEqual(status, 1, out)
        for name in ('ta_a', 'ta_b'):
            self.assertIn(f'FAIL: data/{name}.tadahdb: tutorial/index.rst '
                          'gives no count of its cells and atoms', out)


# The stand-in for tadah: appends its working directory and arguments to
# <files>/../calls.log; exits with the content of the file `exit` of its
# working directory, else 0, after 5 s when the file `sleep` is there.
FAKE_TADAH = '''#!{python}
import os, sys
with open({log!r}, 'a') as fh:
    fh.write(os.getcwd() + ' ' + ' '.join(sys.argv[1:]) + '\\n')
if os.path.isfile('exit'):
    print('stand-in refuses'); sys.exit(int(open('exit').read()))
if os.path.isfile('sleep'):
    import time; time.sleep(5)
print('passed the startup checks')
'''


class CheckTest(unittest.TestCase):
    '''source/nfp/{unit1.rst, unit7.rst} and source/nfp/files/{manifest.txt,
    tutorial/unit1, tutorial/unit7}: unit7's page runs unit1's target file
    and its own.'''

    def setUp(self):
        self.tmp = tempfile.mkdtemp(prefix='tadah_nfp_check_')
        self.addCleanup(shutil.rmtree, self.tmp)
        self.source = os.path.join(self.tmp, 'source')
        self.files = os.path.join(self.source, 'nfp', 'files')
        write(self.files, 'tutorial/unit1/config.hpo', 'OPTIM RCUT2B (1) 4 6\n')
        write(self.files, 'tutorial/unit1/config.train', 'DBFILE x\n')
        write(self.files, 'tutorial/unit7/config.hpo', 'OPTIM RCUT2B (1) 4 6\n')
        write(self.source, 'nfp/unit1.rst', 'U1\n==\n\n.. code-block:: bash\n\n'
              '   tadah hpo --config config.train --hpotarget config.hpo\n'
              '   tadah hpo -h\n')
        write(self.source, 'nfp/unit7.rst', 'U7\n==\n\n.. code-block:: bash\n\n'
              '   tadah hpo --config ../unit1/config.train \\\n'
              '       --hpotarget ../unit1/config.hpo\n'
              '   tadah predict -p best_pot.tadah -d x.tadahdb -A\n'
              '   tadah hpo --check --config ../unit1/config.train '
              '--hpotarget config.hpo\n'
              '   tadah hpo --config ../unit1/config.train --hpotarget config.hpo\n')
        self.manifest('u1 | nfp/unit1.rst | tutorial/unit1 | - | 60 | -',
                      'u7 | nfp/unit7.rst | tutorial/unit7 | - | 60 | -')
        self.log = os.path.join(self.tmp, 'calls.log')
        self.tadah = os.path.join(self.tmp, 'tadah')
        with open(self.tadah, 'w', encoding='utf-8') as fh:
            fh.write(FAKE_TADAH.format(python=sys.executable, log=self.log))
        os.chmod(self.tadah, os.stat(self.tadah).st_mode | stat.S_IXUSR)
        self.work = os.path.join(self.tmp, 'work')

    def manifest(self, *lines):
        write(self.files, 'manifest.txt', '# runs\n' + ''.join(l + '\n' for l in lines))

    def run_check(self, nfp='ON'):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            status = check_nfp_files.main([
                'check', '--manifest', os.path.join(self.files, 'manifest.txt'),
                '--tadah', self.tadah, '--workdir', self.work, '--nfp', nfp])
        return status, out.getvalue()

    def calls(self):
        with open(self.log, encoding='utf-8') as fh:
            return [l.split(' ', 1) for l in fh.read().splitlines()]

    def test_every_target_file_is_checked_with_its_page_arguments(self):
        status, out = self.run_check()
        self.assertEqual(status, 0, out)
        self.assertIn('PASS: 2 target file(s) passed tadah hpo --check', out)
        unit1 = os.path.join(self.work, 'files', 'tutorial', 'unit1')
        unit7 = os.path.join(self.work, 'files', 'tutorial', 'unit7')
        # One check per distinct command, in the directory of its line; a
        # help command is not checked; --check is not given twice.
        self.assertEqual(self.calls(), [
            [unit1, 'hpo --config config.train --hpotarget config.hpo --check'],
            [unit7, 'hpo --config ../unit1/config.train --hpotarget '
                    '../unit1/config.hpo --check'],
            [unit7, 'hpo --check --config ../unit1/config.train --hpotarget '
                    'config.hpo'],
            [unit7, 'hpo --config ../unit1/config.train --hpotarget config.hpo '
                    '--check']])

    def test_a_command_without_a_target_file_is_checked_too(self):
        # tadah 2.0.0 reads no task file: a tadah hpo command without
        # --hpotarget is checked as the page writes it, and tadah decides.
        write(self.source, 'nfp/unit1.rst', 'U1\n==\n\n.. code-block:: bash\n\n'
              '   tadah hpo --config config.train --hpotarget config.hpo\n'
              '   tadah hpo --validation config.val\n')
        status, out = self.run_check()
        self.assertEqual(status, 0, out)
        unit1 = os.path.join(self.work, 'files', 'tutorial', 'unit1')
        self.assertIn([unit1, 'hpo --validation config.val --check'],
                      self.calls())
        self.assertIn('PASS: 2 target file(s) passed tadah hpo --check; 0 left '
                      'to the run of its page; 1 command without --hpotarget '
                      'passed it', out)
        write(self.files, 'tutorial/unit1/exit', '2')
        status, out = self.run_check()
        self.assertEqual(status, 1, out)
        self.assertIn('FAIL: `tadah hpo --validation config.val --check` in '
                      'tutorial/unit1 exited with 2:\nstand-in refuses', out)

    def test_a_relative_tadah_is_taken_from_the_working_directory(self):
        cwd = os.getcwd()
        self.addCleanup(os.chdir, cwd)
        os.chdir(self.tmp)
        self.tadah = os.path.join('.', 'tadah')
        status, out = self.run_check()
        self.assertEqual(status, 0, out)

    def test_a_target_file_that_fails_the_check_fails(self):
        write(self.files, 'tutorial/unit7/exit', '1')
        status, out = self.run_check()
        self.assertEqual(status, 1, out)
        self.assertIn('--hpotarget ../unit1/config.hpo --check` in tutorial/unit7 '
                      'exited with 1:\nstand-in refuses', out)

    def test_a_check_that_does_not_end_fails(self):
        write(self.files, 'tutorial/unit1/sleep', '')
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            status = check_nfp_files.main([
                'check', '--manifest', os.path.join(self.files, 'manifest.txt'),
                '--tadah', self.tadah, '--workdir', self.work, '--nfp', 'ON',
                '--timeout', '1'])
        self.assertEqual(status, 1, out.getvalue())
        self.assertIn('--hpotarget config.hpo --check` in tutorial/unit1 did not '
                      'end within 1 s', out.getvalue())

    def test_a_target_file_that_no_page_runs_fails(self):
        write(self.files, 'examples/pt/config.hpo', 'OPTIM RCUT2B (1) 4 6\n')
        self.manifest('u1 | nfp/unit1.rst | tutorial/unit1 | - | 60 | -')
        status, out = self.run_check()
        self.assertEqual(status, 1, out)
        for rel in ('examples/pt/config.hpo', 'tutorial/unit7/config.hpo'):
            self.assertIn(f'FAIL: {rel}: no tadah hpo command of a page in the '
                          'manifest runs it', out)

    def test_a_warm_seed_that_an_earlier_search_writes_is_left_to_the_run(self):
        # warm.hpo starts at the best potential of the page's first search,
        # which the copy of the files does not have: the run of the page
        # checks it just before its search (check_nfp_run.py).
        write(self.files, 'tutorial/unit9/warm.hpo',
              'OPTIM RCUT2B (1) 4 6\nOPTIMIZER\n    INIT\n'
              '        METHOD WARM best_pot.tadah\n    ENDINIT\nENDOPTIMIZER\n')
        write(self.files, 'tutorial/unit9/shipped.hpo',
              'OPTIMIZER\n    INIT\n        METHOD WARM seed.tadah\n'
              '    ENDINIT\nENDOPTIMIZER\n')
        write(self.files, 'tutorial/unit9/seed.tadah', 'MODEL M_BLR BF_Linear\n')
        write(self.files, 'tutorial/unit9/first.hpo',
              'OPTIMIZER\n    INIT\n        METHOD WARM best_pot.tadah\n'
              '    ENDINIT\nENDOPTIMIZER\n')
        write(self.source, 'nfp/unit9.rst', 'U9\n==\n\n.. code-block:: bash\n\n'
              '   tadah hpo --config ../unit1/config.train --hpotarget first.hpo\n'
              '   tadah hpo --config ../unit1/config.train \\\n'
              '       --hpotarget ../unit1/config.hpo\n'
              '   tadah hpo --config ../unit1/config.train --hpotarget warm.hpo\n'
              '   tadah hpo --config ../unit1/config.train --hpotarget shipped.hpo\n')
        self.manifest('u1 | nfp/unit1.rst | tutorial/unit1 | - | 60 | -',
                      'u7 | nfp/unit7.rst | tutorial/unit7 | - | 60 | -',
                      'u9 | nfp/unit9.rst | tutorial/unit9 | - | 60 | -')
        status, out = self.run_check()
        self.assertEqual(status, 0, out)
        self.assertIn('tutorial/unit9/warm.hpo:4: METHOD WARM best_pot.tadah is '
                      'the best potential of the search on line 7 of '
                      'nfp/unit9.rst, which runs before it: tadah_nfp_u9 checks '
                      'the file just before its search', out)
        self.assertIn('PASS: 4 target file(s) passed tadah hpo --check; 1 left '
                      'to the run of its page', out)
        unit9 = os.path.join(self.work, 'files', 'tutorial', 'unit9')
        checked = [c[1].split('--hpotarget ')[1] for c in self.calls()
                   if c[0] == unit9]
        # A seed no earlier search writes (first.hpo) and a shipped seed
        # are checked here.
        self.assertEqual(checked, ['first.hpo --check',
                                   '../unit1/config.hpo --check',
                                   'shipped.hpo --check'])

    def test_a_page_that_does_not_exist_fails(self):
        self.manifest('u9 | nfp/unit9.rst | tutorial/unit1 | - | 60 | -')
        status, out = self.run_check()
        self.assertEqual(status, 1, out)
        self.assertIn('page nfp/unit9.rst does not exist', out)

    def test_skipped_without_nfp(self):
        status, out = self.run_check(nfp='OFF')
        self.assertEqual(status, check_nfp_files.SKIP, out)
        self.assertFalse(os.path.exists(self.log))

    def test_no_target_file_and_no_line_pass(self):
        shutil.rmtree(os.path.join(self.files, 'tutorial'))
        self.manifest()
        status, out = self.run_check()
        self.assertEqual(status, 0, out)
        self.assertIn('PASS: 0 target file(s)', out)


if __name__ == '__main__':
    unittest.main()
