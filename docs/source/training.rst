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
``--uncertainty``  ``UNCERTAINTY``  Write every weight with its standard deviation, ``weights.tadah``; needs ``OALGO 3`` (default off).
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
#. **Write ``pot.tadah``** (or whatever ``--outfile`` requests) and, with
   ``--uncertainty``, ``weights.tadah``.

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

Two of them — ``LSCALE`` and the resolved ``ESHIFT`` — belong to the model,
its length scale and its energy reference: they are key lines of
``pot.tadah``, and ``tadah predict`` applies them to the data it predicts.
The others (``EFILTER``, ``FFILTER``, ``ESHIFT_ATOM``, ``ESHIFT_DBATOM``,
``EWEIGHT_TEMP``, ``ZERO_COM_FORCE``, ``WDBFILE``, ``WDBFILE_AUTO``) only
prepared the training data: the potential lists them in its
:ref:`training record <training_record>`, not as key lines, ``tadah
predict`` does not apply them (:ref:`load_transforms_predict`), and a
retrain from the potential does not take them from it
(:ref:`retrain_from_potential`). For the per-key CLI form and defaults see
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
``FFILTER  <thr>``           Drop configurations with an atom whose force magnitude is above ``thr``.
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
builds (1.3.0-beta.1, which introduced ``LSCALE``, to 1.5.x, and 1.6.0
development builds without this change) left the stress unchanged, which
made the targets of a ``STRESS true`` fit too large by ``s³`` (6 % at
``s = 1.02``); their log line reads ``LSCALE: ... energy and stress
invariant``, this build's ``... stress by 1/s^3; energy invariant``. ``s = 1``
(the default) is a no-op and ``s <= 0`` is a configuration error.

The intended use is matching a DFT lattice constant to experiment: set
``LSCALE = a_exp / a_DFT`` so the trained model's equilibrium volume
sits at the experimental scale.

``LSCALE`` is stamped into ``pot.tadah``. ``tadah predict`` re-applies
it to the dataset it reads, so a raw DFT dataset can be given as it is;
both the reference (``True``) and the predicted columns it writes are in
the rescaled frame. ``--lscale s`` replaces the stored value; pass
``--no-lscale`` to ignore it when the dataset is already at the trained
scale. The LAMMPS ``pair_style tadah`` does **not** re-apply ``LSCALE`` —
supply LAMMPS with positions already at the trained scale.

.. versionchanged:: 1.6.0
   ``tadah predict`` applies the potential's ``LSCALE``. Earlier versions
   (1.5.x included) replaced it with 1.0, the predict task's default, unless
   ``--lscale`` was given: the data predicted was not rescaled, as with
   ``--no-lscale``.

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
  compositional diversity but no isolated-atom configs. When the
  compositions do not determine the fit (every structure of one
  composition, say) its normal matrix is singular: every ``ESHIFT`` is
  then 0, with a WARNING.

Whichever path is used, the *resolved* per-element values are stamped
into ``pot.tadah`` as ``ESHIFT``, the model's energy reference; the
``ESHIFT_ATOM`` / ``ESHIFT_DBATOM`` toggle that derived them is listed in
the :ref:`training record <training_record>` only. ``tadah predict``
subtracts the stamped values from the reference energies of the data it
predicts (``--eshift`` replaces them) and never derives a shift from that
data: ``--no-eshift`` subtracts none, so ``energy.pred`` compares the
model's energies, which do not change, with the reference energies as they
are. A potential of an earlier version has its toggle as a key line beside
``ESHIFT``; ``tadah predict`` ignores it too.

.. versionchanged:: 1.6.0
   The toggles are no key lines of a potential. Earlier versions wrote them
   as the configuration set them, so ``tadah predict`` of a potential
   trained with one of them ``true`` printed ``WARNING: Multiple ESHIFT
   sources set; precedence: explicit > ATOM > DBATOM.``, and with
   ``--no-eshift`` the toggle derived a shift from the dataset predicted
   (``ESHIFT_DBATOM`` a least-squares fit to it) and subtracted it from its
   reference energies.

