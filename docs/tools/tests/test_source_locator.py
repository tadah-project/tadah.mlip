"""Tests for docs/source/config/source_locator.py and its use in conf.py.

Run: python3 -m unittest discover -s docs/tools/tests -v
"""
import contextlib
import io
import os
import shutil
import subprocess
import sys
import tempfile
import types
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
DOCS = os.path.normpath(os.path.join(HERE, '..', '..'))
sys.path.insert(0, os.path.join(DOCS, 'source', 'config'))
import source_locator as loc  # noqa: E402

CORE_MARKER = os.path.join('blueprint', 'blueprint_tadah.toml')
HPO_MARKER = os.path.join('blueprint', 'anneal_keys.toml')


def make_checkout(root, marker):
    os.makedirs(os.path.join(root, os.path.dirname(marker)), exist_ok=True)
    with open(os.path.join(root, marker), 'w') as fh:
        fh.write('# test\n')
    return root


class FakeGit:
    '''Stands in for subprocess.run; records every command.'''

    def __init__(self, marker=None, returncode=0, stderr=''):
        self.marker, self.returncode, self.stderr = marker, returncode, stderr
        self.calls = []

    def __call__(self, cmd):
        self.calls.append(cmd)
        if self.returncode == 0 and self.marker:
            make_checkout(cmd[-1], self.marker)
        return types.SimpleNamespace(returncode=self.returncode,
                                     stderr=self.stderr)


class LocateTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp(prefix='tadah_locator_test_')
        self.addCleanup(shutil.rmtree, self.tmp)
        self.workspace = os.path.join(self.tmp, 'workspace')
        self.tadah_path = os.path.join(self.tmp, 'tadah_path')
        self.clones = os.path.join(self.tmp, 'clones')
        for d in (self.workspace, self.tadah_path, self.clones):
            os.makedirs(d)

    def locate(self, repo, environ, runner=None):
        return loc.locate(repo, environ=environ, workspace=self.workspace,
                          runner=runner or FakeGit(), clone_root=self.clones)

    # -- 1. explicit override ------------------------------------------------
    def test_explicit_override_wins_over_every_other_location(self):
        explicit = make_checkout(os.path.join(self.tmp, 'mine'), HPO_MARKER)
        make_checkout(os.path.join(self.workspace, 'HPO'), HPO_MARKER)
        make_checkout(os.path.join(self.tadah_path, 'HPO'), HPO_MARKER)
        env = {'TADAH_HPO_SOURCE_DIR': explicit, 'TADAH_PATH': self.tadah_path}
        self.assertEqual(self.locate('HPO', env), explicit)

    def test_explicit_override_without_marker_is_an_error_not_a_fallback(self):
        make_checkout(os.path.join(self.workspace, 'CORE'), CORE_MARKER)
        bad = os.path.join(self.tmp, 'not_core')
        os.makedirs(bad)
        with self.assertRaises(loc.SourceNotFound) as cm:
            self.locate('CORE', {'TADAH_CORE_SOURCE_DIR': bad})
        msg = str(cm.exception)
        self.assertIn(f'TADAH_CORE_SOURCE_DIR={bad}', msg)
        self.assertIn(CORE_MARKER, msg)

    def test_explicit_override_nonexistent_directory(self):
        with self.assertRaises(loc.SourceNotFound) as cm:
            self.locate('HPO', {'TADAH_HPO_SOURCE_DIR': '/nonexistent/hpo'})
        self.assertIn('TADAH_HPO_SOURCE_DIR=/nonexistent/hpo', str(cm.exception))

    def test_explicit_override_set_but_empty_is_an_error(self):
        make_checkout(os.path.join(self.workspace, 'HPO'), HPO_MARKER)
        with self.assertRaises(loc.SourceNotFound) as cm:
            self.locate('HPO', {'TADAH_HPO_SOURCE_DIR': ''})
        self.assertIn('set but empty', str(cm.exception))

    # -- 2. sibling, 3. $TADAH_PATH -------------------------------------------
    def test_sibling_checkout_before_tadah_path(self):
        sibling = make_checkout(os.path.join(self.workspace, 'CORE'), CORE_MARKER)
        make_checkout(os.path.join(self.tadah_path, 'CORE'), CORE_MARKER)
        self.assertEqual(self.locate('CORE', {'TADAH_PATH': self.tadah_path}),
                         sibling)

    def test_lowercase_sibling_checkout(self):
        sibling = make_checkout(os.path.join(self.workspace, 'hpo'), HPO_MARKER)
        self.assertEqual(self.locate('HPO', {}), sibling)

    def test_tadah_path_when_no_sibling(self):
        root = make_checkout(os.path.join(self.tadah_path, 'HPO'), HPO_MARKER)
        self.assertEqual(self.locate('HPO', {'TADAH_PATH': self.tadah_path}), root)

    def test_directory_without_marker_is_not_a_checkout(self):
        os.makedirs(os.path.join(self.workspace, 'HPO'))
        root = make_checkout(os.path.join(self.tadah_path, 'HPO'), HPO_MARKER)
        self.assertEqual(self.locate('HPO', {'TADAH_PATH': self.tadah_path}), root)

    # -- 4. clone, CI/RTD only --------------------------------------------------
    def test_no_clone_outside_ci_and_error_lists_every_location(self):
        git = FakeGit(marker=HPO_MARKER)
        env = {'TADAH_PATH': self.tadah_path,
               'CI_COMMIT_REF_NAME': 'v1-dev'}   # a branch alone is not CI
        with self.assertRaises(loc.SourceNotFound) as cm:
            self.locate('HPO', env, runner=git)
        self.assertEqual(git.calls, [])
        msg = str(cm.exception)
        for expected in ('$TADAH_HPO_SOURCE_DIR: not set',
                         os.path.join(self.workspace, 'HPO', HPO_MARKER),
                         os.path.join(self.workspace, 'hpo', HPO_MARKER),
                         os.path.join(self.tadah_path, 'HPO', HPO_MARKER),
                         os.path.join(self.tadah_path, 'hpo', HPO_MARKER),
                         'skipped (not on GitLab CI or Read the Docs)',
                         'Set TADAH_HPO_SOURCE_DIR'):
            self.assertIn(expected, msg)
        self.assertNotIn('develop', msg)

    def test_error_says_when_tadah_path_is_unset(self):
        with self.assertRaises(loc.SourceNotFound) as cm:
            self.locate('CORE', {})
        self.assertIn('$TADAH_PATH: not set', str(cm.exception))

    def test_gitlab_ci_clones_the_built_branch(self):
        git = FakeGit(marker=HPO_MARKER)
        env = {'GITLAB_CI': 'true', 'CI_COMMIT_REF_NAME': 'followups/x'}
        root = self.locate('HPO', env, runner=git)
        self.assertEqual(git.calls, [[
            'git', 'clone', '--depth', '1', '--branch', 'followups/x',
            'https://github.com/tadah-project/hpo.git',
            os.path.join(self.clones, 'HPO')]])
        self.assertEqual(root, os.path.join(self.clones, 'HPO'))

    def test_tadah_git_base_names_the_clone_host(self):
        # TADAH_GIT_BASE replaces the public base address, as it does for
        # the FetchContent sources (cmake-tools' tadah_git_base); a trailing
        # '/' is dropped and an empty value counts as unset.
        for base, url in (
                ('https://mirror.example.org/tadah',
                 'https://mirror.example.org/tadah/core.git'),
                ('https://mirror.example.org/tadah/',
                 'https://mirror.example.org/tadah/core.git'),
                ('', 'https://github.com/tadah-project/core.git')):
            with self.subTest(base=base):
                git = FakeGit(marker=CORE_MARKER)
                env = {'GITLAB_CI': 'true', 'CI_COMMIT_REF_NAME': 'v1-dev',
                       'TADAH_GIT_BASE': base}
                self.locate('CORE', env, runner=git)
                self.assertEqual(git.calls[0][6], url)

    def test_clone_url_credentials_are_not_printed(self):
        # A CI base address may carry a job token: neither the progress
        # line nor the error shows it.
        git = FakeGit(returncode=128, stderr='fatal: not found')
        env = {'GITLAB_CI': 'true', 'CI_COMMIT_REF_NAME': 'v1-dev',
               'TADAH_GIT_BASE': 'https://ci-token:s3cret@host.example/tadah'}
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            with self.assertRaises(loc.SourceNotFound) as cm:
                self.locate('HPO', env, runner=git)
        self.assertEqual(git.calls[0][6],
                         'https://ci-token:s3cret@host.example/tadah/hpo.git')
        for text in (out.getvalue(), str(cm.exception)):
            self.assertNotIn('s3cret', text)
            self.assertIn('https://host.example/tadah/hpo.git', text)

    def test_rtd_branch_order_matches_tadah_get_git_branch(self):
        git = FakeGit(marker=CORE_MARKER)
        env = {'READTHEDOCS': 'True', 'READTHEDOCS_VERSION': 'release-x',
               'READTHEDOCS_VERSION_NAME': 'release/x'}
        self.locate('CORE', env, runner=git)
        self.assertEqual(git.calls[0][5], 'release/x')
        git = FakeGit(marker=CORE_MARKER)
        self.locate('CORE', {'READTHEDOCS': 'True',
                             'READTHEDOCS_VERSION': 'release-x'}, runner=git)
        self.assertEqual(git.calls[0][5], 'release-x')

    def test_rtd_latest_and_stable_clone_the_git_identifier(self):
        # 'latest' and 'stable' are Read the Docs version names, not refs;
        # READTHEDOCS_GIT_IDENTIFIER is the branch or tag it checked out.
        for version, ident in (('latest', 'v1-dev'), ('stable', 'v1.6.0'),
                               ('release-x', 'release/x')):
            with self.subTest(version=version):
                git = FakeGit(marker=CORE_MARKER)
                env = {'READTHEDOCS': 'True',
                       'READTHEDOCS_VERSION': version,
                       'READTHEDOCS_VERSION_NAME': version,
                       'READTHEDOCS_GIT_IDENTIFIER': ident}
                root = self.locate('CORE', env, runner=git)
                self.assertEqual(git.calls, [[
                    'git', 'clone', '--depth', '1', '--branch', ident,
                    'https://github.com/tadah-project/core.git',
                    os.path.join(self.clones, 'CORE')]])
                self.assertEqual(root, os.path.join(self.clones, 'CORE'))

    def test_built_branch_order_matches_tadah_get_git_branch(self):
        # CI_COMMIT_REF_NAME, READTHEDOCS_GIT_IDENTIFIER,
        # READTHEDOCS_VERSION_NAME, READTHEDOCS_VERSION: the first one set.
        full = {'CI_COMMIT_REF_NAME': 'ci',
                'READTHEDOCS_GIT_IDENTIFIER': 'ident',
                'READTHEDOCS_VERSION_NAME': 'name',
                'READTHEDOCS_VERSION': 'slug'}
        order = ['CI_COMMIT_REF_NAME', 'READTHEDOCS_GIT_IDENTIFIER',
                 'READTHEDOCS_VERSION_NAME', 'READTHEDOCS_VERSION']
        for i, var in enumerate(order):
            env = {k: v for k, v in full.items() if k in order[i:]}
            with self.subTest(first=var):
                self.assertEqual(loc.built_branch(env), (var, full[var]))
        self.assertEqual(loc.built_branch({}), (None, None))

    def test_set_but_empty_variable_names_no_ref(self):
        # As in tadah_get_git_branch (tests/git_branch checks both): a
        # variable counts only when it is set and non-empty.
        env = {'READTHEDOCS_GIT_IDENTIFIER': '',
               'READTHEDOCS_VERSION_NAME': 'release/x',
               'READTHEDOCS_VERSION': 'release-x'}
        self.assertEqual(loc.built_branch(env),
                         ('READTHEDOCS_VERSION_NAME', 'release/x'))
        self.assertEqual(
            loc.built_branch({v: '' for v in loc.BUILT_BRANCH_VARS}),
            (None, None))

    def test_ci_with_only_empty_branch_variables_is_reported(self):
        git = FakeGit(marker=HPO_MARKER)
        env = {'GITLAB_CI': 'true', 'CI_COMMIT_REF_NAME': ''}
        with self.assertRaises(loc.SourceNotFound) as cm:
            self.locate('HPO', env, runner=git)
        self.assertEqual(git.calls, [])
        self.assertIn('on GitLab CI, but none of CI_COMMIT_REF_NAME, '
                      'READTHEDOCS_GIT_IDENTIFIER, READTHEDOCS_VERSION_NAME, '
                      'READTHEDOCS_VERSION is set to a non-empty value',
                      str(cm.exception))

    def test_clone_names_the_variable_it_took_the_ref_from(self):
        git = FakeGit(returncode=128, stderr='fatal: Remote branch v9 not found')
        env = {'READTHEDOCS': 'True', 'READTHEDOCS_VERSION_NAME': 'stable',
               'READTHEDOCS_GIT_IDENTIFIER': 'v9'}
        with self.assertRaises(loc.SourceNotFound) as cm:
            self.locate('HPO', env, runner=git)
        self.assertIn('hpo.git at v9 ($READTHEDOCS_GIT_IDENTIFIER): git clone '
                      'exited with 128', str(cm.exception))

    def test_failed_clone_has_no_second_attempt_and_is_reported(self):
        git = FakeGit(returncode=128,
                      stderr='Cloning...\nfatal: Remote branch feat not found')
        env = {'GITLAB_CI': 'true', 'CI_COMMIT_REF_NAME': 'feat'}
        with self.assertRaises(loc.SourceNotFound) as cm:
            self.locate('HPO', env, runner=git)
        self.assertEqual(len(git.calls), 1)
        msg = str(cm.exception)
        self.assertIn('hpo.git at feat ($CI_COMMIT_REF_NAME): git clone exited '
                      'with 128: fatal: Remote branch feat not found', msg)
        self.assertNotIn('develop', msg)

    def test_clone_without_marker_is_reported(self):
        git = FakeGit(marker=None)
        env = {'GITLAB_CI': 'true', 'CI_COMMIT_REF_NAME': 'v1-dev'}
        with self.assertRaises(loc.SourceNotFound) as cm:
            self.locate('CORE', env, runner=git)
        self.assertIn(f'{CORE_MARKER} not in the clone', str(cm.exception))

    def test_ci_without_branch_variable_is_reported(self):
        git = FakeGit(marker=HPO_MARKER)
        with self.assertRaises(loc.SourceNotFound) as cm:
            self.locate('HPO', {'READTHEDOCS': 'True'}, runner=git)
        self.assertEqual(git.calls, [])
        self.assertIn('on Read the Docs, but none of CI_COMMIT_REF_NAME, '
                      'READTHEDOCS_GIT_IDENTIFIER, READTHEDOCS_VERSION_NAME, '
                      'READTHEDOCS_VERSION is set', str(cm.exception))

    def test_locate_file_and_unknown_repo(self):
        root = make_checkout(os.path.join(self.workspace, 'CORE'), CORE_MARKER)
        self.assertEqual(
            loc.locate_file('CORE', CORE_MARKER, environ={},
                            workspace=self.workspace),
            os.path.join(root, CORE_MARKER))
        with self.assertRaises(loc.SourceNotFound):
            loc.locate_file('CORE', 'no/such/file', environ={},
                            workspace=self.workspace)
        with self.assertRaises(ValueError):
            loc.locate('MODELS', environ={})


class DoxygenXmlTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp(prefix='tadah_xml_test_')
        self.addCleanup(shutil.rmtree, self.tmp)
        self.good = os.path.join(self.tmp, 'xml')
        os.makedirs(self.good)
        open(os.path.join(self.good, 'index.xml'), 'w').close()
        self.missing = os.path.join(self.tmp, 'build', 'docs', 'xml')

    def test_explicit_directory(self):
        self.assertEqual(loc.doxygen_xml_dir(
            {'TADAH_DOXYGEN_XML_DIR': self.good}, default=self.missing), self.good)

    def test_explicit_directory_without_index_is_an_error(self):
        with self.assertRaises(loc.SourceNotFound) as cm:
            loc.doxygen_xml_dir({'TADAH_DOXYGEN_XML_DIR': self.tmp},
                                default=self.good)
        self.assertIn(f'TADAH_DOXYGEN_XML_DIR={self.tmp} does not contain '
                      'index.xml', str(cm.exception))

    def test_explicit_empty_is_an_error(self):
        with self.assertRaises(loc.SourceNotFound):
            loc.doxygen_xml_dir({'TADAH_DOXYGEN_XML_DIR': ''}, default=self.good)

    def test_default_directory(self):
        self.assertEqual(loc.doxygen_xml_dir({}, default=self.good), self.good)

    def test_missing_default_explains_both_remedies(self):
        with self.assertRaises(loc.SourceNotFound) as cm:
            loc.doxygen_xml_dir({}, default=self.missing)
        msg = str(cm.exception)
        self.assertIn(os.path.join(self.missing, 'index.xml'), msg)
        self.assertIn('--target docs', msg)
        self.assertIn('TADAH_DOXYGEN_XML_DIR', msg)

    def test_default_is_the_cmake_docs_target_output(self):
        checkout = os.path.normpath(os.path.join(DOCS, '..'))
        self.assertEqual(loc.DEFAULT_DOXYGEN_XML_DIR,
                         os.path.join(checkout, 'build', 'docs', 'xml'))


