#!/usr/bin/env python3
"""A second SIGINT or SIGTERM ends `tadah hpo` at once.

The first signal asks `tadah hpo` to stop once the evaluations in progress
finish, and writes its results; an evaluation inside a long LAMMPS run does
not finish soon, and a second signal of the same kind ends the process at
once. Starts `tadah hpo` on a tiny problem (four Ta atoms, D2_BP) whose
PC_LAMMPS script runs MD for longer than any test waits, under the
PC_ISOLATION mode given, waits until the script's long `run` has started,
then checks that:

  * after the first signal the process announces the stop on standard
    error at once ("tadah: SIGINT received: stopping once the work in
    progress can stop and save its results; a second SIGINT (Ctrl-C) ends
    tadah at once, ...") and keeps running the evaluation;
  * after the second signal it ends within 20 s, killed by that signal (a
    negative return code: the shell's status 128 + signo), having said
    "tadah: second SIGINT: ending at once";
  * under PC_ISOLATION PROCESS, its worker processes are gone too (they
    ignore the stop signals and die with their parent).

Every file lives in a fresh temporary directory, removed afterwards unless
--keep is given. No network.
"""
import argparse
import os
import pathlib
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

# The script writes started.txt and then runs MD for far longer than the
# test waits: the evaluation is in progress whenever a signal arrives.
LONG_SCRIPT = """clear
units metal
boundary p p p
atom_style atomic
lattice bcc 3.301
region box block 0 2 0 2 0 2 units lattice
create_box 1 box
create_atoms 1 region box
mass 1 180.95
pair_style tadah
pair_coeff * * EXTERN Ta
velocity all create 300.0 4928459
fix 1 all nve
thermo_style custom step pe
print "the long run starts" file started.txt
run 1000000000
variable E equal pe
"""

CONFIG_HPO = """OPTIMIZER
  STRATEGY
    TYPE PLAIN
    LIB TADAH
    ALGO RANDOM
    MAXEVAL 100
    SEED 3
  ENDSTRATEGY
ENDOPTIMIZER
LOSS L2
QFLUSH 1
PC_ISOLATION {isolation}
PC_ERMSE 0 1
PC_LAMMPS --script long.in --varloss E 0 1
OPTIM CGRID2B (1) 0.1 0.9
"""


def write_inputs(work, isolation):
    for name, offset, energy in (("train.db", 0.0, -0.001),
                                 ("val.db", 0.07, -0.04)):
        (work / name).write_text(STRUCTURE.format(
            energy=energy, x0=repr(0.0 + offset), x1=repr(1.6505 + offset)))
    (work / "config.val").write_text("DBFILE val.db\n")
    (work / "config.tadah").write_text(CONFIG_TADAH)
    (work / "long.in").write_text(LONG_SCRIPT)
    (work / "config.hpo").write_text(CONFIG_HPO.format(isolation=isolation))


def children_of(pid):
    """The pids of the direct children of `pid` (every thread's)."""
    kids = set()
    task_dir = pathlib.Path(f"/proc/{pid}/task")
    try:
        for task in task_dir.iterdir():
            try:
                text = (task / "children").read_text()
            except OSError:
                continue
            kids.update(int(k) for k in text.split())
    except OSError:
        pass
    return kids


def alive(pid):
    """Whether `pid` is a live process (not gone, not a zombie)."""
    try:
        stat = pathlib.Path(f"/proc/{pid}/stat").read_text()
    except OSError:
        return False
    return stat[stat.rfind(")") + 2:].split()[0] != "Z"


