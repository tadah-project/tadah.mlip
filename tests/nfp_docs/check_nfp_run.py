#!/usr/bin/env python3
"""Runs the commands of a Nested Fitting page and checks their results.

    check_nfp_run.py --manifest <docs/source/nfp/files/manifest.txt> --id <id>
                     --tadah <tadah> --workdir <scratch> --nfp ON|OFF
    check_nfp_run.py --list <manifest>

The manifest has one line per run of the documentation's files:

    <id> | <page.rst> | <dir> | <labels> | <timeout> | <tolerances>
         [ | <searches> ]

  id          the CTest entry is tadah_nfp_<id> (letters, digits, '_');
  page.rst    the page whose ``bash`` code blocks are run, relative to
              docs/source (the manifest's directory is docs/source/nfp/files);
  dir         the directory they run in, relative to the manifest's
              directory;
  labels      CTest labels besides ``examples``, separated by ',', or '-'
              for none (``slow``: a run longer than about two minutes);
  timeout     the time the commands may take together, in seconds;
  tolerances  ``<name>=<tolerance>`` separated by ',', or '-' for none: a
              relative tolerance, or an absolute one written ``abs:<x>``
              (``best_loss=1e-6``, ``best_rcut2b=abs:0.02``). The name of
              each row's tolerance is in the table below;
  searches    (optional) the searches of the page the line runs, separated
              by ',', each named as a row of expected.csv names it (below):
              the --hpotarget argument as the page writes it, or ``run
              <n>``. Without it the line runs every command of the page.
              This is the short variant of a long run.

Blank lines and lines starting with '#' are comments. --list checks the
manifest and prints ``<id>|<labels>|<timeout>`` per run (labels with
``examples`` first, separated by ','); CMake reads it to add the entries.

Every page under docs/source/nfp that runs ``tadah`` in a ``bash`` code
block needs a line, and so does every directory under docs/source/nfp/files
that holds a target file (*.hpo) or a training configuration (*.train). A
page whose line is labelled ``slow`` needs a second line without it, which
runs some of its searches: the short variant that ``ctest -LE slow`` runs.
coverage() lists what has none, the lines whose page or directory does not
exist, the searches a line names that its page does not run, and every
quote marker (below) that names an id, a source, a search, a command or a
tolerance no line has; the unit tests of the documentation tools
(tadah_docs_tools) fail on the shipped manifest when it lists anything.

The run
-------

A run copies the manifest's directory to <workdir>/files (the source tree is
never written; a run can read files of a sibling directory, such as
../data), then runs every command of the page's ``bash`` code blocks in
<workdir>/files/<dir>, in order (tests/doc_commands/doc_commands.py reads
them), with <tadah> in place of ``tadah``. A line that names its searches
runs each of them with the commands after it up to the next search (the
``tadah predict`` of its best potential), and no other command. Every
command must be a ``tadah`` command and exit 0, the commands within the
timeout, and unless it asks for help (-h, --help):

  * after ``tadah hpo`` (other than ``tadah hpo --check``):
    best_pot.tadah, hpo_run.log and a best_loss.tadah with at least one row
    exist;
  * after ``tadah train``: the potential it writes exists (pot.tadah, or
    the file of -o/--outfile).

``tadah hpo --check`` evaluates no candidate and writes no file: it has to
exit 0. Every other ``tadah hpo`` command is a search. Just before it, the
run checks its files with ``tadah hpo <its arguments> --check``, which has
to exit 0: a target file that needs a file an earlier command writes (the
seed of ``METHOD WARM``) is checked here, in the state the page runs it
(tests/nfp_docs/check_nfp_files.py leaves it to this run). The files of
the search are copied to <workdir>/snap/<n> before the next command runs
(n counts the page's searches from 1; a later search writes over them in
the directory), and the search is checked there:

  * the journal, hpo_run.log, has an "Optimiser status" line (the last one
    counts), and the status is not one of a run that did not complete
    (FAILED -- ..., SHUTDOWN (...), STOPPED -- no evaluation produced a
    usable loss, (no status recorded));
  * its "Total evaluations" line counts at least one evaluation;
  * the best value of every search parameter (the last row of
    best_params.tadah, whose columns after IMPROVEMENT, EVAL and SLOT are
    the dimensions of hpo_search_space.tsv, in its order) lies inside the
    box hpo_search_space.tsv gives it, to the precision of the two files (7
    and 10 significant digits).

The history files of a search have a header line and a row per
evaluation (loss.tadah, params.tadah, outvar.tadah: first column
ITERATION, the evaluation) or per new best (best_params.tadah,
best_outvar.tadah: first columns IMPROVEMENT, the number of the best, and
EVAL, its evaluation); outvar files then have IT-TIME/ms, and every file
SLOT, before the values (LEAD). A file whose header does not start so is
an error.

The standard output and the standard error of every command are kept;
that of a ``tadah predict`` command with the search it follows (the last
``tadah hpo`` before it).

Quotes
------

A page under docs/source/nfp (outside files/), or training.rst, quotes
program output in a ``.. code-block:: text`` preceded by a marker,

    .. output: <ids> <source> [search=<search>] [command=<n>] [tol=<tol>]

(tests/nfp_docs/nfp_files.py describes the grammar, the normalisations
and the match). After the commands and the rows of expected.csv, the run
of a line checks every quote whose marker names its id, on any of these
pages: the source is read from the run (the standard output or error of
the command, or the file of the search in <workdir>/snap/<n>), and the
quote has to match it. A quote that does not fails the run with its first
line that has no place in the source and the output line closest to it.
Only the commands the line runs can be quoted: a line that names its
searches runs only theirs.

expected.csv
------------

<dir>/expected.csv has two columns, a header row ``Quantity,Value``, and
one row per quantity: the table the page shows. A row names its quantity,
then its unit in brackets where the table below has one, then, on a page
with more than one search, the search it belongs to in brackets: the
--hpotarget argument of its ``tadah hpo`` command exactly as the page
writes it (``config.hpo``, ``../unit2/config.hpo``), or ``run <n>``, the
n-th search of the page (needed when two searches name the same target
file). On a page with one search the search may be left out. Every row is
measured from the files of its search; ``<name>`` is a search parameter as
hpo_search_space.tsv labels it (``RCUT2B``, ``CGRID2B[3]``,
``FIXWEIGHT[26]``, ``ESIGMA``, ``FSIGMA[1]``) or a column of outvar.tadah
(``ERMSE``, ``SRMSE_GPA``, ``Ecoh``). A row reads the file that holds its
number: best_pot.tadah, the history files, the table of tadah predict; the
journal, hpo_run.log, only for what no file holds (the status, the counts
of evaluations and of graded failures, the calibrated T_HOT and the
evaluations of each MULTISTART start). The rows, with the name of their
tolerance and what they measure:

  Best loss (tolerance best_loss)
      the "# HPO_BEST_LOSS" line of best_pot.tadah;
  Start loss (start_loss)
      TOTAL_LOSS/N of evaluation 1 (ITERATION 1) in loss.tadah;
  Best <name> (<unit>) (best_<name>)
      the last row of best_params.tadah or of best_outvar.tadah;
  Start <name> (<unit>) (start_<name>)
      the row of evaluation 1 in params.tadah or outvar.tadah;
  Best <name> - DFT (<unit>) (best_<name>_dft)
      Best <name> minus the DFT value of its quantity in dft_reference.csv
      (nfp_files.VARIABLE_QUANTITY maps the name to the quantity);
  Best loss of start <i> (best_loss_of_start)
      the lowest TOTAL_LOSS/N of the evaluations of MULTISTART start i in
      loss.tadah;
  Best <name> of start <i> (<unit>) (best_<name>_of_start)
      the search parameter in params.tadah at that evaluation (the first
      one on a tie);
  Calibrated T_HOT (calibrated_t_hot)
      the T_HOT of ANNEAL_PT after its calibration, the hot end of the
      ladder the search ran with: of the journal line "ANNEAL_PT
      calibration pass 2: T_HOT=<x> ..." when the second pass ran (after
      the step warm-up; it rebuilds the ladder), else the T0 of the line
      "ANNEAL_PT: T0=<x> from <n> probes" of the first;
  Evaluations (evaluations)
      the "Total evaluations" of the journal;
  Graded failures (graded_failures)
      the "Graded failures" of the journal: evaluations that failed and
      were scored a graded loss (FAILSCORE_GRADING);
  Status (no tolerance)
      the "Optimiser status" of the journal, as text, equal;
  <Kind> RMSE <dataset> (<unit>) (<kind>_rmse)
      the RMSE row of <Kind> (Energy, Force, Stress) of the table that
      ``tadah predict -A`` prints, in the column of <dataset>;
  Coefficient <i> of <file> (<unit>) (coefficient)
      the i-th value (from 1) of the WEIGHTS line of <file>, the potential
      a ``tadah train`` command of the page writes (-o/--outfile, else
      pot.tadah), as that command wrote it: the row names no search. A
      line measures it when it runs that command, and lists it as not
      measured otherwise.

``<name>`` in a tolerance is written in lower case (best_rcut2b,
best_fixweight[26], best_ecoh_dft). Words other than names and units may be
written in any case. The evaluations of MULTISTART start i are the rows of
loss.tadah that follow those of the starts before it, as many as its journal
line "MULTISTART start i/k done: <N> evaluation(s)" counts. A ``<Kind> RMSE``
row reads the one ``tadah predict -p best_pot.tadah ... -A`` command after its
search (and before the next) whose -d/--dbfile files include <dataset> (a
file name without directory and .tadahdb; Val<i> is the i-th file); its unit
is the one the RMSE row of the table names (``Force RMSE (eV/Å)``):
meV/atom, eV/Å or GPa.

The unit of a row is that of its quantity (tests/nfp_docs/nfp_files.py),
as tadah writes it (Å for the Angstrom): of a search parameter,
PARAMETER_UNITS, DESCRIPTOR_UNITS of the descriptor that the search's
training configuration (--config) names for its body (a grid key:
CGRID2B, SGRIDMB), or for ESIGMA ESIGMA_UNITS of its ENORM; of an
outvar.tadah column, the unit of its quantity in dft_reference.csv, else
OUTVAR_UNITS; a ``- DFT`` row has the unit of its quantity; a coefficient,
the unit nfp_files.coefficient_unit gives for the descriptors of the
training configuration of its ``tadah train`` command (-c/--config). A
quantity without a known unit is an error.

A page with a search needs expected.csv with the best loss of at least
one search. Each value is compared with the measurement within the
manifest's tolerance of the row (Status: the same text). A row this script
cannot measure, a row whose search, name, dataset or unit it cannot place,
a row without a tolerance and a tolerance that no row uses are errors: a
value a page quotes is checked or not quoted. A line that names its
searches measures the rows of those searches (the others are listed as
not measured), and its tolerances are those of these rows.

Tolerance classes
-----------------

A run on another build of tadah (another compiler, CPU or BLAS) repeats a
search only to the rounding of its numbers, which a search can grow: a
local search that compares two losses differently takes another path and
can end at another point, with another count of evaluations when a test
on the loss stops it. A tolerance of the manifest covers the results of
the line on several BLAS code paths (MKL on its default and its compatible
path, OpenBLAS on several kernels), and is at least that of the class of
each row it checks:

  * a loss (Best loss, Start loss, Best loss of start <i>): 1e-3 of the
    value;
  * a length (a <name> RCUT..., or a0, of a Best, Start, "of start" or
    "- DFT" row): 0.01 Å;
  * a count and a status (Evaluations, Graded failures, Status): exact;
  * every other quantity (grid and width parameters, error scales,
    coefficients, errors, properties, the calibrated T_HOT): 1e-2 of the
    value, and for a "- DFT" row 1e-2 of its property (the row's value
    plus the DFT value).

A tolerance is looser where the results of the line on these paths
spread wider (a search that ends at another point, a count that a test on
the loss sets): the manifest says so above the line. The value of a row is
written to about one digit past its tolerance (the digits below the
leading digit of the limit the tolerance gives it), and with at most two.
The tol= of an output marker, a number or the name of a tolerance of the
line, gives each number of the quote that has a point or an exponent a
limit of at least 1e-3 of the number, the tightest class, and a number
tol= is at least 1e-3 relative or 0.01 absolute; a quote whose numbers
belong to looser classes takes the loosest of them. precision_problems()
lists what breaks these rules, and the unit tests of the documentation
tools (tadah_docs_tools) fail on the shipped manifest when it lists
anything. In a build with MKL, CTest runs each line without slow a second
time with MKL_CBWR=COMPATIBLE, MKL's code path for a processor without
AVX (tadah_nfp_<id>_mkl_compatible, labels examples and slow).

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

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', 'doc_commands'))
sys.path.insert(0, HERE)
from doc_commands import bash_commands  # noqa: E402
import nfp_files  # noqa: E402

SKIP = 77
ID = re.compile(r'[A-Za-z0-9_]+')
LABEL = re.compile(r'[a-z0-9_]+')
NAME = r'[a-z0-9_.]+(?:\[[0-9]+\])?'
# The tolerance names of the manifest (the table of the docstring).
TOLERANCE = re.compile(rf'best_loss|start_loss|evaluations|calibrated_t_hot|'
                       rf'graded_failures|coefficient|'
                       rf'(?:energy|force|stress)_rmse|best_loss_of_start|'
                       rf'best_{NAME}_of_start|best_{NAME}_dft|'
                       rf'(?:best|start)_{NAME}')
# The files of a search that are kept before the next command runs: those
# a quote may name.
SNAPSHOT = nfp_files.SEARCH_FILES
# The status of a run that did not complete (run_driver.h).
UNFINISHED = ('FAILED', 'SHUTDOWN', 'STOPPED -- no evaluation produced a '
              'usable loss', '(no status recorded)')
# The leading columns of the history files of a search, before their
# values: loss.tadah, params.tadah and outvar.tadah have a row per
# evaluation (its number, ITERATION), the best_* files a row per new best
# (its number, IMPROVEMENT, and the evaluation it is, EVAL); outvar files
# give the time of the evaluation (IT-TIME/ms), and every file the replica
# (SLOT). HPO_Host::write_header_head and init_* in HPO's hpo_host_impl.hpp.
LEAD = {
    'loss.tadah': ('ITERATION', 'SLOT'),
    'params.tadah': ('ITERATION', 'SLOT'),
    'outvar.tadah': ('ITERATION', 'IT-TIME/ms', 'SLOT'),
    'best_params.tadah': ('IMPROVEMENT', 'EVAL', 'SLOT'),
    'best_outvar.tadah': ('IMPROVEMENT', 'EVAL', 'IT-TIME/ms', 'SLOT'),
}
# The journal lines of the calibration of T_HOT of ANNEAL_PT (anneal_core.h,
# algorithms_pt.h): the first pass ("from 1 probe" for one), and the second,
# after the step warm-up, which rebuilds the ladder from its T_HOT.
T0_FIRST_PASS = re.compile(r'ANNEAL_PT: T0=(\S+) from [0-9]+ probes?\b')
T_HOT_SECOND_PASS = re.compile(r'ANNEAL_PT calibration pass 2: T_HOT=(\S+) ')
# A row of the table of tadah predict -A: "<Kind> RMSE (<unit>)", then the
# Mean column and a column per dataset (predict_engine.cpp).
RMSE_ROW = re.compile(r'(Energy|Force|Stress) RMSE(?: \(([^()]+)\))?\s+(.*)')

# tolerance_texts: {name: the tolerance as the manifest writes it}.
Run = namedtuple('Run', 'id page dir labels timeout tolerances line '
                       'searches tolerance_texts', defaults=(None,))
Tolerance = namedtuple('Tolerance', 'value absolute')
# A tadah hpo command: n counts the page's searches from 1; target is its
# --hpotarget argument as the page writes it, config its --config argument.
Search = namedtuple('Search', 'n target line config')
# A tadah predict command and the search it follows (None: no search).
Predict = namedtuple('Predict', 'line search potential datasets output')
# A tadah train command: the potential it writes and its --config argument.
Train = namedtuple('Train', 'line potential config')
# A row of expected.csv: what it measures (kind, name, start, dataset,
# unit), the search, the value it gives and its tolerance name; the value
# as the row writes it and the number of the row in the file.
Row = namedtuple('Row', 'label kind name start dataset unit search value '
                        'tolerance text number', defaults=(None, None))


class ManifestError(Exception):
    pass


class RunError(Exception):
    pass


def parse_tolerances(text, where, texts=None):
    '''{name: Tolerance} of the tolerances field *text*; *texts*, when
    given, receives {name: the tolerance as written}.'''
    tolerances = {}
    if text == '-':
        return tolerances
    for item in text.split(','):
        name, eq, value = (s.strip() for s in item.partition('='))
        if not eq or not name or not value:
            raise ManifestError(f'{where}: tolerance "{item.strip()}" is not '
                                '<quantity>=<tolerance>')
        if not TOLERANCE.fullmatch(name):
            raise ManifestError(f'{where}: unknown quantity "{name}" (best_loss, '
                                'start_loss, best_<name>, start_<name>, '
                                'best_<name>_dft, best_loss_of_start, '
                                'best_<name>_of_start, evaluations, '
                                'graded_failures, calibrated_t_hot, '
                                '<kind>_rmse, coefficient)')
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
        if texts is not None:
            texts[name] = value
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
            if len(fields) not in (6, 7):
                raise ManifestError(f'{where}: {len(fields)} fields, expected 6 '
                                    'or 7: <id> | <page.rst> | <dir> | '
                                    '<labels> | <timeout> | <tolerances> '
                                    '[| <searches>]')
            rid, page, rdir, labels, timeout, tolerances = fields[:6]
            searches = None
            if len(fields) == 7:
                searches = [r.strip() for r in fields[6].split(',')]
                if not all(searches):
                    raise ManifestError(f'{where}: searches "{fields[6]}" is not '
                                        '<search>[, <search> ...] (a '
                                        '--hpotarget argument as the page '
                                        'writes it, or run <n>)')
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
            texts = {}
            runs.append(Run(rid, page, rdir, label_list, int(timeout),
                            parse_tolerances(tolerances, where, texts), number,
                            searches, texts))
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
            if r.searches:
                searches, _ = searches_and_predicts(bash_commands(page))
                try:
                    select_searches(r.searches, searches, r.page, where)
                except RunError as err:
                    problems.append(str(err))
        if 'slow' in r.labels and not any(
                o.page == r.page and 'slow' not in o.labels for o in runs):
            problems.append(f'{where}: the line {r.id} is slow, and no line '
                            f'without slow runs {r.page} (a short variant for '
                            'ctest -LE slow)')
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
    for page, line, message in quote_problems(manifest):
        problems.append(f'{os.path.relpath(page, docs_source)}:{line}: '
                        f'{message}')
    return problems


# --------------------------------------------------------------------------
# Quotes of program output
# --------------------------------------------------------------------------

def quote_pages(docs_source):
    """The pages that may quote a run: every page under docs_source/nfp
    (outside files/), then training.rst."""
    nfp = os.path.join(docs_source, 'nfp')
    files = os.path.join(nfp, 'files')
    pages = []
    for top, subdirs, names in os.walk(nfp):
        subdirs.sort()
        if _inside(top, files):
            continue
        pages += [os.path.join(top, n) for n in sorted(names)
                  if n.endswith('.rst')]
    training = os.path.join(docs_source, 'training.rst')
    if os.path.isfile(training):
        pages.append(training)
    return pages


def line_commands(entry, commands, where):
    """The lines of the *commands* of its page that the manifest line
    *entry* runs: every command, or each search it names with the commands
    after it up to the next search."""
    if entry.searches is None:
        return {c.line for c in commands}
    searches, _ = searches_and_predicts(commands)
    chosen = {s.n for s in select_searches(entry.searches, searches,
                                           entry.page, where)}
    of_line = {s.line: s for s in searches}
    kept, owner = set(), None
    for c in commands:
        owner = of_line.get(c.line, owner)
        if owner is not None and owner.n in chosen:
            kept.add(c.line)
    return kept


def measured_rows(entry, commands, rows, where):
    """The rows of expected.csv (*rows*) that the manifest line *entry*
    measures, in order: those of the searches it runs (of the *commands*
    of its page), and the coefficients of the potentials that the tadah
    train commands it runs write."""
    ran = line_commands(entry, commands, where)
    searches, _ = searches_and_predicts(commands)
    measured = {s.n for s in searches if s.line in ran}
    trains = trains_of([c for c in commands if c.line in ran])
    return [r for r in rows if (r.name in trains if r.kind == 'coefficient'
                                else r.search.n in measured)]


def resolve_quote(marker, entry, commands, where):
    """(search, command) of the source of *marker* in the run of the
    manifest line *entry*, whose page has the *commands*: the search of a
    file of a search (command None), the command of a stream and, when the
    stream is that of a search, the search. RunError when the line does
    not run them."""
    searches, _ = searches_and_predicts(commands)
    ran = line_commands(entry, commands, where)
    selected = [s for s in searches if s.line in ran]
    runs = (f'it runs the searches {", ".join(entry.searches)}'
            if entry.searches is not None else 'it runs every command')
    if marker.command is not None:
        if marker.command > len(commands):
            raise RunError(f'command={marker.command}, and {entry.page} has '
                           f'{len(commands)} tadah command(s) (the line '
                           f'{entry.id})')
        command = commands[marker.command - 1]
        if command.line not in ran:
            raise RunError(f'command={marker.command}, which the line '
                           f'{entry.id} does not run ({runs})')
        return None, command
    if marker.search is not None:
        ref = marker.search
        m = RUN_REF.fullmatch(ref)
        same = ([s for s in searches if s.n == int(m.group(1))] if m
                else [s for s in searches if s.target == ref])
        if not same:
            raise RunError(f'search={ref}: the marker names the search '
                           f'"{ref}", which {entry.page} does not run (its '
                           f'searches: {search_names(searches) or "none"})')
        if len(same) > 1:
            raise RunError(f'search={ref}: runs '
                           f'{", ".join(str(s.n) for s in same)} of '
                           f'{entry.page} both use {ref}: name one as '
                           "'run <n>'")
        search = same[0]
        if search not in selected:
            raise RunError(f'search={ref}, which the line {entry.id} does not '
                           f'run ({runs})')
    elif len(selected) != 1:
        raise RunError(f'the marker names no search, and the line {entry.id} '
                       f'runs {len(selected)} '
                       f'({search_names(selected) or "none"}): add search=')
    else:
        search = selected[0]
    command = (next(c for c in commands if c.line == search.line)
               if marker.source in nfp_files.STREAMS else None)
    return search, command


def quote_tolerance(marker, entry):
    """(value, absolute) of the tol= of *marker* for the manifest line
    *entry*, or None without one; RunError for a name the line has no
    tolerance of."""
    if marker.tol is None:
        return None
    try:
        tol = nfp_files.tolerance_of(marker.tol, entry.tolerances)
    except nfp_files.FilesError as err:
        raise RunError(str(err)) from None
    if tol is None:
        raise RunError(f'tol={marker.tol}, for which the line {entry.id} '
                       'gives no tolerance (its tolerances: '
                       f'{", ".join(entry.tolerances) or "none"})')
    return tol


def quote_problems(manifest, pages=None):
    """[(page, line, message)] of the output markers of *pages* (default:
    quote_pages()) that the manifest at *manifest* cannot check: a marker
    outside the grammar or without its text block, an id that is no line
    of the manifest, and a source, search, command or tolerance that the
    line does not have. Raises ManifestError for a manifest that cannot be
    read."""
    files = os.path.dirname(os.path.abspath(manifest))
    docs_source = os.path.dirname(os.path.dirname(files))
    runs = {r.id: r for r in parse_manifest(manifest)}
    problems = []
    commands_of = {}
    for page in quote_pages(docs_source) if pages is None else pages:
        for q in nfp_files.quote_blocks(page):
            if q.error is not None:
                problems.append((page, q.marker_line, q.error))
                continue
            for rid in q.marker.ids:
                entry = runs.get(rid)
                if entry is None:
                    problems.append((page, q.marker_line, f'the output marker '
                                     f'names "{rid}", which is not an id of '
                                     f'{manifest}'))
                    continue
                path = os.path.join(docs_source, entry.page)
                if not os.path.isfile(path):
                    problems.append((page, q.marker_line, f'the page of the '
                                     f'line {rid}, {entry.page}, does not '
                                     'exist'))
                    continue
                if path not in commands_of:
                    commands_of[path] = bash_commands(path)
                where = f'{manifest}:{entry.line}'
                try:
                    resolve_quote(q.marker, entry, commands_of[path], where)
                    quote_tolerance(q.marker, entry)
                except RunError as err:
                    problems.append((page, q.marker_line, str(err)))
    return problems


# --------------------------------------------------------------------------
# Tolerance classes
# --------------------------------------------------------------------------

# The smallest tolerance of each class (the docstring, "Tolerance
# classes"): a loss, relative; a length, absolute (Å); every other
# quantity, relative. Counts and statuses are exact.
LOSS_CLASS = 1e-3
LENGTH_CLASS = 0.01
RELATIVE_CLASS = 1e-2
LENGTH_NAMES = re.compile(r'rcut[a-z0-9_]*|a0', re.IGNORECASE)
LOSS_KINDS = ('best_loss', 'start_loss', 'start_best_loss')
EXACT_KINDS = ('evaluations', 'graded_failures', 'status')
# The most digits past its tolerance a value of expected.csv is written
# with (one is the rule; two leave room for a rounding).
MAX_DIGITS_PAST = 2


def row_class(row, reference):
    """(the smallest limit the class of *row* allows, the class in words)
    of a row of expected.csv, or None for a row checked exactly (a count,
    a status). *reference* is a callable that returns dft_reference.csv
    ({quantity: Reference}), for a "- DFT" row: its class is that of its
    property, the row's value plus the DFT value."""
    if row.kind in EXACT_KINDS:
        return None
    if row.kind in LOSS_KINDS:
        return LOSS_CLASS * abs(row.value), '1e-3 of the value, a loss'
    if (row.kind not in ('rmse', 'coefficient') and row.name is not None
            and LENGTH_NAMES.fullmatch(row.name)):
        return LENGTH_CLASS, '0.01 Å, a length'
    if row.kind == 'best_dft':
        quantity = nfp_files.VARIABLE_QUANTITY.get(row.name)
        if quantity is None:
            raise RunError(f'no quantity of dft_reference.csv is mapped to '
                           f'{row.name} (nfp_files.VARIABLE_QUANTITY)')
        ref = reference().get(quantity)
        if ref is None:
            raise RunError(f'dft_reference.csv has no {quantity}')
        floor = RELATIVE_CLASS * abs(row.value + ref.value)
        return floor, f'1e-2 of the property, {floor:.3g} {row.unit}'
    return RELATIVE_CLASS * abs(row.value), '1e-2 of the value'