@unittest.skipIf(shutil.which('sphinx-build') is None,
                 'sphinx-build is not installed')
class ConfPyTest(unittest.TestCase):
    '''A bad override stops the build: conf.py raises Sphinx's
    configuration error (sphinx.errors.ConfigError) with the locator's
    message, which names the variable and its value on one line. How
    sphinx-build shows it is Sphinx's: 7.4 prints "Configuration error:"
    and the message; 9 prints "Configuration error!" and a report whose
    traceback section ends with the exception and the message.'''

    def setUp(self):
        # conf.py resolves CORE, then HPO, then the XML, and stops at the
        # first failure: fake checkouts make the earlier lookups succeed
        # without depending on this machine's layout (and never clone).
        self.tmp = tempfile.mkdtemp(prefix='tadah_conf_test_')
        self.addCleanup(shutil.rmtree, self.tmp)
        self.core = make_checkout(os.path.join(self.tmp, 'CORE'), CORE_MARKER)
        self.hpo = make_checkout(os.path.join(self.tmp, 'HPO'), HPO_MARKER)

    def sphinx(self, **overrides):
        env = dict(os.environ, **overrides)
        return subprocess.run(
            ['sphinx-build', '-W', '-b', 'html', os.path.join(DOCS, 'source'),
             os.path.join(self.tmp, 'html')],
            env=env, capture_output=True, text=True, timeout=300)

    def conf_error(self, **overrides):
        '''The error conf.py raises when Sphinx reads it (Config.read, which
        passes conf.py's ConfigError on as it is), as "<type>: <message>".
        overrides and tags by keyword: Sphinx 9 requires both, as keywords;
        Sphinx 7.4 takes either form.'''
        code = ('import sys\n'
                'from sphinx.config import Config\n'
                'from sphinx.util.tags import Tags\n'
                'try:\n'
                '    Config.read(sys.argv[1], overrides={}, tags=Tags())\n'
                'except Exception as e:\n'
                '    t = type(e)\n'
                '    print(f"ERROR={t.__module__}.{t.__qualname__}: {e}")\n')
        proc = subprocess.run(
            [sys.executable, '-c', code, os.path.join(DOCS, 'source')],
            env=dict(os.environ, **overrides), capture_output=True, text=True,
            timeout=120)
        self.assertEqual(proc.returncode, 0, proc.stderr)
        lines = [l for l in proc.stdout.splitlines() if l.startswith('ERROR=')]
        self.assertEqual(len(lines), 1, proc.stdout)
        return lines[0][len('ERROR='):]

    def assert_stops(self, message, **overrides):
        # Pins: conf.py raises sphinx.errors.ConfigError whose text is the
        # locator's message alone (no other exception, nothing wrapped
        # around it), and sphinx-build fails with that message on one line
        # of its standard error, whatever Sphinx formats around it.
        # Before the fix: passes as well with Sphinx 7.4 (a pin; the case
        # asserted that standard error has no 'Traceback', which held with
        # Sphinx 7.4 alone: Sphinx 9.0.4 reports a ConfigError raised by
        # conf.py with a traceback section, and the case failed there).
        self.assertEqual(self.conf_error(**overrides),
                         'sphinx.errors.ConfigError: ' + message)
        proc = self.sphinx(**overrides)
        self.assertNotEqual(proc.returncode, 0)
        self.assertTrue(any(l.rstrip().endswith(message)
                            for l in proc.stderr.splitlines()), proc.stderr)

    def test_bad_hpo_override_stops_the_build(self):
        overrides = dict(TADAH_CORE_SOURCE_DIR=self.core,
                         TADAH_HPO_SOURCE_DIR='/nonexistent/hpo')
        with self.assertRaises(loc.SourceNotFound) as cm:
            loc.locate('HPO', environ=overrides)
        message = str(cm.exception)
        self.assertIn('TADAH_HPO_SOURCE_DIR=/nonexistent/hpo does not contain',
                      message)
        self.assert_stops(message, **overrides)

    def test_bad_doxygen_override_stops_the_build(self):
        overrides = dict(TADAH_CORE_SOURCE_DIR=self.core,
                         TADAH_HPO_SOURCE_DIR=self.hpo,
                         TADAH_DOXYGEN_XML_DIR='/nonexistent/xml')
        with self.assertRaises(loc.SourceNotFound) as cm:
            loc.doxygen_xml_dir(overrides)
        message = str(cm.exception)
        self.assertIn('TADAH_DOXYGEN_XML_DIR=/nonexistent/xml does not contain '
                      'index.xml', message)
        self.assert_stops(message, **overrides)


if __name__ == '__main__':
    unittest.main()
