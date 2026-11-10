#!/usr/bin/env python3
"""Checks of the files the Nested Fitting pages ship (docs/source/nfp/files).

    check_nfp_files.py static --files <docs/source/nfp/files>
                       [--data-page <docs/source/nfp/tutorial/index.rst>]
    check_nfp_files.py check --manifest <docs/source/nfp/files/manifest.txt>
                       --tadah <tadah> --workdir <scratch> --nfp ON|OFF

``static`` (CTest tadah_nfp_files_static) reads the files and runs nothing:

  * every file is at most 1 MiB, and every text file (a file without a NUL
    byte) has LF line endings: no CR;
  * every DFT value a target file (*.hpo) gives -- the <target> of each
    ``--varloss <variable> <target> <weight>`` of a PC_LAMMPS or PC_FAILPROBE
    line, the initial value of a ``--hotvar`` and the value of an
    ``--invar`` there whose variable has a DFT quantity, and the
    ``--target`` of a PC_OBSERVABLE line -- equals the value of its
    quantity in dft_reference.csv. nfp_files.VARIABLE_QUANTITY and
    nfp_files.OBSERVABLE_QUANTITY map a variable and an observable to its
    quantity; a --varloss variable or an observable they do not map is an
    error, as is a target file with DFT values and no dft_reference.csv;
  * every SHA256SUMS file (lines ``<sha256>  <file>``, as sha256sum writes
    them) lists files of its directory that exist and have that SHA-256,
    and lists every *.tadahdb file of its directory.
  * every target file with an OPTIMIZER block, a file that runs a search,
    sets the workers of the run: a THREADS line in its OPTIMIZER, STRATEGY
    or REFINE block (nfp_files.run_threads()). Without one, the search and
    its ``tadah hpo --check`` warn when OMP_NUM_THREADS is above 1, and what
    a reader sees depends on the environment; THREADS 1 unless the example
    is about threads, which states its value too;
  * with --data-page, the page that describes the datasets (the Ta data of
    the Tutorial hub) gives what the files hold: the line after the
    ``:download:`` link of a dataset (*.tadahdb) ends with the number of
    its structures and atoms, ``<n> cells, <m> atoms.``, which equal those
    of the file; every dataset of a directory the page links has such a
    line, and every dataset it counts exists; and each ``<quantity>``:
    <value> <unit> of the term of a definition list entry (a line at the
    start of the page's text that the next line indents) equals the value
    and unit of the quantity in dft_reference.csv. A structure of a
    dataset is a run of lines up to a blank line; its atoms are its lines
    after the first whose first word is not a number.

``check`` (CTest tadah_nfp_files_check) runs ``tadah hpo --check`` for every
``tadah hpo`` command of the pages the manifest lists, with the command's
own arguments, in a copy of the files (<workdir>/files) and in the
directory of its manifest line: each must exit 0. Every target file under
the files must be the --hpotarget of such a command; one that no page runs
is an error, as the check could not know its training and validation
configurations. A help command is not checked; a command without
--hpotarget is checked as the page writes it. A target file that starts
from the best potential of an earlier search of its page (``METHOD WARM
best_pot.tadah``, a file the copy does not have and that search writes)
cannot be checked before that search: it is left to the run of its page
(check_nfp_run.py runs ``tadah hpo --check`` just before each search), and
the check says so. In a build without TADAH_ENABLE_NFP (--nfp OFF) it
exits 77, which CTest reports as Skipped.

Each problem is one line on standard output; the exit status is 1 when
there is one.
"""
import argparse
import hashlib
import os
import re
import shlex
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', 'doc_commands'))
sys.path.insert(0, HERE)
from doc_commands import bash_commands  # noqa: E402
import check_nfp_run  # noqa: E402
import nfp_files  # noqa: E402

SKIP = 77
MAX_BYTES = 1024 * 1024


def shipped_files(files):
    '''The files under *files*, relative to it, sorted.'''
    found = []
    for top, subdirs, names in os.walk(files):
        subdirs.sort()
        for name in sorted(names):
            found.append(os.path.relpath(os.path.join(top, name), files))
    return found


SUM_LINE = re.compile(r'([0-9a-f]{64}) [ *](.+)')