def last_unit(text):
    """The value of one unit of the last digit the number *text* writes."""
    mantissa, _, exponent = text.lower().partition('e')
    decimals = len(mantissa.split('.', 1)[1]) if '.' in mantissa else 0
    return 10.0 ** ((int(exponent) if exponent else 0) - decimals)


def digits_past(limit, text):
    """How many digits the number *text* writes below the leading digit of
    *limit* (> 0): 0 when its last digit is that of the limit."""
    return math.floor(math.log10(limit / last_unit(text)) + 1e-9)


def one_digit_past(value, limit):
    """*value* rounded to one digit past the leading digit of *limit*."""
    decimals = 1 - math.floor(math.log10(limit) + 1e-9)
    return f'{value:.{max(decimals, 0)}f}'


def quoted_numbers(lines):
    """The numbers of the quote *lines* that a tolerance compares (a point
    or an exponent; an integer is text), after the masks of the match."""
    numbers = []
    for line in lines:
        text = nfp_files.normalise_output(line)
        numbers += [n for n in nfp_files.NUMBER.findall(text)
                    if re.search(r'[.eE]', n)]
    return numbers


def precision_problems(manifest):
    """[message] of what the manifest at *manifest* checks to more digits
    than its tolerance classes allow (the docstring, "Tolerance
    classes"): a tolerance that gives a row of expected.csv a limit below
    the class of the row, a value of expected.csv written with more than
    MAX_DIGITS_PAST digits past its tolerance, a number given as the tol=
    of an output marker below the tightest class (1e-3 relative, 0.01
    absolute), and a tol= of a marker, a number or the name of a tolerance
    of a line it names, that gives a number of the quote (with a point or
    an exponent) a limit below 1e-3 of the number. Only the rows a line
    measures are judged by its tolerances. A manifest that cannot be read
    is one message."""
    files = os.path.dirname(os.path.abspath(manifest))
    docs_source = os.path.dirname(os.path.dirname(files))
    try:
        runs = parse_manifest(manifest)
    except ManifestError as err:
        return [str(err)]
    loaded = {}

    def reference():
        if 'dft' not in loaded:
            loaded['dft'] = nfp_files.read_dft_reference(
                os.path.join(files, 'dft_reference.csv'))
        return loaded['dft']

    problems = []
    for entry in runs:
        where = f'{manifest}:{entry.line}'
        page = os.path.join(docs_source, entry.page)
        path = os.path.join(files, entry.dir, 'expected.csv')
        if not os.path.isfile(page) or not os.path.isfile(path):
            continue    # coverage() and the run report these
        shown = os.path.relpath(path, docs_source)
        try:
            commands = bash_commands(page)
            searches, _ = searches_and_predicts(commands)
            rows = measured_rows(entry, commands,
                                 read_expected(path, searches), where)
        except RunError as err:
            problems.append(str(err))
            continue
        for row in rows:
            tol = entry.tolerances.get(row.tolerance)
            if tol is None:
                continue    # the run reports a row without a tolerance
            limit = tol.value if tol.absolute else tol.value * abs(row.value)
            unit = f' {row.unit}' if row.unit else ''
            try:
                found = row_class(row, reference)
            except (OSError, RunError, nfp_files.FilesError) as err:
                problems.append(f'{shown}: row {row.number}: "{row.label}": '
                                f'{err}')
                continue
            if found is not None and limit < found[0] * (1 - 1e-9):
                text = entry.tolerance_texts[row.tolerance]
                problems.append(f'{where}: {row.tolerance}={text} gives '
                                f'"{row.label}" a tolerance of {limit:.3g}'
                                f'{unit}, tighter than its class '
                                f'({found[1]})')
            if limit > 0 and digits_past(limit, row.text) > MAX_DIGITS_PAST:
                problems.append(f'{shown}: row {row.number}: "{row.label}" '
                                f'{row.text}: '
                                f'{digits_past(limit, row.text)} digits past '
                                f'its tolerance ({limit:.3g}); one digit '
                                f'past: {one_digit_past(row.value, limit)}')
    lines = {entry.id: entry for entry in runs}
    for path in quote_pages(docs_source):
        shown = os.path.relpath(path, docs_source)
        for q in nfp_files.quote_blocks(path):
            if q.marker is None or q.marker.tol is None:
                continue
            tol = nfp_files.tolerance_of(q.marker.tol, {})
            if tol is not None:
                value, absolute = tol
                if value < (LENGTH_CLASS if absolute
                            else LOSS_CLASS) * (1 - 1e-9):
                    problems.append(f'{shown}:{q.marker_line}: '
                                    f'tol={q.marker.tol} is tighter than '
                                    'every tolerance class (1e-3 relative, '
                                    '0.01 absolute): the numbers of a quote '
                                    'are those of a run on another build as '
                                    'well')
                    continue
                given = [(tol, '')]
            else:
                # A name: the tolerance each line of the marker gives it
                # (coverage() reports a line that gives none).
                given = []
                for rid in q.marker.ids:
                    entry = lines.get(rid)
                    if entry is not None and q.marker.tol in entry.tolerances:
                        t = entry.tolerances[q.marker.tol]
                        given.append(((t.value, t.absolute),
                                      f' ({entry.tolerance_texts[q.marker.tol]}'
                                      f' of the line {rid})'))
            for number in quoted_numbers(q.lines):
                x = abs(float(number))
                for (value, absolute), of in given:
                    limit = value if absolute else value * x
                    if limit < LOSS_CLASS * x * (1 - 1e-9):
                        problems.append(
                            f'{shown}:{q.marker_line}: tol={q.marker.tol}{of} '
                            f'gives {number} a tolerance of {limit:.3g}, '
                            'below 1e-3 of the number, the tightest '
                            'tolerance class')
                        break
                else:
                    continue
                break
    return problems


