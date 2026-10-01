.. _training_page:

Training
========

This page covers the ``tadah train`` command in depth. For a one-shot
introduction see :ref:`training` in the quickstart; for the full
key-by-key reference see :ref:`ConfigSection`.

Command
-------

::

   tadah train -c <config.train> [--force] [--stress] [--uncertainty] \
               [--outfile <pot.tadah>] [--verbose <0|1|2>]

The required input is a single :ref:`config file <config_file>`
(``--config`` / ``-c``) that declares the dataset(s) via ``DBFILE``,
the descriptor(s), and the regression model. Every CLI flag maps
one-to-one to an upper-case configuration key (e.g. ``--force`` ↔
``FORCE true``); CLI flags override file values.

================== ================ =====================================================================
Flag               Config key       Effect
================== ================ =====================================================================
``-c <file>``      ``CONFIG``       Training configuration file.
``-o <file>``      ``OUTFILE``      Trained-model output file (default ``pot.tadah``).
``-F``             ``FORCE``        Train on forces in addition to energies (default off).
``-S``             ``STRESS``       Train on stresses in addition to energies (default off).
``--uncertainty``  ``UNCERTAINTY``  Compute and write per-weight uncertainty (default off).
``-v <0|1|2>``     ``VERBOSE``      Log level: 0 ERROR, 1 WARNING, 2 INFO (default 1).
================== ================ =====================================================================

Training data must be supplied as one or more Tadah! database files via
``DBFILE`` in the configuration; see :ref:`dataset` for the on-disk format.

Pipeline
--------

For each ``tadah train`` invocation the engine runs the following stages
in order:

#. **Load the structures** declared by every ``DBFILE`` key into memory.
#. **Apply load-time data transforms** (``LSCALE``, ``ESHIFT*``,
   ``EFILTER`` / ``FFILTER``, ``EWEIGHT_TEMP``, ``WDBFILE`` /
   ``WDBFILE_AUTO``, ``ZERO_COM_FORCE``) — see :ref:`load_transforms`.
#. **Build the neighbour list** out to ``RCUTMAX``: every periodic image
   of every atom closer than the cutoff, for any cell (triclinic, or
   thinner than the cutoff) and for atoms inside or outside the cell (see
   :doc:`API/nn_finder`).
#. **Run the pre-flight audit** when enabled (``AUDIT`` is ``off`` by
   default; ``warn`` reports findings, ``error`` refuses to launch on a
   FAIL-level finding), which checks dataset statistics and
   per-descriptor metadata for ρ-collapse and out-of-range parameter
   combinations.
#. **Fit the regression model** declared by ``MODEL``.
#. **Write ``pot.tadah``** (or whatever ``--outfile`` requests) and the
   optional uncertainty file.

