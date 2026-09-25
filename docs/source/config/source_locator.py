'''
Locate the external inputs of the Sphinx docs build.

The generated pages read files from two sibling repositories -- CORE's
blueprint (version, configuration keys, CLI) and HPO's ANNEAL key registry
-- and the API pages read Doxygen XML.  Every input is resolved explicitly;
nothing falls back to a default branch, and a failure lists every location
that was tried.

Source checkouts (``locate``), in this order:

  1. ``$TADAH_CORE_SOURCE_DIR`` / ``$TADAH_HPO_SOURCE_DIR``.  When the
     variable is set it must name a checkout that contains the needed file;
     otherwise the build fails without looking anywhere else.
  2. A sibling checkout: ``<workspace>/CORE`` or ``<workspace>/core``, where
     ``<workspace>`` is the directory that contains this Tadah.MLIP checkout.
  3. ``$TADAH_PATH/CORE`` or ``$TADAH_PATH/core``.
  4. On GitLab CI and Read the Docs only: a shallow clone of the branch or
     tag being built, named the way ``tadah_get_git_branch`` (cmake-tools)
     names it for FetchContent -- ``$CI_COMMIT_REF_NAME``, else
     ``$READTHEDOCS_GIT_IDENTIFIER`` (the ref Read the Docs checked out; its
     ``latest`` and ``stable`` versions are named after no ref), else
     ``$READTHEDOCS_VERSION_NAME``, else ``$READTHEDOCS_VERSION``.  As
     there, a variable counts only when it is set and not empty.

Doxygen XML (``doxygen_xml_dir``): ``$TADAH_DOXYGEN_XML_DIR`` when set (it
must contain ``index.xml``), else the ``build/docs/xml`` directory that
``cmake --build build --target docs`` writes in this checkout.
'''
import os
import subprocess
import tempfile

_HERE = os.path.dirname(os.path.abspath(__file__))

# config/ -> source/ -> docs/ -> Tadah.MLIP/ ; its parent is the workspace.
CHECKOUT_ROOT = os.path.normpath(os.path.join(_HERE, '..', '..', '..'))
WORKSPACE = os.path.dirname(CHECKOUT_ROOT)
DEFAULT_DOXYGEN_XML_DIR = os.path.join(CHECKOUT_ROOT, 'build', 'docs', 'xml')

# Repository -> (override variable, clone URL, file that identifies a usable
# checkout).  The marker is the file the docs generators read from it.
REPOS = {
    'CORE': ('TADAH_CORE_SOURCE_DIR',
             'https://git.ecdf.ed.ac.uk/tadah/core.git',
             os.path.join('blueprint', 'blueprint_tadah.toml')),
    'HPO': ('TADAH_HPO_SOURCE_DIR',
            'https://git.ecdf.ed.ac.uk/tadah/hpo.git',
            os.path.join('blueprint', 'anneal_keys.toml')),
}

DOXYGEN_XML_VAR = 'TADAH_DOXYGEN_XML_DIR'


class SourceNotFound(RuntimeError):
    '''A docs-build input could not be located.'''


def _default_runner(cmd):
    return subprocess.run(cmd, check=False, capture_output=True, text=True)


def ci_platform(environ):
    '''Return 'GitLab CI', 'Read the Docs' or None.'''
    if environ.get('GITLAB_CI') == 'true':
        return 'GitLab CI'
    if environ.get('READTHEDOCS') == 'True':
        return 'Read the Docs'
    return None


# The variables that name the ref being built, in tadah_get_git_branch's
# order (cmake-tools, cmake/TadahDevelop.cmake).
BUILT_BRANCH_VARS = ('CI_COMMIT_REF_NAME', 'READTHEDOCS_GIT_IDENTIFIER',
                     'READTHEDOCS_VERSION_NAME', 'READTHEDOCS_VERSION')


def built_branch(environ):
    '''The ref being built on CI/RTD, as tadah_get_git_branch names it:
    (variable, value) of the first of BUILT_BRANCH_VARS that is set and not
    empty, or (None, None).'''
    for var in BUILT_BRANCH_VARS:
        if environ.get(var):
            return var, environ[var]
    return None, None


def _checkout_names(repo):
    return [repo] if repo == repo.lower() else [repo, repo.lower()]


