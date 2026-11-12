#!/usr/bin/env python3
"""pair_style tadah domain-decomposed on several MPI ranks (`mpirun lmp`).

Runs one case: a potential of tests_data/pots under the LAMMPS executables
the build names, on a cell of displaced bcc Ta (make_cell.in, written once by
the reference executable so every run starts from the same state), and
compares every multi-rank run with the reference run by atom id
(run_case.in). The reference is the serial lmp (TADAH_LMP_SERIAL) when it is
given, else the MPI lmp on one rank. The MPI lmp (TADAH_LMP_MPI) runs under
the launcher of its MPI (TADAH_LMP_MPIEXEC) on

  1, 2 and 4 ranks   8x8x8 cell (1024 atoms, 26.4 A), the default grid;
  4 ranks, 4 1 1     5x5x5 cell (250 atoms, 16.5 A): a sub-domain is 4.125 A
                     wide, less than the ghost cutoff, so ghost atoms come
                     through two swaps per direction.

and every run is compared at step 0 (`run 0`) and after 100 NVE steps:

  step 0    per-atom force             |dF|  <= 1e-8 eV/A
            per-atom energy (pe/atom)  |de|  <= 1e-8 eV
            per-atom virial (stress/atom) |dS| <= 1e-8 max|S|
            potential energy           |dPE| <= 1e-10 |PE|
            pressure tensor            |dP|  <= 1e-8 max(|Pxx|,|Pyy|,|Pzz|)
  step 100  unwrapped positions        |dx|  <= 1e-8 A
            velocities                 |dv|  <= 1e-7 A/ps
            potential energy           |dPE| <= 1e-9 |PE|

The runs differ in the order in which contributions are summed, nothing
else, so they agree to round-off. The tolerances are 70 to 4300 times the
largest difference measured in these cases, that of the DM_REAM fixture
(2.5e-11 eV/A, 1.4e-10 eV, 6e-11 max|S|, 3e-13 |PE| and 1.1e-11 max|P_aa|
at step 0; 2.3e-12 A, 4.8e-11 A/ps and 7e-13 |PE| after 100 steps), which
two serial runs that differ only in atom order (atom_modify sort) show as
well: its refit weights, of order 1e3 and cancelling, amplify round-off. For
the other potentials every difference is below 2e-4 of its tolerance (forces
1e-15 to 2e-14 eV/A). A pair_tadah whose ghost atoms lose their densities and
pair descriptors, or get a stale or wrong dF/drho, gives forces wrong by 0.4
to 16 eV/A in these cases.

Each multi-rank run must also report its rank count ("on N procs": a
launcher of another MPI starts N one-rank jobs instead) and at least one
neighbour-list build in the 100 steps (atoms move between ranks), and each
kernel variant must print, or not print, the fast-path line of pair_coeff,
so that the case runs the kernel it names.

The dimer case runs a DIMER potential, the published Tadah!N2 (tadah.n2:
rigid N2 molecules, compute_dimers, which needs newton off), on 512 N2
molecules that the script writes (atom_style bond, one bond each; 8x8x8
cells of 4 A, 32 A, each molecule's centre moved by up to 0.3 A along each
axis from its cell's centre and its bond turned to a direction drawn from a
fixed seed; no molecule crosses the box edge) and compares step 0 (`run 0`)
of the MPI lmp on 1, 2 and 4 ranks and on 4 ranks in a 4 1 1 grid (8 A
sub-domains, less than the ghost cutoff of 14 A) with the reference run, to
the step-0 tolerances above, and checks in every run, the reference
included, that the per-atom energies sum to the energy:

            sum(pe/atom) - PotEng      |d|   <= 1e-10 |PE|

The molecules are not moved: dynamics of rigid molecules needs constraints
(fix shake or rigid) that this check leaves out. The largest differences
measured (PotEng -16.64 eV): forces 5e-14 eV/A (max|F| 68 eV/A), pe/atom
1.1e-16 eV, stress/atom 4.5e-8 bar A^3 (max|S| 5.9e7), PotEng 2.8e-13 eV,
pressure tensor 1.8e-8 bar (5.7e5) and sum(pe/atom) - PotEng 2.2e-13 eV,
all far below the tolerances. A compute_dimers that sets the per-atom
energy of each molecule pair instead of adding it fails the sum in every
run.

Exit status: 0 every comparison holds; 1 one does not, or a run fails, or an
executable that is named is not one; 77 (Skipped) the MPI lmp or its launcher
is not named, or, for the dimer case, an lmp it runs has no atom_style bond
(part of the LAMMPS MOLECULE package; `lmp -h` lists the atom styles).
"""
import argparse
import math
import os
import random
import re
import shlex
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SKIP = 77
RUN_TIMEOUT = 600  # seconds, per LAMMPS run
NSTEPS = 100