.. versionchanged:: 1.6.0
   The neighbour list holds every periodic image. Earlier versions missed
   images of structures whose cell is thinner than the cutoff in some
   direction (e.g. a 2-atom bcc cell with :math:`a = 3.41` Å at a 10 Å
   cutoff kept 168 of each atom's 180 neighbours), or is triclinic, or whose
   atoms lie outside the cell. Potentials trained on such structures, and
   ``tadah predict`` results for them, change; retrain the potentials.
   ``tadah predict`` now agrees with ``pair_style tadah`` in LAMMPS on every
   cell.

.. _load_transforms:

Load-time data transforms
-------------------------

The transforms below reshape the training set *before* any neighbour
finding, descriptor calculation, or fitting. They run inside
``apply_load_transforms()`` as soon as the datasets are loaded and are
shared across ``tadah train``, ``tadah predict``, and ``tadah hpo``.

Two of them — ``LSCALE`` and the resolved ``ESHIFT`` — are stamped into
``pot.tadah`` so ``tadah predict`` round-trips them automatically; the
rest are training-time only. For the per-key CLI form and defaults see
:ref:`ConfigSection` and ``tadah explain <key>`` (e.g. ``tadah explain
train.lscale``).

Order of application
~~~~~~~~~~~~~~~~~~~~

The transforms run in a fixed order, and the order matters because
several feed the next:

#. **Outlier filter** (``EFILTER`` / ``FFILTER``) — runs first, so an
   extreme outlier cannot poison the later least-squares and
   reweighting steps.
#. **Length rescale** (``LSCALE``).
#. **ESHIFT** — determine the per-element shifts, then subtract them.
#. **Boltzmann reweight** (``EWEIGHT_TEMP``) — after ESHIFT, so the
   Boltzmann factor sees the shifted, user-meaningful energies.
#. **COM-force zeroing** (``ZERO_COM_FORCE``).
#. **Per-dataset weights** — ``WDBFILE`` composed multiplicatively with
   ``WDBFILE_AUTO``.
#. **Equilibrium-volume diagnostic** — logs the lowest post-transform
   ``E/N`` configuration as a reference-state sanity check.

Key summary
~~~~~~~~~~~

============================ ===========================================================================
Key                          One-line summary
============================ ===========================================================================
``LSCALE``                   Uniform length rescale (DFT → experimental scale).  Persisted to pot.tadah.
``ESHIFT``                   Explicit per-element energy shift; one value per element ordered by Z.
``ESHIFT_ATOM``              Derive ESHIFT from isolated-atom configurations in the dataset.
``ESHIFT_DBATOM``            Derive ESHIFT by least-squares atomic-energy fit over the database.
``EFILTER  <lo> <hi>``       Drop configurations whose per-atom energy is outside ``[lo, hi]``.
``FFILTER  <thr>``           Drop configurations with any single force component above ``thr``.
``WDBFILE  <w_1> ... <w_n>`` Per-dataset weight multipliers; one per ``DBFILE``.
``WDBFILE_AUTO  <α>``        Automatic size balancing: per-config weight × ``1 / N_i^α``, ``α ∈ [0,1]``.
``EWEIGHT_TEMP  <T>``        Boltzmann reweighting at temperature ``T`` (Kelvin).
``ZERO_COM_FORCE``           Subtract the mean force per configuration.
============================ ===========================================================================

LSCALE: uniform length rescale
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``LSCALE s`` multiplies every position and cell vector by ``s``. The
total energy is invariant under the change of length scale, so forces
are divided by ``s`` and stresses (eV Å⁻³) by ``s³``: the virial
:math:`V\sigma` is invariant and the volume grows by ``s³``. Earlier
builds (1.0.0 to 1.5.x, and 1.6.0 development builds made before
1 October 2026) left the stress unchanged, which made the targets of a
``STRESS true`` fit too large by ``s³`` (6 % at ``s = 1.02``). ``s = 1``
(the default) is a no-op and ``s <= 0`` is a configuration error.

The intended use is matching a DFT lattice constant to experiment: set
``LSCALE = a_exp / a_DFT`` so the trained model's equilibrium volume
sits at the experimental scale.

``LSCALE`` is stamped into ``pot.tadah``. ``tadah predict`` re-applies
it to the dataset it reads, so a raw DFT dataset can be given as it is;
both the reference (``True``) and the predicted columns it writes are in
the rescaled frame. Pass ``--no-lscale`` to ignore the stored value when
the dataset is already at the trained scale. The
LAMMPS ``pair_style tadah`` does **not** re-apply ``LSCALE`` — supply
LAMMPS with positions already at the trained scale.

ESHIFT: per-element energy shift
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

For every configuration ESHIFT *subtracts* a per-element baseline::

   E_after = E_before − Σ_Z N_Z · ESHIFT[Z]

``ESHIFT[Z]`` is the per-atom reference energy of species ``Z`` to be
removed — feeding the isolated-atom energy turns total energies into
cohesive energies. There are three mutually exclusive ways to obtain
the values; if more than one is set the engine warns and applies the
precedence **explicit > ATOM > DBATOM**:

* **Explicit** — ``ESHIFT a b …``, one value per unique species,
  ordered by ``Z``.
* **ESHIFT_ATOM** — the mean energy of the single-atom
  (``natoms == 1``) configurations of each species. A species with no
  isolated-atom config gets ``0`` and a warning; a spread above 1 meV
  across same-species configs is logged.
* **ESHIFT_DBATOM** — a least-squares atomic-energy fit over the
  whole database (``min ‖y − Mβ‖²``, with ``M[i,k]`` the count of
  species ``k`` in config ``i``). Robust when the dataset has
  compositional diversity but no isolated-atom configs.

Whichever path is used, the *resolved* per-element values are stamped
into ``pot.tadah`` (the ``ESHIFT_ATOM`` / ``ESHIFT_DBATOM`` toggles are
not). ``tadah predict`` re-applies them; ``--no-eshift`` ignores the
stored values.

.. note::

   **DM_REAM potentials (EMBF0 ZERO, the default).** The zero-referenced
   embedding fixes the isolated-atom energy of a DM_REAM model at 0, so the
   model has no free per-atom constant unless ``BIAS true`` is set. When the
   reference energies are absolute (e.g. DFT totals), remove the per-atom
   reference with ``ESHIFT``, ``ESHIFT_ATOM`` or ``ESHIFT_DBATOM`` (which
   keeps the LAMMPS ``EAMFAST`` kernel usable) or set ``BIAS true`` (its
   intercept column precedes the descriptor weights, so ``FIXINDEX``/
   ``FIXWEIGHT`` indices shift).
   ``tadah train`` prints a HINT when neither is present. A DM_REAM
   configuration must also declare its density form ``RHOFORM`` (see
   `pot.tadah format`_). See :ref:`DM_REAM <dm_ream>`.

Filtering and weighting
~~~~~~~~~~~~~~~~~~~~~~~

* ``EFILTER lo hi`` drops any configuration whose **per-atom** energy
  ``E/N`` is outside ``[lo, hi]``; ``FFILTER thr`` drops any
  configuration with a single-atom force magnitude above ``thr``. Both
  run first and are training-time only.
* ``WDBFILE w_1 … w_n`` multiplies the ``eweight`` / ``fweight`` /
  ``sweight`` of every config in dataset ``i`` by ``w_i`` — one value
  per ``DBFILE``.
* ``WDBFILE_AUTO α`` multiplies dataset ``i``'s per-config weight by
  ``N_i^(−α)``, where ``N_i`` is its post-filter config count.
  ``α = 0`` disables it, ``α = 0.5`` is a soft balance, ``α = 1`` makes
  every dataset contribute equal aggregate loss. It composes
  multiplicatively with ``WDBFILE``; ``α`` outside ``[0, 1]`` is
  rejected.
* ``EWEIGHT_TEMP T`` multiplies each ``eweight`` by
  ``exp(−(E/N − E_min)/(k_B·T))`` (``E_min`` the minimum per-atom
  energy, ``T`` in Kelvin), biasing the loss toward low-energy configs.
  Applied after ESHIFT; ``T <= 0`` is rejected.
* ``ZERO_COM_FORCE`` subtracts the mean force of each configuration
  so its net force is exactly zero — useful against residual
  translational forces left by incomplete DFT relaxation.

These transforms are training-time only: they are not stamped into
``pot.tadah`` and not re-applied at predict time.

.. _train_outputs:

Output files
------------

``tadah train`` writes its results into the **current working
directory** (i.e. wherever you invoked the command).

================== ============================================================================
File               Contents
================== ============================================================================
``pot.tadah``      The trained interatomic potential. Path overridable via ``--outfile`` /
                   ``OUTFILE``. This is the only required artefact and the file consumed by
                   ``tadah predict`` and the LAMMPS ``pair_style tadah``.
``weights.tadah``  Per-weight mean and uncertainty, written only when ``--uncertainty`` /
                   ``UNCERTAINTY true`` is set.
================== ============================================================================

A potential file that cannot be written (its directory does not exist, or
a directory has its name) stops ``tadah train`` with an error that names
the file and a non-zero exit status; it used to log an ``ERROR`` line and
exit 0 without a potential.

pot.tadah format
~~~~~~~~~~~~~~~~

The potential file is plain ASCII. Each line is one ``KEY VALUE[…]``
pair using the same syntax as the training configuration file
(whitespace separates fields; comments and blank lines are not
required). The file is round-tripped through ``tadah predict`` and
``pair_style tadah`` — every field needed to reproduce the prediction
is stored here.

A minimal single-species, two-body, kernel-ridge example::

   ATOM     Ta
   BIAS     false
   DIMER    false   0   false
   EWEIGHT  1
   FWEIGHT  1
   INIT2B   true
   INITMB   false
   LAMBDA   0
   MODEL    M_KRR   Kern_Linear   1
   NORM     false
   OALGO    1
   RCTYPE2B Cut_Dummy
   RCUT2B   6.5
   SWEIGHT  1
   TYPE2B   D2_LJ   Ta   Ta
   WATOM    73
   WEIGHTS  14.20421334...   2604.39696519...

Three categories of keys appear:

#. **Configuration echo** — every key that fully specifies the model
   geometry: descriptor type and parameters (``TYPE2B``, ``RCUT2B``,
   ``RCTYPE2B``, ``INIT2B`` / ``INITMB``, …), model choice and
   hyperparameters (``MODEL``, ``LAMBDA``, ``NORM``, ``BIAS``, …), and
   the species list (``ATOM`` / ``WATOM``).

#. **Fitted parameters** — the row ``WEIGHTS …`` carries the learned
   coefficients :math:`\mathbf w`. The ordering matches the column
   layout of the design matrix and is reproduced exactly on load.

#. **Stamped load-time transforms** — when the training run resolved
   ``LSCALE`` (≠ 1) or ``ESHIFT`` (explicit or via
   ``ESHIFT_ATOM`` / ``ESHIFT_DBATOM``), the resolved values are
   written so prediction re-applies them automatically. The
   ``*_ATOM`` / ``*_DBATOM`` toggles themselves are not stamped (only
   their resolved output).

DM_REAM potentials always carry explicit ``EMBF0`` and ``RHOFORM`` lines;
the writer spells ``RHOFORM`` in upper case and drops a stray one from a
potential without a DM_REAM block (a notice on standard error, once per
process: :ref:`RHOFORM and EMBF0 without a DM_REAM block <dm_ream_only_keys>`). ``RHOFORM`` has no
default: a DM_REAM training configuration without it stops before any
fitting with the migration message (``RHOFORM SIGNED`` is the density of
every tadah before 1.6.0); see :ref:`DM_REAM <dm_ream_rhoform>`.

Runtime-only knobs (``VERBOSE``, ``NUMERIC``, ``OUTFILE``, ``AUDIT``,
``HEALTH_LOG``) are stripped before write — leaving them in the file
would trigger "Invalid key" errors in the LAMMPS-side parser.

.. _pot_number_format:

Number format
~~~~~~~~~~~~~

Every non-integer number of a potential's key/value lines is written
with ``NUMERIC`` **significant** digits (``printf`` ``%.{N}g``). The
default, ``18``, is exact: 17 significant digits identify every double,
so a potential reloaded from ``pot.tadah`` is bit-identical to the one in
memory. That is why a value typed as ``5.3`` comes back as
``5.29999999999999982`` — the exact double nearest to 5.3 — and why
``1.0`` is written as ``1``. Integers, names, booleans, index lists and
grid generators (``LIN`` / ``GEOM``) are written as they are stored, so
integers are always exact. ``#`` comment lines (such as the header
``tadah hpo`` writes) are annotations that ``NUMERIC`` does not govern.

