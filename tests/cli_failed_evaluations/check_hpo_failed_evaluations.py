#!/usr/bin/env python3
"""`tadah hpo` when evaluations fail.

Runs `tadah hpo` on a tiny self-contained problem (four Ta atoms, D2_BP,
no LAMMPS) whose evaluations fail, and checks what the run leaves behind.
A failed evaluation's loss is FAILSCORE, here the default, the largest
double; only a usable (feasible) loss can become the incumbent.

Cases (--case):

  random_skip  TADAH:RANDOM, MAXEVAL 20; a validation skip cap no potential
               meets fails every evaluation.
  gfs_throws   DLIB:GFS, MAXEVAL 24, eight dimensions; every training throws
               (OALGO 3 with BIAS true and LAMBDA != 0). This is how
               examples/CLI/ex_2 failed before its OALGO fix: dlib crashed
               (SIGSEGV) at its first trust-region step.
  gfs_mixed    DLIB:GFS, MAXEVAL 40, one dimension; the three starting
               points fail, the upper half of the box is usable. dlib
               crashed here too. GFS picks some points at random, and which
               ones depends on the timing of its evaluations: the usable
               half is wide enough for 40 evaluations to reach it.

For random_skip and gfs_throws, no evaluation produced a usable loss:

  * tadah *exits* with status 1 (not killed by a signal) and says on stderr
    "no evaluation produced a usable loss; no best_pot.tadah was written";
  * the journal trailer reads "STOPPED -- no evaluation produced a usable
    loss", and its DIAGNOSTICS count every evaluation as an unusable loss
    and do not claim that every evaluation produced one;
  * loss.tadah has one row per evaluation, numbered 1, 2, 3, ..., each
    FAILSCORE; there is no best_pot.tadah.

For gfs_mixed the run ends as a successful one: status 0, "COMPLETED",
best_pot.tadah holds the lowest loss.tadah row, and every failed
evaluation's row records FAILSCORE.

Every file lives in a fresh temporary directory, removed afterwards unless
--keep is given. No network, no LAMMPS.
"""
import argparse
import os
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile

STRUCTURE = """test_bcc_ta
1.0 1.0 1.0
{energy} 0.0
3.301 0.0 0.0
0.0 3.301 0.0
0.0 0.0 6.602
0.0 0.0 0.0
0.0 0.0 0.0
0.0 0.0 0.0
Ta {x0} 0.0 0.0  0.0 0.0 0.0
Ta {x1} 1.6505 1.6505  0.0 0.0 0.0
Ta {x0} 0.0 3.301  0.0 0.0 0.0
Ta {x1} 1.6505 4.9515  0.0 0.0 0.0

"""

CONFIG_TADAH = """DBFILE train.db
INIT2B true
INITMB false
TYPE2B D2_BP 4 4 Ta Ta
CGRID2B 0.5 1.4 2.4 3.4
SGRID2B 3.0 5.0 4.0 2.0
RCTYPE2B Cut_Cos
RCUT2B 5.0
MODEL M_BLR BF_Linear
LAMBDA 1e-3
OALGO {oalgo}
BIAS {bias}
"""

OBJECTIVE = """LOSS L2
QFLUSH 1
DIGITS 15
"""