# case name -> (elements of the LAMMPS types, newton settings, variants); a
# variant is (label, potential file, extra key lines, kernel line expected or
# None, kernel line forbidden or None)
FAST = r"Tadah! EAM-collapse fast path ON"
PAIR_FAST = r"Tadah! pair-collapse fast path ON"
ANY_FAST = r"fast path ON"
CASES = {
    # The Quick Start potential: D2_Blip + DM_mBlipEnv, Kern_Linear, the
    # generic many-body kernel (compute_2b_mb_half): reverse comm of the
    # densities and pair descriptors, forward comm of dF/drho.
    "quickstart": (["Ta"], ["on", "off"], [
        ("generic kernel", "ta_quickstart_blip_env.tadah", [], None, ANY_FAST),
    ]),
    # Two LAMMPS types mapped to two elements (ghost atoms included) in the
    # generic kernel.
    "two_species": (["Ta", "Nb"], ["on", "off"], [
        ("generic kernel", "tanb_blip_env_any_pair.tadah", [], None, ANY_FAST),
    ]),
    # D2_Knot5 + DM_REAM (the refit family): the EAM fast path (scalar
    # rho_bar reverse comm, F' forward comm) and, with EAMFAST false, the
    # generic kernel with the DM_REAM descriptor.
    "dm_ream": (["Ta"], ["on", "off"], [
        ("EAM fast path", "ta_ream_knot5.tadah", [], FAST, None),
        ("generic kernel", "ta_ream_knot5.tadah", ["EAMFAST false"], None,
         ANY_FAST),
    ]),
    # A non-linear model: the full-list kernel (compute_2b_mb_full), which
    # needs newton on.
    "nonlinear": (["Ta"], ["on"], [
        ("full-list kernel", "ta_poly2_blip_env.tadah", [], None, ANY_FAST),
    ]),
    # Pure two-body: compute_2b_fast and, with EAMFAST false, compute_2b.
    "pair_2b": (["Ta"], ["on", "off"], [
        ("pair fast path", "ta_lj_cutcos.tadah", [], PAIR_FAST, None),
        ("generic kernel", "ta_lj_cutcos.tadah", ["EAMFAST false"], None,
         ANY_FAST),
    ]),
}
# The dimer case (see the docstring): the potential, its element, the
# molecules (cells per edge, cell size in A, the largest move of a centre
# along each axis in A, the seed) and the legs (label, MPI ranks, processors
# command), all on the one cell.
DIMER_POT = "tadah.n2"
DIMER_SPECIES = "N"
DIMER_CELLS, DIMER_A, DIMER_JITTER, DIMER_SEED = 8, 4.0, 0.3, 7
DIMER_LEGS = [
    ("1 rank", 1, "* * *"),
    ("2 ranks", 2, "* * *"),
    ("4 ranks", 4, "* * *"),
    ("4 ranks, 4 1 1", 4, "4 1 1"),
]
# (label, cell, MPI ranks, processors command)
LEGS = [
    ("1 rank", "cell8", 1, "* * *"),
    ("2 ranks", "cell8", 2, "* * *"),
    ("4 ranks", "cell8", 4, "* * *"),
    ("4 ranks, 4 1 1", "cell5", 4, "4 1 1"),
]
CELLS = {"cell8": 8, "cell5": 5}


class Failure(Exception):
    pass


def atom_styles(cmd):
    """The atom styles that `lmp -h` lists for the lmp started by @p cmd."""
    out = run(cmd + ["-h"], os.getcwd(), "no-log")
    block = re.search(r"^\* Atom styles:\s*\n(.*?)\n\s*\n", out, re.M | re.S)
    if not block:
        raise Failure(f"no atom style list in the output of "
                      f"{shlex.join(cmd + ['-h'])}")
    return set(block.group(1).split())


def executable(path, var):
    """None for an empty value; the path if it is an executable file;
    Failure otherwise (a variable that is set must be usable)."""
    if not path:
        return None
    if not (os.path.isfile(path) and os.access(path, os.X_OK)):
        raise Failure(f"{var}={path} is not an executable file")
    return path