* ``NUMERIC`` itself is never written into a potential file.
* An explicit ``NUMERIC`` below 17 is honoured (the numbers are rounded to
  that many significant digits) but ``tadah`` prints a WARNING: the
  written potential then differs slightly from the fitted one.
* ``NUMERIC`` never limits the values held in memory: calculations
  always run at full double precision, whatever it is set to. Every
  ``tadah`` reader of a file ``tadah`` writes (potentials, configurations,
  datasets, POSCAR exports) accepts what the writers produce, including
  exponent forms and subnormal values such as ``1e-310``.
* Dataset files written by ``tadah data convert``, ``merge``, ``split``,
  ``sample``, ``dedup`` and ``balance`` follow the same rule: every
  number with ``NUMERIC`` significant digits (default ``18``, exact — a
  rewritten dataset trains on exactly the values the command computed,
  e.g. the ``--rescale`` weights of ``balance``), and a WARNING below 17.
  So do the LAMMPS data and VASP POSCAR exports of ``tadah data write``
  (both formats read E-notation; ``tadah`` reads its POSCAR export back
  exactly); the CASTEP cell export keeps ``NUMERIC`` decimal places, with
  no warning.
* Other outputs (predictions, analysis tables) keep their established
  meaning of ``NUMERIC``, but the wider default applies to them as well.

