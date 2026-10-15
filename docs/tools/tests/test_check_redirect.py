"""Tests for docs/tools/check_redirect.py and docs/source/_extra/hpo.html.

Run: python3 -m unittest discover -s docs/tools/tests -v
"""
import importlib.util
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import types
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
TOOLS = os.path.normpath(os.path.join(HERE, '..'))
DOCS_SOURCE = os.path.normpath(os.path.join(TOOLS, '..', 'source'))
REDIRECT = os.path.join(DOCS_SOURCE, '_extra', 'hpo.html')
RTD_CONFIG = os.path.normpath(os.path.join(TOOLS, '..', '..',
                                           '.readthedocs.yaml'))
sys.path.insert(0, TOOLS)
import check_redirect  # noqa: E402

ROUTES = {'default': 'conf.html',
          'routes': {'intro': 'bg.html', 'old-name': 'other.html#new-name'}}


def redirect_page(routes=ROUTES, refresh='conf.html', canonical='conf.html',
                  robots=True, link='conf.html', routes_json=None):
    table = json.dumps(routes) if routes_json is None else routes_json
    return f'''<!DOCTYPE html><html><head>
{'<meta name="robots" content="noindex">' if robots else ''}
<link rel="canonical" href="{canonical}">
<script type="application/json" id="redirect-routes">{table}</script>
<noscript><meta http-equiv="refresh" content="0; url={refresh}"></noscript>
</head><body><a href="{link}">go</a></body></html>'''


def page(*ids):
    return ('<html><body>'
            + ''.join(f'<section id="{i}"><h1>{i}</h1></section>' for i in ids)
            + '</body></html>')


class CheckTest(unittest.TestCase):
    def setUp(self):
        self.out = tempfile.mkdtemp(prefix='tadah_redirect_test_')
        self.addCleanup(shutil.rmtree, self.out)
        self.anchors = os.path.join(self.out, 'anchors.txt')
        self.write('anchors.txt', '# comment\nintro\nold-name\n\nkept  # moved\n')
        self.write('hpo.html', redirect_page())
        self.write('conf.html', page('kept'))
        self.write('bg.html', page('intro'))
        self.write('other.html', page('new-name'))

    def write(self, name, text):
        with open(os.path.join(self.out, name), 'w', encoding='utf-8') as fh:
            fh.write(text)

    def check(self):
        return check_redirect.check(self.out, self.anchors)

    def assertOneProblem(self, *fragments):
        problems = self.check()
        self.assertEqual(len(problems), 1, problems)
        for f in fragments:
            self.assertIn(f, problems[0])

    def test_consistent_tree_passes(self):
        self.assertEqual(self.check(), [])

    def test_missing_redirect_page(self):
        os.remove(os.path.join(self.out, 'hpo.html'))
        self.assertOneProblem('hpo.html is missing')

    def test_malformed_routes_table(self):
        self.write('hpo.html', redirect_page(routes_json='{"default": 1'))
        self.assertOneProblem('malformed routes table')
        self.write('hpo.html', redirect_page(routes_json='{"routes": {}}'))
        self.assertOneProblem('malformed routes table')

    def test_unrouted_anchor_missing_from_default_page(self):
        self.write('conf.html', page('something-else'))
        self.assertOneProblem('#kept is sent to conf.html#kept',
                              'conf.html has no id "kept"')

    def test_page_only_route_needs_the_section_on_that_page(self):
        self.write('bg.html', page('elsewhere'))
        problems = self.check()
        # once as a routed fragment, once as an old anchor
        self.assertEqual(len(problems), 2, problems)
        self.assertIn('#intro is sent to bg.html, but bg.html has no id "intro"',
                      problems[0])

    def test_rewriting_route_needs_its_fragment(self):
        self.write('other.html', page('old-name'))
        problems = self.check()
        self.assertEqual(len(problems), 2, problems)
        self.assertIn('#old-name is sent to other.html#new-name, but other.html '
                      'has no id "new-name"', problems[0])

    def test_missing_target_page(self):
        os.remove(os.path.join(self.out, 'other.html'))
        self.assertOneProblem('target page other.html is missing')

    def test_meta_refresh_canonical_robots_and_link(self):
        self.write('hpo.html', redirect_page(refresh='bg.html'))
        self.assertOneProblem('meta refresh to conf.html')
        self.write('hpo.html', redirect_page(canonical='bg.html'))
        self.assertOneProblem('rel=canonical conf.html')
        self.write('hpo.html', redirect_page(robots=False))
        self.assertOneProblem('noindex')
        self.write('hpo.html', redirect_page(link='bg.html'))
        self.assertOneProblem('no visible link to conf.html')

    def test_command_line_exit_status(self):
        script = os.path.join(TOOLS, 'check_redirect.py')
        ok = subprocess.run([sys.executable, script, self.out,
                             '--anchors', self.anchors],
                            capture_output=True, text=True)
        self.assertEqual(ok.returncode, 0, ok.stderr)
        os.remove(os.path.join(self.out, 'bg.html'))
        bad = subprocess.run([sys.executable, script, self.out,
                              '--anchors', self.anchors],
                             capture_output=True, text=True)
        self.assertEqual(bad.returncode, 1)
        self.assertIn('target page bg.html is missing', bad.stderr)


