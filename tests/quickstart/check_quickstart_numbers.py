#!/usr/bin/env python3
"""The numbers the Quick Start page quotes of its potential are tadah's.

    check_quickstart_numbers.py --tadah <tadah>
                                --quickstart <docs/source/quickstart>
                                --workdir <scratch>

docs/source/quickstart.rst (and the CHANGELOG entries of the Quick Start
potential and of LAMBDA -1) quote numbers of the shipped potential,
pot.tadah, and of fits of the shipped data. This check computes each with
the tadah under test, in a scratch directory (the source tree is never
written), and compares it with the number quoted, within half a unit of its
last quoted digit:

  * ``tadah predict -F -S -A`` on db.predict: the energy, force and stress
    RMSE (0.29 meV/atom, 0.032 eV/Å, 0.049 GPa);
  * bcc Ta of pot.tadah (2-atom cubic cells; energy.pred gives the energy
    per atom, stress.pred the six entries of the stress tensor in eV/Å^3):
    the lattice constant at zero stress (3.288 Å), the energy per atom
    there (-8.104 eV), C' = (C11 - C12)/2 from the energy of the strain
    diag(e, -e, 0) (57 GPa), the bulk modulus V d2E/dV2 (115 GPa), fcc above
    bcc at the lowest energy of each (0.117 eV/atom), the local energy
    minimum of compressed bcc (near 2.25 Å, -2.72 eV/atom) and the rise of
    the energy from 3.29 Å to 3.8 Å (5.2 eV/atom);
  * a Ta dimer of pot.tadah: 12.3 eV at 0.2 Å and about 13.6 eV as the
    distance goes to zero (at 0.001 Å);
  * the zero-stress lattice constant of fits of db.train other than
    config.train's: energies only (STRESS false: 3.381 Å), energies and
    forces (STRESS false, -F: 3.389 Å), energies, forces and stresses (-F:
    3.294 Å);
  * the lambda of the evidence approximation of a fit of the first 100
    structures of db.train, each with fweight 0, with FORCE true, STRESS
    false, OALGO 3 and LAMBDA -1, as the balance table of the fit gives it
    (1.9e-5).

A guard of the documentation: it passes on the page as it is, and fails
when a quoted number no longer is what tadah computes (shown with the
WEIGHTS of pot.tadah multiplied by 1.01: the lattice constant, the energy
and the moduli move).

Pins: every number above within half a unit of its last quoted digit.
"""
import argparse
import os
import re
import shutil
import subprocess
import sys

GPA = 160.21766208  # GPa in one eV/A^3


def run(tadah, argv, cwd):
    shown = ' '.join(['tadah'] + argv)
    proc = subprocess.run([tadah] + argv, cwd=cwd, stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT, text=True, timeout=600)
    if proc.returncode != 0:
        tail = '\n'.join(proc.stdout.splitlines()[-40:])
        raise RuntimeError(f'`{shown}` exited with {proc.returncode}:\n{tail}')
    return proc.stdout


def bcc(a, strain=None):
    """A 2-atom cubic bcc cell of edge a, deformed by the matrix strain."""
    cell = [[a, 0.0, 0.0], [0.0, a, 0.0], [0.0, 0.0, a]]
    atoms = [(0.0, 0.0, 0.0), (0.5 * a, 0.5 * a, 0.5 * a)]
    if strain:
        def mul(v):
            return [sum(strain[r][c] * v[c] for c in range(3))
                    for r in range(3)]
        cell = [mul(v) for v in cell]
        atoms = [mul(p) for p in atoms]
    return cell, atoms


def fcc(a):
    """A 4-atom cubic fcc cell of edge a."""
    cell = [[a, 0.0, 0.0], [0.0, a, 0.0], [0.0, 0.0, a]]
    atoms = [(0.0, 0.0, 0.0), (0.5 * a, 0.5 * a, 0.0),
             (0.5 * a, 0.0, 0.5 * a), (0.0, 0.5 * a, 0.5 * a)]
    return cell, atoms


def dimer(r):
    """Two Ta atoms r apart in a cubic box of 30 A."""
    return ([[30.0, 0.0, 0.0], [0.0, 30.0, 0.0], [0.0, 0.0, 30.0]],
            [(5.0, 5.0, 5.0), (5.0 + r, 5.0, 5.0)])


def write_db(path, structures):
    with open(path, 'w', encoding='utf-8') as fh:
        for i, (cell, atoms) in enumerate(structures):
            fh.write(f's{i}\n1 1 1\n0 0\n')
            for v in cell:
                fh.write(' '.join(f'{x:.12f}' for x in v) + '\n')
            fh.write('0 0 0\n0 0 0\n0 0 0\n')
            for p in atoms:
                fh.write('Ta ' + ' '.join(f'{x:.12f}' for x in p)
                         + ' 0 0 0\n')
            fh.write('\n')


def column(path):
    """The predicted column of a .pred file."""
    out = []
    with open(path, encoding='utf-8') as fh:
        for line in fh:
            t = line.split()
            if t and t[0].isdigit():
                out.append(float(t[2]))
    return out


