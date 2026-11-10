"""The restart comparison distinguishes a configured cap from observed work."""
from pathlib import Path
import csv
import re
import unittest


FILES = (Path(__file__).resolve().parents[2] / 'source' / 'nfp' / 'files' /
         'examples' / 'restart')


def header_comment(text):
    '''The comment lines at the top of a target file, joined into one text.'''
    words = []
    for line in text.splitlines():
        if not line.startswith('#'):
            break
        words += line[1:].split()
    return ' '.join(words)


class RestartFilesTest(unittest.TestCase):
    def test_comparison_budget_is_described_as_observed_not_configured(self):
        single = (FILES / 'single.hpo').read_text()
        restart = (FILES / 'config.hpo').read_text()
        budget = int(re.search(r'^\s*MAXEVAL\s+(\d+)', single,
                               re.MULTILINE).group(1))
        per_search = int(re.search(r'^\s*MAXEVAL\s+(\d+)', restart,
                                   re.MULTILINE).group(1))
        rounds = int(re.search(r'^\s*MAX\s+(\d+)', restart,
                               re.MULTILINE).group(1))
        self.assertLess(budget, per_search * (rounds + 1))
        # Pins: the header of single.hpo calls its MAXEVAL a budget of
        # calls, those config.hpo makes in all (its evaluations and the
        # calls answered from the run's record), not a total of
        # evaluations; the evaluations of config.hpo that expected.csv
        # gives are fewer than those calls. Before the fix: the header says
        # "matching the observed total of config.hpo", which does not say
        # what it counts.
        with (FILES / 'expected.csv').open(newline='') as stream:
            rows = {r['Quantity']: r['Value'] for r in csv.DictReader(stream)}
        self.assertLess(int(rows['Evaluations (config.hpo)']), budget)
        header = header_comment(single)
        self.assertIn(f'Its budget of {budget} calls is what config.hpo makes '
                      'in all: its evaluations and the calls that the run '
                      "answers from its record.", header)
        self.assertNotIn('observed total', header)


if __name__ == '__main__':
    unittest.main()
