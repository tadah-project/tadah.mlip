"""What the checks of the Nested Fitting files share: the DFT reference
values, the lines of a target file and the units of the quantities the
pages quote.

docs/source/nfp/files/dft_reference.csv holds the DFT value of every
property a target file compares with DFT (header
``quantity,value,unit,source,method``). A target file names a property in
two ways, mapped to its quantity here and nowhere else:

  * the variable of a ``--varloss <variable> <target> <weight>`` option of
    a PC_LAMMPS or PC_FAILPROBE line, the name the LAMMPS script gives it
    (VARIABLE_QUANTITY);
  * the observable of a ``PC_OBSERVABLE <observable> ... --target <value>``
    line (OBSERVABLE_QUANTITY).

A variable or an observable missing from these tables is an error of the
check that meets it, not a value it leaves unchecked. A script also
starts from DFT values: the initial value of ``--hotvar <variable> <lo>
<hi> <initial>`` and the value of ``--invar <variable> <value>`` are DFT
values when VARIABLE_QUANTITY maps the variable; another ``--hotvar`` or
``--invar`` is an input of the script (a step count, a temperature).

The unit of a quantity a page quotes (a row of expected.csv) is that of
dft_reference.csv for a DFT quantity, OUTVAR_UNITS for a validation error
of outvar.tadah, and for a search parameter PARAMETER_UNITS, or, for a key
whose unit depends on the training configuration, DESCRIPTOR_UNITS of the
descriptor it names (a grid) or ESIGMA_UNITS of its ENORM (ESIGMA). A
quantity none of them gives a unit is an error. Units are written as tadah
writes them: Å for the Angstrom.

Quotes of program output
------------------------

A page quotes what a run of the manifest prints or writes in a
``.. code-block:: text`` preceded by a marker, an rst comment of one line
(blank lines may separate the two)::

    .. output: <ids> <source> [search=<search>] [command=<n>] [tol=<tol>]

  ids      the manifest line that checks the quote (its id), or several
           separated by ',' without blanks; the run of each line checks it;
  source   ``stdout`` or ``stderr`` of a command, or a file a search writes
           (SEARCH_FILES: ``hpo_run.log``, ``best_pot.tadah``, ...), read
           as the search left it, before the next command;
  search   the search, named as expected.csv names one: the --hpotarget
           argument as the page writes it, or ``'run <n>'`` (quoted);
           needed when the line runs more than one search;
  command  the n-th command of the page's bash code blocks (from 1), for
           ``stdout`` and ``stderr``; without it, the ``tadah hpo``
           command of the search;
  tol      the tolerance of the numbers: a relative one (``1e-3``), an
           absolute one (``abs:0.01``), or the name of a tolerance of the
           manifest line (``best_loss``), which takes that line's value.

The marker resolves against the commands of each manifest line it names
(the page of the line), whatever page holds the quote; the guide pages
quote runs of the tutorial and the examples. check_nfp_run.coverage()
checks every marker without running anything; the run of the line checks
the quote (match_quote()).

The check reads the source and the quote line by line, after these
normalisations of both (normalise_output()), and no other:

  * a date ``YYYY-MM-DD`` is ``<date>``, a clock time ``H:MM:SS`` or
    ``HH:MM:SS`` (with a fraction of a second) is ``<time>``;
  * a duration, a number followed by ``ms``, ``us``, ``s``, ``min`` or
    ``h`` (with or without a blank between), or minutes and seconds as the
    journal writes them, with hours when there are any (``1m 5s``,
    ``1h 2m 3s``), is ``<duration>``, and a rate
    of evaluations, a number followed by `` evals/s``, ``/min`` or ``/h``,
    is ``<rate>``;
  * blanks at the start and the end of a line are left out, and a run of
    blanks inside a line is one blank.

A masked duration matches any duration: a time limit that the output
repeats (a timeout, a deadline) is not checked by a quote.

A line of the quote that is ``...`` stands for any number of lines of the
source (none included), unless it is indented four blanks more than the
line before it: then it continues that line (below). The other lines of
the quote are lines of the source in the same order, consecutive between
two ``...`` lines; the quote may start and end anywhere in the source. A
line matches when its text is that of the source line and each number (a
run of digits with an optional sign, decimal point and exponent, not part
of a word: ``RCUT2B`` and ``2.0.0`` are text) is that of the source line.
An integer (no decimal point, no exponent) is the same text. Another
number is written as the source writes it (the same digits after the
point, an exponent of as many digits, a leading '+' beside a sign) and its
value is within two units of its last digit (``2.3946e-01``: 2e-05;
``0.49``: 0.02), or within the marker's tol= when that is wider. A number
that the quote and the source both write with FULL_PRECISION (15)
significant digits or more is a double written in full, whose last digits a
rounding changes, and with them their count: it may have another count of
digits after the point (``17.519476792114382`` and ``17.51947679210062``),
and its value is compared within the tol=.

A source line that starts with the clock time of the journal's progress
lines (``[22:56:41]``) also matches the quote without that column: the
quote may start such a line at its level column (``INFO``, ``BEST``,
``WARNING``). No other column may be left out.

An output line longer than 80 characters is quoted wrapped
(wrap_output()): broken at blanks into lines of at most 80 characters,
each continuation line indented four blanks more than the first line of
the output line. The check joins a continuation line to the line before
it with one blank; it also tries the lines unjoined, so that an output
line that is itself indented four blanks more is quoted as it is. A word
longer than a line is not broken: such a line stays longer than 80
characters, which the width check of the pages refuses, so the quote
leaves it out with ``...``.
"""
import csv
import difflib
import math
import os
import re
import shlex
import textwrap
from collections import namedtuple

