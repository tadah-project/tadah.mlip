'''
Locate the external inputs of the Sphinx docs build.

The generated pages read files of two modules -- CORE's blueprint (version,
configuration keys, CLI) and HPO's ANNEAL key registry -- and the API pages
read Doxygen XML.  Every input is resolved explicitly;
nothing falls back to a default branch, and a failure lists every location
that was tried.

Source checkouts (``locate``), in this order:

  1. ``$TADAH_CORE_SOURCE_DIR`` / ``$TADAH_HPO_SOURCE_DIR``.  When the
     variable is set it must name a checkout that contains the needed file;
     otherwise the build fails without looking anywhere else.
  2. The repository that holds these pages: its directory ``core`` (``hpo``).
     It must contain the needed file; otherwise the build fails without
     looking anywhere else, so a directory of the same name beside the
     checkout is never read in its place.

Doxygen XML (``doxygen_xml_dir``): ``$TADAH_DOXYGEN_XML_DIR`` when set (it
must contain ``index.xml``), else the ``build/docs/xml`` directory that
``cmake --build build --target docs`` writes in this checkout.
'''
import os

_HERE = os.path.dirname(os.path.abspath(__file__))

# config/ -> source/ -> docs/ -> the repository.
CHECKOUT_ROOT = os.path.normpath(os.path.join(_HERE, '..', '..', '..'))
DEFAULT_DOXYGEN_XML_DIR = os.path.join(CHECKOUT_ROOT, 'build', 'docs', 'xml')

# Repository -> (override variable, the module's directory in this
# repository, file that identifies a usable checkout).  The marker is the file the docs
# generators read from it.
REPOS = {
    'CORE': ('TADAH_CORE_SOURCE_DIR', 'core',
             os.path.join('blueprint', 'blueprint_tadah.toml')),
    'HPO': ('TADAH_HPO_SOURCE_DIR', 'hpo',
            os.path.join('blueprint', 'anneal_keys.toml')),
}

DOXYGEN_XML_VAR = 'TADAH_DOXYGEN_XML_DIR'


class SourceNotFound(RuntimeError):
    '''A docs-build input could not be located.'''


def locate(repo, environ=None):
    '''Return the root of a checkout of *repo* ('CORE' or 'HPO').

    The checkout must contain the repository's marker file (see REPOS).
    Raises SourceNotFound naming the location it looked at.  *environ*
    exists for the tests; the default is the process environment.
    '''
    if repo not in REPOS:
        raise ValueError(f"unknown repository {repo!r}; known: {sorted(REPOS)}")
    env_var, name, marker = REPOS[repo]
    environ = os.environ if environ is None else environ

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

    # 2. This repository: authoritative.
    own = os.path.abspath(os.path.join(CHECKOUT_ROOT, name))
    if os.path.isfile(os.path.join(own, marker)):
        return own
    raise SourceNotFound(
        f"The directory {name} of this repository does not contain "
        f"{marker} (looked for {os.path.join(own, marker)}); restore it, "
        f"or set {env_var} to build from another {repo} checkout.")


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
