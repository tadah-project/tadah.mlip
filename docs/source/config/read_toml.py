'''
This script generates documentation in rst format from
the TOML dictionary.

It will generate complete config/config_keys.rst file.

'''
import pytomlpp as p
import os
import shutil
import subprocess

def download_toml_file(repo_url, file_path, output_path):
    try:
        # Resolve the branch:
        #   GitLab CI:   $CI_COMMIT_REF_NAME (e.g. "release/beta-prep")
        #   RTD:         $READTHEDOCS_VERSION_NAME (verbose ref-matching form,
        #                e.g. "release/beta-prep"); $READTHEDOCS_VERSION is the
        #                URL slug ("release-beta-prep") and may not match a
        #                real git ref when the branch contains a slash.
        #   Local:       fall back to "develop".
        branch = (os.environ.get('CI_COMMIT_REF_NAME')
                  or os.environ.get('READTHEDOCS_VERSION_NAME')
                  or os.environ.get('READTHEDOCS_VERSION')
                  or 'develop')

        print(f"Downloading {file_path} from {repo_url} on branch {branch}...")
        # Clone the repository into a temporary directory with the specific branch
        repo_name = "temp_repo"
        subprocess.run(
            [
                'git', 'clone', '--depth', '1',
                '--branch', branch,
                repo_url, repo_name
            ],
            check=True
        )

        # Copy the desired file to the output path
        full_file_path = os.path.join(repo_name, file_path)
        if os.path.exists(full_file_path):
            shutil.copy(full_file_path, output_path)
            print(f"Successfully downloaded {file_path} to {output_path}")
        else:
            print(f"File {file_path} not found in the repository.")

    except subprocess.CalledProcessError as e:
        print(f"Error handling the repository: {e}")
    finally:
        # Clean up
        if os.path.exists(repo_name):
            shutil.rmtree(repo_name)


download_toml_file(
    'https://git.ecdf.ed.ac.uk/tadah/core.git',
    'blueprint/blueprint_tadah.toml',   # source
    'config/blueprint_tadah.toml'       # destination
)
print ("Downloaded blueprint_tadah.toml to config/blueprint_tadah.toml")

# Path to TOML dict
d = p.load("config/blueprint_tadah.toml",'r')

# Path to rst outfile
f = open("config/config_keys.rst", "w")

header = '''
.. This file, including introduction, is auto-generated from the TOML blueprint file. Do not edit it directly.

.. _ConfigSection:

Configuration File
==================

.. note:: This page is auto-generated from the blueprint TOML on every docs build. If you spot a mismatch between this reference and runtime behaviour, please report it on the `Tadah!MLIP GitLab <https://git.ecdf.ed.ac.uk/tadah/tadah.mlip/-/issues>`_.

.. note:: This configuration file is NOT used by the HPO (Hyperparameter Optimization) module of Tadah!MLIP. The HPO module uses a different configuration file format, which is documented in the :ref:`nested_fitting`.

This section describes the format of the configuration file used by Tadah!MLIP.

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

This message indicates that the :ref:`DBFILE` KEY was not specified in the :file:`config.train` file. To resolve this, add the :ref:`DBFILE` key and its corresponding value to :file:`config.train`.

Key Specifics
-------------

The meaning of some keys can vary with the chosen command. Check the documentation for that specific command, model or descriptor to see which keys are required and how they are interpreted. While we strive to keep key meanings consistent across Tadah!MLIP, occasional differences may still occur.

Comments
--------

Use the ``#`` symbol to add comments in the configuration file. Both inline and full-line comments are supported.


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

Value Limits
------------

Each keyword takes a fixed number of values.  Passing the wrong count can raise an error, but enforcement is not yet fully consistent.  While this is being improved, keep these points in mind:

* **Too many values** – Tadah!MLIP may reject the input with a clear message or quietly discard the extras; the latter can later surface as obscure run-time failures (even an occasional segmentation fault).
* **Too few values** – usually triggers an error, although a crash remains possible in rare corner cases.

In short, give every keyword exactly the number of values it expects—no more, no less—to avoid unpleasant surprises.

.. _SupportedKEYS:

Supported KEYS
--------------

This section contains all KEYS currently used by Tadah!MLIP.

'''

f.write(header)

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


# ---------------------------------------------------------------------------
# 1.  Extract the relevant sub-tables from the blueprint
# ---------------------------------------------------------------------------
allowed_keys = d.get('ALLOWED_KEYS', {})
types_table  = d.get('TYPES',        {})

# ---------------------------------------------------------------------------
# 2.  Walk over all KEY definitions and emit documentation
# ---------------------------------------------------------------------------
for key_name, raw_entry in allowed_keys.items():

    # Final field set after resolving possible inheritance
    entry = _resolve_inheritance(raw_entry, types_table)

    # ---------------------------------------------------------------------
    # Start a new labelled section
    # ---------------------------------------------------------------------
    f.write(f".. _{key_name}:\n\n")
    f.write(f"{key_name}\n")
    f.write(f"{'^' * len(key_name)}\n")

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
        f.write(", ".join(joined) + "\n")

    # ---------------------------------------------------------------------
    # Maximum number of values
    # ---------------------------------------------------------------------
    if 'value_N' in entry and entry['value_N']:
        vals = " / ".join(str(abs(v)) for v in entry['value_N'])
        f.write(f"   *Max number of values*: {vals}\n")

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
        f.write(f"   *Default*: {' / '.join(defaults)}\n")

    # ---------------------------------------------------------------------
    # Short description
    # ---------------------------------------------------------------------
    if entry.get('description', ""):
        f.write("\n   *Description*:\n\n")
        # Re-indent every line of the description so wrapped sentences
        # don't break out of the literal block / definition body.
        desc = entry['description'].replace('\n', '\n   ')
        f.write(f"   {desc}\n")

    # ---------------------------------------------------------------------
    # Long description (optional)
    # ---------------------------------------------------------------------
    if entry.get('long_description', ""):
        # Preserve original indentation (strip first newline if present)
        long_desc = entry['long_description'].lstrip("\n")
        for line in long_desc.splitlines():
            f.write(f"   {line}\n")
        f.write("\n")

    # ---------------------------------------------------------------------
    # Examples
    # ---------------------------------------------------------------------
    examples = entry.get('examples', [])
    for idx, example in enumerate(examples, start=1):
        f.write(f"   *Example* {idx}::\n\n")
        # Re-indent every line inside a multi-line example so the
        # literal block stays consistently indented.
        indented = example.replace('\n', '\n       ')
        f.write(f"       {indented}\n\n")

    # Blank line between keys
    f.write("\n")

f.close()