# --varloss variable (as the scripts under nfp/files/lammps name it) ->
# quantity of dft_reference.csv.
VARIABLE_QUANTITY = {
    'Ecoh': 'Ecoh',
    'a0': 'a0',
    # vacancy.in builds its cell at the DFT a0, which the target file gives
    # with --invar a0_dft (a0 is the --hotvar that ecoh_a0.in relaxes).
    'a0_dft': 'a0',
    'Evf': 'Evf',
    'C11': 'C11',
    'C12': 'C12',
    'C44': 'C44',
}

# PC_OBSERVABLE observable -> quantity of dft_reference.csv.
OBSERVABLE_QUANTITY = {
    'cohesive_energy': 'Ecoh',
}

# The unit of a search parameter, by its label in hpo_search_space.tsv: a
# key of one value has no index, a key of several values the index of the
# value (FSIGMA[1] when the training configuration gives both parts of
# FSIGMA, FSIGMA when it gives one). The error scales of the fit: the
# absolute part of FSIGMA in eV/Å, that of SSIGMA in GPa (their relative
# parts, FSIGMA[2] and SSIGMA[2], are ratios); ESIGMA: ESIGMA_UNITS.
PARAMETER_UNITS = {
    'RCUT2B': 'Å',
    'RCUTMB': 'Å',
    'FSIGMA': 'eV/Å',
    'FSIGMA[1]': 'eV/Å',
    'SSIGMA': 'GPa',
    'SSIGMA[1]': 'GPa',
}

# ENORM of the training configuration -> unit of ESIGMA: the fit divides
# the energy residual of a structure of N atoms by N (ATOM, the default)
# or by sqrt(N) (SQRT).
ESIGMA_UNITS = {
    'ATOM': 'meV/atom',
    'SQRT': 'meV/√atom',
}

# (descriptor, key) -> unit of a key whose unit depends on the descriptor:
# the units of the descriptor's metadata in MODELS (ParamMeta::units in
# src/d2_blip.cpp and, for DM_mBlipEnv, src/dm_all.cpp).
DESCRIPTOR_UNITS = {
    ('D2_Blip', 'CGRID2B'): 'Å',
    ('D2_Blip', 'SGRID2B'): 'Å^-1',       # a blip spans 4/eta
    ('DM_mBlipEnv', 'CGRIDMB'): 'Å',
    ('DM_mBlipEnv', 'SGRIDMB'): 'Å^-2',   # the density falls as exp(-s r^2)
}

# Descriptors whose values have no unit: a coefficient of the linear model
# (FIXWEIGHT) multiplies one of them into an energy, in eV.
UNITLESS_DESCRIPTORS = ('D2_Blip', 'DM_mBlipEnv')

# outvar.tadah columns of the validation errors (PC_ERMSE, PC_FRMSE,
# PC_SRMSE_GPA), in the units tadah hpo writes them in.
OUTVAR_UNITS = {
    'ERMSE': 'meV/atom',
    'FRMSE': 'eV/Å',
    'SRMSE_GPA': 'GPa',
}

Reference = namedtuple('Reference', 'value unit line')
Line = namedtuple('Line', 'number words')


class FilesError(Exception):
    pass