def wait_for(predicate, seconds):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        if predicate():
            return True
        time.sleep(0.02)
    return predicate()


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--tadah", required=True, help="path to the tadah binary")
    ap.add_argument("--signal", default="SIGINT", choices=("SIGINT", "SIGTERM"))
    ap.add_argument("--isolation", default="THREAD",
                    choices=("THREAD", "PROCESS"))
    ap.add_argument("--keep", action="store_true",
                    help="keep the working directory")
    opts = ap.parse_args()

    tadah = os.path.abspath(opts.tadah)
    signo = int(getattr(signal, opts.signal))
    name = opts.signal
    ctrl_c = " (Ctrl-C)" if name == "SIGINT" else ""
    announced = (f"tadah: {name} received: stopping once the work in progress "
                 f"can stop and save its results; a second {name}{ctrl_c} "
                 f"ends tadah at once, and what is not yet written is lost")
    second = f"tadah: second {name}: ending at once"
    work = pathlib.Path(tempfile.mkdtemp(prefix="tadah_hpo_second_signal_"))
    failures = []
    proc = None
    try:
        write_inputs(work, opts.isolation)
        out_path = work / "run.out"
        out = open(out_path, "w")
        # TADAH_PC_ISOLATION would override the configuration's mode.
        env = {k: v for k, v in os.environ.items() if k != "TADAH_PC_ISOLATION"}
        env["OMP_NUM_THREADS"] = "1"
        proc = subprocess.Popen(
            [tadah, "hpo", "--hpotarget", "config.hpo",
             "--validation", "config.val", "-c", "config.tadah"],
            cwd=work, stdout=out, stderr=subprocess.STDOUT, env=env)

        def output():
            return out_path.read_text(errors="replace")

        if not wait_for(lambda: proc.poll() is not None or
                        (work / "started.txt").exists(), 120):
            failures.append("the PC_LAMMPS script's long run did not start "
                            "within 120 s")
        if proc.poll() is not None:
            out.close()
            print(output()[-4000:])
            print(f"FAIL: tadah exited with {proc.returncode} before it was "
                  f"signalled")
            return 1
        if failures:
            raise RuntimeError(failures[-1])
        time.sleep(0.5)   # inside the run
        workers = children_of(proc.pid) if opts.isolation == "PROCESS" else set()
        if opts.isolation == "PROCESS" and not workers:
            failures.append("no PC_ISOLATION PROCESS worker found under tadah")

        proc.send_signal(signo)
        if not wait_for(lambda: announced in output(), 10):
            failures.append(f"no announcement of the first {name} within 10 s "
                            f"(expected {announced!r})")
        time.sleep(2.0)
        if proc.poll() is not None:
            failures.append(f"tadah ended ({proc.returncode}) after the first "
                            f"{name}, inside the long evaluation: the stop it "
                            f"asks for waits for the evaluation")

        t_second = time.monotonic()
        proc.send_signal(signo)
        try:
            rc = proc.wait(timeout=20)
        except subprocess.TimeoutExpired:
            proc.kill()
            rc = proc.wait()
            failures.append(f"tadah did not end within 20 s of the second "
                            f"{name} (killed with SIGKILL)")
        else:
            took = time.monotonic() - t_second
            print(f"tadah hpo after the second {name}: return code {rc} "
                  f"after {took:.2f} s")
            if rc != -signo:
                failures.append(f"return code {rc}, expected {-signo} (killed "
                                f"by {name})")
        out.close()
        text = output()
        if second not in text:
            failures.append(f"no {second!r} line")
        if text.count(announced) > 1:
            failures.append("the first signal was announced more than once")

        if workers:
            if not wait_for(lambda: not any(alive(p) for p in workers), 10):
                left = sorted(p for p in workers if alive(p))
                failures.append(f"PC_ISOLATION PROCESS worker(s) {left} still "
                                f"alive 10 s after tadah ended")
                for p in left:
                    try:
                        os.kill(p, signal.SIGKILL)
                    except OSError:
                        pass

        if failures:
            print(text[-4000:])
            for f in failures:
                print("FAIL: " + f)
            return 1
        print("PASS")
        return 0
    except RuntimeError as e:
        if proc is not None and proc.poll() is None:
            proc.kill()
            proc.wait()
        print("FAIL: " + str(e))
        return 1
    finally:
        if proc is not None and proc.poll() is None:
            proc.kill()
            proc.wait()
        if opts.keep:
            print(f"kept {work}")
        else:
            shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
