"""Render representative reference tables with the shipped HTML stylesheet.

Requires Sphinx, sphinx_rtd_theme and a local Chromium-family browser.
The browser reads only a temporary, locally generated documentation page.
"""
import html
from html.parser import HTMLParser
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


SOURCE = Path(__file__).resolve().parents[2] / 'source'
BROWSER = next((shutil.which(name) for name in
                ('chromium', 'chromium-browser', 'google-chrome')
                if shutil.which(name)), None)
HAVE_SPHINX = all(importlib.util.find_spec(name) for name in
                  ('sphinx', 'sphinx_rtd_theme'))


class ReportParser(HTMLParser):
    def __init__(self):
        super().__init__()
        self.recording = False
        self.report = ''

    def handle_starttag(self, tag, attrs):
        if tag == 'script' and dict(attrs).get('id') == 'layout-report':
            self.recording = True

    def handle_endtag(self, tag):
        if tag == 'script':
            self.recording = False

    def handle_data(self, data):
        if self.recording:
            self.report += data


@unittest.skipUnless(HAVE_SPHINX and BROWSER,
                     'Sphinx, sphinx_rtd_theme and Chromium are required')
class TableLayoutTest(unittest.TestCase):
    def test_reference_tables_fit_desktop_and_mobile_content(self):
        with tempfile.TemporaryDirectory(prefix='tadah_table_layout_') as tmp:
            root = Path(tmp)
            # Read the real Sphinx configuration, with small local inputs.
            inputs = {'CORE/blueprint/blueprint_tadah.toml':
                          '[TADAH]\nversion = "0.0.1"\n',
                      'HPO/blueprint/anneal_keys.toml': '# fixture\n',
                      'xml/index.xml': ''}
            for name, content in inputs.items():
                path = root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(content)
            env = dict(os.environ, TADAH_CORE_SOURCE_DIR=str(root / 'CORE'),
                       TADAH_HPO_SOURCE_DIR=str(root / 'HPO'),
                       TADAH_DOXYGEN_XML_DIR=str(root / 'xml'))
            config = subprocess.run(
                [sys.executable, '-c',
                 'import json, sys; from sphinx.config import Config; '
                 'c = Config.read(sys.argv[1]); '
                 'c.add("html_css_files", [], "html", (list, tuple)); '
                 'c.init_values(); '
                 'print("CSS=" + json.dumps(c.html_css_files))',
                 str(SOURCE)], env=env, text=True, capture_output=True,
                timeout=30)
            self.assertEqual(config.returncode, 0, config.stderr)
            css = json.loads(next(line[4:] for line in config.stdout.splitlines()
                                  if line.startswith('CSS=')))
            src = root / 'src'
            src.mkdir()
            (src / 'conf.py').write_text(
                "project = 'Table layout'\nhtml_theme = 'sphinx_rtd_theme'\n"
                f"html_static_path = {[str(SOURCE / '_static')]!r}\n"
                f"html_css_files = {css!r}\n")
            (src / 'index.rst').write_text('''Reference tables
================

.. list-table:: Grouped keys
   :header-rows: 1
   :widths: 48 52

   * - Keys
     - Description
   * - ``PC_SCRIPT_TIMEOUT``, ``PC_ISOLATION``, ``PC_WORKER_TIMEOUT``
     - Script execution controls and the maximum time allowed for each worker.
   * - ``OPTIM ta.embedding.envelope.cutoff (1) 1.0 3.0``
     - Bounds in the selected component coordinates.

.. list-table:: Algorithm parameters
   :header-rows: 1
   :widths: 25 15 15 45

   * - Parameter
     - Type
     - Default
     - Description
   * - ``INIT_JITTER_MAXTIME_FRAC``
     - real
     - ``0.10``
     - Fraction of the time budget available for automatic jitter calibration.
   * - ``INIT_JITTER_PROBES``
     - integer
     - ``64``
     - Calibration probes for the initial distribution of replica states.

Archive path: ``runs/parameter_search/validation_candidates/selected_components/best_pot_<improvement>.tadah``.

.. code-block:: none

   OPTIMIZER
     MAXEVAL 1000
   ENDOPTIMIZER
''')
            out = root / 'html'
            build = subprocess.run(
                [sys.executable, '-m', 'sphinx', '-W', '-b', 'html',
                 str(src), str(out)], capture_output=True, text=True, timeout=60)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            page = out / 'index.html'
            measure = '''<script>
window.addEventListener('load', function () { setTimeout(function () {
  const tables = Array.from(document.querySelectorAll('.wy-table-responsive'));
  const report = {
    width: window.innerWidth,
    tables: tables.map(el => ({client: el.clientWidth, scroll: el.scrollWidth})),
    paragraphs: Array.from(document.querySelectorAll('.rst-content p')).map(
      el => ({client: el.clientWidth, scroll: el.scrollWidth})),
    content: document.querySelector('.rst-content').getBoundingClientRect().width,
    tableWidths: tables.map(el => el.querySelector('table').getBoundingClientRect().width),
    codeWhitespace: getComputedStyle(document.querySelector('.highlight pre')).whiteSpace,
    codeWrap: getComputedStyle(document.querySelector('.highlight pre')).overflowWrap
  };
  const el = document.createElement('script');
  el.id = 'layout-report'; el.type = 'application/json';
  el.textContent = JSON.stringify(report); document.body.appendChild(el);
}, 100); });
</script>'''
            page.write_text(page.read_text().replace('</body>', measure + '</body>'))
            for width in (1280, 900, 520):
                with self.subTest(viewport=width):
                    result = subprocess.run(
                        [BROWSER, '--headless', '--no-sandbox', '--disable-gpu',
                         '--disable-background-networking', '--no-first-run',
                         '--disable-dev-shm-usage', '--no-proxy-server',
                         f'--user-data-dir={root / ("profile-" + str(width))}',
                         f'--window-size={width},900', '--virtual-time-budget=1000',
                         '--dump-dom', page.as_uri()],
                        capture_output=True, text=True, timeout=30)
                    self.assertEqual(result.returncode, 0, result.stderr)
                    parser = ReportParser()
                    parser.feed(result.stdout)
                    self.assertTrue(parser.report, result.stderr)
                    report = json.loads(html.unescape(parser.report))
                    self.assertEqual(len(report['tables']), 2)
                    for table, table_width in zip(report['tables'], report['tableWidths']):
                        self.assertLessEqual(table['scroll'], table['client'] + 1,
                                             report)
                        self.assertLessEqual(table_width, report['content'] + 1,
                                             report)
                    # Code lines are kept whole on desktop windows (1024 px
                    # and wider) and wrap on narrower ones.
                    if width >= 1024:
                        self.assertEqual((report['codeWhitespace'],
                                          report['codeWrap']), ('pre', 'normal'))
                    else:
                        self.assertEqual((report['codeWhitespace'],
                                          report['codeWrap']),
                                         ('pre-wrap', 'anywhere'))
                    for paragraph in report['paragraphs']:
                        self.assertLessEqual(paragraph['scroll'], paragraph['client'] + 1,
                                             report)


if __name__ == '__main__':
    unittest.main()