.. note::

   **DM_REAM potentials (EMBF0 ZERO, the default).** The zero-referenced
   embedding fixes the isolated-atom energy of a DM_REAM model at 0, so the
   model has no free per-atom constant unless ``BIAS true`` is set. When the
   reference energies are absolute (e.g. DFT totals), remove the per-atom
   reference with ``ESHIFT``, ``ESHIFT_ATOM`` or ``ESHIFT_DBATOM`` (which
   keeps the LAMMPS ``EAMFAST`` kernel usable) or set ``BIAS true`` (its
   intercept columns, one per ``ATOM`` species, precede the descriptor
   weights, so ``FIXINDEX``/``FIXWEIGHT`` indices shift; see
   :ref:`the bias term <bias_term>`).
   ``tadah train`` prints a HINT when neither is present. A DM_REAM
   configuration must also declare its density form ``RHOFORM`` (see
   `pot.tadah format`_). See :ref:`DM_REAM <dm_ream>`.

Filtering and weighting
~~~~~~~~~~~~~~~~~~~~~~~

* ``EFILTER lo hi`` drops any configuration whose **per-atom** energy
  ``E/N`` is outside ``[lo, hi]``; ``FFILTER thr`` drops any
  configuration with a single-atom force magnitude above ``thr``. Both
  run first. ``tadah predict`` applies them to the data it predicts only
  when it is given them (``--efilter``, ``--ffilter``), never the
  potential's (:ref:`load_transforms_predict`).
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

.. _load_transforms_predict:

The transforms at predict time
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

A potential has the model's ``LSCALE`` and resolved ``ESHIFT`` as key
lines, and ``tadah predict`` applies them to the data it predicts
(``--lscale`` and ``--eshift`` replace them, ``--no-lscale`` and
``--no-eshift`` cancel them). The keys that prepared the training data are
not the potential's: ``tadah predict``
applies none of them from the potential, so every structure it is given is
predicted as it is. A filter applies only when predict is given it:

* ``--efilter lo hi`` and ``--ffilter thr`` leave out the structures
  outside the window, or with an atom whose force magnitude is above the
  threshold (``energy.pred`` numbers the structures it keeps from 0, and at
  ``-v 2`` a line says how many were left out: ``Outlier filter: dropped 18
  by energy, 0 by force.``); ``--zero-com-force`` subtracts each
  structure's mean force from the forces it reads (the ``True`` columns).
* ``WDBFILE``, ``WDBFILE_AUTO``, ``EWEIGHT_TEMP``, ``ESHIFT_ATOM`` and
  ``ESHIFT_DBATOM`` are never applied: they weighted the training set or
  derived its shift, and the data predicted keep their own weights; nor are
  those of a ``TASK predict`` of a task file.