The same rules apply to ``tadah refit`` (a ``NUMERIC`` in the refit
configuration sets the precision of ``pot.tadah``), to ``tadah refit
--edit`` (a ``NUMERIC`` carried by the edited potential, or set for the
run, is honoured — the edit is then exact only up to that rounding, and a
WARNING says so) and to the potentials ``tadah hpo`` writes (see
:ref:`hpo_potential_archives`).

.. note::

   Potentials written by ``tadah hpo`` 1.5.0 carry an automatically
   inserted ``NUMERIC 12`` line. When such a file is used as a
   configuration, or edited with ``tadah refit --edit``, that line counts
   as an explicit setting: delete it to get exact output again.

weights.tadah format
~~~~~~~~~~~~~~~~~~~~

Written only when ``--uncertainty`` is requested. Three
whitespace-separated columns with a single header line::

   Index   Mean weight        Uncertainty
       0   1.420421e+01       3.117841e-02
       1   2.604397e+03       4.882103e+00
       …

``Index`` matches the ordering of the ``WEIGHTS`` row in
``pot.tadah``.

Diagnostic output (verbose 2)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

With ``--verbose 2`` (INFO level) the engine logs progress to stdout:
load-time transform summaries, neighbour-finding wall-time, the
pre-flight audit report, and the regression solver status. None of
these lines are written to files; capture them with shell redirection
if needed (``tadah train -c config.train -v 2 2>&1 | tee train.log``).

See also
--------

* :ref:`training` — quickstart introduction.
* :ref:`ConfigSection` — every configuration key, with defaults.
* :ref:`prediction` — using a ``pot.tadah`` with ``tadah predict``.
* :ref:`nested_fitting` — driving training from an outer
  hyperparameter optimiser.