def read_dft_reference(path):
    '''{quantity: Reference(value, unit, line)} of dft_reference.csv.'''
    with open(path, encoding='utf-8', newline='') as fh:
        rows = list(csv.reader(fh))
    if not rows or [c.strip() for c in rows[0][:3]] != ['quantity', 'value',
                                                         'unit']:
        raise FilesError(f'{path}: the header does not start with '
                         '"quantity,value,unit"')
    reference = {}
    for number, row in enumerate(rows[1:], 2):
        if not row or not any(c.strip() for c in row):
            continue
        if len(row) < 3:
            raise FilesError(f'{path}:{number}: fewer than 3 columns')
        name = row[0].strip()
        if name in reference:
            raise FilesError(f'{path}:{number}: quantity "{name}" is also on '
                             f'line {reference[name].line}')
        try:
            value = float(row[1])
        except ValueError:
            value = math.nan
        if not math.isfinite(value):
            raise FilesError(f'{path}:{number}: the value of "{name}" is not a '
                             f'number: {row[1]}')
        reference[name] = Reference(value, row[2].strip(), number)
    return reference


def target_lines(path):
    '''The lines of the target file *path* as lists of words: comments
    (from ``#``) removed, a line ending in ``\\`` joined with the next,
    blank lines left out. Each Line has the number of its first line.'''
    with open(path, encoding='utf-8') as fh:
        text = fh.read().splitlines()
    lines, pending, start = [], '', None
    for number, raw in enumerate(text, 1):
        body = raw.split('#', 1)[0].rstrip()
        if start is None:
            start = number
        if body.endswith('\\'):
            pending += body[:-1] + ' '
            continue
        words = shlex.split(pending + body, posix=True)
        if words:
            lines.append(Line(start, words))
        pending, start = '', None
    if pending.strip():
        lines.append(Line(start, shlex.split(pending, posix=True)))
    return lines


DftTarget = namedtuple('DftTarget', 'line where quantity value')


def _is_option(word):
    '''Whether *word* is an option of a PC_* line, not a value: it starts
    with '-' and is not a number.'''
    if not word.startswith('-'):
        return False
    try:
        float(word)
    except ValueError:
        return True
    return False


def dft_targets(path):
    '''The values of the target file *path* that are DFT values: one
    DftTarget(line, where, quantity, value) per ``--varloss`` of a
    PC_LAMMPS or PC_FAILPROBE line, per initial value of a ``--hotvar`` and
    per ``--invar`` there whose variable VARIABLE_QUANTITY maps, and per
    ``--target`` of a PC_OBSERVABLE line. A --varloss variable or an
    observable without a quantity raises FilesError.'''
    found = []
    for line in target_lines(path):
        key = line.words[0].upper()
        words = line.words
        if key in ('PC_LAMMPS', 'PC_FAILPROBE'):
            for i, w in enumerate(words):
                if w == '--hotvar' and i + 4 < len(words):
                    name, initial = words[i + 1], words[i + 4]
                    if name in VARIABLE_QUANTITY and not _is_option(initial):
                        found.append(DftTarget(
                            line.number, f'--hotvar {name} (initial value)',
                            VARIABLE_QUANTITY[name],
                            _number(path, line, initial)))
                    continue
                if w == '--invar' and i + 2 < len(words):
                    name = words[i + 1]
                    if name in VARIABLE_QUANTITY:
                        found.append(DftTarget(
                            line.number, f'--invar {name}',
                            VARIABLE_QUANTITY[name],
                            _number(path, line, words[i + 2])))
                    continue
                if w != '--varloss':
                    continue
                if i + 2 >= len(words):
                    raise FilesError(f'{path}:{line.number}: --varloss without '
                                     '<variable> <target>')
                name, value = words[i + 1], words[i + 2]
                if name not in VARIABLE_QUANTITY:
                    raise FilesError(
                        f'{path}:{line.number}: --varloss {name}: no quantity '
                        'of dft_reference.csv is mapped to the variable '
                        f'"{name}" (VARIABLE_QUANTITY of {__file__})')
                found.append(DftTarget(line.number, f'--varloss {name}',
                                       VARIABLE_QUANTITY[name],
                                       _number(path, line, value)))
        elif key == 'PC_OBSERVABLE':
            observable = words[1] if len(words) > 1 else ''
            for i, w in enumerate(words):
                if w != '--target':
                    continue
                if observable not in OBSERVABLE_QUANTITY:
                    raise FilesError(
                        f'{path}:{line.number}: PC_OBSERVABLE {observable} '
                        '--target: no quantity of dft_reference.csv is mapped '
                        'to the observable (OBSERVABLE_QUANTITY of '
                        f'{__file__})')
                if i + 1 >= len(words):
                    raise FilesError(f'{path}:{line.number}: --target without '
                                     'a value')
                found.append(DftTarget(line.number,
                                       f'PC_OBSERVABLE {observable} --target',
                                       OBSERVABLE_QUANTITY[observable],
                                       _number(path, line, words[i + 1])))
    return found


