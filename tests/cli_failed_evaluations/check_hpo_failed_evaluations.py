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
               (OALGO 2 with LAMBDA 0 refuses the one data row, the
               structure's energy, for the four weights of D2_BP 4; the
               startup checks read no descriptors). Every evaluation of
               examples/CLI/ex_2 failed in this way before its OALGO fix
               (OALGO 3 with BIAS true and LAMBDA != 0, which tadah hpo
               refuses when a run starts), and dlib crashed (SIGSEGV) at its
               first trust-region step.
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

For gfs_mixed the run ends as a successful one: status 0, the trailer
"DLIB:GFS: MAXEVAL reached" (the search spent its MAXEVAL; it read
"COMPLETED", which nothing recorded, before each HPO driver recorded why it
ended), best_pot.tadah holds the lowest loss.tadah row, and every failed
evaluation's row records FAILSCORE.

  fd_straddle  DLIB:BFGS from CGRID2B[2] = 1.6, PARAM fd_step 0.2: the
               finite-difference stencil of the start point is 1.8 and 1.4,
               and a skip cap fails 1.4. A difference never mixes a usable
               loss with a failure: the gradient at 1.6 is the one-sided
               difference into the usable side, 1.8 and 2.0, with the loss
               at 1.6 that dlib has just evaluated. The search goes on to
               the bound 2.0, the best, and ends "DLIB:BFGS: FTOL_ABS
               reached" after 39 evaluations, status 0; the journal names
               the difference taken without 1.4 and DIAGNOSTICS count it
               ("Mixed-kind stencils : 1").
               Until then the search ended at that gradient after three
               evaluations, 1.6, 1.8 and 1.4, with the trailer "STOPPED --
               no usable gradient: a failed evaluation in the
               finite-difference stencil".
  ceres_box    CERES:STEEPEST_DESCENT from CGRID2B[2] = 1.9, PARAM fd_step
               0.2 (the one-sided difference at 1.7 and 1.5: 2.1 is
               outside); Ceres' GradientProblemSolver has no bounds, and its
               first step proposes 0.9, outside the OPTIM box [1, 2]. No
               point outside the box is evaluated or can be the best: 0.9
               is scored as a failure graded by its distance to the box,
               0.1 of its width
               (1.797284568240756e+308, in [0.99975, 1) x FAILSCORE), with a
               WARNING and DIAGNOSTICS "Out-of-box points : 1"; it is the
               only row outside the box; the best lies in the box; the
               search ends "FAILURE (Numerical failure in line search)"
               after 41 evaluations, status 0. Until then 0.9 was evaluated
               there (a failure of the skip cap, FAILSCORE), with the
               WARNING "... lies outside its OPTIM box [1, 2] and is
               evaluated there".
  fd_inside    DLIB:BFGS from CGRID2B[2] = 1.25, inside the failure region:
               the start and its stencil 1.45 and 1.05 fail. The failed
               loss at the start fails its gradient before the stencil is
               evaluated: one evaluation, and a journal warning that names
               the loss ("DLIB:BFGS stopped: no usable gradient: a failed
               evaluation in the finite-difference stencil (the loss at
               CGRID2B[2] = 1.25 is 1.797693135e+308)"). No evaluation is
               usable: status 1, as for random_skip. The difference of two
               failures was an exact 0, a gradient dlib took for
               convergence: six evaluations (1.25, 1.45, 1.05 twice; eight
               before HPO's centre fix, each gradient evaluating 1.25
               again) and no warning.
  failscore_zero  FAILSCORE 0: a startup error at its line, status 1, no
               evaluation. With 0 there was no failure band, and a run all
               of whose evaluations failed ended COMPLETED with a
               best_pot.tadah that held a failed evaluation.

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
LAMBDA {lam}
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
        "oalgo": 2, "bias": "false", "lam": "0", "evals": 24,
        "usable": False,
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
    "fd_straddle": {
        "oalgo": 4, "bias": "false", "evals": 39, "usable": True,
        "kind": "fd_side",
        "tadah_grids": ("CGRID2B 0.5 1.6", "SGRID2B 3.0 5.0", 2),
        "hpo": """OPTIMIZER
  STRATEGY
    TYPE PLAIN
    LIB DLIB
    ALGO BFGS
    MAXEVAL 60
    PARAM fd_step 0.2
  ENDSTRATEGY
ENDOPTIMIZER
""" + OBJECTIVE + """PC_ERMSE 0 1 --skip-above 9.9
OPTIM CGRID2B (2) 1.0 2.0
""",
    },
    "ceres_box": {
        "oalgo": 4, "bias": "false", "evals": 41, "usable": True,
        "kind": "ceres_box",
        "tadah_grids": ("CGRID2B 0.5 1.9", "SGRID2B 3.0 5.0", 2),
        "hpo": """OPTIMIZER
  STRATEGY
    TYPE PLAIN
    LIB CERES
    ALGO STEEPEST_DESCENT
    MAXEVAL 60
    PARAM fd_step 0.2
  ENDSTRATEGY
ENDOPTIMIZER
""" + OBJECTIVE + """PC_ERMSE 0 1 --skip-above 9.9
OPTIM CGRID2B (2) 1.0 2.0
""",
    },
    "fd_inside": {
        "oalgo": 4, "bias": "false", "evals": 1, "usable": False,
        "kind": "fd_inside",
        "tadah_grids": ("CGRID2B 0.5 1.25", "SGRID2B 3.0 5.0", 2),
        "hpo": """OPTIMIZER
  STRATEGY
    TYPE PLAIN
    LIB DLIB
    ALGO BFGS
    MAXEVAL 60
    PARAM fd_step 0.2
  ENDSTRATEGY
ENDOPTIMIZER
""" + OBJECTIVE + """PC_ERMSE 0 1 --skip-above 9.9
OPTIM CGRID2B (2) 1.0 2.0
""",
    },
    "failscore_zero": {
        "oalgo": 4, "bias": "false", "evals": 0, "usable": False,
        "kind": "startup_error",
        "hpo": """OPTIMIZER
  STRATEGY
    TYPE PLAIN
    LIB TADAH
    ALGO RANDOM
    MAXEVAL 20
    SEED 3
  ENDSTRATEGY
ENDOPTIMIZER
""" + OBJECTIVE + """FAILSCORE 0
PC_ERMSE 0 1 --skip-above 1e-30
OPTIM CGRID2B (1) 0.1 0.9
""",
    },
}