# --------------------------------------------------------------------------
# The commands of a page
# --------------------------------------------------------------------------

def _option(argv, short, long_):
    '''The values of option -short/--long in *argv* (the words after it up
    to the next option; ``--long=value`` gives one), or None.'''
    for i, a in enumerate(argv):
        if a.startswith(long_ + '='):
            return [a.split('=', 1)[1]]
        if a in (short, long_):
            values = []
            for b in argv[i + 1:]:
                if b.startswith('-'):
                    break
                values.append(b)
            return values
    return None


def searches_and_predicts(commands):
    '''The searches (tadah hpo) and predictions (tadah predict) of the
    page's commands, in order; a help command and ``tadah hpo --check``,
    which evaluates no candidate and writes no file, are neither.'''
    searches, predicts = [], []
    for c in commands:
        if '-h' in c.argv or '--help' in c.argv:
            continue
        if c.argv[1:2] == ['hpo'] and '--check' in c.argv:
            continue
        if c.argv[1:2] == ['hpo']:
            target = _option(c.argv, '--hpotarget', '--hpotarget') or [None]
            config = _option(c.argv, '-c', '--config') or [None]
            searches.append(Search(len(searches) + 1, target[0], c.line,
                                   config[0]))
        elif c.argv[1:2] == ['predict']:
            pot = _option(c.argv, '-p', '--potential') or [None]
            data = _option(c.argv, '-d', '--dbfile') or []
            predicts.append(Predict(c.line, searches[-1] if searches else None,
                                    pot[0], data, None))
    return searches, predicts