def warm_seeds(path):
    '''[(line, file)] of the lines ``METHOD WARM <file>`` of the target
    file *path* (in an INIT or a RESTART block): the potential a search
    starts from, read relative to the directory tadah hpo runs in.'''
    return [(line.number, line.words[2]) for line in target_lines(path)
            if len(line.words) >= 3 and line.words[0].upper() == 'METHOD'
            and line.words[1].upper() == 'WARM']


# The blocks of a target file. OPTIMIZER, STRATEGY and REFINE set one
# optimiser, the STRATEGY-level one, whose THREADS sets the workers of the
# run; the THREADS of an EXPLORE block sets only how many evaluations its
# exploration asks for at once.
BLOCKS = ('OPTIMIZER', 'STRATEGY', 'INIT', 'NOISE', 'EXPLORE', 'REFINE',
          'INNER', 'RESTART')
RUN_OPTIMISER_BLOCKS = ('OPTIMIZER', 'STRATEGY', 'REFINE')


def run_threads(path):
    '''(line, [line]) of the target file *path*: the line of its first
    OPTIMIZER block (None without one: the file runs no search) and the
    lines of the THREADS keys that set the workers of the run, those of the
    blocks OPTIMIZER, STRATEGY and REFINE.'''
    opened, found, stack = None, [], []
    for line in target_lines(path):
        key = line.words[0].upper()
        if key in BLOCKS and len(line.words) == 1:
            stack.append(key)
            if key == 'OPTIMIZER' and opened is None:
                opened = line.number
        elif key.startswith('END') and key[3:] in BLOCKS:
            if stack and stack[-1] == key[3:]:
                stack.pop()
        elif key == 'THREADS' and stack and stack[-1] in RUN_OPTIMISER_BLOCKS:
            found.append(line.number)
    return opened, found


def _number(path, line, text):
    try:
        value = float(text)
    except ValueError:
        value = math.nan
    if not math.isfinite(value):
        raise FilesError(f'{path}:{line.number}: "{text}" is not a number')
    return value


# The keys of a training configuration that units depend on.
UNIT_KEYS = ('TYPE2B', 'TYPEMB', 'ENORM')


def configuration(path, cwd):
    '''{key: first value} of the keys UNIT_KEYS that the training
    configuration *path* sets. A ``CONFIG <file>`` line includes *file*
    (relative to *cwd*, the directory tadah runs in) in its place, as tadah
    reads it (which refuses a second line of a key of one value).'''
    found = {}
    with open(path, encoding='utf-8') as fh:
        for raw in fh:
            words = raw.split('#', 1)[0].split()
            if len(words) < 2:
                continue
            key = words[0].upper()
            if key == 'CONFIG':
                found.update(configuration(os.path.join(cwd, words[1]), cwd))
            elif key in UNIT_KEYS:
                found[key] = words[1]
    return found


def coefficient_unit(label, found):
    '''The unit of a coefficient of the linear model (a FIXWEIGHT value, a
    value of WEIGHTS), named *label* in a message, of a training
    configuration that sets the keys *found* (configuration()): eV when
    every descriptor it names has values without a unit. Raises FilesError
    otherwise.'''
    descriptors = {k[4:]: v for k, v in found.items() if k.startswith('TYPE')}
    if not descriptors:
        raise FilesError(f'no unit is known for {label}: the training '
                         'configuration names no descriptor (TYPE2B, '
                         'TYPEMB)')
    other = sorted(d for d in descriptors.values()
                   if d not in UNITLESS_DESCRIPTORS)
    if other:
        raise FilesError(f'no unit is known for {label}: the descriptor '
                         f'{other[0]} is not one whose values have no unit '
                         '(nfp_files.UNITLESS_DESCRIPTORS)')
    return 'eV'


def parameter_unit(label, found):
    '''(unit, note) of the search parameter *label* (RCUT2B, SGRIDMB,
    CGRID2B[3], FIXWEIGHT[26], ESIGMA, FSIGMA[1]) of a training
    configuration that sets the keys *found* (configuration()); note names
    the descriptor or the ENORM the unit depends on. Raises FilesError when
    no unit is known.'''
    key = label.split('[', 1)[0]
    if label in PARAMETER_UNITS:
        return PARAMETER_UNITS[label], ''
    if label == 'ESIGMA':
        enorm = found.get('ENORM', 'ATOM').upper()
        if enorm not in ESIGMA_UNITS:
            raise FilesError(f'no unit is known for ESIGMA under ENORM {enorm} '
                             '(nfp_files.ESIGMA_UNITS)')
        return ESIGMA_UNITS[enorm], f' (ENORM {enorm})'
    descriptors = {k[4:]: v for k, v in found.items() if k.startswith('TYPE')}
    if key == 'FIXWEIGHT':
        return coefficient_unit(label, found), ''
    body = key[-2:]
    if key[:-2] in ('CGRID', 'SGRID') and body in ('2B', 'MB'):
        if body not in descriptors:
            raise FilesError(f'no unit is known for {key}: the training '
                             f'configuration has no TYPE{body}')
        unit = DESCRIPTOR_UNITS.get((descriptors[body], key))
        if unit is None:
            raise FilesError(f'no unit is known for {key} of {descriptors[body]} '
                             '(nfp_files.DESCRIPTOR_UNITS)')
        return unit, f' ({descriptors[body]})'
    raise FilesError(f'no unit is known for {label} (nfp_files.PARAMETER_UNITS)')


