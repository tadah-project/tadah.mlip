# Authors

Tadah! is developed and maintained at the University of Edinburgh.
This list is derived from the combined `git shortlog` of all Tadah!
repositories (`core`, `mlip`, `models`, `md`, `hpo`, `tadah.mlip`,
`tadah.lammps`, `cmake-tools`), with duplicate identities collapsed.

## Maintainers

- **Marcin Kirsz** &lt;marcin.kirsz@ed.ac.uk&gt; — project lead and primary maintainer.

## Contributors

- Izzy Rodger &lt;izzy.rodger@ed.ac.uk&gt;

## How this list is generated

Run, from any worktree of the umbrella project:

```bash
for repo in CORE MLIP MODELS MD HPO Tadah.MLIP Tadah.LAMMPS cmake-tools; do
  git -C "$repo" shortlog -sne --all
done | sort -k2 -u
```

then deduplicate by author identity (the lead's commits appear under
several historic email addresses — `mkirsz@ed.ac.uk`,
`s1351949@sms.ed.ac.uk`, `marcin.kirsz@ed.ac.uk` — all the same
person). To propose a correction, open an issue on the umbrella
project.