def run(cmd, cwd, log):
    try:
        p = subprocess.run(cmd, cwd=cwd, stdout=subprocess.PIPE,
                           stderr=subprocess.STDOUT, text=True,
                           timeout=RUN_TIMEOUT)
    except subprocess.TimeoutExpired:
        raise Failure(f"no exit within {RUN_TIMEOUT} s: {shlex.join(cmd)}")
    if p.returncode != 0:
        tail = ""
        if os.path.exists(os.path.join(cwd, log)):
            with open(os.path.join(cwd, log)) as f:
                tail = "".join(f.readlines()[-15:])
        raise Failure(f"exit status {p.returncode}: {shlex.join(cmd)}\n"
                      f"output:\n{p.stdout[-3000:]}\nlog tail:\n{tail}")
    return p.stdout


def read_dump(path):
    """{id: [values]} of a dump sorted by id, and its column names."""
    with open(path) as f:
        lines = f.read().splitlines()
    head = next(i for i, l in enumerate(lines) if l.startswith("ITEM: ATOMS"))
    cols = lines[head].split()[2:]
    rows = {}
    for line in lines[head + 1:]:
        t = line.split()
        if not t:
            continue
        rows[int(t[0])] = [float(v) for v in t[1:]]
    return cols[1:], rows


def thermo_blocks(log):
    """The thermo rows of each run, as lists of {column: value}."""
    blocks, header = [], None
    for line in log.splitlines():
        t = line.split()
        if t and t[0] == "Step":
            header, rows = t, []
            blocks.append(rows)
            continue
        if header and line.startswith("Loop time"):
            header = None
            continue
        if header and len(t) == len(header):
            try:
                rows.append(dict(zip(header, (float(v) for v in t))))
            except ValueError:
                pass  # a WARNING line of as many words
    return blocks


def max_diff(ref, oth, cols, names):
    """max |ref - oth| over the atoms and the columns @p names, and where."""
    idx = [cols.index(n) for n in names]
    worst, where = 0.0, None
    for k, r in ref.items():
        o = oth[k]
        for c in idx:
            d = abs(r[c] - o[c])
            if not d <= worst:  # also catches nan
                worst, where = d, (k, cols[c])
    return worst, where


def max_abs(rows, cols, names):
    idx = [cols.index(n) for n in names]
    return max(abs(r[c]) for r in rows.values() for c in idx)


class Checker:
    def __init__(self):
        self.failures = []

    def check(self, what, value, tol, where=None):
        ok = value <= tol  # False for nan
        mark = "ok" if ok else "FAIL"
        loc = f" (atom {where[0]}, {where[1]})" if where else ""
        print(f"      {what:<34} {value:10.3e} <= {tol:9.3e}  {mark}{loc}")
        if not ok:
            self.failures.append(what)
        return ok


def compare_step0(chk, ref_out, out, natoms):
    """The step-0 comparisons of @p out with @p ref_out: the dumps and the
    first thermo row of each log. Returns the thermo blocks of both."""
    cols, r0 = read_dump(ref_out + ".0.dump")
    cols2, o0 = read_dump(out + ".0.dump")
    if cols != cols2 or set(r0) != set(o0) or len(r0) != natoms:
        raise Failure(f"{out}.0.dump: atoms or columns differ from the "
                      f"reference ({len(o0)} atoms, {len(r0)} in the "
                      f"reference, {natoms} expected)")
    stress = [f"c_satom[{i}]" for i in range(1, 7)]
    d, w = max_diff(r0, o0, cols, ["fx", "fy", "fz"])
    chk.check("step 0 force (eV/A)", d, 1e-8, w)
    d, w = max_diff(r0, o0, cols, ["c_peatom"])
    chk.check("step 0 pe/atom (eV)", d, 1e-8, w)
    d, w = max_diff(r0, o0, cols, stress)
    chk.check("step 0 stress/atom (bar A^3)", d,
              1e-8 * max_abs(r0, cols, stress), w)

    with open(ref_out + ".log") as f:
        tr = thermo_blocks(f.read())
    with open(out + ".log") as f:
        to = thermo_blocks(f.read())
    t0r, t0o = tr[0][0], to[0][0]
    chk.check("step 0 PotEng (eV)", abs(t0r["PotEng"] - t0o["PotEng"]),
              1e-10 * abs(t0r["PotEng"]))
    pscale = max(abs(t0r[n]) for n in ("Pxx", "Pyy", "Pzz"))
    dp = max(abs(t0r[n] - t0o[n])
             for n in ("Pxx", "Pyy", "Pzz", "Pxy", "Pxz", "Pyz"))
    chk.check("step 0 pressure tensor (bar)", dp, 1e-8 * pscale)
    return tr, to


