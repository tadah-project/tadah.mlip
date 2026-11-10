"""Downloadable fitting inputs use the supported quantities and metric units."""
from pathlib import Path
import csv
import unittest


FILES = Path(__file__).resolve().parents[2] / 'source' / 'nfp' / 'files'
RETIRED = {'EWEIGHT', 'FWEIGHT', 'SWEIGHT', 'WDBFILE_AUTO', 'EWEIGHT_TEMP',
           'PC_SRMSE'}


class CurrentInputsTest(unittest.TestCase):
    def test_target_and_training_files_use_supported_keys(self):
        paths = sorted(FILES.rglob('*.hpo')) + sorted(FILES.rglob('*.train'))
        self.assertTrue(paths)
        for path in paths:
            for number, line in enumerate(path.read_text().splitlines(), 1):
                words = line.split('#', 1)[0].split()
                if not words:
                    continue
                key = words[1] if words[0] == 'OPTIM' else words[0]
                with self.subTest(file=str(path.relative_to(FILES)), line=number):
                    self.assertNotIn(key.upper(), RETIRED)

    def test_result_tables_use_the_units_written_by_the_program(self):
        for path in sorted(FILES.rglob('expected.csv')):
            with path.open(newline='') as stream:
                rows = list(csv.reader(stream))
            self.assertEqual(rows[0], ['Quantity', 'Value'])
            for row in rows[1:]:
                with self.subTest(file=str(path.relative_to(FILES)), row=row):
                    self.assertEqual(len(row), 2)
                    self.assertNotIn('EWEIGHT', row[0])
                    self.assertNotIn('SWEIGHT', row[0])
                    self.assertNotIn('(eV/A^3)', row[0])
                    self.assertNotIn('(eV/A)', row[0])
                    self.assertNotIn('(A)', row[0])


if __name__ == '__main__':
    unittest.main()
