#!/usr/bin/env python3
"""Runs the commands of a Nested Fitting page and checks their results.

    check_nfp_run.py --manifest <docs/source/nfp/files/manifest.txt> --id <id>
                     --tadah <tadah> --workdir <scratch> --nfp ON|OFF
    check_nfp_run.py --list <manifest>

The manifest has one line per run of the documentation's files:

    <id> | <page.rst> | <dir> | <labels> | <timeout> | <tolerances>

  id          the CTest entry is tadah_nfp_<id> (letters, digits, '_');
  page.rst    the page whose ``bash`` code blocks are run, relative to
              docs/source (the manifest's directory is docs/source/nfp/files);
  dir         the directory they run in, relative to the manifest's
              directory;
  labels      CTest labels besides ``examples``, separated by ',', or '-'
              for none (``slow``: a run longer than about two minutes);
  timeout     the time the commands may take together, in seconds;
  tolerances  ``<quantity>=<tolerance>`` separated by ',', or '-' for none:
              a relative tolerance, or an absolute one written ``abs:<x>``
              (``best_loss=1e-6``, ``best_loss=abs:1e-9``).

Blank lines and lines starting with '#' are comments. --list checks the
manifest and prints ``<id>|<labels>|<timeout>`` per run (labels with
``examples`` first, separated by ','); CMake reads it to add the entries.

Every page under docs/source/nfp that runs ``tadah`` in a ``bash`` code
block needs a line, and so does every directory under docs/source/nfp/files
that holds a target file (*.hpo) or a training configuration (*.train).
coverage() lists what has none, and the lines whose page or directory does
not exist; the unit tests of the documentation tools (tadah_docs_tools)
fail on the shipped manifest when it lists anything.

A run copies the manifest's directory to <workdir>/files (the source tree is
never written; a run can read files of a sibling directory, such as
../data), then runs every command of the page's ``bash`` code blocks in
<workdir>/files/<dir>, in order (tests/doc_commands/doc_commands.py reads
them), with <tadah> in place of ``tadah``. Every command must be a
``tadah`` command and exit 0, the commands within the timeout, and unless
it asks for help (-h, --help):

  * after ``tadah hpo``: best_pot.tadah, hpo_run.log and a best_loss.tadah
    with at least one row exist;
  * after ``tadah train``: the potential it writes exists (pot.tadah, or
    the file of -o/--outfile).

Then the quantities of <dir>/expected.csv (two columns, a header row
``Quantity,Value``, one row per quantity, the table the page shows) are
compared with what the run wrote, each within the manifest's tolerance:

  * ``Best loss`` (tolerance ``best_loss``): the loss of the best potential,
    the ``# HPO_BEST_LOSS`` line of best_pot.tadah.

expected.csv is required when the page runs ``tadah hpo`` and must then
hold the best loss. A quantity of expected.csv without a tolerance, a
tolerance without its quantity, and a quantity this script cannot measure
are errors: a value a page quotes is checked or not quoted.

A page that runs ``tadah hpo`` needs a build with TADAH_ENABLE_NFP: with
--nfp OFF nothing runs and the script exits 77, which CTest reports as
Skipped.
"""
import argparse
import csv
import math
import os
import re
import shlex
import shutil
import subprocess
import sys
import time
from collections import namedtuple

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                '..', 'doc_commands'))
from doc_commands import bash_commands  # noqa: E402

SKIP = 77
ID = re.compile(r'[A-Za-z0-9_]+')
LABEL = re.compile(r'[a-z0-9_]+')
# Quantity name in expected.csv -> its tolerance name in the manifest.
QUANTITIES = {'best loss': 'best_loss'}

Run = namedtuple('Run', 'id page dir labels timeout tolerances line')
Tolerance = namedtuple('Tolerance', 'value absolute')


class ManifestError(Exception):
    pass


class RunError(Exception):
    pass


def parse_tolerances(text, where):
    tolerances = {}
    if text == '-':
        return tolerances
    for item in text.split(','):
        name, eq, value = (s.strip() for s in item.partition('='))
        if not eq or not name or not value:
            raise ManifestError(f'{where}: tolerance "{item.strip()}" is not '
                                '<quantity>=<tolerance>')
        if name not in QUANTITIES.values():
            raise ManifestError(f'{where}: unknown quantity "{name}" (known: '
                                + ', '.join(sorted(QUANTITIES.values())) + ')')
        if name in tolerances:
            raise ManifestError(f'{where}: tolerance of "{name}" given twice')
        absolute = value.startswith('abs:')
        number = value[4:] if absolute else value
        try:
            x = float(number)
        except ValueError:
            x = math.nan
        if not math.isfinite(x) or x < 0:
            raise ManifestError(f'{where}: tolerance "{value}" of "{name}" is '
                                'not a number >= 0')
        tolerances[name] = Tolerance(x, absolute)
    return tolerances