def locate(repo, environ=None, workspace=None, runner=None, clone_root=None):
    '''Return the root of a checkout of *repo* ('CORE' or 'HPO').

    The checkout must contain the repository's marker file (see REPOS).
    Raises SourceNotFound listing every location tried.  *environ*,
    *workspace*, *runner* and *clone_root* exist for the tests; the defaults
    are the process environment, the directory containing this Tadah.MLIP
    checkout, ``subprocess.run`` and a fresh temporary directory.
    '''
    if repo not in REPOS:
        raise ValueError(f"unknown repository {repo!r}; known: {sorted(REPOS)}")
    env_var, url, marker = REPOS[repo]
    environ = os.environ if environ is None else environ
    workspace = WORKSPACE if workspace is None else workspace
    runner = _default_runner if runner is None else runner

    # 1. Explicit override: authoritative, never followed by another lookup.
    if env_var in environ:
        value = environ[env_var]
        if not value:
            raise SourceNotFound(
                f"{env_var} is set but empty; point it at the {repo} checkout "
                f"(the directory that contains {marker}) or unset it.")
        root = os.path.abspath(value)
        if not os.path.isfile(os.path.join(root, marker)):
            raise SourceNotFound(
                f"{env_var}={value} does not contain {marker} "
                f"(looked for {os.path.join(root, marker)}); point it at the "
                f"{repo} checkout or unset it.")
        return root

    tried = [f"${env_var}: not set"]

    # 2. Sibling checkout, 3. $TADAH_PATH.
    bases = [('sibling checkout', workspace)]
    tadah_path = environ.get('TADAH_PATH')
    if tadah_path:
        bases.append(('$TADAH_PATH', tadah_path))
    for label, base in bases:
        for name in _checkout_names(repo):
            root = os.path.abspath(os.path.join(base, name))
            if os.path.isfile(os.path.join(root, marker)):
                return root
            tried.append(f"{label}: {os.path.join(root, marker)} (not found)")
    if not tadah_path:
        tried.append("$TADAH_PATH: not set")

    # 4. Clone of the built branch, on CI/RTD only.
    platform = ci_platform(environ)
    branch_var, branch = built_branch(environ)
    if platform is None:
        tried.append("clone of the built branch: skipped "
                     "(not on GitLab CI or Read the Docs)")
    elif branch is None:
        tried.append(f"clone of the built branch: skipped (on {platform}, "
                     f"but none of {', '.join(BUILT_BRANCH_VARS)} is set "
                     f"to a non-empty value)")
    else:
        dest = os.path.join(
            clone_root if clone_root is not None
            else tempfile.mkdtemp(prefix=f'tadah_{repo.lower()}_'), repo)
        print(f"source_locator: cloning {url} at {branch} "
              f"(${branch_var}, {platform}) for {marker}", flush=True)
        proc = runner(['git', 'clone', '--depth', '1', '--branch', branch,
                       url, dest])
        if proc.returncode == 0 and os.path.isfile(os.path.join(dest, marker)):
            return dest
        if proc.returncode == 0:
            reason = f"{marker} not in the clone"
        else:
            detail = (proc.stderr or '').strip().splitlines()
            reason = (f"git clone exited with {proc.returncode}"
                      + (f": {detail[-1]}" if detail else ''))
        tried.append(f"clone of {url} at {branch} (${branch_var}): {reason}")

    raise SourceNotFound(
        f"Cannot locate the {repo} sources (need {marker}). Tried:\n"
        + "\n".join(f"  {i}. {t}" for i, t in enumerate(tried, 1))
        + f"\nSet {env_var} to the {repo} checkout, set TADAH_PATH to a Tadah "
          f"workspace that contains {repo}, or build from a workspace with "
          f"{repo} next to Tadah.MLIP.")


def locate_file(repo, relpath, **kwargs):
    '''Return the path of *relpath* inside the located *repo* checkout.'''
    root = locate(repo, **kwargs)
    path = os.path.join(root, relpath)
    if not os.path.isfile(path):
        raise SourceNotFound(f"{repo} checkout {root} has no {relpath}")
    return path


def doxygen_xml_dir(environ=None, default=None):
    '''Return the Doxygen XML directory for breathe, verified by index.xml.'''
    environ = os.environ if environ is None else environ
    default = DEFAULT_DOXYGEN_XML_DIR if default is None else default
    if DOXYGEN_XML_VAR in environ:
        value = environ[DOXYGEN_XML_VAR]
        if not value:
            raise SourceNotFound(
                f"{DOXYGEN_XML_VAR} is set but empty; point it at the xml/ "
                "directory Doxygen wrote, or unset it.")
        xml_dir = os.path.abspath(value)
        if not os.path.isfile(os.path.join(xml_dir, 'index.xml')):
            raise SourceNotFound(
                f"{DOXYGEN_XML_VAR}={value} does not contain index.xml; point "
                "it at the xml/ directory Doxygen wrote (XML_OUTPUT, "
                "docs/xml under Doxygen's working directory).")
        return xml_dir
    if not os.path.isfile(os.path.join(default, 'index.xml')):
        raise SourceNotFound(
            f"Doxygen XML not found: {os.path.join(default, 'index.xml')} is "
            f"missing ({DOXYGEN_XML_VAR} is not set). Generate it with "
            "'cmake --build build --target docs' in the Tadah.MLIP checkout, "
            f"or set {DOXYGEN_XML_VAR} to an existing Doxygen xml/ directory.")
    return default
