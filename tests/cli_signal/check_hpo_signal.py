#!/usr/bin/env python3
"""`tadah hpo` stopped by a signal exits with 128 + the signal number.

Starts `tadah hpo` on a tiny self-contained problem (four Ta atoms, D2_BP,
no LAMMPS, TADAH:RANDOM with a budget it never reaches), waits until
evaluations are being written, sends the signal and checks:

  * the process *exited* with status 128 + signo (130 for SIGINT, 143 for
    SIGTERM, 140 for SIGUSR2 on Linux). A negative return code means the
    process was killed by the signal instead: the handler was not installed
    and nothing was flushed;
  * the journal trailer reads "SHUTDOWN (<SIGNAME>)";
  * loss.tadah has one row per evaluation the journal counts, numbered
    1, 2, 3, ... without a gap;
  * best_pot.tadah holds the incumbent: the evaluation its header names
    (HPO_SOURCE_EVAL, HPO_BEST_LOSS) is the lowest loss in loss.tadah.

Every file lives in a fresh temporary directory, removed afterwards unless
--keep is given. No network, no LAMMPS.
"""
import argparse
import os
import pathlib
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import time

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
TYPE2B D2_BP 2 2 Ta Ta
CGRID2B 0.5 1.4
SGRID2B 3.0 5.0
RCTYPE2B Cut_Cos
RCUT2B 5.0
MODEL M_BLR BF_Linear
LAMBDA 1e-3
OALGO 4
BIAS false
"""

CONFIG_HPO = """OPTIMIZER
  STRATEGY
    TYPE PLAIN
    LIB TADAH
    ALGO RANDOM
    MAXEVAL 100000000
    SEED 3
  ENDSTRATEGY