def outvar_unit(name, reference):
    '''(unit, note) of the outvar.tadah column *name*: its DFT quantity's
    unit in *reference* (read_dft_reference()), or OUTVAR_UNITS. Raises
    FilesError when no unit is known.'''
    quantity = VARIABLE_QUANTITY.get(name)
    if quantity is not None and quantity in reference:
        return reference[quantity].unit, ' (dft_reference.csv)'
    if name in OUTVAR_UNITS:
        return OUTVAR_UNITS[name], ''
    raise FilesError(f'no unit is known for the outvar.tadah column {name} '
                     '(dft_reference.csv, nfp_files.OUTVAR_UNITS)')



# --------------------------------------------------------------------------
# Quotes of program output
# --------------------------------------------------------------------------

# The files of a search that a quote may name, which the run keeps before
# the next command writes over them (check_nfp_run.SNAPSHOT).
SEARCH_FILES = ('hpo_run.log', 'best_pot.tadah', 'best_loss.tadah',
                'best_params.tadah', 'best_outvar.tadah', 'loss.tadah',
                'params.tadah', 'outvar.tadah', 'hpo_search_space.tsv')
STREAMS = ('stdout', 'stderr')
MARKER_OPTIONS = ('search', 'command', 'tol')
MARKER_ID = re.compile(r'[A-Za-z0-9_]+')
MARKER = re.compile(r'^(\s*)\.\. output:(.*)$')
TEXT_BLOCK = re.compile(r'^(\s*)\.\. code-block::\s*text\s*$')
OPTION_LINE = re.compile(r'^\s*:[\w-]+:')
TOLERANCE_SPEC = re.compile(r'(abs:)?([0-9.eE+-]+)|([a-z0-9_.\[\]]+)')
#: The longest line of a quote, and the indent of a continuation line.
WRAP_WIDTH = 80
CONTINUATION = '    '
ELLIPSIS = '...'

Marker = namedtuple('Marker', 'ids source search command tol')
# A text code block of a page and its marker: path, the line of the marker
# (None: no marker) and of the block's directive (None: no block), the
# Marker (None when it has an error), the text of the marker, the lines of
# the block (dedented), the error of the marker (None: none).
Quote = namedtuple('Quote', 'path marker_line line marker text lines error')


def parse_marker(text):
    """The Marker of the text after ``.. output:``; FilesError when it is
    outside the grammar (see the docstring)."""
    try:
        words = shlex.split(text)
    except ValueError as err:
        raise FilesError(f'the output marker "{text.strip()}" cannot be read: '
                         f'{err}') from None
    if len(words) < 2:
        raise FilesError('an output marker names <id> and <source>: '
                         '".. output: <ids> <source> [search=<search>] '
                         '[command=<n>] [tol=<tolerance>]"')
    ids = tuple(words[0].split(','))
    for i in ids:
        if not MARKER_ID.fullmatch(i):
            raise FilesError(f'the id "{i}" is not letters, digits and _ (a '
                             'manifest id; several separated by ",")')
    source = words[1]
    if source not in STREAMS + SEARCH_FILES:
        raise FilesError(f'the source "{source}" is not stdout, stderr or a '
                         f'file of a search ({", ".join(SEARCH_FILES)})')
    options = {}
    for word in words[2:]:
        name, eq, value = word.partition('=')
        if not eq:
            raise FilesError(f'"{word}" is not <option>=<value> (options: '
                             f'{", ".join(MARKER_OPTIONS)})')
        if name not in MARKER_OPTIONS:
            raise FilesError(f'unknown option "{name}" (options: '
                             f'{", ".join(MARKER_OPTIONS)})')
        if name in options:
            raise FilesError(f'option "{name}" given twice')
        options[name] = value
    command = options.get('command')
    if command is not None:
        if not re.fullmatch(r'[1-9][0-9]*', command):
            raise FilesError(f'command={command} is not a whole number from 1 '
                             '(the n-th command of the page)')
        command = int(command)
        if source not in STREAMS:
            raise FilesError('command= names a command, whose source is '
                             f'stdout or stderr, not {source} (a file of a '
                             'search: name the search with search=)')
        if 'search' in options:
            raise FilesError('command= and search= both name the command: '
                             'give one')
    tol = options.get('tol')
    if tol is not None:
        tolerance_of(tol, {})   # raises FilesError outside the grammar
    if options.get('search') == '':
        raise FilesError('search= names no search')
    return Marker(ids, source, options.get('search'), command, tol)