def shipped_routes():
    with open(REDIRECT, encoding='utf-8') as fh:
        text = fh.read()
    m = re.search(r'<script type="application/json" id="redirect-routes">'
                  r'(.*?)</script>', text, re.S)
    return text, json.loads(m.group(1))


class ShippedPageTest(unittest.TestCase):
    '''The page docs/source/_extra/hpo.html as shipped.'''

    def test_configuration_topics_keep_deep_links_after_the_split(self):
        # A visitor following a reference link must reach the relevant
        # option page, rather than the file-format page or its top.
        _, table = shipped_routes()
        for anchor, page in (
            ('loss-functions', 'nfp_losses.html'),
            ('search-space-constraints', 'nfp_search_space.html'),
            ('restart-decorator', 'nfp_restart.html'),
            ('noise-block', 'nfp_noise.html'),
        ):
            with self.subTest(anchor=anchor):
                self.assertEqual(table['routes'].get(anchor),
                                 f'{page}#{anchor}')

    def test_routes_of_the_split(self):
        _, table = shipped_routes()
        self.assertEqual(table['default'], 'nfp_configuration.html')
        routes = table['routes']
        for anchor, target in (('nested-fitting', 'nfp_background.html'),
                               ('background', 'nfp_background.html'),
                               ('quick-start', 'nfp_quickstart.html'),
                               ('nfp-quick-start', 'nfp_quickstart.html')):
            self.assertEqual(routes[anchor], target)

    def test_routes_of_the_tagged_releases(self):
        # hpo.html of the tagged releases v1.0.0 and v1.1.1 (what Read the
        # Docs 'stable' serves) had ids the split pages do not carry.
        _, table = shipped_routes()
        routes = table['routes']
        self.assertEqual(routes['hpo-quick-start'],
                         'nfp_quickstart.html#nfp-quick-start')
        for anchor in ('hpo-manual', 'manual'):
            self.assertEqual(routes[anchor],
                             'nfp_configuration.html#nfp-manual')

    def test_routes_of_the_renamed_parallelism_sections(self):
        # 1.6.0 removed the legacy MPI build: the "Desktop build (OpenMP)"
        # and "MPI build" subsections of Performance & parallelism became
        # "Within one process ..." and "Across MPI ranks (TADAH_HPO_MPI)".
        _, table = shipped_routes()
        routes = table['routes']
        self.assertEqual(
            routes['desktop-build-openmp'],
            'nfp_review_pending.html#within-one-process-openmp-and-the-worker-pool')
        self.assertEqual(
            routes['mpi-build'],
            'nfp_review_pending.html#across-mpi-ranks-tadah-hpo-mpi')

    @unittest.skipIf(shutil.which('node') is None, 'node is not installed')
    def test_current_configuration_urls_follow_moved_topics(self):
        # The pre-split nfp_configuration.html page is still the format
        # guide. Its old deep links must work as well as hpo.html links.
        script = os.path.join(DOCS_SOURCE, '_static', 'nfp_redirect.js')
        with open(script, encoding='utf-8') as fh:
            javascript = fh.read()
        cases = {
            '': None,
            '#nfp-manual': None,
            '#loss-functions': 'nfp_losses.html#loss-functions',
            '#hpo-fd-stencil': 'nfp_gradients.html#hpo-fd-stencil',
            '#__proto__': None,
            '#toString': None,
            '#unknown-section': None,
        }
        harness = '''
var out = {};
JSON.parse(process.argv[1]).forEach(function (hash) {
  out[hash] = null;
  var window = {location: {hash: hash,
                          replace: function (u) { out[hash] = u; }}};
  (function (window) { %s })(window);
});
console.log(JSON.stringify(out));
''' % javascript
        proc = subprocess.run(
            ['node', '-e', harness, json.dumps(list(cases))],
            capture_output=True, text=True, timeout=60)
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertEqual(json.loads(proc.stdout), cases)

    def test_every_legacy_anchor_is_listed_once(self):
        anchors = check_redirect.read_anchors(check_redirect.DEFAULT_ANCHORS)
        self.assertEqual(len(anchors), len(set(anchors)))
        self.assertFalse([a for a in anchors if re.fullmatch(r'id\d+', a)])
        _, table = shipped_routes()
        self.assertFalse(set(table['routes']) - set(anchors),
                         'every route should be an anchor of the old page')

    @unittest.skipIf(shutil.which('node') is None, 'node is not installed')
    def test_inline_script_routes_like_the_checker_assumes(self):
        text, table = shipped_routes()
        scripts = re.findall(r'<script>(.*?)</script>', text, re.S)
        self.assertEqual(len(scripts), 1)
        cases = {'': 'nfp_configuration.html',
                 '#loss-functions': 'nfp_configuration.html#loss-functions',
                 '#__proto__': 'nfp_configuration.html#__proto__',
                 '#toString': 'nfp_configuration.html#toString'}
        for anchor, target in table['routes'].items():
            cases['#' + anchor] = target
        harness = '''
var routes = process.argv[1], out = {};
JSON.parse(process.argv[2]).forEach(function (hash) {
  var document = {getElementById: function (id) {
    return id === "redirect-routes" ? {textContent: routes} : null; }};
  var window = {location: {hash: hash,
                           replace: function (u) { out[hash] = u; }}};
  (function (document, window) { %s })(document, window);
});
console.log(JSON.stringify(out));
''' % scripts[0]
        proc = subprocess.run(
            ['node', '-e', harness, json.dumps(table), json.dumps(list(cases))],
            capture_output=True, text=True, timeout=60)
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertEqual(json.loads(proc.stdout), cases)


