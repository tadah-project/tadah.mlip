#!/usr/bin/env python3
"""The shipped Quick Start potential is what its configuration trains.

    check_quickstart_pot.py --tadah <tadah> --quickstart <docs/source/quickstart>
                            --workdir <scratch>

docs/source/quickstart ships config.train, the datasets and the potential
they train, pot.tadah, which the LAMMPS example (in.ml) runs. This guard
retrains it as the Quick Start page says, ``tadah train -c config.train``,
in a scratch copy of the directory (the source tree is never written).

The command must exit 0 and write pot.tadah. The retrained potential must
equal the shipped one:

  * every key other than WEIGHTS, with the same values, token by token, and
    no key more or less;
  * the same number of weights, and the same predictions: ``tadah predict
    -F -S`` on db.predict with each potential, and every predicted energy,
    force and stress component equal to the shipped potential's within
    1e-9 times the largest magnitude of that quantity.

The weights are not compared directly: the two-body and many-body columns
of this model are nearly collinear, so the weights move far more than the
predictions under a change in the last bits of the data or the arithmetic
(-march=native, -ffast-math), while the predictions are what a user gets.

A failure means that pot.tadah is stale (it is not what the shipped
config.train and the current code produce) or that the commands of the
page no longer work: regenerate pot.tadah with ``tadah train -c
config.train`` in that directory, and check the numbers the page quotes.

It is a guard, not a test of one defect: it keeps the shipped potential, which
the LAMMPS test of the page runs ("quickstart: in.ml with the shipped
pot.tadah ..." in test_tadah), the one the page's commands make.

Pins: both commands exit 0 and write pot.tadah; each has exactly the keys of
the shipped pot.tadah with the same value tokens (WEIGHTS: the same count) and
predicts every energy, force and stress component of db.predict within 1e-9
times the largest magnitude of that quantity of the shipped potential.
Before the fix: fails. The shipped D2_LJ pot.tadah was written by an older
writer: it lacks ESHIFT_ATOM, ESHIFT_DBATOM, FORCE, LSCALE, STRESS,
WDBFILE_AUTO and ZERO_COM_FORCE, and has EWEIGHT, FWEIGHT and SWEIGHT 1.0
where the writer writes 1 (the predictions of the retrained potentials agreed
with it to 1.5e-14).
"""
import argparse
import os
import shutil
import subprocess
import sys

REL_TOL = 1e-9


def run(tadah, argv, cwd):
    shown = ' '.join(['tadah'] + argv)
    print(f'$ {shown}   (in {cwd})', flush=True)
    proc = subprocess.run([tadah] + argv, cwd=cwd, stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT, text=True, timeout=600)
    if proc.returncode != 0:
        tail = '\n'.join(proc.stdout.splitlines()[-40:])
        raise RuntimeError(f'`{shown}` exited with {proc.returncode}:\n{tail}')


def read_pot(path):
    """KEY -> list of value tokens; a key may appear once."""
    keys = {}
    with open(path, encoding='utf-8') as fh:
        for n, line in enumerate(fh, 1):
            line = line.split('#', 1)[0].split()
            if not line:
                continue
            if line[0] in keys:
                raise RuntimeError(f'{path}: key {line[0]} repeated on line {n}')
            keys[line[0]] = line[1:]
    return keys


def predictions(path):
    """The predicted column of a .pred file (index, dataset value, prediction)."""
    values = []
    with open(path, encoding='utf-8') as fh:
        for line in fh:
            t = line.split()
            if not t or not t[0].isdigit():
                continue  # the header
            if len(t) != 3:
                raise RuntimeError(f'{path}: expected 3 columns: {line!r}')
            values.append(float(t[2]))
    if not values:
        raise RuntimeError(f'{path} has no prediction')
    return values


def predict(tadah, pot, data_dir, workdir):
    """Predictions of @p pot on db.predict: {name: [values]}."""
    os.makedirs(workdir)
    shutil.copy(os.path.join(data_dir, 'db.predict'), workdir)
    run(tadah, ['predict', '-p', pot, '-d', 'db.predict', '-F', '-S'],
        workdir)
    return {q: predictions(os.path.join(workdir, f'{q}.pred'))
            for q in ('energy', 'forces', 'stress')}


def compare(name, pot, shipped, pred, shipped_pred):
    """The differences of a retrained potential from the shipped one."""
    problems = []
    got = read_pot(pot)
    for key in sorted(set(shipped) | set(got)):
        if key not in got:
            problems.append(f'{name}: no key {key} (shipped: '
                            f'{" ".join(shipped[key])})')
        elif key not in shipped:
            problems.append(f'{name}: key {key} {" ".join(got[key])} is not '
                            'in the shipped pot.tadah')
        elif key == 'WEIGHTS':
            if len(got[key]) != len(shipped[key]):
                problems.append(f'{name}: {len(got[key])} weights, shipped '
                                f'{len(shipped[key])}')
        elif got[key] != shipped[key]:
            problems.append(f'{name}: {key} {" ".join(got[key])}, shipped '
                            f'{" ".join(shipped[key])}')
    for q, ref in shipped_pred.items():
        mine = pred[q]
        if len(mine) != len(ref):
            problems.append(f'{name}: {len(mine)} {q} predictions, shipped '
                            f'{len(ref)}')
            continue
        scale = max(abs(v) for v in ref)
        dev = max(abs(a - b) for a, b in zip(mine, ref))
        ok = dev <= REL_TOL * scale
        print(f'{name}: {q}: max |pred - shipped| = {dev:.3e} '
              f'(tolerance {REL_TOL:g} x {scale:.6g})'
              + ('' if ok else '  FAIL'))
        if not ok:
            problems.append(f'{name}: {q} predictions differ by up to '
                            f'{dev:.3e} (tolerance {REL_TOL * scale:.3e})')
    return problems


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--tadah', required=True)
    ap.add_argument('--quickstart', required=True)
    ap.add_argument('--workdir', required=True)
    args = ap.parse_args()

    src = os.path.abspath(args.quickstart)
    work = os.path.abspath(args.workdir)
    shipped_pot = os.path.join(src, 'pot.tadah')
    shutil.rmtree(work, ignore_errors=True)
    os.makedirs(work)

    try:
        shipped = read_pot(shipped_pot)
        shipped_pred = predict(args.tadah, shipped_pot, src,
                               os.path.join(work, 'predict_shipped'))

        retrained = {}
        train_dir = os.path.join(work, 'train')
        shutil.copytree(src, train_dir)
        os.remove(os.path.join(train_dir, 'pot.tadah'))
        run(args.tadah, ['train', '-c', 'config.train'], train_dir)
        retrained['tadah train -c config.train'] = train_dir

        problems = []
        for i, (name, d) in enumerate(retrained.items()):
            pot = os.path.join(d, 'pot.tadah')
            if not os.path.isfile(pot):
                problems.append(f'{name}: wrote no pot.tadah')
                continue
            pred = predict(args.tadah, pot, src,
                           os.path.join(work, f'predict_{i}'))
            problems += compare(name, pot, shipped, pred, shipped_pred)
    except (RuntimeError, OSError, ValueError) as e:
        print(f'FAIL: {e}')
        return 1

    if problems:
        print('FAIL: the shipped pot.tadah is not what the Quick Start '
              'commands train:\n  ' + '\n  '.join(problems))
        return 1
    print('PASS: tadah train -c config.train reproduces the shipped '
          'pot.tadah')
    return 0


if __name__ == '__main__':
    sys.exit(main())