def train_output(argv):
    '''The potential a ``tadah train`` command *argv* writes: the file of
    -o/--outfile, else pot.tadah.'''
    out = 'pot.tadah'
    for i, a in enumerate(argv):
        if a in ('-o', '--outfile') and i + 1 < len(argv):
            out = argv[i + 1]
        elif a.startswith('--outfile='):
            out = a.split('=', 1)[1]
    return out


def trains_of(commands):
    '''{potential: Train} of the ``tadah train`` commands of *commands*
    (the last command that writes a file counts); a help command is
    none.'''
    found = {}
    for c in commands:
        if c.argv[1:2] != ['train'] or '-h' in c.argv or '--help' in c.argv:
            continue
        config = _option(c.argv, '-c', '--config') or [None]
        found[train_output(c.argv)] = Train(c.line, train_output(c.argv),
                                            config[0])
    return found


def search_name(search):
    return (f'run {search.n} ({search.target})' if search.target
            else f'run {search.n}')


def search_names(searches):
    return ', '.join(search_name(s) for s in searches)


def select_searches(refs, searches, page, where):
    '''The searches a manifest line names (*refs*: --hpotarget arguments
    as the page writes them, or ``run <n>``), in page order.'''
    chosen = set()
    for ref in refs:
        m = RUN_REF.fullmatch(ref)
        same = ([s for s in searches if s.n == int(m.group(1))] if m
                else [s for s in searches if s.target == ref])
        if not same:
            raise RunError(f'{where}: the line names the search "{ref}", which '
                           f'{page} does not run (its searches: '
                           f'{search_names(searches) or "none"})')
        if len(same) > 1:
            raise RunError(f'{where}: runs {", ".join(str(s.n) for s in same)} '
                           f'of {page} both use {ref}: name one as "run <n>"')
        chosen.add(same[0].n)
    return [s for s in searches if s.n in chosen]


