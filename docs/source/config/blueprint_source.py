'''
Locate CORE's blueprint_tadah.toml -- the single source of truth for the
Tadah! version and the configuration-key reference.

The CORE checkout is found by source_locator.py: $TADAH_CORE_SOURCE_DIR,
then the CORE checkout next to this repository, then $TADAH_PATH/CORE, then
(on GitLab CI / Read the Docs only) a shallow clone of the branch being
built.  There is no default: if the blueprint cannot be found the docs build
fails with the list of locations tried.
'''
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import source_locator

BLUEPRINT_REL = os.path.join('blueprint', 'blueprint_tadah.toml')


def locate_blueprint():
    '''Return the path of blueprint_tadah.toml or raise SourceNotFound.'''
    return source_locator.locate_file('CORE', BLUEPRINT_REL)


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
