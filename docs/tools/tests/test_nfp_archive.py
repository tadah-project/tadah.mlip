"""The downloadable files archive has stable bytes and safe member paths."""
import importlib.util
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
import unittest
import zipfile


TOOLS = Path(__file__).resolve().parents[1]


class ArchiveTest(unittest.TestCase):
    def setUp(self):
        spec = importlib.util.spec_from_file_location(
            'nfp_archive', TOOLS / 'nfp_archive.py')
        self.archive = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.archive)
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.files = self.root / 'files'
        (self.files / 'data').mkdir(parents=True)
        (self.files / 'data' / 'a.tadahdb').write_bytes(b'Ta\n')
        (self.files / 'manifest.txt').write_bytes(b'# runs\n')

    def test_stable_bytes_despite_mtime_and_permissions(self):
        first = self.archive.build_archive(self.files, self.root / 'a.zip')
        for path in self.files.rglob('*'):
            os.utime(path, (1700000000, 1700000000))
            if path.is_file():
                path.chmod(0o600)
        second = self.archive.build_archive(self.files, self.root / 'b.zip')
        self.assertEqual(first.read_bytes(), second.read_bytes())
        with zipfile.ZipFile(first) as archive:
            self.assertEqual(archive.namelist(),
                             ['files/data/a.tadahdb', 'files/manifest.txt'])
            self.assertEqual(archive.read('files/data/a.tadahdb'), b'Ta\n')
            for member in archive.infolist():
                self.assertEqual(member.date_time, (1980, 1, 1, 0, 0, 0))
                self.assertEqual(member.external_attr >> 16, 0o100644)

    def test_changed_content_changes_archive(self):
        first = self.archive.build_archive(self.files, self.root / 'a.zip')
        (self.files / 'data' / 'a.tadahdb').write_bytes(b'Ta 2\n')
        second = self.archive.build_archive(self.files, self.root / 'b.zip')
        self.assertNotEqual(first.read_bytes(), second.read_bytes())

    def test_symlink_is_refused_even_inside_source(self):
        (self.files / 'link').symlink_to('manifest.txt')
        with self.assertRaisesRegex(ValueError, 'symlink'):
            self.archive.build_archive(self.files, self.root / 'bad.zip')
        self.assertFalse((self.root / 'bad.zip').exists())

    def test_symlink_source_root_is_refused_without_replacing_output(self):
        linked = self.root / 'linked'
        linked.symlink_to(self.files, target_is_directory=True)
        output = self.root / 'existing.zip'
        output.write_bytes(b'unchanged')
        with self.assertRaisesRegex(ValueError, 'symlink'):
            self.archive.build_archive(linked, output)
        self.assertEqual(output.read_bytes(), b'unchanged')

    def test_missing_source_and_output_inside_source_are_refused(self):
        with self.assertRaisesRegex(ValueError, 'directory'):
            self.archive.build_archive(self.root / 'absent', self.root / 'a.zip')
        with self.assertRaisesRegex(ValueError, 'inside'):
            self.archive.build_archive(self.files, self.files / 'a.zip')

    def test_sphinx_hook_registers_source_dependencies_and_download(self):
        class Environment:
            def __init__(self):
                self.dependencies = []

            def note_dependency(self, path):
                self.dependencies.append(Path(path))

        class App:
            srcdir = str(self.root)
            outdir = str(self.root / 'html')
            env = Environment()

        (self.root / 'nfp').mkdir()
        self.files.rename(self.root / 'nfp' / 'files')
        self.archive.prepare_download(App())
        self.archive.note_sources(App(), 'nfp/tutorial/index', [''])
        output = self.root / 'nfp' / 'nfp-files.zip'
        self.assertTrue(output.is_file())
        self.assertIn(self.root / 'nfp' / 'files' / 'manifest.txt',
                      App.env.dependencies)
        before = output.stat().st_mtime_ns
        self.archive.prepare_download(App())
        self.assertEqual(output.stat().st_mtime_ns, before)

    def test_tutorial_download_publishes_the_generated_archive(self):
        if not shutil.which('sphinx-build'):
            self.skipTest('sphinx-build is required for the archive download')
        hub = TOOLS.parent / 'source/nfp/tutorial/index.rst'
        roles = re.findall(r':download:`([^`]*nfp-files\.zip[^`]*)`',
                           hub.read_text())
        self.assertEqual(len(roles), 1, 'the tutorial offers one files archive')
        source = self.root / 'source'
        tutorial = source / 'nfp/tutorial'
        tutorial.mkdir(parents=True)
        shutil.copytree(self.files, source / 'nfp/files')
        (source / 'conf.py').write_text(
            f'import sys\nsys.path.insert(0, {str(TOOLS)!r})\n'
            "extensions = ['nfp_archive']\nmaster_doc = 'index'\n")
        (source / 'index.rst').write_text(
            'Downloads\n=========\n\n.. toctree::\n\n   nfp/tutorial/index\n')
        (tutorial / 'index.rst').write_text(
            'Tutorial\n========\n\n:download:`' + roles[0] + '`\n')
        output = self.root / 'html'
        result = subprocess.run(
            ['sphinx-build', '-W', '-b', 'html', str(source), str(output)],
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
            timeout=90)
        self.assertEqual(result.returncode, 0, result.stdout)
        html = (output / 'nfp/tutorial/index.html').read_text()
        links = re.findall(r'href="([^"]+\.zip)"', html)
        self.assertEqual(len(links), 1, html)
        download = (output / 'nfp/tutorial' / links[0]).resolve()
        self.assertTrue(download.is_relative_to(output.resolve()))
        with zipfile.ZipFile(download) as archive:
            self.assertEqual(archive.namelist(),
                             ['files/data/a.tadahdb', 'files/manifest.txt'])
            self.assertEqual(archive.read('files/data/a.tadahdb'), b'Ta\n')


if __name__ == '__main__':
    unittest.main()
