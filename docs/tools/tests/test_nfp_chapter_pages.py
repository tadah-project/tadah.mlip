"""The chapter's topic pages keep stable labels and hub-only navigation."""
from pathlib import Path
import re
import tomllib
import unittest


SOURCE = Path(__file__).resolve().parents[2] / 'source'
ROUTES = Path(__file__).resolve().parents[1] / 'legacy_routes.toml'
# The Nested Fitting menu of index.rst, in the order of the reader's
# decisions: the overview, the quick start, the tutorial and examples hubs,
# the guides, and the reference hub.
MENU = ['nfp/overview', 'nfp/quickstart', 'nfp/tutorial/index',
        'nfp/examples/index', 'nfp/target_file', 'nfp/search_space',
        'nfp/targets_losses', 'nfp/optimizer_block', 'nfp/choosing/index',
        'nfp/output_files', 'nfp/reference/index']
PAGES = {
    'quickstart': 'nfp_quickstart',
    'tutorial/index': 'nfp_tutorial',
    'examples/index': 'nfp_examples',
    'target_file': 'nfp_target_file',
    'search_space': 'nfp_search_space',
    'targets_losses': 'nfp_targets',
    'optimizer_block': 'nfp_optimizer_block',
    'choosing/index': 'nfp_choosing',
    'choosing/annealing': 'nfp_annealing',
    'choosing/asa': 'nfp_asa',
    'output_files': 'nfp_output',
    'reference/index': 'nfp_reference',
}
PAGES.update({f'tutorial/unit{n}': f'nfp_unit_{n}' for n in range(1, 8)})
PAGES.update({f'examples/{name}': f'nfp_example_{name}' for name in (
    'anneal_pt_full', 'asa', 'failure_grading', 'gfs_descriptor',
    'gradient_observable', 'multistart', 'pinned_coefficients', 'restart',
    'warm_start')})


def menu():
    """The options and the pages of the Nested Fitting toctree of index.rst."""
    index = (SOURCE / 'index.rst').read_text()
    block = re.search(r':caption: Nested Fitting\n(.*?)\n\n(.*?)(?:\n\n|\Z)',
                      index, re.DOTALL)
    return (block.group(1).split(),
            [entry.removesuffix('.rst') for entry in block.group(2).split()])


def menu_overview():
    """The first page of the Nested Fitting toctree of index.rst."""
    return menu()[1][0]


class ChapterPagesTest(unittest.TestCase):
    # Pins: the Nested Fitting caption of index.rst lists the pages of the
    # chapter, in the order of MENU and nothing else, with :titlesonly:, so
    # that the sidebar shows the pages and the pages of a hub.
    # Before the fix: the menu listed the earlier pages (nfp_background,
    # nfp_quickstart, nfp_configuration, ..., hpo_examples) and only one
    # page of the chapter, nfp/reference/index.
    def test_menu_lists_the_chapter_pages_in_order(self):
        options, pages = menu()
        self.assertIn(':titlesonly:', options)
        self.assertEqual(pages, MENU)

    # Pins: the label nested_fitting, which other pages cite for the
    # overview, is on nfp/overview, the overview the Nested Fitting menu
    # lists first, and on no other page.
    # Before the fix: the label was on nfp_background, the first page of
    # the earlier menu, and nfp/overview had none.
    def test_nested_fitting_label_is_on_the_menu_overview(self):
        labelled = [str(path.relative_to(SOURCE).with_suffix(''))
                    for path in SOURCE.rglob('*.rst')
                    if 'files' not in path.relative_to(SOURCE).parts and
                    re.search(r'^\.\. _nested_fitting:\s*$',
                              path.read_text(), re.MULTILINE)]
        self.assertEqual(labelled, ['nfp/overview'])
        self.assertEqual(menu_overview(), 'nfp/overview')

    # Pins: the published hpo.html, and its fragments of the overview
    # (#nested-fitting, #background), lead to the label nested_fitting on
    # the overview the menu lists first, in the compatibility table the
    # build generates the old pages from (docs/tools/legacy_routes.toml).
    # #what-problem-this-solves and #setup-effort-honestly, subsections of
    # "Failure handling -- graded FAILSCORE" in 1.5.0, lead to the section
    # on failed evaluations (nfp_targets_failures), not to the overview.
    # Before the fix: the menu listed nfp_background first; then these two
    # fragments led to the overview as well.
    def test_published_overview_fragments_reach_the_menu_overview(self):
        with open(ROUTES, 'rb') as stream:
            page = tomllib.load(stream)['pages']['hpo.html']
        target = menu_overview() + '.html#nested-fitting'
        self.assertEqual(page['default'], target)
        for anchor in ('nested-fitting', 'background'):
            with self.subTest(anchor=anchor):
                self.assertEqual(page['anchors'][anchor], target)
        for anchor in ('what-problem-this-solves', 'setup-effort-honestly'):
            with self.subTest(anchor=anchor):
                self.assertEqual(page['anchors'][anchor],
                                 'nfp/targets_losses.html#nfp-targets-failures')

    # Pins: no page of the site is an orphan (':orphan:'): each one is in
    # the menu or on a hub's toctree, which sphinx-build -W checks for every
    # page that is not an orphan.
    # Before the fix: the ten chapter pages that the menu did not list yet
    # (nfp/overview, nfp/quickstart, the tutorial and examples hubs, the
    # guides and the choosing hub) were orphans.
    def test_no_page_is_an_orphan(self):
        orphans = sorted(str(path.relative_to(SOURCE))
                         for path in SOURCE.rglob('*.rst')
                         if 'files' not in path.relative_to(SOURCE).parts and
                         re.search(r'^:orphan:\s*$', path.read_text(),
                                   re.MULTILINE))
        self.assertEqual(orphans, [])

    def test_each_topic_has_its_final_label(self):
        for page, label in PAGES.items():
            with self.subTest(page=page):
                text = (SOURCE / 'nfp' / f'{page}.rst').read_text()
                self.assertIn(f'.. _{label}:\n', text)
                if '.. toctree::' in text:
                    self.assertTrue(page.endswith('/index'), page)
                    self.assertEqual(text.count('.. toctree::'), 1)
                    self.assertIn(':maxdepth: 1', text)

    def test_final_topic_labels_are_unique_in_sources(self):
        labels = {value.casefold(): [] for value in PAGES.values()}
        for path in SOURCE.rglob('*.rst'):
            if 'files' in path.relative_to(SOURCE).parts:
                continue
            for label in re.findall(r'^\.\. _([^:]+):\s*$', path.read_text(),
                                    re.MULTILINE):
                if label.casefold() in labels:
                    labels[label.casefold()].append(str(path.relative_to(SOURCE)))
        for label, paths in labels.items():
            with self.subTest(label=label):
                self.assertEqual(len(paths), 1, paths)


if __name__ == '__main__':
    unittest.main()
