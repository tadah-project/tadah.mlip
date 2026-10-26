'''
This script generates documentation in rst format from
the TOML dictionary.

It will generate complete config/config_keys.rst file.
Run it from docs/source (conf.py does); importing it has no side effects.

'''
import os
import shutil
import sys

import pytomlpp as p

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from blueprint_source import locate_blueprint
from source_locator import SourceNotFound

header = '''
.. This file, including introduction, is auto-generated from the TOML blueprint file. Do not edit it directly.

.. _ConfigSection:

Configuration File
==================

.. note:: This page is auto-generated from the blueprint TOML on every docs build. Reports of any mismatch between this reference and runtime behaviour are welcome on the `Tadah!MLIP GitLab <https://git.ecdf.ed.ac.uk/tadah/tadah.mlip/-/issues>`_.

This section describes the format of the configuration file used by Tadah!MLIP.

Nested fitting (``tadah hpo``, :ref:`nested_fitting`) reads the same file, given with ``--config``, as the training configuration of every potential it trains.

For example, the configuration file can control the training process, specifying one or more datasets for use during the training stage. It defines cutoff functions and corresponding radii along with the regression model and descriptor choices.

.. important::

   Indexing of items that take *positional indices* (e.g., ``INDEX 1``, CLI flag
   ``--index 1``) starts from **1**, not 0.

Key/Value Pairs
---------------

The primary structure in a configuration file is the KEY/VALUE pair. Each KEY/VALUE pair must be on a separate line, with the KEY appearing first. The KEY is always a string, followed by its VALUE. The format and type of a VALUE depend on the specific KEY.

Common Usage
------------

Typically, only a subset of KEYS is needed to train a model. Tadah!MLIP will use default values for some keys. An error will occur if a required KEY with no default value is missing:

.. code-block:: bash

   [user@host:~] $ tadah train -c config.train
   terminate called after throwing an instance of 'std::runtime_error'
     what():  Key not found: DBFILE
   Aborted (core dumped)

This message indicates that the :ref:`DBFILE` KEY was not specified in the :file:`config.train` file. The :ref:`DBFILE` key with its value, added to :file:`config.train`, resolves it.

Key Specifics
-------------

The meaning of some keys can vary with the chosen command. The documentation of that command, model or descriptor states which keys are required and how they are interpreted. Key meanings are kept consistent across Tadah!MLIP where possible, but occasional differences may still occur.

Comments
--------

The ``#`` symbol starts a comment in the configuration file. Both inline and full-line comments are supported.


Key Values and Formats
----------------------

Some KEYS can have multiple values, specified in one of two ways:

- Single line:

  .. code-block:: bash

     KEY VALUE1 VALUE2 VALUE3

- Multiple lines:

  .. code-block:: bash

     KEY VALUE1
     KEY VALUE2
     KEY VALUE3

Values are separated by blanks (spaces or tabs). A comma or a ``+`` does not
separate them: ``FIXWEIGHT 0.1,0.2`` and ``MPARAMS 1.0+2.0`` are refused,
naming the line, the key and the separator, and the line to write
(``FIXWEIGHT 0.1 0.2``). A word that holds a comma or a ``+`` stays one value
where the key takes a path or an option string (``DBFILE a,b.tadah``), and the
index lists (:ref:`FIXINDEX`, ``--index``) take commas between their entries
(``FIXINDEX 13,14`` is ``FIXINDEX 13 14``). A name holds neither: in the keys
whose values are names (:ref:`ATOM`, :ref:`TYPE2B`, :ref:`TYPEMB`,
:ref:`RCTYPE2B`, :ref:`RCTYPEMB`, :ref:`MODEL`, :ref:`WATOM`,
:ref:`WNOISE`) ``ATOM Ta,Nb`` and ``TYPE2B D2_Blip 4 4 Fe+Fe`` are refused
the same way. A ``+`` is a sign in a number (``+2``, ``1e+5``).

Value Limits
------------

Each key takes a set number of values. A wrong count is not refused for every key:

* **Too many values**: Tadah!MLIP refuses the line with a message, or ignores the extra values without one. Ignored values can cause failures later in the run whose cause is hard to see, in rare cases a segmentation fault.
* **Too few values**: Tadah!MLIP usually refuses the line; in rare cases it crashes.

Every key needs exactly the number of values it expects.

.. _SupportedKEYS:

Supported KEYS
--------------

This section contains all KEYS used by Tadah!MLIP. A key marked
**Deprecated** is still accepted in configuration and potential files so that
older files load: Tadah!MLIP checks its value, ignores it and writes one
warning per key and process to standard error
(``WARNING: Deprecated key MBLOCK is ignored: ...``), however many lines,
files included with CONFIG or reads of the file contain it -- the rule of
every ignored key, :ref:`EMBF0 and RHOFORM without a DM_REAM block
<dm_ream_only_keys>` included. VERBOSE does not silence the warning.
``pair_style tadah`` writes it as a LAMMPS warning on rank 0 instead, in
``log.lammps`` and on the screen, each time ``pair_coeff`` loads such a
potential (``WARNING: pair tadah: potential 'pot.tadah': Deprecated key
MBLOCK is ignored: ...``).

A key marked **Retired** has no effect: its entry gives the reason, what
replaces it and its conversion (:ref:`fit_objective_conversion` lists those
of the keys that weighted the training data of a fit before Tadah! 1.6.0).
A potential that has it still loads
(``tadah predict``, ``pair_style tadah``, ``tadah analysis``, ``tadah
properties``, ``tadah refit --edit``): Tadah!MLIP checks its value, ignores
it and writes one notice per load that lists the retired keys of the file
(``WARNING: Retired keys EWEIGHT (line 8 of old.tadah) and ... are ignored:
...``). Every fit (``tadah train``, the fits of nested fitting, a
program's own training) refuses a configuration that has one, with its
conversion; ``tadah explain <KEY>`` gives the conversion too.

A key marked **Command key** is one that only some commands read: an option
of their command line, such as ``ANALYTICS`` (``-A`` of ``tadah predict``),
or a key of their configuration file, such as ``REFIT_MODE`` (``tadah
refit``). Its entry names the commands, and shows an example line only when
one of them reads a configuration file. A configuration file of another
command refuses its line (:ref:`config_file`). A potential that holds such a
line loads without it, with one warning per key, which goes where the
warning of a deprecated key goes (standard error, or ``log.lammps`` under
``pair_style tadah``).

'''