class Model:
    """Predictions of one potential on generated structures."""

    def __init__(self, tadah, pot, workdir):
        self.tadah, self.pot, self.dir = tadah, pot, workdir
        os.makedirs(workdir, exist_ok=True)

    def predict(self, structures, stress=False):
        """The energies per atom and, with stress, the pressures-like trace
        / 3 of each stress tensor (eV/A^3)."""
        write_db(os.path.join(self.dir, 'cells.tadahdb'), structures)
        run(self.tadah, ['predict', '-p', self.pot, '-d', 'cells.tadahdb']
            + (['-S'] if stress else []), self.dir)
        e = column(os.path.join(self.dir, 'energy.pred'))
        if not stress:
            return e, None
        s = column(os.path.join(self.dir, 'stress.pred'))
        return e, [(s[6 * i] + s[6 * i + 3] + s[6 * i + 5]) / 3.0
                   for i in range(len(structures))]

    def zero_stress_lattice(self, lo=3.15, hi=3.45, step=0.005):
        """The bcc lattice constant where the stress trace changes sign,
        by linear interpolation on a grid, refined once."""
        def crossing(grid):
            _, p = self.predict([bcc(a) for a in grid], stress=True)
            for i in range(len(grid) - 1):
                if p[i] == 0.0:
                    return grid[i]
                if p[i] * p[i + 1] < 0.0:
                    return grid[i] + (grid[i + 1] - grid[i]) * p[i] / (
                        p[i] - p[i + 1])
            raise RuntimeError(f'{self.pot}: no zero of the stress of bcc '
                               f'between {grid[0]} and {grid[-1]} A')
        n = int(round((hi - lo) / step))
        a = crossing([lo + step * i for i in range(n + 1)])
        return crossing([a - 0.002 + 0.0005 * i for i in range(9)])