def parse_manifest(path):
    '''The runs of the manifest at *path*, in order.'''
    runs, seen = [], {}
    with open(path, encoding='utf-8') as fh:
        for number, line in enumerate(fh, 1):
            text = line.strip()
            if not text or text.startswith('#'):
                continue
            where = f'{path}:{number}'
            fields = [f.strip() for f in text.split('|')]
            if len(fields) != 6:
                raise ManifestError(f'{where}: {len(fields)} fields, expected 6: '
                                    '<id> | <page.rst> | <dir> | <labels> | '
                                    '<timeout> | <tolerances>')
            rid, page, rdir, labels, timeout, tolerances = fields
            if not ID.fullmatch(rid):
                raise ManifestError(f'{where}: id "{rid}" is not letters, digits '
                                    'and _')
            if rid in seen:
                raise ManifestError(f'{where}: id "{rid}" is also on line '
                                    f'{seen[rid]}')
            seen[rid] = number
            if not page.endswith('.rst') or os.path.isabs(page):
                raise ManifestError(f'{where}: page "{page}" is not a relative '
                                    'path of an .rst file')
            if not rdir or os.path.isabs(rdir):
                raise ManifestError(f'{where}: directory "{rdir}" is not a '
                                    'relative path')
            label_list = [] if labels == '-' else [l.strip()
                                                   for l in labels.split(',')]
            for label in label_list:
                if not LABEL.fullmatch(label) or label == 'examples':
                    raise ManifestError(f'{where}: label "{label}" (labels are '
                                        'lower-case words besides examples)')
            if not re.fullmatch(r'[1-9][0-9]*', timeout):
                raise ManifestError(f'{where}: timeout "{timeout}" is not a '
                                    'whole number of seconds')
            runs.append(Run(rid, page, rdir, label_list, int(timeout),
                            parse_tolerances(tolerances, where), number))
    return runs


def _inside(path, directory):
    '''Whether *path* is *directory* or lies under it (both normalised).'''
    return os.path.commonpath([path, directory]) == directory


def coverage(manifest):
    '''What the manifest at *manifest* (docs/source/nfp/files/manifest.txt)
    leaves without a run, as a list of messages (empty when nothing):

      * a line whose page does not exist or is not under docs/source/nfp
        (outside docs/source/nfp/files), or whose directory does not exist
        or is not under docs/source/nfp/files;
      * a page under docs/source/nfp (outside files/) with a ``tadah``
        command in a ``bash`` code block that no line names;
      * a directory under docs/source/nfp/files that holds a target file
        (*.hpo) or a training configuration (*.train) and that no line
        runs in.

    A manifest that cannot be read is one message.'''
    files = os.path.dirname(os.path.abspath(manifest))
    nfp = os.path.dirname(files)
    docs_source = os.path.dirname(nfp)
    try:
        runs = parse_manifest(manifest)
    except ManifestError as err:
        return [str(err)]
    problems, pages, dirs = [], set(), set()
    for r in runs:
        where = f'{manifest}:{r.line}'
        page = os.path.normpath(os.path.join(docs_source, r.page))
        if not _inside(page, nfp) or _inside(page, files):
            problems.append(f'{where}: page {r.page} is not under {nfp} '
                            '(outside files/)')
        elif not os.path.isfile(page):
            problems.append(f'{where}: page {r.page} does not exist')
        else:
            pages.add(page)
        run_dir = os.path.normpath(os.path.join(files, r.dir))
        if not _inside(run_dir, files):
            problems.append(f'{where}: directory {r.dir} is not under {files}')
        elif not os.path.isdir(run_dir):
            problems.append(f'{where}: directory {r.dir} does not exist')
        else:
            dirs.add(run_dir)
    for top, subdirs, names in os.walk(nfp):
        subdirs.sort()
        if _inside(top, files):
            continue
        for name in sorted(names):
            page = os.path.join(top, name)
            if not name.endswith('.rst') or page in pages:
                continue
            tadah = [c for c in bash_commands(page) if c.argv[0] == 'tadah']
            if tadah:
                rel = os.path.relpath(page, docs_source)
                problems.append(f'{rel}:{tadah[0].line}: runs tadah, and no line '
                                f'of {manifest} runs the page')
    kinds = (('.hpo', 'a target file'), ('.train', 'a training configuration'))
    for top, subdirs, names in os.walk(files):
        subdirs.sort()
        if top in dirs:
            continue
        for suffix, kind in kinds:
            found = sorted(n for n in names if n.endswith(suffix))
            if found:
                rel = os.path.relpath(top, docs_source)
                problems.append(f'{rel} holds {kind} ({", ".join(found)}), and '
                                f'no line of {manifest} runs in it')
                break
    return problems