def _deepcopy(obj):
    """Tiny helper because we cannot import copy in the RTD sandbox."""
    if isinstance(obj, dict):
        return {k: _deepcopy(v) for k, v in obj.items()}
    elif isinstance(obj, list):
        return [_deepcopy(v) for v in obj]
    return obj


def _merge_missing(base: dict, parent: dict) -> dict:
    """
    Return *base* with all keys that are missing in *base* but exist in *parent*.
    base takes precedence.
    """
    merged = _deepcopy(parent)
    merged.update(base)          # base wins
    return merged


def _resolve_inheritance(entry: dict, types_tbl: dict) -> dict:
    """
    If the entry contains
        INHERIT = ["TYPES.SOMETHING", "TYPES.OTHER"]
    merge those TYPE-blocks into *entry*, giving precedence to the fields
    that *entry* already defines.
    """
    if not entry or 'INHERIT' not in entry:
        return entry

    merged = _deepcopy(entry)
    for target in entry.get('INHERIT', []):
        try:
            scope, name = target.split('.', 1)
        except ValueError:
            # malformed entry – ignore
            continue

        if scope != 'TYPES':
            continue
        parent = types_tbl.get(name, {})
        merged = _merge_missing(merged, parent)
    return merged


def commands_reading(blueprint: dict) -> dict:
    """Map each key (upper case) to the commands, as typed ("tadah data
    print"), whose [TASKS.*] table lists it: among its options (an INHERIT
    of ALLOWED_KEYS.<key>, or a key written out) or the keys of its option
    groups, sorted. A command whose table writes the key out as retired
    (an option it refuses with the reason) does not read it, as CORE's
    commands_reading_key has it."""
    readers = {}

    def note(command, keys_table):
        if not isinstance(keys_table, dict):
            return
        names = [ref.split('.', 1)[1] for ref in keys_table.get('INHERIT', [])
                 if ref.startswith('ALLOWED_KEYS.')]
        names += [k for k, v in keys_table.items() if isinstance(v, dict)]
        retired = {k.upper() for k, v in keys_table.items()
                   if isinstance(v, dict) and v.get('retired')}
        for name in names:
            if name.upper() not in retired:
                readers.setdefault(name.upper(), set()).add(command)

    def walk(prefix, table):
        for name, sub in table.items():
            if not isinstance(sub, dict) or name in ('keys', 'groups'):
                continue
            command = prefix + ' ' + name.lower()
            note(command, sub.get('keys'))
            for group in (sub.get('groups') or {}).values():
                if isinstance(group, dict):
                    note(command, group.get('keys'))
            walk(command, sub)

    walk('tadah', blueprint.get('TASKS', {}))
    return {k: sorted(v) for k, v in readers.items()}


