#!/usr/bin/env python3
"""Runs a CLI example that the documentation includes, as its README says.

    check_cli_example.py --tadah <tadah> --example <examples/CLI/ex_N>
                         --workdir <scratch> --nfp ON|OFF

The example directory is copied to <workdir> (the source tree is never
written) and every ``tadah`` command in the ``.. code-block:: bash`` blocks
of its README.rst runs there, in order, with <tadah> in place of ``tadah``.
Each command must exit 0, and, unless it asks for help (-h, --help):

  * after ``tadah train``: the potential it writes, pot.tadah, exists;
  * after ``tadah hpo``: best_pot.tadah exists and best_loss.tadah has at
    least one row -- an hpo run in which every evaluation fails still exits
    0, with no best at all.

A README without a ``tadah`` command is an error. An example that runs
``tadah hpo`` needs a build with TADAH_ENABLE_NFP; with --nfp OFF it runs
nothing and exits 77, which CTest reports as Skipped.
"""
import argparse
import os
import shlex
import shutil
import subprocess
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                '..', 'doc_commands'))
from doc_commands import tadah_commands  # noqa: E402

SKIP = 77


def readme_commands(readme):
    '''The `tadah` command lines of the bash code blocks, in order
    (doc_commands.py).'''
    return tadah_commands(readme)


def data_rows(path):
    with open(path, encoding='utf-8') as fh:
        return [l for l in fh if l.strip() and not l.lstrip().startswith('#')]


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--tadah', required=True)
    ap.add_argument('--example', required=True)
    ap.add_argument('--workdir', required=True)
    ap.add_argument('--nfp', required=True, choices=('ON', 'OFF'))
    args = ap.parse_args()

    readme = os.path.join(args.example, 'README.rst')
    commands = readme_commands(readme)
    if not commands:
        print(f'FAIL: {readme} has no tadah command in a bash code block')
        return 1
    if args.nfp == 'OFF' and any(c[1:2] == ['hpo'] for c in commands):
        print(f'SKIP: {readme} runs tadah hpo, which needs a build with '
              'TADAH_ENABLE_NFP=ON')
        return SKIP

    shutil.rmtree(args.workdir, ignore_errors=True)
    shutil.copytree(args.example, args.workdir)
    for argv in commands:
        shown = ' '.join(shlex.quote(a) for a in argv)
        print(f'$ {shown}', flush=True)
        proc = subprocess.run([args.tadah] + argv[1:], cwd=args.workdir,
                              stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                              text=True, timeout=600)
        tail = '\n'.join(proc.stdout.splitlines()[-40:])
        if proc.returncode != 0:
            print(f'FAIL: `{shown}` exited with {proc.returncode}:\n{tail}')
            return 1
        missing = []
        if '-h' in argv or '--help' in argv:
            pass
        elif argv[1:2] == ['train']:
            if not os.path.isfile(os.path.join(args.workdir, 'pot.tadah')):
                missing.append('pot.tadah was not written')
        elif argv[1:2] == ['hpo']:
            if not os.path.isfile(os.path.join(args.workdir, 'best_pot.tadah')):
                missing.append('best_pot.tadah was not written')
            best = os.path.join(args.workdir, 'best_loss.tadah')
            if not os.path.isfile(best) or not data_rows(best):
                missing.append('best_loss.tadah has no row: no evaluation '
                               'gave a best (see hpo_run.log)')
        if missing:
            print(f'FAIL: `{shown}` exited with 0, but '
                  + '; '.join(missing) + f':\n{tail}')
            return 1
        print('ok (exit 0)', flush=True)
    print(f'PASS: {len(commands)} command(s) of {readme}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
