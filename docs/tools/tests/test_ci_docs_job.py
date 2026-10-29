"""Tests for the CI docs_sphinx job (.gitlab-ci.yml).

The job must install docs/requirements.txt every time, into an environment
of its own, and build with the sphinx-build that installation provides: a
runner that already has some Sphinx would otherwise build with it and
without the extensions the requirements list. The job's script, read from
.gitlab-ci.yml, runs here with stub commands first on PATH (python3, cmake,
git and the runner's own sphinx-build), each logging how it was called;
nothing is installed or downloaded. The python3 stub models what pip does
on a runner: `python3 -m pip install --user -r FILE` puts a sphinx-build in
$HOME/.local/bin, and the python of a `python3 -m venv DIR` environment puts
one in DIR/bin.

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


# The python3 of the runner. `-m pip install --user -r FILE` installs a
# sphinx-build into $HOME/.local/bin, as pip does; `-m venv DIR` makes an
# environment whose python installs a sphinx-build of its own when asked to
# install the requirements (unless VENV_FAILS is set); everything else is
# only logged.
PYTHON3 = r'''
echo "python3 $*" >> "$LOG"
if [ "$1 $2 $3 $4 $5" = "-m pip install --user -r" ]; then
  mkdir -p "$HOME/.local/bin"
  printf '#!/bin/bash\necho "user-sphinx-build $*" >> "$LOG"\n' > "$HOME/.local/bin/sphinx-build"
  chmod +x "$HOME/.local/bin/sphinx-build"
fi
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

# The build the job must run, with a sphinx-build its installation of the
# requirements provided ('user-' or 'venv-', the stubs above), never the
# runner's ('runner-').
BUILD = 'sphinx-build -W --keep-going -b html docs/source docs/build/html'


def installs(calls):
    """The logged installations of docs/requirements.txt."""
    return [c for c in calls
            if 'pip install' in c and '-r docs/requirements.txt' in c]


def builds(calls):
    """The logged sphinx-build runs."""
    return [c for c in calls if 'sphinx-build ' in c]


def installed_build(call):
    """Whether @p call is the build run with a sphinx-build the
    installation of the requirements provided."""
    return call in ('user-' + BUILD, 'venv-' + BUILD)


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

    def run_job(self, runner_sphinx=True, venv_fails=False,
                git_base='https://host.example/tadah'):
        if runner_sphinx:
            write_exe(os.path.join(self.bin, 'sphinx-build'),
                      'echo "runner-sphinx-build $*" >> "$LOG"\n')
        env = {'PATH': self.bin + os.pathsep + '/usr/bin' + os.pathsep +
                       '/bin',
               'HOME': self.home, 'LOG': self.log,
               'CI_PROJECT_DIR': self.project, 'CI_COMMIT_REF_NAME': 'v1-dev',
               'TMPDIR': self._tmp.name}
        if git_base is not None:
            env['TADAH_GIT_BASE'] = git_base
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

    def test_ci_git_base_is_the_projects_own_group(self):
        # The pipeline's TADAH_GIT_BASE (CMake's FetchContent and the clones
        # below read it) is the group of the project being built, on the
        # server running the pipeline; a group or project CI/CD variable of
        # that name takes precedence over it.
        # Before the fix: fails: the file defines no variables.
        import yaml
        with open(CI_CONFIG, encoding='utf-8') as fh:
            config = yaml.safe_load(fh)
        self.assertEqual(config.get('variables', {}).get('TADAH_GIT_BASE'),
                         '${CI_SERVER_URL}/${CI_PROJECT_NAMESPACE}')

    def test_core_and_hpo_are_cloned_from_the_git_base(self):
        # Before the fix: fails: the clones named a fixed address.
        proc, calls = self.run_job()
        report = proc.stdout + proc.stderr + '\n'.join(calls)
        self.assertEqual(proc.returncode, 0, report)
        clones = [c for c in calls if c.startswith('git clone ')]
        self.assertEqual(len(clones), 2, report)
        self.assertIn(' https://host.example/tadah/core.git ', clones[0])
        self.assertIn(' https://host.example/tadah/hpo.git ', clones[1])
        for c in clones:
            self.assertIn('--branch v1-dev ', c)

    def test_no_git_base_fails_the_job_naming_it(self):
        # Before the fix: fails: the job cloned from a fixed address.
        proc, calls = self.run_job(git_base=None)
        report = proc.stdout + proc.stderr + '\n'.join(calls)
        self.assertNotEqual(proc.returncode, 0, report)
        self.assertIn('TADAH_GIT_BASE', proc.stderr, report)
        self.assertFalse([c for c in calls if c.startswith('git clone ')],
                         report)

    def test_requirements_installed_when_the_runner_has_sphinx(self):
        # Pins: on a runner with a sphinx-build of its own the job exits 0,
        # installs docs/requirements.txt, and builds once, with the
        # sphinx-build that installation provided, not the runner's.
        # Before the fix: fails: the job installed nothing when the runner
        # had a sphinx-build (`if ! command -v sphinx-build`), and built with
        # the runner's ('runner-sphinx-build ...').
        proc, calls = self.run_job(runner_sphinx=True)
        report = proc.stdout + proc.stderr + '\n'.join(calls)
        self.assertEqual(proc.returncode, 0, report)
        self.assertTrue(installs(calls), report)
        self.assertEqual(len(builds(calls)), 1, report)
        self.assertTrue(installed_build(builds(calls)[0]), report)

    def test_requirements_installed_when_the_runner_has_no_sphinx(self):
        # Pins: on a runner without a sphinx-build the job exits 0, installs
        # docs/requirements.txt and builds once, with the sphinx-build that
        # installation provided.
        # Before the fix: passes as well (a pin): the job installed the
        # requirements with `pip install --user` and found their
        # sphinx-build in $HOME/.local/bin.
        proc, calls = self.run_job(runner_sphinx=False)
        report = proc.stdout + proc.stderr + '\n'.join(calls)
        self.assertEqual(proc.returncode, 0, report)
        self.assertTrue(installs(calls), report)
        self.assertEqual(len(builds(calls)), 1, report)
        self.assertTrue(installed_build(builds(calls)[0]), report)

    def test_the_requirements_are_installed_before_the_build(self):
        # Pins: on a runner with a sphinx-build of its own, docs/requirements.txt
        # is installed before the job's first cmake (the Doxygen step and the
        # build both run with what it installs).
        # Before the fix: fails: nothing was installed on such a runner.
        proc, calls = self.run_job(runner_sphinx=True)
        report = proc.stderr + '\n'.join(calls)
        self.assertEqual(proc.returncode, 0, report)
        self.assertTrue(installs(calls), report)
        install = calls.index(installs(calls)[0])
        first_cmake = next(i for i, c in enumerate(calls)
                           if c.startswith('cmake '))
        self.assertLess(install, first_cmake, report)

    def test_no_venv_fails_the_job_naming_it(self):
        # Pins: when python3 cannot make the job's virtual environment (an
        # image without Python's venv module), the job exits non-zero naming
        # python3-venv, and runs no sphinx-build.
        # Before the fix: fails: the job made no environment; on a runner with
        # a sphinx-build it built with that one and exited 0.
        proc, calls = self.run_job(runner_sphinx=True, venv_fails=True)
        self.assertNotEqual(proc.returncode, 0, '\n'.join(calls))
        self.assertIn('python3-venv', proc.stdout + proc.stderr)
        self.assertFalse(builds(calls), '\n'.join(calls))

if __name__ == '__main__':
    unittest.main()