NO_USABLE_STATUS = "STOPPED -- no evaluation produced a usable loss"
FD_SIDE_STATUS = "DLIB:BFGS: FTOL_ABS reached"
FD_SIDE_NOTE = ("finite differences: at CGRID2B[2] = 1.6 (a usable loss), the "
                "stencil point CGRID2B[2] = 1.4 (loss 1.797693135e+308) is of "
                "another kind, so the difference is taken without it: "
                "one-sided 3-point difference, at CGRID2B[2] = 1.8 and "
                "CGRID2B[2] = 2 with the loss at the point itself")
CERES_BOX_STATUS = ("STOPPED -- CERES:STEEPEST_DESCENT: FAILURE (Numerical "
                    "failure in line search)")
CERES_BOX_WARNING = ("eval 4: CGRID2B[2] = 0.9 lies outside its OPTIM box "
                     "[1, 2]: it is not evaluated, and is scored "
                     "1.797284568240756e+308, a failure graded by its "
                     "distance to the box")
CERES_BOX_SCORE_ROW = "1.797284568240756e+308"
GFS_MAXEVAL_STATUS = "DLIB:GFS: MAXEVAL reached"
FD_INSIDE_STOP = ("DLIB:BFGS stopped: no usable gradient: a failed evaluation "
                  "in the finite-difference stencil (the loss at CGRID2B[2] "
                  "= 1.25 is 1.797693135e+308)")
FAILSCORE_ZERO_ERROR = ("Error on line 13: FAILSCORE must be a positive "
                        "finite number, got '0'")
NO_USABLE_ERROR = ("no evaluation produced a usable loss; no best_pot.tadah "
                   "was written")
# The default FAILSCORE, the largest double, as loss.tadah prints it with
# DIGITS 15.
FAILSCORE_ROW = "1.797693134862316e+308"
# Its value: the 16 printed digits round above the largest double, which
# float() reads as inf.
FAILSCORE = sys.float_info.max
# A loss at or above this is a failure: FAILSCORE, a graded failure, or the
# score of a point outside the OPTIM box.
FAIL_BAND_FLOOR = 0.999 * FAILSCORE


def loss_of(text):
    """A loss.tadah value; the FAILSCORE row is the largest double."""
    return FAILSCORE if text == FAILSCORE_ROW else float(text)
# loss.tadah prints 16 significant digits: losses closer than this cannot
# be ordered from the file.
PRINT_RESOLUTION = 2e-15


def write_inputs(work, case):
    for name, offset, energy in (("train.db", 0.0, -0.001),
                                 ("val.db", 0.07, -0.04)):
        (work / name).write_text(STRUCTURE.format(
            energy=energy, x0=repr(0.0 + offset), x1=repr(1.6505 + offset)))
    (work / "config.val").write_text("DBFILE val.db\n")
    tadah = CONFIG_TADAH.format(oalgo=case["oalgo"], bias=case["bias"],
                               lam=case.get("lam", "1e-3"))
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


def check_startup_error(work, rc, stderr):
    """failscore_zero: refused before any evaluation."""
    failures = []
    if rc != 1:
        failures.append(f"exit status {rc}, expected 1")
    if FAILSCORE_ZERO_ERROR not in stderr:
        failures.append(f"stderr lacks {FAILSCORE_ZERO_ERROR!r}")
    if data_rows(work / "loss.tadah"):
        failures.append("loss.tadah has rows")
    if (work / "best_pot.tadah").exists():
        failures.append("best_pot.tadah exists")
    return failures