def tolerance_of(text, tolerances):
    """(value, absolute) of the tol= *text*: a number, ``abs:<number>``,
    or the name of a tolerance in *tolerances* ({name: (value, absolute)});
    None for a name *tolerances* does not hold. FilesError outside the
    grammar."""
    m = TOLERANCE_SPEC.fullmatch(text)
    if m and m.group(2) is not None:
        try:
            x = float(m.group(2))
        except ValueError:
            x = math.nan
        if math.isfinite(x) and x >= 0:
            return (x, bool(m.group(1)))
    elif m and m.group(3) is not None and not m.group(1):
        if text in tolerances:
            t = tolerances[text]
            return (t[0], t[1])
        return None
    raise FilesError(f'tol={text} is not a relative tolerance (1e-3), an '
                     'absolute one (abs:0.01) or the name of a tolerance of '
                     'the manifest line (best_loss)')


def _indent(line):
    return len(line) - len(line.lstrip())


def _block_lines(lines, i, indent):
    """(dedented content, index after the block) of the directive whose
    options start at lines[i], the directive indented by *indent*."""
    while (i < len(lines) and lines[i].strip() and _indent(lines[i]) > indent
           and OPTION_LINE.match(lines[i])):
        i += 1
    content = []
    while i < len(lines):
        line = lines[i]
        if line.strip() and _indent(line) <= indent:
            break
        content.append(line.rstrip())
        i += 1
    while content and not content[0].strip():
        content.pop(0)
    while content and not content[-1].strip():
        content.pop()
    width = min((_indent(l) for l in content if l.strip()), default=0)
    return [l[width:] for l in content], i


def text_blocks(path):
    """The lines of the ``.. code-block:: text`` directives of the rst file
    *path* (the line Sphinx gives the block), marked or not."""
    with open(path, encoding='utf-8') as fh:
        return [n for n, line in enumerate(fh.read().splitlines(), 1)
                if TEXT_BLOCK.match(line)]


def quote_blocks(path):
    """The output markers of the rst file *path*, each a Quote with the
    text code block it precedes."""
    with open(path, encoding='utf-8') as fh:
        lines = fh.read().splitlines()
    quotes = []
    for i, line in enumerate(lines):
        m = MARKER.match(line)
        if not m:
            continue
        indent, text = len(m.group(1)), m.group(2)
        marker, error = None, None
        try:
            marker = parse_marker(text)
        except FilesError as err:
            error = str(err)
        j = i + 1
        if (j < len(lines) and lines[j].strip()
                and _indent(lines[j]) > indent):
            quotes.append(Quote(path, i + 1, None, None, text, [],
                                'an output marker is one line: the line after '
                                'it is part of the comment (no block follows '
                                'it)'))
            continue
        while j < len(lines) and not lines[j].strip():
            j += 1
        following = lines[j] if j < len(lines) else '(the end of the file)'
        b = TEXT_BLOCK.match(following)
        if not b or len(b.group(1)) != indent:
            quotes.append(Quote(path, i + 1, None, None, text, [],
                                f'an output marker is followed by '
                                f'"{following.strip()}", not by ".. '
                                'code-block:: text"'))
            continue
        content, _ = _block_lines(lines, j + 1, indent)
        quotes.append(Quote(path, i + 1, j + 1, marker, text, content, error))
    return quotes


# ------------------------------------------------------------ the match ----

_DATE = re.compile(r'\b\d{4}-\d{2}-\d{2}\b')
_TIME = re.compile(r'\b\d{1,2}:\d{2}:\d{2}(?:\.\d+)?\b')
_NUMBER_TEXT = r'\d+(?:\.\d+)?(?:[eE][-+]?\d+)?'
_RATE = re.compile(rf'(?<![\w.]){_NUMBER_TEXT} evals/(?:s|min|h)\b')
_DURATION = re.compile(rf'(?<![\w.]){_NUMBER_TEXT} ?(?:ms|us|s|min|h)\b')
# A duration of the journal in minutes and seconds, with hours when there
# are any ("1m 5s", "1h 2m 3s"; HPOLog::format_duration and fmt_millis of
# HPO's hpo_log.cpp): one duration, masked before its seconds are.
_MINUTES = re.compile(r'(?<![\w.])(?:\d+h )?\d+m \d+(?:\.\d+)?s\b')
_BLANKS = re.compile(r'\s+')
_NUMBER = re.compile(r'(?<![\w.])[-+]?(?:\d+\.\d*|\.\d+|\d+)(?:[eE][-+]?\d+)?'
                     r'(?!\w|\.\d)')