# --------------------------------------------------------------------------
# expected.csv
# --------------------------------------------------------------------------

I = re.IGNORECASE
SPLIT = re.compile(r'(.*\S)\s+\(([^()]+)\)')
RUN_REF = re.compile(r'run ([1-9][0-9]*)', I)
QUANTITY_PATTERNS = (
    ('status', re.compile(r'status', I)),
    ('evaluations', re.compile(r'evaluations', I)),
    ('graded_failures', re.compile(r'graded failures', I)),
    ('best_loss', re.compile(r'best loss', I)),
    ('start_loss', re.compile(r'start loss', I)),
    ('t_hot', re.compile(r'calibrated T_HOT', I)),
    ('start_best_loss', re.compile(r'best loss of start ([1-9][0-9]*)', I)),
    ('start_best', re.compile(r'best (\S+) of start ([1-9][0-9]*) '
                              r'\(([^()]+)\)', I)),
    ('rmse', re.compile(r'(energy|force|stress) RMSE (\S+) \(([^()]+)\)', I)),
    ('coefficient', re.compile(r'coefficient ([1-9][0-9]*) of (\S+) '
                               r'\(([^()]+)\)', I)),
    ('best_dft', re.compile(r'best (\S+) - DFT \(([^()]+)\)', I)),
    ('best', re.compile(r'best (\S+) \(([^()]+)\)', I)),
    ('start', re.compile(r'start (\S+) \(([^()]+)\)', I)),
)


def resolve_search(ref, searches, where):
    '''The search a row names (*ref*, None: none named).'''
    if not searches:
        raise RunError(f'{where}: the page runs no tadah hpo search')
    if ref is None:
        if len(searches) == 1:
            return searches[0]
        raise RunError(f'{where}: the page runs {len(searches)} searches '
                       f'({search_names(searches)}), and the row names none: '
                       'end it with " (<target file>)" or " (run <n>)"')
    m = RUN_REF.fullmatch(ref)
    if m:
        n = int(m.group(1))
        if n > len(searches):
            raise RunError(f'{where}: the page runs {len(searches)} '
                           f'search(es), not {n}')
        return searches[n - 1]
    same = [s for s in searches if s.target == ref]
    if len(same) > 1:
        raise RunError(f'{where}: runs {", ".join(str(s.n) for s in same)} '
                       f'both use {ref}: name one as " (run <n>)"')
    return same[0]


def parse_row(label, value, searches, where):
    '''The Row of the expected.csv row (*label*, *value*).'''
    head, ref = label, None
    m = SPLIT.fullmatch(label)
    if m and (RUN_REF.fullmatch(m.group(2))
              or any(s.target == m.group(2) for s in searches)):
        head, ref = m.group(1), m.group(2)
    for kind, pattern in QUANTITY_PATTERNS:
        m = pattern.fullmatch(head)
        # "loss" is the word of the loss rows, never a <name>.
        if m and not (kind in ('best', 'start', 'best_dft', 'start_best')
                      and m.group(1).lower() == 'loss'):
            break
    else:
        raise RunError(f'{where}: no check measures "{label}" (searches of '
                       f'the page: {search_names(searches) or "none"}; see '
                       'the rows of tests/nfp_docs/check_nfp_run.py)')
    if kind == 'coefficient':
        if ref is not None:
            raise RunError(f'{where}: "{label}" names a search; the '
                           'coefficient of a potential tadah train writes '
                           'belongs to none')
        search = None
    else:
        search = resolve_search(ref, searches, where)
    name = start = dataset = unit = None
    if kind == 'status':
        tolerance = None
    elif kind in ('best_loss', 'start_loss', 'evaluations', 'graded_failures'):
        tolerance = kind
    elif kind == 't_hot':
        tolerance = 'calibrated_t_hot'
    elif kind == 'start_best_loss':
        start, tolerance = int(m.group(1)), 'best_loss_of_start'
    elif kind == 'start_best':
        name, start, unit = m.group(1), int(m.group(2)), m.group(3)
        tolerance = f'best_{name.lower()}_of_start'
    elif kind == 'rmse':
        dataset, unit = m.group(2), m.group(3)
        name = m.group(1).lower()
        tolerance = name + '_rmse'
    elif kind == 'best_dft':
        name, unit = m.group(1), m.group(2)
        tolerance = f'best_{name.lower()}_dft'
    elif kind == 'coefficient':
        start, name, unit = int(m.group(1)), m.group(2), m.group(3)
        tolerance = 'coefficient'
    else:   # best, start
        name, unit = m.group(1), m.group(2)
        tolerance = f'{kind}_{name.lower()}'
    if kind == 'status':
        parsed = value
        if not value:
            raise RunError(f'{where}: the status is empty')
    else:
        try:
            parsed = float(value)
        except ValueError:
            parsed = math.nan
        if not math.isfinite(parsed):
            raise RunError(f'{where}: the value of "{label}" is not a number: '
                           f'{value}')
    return Row(label, kind, name, start, dataset, unit, search, parsed,
               tolerance, value)


def read_expected(path, searches):
    '''The rows of expected.csv at *path*, each a Row.'''
    with open(path, encoding='utf-8', newline='') as fh:
        rows = [r for r in csv.reader(fh) if r and any(c.strip() for c in r)]
    if not rows or [c.strip() for c in rows[0]] != ['Quantity', 'Value']:
        raise RunError(f'{path}: the header row is not "Quantity,Value"')
    expected, seen = [], {}
    for number, row in enumerate(rows[1:], 2):
        where = f'{path}: row {number}'
        if len(row) != 2:
            raise RunError(f'{where}: {row} has {len(row)} columns, not 2')
        label, value = row[0].strip(), row[1].strip()
        parsed = parse_row(label, value, searches, where)._replace(
            number=number)
        key = (parsed.kind, parsed.name, parsed.start, parsed.dataset,
               parsed.search.n if parsed.search is not None else None)
        if key in seen:
            raise RunError(f'{where}: "{label}" measures what row {seen[key]} '
                           'measures')
        seen[key] = number
        expected.append(parsed)
    return expected


# --------------------------------------------------------------------------
# The files of a search
# --------------------------------------------------------------------------

def read_table(path):
    '''(column names, rows) of a tadah history file: a header line
    ``#NAME NAME ...`` and rows of words.'''
    if not os.path.isfile(path):
        raise RunError(f'{path} was not written')
    names, rows = None, []
    with open(path, encoding='utf-8') as fh:
        for line in fh:
            if not line.strip():
                continue
            if line.startswith('#'):
                if names is None:
                    names = line[1:].split()
                continue
            rows.append(line.split())
    if names is None:
        raise RunError(f'{path} has no header line')
    for row in rows:
        if len(row) != len(names):
            raise RunError(f'{path}: a row has {len(row)} columns, the header '
                           f'{len(names)}')
    return names, rows


def read_history(snap, history):
    '''(column names, rows) of the history file *history* of the search
    (a file of LEAD), whose columns start with those LEAD gives it.'''
    path = os.path.join(snap, history)
    names, rows = read_table(path)
    lead = LEAD[history]
    if tuple(names[:len(lead)]) != lead:
        raise RunError(f'{path}: the columns {names} do not start with '
                       f'{", ".join(lead)}')
    return names, rows


def best_loss(snap):
    path = os.path.join(snap, 'best_pot.tadah')
    with open(path, encoding='utf-8') as fh:
        for line in fh:
            m = re.match(r'#\s*HPO_BEST_LOSS\s+(\S+)\s*$', line)
            if m:
                return float(m.group(1))
    raise RunError(f'{path} has no "# HPO_BEST_LOSS" line')


def search_space(snap):
    '''[(label, lo, hi)] of hpo_search_space.tsv, in dimension order.'''
    path = os.path.join(snap, 'hpo_search_space.tsv')
    if not os.path.isfile(path):
        raise RunError(f'{path} was not written')
    with open(path, encoding='utf-8') as fh:
        lines = [l for l in fh if l.strip() and not l.startswith('#')]
    if not lines:
        raise RunError(f'{path} has no header row')
    header = [c.strip() for c in lines[0].split('\t')]
    try:
        col = {k: header.index(k) for k in ('label', 'lo', 'hi')}
    except ValueError:
        raise RunError(f'{path}: the header has no label, lo and hi '
                       f'columns: {header}') from None
    dims = []
    for line in lines[1:]:
        cells = [c.strip() for c in line.split('\t')]
        dims.append((cells[col['label']], float(cells[col['lo']]),
                     float(cells[col['hi']])))
    if not dims:
        raise RunError(f'{path} lists no dimension')
    return dims


