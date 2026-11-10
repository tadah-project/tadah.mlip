"""Reference entries link to the guide that owns the choice they describe."""
import copy
import os
import re
import tempfile
import unittest

from test_read_hpo_registry import (
    HAVE_SPHINX, HAVE_TOMLLIB, build_pages, entries_of, field_of,
    generator, hpo_checkout,
)


@unittest.skipUnless(HAVE_TOMLLIB, 'tomllib needs Python 3.11 or later')
class GuideLinksTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        hpo, reason = hpo_checkout()
        if hpo is None:
            raise unittest.SkipTest(reason)
        cls.rhr = generator()
        cls.sources = cls.rhr.read_sources(hpo)

    def links(self):
        return self.rhr.read_guide_links()

    def test_every_generated_entry_and_secondary_id_has_an_explicit_link(self):
        reg, optimizers, anneal = self.sources
        pages = self.rhr.Writer(reg, optimizers, anneal).pages()
        ids = {ident for page in pages for entry in page.entries()
               for ident in (entry.ident,) + tuple(entry.also)}
        links = self.links()
        self.assertEqual(set(links), ids)
        self.assertTrue(set(reg.by_id()).issubset(links))
        self.assertTrue(all(re.fullmatch(r'nfp_[a-z0-9_]+', label)
                            for label in links.values()))
        # Each entry links a guide page or a section of it: the label of a
        # section starts with the label of its page (the section of
        # training.rst on tadah hpo: with nfp_training_hpo).
        guides = ('nfp_training_hpo', 'nfp_target_file', 'nfp_search_space',
                  'nfp_targets', 'nfp_optimizer_block', 'nfp_choosing',
                  'nfp_annealing', 'nfp_asa', 'nfp_output')
        for label in set(links.values()):
            with self.subTest(label=label):
                self.assertTrue(any(label == g or label.startswith(g + '_')
                                    for g in guides), label)
        self.assertEqual({g for g in guides
                          if any(label == g or label.startswith(g + '_')
                                 for label in links.values())}, set(guides))

    def test_choices_link_to_their_owner_not_a_second_reference(self):
        links = self.links()
        # The section of the guide page that owns the choice.
        owners = {
            'LEXING': 'nfp_target_file_lines',
            'STRUCTURE': 'nfp_target_file_blocks',
            'RULE.settings_taken_by_lines': 'nfp_target_file_order',
            'ROOT.OPTIM': 'nfp_search_space_parameters',
            'RULE.fit_LAMBDA_negative': 'nfp_training_hpo_refused',
            'OPTIM': 'nfp_search_space_parameters',
            'OPTIM.AUTO': 'nfp_search_space_bounds',
            'OPTIM.LOG': 'nfp_search_space_transforms',
            'ROOT.OPTIM_SCALE': 'nfp_search_space_scaling',
            'ROOT.RHOGATE': 'nfp_search_space_feasibility',
            'OPTIM.TARGET.FIXWEIGHT': 'nfp_search_space_coefficients',
            'ROOT.PC_ERMSE': 'nfp_targets_losses',
            'ROOT.LOSS': 'nfp_targets_function',
            'PC_LAMMPS.--varloss': 'nfp_targets_scripts',
            'PC_OBSERVABLE.cohesive_energy.--ptol': 'nfp_targets_observables',
            'ROOT.FAILSCORE': 'nfp_targets_failures',
            'STRATEGY.N_STARTS': 'nfp_optimizer_block_strategies',
            'METHOD.WARM': 'nfp_optimizer_block_initialisation',
            'OPTIMIZER.MAXEVAL': 'nfp_optimizer_block_budgets',
            'OPTIMIZER.THREADS': 'nfp_optimizer_block_threads',
            'RESTART.MAX': 'nfp_optimizer_block_restart',
            'NOISE.AUTO': 'nfp_optimizer_block_noise',
            'BLOCK.INNER': 'nfp_optimizer_block_inner',
            'NLOPT:LN_BOBYQA': 'nfp_choosing_local',
            'DLIB:GFS': 'nfp_choosing_global',
            'DLIB:BFGS': 'nfp_choosing_gradients',
            'OPTIMIZER.GRAD_STRATEGY': 'nfp_choosing_gradients',
            'TADAH:GRID': 'nfp_choosing_baselines',
            'OPTIMIZER.STEP': 'nfp_choosing_step',
            'PARAM.TADAH:ANNEAL.T_HOT': 'nfp_annealing_temperatures',
            'PARAM.TADAH:ANNEAL.REPLICAS': 'nfp_annealing_replicas',
            'PARAM.TADAH:ANNEAL.STEP_FRAC': 'nfp_annealing_steps',
            'PARAM.TADAH:ANNEAL.INIT_MODE': 'nfp_annealing_initialisation',
            'PARAM.TADAH:ANNEAL.NEPS': 'nfp_annealing_stopping',
            'PARAM.ASA:ASA.Temperature_Ratio_Scale': 'nfp_asa_tuning',
            'PARAM.ASA:ASA.Accepted_To_Generated_Ratio': 'nfp_asa_tuning',
            'PARAM.ASA:ASA.Gener_Block': 'nfp_asa_batches',
            'PARAM.ASA:ASA.Limit_Generated': 'nfp_asa_stopping',
            'ROOT.OUTPUT': 'nfp_output_histories',
            'ROOT.DUMP': 'nfp_output_archives',
        }
        for ident, label in owners.items():
            with self.subTest(entry=ident):
                self.assertEqual(links.get(ident), label)

    def test_every_entry_renders_one_guide_field(self):
        links = self.links()
        pages = self.rhr.render(*self.sources)
        count = 0
        for text in pages.values():
            for ident, (kind, body) in entries_of(text).items():
                # hpo:target declares a library heading's anchor without
                # an entry body; its algorithms each have their own entry.
                if kind == 'target':
                    self.assertIsNone(field_of(body, 'Guide'))
                    continue
                with self.subTest(entry=ident):
                    self.assertEqual(field_of(body, 'Guide'),
                                     f':ref:`{links[ident]}`')
                    self.assertEqual(body.count(':Guide:'), 1)
                count += 1
        self.assertGreater(count, 300)

    def test_missing_coverage_names_each_id(self):
        links = self.links()
        for ident in ('OPTIMIZER.MAXEVAL', 'OPTIM.AUTO'):
            del links[ident]
        with self.assertRaises(self.rhr.GenerationError) as caught:
            self.rhr.render(*self.sources, guide_links=links)
        self.assertIn('without guide links', str(caught.exception))
        self.assertIn('OPTIMIZER.MAXEVAL', str(caught.exception))
        self.assertIn('OPTIM.AUTO', str(caught.exception))

    def test_unknown_ids_are_not_silently_ignored(self):
        links = dict(self.links(), **{'ROOT.NOT_A_KEY': 'nfp_search_space'})
        with self.assertRaisesRegex(self.rhr.GenerationError,
                                    'unknown.*ROOT.NOT_A_KEY'):
            self.rhr.render(*self.sources, guide_links=links)

    def test_a_registry_key_added_without_guidance_stops_generation(self):
        reg, optimizers, anneal = self.sources
        reg = copy.copy(reg)
        key = copy.copy(next(k for k in reg.keys if k.id == 'ROOT.DIGITS'))
        key.id, key.name = 'ROOT.NEW_SETTING', 'NEW_SETTING'
        reg.keys = list(reg.keys) + [key]
        with self.assertRaisesRegex(self.rhr.GenerationError,
                                    'without guide links.*ROOT.NEW_SETTING'):
            self.rhr.render(reg, optimizers, anneal)

    def test_secondary_id_cannot_choose_a_different_owner(self):
        links = dict(self.links(), OPTIM='nfp_targets')
        with self.assertRaisesRegex(self.rhr.GenerationError,
                                    'OPTIM.*same guide.*ROOT.OPTIM'):
            self.rhr.render(*self.sources, guide_links=links)

    def test_map_schema_and_label_spelling_are_checked(self):
        bad = (
            ('[other]\n', 'links'),
            ('[guides]\n"ROOT.OPTIM" = ["ROOT.OPTIM"]\n', 'ROOT.OPTIM'),
            ('[guides]\n"nfp_search_space" = 1\n', 'nfp_search_space'),
            ('[guides]\n"nfp_search_space" = []\n', 'nfp_search_space'),
            ('[guides]\n"nfp_search_space`bad" = ["ROOT.OPTIM"]\n',
             'nfp_search_space`bad'),
            ('[guides]\n"nfp_ref_pc_options" = ["ROOT.OPTIM"]\n',
             'nfp_ref_pc_options'),
            ('[guides]\n"nfp_search_space" = [1]\n', 'nfp_search_space'),
        )
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, 'links.toml')
            for text, message in bad:
                with self.subTest(text=text):
                    with open(path, 'w', encoding='utf-8') as fh:
                        fh.write(text)
                    with self.assertRaisesRegex(self.rhr.GenerationError,
                                                message):
                        self.rhr.read_guide_links(path)

    def test_an_id_has_exactly_one_guide_owner(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, 'links.toml')
            with open(path, 'w', encoding='utf-8') as fh:
                fh.write('[guides]\nnfp_search_space = ["ROOT.OPTIM"]\n'
                         'nfp_targets = ["ROOT.OPTIM"]\n')
            with self.assertRaisesRegex(self.rhr.GenerationError,
                                        'ROOT.OPTIM.*more than once'):
                self.rhr.read_guide_links(path)

    def test_missing_or_malformed_map_is_a_generation_error(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, 'links.toml')
            with self.assertRaisesRegex(self.rhr.GenerationError,
                                        'guide links.*links.toml'):
                self.rhr.read_guide_links(path)
            with open(path, 'w', encoding='utf-8') as fh:
                fh.write('[links\n')
            with self.assertRaisesRegex(self.rhr.GenerationError,
                                        'guide links.*links.toml'):
                self.rhr.read_guide_links(path)

    @unittest.skipUnless(HAVE_SPHINX, 'Sphinx and sphinx_rtd_theme are required')
    def test_sphinx_checks_guide_labels_and_renders_links(self):
        links = self.links()
        pages = self.rhr.render(*self.sources)
        labels = sorted(set(links.values()))

        def guides(omit=None):
            text = ['Guidance', '========', '']
            for i, label in enumerate(labels):
                if label != omit:
                    text += [f'.. _{label}:', '']
                title = f'Choice {i}'
                text += [title, '-' * len(title), '', 'Choice guidance.', '']
            return '\n'.join(text)

        extra = {'guide_targets.rst': guides(),
                 'nfp/reference/optimizer_keys.rst': pages['optimizer_keys']}
        status, warnings, html = build_pages(pages, extra)
        self.assertEqual((status, warnings), (0, []))
        self.assertIn('guide_targets.html#nfp-optimizer-block',
                      html['nfp/reference/optimizer_keys'])

        extra['guide_targets.rst'] = guides('nfp_optimizer_block')
        status, warnings, _html = build_pages(pages, extra)
        self.assertNotEqual(status, 0)
        self.assertTrue(any('undefined label' in warning
                            and 'nfp_optimizer_block' in warning
                            for warning in warnings), warnings)


if __name__ == '__main__':
    unittest.main()