def compare(chk, ref_out, out, natoms):
    tr, to = compare_step0(chk, ref_out, out, natoms)

    cols, rn = read_dump(ref_out + ".N.dump")
    _, on = read_dump(out + ".N.dump")
    if set(rn) != set(on):
        raise Failure(f"{out}.N.dump: atoms differ from the reference")
    d, w = max_diff(rn, on, cols, ["xu", "yu", "zu"])
    chk.check(f"step {NSTEPS} position (A)", d, 1e-8, w)
    d, w = max_diff(rn, on, cols, ["vx", "vy", "vz"])
    chk.check(f"step {NSTEPS} velocity (A/ps)", d, 1e-7, w)
    tnr, tno = tr[-1][-1], to[-1][-1]
    if tnr["Step"] != NSTEPS or tno["Step"] != NSTEPS:
        raise Failure(f"{out}.log: no thermo row of step {NSTEPS}")
    chk.check(f"step {NSTEPS} PotEng (eV)",
              abs(tnr["PotEng"] - tno["PotEng"]), 1e-9 * abs(tnr["PotEng"]))


def check_log(chk, path, nprocs, grid, expect, forbid, multi, nruns=2):
    with open(path) as f:
        log = f.read()
    procs = re.findall(r"^Loop time of .* on (\d+) procs", log, re.M)
    ok = len(procs) == nruns and all(int(p) == nprocs for p in procs)
    print(f"      {'runs on ' + str(nprocs) + ' rank(s)':<34} "
          f"{'ok' if ok else 'FAIL (' + ', '.join(procs) + ' procs)'}")
    if not ok:
        chk.failures.append(f"{path}: rank count")
    found = re.findall(r"^\s*(\d+) by (\d+) by (\d+) MPI processor grid",
                       log, re.M)
    want = tuple(grid.split()) if "*" not in grid else None
    ok = bool(found) and (want is None or found[-1] == want)
    print(f"      {'processor grid':<34} "
          f"{' x '.join(found[-1]) if found else 'none'}"
          f"{'' if want is None else ' (' + ' x '.join(want) + ' asked)'}"
          f"  {'ok' if ok else 'FAIL'}")
    if not ok:
        chk.failures.append(f"{path}: processor grid")
    if multi:
        builds = re.findall(r"^Neighbor list builds = (\d+)", log, re.M)
        ok = bool(builds) and int(builds[-1]) >= 1
        print(f"      {'neighbour-list builds in the run':<34} "
              f"{builds[-1] if builds else 'none'} >= 1  "
              f"{'ok' if ok else 'FAIL'}")
        if not ok:
            chk.failures.append(f"{path}: no neighbour-list build")
    if expect and not re.search(re.escape(expect), log):
        print(f"      kernel line '{expect}' missing  FAIL")
        chk.failures.append(f"{path}: kernel line")
    if forbid and re.search(re.escape(forbid), log):
        print(f"      kernel line '{forbid}' present  FAIL")
        chk.failures.append(f"{path}: kernel line")


def write_dimers(path, bond):
    """The molecules of the dimer case (see DIMER_*) as a LAMMPS data file
    of atom_style bond: atoms 2m-1 and 2m are molecule m, bonded. Returns
    the number of atoms."""
    rng = random.Random(DIMER_SEED)
    n, a = DIMER_CELLS, DIMER_A
    box = n * a
    atoms = []
    for i in range(n):
        for j in range(n):
            for k in range(n):
                c = [(q + 0.5) * a + rng.uniform(-DIMER_JITTER, DIMER_JITTER)
                     for q in (i, j, k)]
                while True:
                    u = [rng.gauss(0.0, 1.0) for _ in range(3)]
                    norm = math.sqrt(sum(x * x for x in u))
                    if norm > 1e-3:
                        break
                for sign in (1.0, -1.0):
                    p = [c[d] + sign * 0.5 * bond * u[d] / norm
                         for d in range(3)]
                    if not all(0.0 < x < box for x in p):
                        raise Failure(f"dimer molecule {len(atoms) // 2 + 1}"
                                      " crosses the box edge")
                    atoms.append(p)
    nmol = len(atoms) // 2
    with open(path, "w") as f:
        f.write(f"N2 molecules of check_pair_tadah_mpi.py (dimer case)\n\n"
                f"{len(atoms)} atoms\n1 atom types\n{nmol} bonds\n"
                f"1 bond types\n\n0 {box:g} xlo xhi\n0 {box:g} ylo yhi\n"
                f"0 {box:g} zlo zhi\n\nMasses\n\n1 14.007\n\n"
                f"Atoms # bond\n\n")
        for idx, p in enumerate(atoms, 1):
            f.write(f"{idx} {(idx + 1) // 2} 1 "
                    f"{p[0]:.15f} {p[1]:.15f} {p[2]:.15f}\n")
        f.write("\nBonds\n\n")
        for m in range(1, nmol + 1):
            f.write(f"{m} 1 {2 * m - 1} {2 * m}\n")
    return len(atoms)


