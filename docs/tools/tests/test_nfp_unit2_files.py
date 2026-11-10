"""The shared tutorial fit and its loss use the same fixed error scales."""
import csv
from pathlib import Path
import sys
import unittest


REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / 'tests' / 'nfp_docs'))
import nfp_files  # noqa: E402

UNIT = REPO / 'docs' / 'source' / 'nfp' / 'files' / 'tutorial' / 'unit2'


class Unit2FilesTest(unittest.TestCase):
    def test_shared_fit_uses_error_scales_and_evidence_regularisation(self):
        lines = nfp_files.target_lines(UNIT / 'config.train')
        keys = {line.words[0]: line.words[1:] for line in lines}
        for retired in ('EWEIGHT', 'FWEIGHT', 'SWEIGHT', 'WDBFILE_AUTO',
                        'EWEIGHT_TEMP'):
            with self.subTest(key=retired):
                self.assertNotIn(retired, keys)
        for key, values in {
                'MODEL': ['M_BLR', 'BF_Linear'],
                'OALGO': ['3'], 'LAMBDA': ['-1'],
                'ESIGMA': ['2'], 'FSIGMA': ['0.1'], 'SSIGMA': ['0.5'],
                'FORCE': ['true'], 'STRESS': ['true']}.items():
            with self.subTest(key=key):
                self.assertEqual(keys.get(key), values)
        self.assertEqual(keys.get('ENORM', ['ATOM']), ['ATOM'])

    def test_loss_terms_keep_fixed_accuracies_in_the_metric_units(self):
        lines = nfp_files.target_lines(UNIT / 'config.hpo')
        metrics = {line.words[0]: [float(x) for x in line.words[1:]]
                   for line in lines if line.words[0].startswith('PC_')}
        self.assertEqual(set(metrics),
                         {'PC_ERMSE', 'PC_FRMSE', 'PC_SRMSE_GPA'})
        for key, accuracy in (('PC_ERMSE', 2), ('PC_FRMSE', 0.1),
                              ('PC_SRMSE_GPA', 0.5)):
            with self.subTest(key=key):
                self.assertEqual(metrics.get(key, [])[:1], [0])
                self.assertAlmostEqual(metrics.get(key, [0, 0])[1],
                                       1 / accuracy**2)

    def test_expected_search_parameters_use_descriptor_units(self):
        with (UNIT / 'expected.csv').open(encoding='utf-8', newline='') as fh:
            rows = list(csv.reader(fh))
        self.assertEqual(rows[0], ['Quantity', 'Value'])
        config = nfp_files.configuration(UNIT / 'config.train', str(UNIT))
        expected = {'Best loss', 'Status'}
        for key in ('RCUT2B', 'RCUTMB', 'SGRIDMB'):
            unit, _ = nfp_files.parameter_unit(key, config)
            expected.add(f'Best {key} ({unit})')
        self.assertEqual({row[0] for row in rows[1:]}, expected)

    def test_expected_result_records_the_stop_at_the_budget(self):
        # The unit says that its search stops at MAXEVAL before it has
        # finished; the Status row makes the run check that stop.
        with (UNIT / 'expected.csv').open(encoding='utf-8', newline='') as fh:
            rows = {row[0]: row[1] for row in csv.reader(fh)}
        self.assertTrue(rows.get('Status', '').startswith('MAXEVAL_REACHED'),
                        rows.get('Status'))


if __name__ == '__main__':
    unittest.main()