A potential of an earlier version has these keys as key lines; ``tadah
predict`` ignores them, and at ``-v 2`` one line names those it ignored
that are not at their default::

   INFO: tadah predict: old.tadah: EFILTER -0.6 -0.4, ESHIFT_DBATOM true
   and ZERO_COM_FORCE true prepared the training data of the potential and
   are not applied to the data predicted (tadah keeps such keys in a
   potential's training record, not as key lines); --efilter, --ffilter and
   --zero-com-force filter the data predicted.

.. versionchanged:: 1.6.0
   Earlier versions applied a potential's ``EFILTER`` and ``FFILTER`` to
   the data it predicted (a structure outside the training window was left
   out of ``energy.pred`` and of the ``-A`` metrics) and its
   ``ESHIFT_ATOM``/``ESHIFT_DBATOM`` with ``--no-eshift``; its
   ``ZERO_COM_FORCE true`` was not applied (the predict task's default,
   ``false``, replaced it).

A retrain from the potential (:ref:`retrain_from_potential`) does not take
them from the potential either.

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
``weights.tadah``  Every weight with its standard deviation, written only when
                   ``--uncertainty`` / ``UNCERTAINTY true`` is set (``OALGO 3``).
================== ============================================================================

A potential file that cannot be written (its directory does not exist, or
a directory has its name) stops ``tadah train`` with an error that names
the file and a non-zero exit status; it used to log an ``ERROR`` line and
exit 0 without a potential.

pot.tadah format
~~~~~~~~~~~~~~~~

The potential file is plain ASCII. Each key line is one ``KEY VALUE[…]``
pair using the same syntax as the training configuration file
(whitespace separates fields; comments and blank lines are not
required). The file is round-tripped through ``tadah predict`` and
``pair_style tadah`` — every field needed to reproduce the prediction
is stored here. Above the key lines, a block of comment lines, the
:ref:`training record <training_record>`, names the datasets and the
settings of the run that wrote it, for your record; no key is read from
it.

A minimal single-species, two-body, kernel-ridge example::

   # --- training record (no key is read from it) ---
   # DBFILE db.train
   # --- end of training record ---
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

Three categories of key lines appear:

#. **Configuration echo** — every key that fully specifies the model
   geometry: descriptor type and parameters (``TYPE2B``, ``RCUT2B``,
   ``RCTYPE2B``, ``INIT2B`` / ``INITMB``, …), model choice and
   hyperparameters (``MODEL``, ``LAMBDA``, ``NORM``, ``BIAS``, …), and
   the species list (``ATOM`` / ``WATOM``), with the other keys of the
   training configuration but ``DBFILE``, the keys that prepared the
   training data and the settings of the run: the
   :ref:`training record <training_record>` above the key lines names those
   (see also :ref:`retrain_from_potential`).

#. **Fitted parameters** — the row ``WEIGHTS …`` carries the learned
   coefficients :math:`\mathbf w`. The ordering matches the column
   layout of the design matrix and is reproduced exactly on load.

#. **Stamped load-time transforms** — the model's length scale and
   energy reference: ``LSCALE`` and, when the training run resolved one
   (explicit or via ``ESHIFT_ATOM`` / ``ESHIFT_DBATOM``), ``ESHIFT``, so
   prediction re-applies them automatically and a retrain from the
   potential uses them. The ``ESHIFT_ATOM`` / ``ESHIFT_DBATOM`` toggles,
   the filters and the weights of the training data are no key lines (the
   :ref:`training record <training_record>` lists them).

DM_REAM potentials always carry explicit ``EMBF0`` and ``RHOFORM`` lines;
the writer spells ``RHOFORM`` in upper case and drops a stray one from a
potential without a DM_REAM block (a notice on standard error, once per
process: :ref:`RHOFORM and EMBF0 without a DM_REAM block <dm_ream_only_keys>`). ``RHOFORM`` has no
default: a DM_REAM training configuration without it stops before any
fitting with the migration message (``RHOFORM SIGNED`` is the density of
every tadah before 1.6.0); see :ref:`DM_REAM <dm_ream_rhoform>`.

.. _training_record:

Training record
^^^^^^^^^^^^^^^

The key lines are what is needed to use the model: its settings and its
fit, its ``LSCALE`` and its resolved ``ESHIFT``. The keys a potential does
not use are not key lines: the training datasets, ``DBFILE``; the keys that
only prepared the training data, ``EFILTER``, ``FFILTER``, ``ESHIFT_ATOM``,
``ESHIFT_DBATOM``, ``EWEIGHT_TEMP``, ``ZERO_COM_FORCE``, ``WDBFILE`` and
``WDBFILE_AUTO`` (:ref:`load_transforms`); and these settings of the run:
``VERBOSE``, ``NUMERIC``, ``OUTFILE``, ``AUDIT``, ``AUDIT_SAMPLE``,
``HEALTH_LOG`` and ``UNCERTAINTY``, and the ``HPOTARGET`` and
``VALIDATION`` of a ``tadah hpo`` run. For your record the potential keeps
the values its run was given in its *training record*, a block of comment
lines right above the key lines::

   # --- training record (no key is read from it) ---
   # DBFILE db1.train db2.train
   # WDBFILE 1 0.5
   # VERBOSE 0
   # OUTFILE pot2.tadah
   # --- end of training record ---

It has one line for each of these keys the run was given, in the order
``DBFILE``, ``EFILTER``, ``FFILTER``, ``ESHIFT_ATOM``, ``ESHIFT_DBATOM``,
``EWEIGHT_TEMP``, ``ZERO_COM_FORCE``, ``WDBFILE``, ``WDBFILE_AUTO``,
``VALIDATION``, ``HPOTARGET``, ``UNCERTAINTY``, ``VERBOSE``, ``NUMERIC``,
``OUTFILE``, ``AUDIT``, ``AUDIT_SAMPLE``, ``HEALTH_LOG``, with the values
as the configuration file, the task file or the command line wrote them
(``NUMERIC`` does not change them). A key nobody set is not
listed: the defaults tadah takes (``VERBOSE 1``, ``OUTFILE pot.tadah``,
``ESHIFT_ATOM false``, ``WDBFILE_AUTO 0``, ...) are left out, so ``tadah train -c config.train`` records the ``DBFILE`` of
``config.train`` alone, and ``-o pot2.tadah -v 0`` adds ``VERBOSE 0`` and
``OUTFILE pot2.tadah``. The potentials of ``tadah train``, ``tadah refit``
and ``tadah hpo`` (``best_pot.tadah``, its rank-local copies and the
``DUMP`` and ``BDUMP`` archives) have it; a run that set none of these keys
writes no block. ``tadah refit --edit`` copies the record of its input
(:ref:`refit_edit`).

No key is read from the block: ``tadah predict``, ``tadah train -c``,
``tadah hpo -c``, ``pair_style tadah`` and ``tadah refit --edit``, when they
read the potential's keys, skip every line that starts with ``#``. A
potential loads, predicts and retrains as it would without the block, and
the ``VERBOSE`` of its record does not set the log level. A potential
without a record (one of an earlier version) loads as before. ``tadah refit
--edit`` alone reads the block itself, to copy it into the edit
(:ref:`refit_edit`): what you write in it is carried into the edited
potential, and the edit is refused, naming the file and the lines, when the
block is malformed: an opening line without its end line (since every
writer puts the key lines right after the record, deleting the end line
makes the refusal name the opening line and the potential's first key
line), an end line without its opening line, or a line inside the block
that does not start with ``#``. Nothing else reads the block.

A potential of an earlier version may have some of these keys as key
lines (an ``M_BLR`` potential its ``DBFILE``, a potential of tadah 1.5.x
its ``UNCERTAINTY``, a ``best_pot.tadah`` its run's ``HPOTARGET``,
``VALIDATION`` and ``VERBOSE``, and every potential of ``tadah train`` or
``tadah hpo`` the keys that prepared its training data, ``ESHIFT_ATOM
false``, ``ESHIFT_DBATOM false``, ``WDBFILE_AUTO 0`` and ``ZERO_COM_FORCE
false`` when its configuration set none); it still loads, and ``tadah refit
--edit`` of it lists them in the edit's record instead. ``tadah predict``
ignores the keys that prepared the training data
(:ref:`load_transforms_predict`) and a retrain from the potential those of
its file (:ref:`retrain_from_potential`). ``tadah predict`` and
``pair_style tadah`` read none of the other keys except ``VERBOSE``: parsing a file
with a ``VERBOSE`` key line sets the Tadah log level of the process
(``pair_style tadah`` in LAMMPS, a retrain with ``tadah train -c`` without
``-v``), and the model built from the file takes that verbosity (a kernel
such as ``Kern_RBF`` prints its parameters, ``Kern_RBF | sigma: 1 | gamma:
1``, when it is built at ``VERBOSE`` 1 or more, the default of a file
without the line: for every model that ``tadah train``, ``tadah predict``,
``pair_style tadah`` or a task of a task file builds; a ``tadah hpo`` run
prints it once, for its ``-c`` model, though it builds a model for every
evaluation).

.. versionchanged:: 1.6.0
   The training record is new (MK's decisions of 2026-10-14). Its opening
   line is ``# --- training record (no key is read from it) ---``; ``tadah
   refit --edit`` refuses a block opened by any other line at its end
   line. tadah 1.5.x
   wrote some of these keys as key lines (the ``DBFILE`` of an ``M_BLR``
   potential, ``UNCERTAINTY``, a ``best_pot.tadah``'s ``HPOTARGET``,
   ``VALIDATION``, ``VERBOSE`` and ``NUMERIC``, ``AUDIT_SAMPLE``, and the
   keys that prepared the training data) and left the others out; 1.6.0 development builds between 2026-10-12 and 2026-10-14 left all
   of them out of the potential.

.. _retrain_from_potential:

Retraining from a potential
~~~~~~~~~~~~~~~~~~~~~~~~~~~

A potential's key lines are the model's settings and its fit, not the
training data: no potential that ``tadah train``, ``tadah hpo`` or ``tadah
refit`` writes has a ``DBFILE`` line, whichever the model (its
:ref:`training record <training_record>`, a comment, names the datasets its
weights were fitted to, when there were any: a refit has none, and a
potential of an earlier version has no record). A potential is a training configuration without its
datasets, so it retrains with the ``DBFILE`` lines of the retrain added to a
copy of it::

   cp pot.tadah retrain.train
   echo "DBFILE db.train" >> retrain.train
   tadah train -c retrain.train -o pot2.tadah

``pot2.tadah`` records the retrain: its training record names the
``DBFILE`` of the retrain (``db.train``) and the retrain's other keys, not
the datasets ``pot.tadah`` was trained on. The record of ``pot.tadah``, a
comment in the copy, is neither read nor copied: it describes another fit,
of other weights, which ``pot.tadah`` itself keeps.

Without the ``DBFILE`` line ``tadah train`` refuses the configuration
before it reads anything::

   The training configuration has no DBFILE: tadah train fits the datasets
   it lists. A potential has no DBFILE line (no potential tadah writes one;
   its training record, the comment block above its key lines, lists the
   datasets its weights were fitted to in a '# DBFILE' line when there were
   any: a refit has none, and a potential of an earlier version has no
   record): to train from one, add a DBFILE line to a copy of it; a
   potential of tadah refit or refit --edit also pins every weight with its
   FIXINDEX and FIXWEIGHT lines, the seed of a tadah hpo coefficient search,
   so delete them from the copy for the fit to change the weights (the
   Training page of the documentation, Retraining from a potential).

A potential that ``tadah refit`` or ``tadah refit --edit`` writes, and the
``best_pot.tadah`` of a ``tadah hpo`` run that pins every weight, has
``FIXINDEX`` and ``FIXWEIGHT`` lines for every weight: the seed of a
``tadah hpo`` coefficient search (:ref:`hpo_fixweight`).  A fit whose
``FIXINDEX`` pins every weight changes none, so for a retrain delete both
lines from the copy as well::

   grep -v -e '^FIXINDEX' -e '^FIXWEIGHT' pot.tadah > retrain.train
   echo "DBFILE db.train" >> retrain.train
   tadah train -c retrain.train -o pot2.tadah

With the lines kept, ``tadah train`` writes a potential whose ``WEIGHTS``
are the ``FIXWEIGHT`` values and says so before it reads a dataset::

   WARNING: tadah train: FIXINDEX and FIXWEIGHT pin every weight (all 84 of
   them), so the fit changes none: the WEIGHTS of the potential written are
   the FIXWEIGHT values. tadah refit and refit --edit write FIXINDEX and
   FIXWEIGHT for every weight of their potentials, the seed of a tadah hpo
   coefficient search, and so does a tadah hpo run that pins every weight in
   its best_pot.tadah: to fit the weights to the DBFILE data, delete both
   lines from the training configuration (the Training page of the
   documentation, Retraining from a potential).

It is a warning, not a refusal: such a fit can be deliberate (under
``OALGO 3`` it gives the noise of the data about the pinned weights).  Until
2026-10-13 nothing said so, and a refit potential retrained this way kept
its weights.

``tadah hpo`` runs without a training ``DBFILE`` only when ``FIXINDEX`` and
``FIXWEIGHT`` pin every weight (:ref:`hpo_fixweight`). Its refusals of a
``-c`` configuration without one end with the same advice where the weights
cannot be determined (no ``FIXINDEX``/``FIXWEIGHT``, pins that do not cover
every weight, a basis drawn from the training data) and where ``ATOM`` is
missing. With every weight pinned, ``GRAD_STRATEGY ANALYTICAL`` is refused
for the missing ``DBFILE`` with the advice to set ``GRAD_STRATEGY HYBRID``
instead: with a ``DBFILE`` line the pins would be refused next (``FIXINDEX
pins model weights``).

The fit replaces what the potential holds of its own fit (``WEIGHTS``,
those ``FIXINDEX`` pins excepted, the ``BASIS`` of a non-linear kernel,
``NMEAN``, ``NSTDEV``, ``SIGMA``); none of
it is read when the model is built, so the retrain may also change, for
example, ``SBASIS`` or the descriptor. The other key lines are the model of
the first fit, with its ``LSCALE`` and ``ESHIFT``: the energy reference of
the retrain is the potential's.

The keys that prepared the training data of the first fit (``EFILTER``,
``FFILTER``, ``ESHIFT_ATOM``, ``ESHIFT_DBATOM``, ``EWEIGHT_TEMP``,
``ZERO_COM_FORCE``, ``WDBFILE``, ``WDBFILE_AUTO``) are in its training
record, not among its key lines, and a retrain does not take them from the
potential, whatever version wrote it: a retrain works with any number of
datasets, and with none of the first fit's filters or weights. Give the
retrain's own on the command line, where they are recorded in the new
potential's record::

   tadah train -c retrain.train -o pot2.tadah --wdbfile 1 0.5 --efilter -9 -2

A configuration that has ``WEIGHTS`` (a potential, a copy of one, or a file
that includes one with ``CONFIG``; a ``tadah hpo -c`` seed too) is taken as
a potential: those of the eight that its files give are ignored, and a
``WARNING`` names those that are not at their default, for a potential of
an earlier version, which has them as key lines, or a line you added to the
copy::

   WARNING: tadah train: the configuration file retrain.train is a potential
   (it has WEIGHTS): its EFILTER 5 6 and WDBFILE 1 1 prepared the training
   data of the fit that wrote it, not of this run, and are ignored (tadah
   keeps such keys in a potential's training record, not as key lines). Give
   --efilter or --wdbfile on the command line to apply one to this run (the
   Training page of the documentation, Retraining from a potential).

To derive ``ESHIFT`` from the data of the retrain instead of using the
potential's, delete the ``ESHIFT`` line from the copy and set
``ESHIFT_ATOM`` or ``ESHIFT_DBATOM`` (in the copy or on the command line):
without an ``ESHIFT`` line the toggle is the only source of the shift and
applies. Beside the ``ESHIFT`` line a toggle of the copy is ignored (the
explicit values overrule it in any configuration), and the warning says so.
To reproduce the first fit, give the keys its record lists on the command
line.

.. versionchanged:: 1.6.0
   A potential had the keys that prepared its training data as key lines,
   and a retrain applied them to its own datasets: a ``WDBFILE`` of the
   first fit's datasets stopped a retrain with another number of datasets
   (``WDBFILE size (2) does not match number of datasets (3).``), and the
   first fit's ``EFILTER``, ``FFILTER``, ``EWEIGHT_TEMP`` and
   ``ZERO_COM_FORCE`` applied to the retrain's.

With ``LAMBDA -1`` the potential records the ``ALPHA`` and ``BETA``
the evidence approximation found (the fit's :math:`\lambda = \alpha/\beta`;
they are the approximation's starting guesses in a configuration), so a
retrain starts the approximation from them (on the same datasets it stops
there, to its convergence tolerance).

A ``DBFILE`` that lists one dataset file twice (the same path, two spellings
of it, or a link to it) is refused, naming both entries, by ``tadah train``
and, at startup, by ``tadah hpo``: each entry is a dataset, so each of its
structures would be fitted twice. To give a dataset more weight, use
``WDBFILE``.

.. versionchanged:: 1.6.0
   An ``M_BLR`` potential had the ``DBFILE`` line it was trained on (an
   ``M_KRR`` potential did not), so the retrain above listed its dataset
   twice: every structure was fitted twice, a fixed ``LAMBDA`` acted at half
   strength and the ``OALGO 3`` ``SIGMA`` was that of twice the data, and
   nothing said so. An ``M_BLR`` potential written by Tadah! 1.5.x or
   earlier, or a 1.6.0 development build, still has its ``DBFILE`` line
   (``tadah predict`` and ``pair_style tadah`` do not read it): remove it
   before you add the ``DBFILE`` of a retrain (the duplicated entry is
   refused), or train from it as it is, on the dataset it names. ``M_KRR``
   potentials did not record ``ALPHA`` and ``BETA``; they do now, as
   ``M_BLR`` ones did. ``tadah train`` with a configuration without
   ``DBFILE`` (an ``M_KRR`` potential as it is) stopped with ``Key not found
   in the Context object: DBFILE``.

.. _pot_number_format:

Number format
~~~~~~~~~~~~~

Every non-integer number of a potential's key/value lines is written
with ``NUMERIC`` **significant** digits (``printf`` ``%.{N}g``; those of
the cutoffs and grids exactly, see below). The default, ``18``, is exact:
17 significant digits identify every double, so a potential reloaded from
``pot.tadah`` is bit-identical to the one in memory. That is why a value typed as ``5.3`` comes back as
``5.29999999999999982`` — the exact double nearest to 5.3 — and why
``1.0`` is written as ``1``. Integers, names, booleans, index lists and
grid generators (``LIN`` / ``GEOM``) are written as they are stored, so
integers are always exact. ``#`` comment lines (such as the header
``tadah hpo`` writes) are annotations that ``NUMERIC`` does not govern.

* ``NUMERIC`` itself is never written into a potential file.
* An explicit ``NUMERIC`` below 17 is honoured (the numbers are rounded to
  that many significant digits) but ``tadah`` prints a WARNING: the
  written potential then differs slightly from the fitted one.
* The cutoffs and the radial grids, ``RCUT2B``, ``RCUTMB``, ``CGRID2B``,
  ``CGRIDMB``, ``SGRID2B`` and ``SGRIDMB``, are written exactly whatever
  ``NUMERIC`` is (below 17 as the shortest text that reads back as the
  same value: ``RCUT2B 5.58190742024`` at ``NUMERIC 6``). A load checks
  them exactly: ``D2_EAM`` and ``DM_EAM`` refuse a cutoff other than that
  of their setfl file, a knot channel's cutoff is its largest knot, and
  the knot and Blip descriptors warn of a knot or a bump past the cutoff.
  So a potential written with a low ``NUMERIC`` loads with the cutoffs and
  grids it was fitted with; only the fitted numbers (``WEIGHTS``,
  ``WCOV``, ``NMEAN``, ...) and the other settings are rounded. Earlier
  versions rounded the cutoffs and grids too: such an EAM potential is
  refused until its ``RCUT2B``/``RCUTMB`` is set to the setfl file's
  cutoff; and the cutoff, a bump's centre and its width, each rounded on
  its own, could put a ``Cut_Dummy`` bump that ended at or just inside the
  cutoff past it, which every load warns of and ``tadah hpo`` refuses in
  its ``-c`` potential (the bump-support gate, whose message names the
  width that brings the bump inside). The configuration-keys page marks
  these keys (*In a potential file*).
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

.. _weight_covariance:

Weight covariance and weights.tadah
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

A fit with ``OALGO 3`` (the SVD solve) computes the covariance of its
weights, the posterior covariance of Bayesian linear regression,

.. math::

   \Sigma_w = s^2 \left(\Phi^\top W \Phi + \lambda I\right)^{-1},
   \qquad
   s^2 = \frac{\mathrm{RSS}}{n_\mathrm{obs} - \gamma},
   \qquad
   \gamma = \sum_{i\ \mathrm{kept}} \frac{s_i^2}{s_i^2 + \lambda},

with :math:`\Phi` the design matrix, :math:`W` the row weights
(``eweight`` × ``EWEIGHT`` and so on), :math:`\lambda` the fit's ``LAMBDA``
(the evidence approximation's for ``LAMBDA -1``), RSS the weighted residual
sum of squares, :math:`n_\mathrm{obs}` the number of rows with a non-zero
weight (the observations: structures stored with ``fweight 0`` add no force
rows, for example) and :math:`s_i` the singular values of
:math:`W^{1/2}\Phi`. :math:`s^2` is the noise variance of a row of unit
weight. The solve keeps a singular direction when its :math:`s_i` is above
``rcond`` times the largest (``rcond`` the second value of ``OALGO``,
1e-8 by default); :math:`\gamma` sums over the kept directions, and the
inverse is over them too: a direction the solve leaves out of the weights
is left out of :math:`\Sigma_w` and of :math:`\gamma`, and a WARNING gives
their number. ``pot.tadah`` gets two keys:

- :ref:`WCOV`: N, then the N×N matrix :math:`\Sigma_w` column-major, in the
  parameterisation of ``WEIGHTS`` (with ``NORM true`` and ``BF_Linear`` or
  ``Kern_Linear`` that of the weights of the raw descriptors). A weight
  ``FIXINDEX`` pins has no variance: its row and column are 0.
- :ref:`WNOISE`: the noise variance of one value of each row type the fit
  had rows of with a non-zero weight, :math:`s^2/W_t` for ``E``
  (``EWEIGHT``), ``F`` (``FWEIGHT``) and ``S`` (``SWEIGHT``), in raw units
  (eV², (eV/Å)², (eV/Å³)²), for a structure of weight 1. A fit without
  energies of a non-zero weight (``EWEIGHT 0``) lists no ``E``: ``tadah
  predict --error`` refuses such a potential, since every prediction has an
  energy, and ``tadah train`` warns when it writes it.

When the fit has no more observations than effective weights
(:math:`n_\mathrm{obs} \le \gamma`) the noise variance is undefined: no
``WCOV`` and ``WNOISE`` are written, and a WARNING says why. Any other
``OALGO`` writes neither. ``tadah predict --error`` reads them (see
:ref:`predict_error`).

``--uncertainty`` writes ``weights.tadah``: every weight with its standard
deviation, the square root of the diagonal of :math:`\Sigma_w` (0 for a
pinned weight)::

   Index   Mean weight        Std. dev.
       0   1.420421e+01       3.117841e-02
       1   2.604397e+03       4.882103e+00
       …

``Index`` matches the ordering of the ``WEIGHTS`` row in ``pot.tadah``.
``UNCERTAINTY`` is a switch of the run, not part of the model: a potential
has no ``UNCERTAINTY`` line (its :ref:`training record <training_record>`
lists the run's). A potential given as the configuration (``tadah train
-c pot.tadah``, with a ``DBFILE`` line added) does not ask for
``weights.tadah`` with an ``UNCERTAINTY true`` line, which tadah 1.5.x and
1.6.0 development builds wrote into every potential trained with
``--uncertainty``: a WARNING names the file, and ``--uncertainty`` asks for
it. ``UNCERTAINTY true`` in a training configuration or a task file is a
request, and a refusal names where it came from.
``--uncertainty`` is refused before training with an ``OALGO`` other than 3
and with M_KRR implementation 2 (``MODEL M_KRR <kernel> 2``, whose solve of
the kernel matrix computes no covariance: use implementation 1), and before
``pot.tadah`` is written when the covariance is undefined; the message says
why and what to change (for too few observations: add data with a non-zero
weight).

.. versionchanged:: 1.6.0
   ``WCOV`` and ``WNOISE`` replace ``SIGMA``, which Tadah! 1.0.0 to 1.5.0
   (and 1.6.0 development builds before this change) wrote with a
   transposed formula
   (:math:`V^\top R V` instead of :math:`V R V^\top`) and without
   :math:`s^2`; it is a deprecated key now, ignored with a notice. The third
   column of ``weights.tadah``, headed ``Uncertainty``, was its diagonal,
   and with ``FIXINDEX`` the file had a row per free weight only. The
   evidence approximation (``LAMBDA -1``) counts the observations alone
   (it counted the design matrix's augmentation rows, one per weight, and
   its rows of weight 0), so the λ and the weights of a ``LAMBDA -1`` fit
   change: by a little when only the augmentation rows were counted, by
   orders of magnitude in λ when the fit has many rows of weight 0 (a
   ``FORCE true`` fit of structures stored with ``fweight 0``).

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
