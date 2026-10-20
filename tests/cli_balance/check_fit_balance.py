#!/usr/bin/env python3
"""The balance table of a fit, as `tadah train` and `tadah hpo` print it and
keep it in the training record of the potential they write.

    check_fit_balance.py --tadah <tadah> --case train|hpo|hpo_killed

Each case runs `tadah` on a tiny self-contained problem (Ta cells of four
atoms in two datasets, energies and forces, D2_BP, no LAMMPS) in a fresh
temporary directory and checks:

  train       `tadah train` prints one table -- a "Fit balance: LAMBDA ..."
              line, the column header, a row per quantity of every dataset
              and of each dataset, the prior's row -- and then the legend,
              one line per column; the training record of pot.tadah ends
              with the same lines (its last lines, before the end of the
              record). At -v 0 nothing is printed and the record holds the
              table all the same. A task file of two TASK train sections
              prints two tables and the legend once.
  hpo         `tadah hpo` prints the table once, at the end of the run, after
              "[HPO] best_pot.tadah:"; it is the table of the training record
              of best_pot.tadah, and that of a retrain of best_pot.tadah on
              the same data (the table of the evaluation's own fit); the
              BDUMP archives of the new bests have none.
  hpo_killed  `tadah hpo` killed by SIGKILL once its first best potential is
              on disk: best_pot.tadah has the table of its fit in its
              training record (that of a retrain of it).

Every file lives in a fresh temporary directory, removed afterwards unless
--keep is given. A case that runs `tadah hpo` needs a build with
TADAH_ENABLE_NFP: with --nfp OFF it exits 77, which CTest reports as
Skipped.
"""
import argparse
import math
import os
import pathlib
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import time

SKIP = 77

CONFIG = """DBFILE train1.db train2.db
INIT2B true
INITMB false
TYPE2B D2_BP 2 2 Ta Ta
CGRID2B 0.5 1.4
SGRID2B 3.0 5.0
RCTYPE2B Cut_Cos
RCUT2B 5.0
MODEL M_BLR BF_Linear
LAMBDA 1e-3
OALGO 3
BIAS false
FORCE true
ESIGMA 2
FSIGMA 0.2
"""

CONFIG_HPO = """OPTIMIZER
  STRATEGY
    TYPE PLAIN
    LIB TADAH
    ALGO RANDOM
    MAXEVAL {maxeval}
    SEED 3
  ENDSTRATEGY
ENDOPTIMIZER
LOSS L2
QFLUSH 1
PC_ERMSE 0 1
OPTIM CGRID2B (1) 0.1 0.9
OPTIM SGRID2B (2) 4.0 6.0
BDUMP 1 bests
"""

COLUMNS = ["quantity", "dataset", "n", "sigma", "RMSE", "unit", "r/sigma",
           "share", "gamma"]


def structure(k, shift):
    """A cell of four Ta atoms, each structure displaced its own way, with
    an energy and forces that are not those of any pair model."""
    a = 3.301 + 0.02 * k
    pos = [(0.0, 0.0, 0.0), (1.6505, 1.6505, 0.0),
           (0.0, 0.0, 3.301), (1.6505, 1.6505, 4.9515)]
    lines = [f"cell {k}", "1.0 1.0 1.0",
             f"{-8.1 * 4 + 0.05 * math.sin(1.3 * k + shift)!r} 0.0",
             f"{a!r} 0.0 0.0", f"0.0 {a!r} 0.0", f"0.0 0.0 {2 * a!r}",
             "0.0 0.0 0.0", "0.0 0.0 0.0", "0.0 0.0 0.0"]
    for i, (x, y, z) in enumerate(pos):
        dx = 0.07 * math.sin(k + i + shift)
        dy = 0.05 * math.cos(2 * k + i)
        dz = 0.06 * math.sin(3 * k - i)
        f = [0.3 * math.sin(k * 0.7 + i), -0.2 * math.cos(k + 0.5 * i),
             0.1 * math.sin(2.0 * k * i + shift)]
        lines.append(f"Ta {x + dx!r} {y + dy!r} {z + dz!r} "
                     f"{f[0]!r} {f[1]!r} {f[2]!r}")
    return "\n".join(lines) + "\n\n"


def write_inputs(work):
    for name, shift, count in (("train1.db", 0.0, 4), ("train2.db", 0.9, 3)):
        (work / name).write_text("".join(structure(k, shift)
                                         for k in range(count)))
    (work / "val.db").write_text("".join(structure(k, 2.1) for k in range(2)))
    (work / "config.train").write_text(CONFIG)
    (work / "config.val").write_text("DBFILE val.db\n")


