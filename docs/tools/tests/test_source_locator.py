"""Tests for docs/source/config/source_locator.py and its use in conf.py.

Run: python3 -m unittest discover -s docs/tools/tests -v
"""
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

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


class LocateTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp(prefix='tadah_locator_test_')
        self.addCleanup(shutil.rmtree, self.tmp)
        self.checkout = os.path.join(self.tmp, 'checkout')
        os.makedirs(self.checkout)
        # The repository of the pages is a fixture too: these tests are about
        # the order of the locations, not about the real modules.
        patch = mock.patch.object(loc, 'CHECKOUT_ROOT', self.checkout)
        patch.start()
        self.addCleanup(patch.stop)

    # -- 1. explicit override ------------------------------------------------
    def test_explicit_override_wins_over_the_own_directory(self):
        explicit = make_checkout(os.path.join(self.tmp, 'mine'), HPO_MARKER)
        make_checkout(os.path.join(self.checkout, 'hpo'), HPO_MARKER)
        self.assertEqual(loc.locate('HPO', {'TADAH_HPO_SOURCE_DIR': explicit}),
                         explicit)

    def test_explicit_override_without_marker_is_an_error_not_a_fallback(self):
        make_checkout(os.path.join(self.checkout, 'core'), CORE_MARKER)
        bad = os.path.join(self.tmp, 'not_core')
        os.makedirs(bad)
        with self.assertRaises(loc.SourceNotFound) as cm:
            loc.locate('CORE', {'TADAH_CORE_SOURCE_DIR': bad})
        msg = str(cm.exception)
        self.assertIn(f'TADAH_CORE_SOURCE_DIR={bad}', msg)
        self.assertIn(CORE_MARKER, msg)

    def test_explicit_override_nonexistent_directory(self):
        with self.assertRaises(loc.SourceNotFound) as cm:
            loc.locate('HPO', {'TADAH_HPO_SOURCE_DIR': '/nonexistent/hpo'})
        self.assertIn('TADAH_HPO_SOURCE_DIR=/nonexistent/hpo', str(cm.exception))

    def test_explicit_override_set_but_empty_is_an_error(self):
        make_checkout(os.path.join(self.checkout, 'hpo'), HPO_MARKER)
        with self.assertRaises(loc.SourceNotFound) as cm:
            loc.locate('HPO', {'TADAH_HPO_SOURCE_DIR': ''})
        self.assertIn('set but empty', str(cm.exception))

    # -- 2. the repository of the pages -----------------------------------------
    def test_own_directory(self):
        own = make_checkout(os.path.join(self.checkout, 'hpo'), HPO_MARKER)
        self.assertEqual(loc.locate('HPO', {}), own)

    def test_own_directory_without_the_file_is_an_error_not_a_fallback(self):
        # An incomplete checkout: reading the checkout beside it instead
        # would make the result depend on the surroundings again.
        own = os.path.join(self.checkout, 'core')
        os.makedirs(own)
        beside = make_checkout(os.path.join(self.tmp, 'CORE'), CORE_MARKER)
        with self.assertRaises(loc.SourceNotFound) as cm:
            loc.locate('CORE', {})
        msg = str(cm.exception)
        self.assertIn(os.path.join(own, CORE_MARKER), msg)
        self.assertIn('TADAH_CORE_SOURCE_DIR', msg)
        self.assertNotIn(beside, msg)

    def test_missing_own_directory_is_an_error_not_a_fallback(self):
        make_checkout(os.path.join(self.tmp, 'HPO'), HPO_MARKER)
        make_checkout(os.path.join(self.tmp, 'hpo'), HPO_MARKER)
        with self.assertRaises(loc.SourceNotFound) as cm:
            loc.locate('HPO', {})
        msg = str(cm.exception)
        self.assertIn(os.path.join(self.checkout, 'hpo', HPO_MARKER), msg)
        self.assertIn('TADAH_HPO_SOURCE_DIR', msg)

    def test_locate_file_and_unknown_repo(self):
        root = make_checkout(os.path.join(self.checkout, 'core'), CORE_MARKER)
        self.assertEqual(loc.locate_file('CORE', CORE_MARKER, environ={}),
                         os.path.join(root, CORE_MARKER))
        with self.assertRaises(loc.SourceNotFound):
            loc.locate_file('CORE', 'no/such/file', environ={})
        with self.assertRaises(ValueError):
            loc.locate('MODELS', environ={})


class OwnRepositoryTest(unittest.TestCase):
    """The modules the pages are generated from are those of the repository
    that holds the pages, whatever lies beside its checkout."""

    def test_a_module_directory_beside_the_checkout_is_not_read(self):
        with tempfile.TemporaryDirectory(prefix='tadah_locator_test_') as tmp:
            for repo, marker in (('CORE', CORE_MARKER), ('HPO', HPO_MARKER)):
                make_checkout(os.path.join(tmp, repo), marker)
                make_checkout(os.path.join(tmp, repo.lower()), marker)
                self.assertEqual(
                    loc.locate(repo, environ={'TADAH_PATH': tmp}),
                    os.path.join(loc.CHECKOUT_ROOT, repo.lower()))


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
        # without depending on this machine's layout.
        self.tmp = tempfile.mkdtemp(prefix='tadah_conf_test_')
        self.addCleanup(shutil.rmtree, self.tmp)
        self.core = make_checkout(os.path.join(self.tmp, 'CORE'), CORE_MARKER)
        self.hpo = make_checkout(os.path.join(self.tmp, 'HPO'), HPO_MARKER)

    def sphinx(self, **overrides):
        env = dict(os.environ, **overrides)
        return subprocess.run(
            # --no-color: the message is compared as text (FORCE_COLOR).
            ['sphinx-build', '-W', '--no-color', '-b', 'html',
             os.path.join(DOCS, 'source'),
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
