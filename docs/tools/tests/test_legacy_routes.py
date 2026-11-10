"""Tests for redirects generated from one TOML table."""
import ast
import copy
import fnmatch
import hashlib
import html.parser
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import types
import unittest

TOOLS = Path(__file__).resolve().parents[1]
CONF = TOOLS.parent / 'source' / 'conf.py'
sys.path.insert(0, str(TOOLS))
import legacy_routes  # noqa: E402


TABLE = {
    'version': 1,
    'pages': {
        'hpo.html': {
            'default': 'nfp/overview.html#nested-fitting',
            'title': 'Nested Fitting',
            'anchors': {
                'old-name': 'nfp/target_file.html#nfp-target-file',
                '__proto__': 'nfp/target_file.html#nfp-target-file',
                'toString': 'nfp/target_file.html',
            },
        },
        'hpo_examples.html': {
            'default': 'nfp/examples/index.html#nfp-examples',
            'title': 'Nested Fitting Examples',
            'anchors': {},
        },
    },
}


def toml_table(table):
    lines = [f'version = {json.dumps(table["version"])}']
    for source, entry in table['pages'].items():
        lines.extend([f'\n[pages.{json.dumps(source)}]',
                      f'default = {json.dumps(entry["default"])}',
                      f'title = {json.dumps(entry["title"])}',
                      f'\n[pages.{json.dumps(source)}.anchors]'])
        for anchor, target in entry['anchors'].items():
            lines.append(f'{json.dumps(anchor)} = {json.dumps(target)}')
    return '\n'.join(lines) + '\n'


class ParsedPage(html.parser.HTMLParser):
    def __init__(self, text):
        super().__init__()
        self.meta = []
        self.links = []
        self.hrefs = []
        self.scripts = []
        self.current = None
        self.feed(text)

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if tag == 'meta':
            self.meta.append(attrs)
        elif tag == 'link':
            self.links.append(attrs)
        elif tag == 'a':
            self.hrefs.append(attrs.get('href'))
        elif tag == 'script':
            self.scripts.append([attrs, ''])
            self.current = self.scripts[-1]

    def handle_endtag(self, tag):
        if tag == 'script':
            self.current = None

    def handle_data(self, data):
        if self.current is not None:
            self.current[1] += data


class RoutesFixture(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix='tadah_routes_')
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.out = self.root / 'html'
        self.out.mkdir()
        self.routes = self.root / 'routes.toml'
        self.table = copy.deepcopy(TABLE)
        self.save()
        self.write('nfp/overview.html', '<section id="nested-fitting"></section>')
        self.write('nfp/target_file.html',
                   '<section id="nfp-target-file"></section>')
        self.write('nfp/examples/index.html',
                   '<section id="nfp-examples"></section>')

    def save(self):
        self.routes.write_text(toml_table(self.table), encoding='utf-8')

    def write(self, name, text):
        path = self.out / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding='utf-8')