def check_pe_sum(chk, out):
    """sum(pe/atom) == PotEng at step 0 of the run @p out."""
    with open(out + ".log") as f:
        row = thermo_blocks(f.read())[0][0]
    chk.check("step 0 sum(pe/atom) - PotEng (eV)",
              abs(row["c_pesum"] - row["PotEng"]), 1e-10 * abs(row["PotEng"]))


def run_dimer(chk, ref_cmd, ref_name, mpi_cmd, pots, workdir):
    with open(os.path.join(pots, DIMER_POT)) as f:
        dimer = re.search(r"^DIMER\s+\S+\s+(\S+)", f.read(), re.M)
    if not dimer:
        raise Failure(f"{DIMER_POT} has no DIMER line")
    bond = float(dimer.group(1))
    natoms = write_dimers(os.path.join(workdir, "dimers.data"), bond)
    shutil.copy(os.path.join(pots, DIMER_POT), workdir)
    print(f"dimer: {DIMER_POT} (compute_dimers), {natoms // 2} molecules, "
          f"bond {bond} A, newton off; reference: {ref_name}")

    def lmp_args(procs, out):
        return ["-in", os.path.join(HERE, "run_dimer.in"),
                "-log", out + ".log", "-screen", "none",
                "-var", "procs", procs,
                "-var", "data", "dimers.data",
                "-var", "pot", DIMER_POT,
                "-var", "species", DIMER_SPECIES,
                "-var", "bond", repr(bond),
                "-var", "out", out]

    ref = "dimer_ref"
    run(ref_cmd + lmp_args("* * *", ref), workdir, ref + ".log")
    print(f"    reference ({natoms} atoms)")
    check_log(chk, os.path.join(workdir, ref + ".log"), 1, "1 1 1", None,
              ANY_FAST, False, nruns=1)
    check_pe_sum(chk, os.path.join(workdir, ref))
    for leg, n, procs in DIMER_LEGS:
        out = f"dimer_np{n}_{procs.replace(' ', '')}".replace("*", "x")
        run(mpi_cmd(n) + lmp_args(procs, out), workdir, out + ".log")
        print(f"    MPI lmp, {leg} ({natoms} atoms)")
        check_log(chk, os.path.join(workdir, out + ".log"), n, procs, None,
                  ANY_FAST, False, nruns=1)
        compare_step0(chk, os.path.join(workdir, ref),
                      os.path.join(workdir, out), natoms)
        check_pe_sum(chk, os.path.join(workdir, out))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--case", required=True,
                    choices=sorted(list(CASES) + ["dimer"]))
    ap.add_argument("--lmp-serial", default="")
    ap.add_argument("--lmp-mpi", default="")
    ap.add_argument("--mpiexec", default="")
    ap.add_argument("--preflags", default="",
                    help="launcher flags before -n, one shell-quoted string")
    ap.add_argument("--pots", required=True)
    ap.add_argument("--workdir", required=True)
    a = ap.parse_args()

    try:
        serial = executable(a.lmp_serial, "TADAH_LMP_SERIAL")
        lmp_mpi = executable(a.lmp_mpi, "TADAH_LMP_MPI")
        mpiexec = executable(a.mpiexec, "TADAH_LMP_MPIEXEC")
    except Failure as e:
        print(f"FAILED: {e}")
        return 1
    missing = [v for v, p in (("TADAH_LMP_MPI", lmp_mpi),
                              ("TADAH_LMP_MPIEXEC", mpiexec)) if not p]
    if missing:
        print("Skipped: no multi-rank LAMMPS run: " + " and ".join(missing)
              + (" is" if len(missing) == 1 else " are") + " not set "
              "(TADAH_LMP_MPI: an lmp built with -DBUILD_MPI=ON and "
              "ML-TADAH; TADAH_LMP_MPIEXEC: the mpiexec of its MPI)")
        return SKIP

    pre = shlex.split(a.preflags)

    def mpi_cmd(n):
        return [mpiexec] + pre + ["-n", str(n), lmp_mpi]

    ref_cmd = [serial] if serial else mpi_cmd(1)
    ref_name = ("the serial lmp (TADAH_LMP_SERIAL)" if serial
                else "the MPI lmp on 1 rank")

    shutil.rmtree(a.workdir, ignore_errors=True)
    os.makedirs(a.workdir)
    chk = Checker()
    if a.case == "dimer":
        try:
            lmps = [(ref_name, ref_cmd)]
            if ref_cmd != mpi_cmd(1):
                lmps.append(("the MPI lmp", mpi_cmd(1)))
            lacking = [name for name, cmd in lmps
                       if "bond" not in atom_styles(cmd)]
            if lacking:
                print("Skipped: the dimer case needs atom_style bond, which "
                      + " and ".join(lacking)
                      + (" lack" if len(lacking) > 1 else " lacks")
                      + " (the LAMMPS MOLECULE package, -DPKG_MOLECULE=ON)")
                return SKIP
            run_dimer(chk, ref_cmd, ref_name, mpi_cmd, a.pots, a.workdir)
        except Failure as e:
            print(f"FAILED: {e}")
            return 1
        if chk.failures:
            print(f"FAILED: {len(chk.failures)} comparison(s) out of "
                  f"tolerance or run check(s) failed")
            return 1
        print("PASSED")
        return 0

    species, newtons, variants = CASES[a.case]
    try:
        for cell, nx in CELLS.items():
            run(ref_cmd + ["-in", os.path.join(HERE, "make_cell.in"),
                           "-log", cell + ".make.log", "-screen", "none",
                           "-var", "nx", str(nx),
                           "-var", "ntypes", str(len(species)),
                           "-var", "out", cell + ".data"],
                a.workdir, cell + ".make.log")
        for vi, (label, pot, extra, expect, forbid) in enumerate(variants):
            src = os.path.join(a.pots, pot)
            with open(src) as f:
                text = f.read()
            for line in extra:
                key = line.split()[0]
                if re.search(rf"^{key}\s", text, re.M):
                    raise Failure(f"{pot} has a {key} line already")
                text += ("" if text.endswith("\n") else "\n") + line + "\n"
            potfile = f"pot{vi}.tadah"
            with open(os.path.join(a.workdir, potfile), "w") as f:
                f.write(text)
            for newton in newtons:
                print(f"{a.case}: {pot}{' + ' + ', '.join(extra) if extra else ''}"
                      f" ({label}), newton {newton}; reference: {ref_name}")

                def lmp_args(cell, procs, out):
                    return ["-in", os.path.join(HERE, "run_case.in"),
                            "-log", out + ".log", "-screen", "none",
                            "-var", "newton", newton,
                            "-var", "procs", procs,
                            "-var", "data", cell + ".data",
                            "-var", "pot", potfile,
                            "-var", "species", " ".join(species),
                            "-var", "nsteps", str(NSTEPS),
                            "-var", "out", out]

                refs = {}
                for cell, nx in CELLS.items():
                    out = f"v{vi}_{newton}_{cell}_ref"
                    run(ref_cmd + lmp_args(cell, "* * *", out), a.workdir,
                        out + ".log")
                    refs[cell] = out
                    print(f"    reference, {cell} ({2 * nx ** 3} atoms)")
                    check_log(chk, os.path.join(a.workdir, out + ".log"), 1,
                              "1 1 1", expect, forbid, False)
                for leg, cell, n, procs in LEGS:
                    out = f"v{vi}_{newton}_{cell}_np{n}_{procs.replace(' ', '')}"
                    out = out.replace("*", "x")
                    run(mpi_cmd(n) + lmp_args(cell, procs, out), a.workdir,
                        out + ".log")
                    print(f"    MPI lmp, {leg}, {cell} "
                          f"({2 * CELLS[cell] ** 3} atoms)")
                    check_log(chk, os.path.join(a.workdir, out + ".log"), n,
                              procs, expect, forbid, True)
                    compare(chk, os.path.join(a.workdir, refs[cell]),
                            os.path.join(a.workdir, out), 2 * CELLS[cell] ** 3)
    except Failure as e:
        print(f"FAILED: {e}")
        return 1
    if chk.failures:
        print(f"FAILED: {len(chk.failures)} comparison(s) out of tolerance "
              f"or run check(s) failed")
        return 1
    print("PASSED")
    return 0


if __name__ == "__main__":
    sys.exit(main())
