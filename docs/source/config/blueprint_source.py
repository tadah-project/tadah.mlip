'''
Locate CORE's blueprint_tadah.toml -- the single source of truth for the
Tadah! version and the configuration-key reference.

Resolution order:
  1. The CORE checkout next to this repository
     (<workspace>/CORE/blueprint/blueprint_tadah.toml, where <workspace>
     is the directory containing Tadah.MLIP), or $TADAH_PATH/CORE.
  2. On GitLab CI / Read the Docs only: a shallow clone of core.git at the
     branch being built ($CI_COMMIT_REF_NAME, $READTHEDOCS_VERSION_NAME,
     $READTHEDOCS_VERSION).

There is no default: if the blueprint cannot be found the docs build fails
with the list of locations tried.
'''
import os
import re
import subprocess
import tempfile

_HERE = os.path.dirname(os.path.abspath(__file__))
_CORE_URL = 'https://git.ecdf.ed.ac.uk/tadah/core.git'
_REL = os.path.join('blueprint', 'blueprint_tadah.toml')


def _local_candidates():
    # config/ -> source/ -> docs/ -> Tadah.MLIP/ -> workspace
    workspace = os.path.normpath(os.path.join(_HERE, '..', '..', '..', '..'))
    cands = [os.path.join(workspace, 'CORE', _REL),
             os.path.join(workspace, 'core', _REL)]
    tadah_path = os.environ.get('TADAH_PATH')
    if tadah_path:
        cands.append(os.path.join(tadah_path, 'CORE', _REL))
    return cands


def _clone_branch():
    return (os.environ.get('CI_COMMIT_REF_NAME')
            or os.environ.get('READTHEDOCS_VERSION_NAME')
            or os.environ.get('READTHEDOCS_VERSION'))


_cached = None


def locate_blueprint():
    '''Return the path of blueprint_tadah.toml or raise RuntimeError.'''
    global _cached
    if _cached:
        return _cached
    cands = _local_candidates()
    for c in cands:
        if os.path.exists(c):
            _cached = c
            return c
    branch = _clone_branch()
    if branch:
        tmp = tempfile.mkdtemp(prefix='tadah_core_')
        print(f"Cloning {_CORE_URL} (branch {branch}) for the blueprint...")
        subprocess.run(['git', 'clone', '--depth', '1', '--branch', branch,
                        _CORE_URL, tmp], check=True)
        c = os.path.join(tmp, _REL)
        if os.path.exists(c):
            _cached = c
            return c
        cands.append(f"{_CORE_URL}@{branch}:{_REL}")
    raise RuntimeError(
        "blueprint_tadah.toml not found; tried: " + ", ".join(cands) +
        ". Build the docs from a workspace with a CORE checkout next to "
        "Tadah.MLIP, set TADAH_PATH, or build on CI/RTD.")


def read_version(path):
    '''Return [TADAH].version from the blueprint at *path*.'''
    in_tadah = False
    with open(path, 'r', encoding='utf-8') as fh:
        for line in fh:
            if re.match(r'^\s*\[TADAH\]\s*$', line):
                in_tadah = True
                continue
            if in_tadah and re.match(r'^\s*\[', line):
                break
            if in_tadah:
                m = re.match(r'^\s*version\s*=\s*[\'"]([^\'"]+)[\'"]', line)
                if m:
                    return m.group(1)
    raise RuntimeError(f"'version' not found in the [TADAH] table of {path}")