def check_files(work, case, rc, stderr, journal):
    """Failures, as a list."""
    if case.get("kind") == "startup_error":
        return check_startup_error(work, rc, stderr)
    failures = []
    rows = data_rows(work / "loss.tadah")
    iters = [int(r[0]) for r in rows]
    if iters != list(range(1, case["evals"] + 1)):
        failures.append(f"loss.tadah rows are {iters}, expected 1 .. "
                        f"{case['evals']}")
    failed = [r for r in rows if loss_of(r[-1]) >= FAIL_BAND_FLOOR]
    status = journal_value(journal, "Optimiser status")
    unusable = journal_value(journal, "Unusable losses")
    unusable_n = int(unusable.split()[0]) if unusable else None
    if unusable_n != len(failed):
        failures.append(f"journal 'Unusable losses' is {unusable!r}; "
                        f"loss.tadah has {len(failed)} failed rows")
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
        if case.get("kind") == "fd_inside":
            xs = [float(r[-1]) for r in data_rows(work / "params.tadah")]
            if [round(x, 12) for x in xs] != [1.25]:
                failures.append(f"the evaluations are {xs}, expected the "
                                "start point 1.25 alone")
            if journal.count(FD_INSIDE_STOP) != 1:
                failures.append(f"the journal does not say {FD_INSIDE_STOP!r} "
                                "once")
        return failures

    if rc != 0:
        failures.append(f"exit status {rc}, expected 0")
    if NO_USABLE_ERROR in stderr:
        failures.append(f"stderr says {NO_USABLE_ERROR!r}")
    if case.get("kind") == "fd_side":
        if status != FD_SIDE_STATUS:
            failures.append(f"journal status {status!r}, expected "
                            f"{FD_SIDE_STATUS!r}")
        if "stopped: no usable gradient" in journal:
            failures.append("the journal says the search stopped without a "
                            "usable gradient")
        if journal.count(FD_SIDE_NOTE) != 1:
            failures.append(f"the journal does not say {FD_SIDE_NOTE!r} once")
        if not re.search(r"Mixed-kind stencils\s*: 1 ", journal):
            failures.append("DIAGNOSTICS do not count one mixed-kind stencil")
        xs = [float(r[-1]) for r in data_rows(work / "params.tadah")]
        if [round(x, 12) for x in xs[:4]] != [1.6, 1.8, 1.4, 2.0]:
            failures.append(f"the first evaluations are {xs[:4]}, expected "
                            "1.6, its stencil 1.8 and 1.4, and 2.0")
        if [r[-1] == FAILSCORE_ROW for r in rows] != \
                [i == 2 for i in range(len(rows))]:
            failures.append("only the stencil point 1.4 should fail: "
                            f"{[r[-1] for r in rows]}")
    elif case.get("kind") == "ceres_box":
        if status != CERES_BOX_STATUS:
            failures.append(f"journal status {status!r}, expected "
                            f"{CERES_BOX_STATUS!r}")
        if journal.count(CERES_BOX_WARNING) != 1:
            failures.append(f"the journal does not say {CERES_BOX_WARNING!r} "
                            "once")
        if not re.search(r"Out-of-box points\s*: 1 ", journal):
            failures.append("DIAGNOSTICS do not count one out-of-box point")
        xs = [float(r[-1]) for r in data_rows(work / "params.tadah")]
        outside = [i for i, x in enumerate(xs) if x < 1.0 or x > 2.0]
        if outside != [3] or rows[3][-1] != CERES_BOX_SCORE_ROW:
            failures.append(f"the points outside the box are rows "
                            f"{[i + 1 for i in outside]} "
                            f"({[xs[i] for i in outside]}), expected row 4, "
                            f"0.9, scored {CERES_BOX_SCORE_ROW}: "
                            f"{[rows[i][-1] for i in outside]}")
        for i in outside:
            v = loss_of(rows[i][-1])
            if not (0.99975 * FAILSCORE <= v < FAILSCORE):
                failures.append(f"row {i + 1} outside the box has the loss "
                                f"{rows[i][-1]}, not an out-of-box score")
    else:
        if status != GFS_MAXEVAL_STATUS:
            failures.append(f"journal status {status!r}, expected "
                            f"{GFS_MAXEVAL_STATUS!r}")
        if any(r[-1] != FAILSCORE_ROW for r in rows[:3]):
            failures.append("the three starting points did not all fail: "
                            f"{[r[-1] for r in rows[:3]]}")
    usable = {int(r[0]): loss_of(r[-1]) for r in rows
              if loss_of(r[-1]) < FAIL_BAND_FLOOR}
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