def quoted_checks(tadah, src, work):
    """(name, value, quoted, tolerance) of every number checked."""
    out = []
    shipped = os.path.join(src, 'pot.tadah')

    # tadah predict -F -S -A on db.predict
    d = os.path.join(work, 'predict')
    os.makedirs(d)
    shutil.copy(os.path.join(src, 'db.predict'), d)
    text = run(tadah, ['predict', '-p', shipped, '-d', 'db.predict', '-F',
                       '-S', '-A'], d)
    for name, label, q, tol in (
            ('energy RMSE (meV/atom)', 'Energy RMSE (meV/atom)', 0.29,
             0.005),
            ('force RMSE (eV/A)', 'Force RMSE (eV/Å)', 0.032, 0.0005),
            ('stress RMSE (GPa)', 'Stress RMSE (GPa)', 0.049, 0.0005)):
        m = re.search(re.escape(label) + r'\s+(\S+)', text)
        if not m:
            raise RuntimeError(f'no row "{label}" in the output of tadah '
                               'predict -A')
        out.append((name, float(m.group(1)), q, tol))

    # bcc and fcc Ta, and the dimer, of the shipped potential
    m = Model(tadah, shipped, os.path.join(work, 'shipped'))
    a0 = m.zero_stress_lattice()
    out.append(('bcc lattice constant (A)', a0, 3.288, 0.0005))
    (e0,), _ = m.predict([bcc(a0)])
    out.append(('bcc energy (eV/atom)', e0, -8.104, 0.0005))
    h = 0.002
    ev, _ = m.predict([bcc(a0 * (1.0 + x)) for x in (-h, 0.0, h)])
    v = [(a0 * (1.0 + x)) ** 3 / 2.0 for x in (-h, 0.0, h)]  # per atom
    d2 = ((ev[2] - ev[1]) / (v[2] - v[1]) - (ev[1] - ev[0]) / (v[1] - v[0])) \
        / ((v[2] - v[0]) / 2.0)
    out.append(('bulk modulus (GPa)', v[1] * d2 * GPA, 115.0, 0.5))
    e = 0.002
    ec, _ = m.predict([bcc(a0, [[1 + e, 0, 0], [0, 1 - e, 0], [0, 0, 1]]),
                       bcc(a0),
                       bcc(a0, [[1 - e, 0, 0], [0, 1 + e, 0], [0, 0, 1]])])
    # E/atom = E0 + 2 C' (V/atom) e^2 for the strain diag(e, -e, 0)
    cprime = (ec[0] - 2.0 * ec[1] + ec[2]) / (e * e) / 4.0 \
        / (a0 ** 3 / 2.0) * GPA
    out.append(("C' (GPa)", cprime, 57.0, 0.5))
    ef, _ = m.predict([fcc(4.0 + 0.005 * i) for i in range(120)])
    eb, _ = m.predict([bcc(3.1 + 0.0025 * i) for i in range(120)])
    out.append(('fcc above bcc (eV/atom)', min(ef) - min(eb), 0.117, 0.0005))
    grid = [2.15 + 0.001 * i for i in range(201)]
    ek, _ = m.predict([bcc(a) for a in grid])
    minima = [(grid[i], ek[i]) for i in range(1, len(grid) - 1)
              if ek[i] < ek[i - 1] and ek[i] < ek[i + 1]]
    if len(minima) != 1:
        raise RuntimeError(f'{len(minima)} local energy minima of bcc '
                           'between 2.15 and 2.35 A, expected one')
    fine = [minima[0][0] - 0.002 + 0.0001 * i for i in range(41)]
    ek, _ = m.predict([bcc(a) for a in fine])
    k = ek.index(min(ek))
    out.append(('compressed bcc minimum (A)', fine[k], 2.25, 0.005))
    out.append(('compressed bcc minimum (eV/atom)', ek[k], -2.72, 0.005))
    er, _ = m.predict([bcc(3.29), bcc(3.8)])
    out.append(('rise from 3.29 to 3.8 A (eV/atom)', er[1] - er[0], 5.2,
                0.05))
    ed, _ = m.predict([dimer(0.2), dimer(0.001)])
    out.append(('dimer at 0.2 A (eV)', 2.0 * ed[0], 12.3, 0.05))
    out.append(('dimer at 0.001 A (eV)', 2.0 * ed[1], 13.6, 0.05))

    # fits of db.train other than config.train's
    fits = os.path.join(work, 'fits')
    os.makedirs(fits)
    shutil.copy(os.path.join(src, 'db.train'), fits)
    with open(os.path.join(src, 'config.train'), encoding='utf-8') as fh:
        config = fh.read()
    if not re.search(r'^STRESS\s+true\b', config, re.M):
        raise RuntimeError('config.train has no line STRESS true')
    no_stress = re.sub(r'^STRESS\s+true\b', 'STRESS false', config,
                       flags=re.M)
    with open(os.path.join(fits, 'e.train'), 'w', encoding='utf-8') as fh:
        fh.write(no_stress)
    shutil.copy(os.path.join(src, 'config.train'), fits)
    for name, argv, quoted in (
            ('a0, energies only (A)', ['-c', 'e.train', '-o', 'e.tadah'],
             3.381),
            ('a0, energies and forces (A)',
             ['-c', 'e.train', '-F', '-o', 'ef.tadah'], 3.389),
            ('a0, energies, forces and stresses (A)',
             ['-c', 'config.train', '-F', '-o', 'efs.tadah'], 3.294)):
        run(tadah, ['train'] + argv + ['-v', '0'], fits)
        pot = os.path.join(fits, argv[argv.index('-o') + 1])
        fit = Model(tadah, pot, os.path.join(work, os.path.basename(pot)))
        out.append((name, fit.zero_stress_lattice(), quoted, 0.0005))

    # LAMBDA -1 of the first 100 structures of db.train with fweight 0
    lam = os.path.join(work, 'lambda')
    os.makedirs(lam)
    with open(os.path.join(src, 'db.train'), encoding='utf-8') as fh:
        blocks = [b for b in fh.read().split('\n\n') if b.strip()]
    if len(blocks) < 100:
        raise RuntimeError(f'db.train has {len(blocks)} structures')
    with open(os.path.join(lam, 'first100.tadahdb'), 'w',
              encoding='utf-8') as fh:
        for b in blocks[:100]:
            lines = b.strip('\n').split('\n')
            w = lines[1].split()
            lines[1] = f'{w[0]} 0 {w[2]}'
            fh.write('\n'.join(lines) + '\n\n')
    lam_config = re.sub(r'^DBFILE\s.*$', 'DBFILE first100.tadahdb',
                        no_stress, flags=re.M)
    lam_config = re.sub(r'^LAMBDA\s.*$', 'LAMBDA -1', lam_config, flags=re.M)
    with open(os.path.join(lam, 'lambda.train'), 'w', encoding='utf-8') as fh:
        fh.write(lam_config + '\nOALGO 3\nFORCE true\n')
    text = run(tadah, ['train', '-c', 'lambda.train', '-o', 'lambda.tadah'],
               lam)
    m = re.search(r'evidence approximation: ([0-9.eE+-]+)\)', text)
    if not m:
        raise RuntimeError('no lambda of the evidence approximation in the '
                           'balance table of the fit')
    out.append(('lambda of LAMBDA -1, 100 structures of fweight 0',
                float(m.group(1)), 1.9e-5, 0.05e-5))
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--tadah', required=True)
    ap.add_argument('--quickstart', required=True)
    ap.add_argument('--workdir', required=True)
    args = ap.parse_args()

    src = os.path.abspath(args.quickstart)
    work = os.path.abspath(args.workdir)
    shutil.rmtree(work, ignore_errors=True)
    os.makedirs(work)
    try:
        checks = quoted_checks(os.path.abspath(args.tadah), src, work)
    except (RuntimeError, OSError, ValueError) as e:
        print(f'FAIL: {e}')
        return 1
    bad = 0
    for name, value, quoted, tol in checks:
        ok = abs(value - quoted) <= tol * (1.0 + 1e-9)
        bad += not ok
        print(f'{name}: {value:.6g} (quoted {quoted:g} +- {tol:g})'
              + ('' if ok else '  FAIL'))
    if bad:
        print(f'FAIL: {bad} quoted number(s) differ from what tadah '
              'computes')
        return 1
    print(f'PASS: the {len(checks)} numbers the Quick Start quotes')
    return 0


if __name__ == '__main__':
    sys.exit(main())