def read_expected(path):
    '''{quantity name as in QUANTITIES: (value, row text)} of expected.csv.'''
    with open(path, encoding='utf-8', newline='') as fh:
        rows = [r for r in csv.reader(fh) if r and any(c.strip() for c in r)]
    if not rows or [c.strip() for c in rows[0]] != ['Quantity', 'Value']:
        raise RunError(f'{path}: the header row is not "Quantity,Value"')
    expected = {}
    for row in rows[1:]:
        if len(row) != 2:
            raise RunError(f'{path}: row {row} has {len(row)} columns, not 2')
        label, value = row[0].strip(), row[1].strip()
        name = QUANTITIES.get(label.lower())
        if name is None:
            raise RunError(f'{path}: no check measures "{label}" (known: '
                           + ', '.join(sorted(QUANTITIES)) + ')')
        if name in expected:
            raise RunError(f'{path}: "{label}" is given twice')
        try:
            expected[name] = float(value)
        except ValueError:
            raise RunError(f'{path}: the value of "{label}" is not a number: '
                           f'{value}') from None
    return expected


def best_loss(run_dir):
    path = os.path.join(run_dir, 'best_pot.tadah')
    with open(path, encoding='utf-8') as fh:
        for line in fh:
            m = re.match(r'#\s*HPO_BEST_LOSS\s+(\S+)\s*$', line)
            if m:
                return float(m.group(1))
    raise RunError(f'{path} has no "# HPO_BEST_LOSS" line')


# How each quantity of QUANTITIES is read from the run directory.
MEASURE = {'best_loss': best_loss}


def outputs_missing(argv, run_dir):
    '''What a command that exited 0 should have written and did not.'''
    exists = lambda name: os.path.isfile(os.path.join(run_dir, name))
    if '-h' in argv or '--help' in argv:
        return []
    missing = []
    if argv[1:2] == ['hpo']:
        for name in ('best_pot.tadah', 'hpo_run.log'):
            if not exists(name):
                missing.append(f'{name} was not written')
        best = os.path.join(run_dir, 'best_loss.tadah')
        rows = []
        if os.path.isfile(best):
            with open(best, encoding='utf-8') as fh:
                rows = [l for l in fh if l.strip() and not l.lstrip().startswith('#')]
        if not rows:
            missing.append('best_loss.tadah has no row: no evaluation gave a '
                           'best (see hpo_run.log)')
    elif argv[1:2] == ['train']:
        out = 'pot.tadah'
        for i, a in enumerate(argv):
            if a in ('-o', '--outfile') and i + 1 < len(argv):
                out = argv[i + 1]
            elif a.startswith('--outfile='):
                out = a.split('=', 1)[1]
        if not exists(out):
            missing.append(f'{out} was not written')
    return missing