def checksum_problems(files, names):
    '''The problems of the SHA256SUMS files under *files*, and the number
    of files they list.'''
    problems, listed = [], 0
    for rel in names:
        if os.path.basename(rel) != 'SHA256SUMS':
            continue
        directory = os.path.dirname(rel)
        named = set()
        with open(os.path.join(files, rel), encoding='utf-8') as fh:
            for number, line in enumerate(fh.read().splitlines(), 1):
                if not line.strip():
                    continue
                m = SUM_LINE.fullmatch(line)
                if not m:
                    problems.append(f'{rel}:{number}: "{line}" is not '
                                    '"<sha256>  <file>"')
                    continue
                digest, name = m.groups()
                named.add(name)
                path = os.path.join(files, directory, name)
                if not os.path.isfile(path):
                    problems.append(f'{rel}:{number}: {name} does not exist')
                    continue
                listed += 1
                with open(path, 'rb') as data:
                    if hashlib.sha256(data.read()).hexdigest() != digest:
                        problems.append(f'{os.path.join(directory, name)}: its '
                                        f'SHA-256 is not the one {rel} gives '
                                        f'(line {number})')
        for name in sorted(os.listdir(os.path.join(files, directory))):
            if name.endswith('.tadahdb') and name not in named:
                problems.append(f'{os.path.join(directory, name)}: {rel} does '
                                'not list it')
    return problems, listed


DOWNLOAD = re.compile(r':download:`[^`<]*<([^>`]+\.tadahdb)>`\s*')
COUNTS = re.compile(r'.*\b([0-9]+) cells?, ([0-9]+) atoms?\.')
TERM_VALUE = re.compile(r'``([A-Za-z_][A-Za-z0-9_]*)``: (-?[0-9][0-9.eE+-]*) '
                        r'([^\s,]+)')
NUMBER = re.compile(r'[-+]?(?:[0-9]+\.?[0-9]*|\.[0-9]+)(?:[eE][-+]?[0-9]+)?')


def dataset_counts(path):
    '''(structures, atoms) of the dataset at *path*: a structure is a run of
    non-blank lines, and its atoms are its lines after the first (the
    label) whose first word is not a number.'''
    cells = atoms = 0
    with open(path, encoding='utf-8') as fh:
        block = []
        for line in fh.read().splitlines() + ['']:
            if line.strip():
                block.append(line.split())
                continue
            if block:
                cells += 1
                atoms += sum(1 for w in block[1:] if not NUMBER.fullmatch(w[0]))
                block = []
    return cells, atoms


def data_page_problems(files, page):
    '''The problems of the counts and reference values that *page* gives,
    and (number of datasets counted, number of reference values).'''
    problems = []
    shown = os.path.relpath(page, os.path.dirname(files))
    with open(page, encoding='utf-8') as fh:
        lines = fh.read().splitlines()
    counted, linked_dirs = {}, set()
    for i, line in enumerate(lines):
        m = DOWNLOAD.fullmatch(line.strip())
        if not m:
            continue
        path = os.path.normpath(os.path.join(os.path.dirname(page),
                                             m.group(1)))
        linked_dirs.add(os.path.dirname(path))
        rel = os.path.relpath(path, files)
        following = lines[i + 1] if i + 1 < len(lines) else ''
        c = COUNTS.fullmatch(following.strip())
        if not c:
            continue
        counted[path] = rel
        if not os.path.isfile(path):
            problems.append(f'{shown}:{i + 2}: {rel} does not exist')
            continue
        want = (int(c.group(1)), int(c.group(2)))
        got = dataset_counts(path)
        if got != want:
            problems.append(f'{shown}:{i + 2}: {rel} has {got[0]} cells and '
                            f'{got[1]} atoms, not {want[0]} cells and '
                            f'{want[1]} atoms')
    # Every dataset of the shipped data directories: those the page links,
    # and data/ under the files.
    linked_dirs.add(os.path.join(files, 'data'))
    for directory in sorted(linked_dirs):
        if not os.path.isdir(directory):
            continue
        for name in sorted(os.listdir(directory)):
            path = os.path.join(directory, name)
            if name.endswith('.tadahdb') and path not in counted:
                problems.append(f'{os.path.relpath(path, files)}: {shown} '
                                'gives no count of its cells and atoms')
    values = 0
    reference = None
    for i, line in enumerate(lines):
        following = lines[i + 1] if i + 1 < len(lines) else ''
        if (not line.strip() or line[0].isspace() or not following.strip()
                or not following[0].isspace()):
            continue
        for q, value, unit in TERM_VALUE.findall(line):
            if reference is None:
                reference = nfp_files.read_dft_reference(
                    os.path.join(files, 'dft_reference.csv'))
            values += 1
            ref = reference.get(q)
            if ref is None:
                problems.append(f'{shown}:{i + 1}: {q} {value} {unit}: '
                                f'dft_reference.csv has no {q}')
            elif float(value) != ref.value or unit != ref.unit:
                problems.append(f'{shown}:{i + 1}: {q} {value} {unit}: '
                                f'dft_reference.csv gives {q} {ref.value!r} '
                                f'{ref.unit} (line {ref.line})')
    return problems, len(counted), values