CASES = {
    "random_skip": {
        "oalgo": 4, "bias": "false", "evals": 20, "usable": False,
        "hpo": """OPTIMIZER
  STRATEGY
    TYPE PLAIN
    LIB TADAH
    ALGO RANDOM
    MAXEVAL 20
    SEED 3
  ENDSTRATEGY
ENDOPTIMIZER
""" + OBJECTIVE + """PC_ERMSE 0 1 --skip-above 1e-30
OPTIM CGRID2B (1) 0.1 0.9
OPTIM CGRID2B (2) 1.0 2.0
OPTIM SGRID2B (2) 4.0 6.0
""",
    },
    "gfs_throws": {
        "oalgo": 3, "bias": "true", "evals": 24, "usable": False,
        "hpo": """OPTIMIZER
  STRATEGY
    TYPE PLAIN
    LIB DLIB
    ALGO GFS
    MAXEVAL 24
  ENDSTRATEGY
ENDOPTIMIZER
""" + OBJECTIVE + """PC_ERMSE 0 1
OPTIM CGRID2B (1-4) 0.1 5.0
OPTIM SGRID2B (1-4) 0.1 5.0
""",
    },
    "gfs_mixed": {
        # CGRID2B[2] in [1, 2]: the validation ERMSE falls from 10.0 to
        # 9.75 and passes the 9.9 cap near 1.45. The starting points, the
        # Sobol points 1.25 and 1.375 (block 1 of METHOD SOBOL 2: the chain
        # hands its first method the SEED itself) and the configuration's
        # 1.4, all fail.
        "oalgo": 4, "bias": "false", "evals": 40, "usable": True,
        "tadah_grids": ("CGRID2B 0.5 1.4", "SGRID2B 3.0 5.0", 2),
        "hpo": """OPTIMIZER
  INIT
    METHOD SOBOL 2
    METHOD CONFIG
    SEED 1
  ENDINIT
  STRATEGY
    TYPE PLAIN
    LIB DLIB
    ALGO GFS
    MAXEVAL 40
  ENDSTRATEGY
ENDOPTIMIZER
""" + OBJECTIVE + """PC_ERMSE 0 1 --skip-above 9.9
OPTIM CGRID2B (2) 1.0 2.0
""",
    },
}

NO_USABLE_STATUS = "STOPPED -- no evaluation produced a usable loss"
NO_USABLE_ERROR = ("no evaluation produced a usable loss; no best_pot.tadah "
                   "was written")
# The default FAILSCORE, the largest double, as loss.tadah prints it with
# DIGITS 15.
FAILSCORE_ROW = "1.797693134862316e+308"
# loss.tadah prints 16 significant digits: losses closer than this cannot
# be ordered from the file.
PRINT_RESOLUTION = 2e-15


def write_inputs(work, case):
    for name, offset, energy in (("train.db", 0.0, -0.001),
                                 ("val.db", 0.07, -0.04)):
        (work / name).write_text(STRUCTURE.format(
            energy=energy, x0=repr(0.0 + offset), x1=repr(1.6505 + offset)))
    (work / "config.val").write_text("DBFILE val.db\n")
    tadah = CONFIG_TADAH.format(oalgo=case["oalgo"], bias=case["bias"])
    if "tadah_grids" in case:
        cgrid, sgrid, n = case["tadah_grids"]
        tadah = (tadah.replace("TYPE2B D2_BP 4 4 Ta Ta",
                               f"TYPE2B D2_BP {n} {n} Ta Ta")
                      .replace("CGRID2B 0.5 1.4 2.4 3.4", cgrid)
                      .replace("SGRID2B 3.0 5.0 4.0 2.0", sgrid))
    (work / "config.tadah").write_text(tadah)
    (work / "config.hpo").write_text(case["hpo"])


def data_rows(path):
    if not path.exists():
        return []
    rows = []
    for line in path.read_text().splitlines():
        s = line.strip()
        if s and not s.startswith("#"):
            rows.append(s.split())
    return rows


def journal_value(journal, key):
    m = re.search(re.escape(key) + r"\s*: ([^\n]*)", journal)
    return m.group(1).strip() if m else None