ENDOPTIMIZER
LOSS L2
QFLUSH 1
DIGITS 15
PC_ERMSE 0 1
OPTIM CGRID2B (1) 0.1 0.9
OPTIM CGRID2B (2) 1.0 2.0
OPTIM SGRID2B (2) 4.0 6.0
"""


def write_inputs(work):
    for name, offset, energy in (("train.db", 0.0, -0.001),
                                 ("val.db", 0.07, -0.04)):
        (work / name).write_text(STRUCTURE.format(
            energy=energy, x0=repr(0.0 + offset), x1=repr(1.6505 + offset)))
    (work / "config.val").write_text("DBFILE val.db\n")
    (work / "config.tadah").write_text(CONFIG_TADAH)
    (work / "config.hpo").write_text(CONFIG_HPO)


def data_rows(path):
    if not path.exists():
        return []
    rows = []
    for line in path.read_text().splitlines():
        s = line.strip()
        if s and not s.startswith("#"):
            rows.append(s.split())
    return rows


# loss.tadah prints 16 significant digits (DIGITS 15), one short of a
# double's exact round trip: losses closer than this cannot be ordered from
# the file, so every row within it of the lowest one may be the incumbent.
PRINT_RESOLUTION = 2e-15


def check_incumbent(work):
    """Failures of the best_pot.tadah incumbent check, as a list."""
    header = {}
    for line in (work / "best_pot.tadah").read_text().splitlines():
        m = re.match(r"#\s*(HPO_BEST_LOSS|HPO_SOURCE_EVAL)\s+(\S+)", line)
        if m:
            header[m.group(1)] = m.group(2)
    if set(header) != {"HPO_BEST_LOSS", "HPO_SOURCE_EVAL"}:
        return [f"best_pot.tadah header lacks the incumbent key: {header}"]
    best_eval = int(header["HPO_SOURCE_EVAL"])
    best_loss = float(header["HPO_BEST_LOSS"])
    losses = {int(r[0]): float(r[-1])
              for r in data_rows(work / "loss.tadah")}
    lowest = min(losses.values())
    candidates = {i for i, v in losses.items()
                  if abs(v - lowest) <= PRINT_RESOLUTION * abs(lowest)}
    if best_eval not in candidates:
        return [f"best_pot.tadah holds eval {best_eval} (loss {best_loss!r}) "
                f"but the lowest loss.tadah row(s) are {sorted(candidates)} "
                f"(loss {lowest!r})"]
    if abs(best_loss - losses[best_eval]) > \
            PRINT_RESOLUTION * abs(losses[best_eval]):
        return [f"best_pot.tadah loss {best_loss!r} differs from loss.tadah "
                f"row {best_eval} ({losses[best_eval]!r})"]
    return []


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--tadah", required=True, help="path to the tadah binary")
    ap.add_argument("--signal", default="SIGINT",
                    choices=("SIGINT", "SIGTERM", "SIGUSR2"))
    ap.add_argument("--keep", action="store_true",
                    help="keep the working directory")
    opts = ap.parse_args()

    tadah = os.path.abspath(opts.tadah)
    signo = int(getattr(signal, opts.signal))
    expected = 128 + signo
    work = pathlib.Path(tempfile.mkdtemp(prefix="tadah_hpo_signal_"))
    failures = []
    try:
        write_inputs(work)
        out = open(work / "run.out", "w")
        proc = subprocess.Popen(
            [tadah, "hpo", "--hpotarget", "config.hpo",
             "--validation", "config.val", "-c", "config.tadah"],
            cwd=work, stdout=out, stderr=subprocess.STDOUT,
            env=dict(os.environ, OMP_NUM_THREADS="1"))

        # Wait until three evaluations are on disk: the signal handlers are
        # installed before the first evaluation.
        deadline = time.monotonic() + 120
        while proc.poll() is None and time.monotonic() < deadline:
            if len(data_rows(work / "loss.tadah")) >= 3:
                break
            time.sleep(0.005)
        if proc.poll() is not None:
            out.close()
            print((work / "run.out").read_text())
            print(f"FAIL: tadah exited with {proc.returncode} before it was "
                  f"signalled")
            return 1
        if len(data_rows(work / "loss.tadah")) < 3:
            proc.kill()
            proc.wait()
            print("FAIL: no evaluations within 120 s")
            return 1

        proc.send_signal(signo)
        try:
            rc = proc.wait(timeout=120)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait()
            print(f"FAIL: tadah did not stop within 120 s of {opts.signal}")
            return 1
        out.close()

        print(f"tadah hpo after {opts.signal}: exit status {rc} "
              f"(expected {expected})")
        if rc < 0:
            failures.append(f"killed by signal {-rc} instead of exiting: "
                            f"the signal was not handled")
        elif rc != expected:
            failures.append(f"exit status {rc}, expected {expected}")

        journal = (work / "hpo_run.log").read_text() \
            if (work / "hpo_run.log").exists() else ""
        m = re.search(r"Optimiser status\s*: ([^\n]*)", journal)
        status = m.group(1) if m else "(no RESULT section)"
        print(f"journal status: {status}")
        if status != f"SHUTDOWN ({opts.signal})":
            failures.append(f"journal status {status!r}, expected "
                            f"'SHUTDOWN ({opts.signal})'")

        iters = [int(r[0]) for r in data_rows(work / "loss.tadah")]
        if iters != list(range(1, len(iters) + 1)):
            failures.append(f"loss.tadah rows are not 1, 2, 3, ...: {iters}")
        m = re.search(r"Total evaluations\s*:\s*(\d+)", journal)
        if not m:
            failures.append("the journal has no 'Total evaluations'")
        elif int(m.group(1)) != len(iters):
            failures.append(f"loss.tadah has {len(iters)} rows for "
                            f"{m.group(1)} evaluations")

        if not (work / "best_pot.tadah").exists():
            failures.append("best_pot.tadah is missing")
        elif iters:
            failures.extend(check_incumbent(work))

        if failures:
            print((work / "run.out").read_text()[-4000:])
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