def _column_names_label(column, label):
    '''Whether a column name of params.tadah names *label*: the label
    itself, or the label shortened to fit, with '*' for the left-out
    characters (FIXWEI*[26] for FIXWEIGHT[26]).'''
    if column == label:
        return True
    if '*' in column:
        head, tail = column.split('*', 1)
        return (label.startswith(head) and label.endswith(tail)
                and len(label) >= len(head) + len(tail))
    return False


def parameter_columns(snap, history):
    '''{label: column index} of the search parameters in *history*
    (best_params.tadah or params.tadah), whose columns after the leading
    ones (LEAD) are the dimensions of hpo_search_space.tsv, in its order.'''
    names, rows = read_table(os.path.join(snap, history))
    dims = search_space(snap)
    lead = LEAD[history]
    n = len(lead)
    if tuple(names[:n]) != lead or len(names) != n + len(dims):
        raise RunError(f'{snap}/{history}: the columns {names} are not '
                       f'{", ".join(lead)} and the {len(dims)} dimension(s) '
                       'of hpo_search_space.tsv')
    columns = {}
    for i, (label, _, _) in enumerate(dims):
        if not _column_names_label(names[n + i], label):
            raise RunError(f'{snap}/{history}: column {n + 1 + i} is '
                           f'{names[n + i]}, hpo_search_space.tsv dimension '
                           f'{i + 1} is {label}')
        columns[label] = n + i
    return columns, rows


def best_parameters(snap):
    '''{label: best value} of the search: the last row of
    best_params.tadah.'''
    columns, rows = parameter_columns(snap, 'best_params.tadah')
    if not rows:
        raise RunError(f'{snap}/best_params.tadah has no row')
    return {label: float(rows[-1][c]) for label, c in columns.items()}


def journal(snap):
    path = os.path.join(snap, 'hpo_run.log')
    with open(path, encoding='utf-8', errors='replace') as fh:
        return fh.read()


def journal_status(snap):
    found = re.findall(r'^\s*Optimiser status\s*:\s*(.*?)\s*$', journal(snap),
                       re.MULTILINE)
    if not found:
        raise RunError(f'{snap}/hpo_run.log has no "Optimiser status" line')
    return found[-1]


def journal_count(snap, name):
    '''The count of the journal line "<name> : <n>" (the last one).'''
    found = re.findall(rf'^\s*{name}\s*:\s*([0-9]+)', journal(snap),
                       re.MULTILINE)
    if not found:
        raise RunError(f'{snap}/hpo_run.log has no "{name}" line')
    return int(found[-1])


def journal_evaluations(snap):
    return journal_count(snap, 'Total evaluations')


def calibrated_t_hot(snap, where):
    '''The T_HOT of ANNEAL_PT after its calibration, the hot end of the
    ladder the search ran with: that of the second pass when it ran (it
    rebuilds the ladder), else the T0 of the first.'''
    text = journal(snap)
    first = T0_FIRST_PASS.findall(text)
    if len(first) != 1:
        raise RunError(f'{where}: {len(first)} journal lines "ANNEAL_PT: '
                       'T0=<x> from <n> probes" (the first calibration of '
                       'T_HOT), not 1')
    second = T_HOT_SECOND_PASS.findall(text)
    if len(second) > 1:
        raise RunError(f'{where}: {len(second)} journal lines "ANNEAL_PT '
                       'calibration pass 2: T_HOT=<x>", not 0 or 1')
    return float(second[0] if second else first[0])


def check_search(snap, search):
    '''The checks every search passes, whatever expected.csv holds; a list
    of the failures.'''
    where = search_name(search)
    problems = []
    status = journal_status(snap)
    if any(status.startswith(s) for s in UNFINISHED):
        problems.append(f'{where}: the search did not complete: status '
                        f'"{status}"')
    evaluations = journal_evaluations(snap)
    if evaluations < 1:
        problems.append(f'{where}: the journal counts {evaluations} '
                        'evaluations')
    box = {label: (lo, hi) for label, lo, hi in search_space(snap)}
    for label, value in best_parameters(snap).items():
        lo, hi = box[label]
        slack = 1e-6 * max(abs(value), abs(lo), abs(hi))
        if not lo - slack <= value <= hi + slack:
            problems.append(f'{where}: the best {label} {value!r} lies outside '
                            f'its OPTIM box [{lo!r}, {hi!r}]')
    return problems, status, evaluations


def evaluation_row(snap, history, iteration=1):
    '''(column names, the row of evaluation *iteration*) of *history*
    (loss.tadah, params.tadah or outvar.tadah, whose first column is
    ITERATION).'''
    names, rows = read_history(snap, history)
    found = [r for r in rows if r[0] == str(iteration)]
    if len(found) != 1:
        raise RunError(f'{snap}/{history}: {len(found)} rows of evaluation '
                       f'{iteration}, not 1')
    return names, found[0]


def column(names, row, name, path):
    if name not in names:
        raise RunError(f'{path} has no column {name} (columns: '
                       f'{" ".join(names)})')
    return float(row[names.index(name)])


def outvar_names(snap):
    '''The columns of best_outvar.tadah after its leading ones (LEAD): the
    validation errors, the variables of the LAMMPS scripts and the values of
    the observables.'''
    history = 'best_outvar.tadah'
    if not os.path.isfile(os.path.join(snap, history)):
        return []
    names, _ = read_history(snap, history)
    return names[len(LEAD[history]):]


def named_source(snap, name, where):
    '''Whether *name* is a search parameter ('param') or an outvar.tadah
    column ('outvar') of the search.'''
    params = [label for label, _, _ in search_space(snap)]
    outvars = outvar_names(snap)
    if name in params and name in outvars:
        raise RunError(f'{where}: {name} is both a search parameter and a '
                       'column of outvar.tadah')
    if name in params:
        return 'param'
    if name in outvars:
        return 'outvar'
    raise RunError(f'{where}: {name} is neither a search parameter of run '
                   f'(search parameters: {" ".join(params)}) nor a column of '
                   f'outvar.tadah (columns: {" ".join(outvars) or "none"})')


def multistart_rows(snap, start, where):
    '''The loss.tadah rows (and their column names) of MULTISTART start
    *start*.'''
    done = re.findall(r'MULTISTART start ([0-9]+)/([0-9]+) done: ([0-9]+) '
                      r'evaluation\(s\)', journal(snap))
    if not done:
        raise RunError(f'{where}: the journal has no "MULTISTART start i/k '
                       'done" line')
    counts = {}
    for i, k, n in done:
        if int(i) in counts:
            raise RunError(f'{where}: the journal ends start {i} twice')
        counts[int(i)] = int(n)
    if sorted(counts) != list(range(1, len(counts) + 1)):
        raise RunError(f'{where}: the journal ends starts {sorted(counts)}')
    if start not in counts:
        raise RunError(f'{where}: the search ran {len(counts)} start(s), not '
                       f'{start}')
    names, rows = read_history(snap, 'loss.tadah')
    if sum(counts.values()) != len(rows):
        raise RunError(f'{where}: the starts count {sum(counts.values())} '
                       f'evaluations, loss.tadah has {len(rows)} rows')
    first = sum(counts[i] for i in range(1, start))
    return names, rows[first:first + counts[start]]


def start_best(snap, start, where):
    '''(loss, ITERATION) of the best evaluation of MULTISTART start
    *start* (the first one on a tie).'''
    names, rows = multistart_rows(snap, start, where)
    if 'TOTAL_LOSS/N' not in names or not rows:
        raise RunError(f'{where}: no TOTAL_LOSS/N of start {start} in '
                       'loss.tadah')
    c = names.index('TOTAL_LOSS/N')
    best = min(rows, key=lambda r: float(r[c]))
    return float(best[c]), best[0]


def predict_table(text, where):
    '''{(kind, column index from 1): (value, unit)} of the RMSE rows of the
    table tadah predict -A prints: a header "Metric Mean Val1 Val2 ...",
    and per kind a row "<Kind> RMSE (<unit>)" with a value per column of
    the header (unit None when the row names none).'''
    header = None
    values = {}
    for line in text.splitlines():
        words = line.split()
        if words[:1] == ['Metric']:
            header = words[1:]
            continue
        m = RMSE_ROW.fullmatch(line.strip())
        if not m or header is None:
            continue
        cells = m.group(3).split()
        if len(cells) != len(header):
            raise RunError(f'{where}: the {m.group(1)} RMSE row of the table '
                           f'of tadah predict has {len(cells)} values, its '
                           f'header {len(header)} columns ({" ".join(header)})')
        for name, x in zip(header, cells):
            col = re.fullmatch(r'Val([0-9]+)', name)
            if col:
                values[(m.group(1).lower(), int(col.group(1)))] = (
                    float(x), m.group(2))
    if header is None:
        raise RunError(f'{where}: the output of tadah predict has no '
                       '"Metric" table (-A)')
    return values


def dataset_name(path):
    name = os.path.basename(path)
    return name[:-len('.tadahdb')] if name.endswith('.tadahdb') else name


