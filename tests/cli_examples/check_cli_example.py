#!/usr/bin/env python3
"""Runs a CLI example that the documentation includes, as its README says.

    check_cli_example.py --tadah <tadah> --example <examples/CLI/ex_N>
                         --workdir <scratch> --nfp ON|OFF

The example directory is copied to <workdir>/<ex_N> (the source tree is
never written), and every other directory beside it (the other examples)
is linked into <workdir>, so that a README's command can read the files of
another example where they are (``../ex_2/tdata.db``). Every ``tadah``
command in the ``.. code-block:: bash`` blocks of its README.rst runs in
<workdir>/<ex_N>, in order, with <tadah> in place of ``tadah``. Each
command must exit 0, and, unless it asks for help (-h, --help):

  * after ``tadah train``: the potential it writes exists (pot.tadah, or
    the file of -o/--outfile);
  * after ``tadah hpo``: best_pot.tadah exists and best_loss.tadah has at
    least one row -- an hpo run in which every evaluation fails still exits
    0, with no best at all.

An example may ship the fit balance table of a potential a ``tadah train``
command writes: <stem>.balance.txt for <stem>.tadah, the table as tadah
prints it, which the documentation quotes. The table of the training record
of the potential the run writes must have its lines: the same words, counts
and error scales, and each other number (RMSE, r/sigma, share, gamma, the
lambda of the evidence approximation) within two units of the last digit
the expected table prints (4 significant digits, 3 in exponent form, 6 for
the lambda). An expected table that no command of the README writes is an
error.

A README without a ``tadah`` command is an error. An example that runs
``tadah hpo`` needs a build with TADAH_ENABLE_NFP; with --nfp OFF it runs
nothing and exits 77, which CTest reports as Skipped.
"""
import argparse
import glob
import math
import os
import re
import shlex
import shutil
import subprocess
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                '..', 'doc_commands'))
from doc_commands import tadah_commands  # noqa: E402

SKIP = 77
RECORD_OPEN = '# --- training record (no key is read from it) ---'
RECORD_END = '# --- end of training record ---'
BALANCE = '.balance.txt'
# The words of a row before its numbers and the columns of its numbers:
# quantity, dataset, n, sigma (one value for the energy, a and b for the
# others), RMSE, unit, r/sigma, share, gamma.
COLUMNS = ['quantity', 'dataset', 'n', 'sigma', 'RMSE', 'unit', 'r/sigma',
           'share', 'gamma']


def readme_commands(readme):
    '''The `tadah` command lines of the bash code blocks, in order
    (doc_commands.py).'''
    return tadah_commands(readme)


def data_rows(path):
    with open(path, encoding='utf-8') as fh:
        return [l for l in fh if l.strip() and not l.lstrip().startswith('#')]


def option(argv, *names):
    '''The value of the first of the options *names* in *argv*, or None.'''
    for i, a in enumerate(argv[:-1]):
        if a in names:
            return argv[i + 1]
    return None


def record_table(path):
    '''The lines of the fit balance table of the training record of the
    potential *path*: from its "Fit balance:" line to the end of the
    record, without the leading "# ".'''
    with open(path, encoding='utf-8') as fh:
        lines = fh.read().splitlines()
    table, inside = [], False
    for line in lines:
        if line == RECORD_OPEN:
            inside = True
        elif line == RECORD_END:
            inside = False
        elif inside:
            text = line[2:] if line.startswith('# ') else line[1:]
            if table or text.startswith('Fit balance:'):
                table.append(text)
    return table


def unit_of_last_digit(text, digits):
    '''One unit of the last digit of the number *text*, printed with
    *digits* significant digits (fewer in exponent form: digits - 1).'''
    v = float(text)
    if v == 0.0:
        return 0.0
    sig = digits - 1 if 'e' in text.lower() else digits
    return 10.0 ** (math.floor(math.log10(abs(v))) - (sig - 1))


def compare_number(name, got, want, digits, where):
    '''A failure, or None: *got* is not *want* within two units of the last
    digit of *want* ("-" only as "-").'''
    if want == '-' or got == '-':
        return None if got == want else f'{where}: {name} {got}, expected {want}'
    try:
        g, w = float(got), float(want)
    except ValueError:
        return f'{where}: {name} {got}, expected {want}'
    if abs(g - w) > 2.0 * unit_of_last_digit(want, digits) * (1 + 1e-9):
        return (f'{where}: {name} {got}, expected {want} (within two units '
                f'of its last digit)')
    return None