_TIME_COLUMN = '[<time>] '
# A number of a line (the docstring, the match): check_nfp_run reads the
# numbers of a quote with it.
NUMBER = _NUMBER


def normalise_output(line):
    """*line* with its dates, clock times, durations and rates masked and
    its blanks normalised (see the docstring)."""
    line = _DATE.sub('<date>', line)
    line = _TIME.sub('<time>', line)
    line = _RATE.sub('<rate>', line)
    line = _MINUTES.sub('<duration>', line)
    line = _DURATION.sub('<duration>', line)
    return _BLANKS.sub(' ', line.strip())


def _form(number):
    """How *number* is written, apart from its value: the digits after the
    decimal point (None: no point) and of the exponent (None: none)."""
    mantissa, e, exponent = number.lower().partition('e')
    decimals = len(mantissa.split('.', 1)[1]) if '.' in mantissa else None
    return decimals, (len(exponent.lstrip('+-')) if e else None)


# A number written with this many significant digits or more is a double
# written in full (the shortest text that reads back as it): its last digits
# are rounding, and their count changes with them.
FULL_PRECISION = 15


def _significant_digits(number):
    """The significant digits *number* writes."""
    mantissa = number.lower().partition('e')[0].lstrip('+-').replace('.', '')
    return len(mantissa.lstrip('0'))


def _written_alike(q, s):
    """Whether the numbers *q* and *s* are written in one form: the same
    digits after the point and of the exponent; for two numbers written in
    full precision (FULL_PRECISION significant digits or more on both
    sides), the same notation, whatever the digits after the point."""
    fq, fs = _form(q), _form(s)
    if fq == fs:
        return True
    return (fq[1] == fs[1] and fq[0] is not None and fs[0] is not None
            and min(_significant_digits(q), _significant_digits(s))
            >= FULL_PRECISION)


def _sign_written_alike(q, s):
    """Whether the numbers *q* and *s* are signed alike: a '+' on one of
    them only beside a '+' or '-' on the other (a format that signs every
    value; a '-' comes with the value)."""
    signs = {n[:1] if n[:1] in ('+', '-') else '' for n in (q, s)}
    return signs != {'+', ''}


def _last_unit(number):
    """The value of one unit of the last digit *number* writes."""
    mantissa, _, exponent = number.lower().partition('e')
    decimals = len(mantissa.split('.', 1)[1]) if '.' in mantissa else 0
    return 10.0 ** ((int(exponent) if exponent else 0) - decimals)


def _same_line(quoted, source, tolerance):
    """Whether the normalised *quoted* line matches the normalised *source*
    line: the same text, and each number as the docstring says."""
    q_text, s_text = _NUMBER.split(quoted), _NUMBER.split(source)
    if q_text != s_text:
        return False
    for q, s in zip(_NUMBER.findall(quoted), _NUMBER.findall(source)):
        if not re.search(r'[.eE]', q):
            if q != s or re.search(r'[.eE]', s):
                return False
            continue
        if not _written_alike(q, s) or not _sign_written_alike(q, s):
            return False
        x, y = float(q), float(s)
        limit = 2 * _last_unit(q)
        if tolerance is not None:
            value, absolute = tolerance[0], tolerance[1]
            limit = max(limit, value if absolute else value * abs(x))
        if abs(x - y) > limit * (1 + 1e-9):
            return False
    return True


def _matches(quoted, source, tolerance):
    if _same_line(quoted, source, tolerance):
        return True
    return (source.startswith(_TIME_COLUMN) and not
            quoted.startswith(_TIME_COLUMN[:-1])
            and _same_line(quoted, source[len(_TIME_COLUMN):], tolerance))


def _items(lines):
    """The lines of a quote as segments (split at ``...`` lines) of items:
    each item (the index of its first line, its lines), a line and the
    lines after it indented four blanks more (its continuations)."""
    segments, current = [], []
    i = 0
    while i < len(lines):
        if lines[i].strip() == ELLIPSIS:
            segments.append(current)
            current = []
            i += 1
            continue
        start = i
        base = _indent(lines[i])
        parts = [lines[i]]
        i += 1
        # A continuation, even one that is "...": an output line broken
        # before a last word "..." (wrap_output()).
        while (i < len(lines) and lines[i].strip()
               and _indent(lines[i]) == base + len(CONTINUATION)):
            parts.append(lines[i])
            i += 1
        current.append((start, parts))
    segments.append(current)
    return segments