def have(module):
    return importlib.util.find_spec(module) is not None


class ExtensionSetupTest(unittest.TestCase):
    def test_setup_connects_the_check_to_build_finished(self):
        connected = []
        app = types.SimpleNamespace(
            connect=lambda event, handler: connected.append((event, handler)))
        meta = check_redirect.setup(app)
        self.assertEqual(connected,
                         [('build-finished', check_redirect.verify_build)])
        self.assertTrue(meta['parallel_read_safe'])
        self.assertTrue(meta['parallel_write_safe'])


@unittest.skipIf(not have('sphinx'), 'sphinx is not installed')
class ExtensionBuildTest(unittest.TestCase):
    '''The extension in a real Sphinx build of a one-page project that ships
    the shipped hpo.html, whose target pages it does not have.'''

    def setUp(self):
        self.tmp = tempfile.mkdtemp(prefix='tadah_redirect_ext_test_')
        self.addCleanup(shutil.rmtree, self.tmp)
        self.src = os.path.join(self.tmp, 'src')
        os.makedirs(os.path.join(self.src, '_extra'))
        shutil.copy(REDIRECT, os.path.join(self.src, '_extra', 'hpo.html'))
        with open(os.path.join(self.src, 'conf.py'), 'w') as fh:
            fh.write("extensions = ['check_redirect']\n"
                     "html_extra_path = ['_extra']\n")
        with open(os.path.join(self.src, 'index.rst'), 'w') as fh:
            fh.write('Tiny\n====\n\nOne page.\n')

    def build(self, builder):
        path = os.pathsep.join(p for p in (TOOLS, os.environ.get('PYTHONPATH'))
                               if p)
        return subprocess.run(
            [sys.executable, '-m', 'sphinx', '-W', '--keep-going', '-b',
             builder, self.src, os.path.join(self.tmp, builder)],
            env=dict(os.environ, PYTHONPATH=path),
            capture_output=True, text=True, timeout=300)

    def test_html_build_fails_on_a_broken_redirect(self):
        proc = self.build('html')
        self.assertNotEqual(proc.returncode, 0, proc.stderr)
        self.assertIn('hpo.html redirect: target page nfp_configuration.html '
                      'is missing from the build', proc.stderr)

    def test_other_html_builders_say_the_redirect_is_unchecked(self):
        for builder in ('dirhtml', 'singlehtml'):
            with self.subTest(builder=builder):
                proc = self.build(builder)
                self.assertTrue(os.path.isfile(
                    os.path.join(self.tmp, builder, 'hpo.html')))
                self.assertNotEqual(proc.returncode, 0, proc.stderr)
                self.assertIn("hpo.html redirect: not checked for the "
                              f"'{builder}' builder", proc.stderr)
                self.assertNotIn('is missing from the build', proc.stderr)

    def test_builder_without_the_page_is_not_reported(self):
        proc = self.build('text')
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertNotIn('hpo.html redirect', proc.stderr)