def run(args):
    runs = {r.id: r for r in parse_manifest(args.manifest)}
    if args.id not in runs:
        raise RunError(f'{args.manifest} has no run "{args.id}"')
    entry = runs[args.id]
    files = os.path.dirname(os.path.abspath(args.manifest))
    docs_source = os.path.normpath(os.path.join(files, '..', '..'))
    page = os.path.join(docs_source, entry.page)
    src_dir = os.path.normpath(os.path.join(files, entry.dir))
    if not os.path.isfile(page):
        raise RunError(f'{args.manifest}:{entry.line}: page {page} does not exist')
    if not os.path.isdir(src_dir):
        raise RunError(f'{args.manifest}:{entry.line}: directory {src_dir} does '
                       'not exist')

    commands = bash_commands(page)
    if not commands:
        raise RunError(f'{page} has no command in a bash code block')
    for c in commands:
        if c.argv[0] != 'tadah':
            raise RunError(f'{page}:{c.line}: "{" ".join(c.argv)}" is not a '
                           'tadah command (only tadah commands are run)')
    runs_hpo = any(c.argv[1:2] == ['hpo'] for c in commands)
    if args.nfp == 'OFF' and runs_hpo:
        print(f'SKIP: {entry.page} runs tadah hpo, which needs a build with '
              'TADAH_ENABLE_NFP=ON')
        return SKIP

    expected_csv = os.path.join(src_dir, 'expected.csv')
    expected = read_expected(expected_csv) if os.path.isfile(expected_csv) else {}
    if runs_hpo and 'best_loss' not in expected:
        raise RunError(f'{expected_csv} must hold the "Best loss" of the run '
                       '(the page runs tadah hpo)')
    for name in expected:
        if name not in entry.tolerances:
            raise RunError(f'{args.manifest}:{entry.line}: no tolerance for '
                           f'"{name}" of {expected_csv}')
    for name in entry.tolerances:
        if name not in expected:
            raise RunError(f'{args.manifest}:{entry.line}: tolerance of '
                           f'"{name}", which {expected_csv} does not give')

    workdir = os.path.abspath(args.workdir)
    shutil.rmtree(workdir, ignore_errors=True)
    shutil.copytree(files, os.path.join(workdir, 'files'))
    run_dir = os.path.join(workdir, 'files', entry.dir)
    deadline = time.monotonic() + entry.timeout
    for c in commands:
        shown = ' '.join(shlex.quote(a) for a in c.argv)
        print(f'$ {shown}   ({entry.page}:{c.line})', flush=True)
        left = deadline - time.monotonic()
        try:
            proc = subprocess.run([args.tadah] + c.argv[1:], cwd=run_dir,
                                  stdout=subprocess.PIPE,
                                  stderr=subprocess.STDOUT, text=True,
                                  timeout=max(left, 1))
        except subprocess.TimeoutExpired:
            raise RunError(f'`{shown}` did not end within the run\'s '
                           f'{entry.timeout} s') from None
        tail = '\n'.join(proc.stdout.splitlines()[-40:])
        if proc.returncode != 0:
            raise RunError(f'`{shown}` exited with {proc.returncode}:\n{tail}')
        missing = outputs_missing(c.argv, run_dir)
        if missing:
            raise RunError(f'`{shown}` exited with 0, but '
                           + '; '.join(missing) + f':\n{tail}')
        print('ok (exit 0)', flush=True)

    failed = []
    for name, want in sorted(expected.items()):
        got = MEASURE[name](run_dir)
        tol = entry.tolerances[name]
        limit = tol.value if tol.absolute else tol.value * abs(want)
        kind = 'absolute' if tol.absolute else 'relative'
        verdict = 'ok' if abs(got - want) <= limit else 'FAIL'
        print(f'{verdict}: {name} {got!r}, expected {want!r} within {kind} '
              f'{tol.value!r}')
        if verdict != 'ok':
            failed.append(name)
    if failed:
        raise RunError('outside the tolerance: ' + ', '.join(failed)
                       + f' (expected.csv of {entry.dir})')
    print(f'PASS: {len(commands)} command(s) of {entry.page} in {entry.dir}')
    return 0


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--list', metavar='MANIFEST',
                    help='check the manifest and print one line per run')
    ap.add_argument('--manifest')
    ap.add_argument('--id')
    ap.add_argument('--tadah')
    ap.add_argument('--workdir')
    ap.add_argument('--nfp', choices=('ON', 'OFF'))
    args = ap.parse_args(argv)
    try:
        if args.list:
            for r in parse_manifest(args.list):
                print(f'{r.id}|{",".join(["examples"] + r.labels)}|{r.timeout}')
            return 0
        needed = ('manifest', 'id', 'tadah', 'workdir', 'nfp')
        absent = [f'--{n}' for n in needed if getattr(args, n) is None]
        if absent:
            ap.error('a run needs ' + ', '.join(absent))
        return run(args)
    except ManifestError as err:
        print(f'manifest error: {err}', file=sys.stderr)
        return 2
    except RunError as err:
        print(f'FAIL: {err}')
        return 1


if __name__ == '__main__':
    sys.exit(main())
