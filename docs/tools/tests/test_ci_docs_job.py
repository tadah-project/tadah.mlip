"""Tests for the GitLab docs_sphinx job (.gitlab-ci.yml).

The job must install docs/requirements.txt every time, into an environment
of its own, and build with that environment's sphinx-build: a runner that
already has some Sphinx would otherwise build with it and without the
extensions the requirements list. The job's script, read from
.gitlab-ci.yml, runs here with stub commands first on PATH (python3, cmake,
git and the runner's own sphinx-build), each logging how it was called;
nothing is installed or downloaded.

Run: python3 -m unittest discover -s docs/tools/tests -v
"""
import importlib.util
import os
import shutil
import stat
import subprocess
import tempfile
import textwrap
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, '..', '..', '..'))
CI_CONFIG = os.path.join(REPO, '.gitlab-ci.yml')


def have(module):
    return importlib.util.find_spec(module) is not None


def write_exe(path, body):
    with open(path, 'w', encoding='utf-8') as fh:
        fh.write('#!/bin/bash\n' + textwrap.dedent(body))
    os.chmod(path, os.stat(path).st_mode | stat.S_IXUSR)


# The python3 of the runner. `-m venv DIR` makes an environment whose python
# installs a sphinx-build of its own when asked to install the requirements
# (unless VENV_FAILS is set); everything else is only logged.
PYTHON3 = r'''
echo "python3 $*" >> "$LOG"
if [ "$1" = "-m" ] && [ "$2" = "venv" ]; then
  [ -n "$VENV_FAILS" ] && exit 1
  dir="$3"
  mkdir -p "$dir/bin"
  cat > "$dir/bin/activate" <<EOF
VIRTUAL_ENV="$dir"
PATH="$dir/bin:\$PATH"
export VIRTUAL_ENV PATH
EOF
  cat > "$dir/bin/python" <<EOF
#!/bin/bash
echo "venv-python \$*" >> "\$LOG"
if [ "\$1 \$2 \$3 \$4" = "-m pip install -r" ]; then
  printf '#!/bin/bash\necho "venv-sphinx-build \$*" >> "\$LOG"\n' > "$dir/bin/sphinx-build"
  chmod +x "$dir/bin/sphinx-build"
fi
EOF
  chmod +x "$dir/bin/python"
  cp "$dir/bin/python" "$dir/bin/python3"
fi
exit 0
'''

GIT = r'''
echo "git $*" >> "$LOG"
if [ "$1" = "clone" ]; then mkdir -p "${@: -1}"; fi
'''


@unittest.skipIf(not have('yaml'), 'PyYAML is not installed')
@unittest.skipIf(shutil.which('bash') is None, 'bash is not installed')
class DocsSphinxJobTest(unittest.TestCase):
    def setUp(self):
        import yaml
        with open(CI_CONFIG, encoding='utf-8') as fh:
            job = yaml.safe_load(fh)['docs_sphinx']
        self.script = '\n'.join(job['script'])
        self._tmp = tempfile.TemporaryDirectory()
        tmp = self._tmp.name
        self.log = os.path.join(tmp, 'calls.log')
        self.project = os.path.join(tmp, 'project')
        os.makedirs(os.path.join(self.project, 'docs'))
        with open(os.path.join(self.project, 'docs', 'requirements.txt'),
                  'w', encoding='utf-8') as fh:
            fh.write('sphinx\nbreathe\n')
        self.bin = os.path.join(tmp, 'bin')
        os.makedirs(self.bin)
        write_exe(os.path.join(self.bin, 'python3'), PYTHON3)
        write_exe(os.path.join(self.bin, 'git'), GIT)
        write_exe(os.path.join(self.bin, 'cmake'),
                  'echo "cmake $*" >> "$LOG"\n')
        self.home = os.path.join(tmp, 'home')
        os.makedirs(self.home)

    def tearDown(self):
        self._tmp.cleanup()

    def run_job(self, runner_sphinx=True, venv_fails=False):
        if runner_sphinx:
            write_exe(os.path.join(self.bin, 'sphinx-build'),
                      'echo "runner-sphinx-build $*" >> "$LOG"\n')
        env = {'PATH': self.bin + os.pathsep + '/usr/bin' + os.pathsep +
                       '/bin',
               'HOME': self.home, 'LOG': self.log,
               'CI_PROJECT_DIR': self.project, 'CI_COMMIT_REF_NAME': 'v1-dev',
               'TMPDIR': self._tmp.name}
        if venv_fails:
            env['VENV_FAILS'] = '1'
        # GitLab runs a script under `set -eo pipefail`.
        proc = subprocess.run(['bash', '-e', '-o', 'pipefail', '-c',
                               self.script],
                              cwd=self.project, env=env, capture_output=True,
                              text=True, timeout=60)
        calls = []
        if os.path.exists(self.log):
            with open(self.log, encoding='utf-8') as fh:
                calls = fh.read().splitlines()
        return proc, calls

    def test_requirements_installed_when_the_runner_has_sphinx(self):
        proc, calls = self.run_job(runner_sphinx=True)
        report = proc.stdout + proc.stderr + '\n'.join(calls)
        self.assertEqual(proc.returncode, 0, report)
        self.assertIn('venv-python -m pip install -r docs/requirements.txt',
                      calls, report)
        builds = [c for c in calls if 'sphinx-build ' in c]
        self.assertEqual(
            builds,
            ['venv-sphinx-build -W --keep-going -b html docs/source '
             'docs/build/html'], report)

    def test_requirements_installed_when_the_runner_has_no_sphinx(self):
        proc, calls = self.run_job(runner_sphinx=False)
        report = proc.stdout + proc.stderr + '\n'.join(calls)
        self.assertEqual(proc.returncode, 0, report)
        self.assertIn('venv-python -m pip install -r docs/requirements.txt',
                      calls, report)
        self.assertIn('venv-sphinx-build -W --keep-going -b html docs/source '
                      'docs/build/html', calls, report)

    def test_the_environment_is_installed_before_the_build(self):
        proc, calls = self.run_job(runner_sphinx=True)
        self.assertEqual(proc.returncode, 0, proc.stderr)
        install = calls.index(
            'venv-python -m pip install -r docs/requirements.txt')
        first_cmake = next(i for i, c in enumerate(calls)
                           if c.startswith('cmake '))
        self.assertLess(install, first_cmake, '\n'.join(calls))

    def test_no_venv_fails_the_job_naming_it(self):
        proc, calls = self.run_job(runner_sphinx=True, venv_fails=True)
        self.assertNotEqual(proc.returncode, 0, '\n'.join(calls))
        self.assertIn('python3-venv', proc.stdout + proc.stderr)
        self.assertFalse([c for c in calls if 'sphinx-build ' in c],
                         '\n'.join(calls))


if __name__ == '__main__':
    unittest.main()