def check_files(work, case, rc, stderr, journal):
    """Failures, as a list."""
    failures = []
    rows = data_rows(work / "loss.tadah")
    iters = [int(r[0]) for r in rows]
    if iters != list(range(1, case["evals"] + 1)):
        failures.append(f"loss.tadah rows are {iters}, expected 1 .. "
                        f"{case['evals']}")
    failed = [r for r in rows if r[-1] == FAILSCORE_ROW]
    status = journal_value(journal, "Optimiser status")
    unusable = journal_value(journal, "Unusable losses")
    unusable_n = int(unusable.split()[0]) if unusable else None
    if unusable_n != len(failed):
        failures.append(f"journal 'Unusable losses' is {unusable!r}; "
                        f"loss.tadah has {len(failed)} FAILSCORE rows")
    if "[ok] Every evaluation produced a usable loss" in journal:
        failures.append("the journal claims every evaluation produced a "
                        "usable loss")

    if not case["usable"]:
        if rc != 1:
            failures.append(f"exit status {rc}, expected 1")
        if NO_USABLE_ERROR not in stderr:
            failures.append(f"stderr lacks {NO_USABLE_ERROR!r}")
        if status != NO_USABLE_STATUS:
            failures.append(f"journal status {status!r}, expected "
                            f"{NO_USABLE_STATUS!r}")
        if len(failed) != len(rows):
            failures.append(f"{len(rows) - len(failed)} loss.tadah rows are "
                            f"not FAILSCORE")
        if "[!] No evaluation produced a usable loss." not in journal:
            failures.append("the journal DIAGNOSTICS do not say that no "
                            "evaluation produced a usable loss")
        if (work / "best_pot.tadah").exists():
            failures.append("best_pot.tadah exists")
        return failures

    if rc != 0:
        failures.append(f"exit status {rc}, expected 0")
    if NO_USABLE_ERROR in stderr:
        failures.append(f"stderr says {NO_USABLE_ERROR!r}")
    if status != "COMPLETED":
        failures.append(f"journal status {status!r}, expected 'COMPLETED'")
    if any(r[-1] != FAILSCORE_ROW for r in rows[:3]):
        failures.append("the three starting points did not all fail: "
                        f"{[r[-1] for r in rows[:3]]}")
    usable = {int(r[0]): float(r[-1]) for r in rows if r[-1] != FAILSCORE_ROW}
    if not usable:
        failures.append("no evaluation produced a usable loss")
        return failures
    if any(v > 98.01 for v in usable.values()):
        failures.append("a row is neither FAILSCORE nor a usable loss: "
                        f"{sorted(usable.values())[-1]!r}")
    header = {}
    best = work / "best_pot.tadah"
    if not best.exists():
        failures.append("best_pot.tadah is missing")
        return failures
    for line in best.read_text().splitlines():
        m = re.match(r"#\s*(HPO_BEST_LOSS|HPO_SOURCE_EVAL)\s+(\S+)", line)
        if m:
            header[m.group(1)] = m.group(2)
    lowest = min(usable.values())
    candidates = {i for i, v in usable.items()
                  if abs(v - lowest) <= PRINT_RESOLUTION * abs(lowest)}
    if int(header.get("HPO_SOURCE_EVAL", "0")) not in candidates:
        failures.append(f"best_pot.tadah holds {header}, the lowest "
                        f"loss.tadah row(s) are {sorted(candidates)}")
    return failures


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--tadah", required=True, help="path to the tadah binary")
    ap.add_argument("--case", required=True, choices=sorted(CASES))
    ap.add_argument("--keep", action="store_true",
                    help="keep the working directory")
    opts = ap.parse_args()

    case = CASES[opts.case]
    tadah = os.path.abspath(opts.tadah)
    work = pathlib.Path(tempfile.mkdtemp(prefix="tadah_hpo_failed_"))
    try:
        write_inputs(work, case)
        env = dict(os.environ, OMP_NUM_THREADS="1")
        env.pop("TADAH_FAILSCORE_GRADING", None)
        try:
            proc = subprocess.run(
                [tadah, "hpo", "--hpotarget", "config.hpo",
                 "--validation", "config.val", "-c", "config.tadah"],
                cwd=work, env=env, capture_output=True, text=True,
                timeout=240)
        except subprocess.TimeoutExpired:
            print("FAIL: tadah hpo did not end within 240 s")
            return 1
        rc = proc.returncode
        print(f"tadah hpo ({opts.case}): exit status {rc}")
        journal = (work / "hpo_run.log").read_text() \
            if (work / "hpo_run.log").exists() else ""
        print(f"journal status: {journal_value(journal, 'Optimiser status')}")

        if rc < 0:
            failures = [f"killed by signal {-rc} instead of exiting"]
        else:
            failures = check_files(work, case, rc, proc.stderr, journal)
        if failures:
            print("--- stdout (tail) ---\n" + proc.stdout[-3000:])
            print("--- stderr (tail) ---\n" + proc.stderr[-3000:])
            for f in failures:
                print("FAIL: " + f)
            return 1
        print("PASS")
        return 0
    finally:
        if opts.keep:
            print(f"kept {work}")
        else:
            shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