def run(tadah, work, *args, check=True):
    proc = subprocess.run([tadah, *args], cwd=work, stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT, text=True, timeout=600,
                          env=dict(os.environ, OMP_NUM_THREADS="1"))
    if check and proc.returncode != 0:
        raise AssertionError(f"`tadah {' '.join(args)}` exited "
                             f"{proc.returncode}:\n{proc.stdout}")
    return proc.stdout


def record_lines(path):
    """The lines of the training record of the potential @p path, without
    their leading '# '."""
    out, inside = [], False
    for line in pathlib.Path(path).read_text().splitlines():
        if line == "# --- training record (no key is read from it) ---":
            inside = True
        elif line == "# --- end of training record ---":
            inside = False
        elif inside:
            out.append(line[2:] if line.startswith("# ") else line[1:])
    return out


def record_table(path):
    """The balance table of the training record of @p path: its lines from
    "Fit balance:" on."""
    rec = record_lines(path)
    for i, line in enumerate(rec):
        if line.startswith("Fit balance:"):
            return rec[i:]
    return []


def printed_tables(output):
    """Each table the output prints: from a "Fit balance:" line to its
    prior's row."""
    lines = output.splitlines()
    tables, i = [], 0
    while i < len(lines):
        if lines[i].startswith("Fit balance:"):
            t = [lines[i]]
            i += 1
            while i < len(lines) and lines[i].startswith("  "):
                t.append(lines[i])
                i += 1
                if t[-1].startswith("  prior"):
                    break
            tables.append(t)
        else:
            i += 1
    return tables


def legend_count(output):
    return sum(1 for line in output.splitlines()
               if line.startswith("  gamma ") and "effective" in line)


def table_failures(table, quantities, datasets):
    """What is wrong with the lines of @p table of a fit of @p quantities in
    @p datasets datasets."""
    f = []
    if not table or not re.match(r"Fit balance: LAMBDA \S+; \d+ coefficients"
                                 r"( \(FIXINDEX pins \d+ more\))?, rank \d+$",
                                 table[0]):
        return [f"no table header: {table[:1]}"]
    if len(table) < 2 or table[1].split() != COLUMNS:
        f.append(f"column header: {table[1:2]}")
    rows = [line.split() for line in table[2:]]
    want = []
    for q in quantities:
        want.append((q, "all"))
        if datasets > 1:
            want += [(q, str(d)) for d in range(1, datasets + 1)]
    got = [(r[0], r[1]) for r in rows[:-1]]
    if got != want:
        f.append(f"rows {got}, expected {want}")
    if not rows or rows[-1][0] != "prior" or len(rows[-1]) != 3:
        f.append(f"no prior row: {table[-1:]}")
        return f
    # gamma: every quantity's row of every dataset plus the prior's is the
    # rank; share: they sum to 1 (to the printed digits).
    rank = int(table[0].rsplit(" ", 1)[1])
    gamma = sum(float(r[-1]) for r in rows[:-1] if r[1] == "all")
    gamma += float(rows[-1][2])
    share = sum(float(r[-2]) for r in rows[:-1] if r[1] == "all")
    share += float(rows[-1][1])
    if abs(gamma - rank) > 2e-3 * max(rank, 1):
        f.append(f"gamma sums to {gamma}, the rank is {rank}")
    if abs(share - 1.0) > 2e-3:
        f.append(f"the shares sum to {share}")
    return f


def case_train(tadah, work):
    failures = []
    out = run(tadah, work, "train", "-c", "config.train")
    tables = printed_tables(out)
    if len(tables) != 1:
        return [f"tadah train printed {len(tables)} tables:\n{out}"]
    failures += table_failures(tables[0], ["energy", "force"], 2)
    rec = record_table(work / "pot.tadah")
    if rec != tables[0]:
        failures.append(f"the record's table {rec} is not the printed one "
                        f"{tables[0]}")
    if record_lines(work / "pot.tadah")[-len(rec):] != rec:
        failures.append("the table is not the end of the training record")
    if legend_count(out) != 1:
        failures.append(f"the legend is printed {legend_count(out)} times")
    elif out.splitlines()[-1] != "  gamma     effective parameters; with "\
                                 "the prior's, the rank":
        failures.append(f"the legend does not follow the table:\n{out}")

    quiet = run(tadah, work, "train", "-c", "config.train", "-v", "0",
                "-o", "quiet.tadah")
    if quiet.strip():
        failures.append(f"tadah train -v 0 printed:\n{quiet}")
    if record_table(work / "quiet.tadah") != rec:
        failures.append("tadah train -v 0 wrote another table into the "
                        "record")

    (work / "two.task").write_text(
        "TASK train\nCONFIG config.train\nOUTFILE a.tadah\n"
        "TASK train\nCONFIG config.train\nOUTFILE b.tadah\n")
    two = run(tadah, work, "train", "--task", "two.task")
    if len(printed_tables(two)) != 2 or legend_count(two) != 1:
        failures.append(f"two TASK train sections printed "
                        f"{len(printed_tables(two))} tables and "
                        f"{legend_count(two)} legends:\n{two}")
    return failures


