#!/usr/bin/env python3
"""The written copies of the version agree with the blueprint.

    check_version_copies.py --source <repository>

The version is written once, in the [TADAH] table of
core/blueprint/blueprint_tadah.toml. The program, the LAMMPS pair style, the
journal of tadah hpo and the documentation read it from there. These files
hold a copy, which a release changes together with the blueprint:

  * CITATION.cff: ``version``;
  * README.md: the version badge;
  * CHANGELOG.md: the first heading, exactly ``## [<version>]``;
  * docs/source/installation.rst and lammps/integration/README.md: the
    release tag of the clone commands, ``v<version>``, and the comment
    "the <version> release".

No file holds the date of a release, which the tag of the release carries:
the heading of the changelog is the version alone, and CITATION.cff has no
``date-released``.

A copy that differs from the blueprint, a file without its copy or a release
date is reported with the file and the line, and the exit status is 1.
"""
import argparse
import os
import re
import sys

# A release tag in a page: v2.0.0, v2.0.0-beta.1 (not the end of a word, and
# no full stop of a sentence after it).
TAG = re.compile(r'(?<![\w.])v(\d+\.\d+\.\d+'
                 r'(?:-[0-9A-Za-z]+(?:\.[0-9A-Za-z]+)*)?)')
PAGES = ('docs/source/installation.rst', 'lammps/integration/README.md')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--source', required=True)
    source = parser.parse_args().source

    # The reader of the documentation build, so that the pages and this
    # check read the blueprint the same way.
    sys.path.insert(0, os.path.join(source, 'docs', 'source', 'config'))
    import blueprint_source
    version = blueprint_source.read_version(
        os.path.join(source, 'core', blueprint_source.BLUEPRINT_REL))

    errors = []

    def lines(name):
        with open(os.path.join(source, name), encoding='utf-8') as handle:
            return list(enumerate(handle.read().splitlines(), 1))

    def copies(name, pattern, expected, what, read=str):
        '''Every match of *pattern* in *name* is *expected*; one at least.'''
        found = [(n, read(m.group(1))) for n, line in lines(name)
                 for m in pattern.finditer(line)]
        if not found:
            errors.append(f'{name}: no {what}')
        for n, value in found:
            if value != expected:
                errors.append(f'{name}:{n}: {what} is {value}, '
                              f'the blueprint has {expected}')

    copies('CITATION.cff', re.compile(r'^version:\s*"?([^"\s]+)"?\s*$'),
           version, 'version')
    # A badge writes a hyphen of its text twice.
    copies('README.md', re.compile(r'badge/version-(.+?)-blue\.svg'),
           version, 'version badge', lambda text: text.replace('--', '-'))
    for page in PAGES:
        copies(page, TAG, version, 'release tag')
    copies('docs/source/installation.rst',
           re.compile(r'# or the (\S+) release'), version, 'release comment')

    # The first heading of the changelog is the version alone.
    heading = next(((n, line) for n, line in lines('CHANGELOG.md')
                    if line.startswith('## ')), None)
    if heading is None:
        errors.append('CHANGELOG.md: no heading of a version')
    elif heading[1] != f'## [{version}]':
        errors.append(f'CHANGELOG.md:{heading[0]}: the first heading is not '
                      f'"## [{version}]": {heading[1]}')
    for n, line in lines('CITATION.cff'):
        if re.match(r'^\s*date-released\s*:', line):
            errors.append(f'CITATION.cff:{n}: a release date: {line.strip()}')

    for error in errors:
        print(error)
    if errors:
        return 1
    print(f'every copy says {version}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
