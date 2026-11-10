"""Shared data metadata uses metric units and a single provenance owner."""
from pathlib import Path
import csv
import unittest


FILES = Path(__file__).resolve().parents[2] / 'source' / 'nfp' / 'files'


class DataFilesTest(unittest.TestCase):
    def test_reference_units_match_the_quantities_used_by_the_examples(self):
        with (FILES / 'dft_reference.csv').open(newline='') as stream:
            units = {row['quantity']: row['unit'] for row in csv.DictReader(stream)}
        self.assertEqual(units, {'free_atom_energy': 'eV', 'Ecoh': 'eV/atom',
                                 'a0': 'Å', 'Evf': 'eV', 'Evf_unrelaxed': 'eV',
                                 'C11': 'GPa', 'C12': 'GPa', 'C44': 'GPa',
                                 'B': 'GPa', 'B_eos': 'GPa'})

    def test_readme_points_to_the_provenance_owner_without_duplicating_it(self):
        text = (FILES / 'data/README.md').read_text()
        self.assertIn('[The Ta data][ta-data] accompanies these files.', text)
        self.assertIn('#nfp-ta-data', text)
        self.assertNotIn('Calculation settings, the energy reference', text)


if __name__ == '__main__':
    unittest.main()