@unittest.skipIf(not (have('sphinx') and have('sphinx_rtd_theme')),
                 'sphinx or sphinx_rtd_theme is not installed')
class ConfLoadsTheExtensionTest(unittest.TestCase):
    '''docs/source/conf.py, read by Sphinx, enables the check.'''

    def test_conf_lists_check_redirect_in_extensions(self):
        tmp = tempfile.mkdtemp(prefix='tadah_redirect_conf_test_')
        self.addCleanup(shutil.rmtree, tmp)
        files = {'CORE/blueprint/blueprint_tadah.toml':
                     '[TADAH]\nversion = "0.0.1"\n',
                 'HPO/blueprint/anneal_keys.toml': '# test\n',
                 'xml/index.xml': ''}
        for rel, text in files.items():
            path = os.path.join(tmp, rel)
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, 'w') as fh:
                fh.write(text)
        # Config.read executes conf.py but not its setup(), so the page
        # generators do not run.
        code = ('import json, sys\n'
                'from sphinx.config import Config\n'
                'config = Config.read(sys.argv[1])\n'
                'print("EXTENSIONS=" + json.dumps(config.extensions))\n')
        proc = subprocess.run(
            [sys.executable, '-c', code, DOCS_SOURCE],
            env=dict(os.environ,
                     TADAH_CORE_SOURCE_DIR=os.path.join(tmp, 'CORE'),
                     TADAH_HPO_SOURCE_DIR=os.path.join(tmp, 'HPO'),
                     TADAH_DOXYGEN_XML_DIR=os.path.join(tmp, 'xml')),
            capture_output=True, text=True, timeout=120)
        self.assertEqual(proc.returncode, 0, proc.stderr)
        line = [l for l in proc.stdout.splitlines()
                if l.startswith('EXTENSIONS=')]
        self.assertEqual(len(line), 1, proc.stdout)
        self.assertIn('check_redirect',
                      json.loads(line[0][len('EXTENSIONS='):]))


@unittest.skipIf(not have('yaml'), 'PyYAML is not installed')
class ReadTheDocsSearchTest(unittest.TestCase):
    def test_search_ignores_the_redirect_and_keeps_the_defaults(self):
        import yaml
        with open(RTD_CONFIG, encoding='utf-8') as fh:
            ignore = yaml.safe_load(fh)['search']['ignore']
        # Setting 'ignore' replaces Read the Docs' default list.
        for pattern in ('hpo.html', 'search.html', 'search/index.html',
                        '404.html', '404/index.html'):
            self.assertIn(pattern, ignore)


if __name__ == '__main__':
    unittest.main()