def measure_rmse(row, predicts, where):
    after = [p for p in predicts if p.search is not None
             and p.search.n == row.search.n]
    reading = [p for p in after if row.dataset in
               [dataset_name(d) for d in p.datasets]]
    if not reading:
        raise RunError(f'{where}: no tadah predict command after '
                       f'{search_name(row.search)} reads {row.dataset}')
    if len(reading) > 1:
        raise RunError(f'{where}: tadah predict commands on lines '
                       f'{", ".join(str(p.line) for p in reading)} read '
                       f'{row.dataset} after run {row.search.n}')
    p = reading[0]
    if p.potential != 'best_pot.tadah':
        raise RunError(f'{where}: the tadah predict command on line {p.line} '
                       f'reads {p.potential}, not the best_pot.tadah of run '
                       f'{row.search.n}')
    names = [dataset_name(d) for d in p.datasets]
    if names.count(row.dataset) > 1:
        raise RunError(f'{where}: the tadah predict command on line {p.line} '
                       f'reads two files named {row.dataset}')
    table = predict_table(p.output, f'{where}: line {p.line}')
    key = (row.name, names.index(row.dataset) + 1)
    if key not in table:
        raise RunError(f'{where}: the table of the tadah predict command on '
                       f'line {p.line} has no {row.name} RMSE of '
                       f'Val{key[1]}')
    value, unit = table[key]
    if unit is None:
        raise RunError(f'{where}: the {row.name.capitalize()} RMSE row of the '
                       f'table of the tadah predict command on line {p.line} '
                       'names no unit')
    if unit != row.unit:
        raise RunError(f'{where}: tadah predict prints the {row.name} RMSE in '
                       f'{unit}, not {row.unit}')
    return value


def check_unit(row, unit, note, where):
    if row.unit != unit:
        raise RunError(f'{where}: {row.name} is in {unit}{note}, not {row.unit}')


def parameter_unit(row, run_dir, where):
    '''(unit, note) of the search parameter of *row*, whose grid keys take
    the unit of the descriptor of the search's training configuration, and
    ESIGMA that of its ENORM.'''
    config = row.search.config
    if config is None:
        raise RunError(f'{where}: the training configuration of '
                       f'{search_name(row.search)} is not on its command line '
                       '(--config), so the unit of its search parameters is '
                       'not known')
    try:
        found = nfp_files.configuration(os.path.join(run_dir, config), run_dir)
        return nfp_files.parameter_unit(row.name, found)
    except (OSError, nfp_files.FilesError) as err:
        raise RunError(f'{where}: {err}') from None


def outvar_unit(row, files, where):
    path = os.path.join(files, 'dft_reference.csv')
    try:
        reference = (nfp_files.read_dft_reference(path)
                     if os.path.isfile(path) else {})
        return nfp_files.outvar_unit(row.name, reference)
    except (OSError, nfp_files.FilesError) as err:
        raise RunError(f'{where}: {err}') from None


def weights_line(path):
    '''The values of the WEIGHTS line of the potential at *path*, or None
    when it has none.'''
    with open(path, encoding='utf-8') as fh:
        for line in fh:
            words = line.split('#', 1)[0].split()
            if words[:1] == ['WEIGHTS']:
                try:
                    return [float(w) for w in words[1:]]
                except ValueError:
                    raise RunError(f'{path}: a value of its WEIGHTS line is not '
                                   'a number') from None
    return None


def measure_coefficient(row, trained, run_dir, where):
    '''The coefficient of *row*: of the WEIGHTS line that its tadah train
    command wrote (*trained*: {potential: (Train, values or None)}).'''
    train, values = trained[row.name]
    if train.config is None:
        raise RunError(f'{where}: the training configuration of the tadah '
                       f'train command that writes {row.name} is not on its '
                       'command line (-c), so the unit of its coefficients is '
                       'not known')
    try:
        found = nfp_files.configuration(os.path.join(run_dir, train.config),
                                        run_dir)
        unit = nfp_files.coefficient_unit(f'a coefficient of {row.name}',
                                          found)
    except (OSError, nfp_files.FilesError) as err:
        raise RunError(f'{where}: {err}') from None
    if row.unit != unit:
        raise RunError(f'{where}: a coefficient of {row.name} is in {unit}, '
                       f'not {row.unit}')
    if values is None:
        raise RunError(f'{where}: {row.name} has no WEIGHTS line')
    if row.start > len(values):
        raise RunError(f'{where}: {row.name} has {len(values)} coefficients '
                       f'(WEIGHTS), not {row.start}')
    return values[row.start - 1]


def measure(row, snaps, predicts, files, run_dir, where, trained=None):
    '''The value of *row* measured from the files of its search, or for a
    coefficient from the potential its tadah train command wrote.'''
    if row.kind == 'coefficient':
        return measure_coefficient(row, trained, run_dir, where)
    snap = snaps[row.search.n]
    kind = row.kind
    if kind == 'best_loss':
        return best_loss(snap)
    if kind == 'start_loss':
        names, r = evaluation_row(snap, 'loss.tadah')
        return column(names, r, 'TOTAL_LOSS/N', f'{snap}/loss.tadah')
    if kind == 'evaluations':
        return journal_evaluations(snap)
    if kind == 'graded_failures':
        return journal_count(snap, 'Graded failures')
    if kind == 'status':
        return journal_status(snap)
    if kind == 't_hot':
        return calibrated_t_hot(snap, where)
    if kind == 'start_best_loss':
        return start_best(snap, row.start, where)[0]
    if kind == 'start_best':
        if named_source(snap, row.name, where) != 'param':
            raise RunError(f'{where}: {row.name} is not a search parameter')
        check_unit(row, *parameter_unit(row, run_dir, where), where)
        _, iteration = start_best(snap, row.start, where)
        columns, rows = parameter_columns(snap, 'params.tadah')
        found = [r for r in rows if r[0] == iteration]
        if len(found) != 1:
            raise RunError(f'{where}: {len(found)} rows of evaluation '
                           f'{iteration} in params.tadah, not 1')
        return float(found[0][columns[row.name]])
    if kind == 'rmse':
        return measure_rmse(row, predicts, where)
    source = named_source(snap, row.name, where)
    if kind in ('best', 'start'):
        check_unit(row, *(parameter_unit(row, run_dir, where)
                          if source == 'param'
                          else outvar_unit(row, files, where)), where)
    if kind == 'best':
        if source == 'param':
            return best_parameters(snap)[row.name]
        names, rows = read_history(snap, 'best_outvar.tadah')
        if not rows:
            raise RunError(f'{snap}/best_outvar.tadah has no row')
        return column(names, rows[-1], row.name, f'{snap}/best_outvar.tadah')
    if kind == 'start':
        if source == 'param':
            columns, _ = parameter_columns(snap, 'params.tadah')
            names, r = evaluation_row(snap, 'params.tadah')
            return float(r[columns[row.name]])
        names, r = evaluation_row(snap, 'outvar.tadah')
        return column(names, r, row.name, f'{snap}/outvar.tadah')
    # best_dft
    if source != 'outvar':
        raise RunError(f'{where}: {row.name} is a search parameter, not a '
                       'property with a DFT value')
    quantity = nfp_files.VARIABLE_QUANTITY.get(row.name)
    if quantity is None:
        raise RunError(f'{where}: no quantity of dft_reference.csv is mapped '
                       f'to {row.name} (nfp_files.VARIABLE_QUANTITY)')
    path = os.path.join(files, 'dft_reference.csv')
    try:
        reference = nfp_files.read_dft_reference(path)
    except (OSError, nfp_files.FilesError) as err:
        raise RunError(f'{where}: {err}') from None
    if quantity not in reference:
        raise RunError(f'{where}: {path} has no {quantity}')
    check_unit(row, reference[quantity].unit, ' (dft_reference.csv)', where)
    names, rows = read_history(snap, 'best_outvar.tadah')
    if not rows:
        raise RunError(f'{snap}/best_outvar.tadah has no row')
    best = column(names, rows[-1], row.name, f'{snap}/best_outvar.tadah')
    return best - reference[quantity].value


# --------------------------------------------------------------------------
# A run
# --------------------------------------------------------------------------

def outputs_missing(argv, run_dir):
    '''What a command that exited 0 should have written and did not.'''
    exists = lambda name: os.path.isfile(os.path.join(run_dir, name))
    if '-h' in argv or '--help' in argv:
        return []
    missing = []
    if argv[1:2] == ['hpo'] and '--check' in argv:
        return []
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
        out = train_output(argv)
        if not exists(out):
            missing.append(f'{out} was not written')
    return missing


def last_lines(proc, n=40):
    """The last *n* lines of the standard output, then of the standard
    error, of a completed process."""
    lines = (proc.stdout or '').splitlines() + (proc.stderr or '').splitlines()
    return '\n'.join(lines[-n:])