def _match_segments_any(segments, out, tolerance):
    """Whether the *segments* of a quote (_items()) are lines of *out* in
    order: the first may start anywhere, each next one anywhere after the
    one before it (a "..." line separates them)."""
    # A segment ending earlier is never worse: whatever the segments after
    # it match from a line on, they also match from any line before it (a
    # "..." line stands for any number of lines). So each segment takes
    # its earliest end, and no order of the lines is tried twice.
    j = 0
    for items in segments:
        if not items:
            continue
        best = None
        for p in range(j, len(out)):
            if best is not None and p + 1 >= best:
                break
            end = _earliest_end(items, out, p, tolerance)
            if end is not None and (best is None or end < best):
                best = end
        if best is None:
            return False
        j = best
    return True


def _earliest_end(items, out, j, tolerance):
    """The smallest output index after the *items* of a segment matched
    from out[j] on, or None. An item matches its lines joined (longest
    first), or fewer of them joined, the rest read as an item of its own
    (an output line indented four blanks more than the line before it)."""
    memo = {}

    def end(k, first, at):
        # Items k on, item k from its line *first* on, from out[at] on.
        if k == len(items):
            return at
        key = (k, first, at)
        if key in memo:
            return memo[key]
        parts = items[k][1]
        found = None
        for n in range(len(parts), first, -1):
            if at >= len(out):
                break
            joined = normalise_output(' '.join(p.strip()
                                               for p in parts[first:n]))
            if not _matches(joined, out[at], tolerance):
                continue
            e = (end(k, n, at + 1) if n < len(parts)
                 else end(k + 1, 0, at + 1))
            if e is not None and (found is None or e < found):
                found = e
        memo[key] = found
        return found
    return end(0, 0, j)


def match_quote(lines, output, tolerance=None):
    """None when the quote *lines* (as the page writes them, dedented) is
    *output* (text) by the rules of the docstring, else why not: the first
    line of the quote that no order of the source lines matches, and the
    output line closest to it. *tolerance* is the marker's tol=, (value,
    absolute), or None."""
    raw = output.splitlines()
    out = [normalise_output(l) for l in raw]
    segments = _items(lines)
    if any(segments) and _match_segments_any(segments, out, tolerance):
        return None
    # The first item whose quote, with every item before it, has no place.
    flat = [(s, k) for s, seg in enumerate(segments) for k in range(len(seg))]
    failing = None
    for s, k in flat:
        prefix = [seg[:] for seg in segments[:s]] + [segments[s][:k + 1]]
        if not _match_segments_any(prefix, out, tolerance):
            failing = segments[s][k]
            break
    if failing is None:     # an empty quote, or one of "..." lines only
        return 'the quote holds no line of the output'
    start, parts = failing
    shown = normalise_output(' '.join(p.strip() for p in parts))
    plain = shown[len(_TIME_COLUMN):] if shown.startswith(_TIME_COLUMN) \
        else shown
    best, best_ratio = None, -1.0
    for n, line in enumerate(out):
        candidate = line[len(_TIME_COLUMN):] if line.startswith(
            _TIME_COLUMN) else line
        ratio = difflib.SequenceMatcher(None, plain, candidate).ratio()
        if ratio > best_ratio:
            best, best_ratio = n, ratio
    closest = (f'closest output line {best + 1}: "{raw[best]}"'
               if best is not None else 'the output is empty')
    where = (f'lines {start + 1}-{start + len(parts)}' if len(parts) > 1
             else f'line {start + 1}')
    return (f'{where} of the quote, "{shown}", is not the next line of the '
            f'output in order; {closest}')


def wrap_output(text, width=WRAP_WIDTH):
    """The lines of *text* (program output) as a quote shows them: each
    line longer than *width* broken at blanks into lines of at most
    *width* characters, each continuation indented CONTINUATION more than
    the line it continues (see the docstring)."""
    lines = []
    for line in text.rstrip('\n').split('\n'):
        line = line.rstrip()
        if len(line) <= width:
            lines.append(line)
            continue
        indent = line[:_indent(line)]
        lines += textwrap.wrap(line, width, initial_indent='',
                               subsequent_indent=indent + CONTINUATION,
                               break_long_words=False,
                               break_on_hyphens=False) or ['']
    return lines
