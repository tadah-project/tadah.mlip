"""Prints the ref the docs build clones CORE and HPO at on CI and Read the
Docs (source_locator.built_branch() under this process's environment), for
check_git_branch.cmake:

    python3 built_branch.py <dir of source_locator.py>

Output: BUILT_BRANCH=[<ref>], empty when no CI/RTD variable names one.
"""
import os
import sys

sys.path.insert(0, sys.argv[1])
import source_locator  # noqa: E402

_var, ref = source_locator.built_branch(os.environ)
print(f"BUILT_BRANCH=[{ref or ''}]")