def retrain_table(tadah, work, pot):
    (work / "retrain.train").write_text(
        f"CONFIG {pot}\nDBFILE train1.db train2.db\n")
    run(tadah, work, "train", "-c", "retrain.train", "-o", "retrained.tadah",
        "-v", "0")
    return record_table(work / "retrained.tadah")


def case_hpo(tadah, work):
    failures = []
    (work / "config.hpo").write_text(CONFIG_HPO.format(maxeval=12))
    out = run(tadah, work, "hpo", "-c", "config.train", "--hpotarget",
              "config.hpo", "--validation", "config.val")
    tables = printed_tables(out)
    if len(tables) != 1:
        return [f"tadah hpo printed {len(tables)} tables:\n{out}"]
    lines = out.splitlines()
    at = lines.index(tables[0][0])
    if at == 0 or lines[at - 1] != "[HPO] best_pot.tadah:":
        failures.append(f"the table does not follow '[HPO] best_pot.tadah:'"
                        f":\n{out}")
    if any(line.startswith("[HPO]") for line in lines[at:]):
        failures.append(f"the table is not printed at the end:\n{out}")
    failures += table_failures(tables[0], ["energy", "force"], 2)
    rec = record_table(work / "best_pot.tadah")
    if rec != tables[0]:
        failures.append(f"best_pot.tadah's table {rec} is not the printed "
                        f"one {tables[0]}")
    again = retrain_table(tadah, work, "best_pot.tadah")
    if again != rec:
        failures.append(f"a retrain of best_pot.tadah has the table "
                        f"{again}, best_pot.tadah {rec}")
    archives = sorted((work / "bests").glob("*.tadah"))
    if not archives:
        failures.append("no BDUMP archive was written")
    for a in archives:
        if record_table(a):
            failures.append(f"the BDUMP archive {a.name} has a table")
    return failures


def case_hpo_killed(tadah, work):
    (work / "config.hpo").write_text(CONFIG_HPO.format(maxeval=100000000))
    out = open(work / "run.out", "w")
    proc = subprocess.Popen(
        [tadah, "hpo", "-c", "config.train", "--hpotarget", "config.hpo",
         "--validation", "config.val"],
        cwd=work, stdout=out, stderr=subprocess.STDOUT,
        env=dict(os.environ, OMP_NUM_THREADS="1"))
    best = work / "best_pot.tadah"
    deadline = time.monotonic() + 120
    while proc.poll() is None and time.monotonic() < deadline:
        if best.exists() and "# HPO_BEST_LOSS" in best.read_text():
            break
        time.sleep(0.002)
    if proc.poll() is not None:
        out.close()
        return [f"tadah hpo exited with {proc.returncode} before it was "
                f"killed:\n{(work / 'run.out').read_text()}"]
    proc.send_signal(signal.SIGKILL)
    rc = proc.wait(timeout=60)
    out.close()
    failures = []
    if rc != -signal.SIGKILL:
        failures.append(f"tadah hpo was not killed by SIGKILL (status {rc})")
    if not best.exists():
        return failures + ["no best_pot.tadah after the kill"]
    rec = record_table(best)
    failures += table_failures(rec, ["energy", "force"], 2)
    again = retrain_table(tadah, work, "best_pot.tadah")
    if again != rec:
        failures.append(f"a retrain of the published best_pot.tadah has the "
                        f"table {again}, best_pot.tadah {rec}")
    return failures


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--tadah", required=True)
    ap.add_argument("--case", required=True,
                    choices=("train", "hpo", "hpo_killed"))
    ap.add_argument("--nfp", default="ON", choices=("ON", "OFF"))
    ap.add_argument("--keep", action="store_true")
    opts = ap.parse_args()
    if opts.case != "train" and opts.nfp == "OFF":
        print(f"SKIP: the case {opts.case} runs tadah hpo, which needs a "
              "build with TADAH_ENABLE_NFP=ON")
        return SKIP
    tadah = os.path.abspath(opts.tadah)
    work = pathlib.Path(tempfile.mkdtemp(prefix="tadah_fit_balance_"))
    try:
        write_inputs(work)
        case = {"train": case_train, "hpo": case_hpo,
                "hpo_killed": case_hpo_killed}[opts.case]
        failures = case(tadah, work)
    except AssertionError as e:
        failures = [str(e)]
    finally:
        if opts.keep:
            print(f"kept {work}")
        else:
            shutil.rmtree(work, ignore_errors=True)
    for f in failures:
        print(f"FAIL: {f}")
    if failures:
        return 1
    print(f"PASS: {opts.case}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