def check_quotes(entry, commands, docs_source, snaps, outputs, where):
    """Checks every quote whose marker names the manifest line *entry*
    against the run: the files of each search in *snaps* ({n: snapshot
    directory}) and the (stdout, stderr) of each command in *outputs*
    ({line: ...}); prints a line per quote and returns the quotes that
    fail (page:line)."""
    failed = []
    for page in quote_pages(docs_source):
        rel = os.path.relpath(page, docs_source)
        for q in nfp_files.quote_blocks(page):
            if q.marker is None or entry.id not in q.marker.ids:
                continue
            name = f'{rel}:{q.line}'
            try:
                search, command = resolve_quote(q.marker, entry, commands,
                                                where)
                tolerance = quote_tolerance(q.marker, entry)
                if command is not None:
                    n = commands.index(command) + 1
                    what = (f'{q.marker.source} of command {n} '
                            f'({entry.page}:{command.line})')
                    stream = nfp_files.STREAMS.index(q.marker.source)
                    text = outputs[command.line][stream]
                else:
                    what = f'{q.marker.source} of {search_name(search)}'
                    path = os.path.join(snaps[search.n], q.marker.source)
                    if not os.path.isfile(path):
                        raise RunError(f'{search_name(search)} wrote no '
                                       f'{q.marker.source}')
                    with open(path, encoding='utf-8', errors='replace') as fh:
                        text = fh.read()
                problem = nfp_files.match_quote(q.lines, text, tolerance)
            except RunError as err:
                what, problem = q.marker.source, str(err)
            if problem is None:
                print(f'ok: quote {name} ({what})')
            else:
                print(f'FAIL: quote {name} ({what}): {problem}')
                failed.append(name)
    return failed


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
    page_commands = commands
    for c in commands:
        if c.argv[0] != 'tadah':
            raise RunError(f'{page}:{c.line}: "{" ".join(c.argv)}" is not a '
                           'tadah command (only tadah commands are run)')
    searches, predicts = searches_and_predicts(commands)
    where = f'{args.manifest}:{entry.line}'
    selected = (searches if entry.searches is None else
                select_searches(entry.searches, searches, entry.page, where))
    # The commands of the line: every command, or each named search with
    # the commands after it up to the next search.
    search_of_line = {s.line: s for s in searches}
    if entry.searches is not None:
        chosen, owner = {s.n for s in selected}, None
        kept = []
        for c in commands:
            owner = search_of_line.get(c.line, owner)
            if owner is not None and owner.n in chosen:
                kept.append(c)
        commands = kept
    if args.nfp == 'OFF' and any(c.argv[1:2] == ['hpo'] for c in commands):
        print(f'SKIP: {entry.page} runs tadah hpo, which needs a build with '
              'TADAH_ENABLE_NFP=ON')
        return SKIP

    expected_csv = os.path.join(src_dir, 'expected.csv')
    rows = (read_expected(expected_csv, searches)
            if os.path.isfile(expected_csv) else [])
    if searches and not any(r.kind == 'best_loss' for r in rows):
        raise RunError(f'{expected_csv} must hold the "Best loss" of a search '
                       '(the page runs tadah hpo)')
    page_trains = trains_of(page_commands)
    for r in rows:
        if r.kind == 'coefficient' and r.name not in page_trains:
            raise RunError(f'{expected_csv}: "{r.label}": {r.name} is not the '
                           'potential of a tadah train command of '
                           f'{entry.page} ('
                           f'{", ".join(sorted(page_trains)) or "none"})')
    expected = measured_rows(entry, page_commands, rows, where)
    line_trains = trains_of(commands)
    used = {r.tolerance for r in expected if r.tolerance is not None}
    for name in sorted(used):
        if name not in entry.tolerances:
            raise RunError(f'{where}: no tolerance for "{name}" of '
                           f'{expected_csv}')
    for name in entry.tolerances:
        if name not in used:
            which = (f'no row of {expected_csv} uses' if entry.searches is None
                     else 'no row of the searches of the line uses '
                          f'({expected_csv})')
            raise RunError(f'{where}: tolerance of "{name}", which {which}')

    # The commands run in the copy: a relative path to tadah is the
    # caller's, a bare name is looked up on PATH.
    tadah = os.path.abspath(args.tadah) if os.sep in args.tadah else args.tadah
    workdir = os.path.abspath(args.workdir)
    shutil.rmtree(workdir, ignore_errors=True)
    shutil.copytree(files, os.path.join(workdir, 'files'))
    run_dir = os.path.join(workdir, 'files', entry.dir)
    snaps, outputs, trained = {}, {}, {}
    deadline = time.monotonic() + entry.timeout
    started = time.monotonic()

    def call(argv, line):
        '''Runs the tadah command *argv* in the run's directory; the
        completed process (its standard output and error apart), or
        RunError unless it exits 0 in time.'''
        shown = ' '.join(shlex.quote(a) for a in argv)
        print(f'$ {shown}   ({entry.page}:{line})', flush=True)
        left = deadline - time.monotonic()
        try:
            # tadah writes UTF-8 (the Å of the units), whatever the locale.
            proc = subprocess.run([tadah] + argv[1:], cwd=run_dir,
                                  stdout=subprocess.PIPE,
                                  stderr=subprocess.PIPE, encoding='utf-8',
                                  errors='replace', timeout=max(left, 1))
        except subprocess.TimeoutExpired:
            raise RunError(f'`{shown}` did not end within the run\'s '
                           f'{entry.timeout} s') from None
        if proc.returncode != 0:
            raise RunError(f'`{shown}` exited with {proc.returncode}:\n'
                           f'{last_lines(proc)}')
        return proc

    for c in commands:
        shown = ' '.join(shlex.quote(a) for a in c.argv)
        if c.line in search_of_line:
            # The files of the search as the page runs it, after the
            # commands before it (a WARM seed that one of them writes).
            call(c.argv + ['--check'], c.line)
            print('ok (exit 0: tadah hpo --check)', flush=True)
        t0 = time.monotonic()
        proc = call(c.argv, c.line)
        missing = outputs_missing(c.argv, run_dir)
        if missing:
            raise RunError(f'`{shown}` exited with 0, but '
                           + '; '.join(missing) + f':\n{last_lines(proc)}')
        outputs[c.line] = (proc.stdout, proc.stderr)
        print(f'ok (exit 0, {time.monotonic() - t0:.1f} s)', flush=True)
        if c.argv[1:2] == ['train'] and train_output(c.argv) in line_trains:
            # The coefficients as this command wrote them: a later command
            # may write the file again.
            out = train_output(c.argv)
            train = line_trains[out]
            if train.line == c.line:
                trained[out] = (train, weights_line(os.path.join(run_dir,
                                                                 out)))
        search = search_of_line.get(c.line)
        if search is not None:
            snap = os.path.join(workdir, 'snap', str(search.n))
            os.makedirs(snap)
            for name in SNAPSHOT:
                if os.path.isfile(os.path.join(run_dir, name)):
                    shutil.copy2(os.path.join(run_dir, name), snap)
            snaps[search.n] = snap
            problems, status, evaluations = check_search(snap, search)
            print(f'{search_name(search)}: status "{status}", '
                  f'{evaluations} evaluations', flush=True)
            if problems:
                raise RunError('; '.join(problems))
    predicts = [p._replace(output=outputs.get(p.line, (None,))[0])
                for p in predicts]

    failed = []
    for row in rows:
        if row in expected:
            continue
        if row.kind == 'coefficient':
            print(f'not measured: {row.label} (no command of this line writes '
                  f'{row.name})')
        else:
            print(f'not measured: {row.label} ({search_name(row.search)} is '
                  'not run by this line)')
    for row in expected:
        where = f'{expected_csv}: "{row.label}"'
        got = measure(row, snaps, predicts, os.path.join(workdir, 'files'),
                      run_dir, where, trained)
        if row.kind == 'status':
            verdict = 'ok' if got == row.value else 'FAIL'
            print(f'{verdict}: {row.label}: "{got}", expected "{row.value}"')
        else:
            tol = entry.tolerances[row.tolerance]
            limit = tol.value if tol.absolute else tol.value * abs(row.value)
            kind = 'absolute' if tol.absolute else 'relative'
            verdict = 'ok' if abs(got - row.value) <= limit else 'FAIL'
            print(f'{verdict}: {row.label}: {got!r}, expected {row.value!r} '
                  f'within {kind} {tol.value!r} ({row.tolerance})')
        if verdict != 'ok':
            failed.append(row.label)
    unquoted = check_quotes(entry, page_commands, docs_source, snaps, outputs,
                            f'{args.manifest}:{entry.line}')
    if failed or unquoted:
        raise RunError('; '.join(
            ([f'outside the tolerance: {"; ".join(failed)} (expected.csv of '
              f'{entry.dir})'] if failed else [])
            + ([f'quotes that are not the output of the run: '
                f'{", ".join(unquoted)}'] if unquoted else [])))
    print(f'PASS: {len(commands)} command(s) of {entry.page} in {entry.dir} '
          f'({time.monotonic() - started:.1f} s)')
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