def static_problems(files):
    '''The problems of the static checks of the tree *files*, and what was
    checked: (problems, number of files, number of DFT values, number of
    files their SHA256SUMS lists, number of target files that run a
    search).'''
    problems = []
    names = shipped_files(files)
    for rel in names:
        size = os.path.getsize(os.path.join(files, rel))
        if size > MAX_BYTES:
            problems.append(f'{rel}: {size} bytes, more than 1 MiB')
        with open(os.path.join(files, rel), 'rb') as fh:
            data = fh.read()
        if b'\0' not in data and b'\r' in data:
            problems.append(f'{rel}: CR line endings (LF only)')
    sum_problems, listed = checksum_problems(files, names)
    problems += sum_problems
    targets, searches = [], 0
    for rel in names:
        if not rel.endswith('.hpo'):
            continue
        try:
            opened, threads = nfp_files.run_threads(os.path.join(files, rel))
        except (ValueError, nfp_files.FilesError) as err:
            problems.append(f'{rel}: {err}')
            continue
        if opened is not None:
            searches += 1
            if not threads:
                problems.append(
                    f'{rel}:{opened}: the OPTIMIZER block sets no THREADS (in '
                    'OPTIMIZER, STRATEGY or REFINE): a shipped search states '
                    'its workers, THREADS 1 unless its example is about '
                    'threads')
        try:
            targets += [(rel, t) for t in
                        nfp_files.dft_targets(os.path.join(files, rel))]
        except nfp_files.FilesError as err:
            problems.append(str(err))
    if targets:
        path = os.path.join(files, 'dft_reference.csv')
        try:
            reference = nfp_files.read_dft_reference(path)
        except FileNotFoundError:
            problems.append(f'{path} does not exist, and target files give '
                            'DFT values')
            return problems, len(names), len(targets), listed, searches
        except nfp_files.FilesError as err:
            problems.append(str(err))
            return problems, len(names), len(targets), listed, searches
        for rel, t in targets:
            ref = reference.get(t.quantity)
            if ref is None:
                problems.append(f'{rel}:{t.line}: {t.where} {t.value!r}: '
                                f'dft_reference.csv has no {t.quantity}')
            elif t.value != ref.value:
                problems.append(f'{rel}:{t.line}: {t.where} {t.value!r}: '
                                f'dft_reference.csv gives {t.quantity} '
                                f'{ref.value!r} (line {ref.line})')
    return problems, len(names), len(targets), listed, searches


def run_static(args):
    files = os.path.abspath(args.files)
    problems, n_files, n_values, n_sums, n_searches = static_problems(files)
    page_note = ''
    if args.data_page is not None:
        page = os.path.abspath(args.data_page)
        page_problems, n_sets, n_refs = data_page_problems(files, page)
        problems += page_problems
        page_note = (f'; the cells and atoms of {n_sets} dataset(s) and '
                     f'{n_refs} reference value(s) that '
                     f'{os.path.relpath(page, os.path.dirname(files))} gives '
                     'match the files')
    for p in problems:
        print(f'FAIL: {p}')
    if problems:
        return 1
    print(f'PASS: {n_files} file(s) of at most 1 MiB with LF line endings; '
          f'{n_values} DFT value(s) of target files equal dft_reference.csv; '
          f'{n_sums} file(s) match their SHA256SUMS; {n_searches} target '
          'file(s) that run a search set THREADS' + page_note)
    return 0


def check_commands(manifest):
    '''[(manifest line, run directory relative to the files, argv, line of
    the page's last search before it or None)] of the tadah hpo commands of
    the manifest's pages, each once.'''
    files = os.path.dirname(os.path.abspath(manifest))
    docs_source = os.path.normpath(os.path.join(files, '..', '..'))
    found, seen = [], set()
    for r in check_nfp_run.parse_manifest(manifest):
        page = os.path.join(docs_source, r.page)
        if not os.path.isfile(page):
            raise check_nfp_run.RunError(f'{manifest}:{r.line}: page {r.page} '
                                         'does not exist')
        last_search = None
        for c in bash_commands(page):
            argv = c.argv
            if argv[:2] != ['tadah', 'hpo'] or '-h' in argv or '--help' in argv:
                continue
            key = (os.path.normpath(r.dir), tuple(argv))
            if key not in seen:
                seen.add(key)
                found.append((r, os.path.normpath(r.dir), argv, last_search))
            if '--check' not in argv:
                last_search = c.line
    return found