def compare_tables(got, want, potential, expected):
    '''The failures of the table *got* of *potential* against the table
    *want* of the file *expected*.'''
    if len(got) != len(want):
        return [f'the fit balance table of {potential} has {len(got)} '
                f'lines, {expected} {len(want)}:\n' + '\n'.join(got)]
    fails = []
    for k, (g_line, w_line) in enumerate(zip(got, want)):
        where = f'{expected}:{k + 1}'
        g, w = g_line.split(), w_line.split()
        if k == 0:
            # Fit balance: LAMBDA <l>[ (evidence approximation: <x>)]; ...
            m_g = re.search(r'approximation: (\S+)\);', g_line)
            m_w = re.search(r'approximation: (\S+)\);', w_line)
            if m_g and m_w:
                f = compare_number('lambda', m_g.group(1), m_w.group(1), 6,
                                   where)
                if f:
                    fails.append(f)
                g_line = g_line.replace(m_g.group(1), '')
                w_line = w_line.replace(m_w.group(1), '')
            if g_line.split() != w_line.split():
                fails.append(f'{where}: "{g_line}", expected "{w_line}"')
            continue
        if k == 1 or g[:1] == ['prior'] or w[:1] == ['prior']:
            if g[:1] != w[:1] or len(g) != len(w):
                fails.append(f'{where}: "{g_line}", expected "{w_line}"')
                continue
            if k == 1:
                if g != w:
                    fails.append(f'{where}: "{g_line}", expected "{w_line}"')
                continue
            for name, a, b in zip(('share', 'gamma'), g[1:], w[1:]):
                f = compare_number(name, a, b, 4, where)
                if f:
                    fails.append(f)
            continue
        if not g or not w or g[0] != w[0]:
            fails.append(f'{where}: quantity {g[:1]}, expected {w[:1]}: '
                         f'"{g_line}"')
            continue
        two = 0 if w[0] == 'energy' else 1
        if len(g) != len(w) or len(w) != 9 + two:
            fails.append(f'{where}: "{g_line}", expected "{w_line}"')
            continue
        words = {'dataset': (g[1], w[1]), 'n': (g[2], w[2]),
                 'sigma': (' '.join(g[3:4 + two]), ' '.join(w[3:4 + two])),
                 'unit': (g[5 + two], w[5 + two])}
        for name, (a, b) in words.items():
            if a != b:
                fails.append(f'{where}: {name} {a}, expected {b}')
        for name, i in (('RMSE', 4), ('r/sigma', 6), ('share', 7),
                        ('gamma', 8)):
            f = compare_number(name, g[i + two], w[i + two], 4, where)
            if f:
                fails.append(f)
    return fails


def link_siblings(example, workdir):
    '''Links every directory beside *example* into *workdir*.'''
    parent = os.path.dirname(os.path.abspath(example))
    name = os.path.basename(os.path.abspath(example))
    for other in sorted(os.listdir(parent)):
        src = os.path.join(parent, other)
        if other != name and os.path.isdir(src):
            os.symlink(src, os.path.join(workdir, other))


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--tadah', required=True)
    ap.add_argument('--example', required=True)
    ap.add_argument('--workdir', required=True)
    ap.add_argument('--nfp', required=True, choices=('ON', 'OFF'))
    args = ap.parse_args(argv)

    readme = os.path.join(args.example, 'README.rst')
    commands = readme_commands(readme)
    if not commands:
        print(f'FAIL: {readme} has no tadah command in a bash code block')
        return 1
    if args.nfp == 'OFF' and any(c[1:2] == ['hpo'] for c in commands):
        print(f'SKIP: {readme} runs tadah hpo, which needs a build with '
              'TADAH_ENABLE_NFP=ON')
        return SKIP

    # The tables the example expects, by the potential that has them.
    expected = {os.path.basename(p)[:-len(BALANCE)] + '.tadah': p
                for p in glob.glob(os.path.join(args.example, '*' + BALANCE))}
    written = {option(argv, '-o', '--outfile') or 'pot.tadah'
               for argv in commands if argv[1:2] == ['train']}
    orphans = [p for pot, p in sorted(expected.items()) if pot not in written]
    if orphans:
        for p in orphans:
            stem = os.path.basename(p)[:-len(BALANCE)]
            print(f'FAIL: {os.path.basename(p)}: no tadah train command of '
                  f'the README writes {stem}.tadah')
        return 1

    shutil.rmtree(args.workdir, ignore_errors=True)
    os.makedirs(args.workdir)
    rundir = os.path.join(args.workdir,
                          os.path.basename(os.path.abspath(args.example)))
    shutil.copytree(args.example, rundir)
    link_siblings(args.example, args.workdir)
    for argv in commands:
        shown = ' '.join(shlex.quote(a) for a in argv)
        print(f'$ {shown}', flush=True)
        proc = subprocess.run([args.tadah] + argv[1:], cwd=rundir,
                              stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                              text=True, timeout=600)
        tail = '\n'.join(proc.stdout.splitlines()[-40:])
        if proc.returncode != 0:
            print(f'FAIL: `{shown}` exited with {proc.returncode}:\n{tail}')
            return 1
        missing, fails, oks = [], [], []
        if '-h' in argv or '--help' in argv:
            pass
        elif argv[1:2] == ['train']:
            pot = option(argv, '-o', '--outfile') or 'pot.tadah'
            path = os.path.join(rundir, pot)
            if not os.path.isfile(path):
                missing.append(f'{pot} was not written')
            elif pot in expected:
                with open(expected[pot], encoding='utf-8') as fh:
                    want = fh.read().splitlines()
                got = record_table(path)
                name = os.path.basename(expected[pot])
                if not got:
                    fails.append(f'{pot} has no fit balance table in its '
                                 f'training record ({name} expects one)')
                else:
                    fails += compare_tables(got, want, pot, name)
                    if not fails:
                        oks.append(f'ok: the fit balance table of {pot} is '
                                   f'{name}')
        elif argv[1:2] == ['hpo']:
            if not os.path.isfile(os.path.join(rundir, 'best_pot.tadah')):
                missing.append('best_pot.tadah was not written')
            best = os.path.join(rundir, 'best_loss.tadah')
            if not os.path.isfile(best) or not data_rows(best):
                missing.append('best_loss.tadah has no row: no evaluation '
                               'gave a best (see hpo_run.log)')
        if missing:
            print(f'FAIL: `{shown}` exited with 0, but '
                  + '; '.join(missing) + f':\n{tail}')
            return 1
        if fails:
            for f in fails:
                print(f'FAIL: {f}')
            return 1
        for ok in oks:
            print(ok)
        print('ok (exit 0)', flush=True)
    print(f'PASS: {len(commands)} command(s) of {readme}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
