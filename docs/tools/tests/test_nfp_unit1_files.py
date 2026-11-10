"""The first tutorial's validation offset agrees with the shipped data."""
from pathlib import Path
import re
import unittest


FILES = Path(__file__).resolve().parents[2] / 'source' / 'nfp' / 'files'


class Unit1FilesTest(unittest.TestCase):
    def test_validation_offset_and_its_explanation(self):
        values = []
        for name in ('train', 'val'):
            text = (FILES / 'data' / f'ta_eos_{name}.tadahdb').read_text()
            values.append([float(value) for value in re.findall(
                r'^vasp_data/HS_FROZEN_PHONON/a([0-9.]+)/OUTCAR', text,
                re.MULTILINE)])
        self.assertEqual([len(group) for group in values], [40, 40])
        for training, validation in zip(*values):
            self.assertAlmostEqual(validation - training, 0.01, places=12)
        comment = (FILES / 'tutorial/unit1/config.val').read_text()
        self.assertIn('0.01 A above', comment)


if __name__ == '__main__':
    unittest.main()