def _listed(items):
    """"a", "a and b", "a, b and c"."""
    if len(items) < 2:
        return ''.join(items)
    return ', '.join(items[:-1]) + ' and ' + items[-1]


def render_key(key_name: str, entry: dict, readers=None,
               config_commands=()) -> str:
    """Return the rst section of one ALLOWED_KEYS entry (inheritance resolved).
    @p readers: the commands that read the key (commands_reading()), for a
    key that only commands read (blueprint field `command_only`);
    @p config_commands: the commands that read a configuration file (those
    that read CONFIG, the key of -c)."""
    out = []

    # ---------------------------------------------------------------------
    # Start a new labelled section
    # ---------------------------------------------------------------------
    out.append(f".. _{key_name}:\n\n")
    out.append(f"{key_name}\n")
    out.append(f"{'^' * len(key_name)}\n")

    # ---------------------------------------------------------------------
    # Deprecated key (blueprint field `deprecated`): still accepted in
    # files, warned about once per parse and ignored. It has no default.
    # ---------------------------------------------------------------------
    if entry.get('deprecated', ""):
        reason = entry['deprecated'].strip().replace('\n', '\n   ')
        out.append(f"\n.. warning::\n\n   **Deprecated.** {reason}\n\n")

    # ---------------------------------------------------------------------
    # Retired key (blueprint field `retired`: reason, replacement and
    # conversion): a potential that has it loads, a fit refuses it with the
    # conversion. Its section and label stay, for the pages that cite it.
    # ---------------------------------------------------------------------
    retired = entry.get('retired') or {}
    if retired:
        def one(field):
            return ' '.join(str(retired.get(field, '')).split())
        out.append("\n.. warning::\n\n"
                   f"   **Retired.** {one('reason')}\n\n"
                   f"   Replaced by: {one('replacement')}\n\n"
                   f"   Conversion: {one('conversion')}\n\n")

    # ---------------------------------------------------------------------
    # VALUE TYPE + FORMAT
    # ---------------------------------------------------------------------
    value_types   = entry.get('value_type',   [])
    value_formats = entry.get('value_format', [])

    if value_types and value_formats:
        joined = []
        # zip-longest so that differing lengths are still handled
        for idx in range(max(len(value_types), len(value_formats))):
            vtype = value_types[idx]   if idx < len(value_types)   else ""
            vfmt  = value_formats[idx] if idx < len(value_formats) else ""
            joined.append(f"[*{vtype}*] {vfmt}".strip())
        out.append(", ".join(joined) + "\n")

    # ---------------------------------------------------------------------
    # Maximum number of values
    # ---------------------------------------------------------------------
    if 'value_N' in entry and entry['value_N']:
        vals = " / ".join(str(abs(v)) for v in entry['value_N'])
        out.append(f"   *Max number of values*: {vals}\n")

    # ---------------------------------------------------------------------
    # Written exactly into a potential (blueprint field `exact_in_potential`):
    # a load checks the key's numbers exactly, so CORE writes them exactly
    # whatever NUMERIC is.
    # ---------------------------------------------------------------------
    if entry.get('exact_in_potential'):
        out.append("   *In a potential file*: written exactly, whatever "
                   "NUMERIC is (a load checks it exactly)\n")

    # ---------------------------------------------------------------------
    # A key that only commands read (blueprint field `command_only`, CORE):
    # an option of their command line, their positional argument (a
    # cli_long_flag that does not start with "-") or a key of their
    # configuration file, named as CORE's messages name it
    # (command_key_role). The rule of such a key is on the page the note
    # links to; the note is a paragraph of its own.
    # ---------------------------------------------------------------------
    if entry.get('command_only'):
        commands = _listed(['``%s``' % c for c in (readers or [])])
        flag = entry.get('cli_long_flag', '')
        if not commands:
            role = ('read by no command; every configuration file refuses '
                    'its line')
        elif flag.startswith('-'):
            short = entry.get('cli_short_flag', '')
            role = 'the option ``%s``%s of %s' % (
                flag, ' (``%s``)' % short if short else '', commands)
        elif flag:
            role = 'the argument of %s' % commands
        else:
            role = 'a key of %s' % commands
        out.append("\n   *Command key* (:ref:`config_file`): %s.\n\n" % role)

    # ---------------------------------------------------------------------
    # Allowed values (blueprint field `allowed_values`): the only values of
    # a string key, matched without regard to case; CORE rejects any other
    # at its line.
    # ---------------------------------------------------------------------
    if entry.get('allowed_values'):
        allowed = " / ".join(str(v) for v in entry['allowed_values'])
        out.append(f"   *Allowed values*: {allowed} (any case; any other "
                   "value is an error)\n")

    # ---------------------------------------------------------------------
    # Default value(s)
    # ---------------------------------------------------------------------
    if 'default_value' in entry and entry['default_value']:
        defaults = []
        for v in entry['default_value']:
            if isinstance(v, bool):
                defaults.append(str(v).lower())
            else:
                defaults.append(str(v))
        out.append(f"   *Default*: {' / '.join(defaults)}\n")

    # ---------------------------------------------------------------------
    # Short description
    # ---------------------------------------------------------------------
    if entry.get('description', ""):
        out.append("\n   *Description*:\n\n")
        # Re-indent every line of the description so wrapped sentences
        # don't break out of the literal block / definition body.
        desc = entry['description'].replace('\n', '\n   ')
        out.append(f"   {desc}\n")

    # ---------------------------------------------------------------------
    # Long description (optional)
    # ---------------------------------------------------------------------
    if entry.get('long_description', ""):
        # Preserve original indentation (strip first newline if present)
        long_desc = entry['long_description'].lstrip("\n")
        in_top_level_list = False
        previous_blank = True
        for line in long_desc.splitlines():
            # A blueprint description is plain text shared with CLI help, so
            # authors reasonably write ``Heading:\n- item``.  RST requires a
            # blank line before that top-level list (and after it if the source
            # omitted one); add only those structural blanks while preserving
            # all author-supplied wrapping and nested indentation.
            if line.startswith(("- ", "* ", "+ ")):
                if not in_top_level_list and not previous_blank:
                    out.append("   \n")
                in_top_level_list = True
            elif not line.strip():
                in_top_level_list = False
            elif in_top_level_list and not line[0].isspace():
                out.append("   \n")
                in_top_level_list = False
            out.append(f"   {line}\n")
            previous_blank = not line.strip()
        out.append("\n")

    # ---------------------------------------------------------------------
    # Examples: each is a value of the key (CORE's rule for the blueprint),
    # one of several lines one value per line; each line is shown as a line
    # of the key, as CoreUserConfigParser lists the examples of a refused
    # value. A command key that no command reading a configuration file
    # reads has no such line (every configuration file refuses it): its
    # entry shows no example, and the reference of the command-line options
    # gives its examples.
    # ---------------------------------------------------------------------
    examples = entry.get('examples', [])
    if entry.get('command_only') and not any(
            c in config_commands for c in (readers or [])):
        examples = []
    for idx, example in enumerate(examples, start=1):
        out.append(f"   *Example* {idx}::\n\n")
        for line in example.splitlines():
            if line.strip():
                out.append(f"       {key_name}   {line.strip()}\n")
        out.append("\n")

    # Blank line between keys
    out.append("\n")
    return "".join(out)


def render(blueprint: dict) -> str:
    """Return the whole config_keys.rst page for a parsed blueprint."""
    allowed_keys = blueprint.get('ALLOWED_KEYS', {})
    types_table = blueprint.get('TYPES', {})
    readers = commands_reading(blueprint)
    config_commands = set(readers.get('CONFIG', []))
    parts = [header]
    for key_name, raw_entry in allowed_keys.items():
        parts.append(render_key(key_name,
                                _resolve_inheritance(raw_entry, types_table),
                                readers.get(key_name.upper(), []),
                                config_commands))
    return "".join(parts)


def main():
    # Copy CORE's blueprint next to the generated pages (generate_cli.py
    # reads it).
    try:
        src = locate_blueprint()
    except SourceNotFound as err:
        sys.exit(f"read_toml.py: {err}")
    shutil.copy(src, "config/blueprint_tadah.toml")
    print(f"Using blueprint {src}")
    d = p.load("config/blueprint_tadah.toml", 'r')
    with open("config/config_keys.rst", "w") as f:
        f.write(render(d))


if __name__ == '__main__':
    main()