class RoutesTest(RoutesFixture):
    def test_loads_one_table_for_pages_and_anchors(self):
        self.assertEqual(legacy_routes.load_routes(self.routes), self.table)

    def test_rejects_invalid_schema_and_toml(self):
        invalid = [
            '', 'version = 1\n',
            'version = true\n[pages]\n',
            'version = 2\n[pages]\n',
            'version = 1\n[pages]\n',
            toml_table(TABLE) + '\n[unknown]\n',
            toml_table(TABLE).replace('version = 1', 'schema = 1'),
            toml_table(TABLE).replace('default = ', 'destination = ', 1),
            toml_table(TABLE).replace('default = ', 'default = 1\nunused = ', 1),
            toml_table(TABLE).replace('"old-name" = ', '"old-name" = 1\nx = ', 1),
            toml_table(TABLE).replace('"old-name" = ', '"" = ', 1),
            toml_table(TABLE) + '\n[pages."hpo.html"]\n',
            'version = 1\npages = []\n',
            'version = 1\n[pages]\n"hpo.html" = []\n',
            'version = 1\n[pages."hpo.html"]\ndefault = "ok.html"\n',
            'version = 1\n[pages."hpo.html"]\n'
            'default = "ok.html"\nanchors = []\n',
            # A page without its title, or with a title that is not one
            # line of text. Before the fix: the first loaded (a page had
            # no title) and the others were refused for an unknown field.
            toml_table(TABLE).replace('title = "Nested Fitting"\n', '', 1),
            toml_table(TABLE).replace('title = "Nested Fitting"',
                                      'title = 1', 1),
            toml_table(TABLE).replace('title = "Nested Fitting"',
                                      'title = ""', 1),
            toml_table(TABLE).replace('title = "Nested Fitting"',
                                      'title = "Nested\\nFitting"', 1),
        ]
        for text in invalid:
            with self.subTest(text=text):
                self.routes.write_text(text, encoding='utf-8')
                with self.assertRaises(ValueError):
                    legacy_routes.load_routes(self.routes)

    def test_rejects_unsafe_sources_defaults_and_targets(self):
        invalid = ('../escape.html', '/escape.html', '//example.org/a.html',
                   'https://example.org/a.html', 'javascript:alert(1)',
                   'file:///escape.html', 'a\\b.html', 'a//b.html',
                   './a.html', 'a/../b.html', 'a/%2e%2e/b.html',
                   'a.html?query=1', 'a.html#', 'a.html#a#b',
                   'a.html#bad%20id', 'a.html#bad id', 'a.html\n',
                   'a.html\x00', 'a.htm', 'a.html/other.html',
                   'a.html#</script>', 'a.html#é')
        for value in invalid:
            for location in ('source', 'default', 'anchor'):
                with self.subTest(value=value, location=location):
                    self.table = copy.deepcopy(TABLE)
                    if location == 'source':
                        entry = self.table['pages'].pop('hpo.html')
                        self.table['pages'][value] = entry
                    elif location == 'default':
                        self.table['pages']['hpo.html']['default'] = value
                    else:
                        self.table['pages']['hpo.html']['anchors']['old'] = value
                    self.save()
                    with self.assertRaises(ValueError):
                        legacy_routes.load_routes(self.routes)

    def test_rejects_redirect_chains_and_self_redirects(self):
        for value in ('hpo.html', 'hpo.html#old-name', 'hpo_examples.html'):
            with self.subTest(value=value):
                self.table['pages']['hpo.html']['anchors']['old-name'] = value
                self.save()
                with self.assertRaisesRegex(ValueError, 'redirect'):
                    legacy_routes.load_routes(self.routes)

    def test_rejects_unsafe_anchor_names(self):
        for value in ('', '#name', 'bad name', 'bad%20name', '</script>', '\n'):
            with self.subTest(value=value):
                self.table = copy.deepcopy(TABLE)
                self.table['pages']['hpo.html']['anchors'][value] = 'ok.html'
                self.save()
                with self.assertRaises(ValueError):
                    legacy_routes.load_routes(self.routes)

    def test_nested_source_paths_are_relative_to_the_source_page(self):
        self.table['pages']['old/hpo.html'] = self.table['pages'].pop('hpo.html')
        self.save()
        legacy_routes.generate(self.out, self.routes)
        parsed = ParsedPage((self.out / 'old/hpo.html').read_text())
        self.assertIn('../nfp/overview.html#nested-fitting', parsed.hrefs)
        table = json.loads(parsed.scripts[0][1])
        self.assertEqual(table['routes']['old-name'],
                         '../nfp/target_file.html#nfp-target-file')
        self.assertEqual(legacy_routes.check(self.out, self.routes), [])

    def test_validates_default_and_every_anchor_target(self):
        self.assertEqual(legacy_routes.validate_targets(self.out, self.table), [])
        self.write('nfp/overview.html', '<section id="elsewhere"></section>')
        self.write('nfp/target_file.html', '<section id="elsewhere"></section>')
        problems = legacy_routes.validate_targets(self.out, self.table)
        self.assertTrue(any('nested-fitting' in p for p in problems), problems)
        self.assertTrue(any('nfp-target-file' in p for p in problems), problems)
        (self.out / 'nfp/examples/index.html').unlink()
        problems = legacy_routes.validate_targets(self.out, self.table)
        self.assertTrue(any('nfp/examples/index.html' in p and 'missing' in p
                            for p in problems), problems)

    def test_invalid_targets_leave_all_outputs_unchanged(self):
        self.write('hpo.html', 'existing redirect')
        (self.out / 'nfp/examples/index.html').unlink()
        with self.assertRaisesRegex(ValueError, 'missing'):
            legacy_routes.generate(self.out, self.routes)
        self.assertEqual((self.out / 'hpo.html').read_text(), 'existing redirect')
        self.assertFalse((self.out / 'hpo_examples.html').exists())

    def test_refuses_target_symlink_escape(self):
        outside = self.root / 'outside.html'
        outside.write_text('<section id="nfp-target-file"></section>')
        target = self.out / 'nfp/target_file.html'
        target.unlink()
        target.symlink_to(outside)
        with self.assertRaisesRegex(ValueError, 'outside|escape'):
            legacy_routes.generate(self.out, self.routes)
        self.assertFalse((self.out / 'hpo.html').exists())

    def test_refuses_symlinked_output_and_parent(self):
        outside = self.root / 'outside.html'
        outside.write_text('untouched')
        (self.out / 'hpo_examples.html').symlink_to(outside)
        with self.assertRaisesRegex(ValueError, 'symlink|symbolic'):
            legacy_routes.generate(self.out, self.routes)
        self.assertEqual(outside.read_text(), 'untouched')
        self.assertFalse((self.out / 'hpo.html').exists())
        (self.out / 'hpo_examples.html').unlink()
        self.table['pages']['old/hpo.html'] = self.table['pages'].pop('hpo.html')
        self.save()
        (self.out / 'old').symlink_to(self.root, target_is_directory=True)
        with self.assertRaisesRegex(ValueError, 'symlink|symbolic|outside'):
            legacy_routes.generate(self.out, self.routes)
        self.assertFalse((self.out / 'hpo_examples.html').exists())

    def test_refuses_target_alias_to_a_redirect_output(self):
        self.write('hpo.html', '<section id="nested-fitting"></section>')
        target = self.out / 'nfp/overview.html'
        target.unlink()
        target.symlink_to(self.out / 'hpo.html')
        with self.assertRaisesRegex(ValueError, 'redirect'):
            legacy_routes.generate(self.out, self.routes)

    def test_generation_is_byte_deterministic_and_checkable(self):
        self.assertEqual(legacy_routes.generate(self.out, self.routes),
                         ['hpo.html', 'hpo_examples.html'])
        before = {p: (self.out / p).read_bytes() for p in TABLE['pages']}
        self.table['pages'] = dict(reversed(list(self.table['pages'].items())))
        entry = self.table['pages']['hpo.html']
        entry['anchors'] = dict(reversed(list(entry['anchors'].items())))
        self.save()
        legacy_routes.generate(self.out, self.routes)
        self.assertEqual(before, {p: (self.out / p).read_bytes() for p in before})
        self.assertEqual(legacy_routes.check(self.out, self.routes), [])

    def test_generated_page_has_fallback_and_search_metadata(self):
        legacy_routes.generate(self.out, self.routes)
        for source, entry in TABLE['pages'].items():
            with self.subTest(source=source):
                text = (self.out / source).read_text()
                page = ParsedPage(text)
                self.assertIn(entry['default'], page.hrefs)
                self.assertIn({'rel': 'canonical', 'href': entry['default']},
                              page.links)
                self.assertIn({'name': 'robots', 'content': 'noindex'}, page.meta)
                self.assertIn({'http-equiv': 'refresh',
                               'content': '0; url=' + entry['default']}, page.meta)
                self.assertIn('<noscript>', text)
                self.assertEqual(json.loads(page.scripts[0][1]),
                                 {'default': entry['default'],
                                  'routes': entry['anchors']})
                self.assertNotRegex(text.lower(), r'\b(you|your|we|our)\b')
                # Pins: the page's title, heading and visible link name the
                # page of its entry's title. Before the fix: every page
                # was titled Nested Fitting, whatever it led to.
                self.assertIn(f'<title>{entry["title"]} — Tadah! '
                              'documentation</title>', text)
                self.assertIn(f'<h1>{entry["title"]}</h1>', text)
                self.assertIn(f'<a href="{entry["default"]}">'
                              f'{entry["title"]}</a>', text)

    def test_title_is_escaped(self):
        self.table['pages']['hpo.html']['title'] = 'A <b> & "C"'
        self.save()
        legacy_routes.generate(self.out, self.routes)
        text = (self.out / 'hpo.html').read_text()
        self.assertIn('<h1>A &lt;b&gt; &amp; &quot;C&quot;</h1>', text)
        self.assertNotIn('<b>', text)

    def test_check_detects_missing_or_changed_output_and_targets(self):
        problems = legacy_routes.check(self.out, self.routes)
        self.assertTrue(any('hpo.html' in p and 'missing' in p for p in problems))
        legacy_routes.generate(self.out, self.routes)
        self.write('hpo.html', (self.out / 'hpo.html').read_text().replace(
            'window.location.replace', 'window.location.assign'))
        problems = legacy_routes.check(self.out, self.routes)
        self.assertTrue(any('hpo.html' in p for p in problems), problems)
        (self.out / 'nfp/examples/index.html').unlink()
        self.assertTrue(legacy_routes.check(self.out, self.routes))

    def test_check_returns_errors_for_invalid_tables(self):
        self.routes.write_text('version = [')
        self.assertTrue(legacy_routes.check(self.out, self.routes))

    @unittest.skipUnless(shutil.which('node'), 'node is not installed')
    def test_javascript_resolves_all_known_hashes_without_prototype_lookup(self):
        legacy_routes.generate(self.out, self.routes)
        for source, entry in TABLE['pages'].items():
            with self.subTest(source=source):
                page = ParsedPage((self.out / source).read_text())
                routes = page.scripts[0][1]
                script = page.scripts[1][1]
                cases = {'': entry['default'], '#unknown': entry['default'],
                         '#%': entry['default'], '#__proto__': entry['default'],
                         '#toString': entry['default']}
                cases.update({'#' + k: v for k, v in entry['anchors'].items()})
                if 'old-name' in entry['anchors']:
                    cases['#old%2Dname'] = entry['anchors']['old-name']
                harness = '''
const table = process.argv[1], hashes = JSON.parse(process.argv[2]);
const out = {};
for (const hash of hashes) {
  const document = {getElementById: () => ({textContent: table})};
  const window = {location: {hash, replace: target => {out[hash] = target;}}};
  (function (document, window) { %s })(document, window);
}
console.log(JSON.stringify(out));
''' % script
                result = subprocess.run(
                    ['node', '-e', harness, routes, json.dumps(list(cases))],
                    capture_output=True, text=True, timeout=60)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(json.loads(result.stdout), cases)

    def test_command_line_generate_check_and_refusal(self):
        command = [sys.executable, str(TOOLS / 'legacy_routes.py'), str(self.out),
                   '--routes', str(self.routes)]
        result = subprocess.run(command, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        result = subprocess.run(command + ['--check'],
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.routes.write_text('version = [')
        before = (self.out / 'hpo.html').read_bytes()
        result = subprocess.run(command, capture_output=True, text=True)
        self.assertEqual(result.returncode, 1)
        self.assertNotIn('Traceback', result.stderr)
        self.assertEqual((self.out / 'hpo.html').read_bytes(), before)


# The ids of the command sections of cli/cli_auto_gen.html, the CLI page the
# releases 1.0.0, 1.1.1, 1.3.0-beta.1 and 1.5.0 published beside
# cli/cli.html, which includes it: every id of its article body, built with
# Sphinx 7.4.7 from generate_cli.py and CORE's blueprint of each release,
# merged (refit is a command of 1.5.0 only).
RELEASED_CLI_IDS = (
    'analysis', 'analysis-bfunc', 'analysis-check-bounds', 'analysis-cutoff',
    'analysis-dataset-stats', 'analysis-descriptor', 'analysis-suggest-bounds',
    'data', 'data-balance', 'data-convert', 'data-dedup', 'data-merge',
    'data-print', 'data-sample', 'data-split', 'data-write', 'explain', 'hpo',
    'predict', 'properties', 'properties-pairwise', 'refit', 'train')
# The ids of hpo_examples.html as 1.3.0-beta.1 published it.
RELEASED_EXAMPLES_IDS = ('how-to-run-one-example', 'how-to-run-them-all',
                         'hpo-examples', 'index',
                         'nested-fitting-worked-examples')
# The pages of the Nested Fitting menu of 1.5.0 besides hpo.html and
# hpo_examples.html, which the site does not build: every id of each
# article body, built as above (no numbered ids), with the default and the
# title of each page's entry.
RELEASED_150_PAGES = {
    'optimizer.html': (
        'nfp/choosing/index.html#nfp-choosing', 'Choosing an Optimiser',
        ('anneal-vocabulary', 'hpo-optimizer', 'optimizer',
         'population-annealing-anneal-pa',
         'scaling-steps-and-tolerances-by-backend', 'shared-vocabulary',
         'tadah-anneal-pa-block', 'term-chain', 'term-epoch', 'term-eval',
         'term-exchange-round', 'term-pair', 'term-proposal', 'term-replica',
         'term-rung', 'term-slot', 'term-sweep', 'term-walker',
         'term-window')),
    'anneal.html': (
        'nfp/choosing/annealing.html#nfp-annealing', 'Annealing Optimisers',
        ('anneal-keys', 'automatic-chain-length',
         'cycle-lengths-and-the-return-to-the-best', 'key-reference', 'mpi',
         'one-coordinate-at-a-time', 're-annealing', 'reading-hpo-run-log',
         'see-also', 'simulated-annealing-anneal', 'state-and-objective',
         'stops', 'tadah-anneal-block', 'tadah-anneal-corana',
         'tadah-anneal-journal', 'the-acceptance-rule',
         'the-classical-algorithm', 'the-cooling-schedule',
         'the-corana-variant', 'the-proposal', 'the-starting-temperature',
         'the-step-adaptation', 'when-to-prefer-which-kernel')),
    'anneal_pt.html': (
        'nfp/choosing/annealing.html#nfp-annealing-replicas',
        'Annealing Optimisers',
        ('anneal-pt-guide', 'anneal-pt-init-jitter-ladder',
         'anneal-pt-journal', 'anneal-pt-reference', 'determinism',
         'key-reference', 'ladder-adaptation', 'parallel-tempering-anneal-pt',
         'part-a-reference', 'part-b-setting-up-a-run', 'reading-hpo-run-log',
         'replica-initialisation', 'see-also', 'step-modes', 'stopping',
         'tadah-anneal-pt-block', 'the-temperature-ladder',
         'the-tempering-loop', 'two-pass-temperature-calibration')),
    'asa.html': (
        'nfp/choosing/asa.html#nfp-asa', 'ASA',
        ('adaptive-simulated-annealing-asa', 'asa-optimizer', 'background',
         'deviations-from-upstream-defaults', 'journal-output', 'options',
         'parallelism', 'quick-start', 'see-also', 'stops-honoured',
         'troubleshooting-tuning')),
}


class ShippedTableTest(unittest.TestCase):
    # Pins: the table covers the pages of the released site that the site
    # no longer builds: hpo.html, hpo_examples.html, the CLI page
    # cli/cli_auto_gen.html, which the CLI reference includes and the site
    # builds no more as a page of its own, and the pages optimizer.html,
    # anneal.html, anneal_pt.html and asa.html of 1.5.0, each with every
    # id it had, its default and its title. Each command section of the
    # CLI page, refit of 1.5.0 included, leads to the label of that command
    # on cli/cli.html (cli_<words>, id cli-<words>), the page to its
    # commands reference.
    # Before the fix: the table had none of the four pages of 1.5.0, and no
    # refit anchor for cli/cli_auto_gen.html (cli_auto_gen.html#refit led to
    # the commands reference).
    def test_covers_every_page_the_site_no_longer_builds(self):
        table = legacy_routes.load_routes()['pages']
        self.assertEqual(set(table), {'hpo.html', 'hpo_examples.html',
                                      'cli/cli_auto_gen.html',
                                      *RELEASED_150_PAGES})
        cli = table['cli/cli_auto_gen.html']
        self.assertEqual(cli['default'], 'cli/cli.html#cli-commands')
        self.assertEqual(cli['title'], 'Command Line Interface')
        self.assertEqual(cli['anchors'], {
            anchor: f'cli/cli.html#cli-{anchor}' for anchor in RELEASED_CLI_IDS})
        self.assertEqual(set(table['hpo_examples.html']['anchors']),
                         set(RELEASED_EXAMPLES_IDS))
        self.assertEqual(table['hpo.html']['title'], 'Nested Fitting')
        self.assertEqual(table['hpo_examples.html']['title'],
                         'Nested Fitting Examples')
        for page, (default, title, ids) in RELEASED_150_PAGES.items():
            with self.subTest(page=page):
                self.assertEqual(table[page]['default'], default)
                self.assertEqual(table[page]['title'], title)
                self.assertEqual(set(table[page]['anchors']), set(ids))

    def test_covers_the_recorded_anchor_inventory_without_numbered_ids(self):
        table = legacy_routes.load_routes()
        anchors = table['pages']['hpo.html']['anchors']
        # The digest checks the inventory independently of its destinations.
        digest = hashlib.sha256(('\n'.join(sorted(anchors)) + '\n').encode())
        self.assertEqual(len(anchors), 83)
        self.assertEqual(digest.hexdigest(),
                         '5c0024e1e47b3eb7f1f89bde1912e2f7'
                         'aaa802dfa56f77ca5664910f59c7bfd0')
        self.assertFalse(any(re.fullmatch(r'id\d+', key) for key in anchors))

    # Pins: an anchor of a released page leads to the section of the
    # chapter, of Installation or of Training that holds its topic, by the
    # section's label, or to the entry of a reference page by its id; an
    # anchor without a section of its own leads to the top of its page.
    # The subsections of 1.5.0's "Failure handling" lead to the section on
    # failed evaluations.
    # Before the fix: every hpo.html anchor but those of Installation and
    # Training led to the top of a page (loss-functions to
    # nfp/targets_losses.html#nfp-targets, hpo-fixweight to
    # nfp/search_space.html#nfp-search-space, noise-block to
    # nfp/optimizer_block.html#nfp-optimizer-block, corana-adaptive-variant-
    # opt-in to nfp/choosing/annealing.html#nfp-annealing,
    # result-contract-for-user-scripts to
    # nfp/reference/pc_options.html#nfp-ref-pc-options), the-knobs-most-
    # important-first to nfp/choosing/index.html#nfp-choosing and
    # what-problem-this-solves to nfp/overview.html#nested-fitting; the
    # pages of 1.5.0 had no entries.
    def test_representative_topics_reach_their_owner_sections(self):
        table = legacy_routes.load_routes()['pages']
        expected = {
            ('hpo.html', 'nested-fitting'): 'nfp/overview.html#nested-fitting',
            ('hpo.html', 'quick-start'): 'nfp/quickstart.html#nfp-quickstart',
            ('hpo.html', 'hpo-manual'): 'nfp/target_file.html#nfp-target-file',
            ('hpo.html', 'block-hierarchy'):
                'nfp/target_file.html#nfp-target-file-blocks',
            ('hpo.html', 'loss-functions'):
                'nfp/targets_losses.html#nfp-targets-function',
            ('hpo.html', 'failscore-grading'):
                'nfp/targets_losses.html#nfp-targets-failures',
            ('hpo.html', 'the-knobs-most-important-first'):
                'nfp/targets_losses.html#nfp-targets-failures',
            ('hpo.html', 'what-problem-this-solves'):
                'nfp/targets_losses.html#nfp-targets-failures',
            ('hpo.html', 'hpo-fixweight'):
                'nfp/search_space.html#nfp-search-space-coefficients',
            ('hpo.html', 'noise-block'):
                'nfp/optimizer_block.html#nfp-optimizer-block-noise',
            ('hpo.html', 'restart-decorator'):
                'nfp/optimizer_block.html#nfp-optimizer-block-restart',
            ('hpo.html', 'best-improvement-histories'):
                'nfp/output_files.html#nfp-output-histories',
            ('hpo.html', 'corana-adaptive-variant-opt-in'):
                'nfp/choosing/annealing.html#nfp-annealing-steps',
            ('hpo.html', 'mpi-build'): 'installation.html#installation-hpo-mpi',
            ('hpo.html', 'hpo-load-transforms'): 'training.html#load-transforms',
            ('hpo.html', 'validation-set-sanitisation'):
                'training.html#nfp-training-hpo-validation',
            ('hpo.html', 'result-contract-for-user-scripts'):
                'nfp/reference/pc_options.html#ROOT.PC_LAMMPS',
            ('hpo.html', 'ceres-param-keys'):
                'nfp/reference/optimisers.html#PARAM.CERES',
            ('optimizer.html', 'term-replica'):
                'nfp/choosing/annealing.html#nfp-annealing-terms',
            ('optimizer.html', 'scaling-steps-and-tolerances-by-backend'):
                'nfp/choosing/index.html#nfp-choosing-step',
            ('anneal.html', 'the-cooling-schedule'):
                'nfp/choosing/annealing.html#nfp-annealing-temperatures',
            ('anneal.html', 'anneal-keys'):
                'nfp/reference/anneal_params.html#nfp-ref-anneal-params',
            ('anneal.html', 'reading-hpo-run-log'):
                'nfp/output_files.html#nfp-output-histories',
            ('anneal_pt.html', 'replica-initialisation'):
                'nfp/choosing/annealing.html#nfp-annealing-initialisation',
            ('anneal_pt.html', 'part-b-setting-up-a-run'):
                'nfp/choosing/annealing.html#nfp-annealing-recipe',
            ('asa.html', 'options'):
                'nfp/reference/asa_params.html#nfp-ref-asa-params',
            ('asa.html', 'stops-honoured'): 'nfp/choosing/asa.html#nfp-asa-stopping',
        }
        for (page, key), value in expected.items():
            with self.subTest(page=page, key=key):
                self.assertEqual(table[page]['anchors'][key], value)
        self.assertEqual(table['hpo_examples.html']['default'],
                         'nfp/examples/index.html#nfp-examples')

    # Pins: Read the Docs leaves every page of the table out of its search
    # index (.readthedocs.yaml, search.ignore), as each only leads to
    # another page.
    # Before the fix: passes as well (a pin: the list names hpo.html,
    # hpo_examples.html and cli/cli_auto_gen.html, the pages the table had);
    # with the four pages of 1.5.0 in the table and not in the list, it
    # fails for each of them.
    def test_read_the_docs_search_leaves_out_every_page_of_the_table(self):
        text = (TOOLS.parents[1] / '.readthedocs.yaml').read_text(
            encoding='utf-8')
        block = re.search(r'^search:\n((?:[ \t]+.*\n|\n)*)', text, re.M)
        self.assertIsNotNone(block, 'no search section')
        ignored = set(re.findall(r'^[ \t]+-[ \t]+(\S+)[ \t]*$',
                                 block.group(1), re.M))
        for page in legacy_routes.load_routes()['pages']:
            with self.subTest(page=page):
                self.assertIn(page, ignored)


def conf_values(*names):
    '''The values that docs/source/conf.py assigns to *names* at its top
    level, as literals (a name it does not assign is absent).'''
    tree = ast.parse(CONF.read_text(encoding='utf-8'))
    values = {}
    for node in tree.body:
        if isinstance(node, ast.Assign):
            for target in node.targets:
                if isinstance(target, ast.Name) and target.id in names:
                    values[target.id] = ast.literal_eval(node.value)
    return values


class SiteConfigurationTest(unittest.TestCase):
    # Pins: the site's configuration (docs/source/conf.py) generates the
    # compatibility pages from the shipped table, with the extension and its
    # default table, and nothing else writes them: no page of the table is
    # copied by html_extra_path, and none is a source page that the build
    # would write and the extension replace (hpo_examples.rst, or the
    # generated cli/cli_auto_gen.rst unless exclude_patterns leaves it out).
    # Before the fix: conf.py listed check_redirect, not legacy_routes, and
    # html_extra_path copied _extra/hpo.html, a page of its own with its own
    # routes; hpo_examples.rst was a page of the site.
    def test_compatibility_pages_come_from_the_shipped_table_only(self):
        values = conf_values('extensions', 'legacy_routes_file',
                             'html_extra_path', 'exclude_patterns')
        self.assertIn('legacy_routes', values['extensions'])
        self.assertNotIn('check_redirect', values['extensions'])
        self.assertNotIn('legacy_routes_file', values)
        self.assertEqual(Path(legacy_routes.DEFAULT_ROUTES),
                         TOOLS / 'legacy_routes.toml')
        source = CONF.parent
        excluded = values.get('exclude_patterns', [])
        for page in legacy_routes.load_routes()['pages']:
            with self.subTest(page=page):
                for extra in values.get('html_extra_path', []):
                    self.assertFalse((source / extra / page).exists())
                stem = page.removesuffix('.html')
                for suffix in ('.rst', '.md'):
                    name = stem + suffix
                    if (source / name).exists():
                        self.assertTrue(any(fnmatch.fnmatch(name, pattern)
                                            for pattern in excluded), name)


class PythonVersionTest(unittest.TestCase):
    # Pins: without tomllib (Python 3.10 and older) the module -- which the
    # site's configuration loads as an extension before conf.py's setup()
    # runs the page generators -- exits non-zero as it is imported, with a
    # message that it needs Python 3.11 or later, that it reads its routes
    # table with tomllib, and which Python it is; no traceback, nothing
    # written to the working directory.
    # Before the fix: the import failed with a traceback, "ModuleNotFoundError:
    # No module named 'tomllib'", which Sphinx reports as "Could not import
    # extension legacy_routes".
    def test_python_without_tomllib_stops_with_a_clear_message(self):
        code = ("import sys\n"
                "sys.modules['tomllib'] = None\n"
                f"sys.path.insert(0, {str(TOOLS)!r})\n"
                "import legacy_routes\n")
        with tempfile.TemporaryDirectory() as tmp:
            result = subprocess.run(
                [sys.executable, '-c', code], cwd=tmp, capture_output=True,
                text=True, env=dict(os.environ, PYTHONDONTWRITEBYTECODE='1'))
            self.assertNotEqual(result.returncode, 0,
                                result.stdout + result.stderr)
            self.assertIn('legacy_routes.py needs Python 3.11 or later: it '
                          'reads docs/tools/legacy_routes.toml with tomllib, '
                          'which Python %d.%d does not have'
                          % sys.version_info[:2], result.stderr)
            self.assertIn(sys.executable, result.stderr)
            self.assertNotIn('Traceback', result.stderr)
            self.assertEqual(os.listdir(tmp), [])


class ExtensionTest(RoutesFixture):
    def test_setup_registers_one_build_finished_handler(self):
        connected, config = [], []
        app = types.SimpleNamespace(
            connect=lambda *args: connected.append(args),
            add_config_value=lambda *args: config.append(args))
        meta = legacy_routes.setup(app)
        self.assertEqual(connected,
                         [('build-finished', legacy_routes.generate_build)])
        self.assertEqual(config[0][0], 'legacy_routes_file')
        self.assertTrue(meta['parallel_read_safe'])
        self.assertTrue(meta['parallel_write_safe'])

    def test_failed_build_does_not_generate(self):
        app = types.SimpleNamespace()
        legacy_routes.generate_build(app, RuntimeError('failed build'))
        self.assertFalse((self.out / 'hpo.html').exists())


@unittest.skipUnless(importlib.util.find_spec('sphinx'), 'sphinx is not installed')
class SphinxBuildTest(RoutesFixture):
    def build(self, builder='html'):
        src = self.root / 'src'
        src.mkdir(exist_ok=True)
        (src / 'conf.py').write_text(
            "extensions = ['legacy_routes']\n"
            "project = 'Routes'\nversion = '1'\ncopyright = 'Routes'\n"
            f'legacy_routes_file = {str(self.routes)!r}\n')
        (src / 'index.rst').write_text(
            'Home\n====\n\n.. toctree::\n\n   nfp/overview\n'
            '   nfp/target_file\n   nfp/examples/index\n')
        for page, label in (('nfp/overview', 'nested_fitting'),
                            ('nfp/target_file', 'nfp_target_file'),
                            ('nfp/examples/index', 'nfp_examples')):
            path = src / (page + '.rst')
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(f'.. _{label}:\n\nTitle\n=====\n\nText.\n')
        return subprocess.run(
            # The doctrees stay out of the output, which an EPUB packs.
            [sys.executable, '-m', 'sphinx', '-W', '--keep-going', '-b', builder,
             '-d', str(self.root / 'doctrees' / builder),
             str(src), str(self.root / 'build' / builder)],
            capture_output=True, text=True, timeout=120,
            env=dict(os.environ, PYTHONPATH=os.pathsep.join(
                filter(None, (str(TOOLS), os.environ.get('PYTHONPATH'))))))

    def test_html_build_generates_checked_redirects(self):
        result = self.build()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(legacy_routes.check(self.root / 'build/html', self.routes),
                         [])

    def test_html_build_fails_on_a_missing_target_anchor(self):
        self.table['pages']['hpo.html']['anchors']['absent'] = (
            'nfp/target_file.html#missing-section')
        self.save()
        result = self.build()
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn('missing-section', result.stdout + result.stderr)
        self.assertFalse((self.root / 'build/html/hpo.html').exists())

    # Pins: the HTML builders other than html (dirhtml, singlehtml, epub)
    # build, write no compatibility page and say so: their output does not
    # have the flat page paths of the released site, which only the html
    # builder writes.
    # Before the fix: each of them stopped the build with "compatibility
    # redirects need the flat html builder; '<builder>' is not supported".
    def test_other_html_builders_write_no_compatibility_pages(self):
        for builder in ('dirhtml', 'singlehtml', 'epub'):
            with self.subTest(builder=builder):
                result = self.build(builder)
                output = result.stdout + result.stderr
                self.assertEqual(result.returncode, 0, output)
                self.assertIn('legacy_routes: the html builder writes the '
                              'compatibility pages; ' + repr(builder) +
                              ' writes none', output)
                for page in self.table['pages']:
                    self.assertFalse(
                        (self.root / 'build' / builder / page).exists(), page)

    def test_non_html_build_does_not_generate_redirects(self):
        result = self.build('text')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertFalse((self.root / 'build/text/hpo.html').exists())


if __name__ == '__main__':
    unittest.main()