def left_to_the_run(target, run_dir, last_search):
    '''(line, seed) of the ``METHOD WARM`` of *target* that the copy of
    the files does not have and the search on line *last_search* of the
    page writes (best_pot.tadah in *run_dir*), or None.'''
    if last_search is None or not os.path.isfile(target):
        return None
    for line, seed in nfp_files.warm_seeds(target):
        if (os.path.normpath(seed) == 'best_pot.tadah'
                and not os.path.exists(os.path.join(run_dir, seed))):
            return line, seed
    return None


def run_check(args):
    if args.nfp == 'OFF':
        print('SKIP: tadah hpo --check needs a build with TADAH_ENABLE_NFP=ON')
        return SKIP
    files = os.path.dirname(os.path.abspath(args.manifest))
    commands = check_commands(args.manifest)
    # The commands run in the copy: a relative path to tadah is the
    # caller's, a bare name is looked up on PATH.
    tadah = os.path.abspath(args.tadah) if os.sep in args.tadah else args.tadah
    workdir = os.path.abspath(args.workdir)
    shutil.rmtree(workdir, ignore_errors=True)
    shutil.copytree(files, os.path.join(workdir, 'files'))
    problems, covered, checked, left = [], set(), set(), set()
    untargeted = 0      # the commands without --hpotarget that passed
    for r, rdir, argv, last_search in commands:
        target = check_nfp_run._option(argv, '--hpotarget', '--hpotarget')
        run_dir = os.path.join(workdir, 'files', rdir)
        rel = None
        if target:
            rel = os.path.normpath(os.path.join(rdir, target[0]))
            covered.add(rel)
            seed = left_to_the_run(os.path.join(run_dir, target[0]), run_dir,
                                   last_search)
            if seed is not None:
                print(f'{rel}:{seed[0]}: METHOD WARM {seed[1]} is the best '
                      f'potential of the search on line {last_search} of '
                      f'{r.page}, which runs before it: tadah_nfp_{r.id} '
                      'checks the file just before its search', flush=True)
                left.add(rel)
                continue
        cmd = argv[1:] + ([] if '--check' in argv else ['--check'])
        shown = ' '.join(shlex.quote(a) for a in ['tadah'] + cmd)
        print(f'$ {shown}   (in {rdir}; {r.page})', flush=True)
        try:
            # tadah writes UTF-8 (the Å of the units), whatever the locale.
            proc = subprocess.run([tadah] + cmd, cwd=run_dir,
                                  stdout=subprocess.PIPE,
                                  stderr=subprocess.STDOUT, encoding='utf-8',
                                  errors='replace', timeout=args.timeout)
        except subprocess.TimeoutExpired:
            problems.append(f'`{shown}` in {rdir} did not end within '
                            f'{args.timeout:g} s')
            continue
        if proc.returncode != 0:
            tail = '\n'.join(proc.stdout.splitlines()[-20:])
            problems.append(f'`{shown}` in {rdir} exited with '
                            f'{proc.returncode}:\n{tail}')
            continue
        print('ok (exit 0)', flush=True)
        if rel is None:
            untargeted += 1
        else:
            checked.add(rel)
    for rel in shipped_files(files):
        if rel.endswith('.hpo') and os.path.normpath(rel) not in covered:
            problems.append(f'{rel}: no tadah hpo command of a page in the '
                            'manifest runs it, so its configurations are not '
                            'known')
    for p in problems:
        print(f'FAIL: {p}')
    if problems:
        return 1
    print(f'PASS: {len(checked)} target file(s) passed tadah hpo --check; '
          f'{len(left - checked)} left to the run of its page'
          + (f'; {untargeted} command{"s" if untargeted > 1 else ""} without '
             '--hpotarget passed it' if untargeted else ''))
    return 0


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    sub = ap.add_subparsers(dest='mode', required=True)
    s = sub.add_parser('static', help='the checks that run nothing')
    s.add_argument('--files', required=True)
    s.add_argument('--data-page',
                   help='the page that gives the counts of the datasets and '
                        'the reference values (docs/source/nfp/tutorial/'
                        'index.rst)')
    c = sub.add_parser('check', help='tadah hpo --check of every target file')
    c.add_argument('--manifest', required=True)
    c.add_argument('--tadah', required=True)
    c.add_argument('--workdir', required=True)
    c.add_argument('--nfp', choices=('ON', 'OFF'), required=True)
    c.add_argument('--timeout', type=float, default=300,
                   help='seconds one command may take (default 300)')
    args = ap.parse_args(argv)
    try:
        return run_static(args) if args.mode == 'static' else run_check(args)
    except (check_nfp_run.ManifestError, check_nfp_run.RunError,
            nfp_files.FilesError) as err:
        print(f'FAIL: {err}')
        return 1


if __name__ == '__main__':
    sys.exit(main())
