.. _training_page:

Training
========

This page covers the ``tadah train`` command in depth. :ref:`training` in
the quickstart gives a one-shot introduction, and :ref:`ConfigSection` the
full key-by-key reference.

Command
-------

::

   tadah train -c <config.train> [--force] [--stress] \
               [--outfile <pot.tadah>] [--verbose <0|1|2>]

The required input is a single :ref:`training configuration <config_file>`
(``--config`` / ``-c``) that declares the dataset(s) via ``DBFILE``,
the descriptor(s), and the regression model. Every CLI flag maps
one-to-one to an upper-case configuration key (e.g. ``--force`` ↔
``FORCE true``); CLI flags override file values.

The options of ``tadah train``, with their keys and defaults, are listed
in the command-line reference (:doc:`cli/cli`).

Training data must be supplied as one or more Tadah! database files via
``DBFILE`` in the configuration; :ref:`dataset` gives the on-disk format.

Pipeline
--------

For each ``tadah train`` invocation the engine runs the following stages
in order:

#. **Loading the structures** declared by every ``DBFILE`` key into memory.
#. **Applying load-time data transforms** (``LSCALE``, ``ESHIFT*``,
   ``EFILTER`` / ``FFILTER``, ``ZERO_COM_FORCE``; :ref:`load_transforms`).
#. **Building the neighbour list** out to ``RCUTMAX``: every periodic image
   of every atom closer than the cutoff, for any cell (triclinic, or
   thinner than the cutoff) and for atoms inside or outside the cell
   (:doc:`API/nn_finder`).
#. **Running the pre-flight audit** when enabled (:ref:`AUDIT`, ``off`` by
   default): it checks the parameters of each descriptor against the
   neighbour distances of each element pair it sums over. These are the
   pairs its :ref:`TYPE2B <TYPE2B>` or :ref:`TYPEMB <TYPEMB>` line names,
   or the ``PAIRS`` of its ``DESCRIPTOR`` block.
#. **Fitting the regression model** declared by ``MODEL``: it minimises
   the sum of :ref:`fit_objective`, with the :term:`error scales
   <error scale>` of the configuration and the :term:`priorities
   <priority>` of the data (``WDBFILE`` among them).
#. **Writing the potential** (``pot.tadah``, or the file ``--outfile`` names).

``tadah predict`` agrees with ``pair_style tadah`` in LAMMPS on every cell.

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
``ZERO_COM_FORCE``) only prepared the training data, and ``WDBFILE``, the
priority of each dataset, is a setting of the fit (:ref:`fit_objective`):
the potential lists them in its
:ref:`training record <training_record>`, not as key lines, ``tadah
predict`` does not apply them (:ref:`load_transforms_predict`), and a
retrain from the potential does not take them from it
(:ref:`retrain_from_potential`). :ref:`ConfigSection` and ``tadah explain
<key>`` (e.g. ``tadah explain train.lscale``) give the per-key CLI form and
defaults.

Order of application
~~~~~~~~~~~~~~~~~~~~

The transforms run in a fixed order, and the order matters because
several feed the next:

#. **Outlier filter** (``EFILTER`` / ``FFILTER``) — runs first, so an
   extreme outlier cannot poison the later least-squares step (the
   ``ESHIFT_DBATOM`` fit).
#. **Length rescale** (``LSCALE``).
#. **ESHIFT** — the per-element shifts are determined, then subtracted.
#. **COM-force zeroing** (``ZERO_COM_FORCE``).
#. **Equilibrium-volume diagnostic** — logs the lowest post-transform
   ``E/N`` structure as a reference-state sanity check.

Key summary
~~~~~~~~~~~

============================ ===========================================================================
Key                          One-line summary
============================ ===========================================================================
``LSCALE``                   Uniform length rescale (DFT → experimental scale).  Persisted to pot.tadah.
``ESHIFT``                   Explicit per-element energy shift; one value per element ordered by Z.
``ESHIFT_ATOM``              Derives ESHIFT from isolated-atom structures in the dataset.
``ESHIFT_DBATOM``            Derives ESHIFT by least-squares atomic-energy fit over the database.
``EFILTER  <lo> <hi>``       Drops structures whose per-atom energy is outside ``[lo, hi]``.
``FFILTER  <thr>``           Drops structures with an atom whose force magnitude is above ``thr``.
``WDBFILE  <w_1> ... <w_n>`` Per-dataset priorities, applied in the fit; one per ``DBFILE``.
``ZERO_COM_FORCE``           Subtracts the mean force per structure.
============================ ===========================================================================

LSCALE: uniform length rescale
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``LSCALE s`` multiplies every position and cell vector by ``s``. The
total energy is invariant under the change of length scale, so forces
are divided by ``s`` and stresses (eV Å⁻³) by ``s³``: the virial
:math:`V\sigma` is invariant and the volume grows by ``s³``. The log line
reads ``LSCALE: ... stress by 1/s^3; energy invariant``. ``s = 1``
(the default) is a no-op and ``s <= 0`` is a configuration error.

The intended use is matching a DFT lattice constant to experiment:
``LSCALE = a_exp / a_DFT`` puts the trained model's equilibrium volume
at the experimental scale.

``LSCALE`` is stamped into ``pot.tadah``. ``tadah predict`` re-applies
it to the dataset it reads, so a raw DFT dataset can be given as it is;
both the reference (``True``) and the predicted columns it writes are in
the rescaled frame. ``--lscale s`` replaces the stored value;
``--no-lscale`` ignores it when the dataset is already at the trained
scale. The LAMMPS ``pair_style tadah`` does **not** re-apply ``LSCALE`` —
LAMMPS needs positions already at the trained scale.

ESHIFT: per-element energy shift
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

For every structure ESHIFT *subtracts* a per-element baseline::

   E_after = E_before − Σ_Z N_Z · ESHIFT[Z]

``ESHIFT[Z]`` is the per-atom reference energy of species ``Z`` to be
removed — feeding the isolated-atom energy turns total energies into
cohesive energies. There are three mutually exclusive ways to obtain
the values; if more than one is set the engine warns and applies the
precedence **explicit > ATOM > DBATOM**:

* **Explicit** — ``ESHIFT a b …``, one value per unique species,
  ordered by ``Z``.
* **ESHIFT_ATOM** — the mean energy of the single-atom
  (``natoms == 1``) structures of each species that have an energy
  (``eweight`` not 0). A species with no such isolated-atom structure gets
  ``0`` and a warning; a spread above 1 meV across the structures of one
  species is logged.
* **ESHIFT_DBATOM** — a least-squares atomic-energy fit over the
  structures of the database that have an energy (``min ‖y − Mβ‖²``,
  with ``M[i,k]`` the count of species ``k`` in structure ``i``). This fit
  suits a dataset with varied compositions and no isolated-atom structures.
  A species that
  only structures with ``eweight`` 0 contain has no energy to fit: its
  ``ESHIFT`` is 0, a WARNING names it, and the other species are fitted.
  When the compositions do not determine the fit (every structure of one
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
are. A potential written before Tadah! 1.6.0 has its toggle as a key line
beside ``ESHIFT``; ``tadah predict`` ignores it too.

.. note::

   **DM_REAM potentials (EMBF0 ZERO, the default).** The zero-referenced
   embedding fixes the isolated-atom energy of a DM_REAM model at 0, so the
   model has no free per-atom constant unless ``BIAS true`` is set. When the
   reference energies are absolute, such as DFT totals, ``ESHIFT``,
   ``ESHIFT_ATOM`` or ``ESHIFT_DBATOM`` removes the per-atom reference
   (which keeps the LAMMPS ``EAMFAST`` kernel usable), or ``BIAS true``
   fits it (its intercept columns, one per ``ATOM`` species, precede the
   descriptor coefficients, so ``FIXINDEX``/``FIXWEIGHT`` indices shift;
   :ref:`the bias term <bias_term>`).
   ``tadah train`` prints a HINT when neither is present. When the
   configuration is a potential (a ``tadah refit`` potential with a
   ``DBFILE`` line, say), the toggle is the run's own: ``--eshift-atom`` or
   ``--eshift-dbatom``, or a line of a file that includes the potential with
   ``CONFIG``; a toggle line of the potential is not read
   (:ref:`retrain_from_potential`). A DM_REAM
   configuration must also declare its density form ``RHOFORM``
   (`pot.tadah format`_), described in :ref:`DM_REAM <dm_ream>`.

Filters and the net force
~~~~~~~~~~~~~~~~~~~~~~~~~

* ``EFILTER lo hi`` drops any structure whose **per-atom** energy
  ``E/N`` is outside ``[lo, hi]``; ``FFILTER thr`` drops any
  structure with a single-atom force magnitude above ``thr``. Both
  run first. A structure with ``eweight`` 0 (``fweight`` 0) has no
  energy (forces) of its own and is not dropped for it. ``tadah predict``
  applies them to the data it predicts only
  when it is given them (``--efilter``, ``--ffilter``), never the
  potential's (:ref:`load_transforms_predict`).
* ``WDBFILE w_1 … w_n`` changes no structure: it is the priority of each
  dataset in the fit, one value ``>= 0`` per ``DBFILE``
  (:ref:`fit_objective`).
* ``ZERO_COM_FORCE`` subtracts the mean force of each structure
  so its net force is exactly zero — useful against residual
  translational forces left by numerical errors of the DFT calculation
  (finite basis, real-space grid, self-consistency tolerance).

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
* ``WDBFILE``, ``ESHIFT_ATOM`` and ``ESHIFT_DBATOM`` of the potential are
  never applied: they gave the training data their priorities or derived
  its shift, and the data predicted keep their own priorities.
  ``tadah predict`` has no option for them and fits nothing. A potential of
  Tadah! 1.5.0 has a line of the
  retired ``WDBFILE_AUTO``, and one trained with ``EWEIGHT_TEMP`` a line
  of it: ``tadah predict`` reads them with one notice
  (:ref:`fit_objective_conversion`).

A potential written before Tadah! 1.6.0 has these keys as key lines; ``tadah
predict`` ignores them, and at ``-v 2`` one line names those it ignored
that are not at their default::

   INFO: tadah predict: old.tadah: EFILTER -0.6 -0.4, ESHIFT_DBATOM true
   and ZERO_COM_FORCE true prepared the training data of the potential and
   are not applied to the data predicted (tadah keeps such keys in a
   potential's training record, not as key lines); --efilter, --ffilter and
   --zero-com-force filter the data predicted.

A retrain from the potential (:ref:`retrain_from_potential`) does not take
them from the potential either.

.. _train_outputs:

Output files
------------

``tadah train`` writes its results into the **current working
directory** (i.e. wherever the command was invoked).

================== ============================================================================
File               Contents
================== ============================================================================
``pot.tadah``      The trained interatomic potential. Path overridable via ``--outfile`` /
                   ``OUTFILE``. This is the only required artefact and the file consumed by
                   ``tadah predict`` and the LAMMPS ``pair_style tadah``.
================== ============================================================================

A potential file that cannot be written (its directory does not exist, or
a directory has its name) stops ``tadah train`` with an error that names
the file and a non-zero exit status.

A configuration shared with ``tadah hpo`` has no :ref:`OUTFILE` line, as a
configuration file of ``tadah hpo`` refuses one (:ref:`config_file`);
``tadah train -o`` names the potential file.

pot.tadah format
~~~~~~~~~~~~~~~~

The potential file is plain ASCII. Each key line is one ``KEY VALUE[…]``
pair using the same syntax as the training configuration file
(whitespace separates fields; comments and blank lines are not
required). The file is round-tripped through ``tadah predict`` and
``pair_style tadah`` — every field needed to reproduce the prediction
is stored here. Above the key lines, a block of comment lines, the
:ref:`training record <training_record>`, names the datasets and the
settings of the run that wrote it, for the record; no key is read from
it.

A minimal single-species, two-body, kernel-ridge example::

   # --- training record (no key is read from it) ---
   # DBFILE db.train
   # Fit balance: LAMBDA 0; ...
   #   ...
   # --- end of training record ---
   ATOM     Ta
   BIAS     false
   DIMER    false   0   false
   ENERGY   true
   ENORM    ATOM
   ESIGMA   1
   FORCE    false
   FSIGMA   0.100000000000000006   0
   INIT2B   true
   INITMB   false
   LAMBDA   0
   LSCALE   1
   MODEL    M_KRR   Kern_Linear   1
   NORM     false
   OALGO    1
   RCTYPE2B Cut_Dummy
   RCUT2B   6.5
   SSIGMA   0.5   0
   STRESS   false
   TYPE2B   D2_LJ   Ta   Ta
   WATOM    73
   WEIGHTS  14.20421334...   2604.39696519...

Three categories of key lines appear:

#. **Configuration echo** — every key that fully specifies the model
   geometry: descriptor type and parameters (``TYPE2B``, ``RCUT2B``,
   ``RCTYPE2B``, ``INIT2B`` / ``INITMB``, …), model choice and
   hyperparameters (``MODEL``, ``LAMBDA``, ``NORM``, ``BIAS``, …), the
   error scales of the fit (``ESIGMA``, ``FSIGMA``, ``SSIGMA``, ``ENORM``,
   ``ENERGY``: :ref:`fit_objective`), and the species list (``ATOM`` /
   ``WATOM``), and every other key of the training configuration except
   ``DBFILE``, the keys that prepared the training data and the settings of
   the run. The :ref:`training record <training_record>` above the key lines
   names those (:ref:`retrain_from_potential`).

#. **Fitted parameters** — the row ``WEIGHTS …`` carries the learned
   coefficients :math:`\mathbf w`. The ordering matches the column
   layout of the design matrix and is reproduced exactly on load.

#. **Stamped load-time transforms** — the model's length scale and
   energy reference: ``LSCALE`` and, when the training run resolved one
   (explicit or via ``ESHIFT_ATOM`` / ``ESHIFT_DBATOM``), ``ESHIFT``, so
   prediction re-applies them automatically and a retrain from the
   potential uses them. The ``ESHIFT_ATOM`` / ``ESHIFT_DBATOM`` toggles,
   the filters and the dataset priorities of the training data are no key
   lines (the :ref:`training record <training_record>` lists them).

DM_REAM potentials always carry explicit ``EMBF0`` and ``RHOFORM`` lines;
the writer spells ``RHOFORM`` in upper case and drops a stray one from a
potential without a DM_REAM block (a notice on standard error, once per
process: :ref:`RHOFORM and EMBF0 without a DM_REAM block <dm_ream_only_keys>`). ``RHOFORM`` has no
default: a DM_REAM training configuration without it stops before any
fitting; the message and the choice of ``RHOFORM`` are described in
:ref:`DM_REAM <dm_ream_rhoform>`.

.. _training_record:

Training record
^^^^^^^^^^^^^^^

The key lines are what is needed to use the model: its settings and its
fit, its ``LSCALE`` and its resolved ``ESHIFT``. The keys a potential does
not use are not key lines: the training datasets, ``DBFILE``; the keys that
only prepared the training data, ``EFILTER``, ``FFILTER``, ``ESHIFT_ATOM``,
``ESHIFT_DBATOM``, ``ZERO_COM_FORCE`` and ``WDBFILE`` (:ref:`load_transforms`);
and these settings of the run:
``VERBOSE``, ``NUMERIC``, ``OUTFILE``, ``AUDIT``, ``AUDIT_SAMPLE`` and
``HEALTH_LOG``, the ``HPOTARGET`` and
``VALIDATION`` of a ``tadah hpo`` run, and ``CHECK``, the setting of
``tadah hpo --check``. For the record, the potential keeps
the values its run was given in its *training record*, a block of comment
lines right above the key lines::

   # --- training record (no key is read from it) ---
   # DBFILE db1.train db2.train
   # WDBFILE 1 0.5
   # VERBOSE 0
   # OUTFILE pot2.tadah
   # Fit balance: LAMBDA 0; ...
   #   ...
   # --- end of training record ---

It has one line for each of these keys the run was given, in the order
``DBFILE``, ``EFILTER``, ``FFILTER``, ``ESHIFT_ATOM``, ``ESHIFT_DBATOM``,
``ZERO_COM_FORCE``, ``WDBFILE``,
``VALIDATION``, ``HPOTARGET``, ``CHECK``, ``VERBOSE``,
``NUMERIC``, ``OUTFILE``, ``AUDIT``, ``AUDIT_SAMPLE``, ``HEALTH_LOG``,
with the values
as the configuration file or the command line wrote them
(``NUMERIC`` does not change them). ``CHECK`` is listed by ``tadah train``
and ``tadah refit``, which do not read it; a ``tadah hpo`` potential lists
none. The records of the native Tadah! potentials of :ref:`mlips` also
list the retired keys that weighted their fit
(:ref:`fit_objective_conversion`). A key nobody set is not
listed: the defaults tadah takes (``VERBOSE 1``, ``OUTFILE pot.tadah``,
``ESHIFT_ATOM false``, ...) are left out, so ``tadah train -c config.train`` records the ``DBFILE`` of
``config.train`` alone, and ``-o pot2.tadah -v 0`` adds ``VERBOSE 0`` and
``OUTFILE pot2.tadah``. The potentials of ``tadah train`` and
``best_pot.tadah`` (and its rank-local copies) end the block with the
:term:`balance table` of their fit (:ref:`fit_balance`), and so always have it. The
other potentials of ``tadah refit`` and ``tadah hpo`` (the ``DUMP`` and
``BDUMP`` archives) have it when the run set one of these keys. ``tadah
refit --edit`` copies the record of its input without its balance table
(:ref:`refit_edit`).

No key is read from the block: ``tadah predict``, ``tadah train -c``,
``tadah hpo -c``, ``pair_style tadah`` and ``tadah refit --edit``, when they
read the potential's keys, skip every line that starts with ``#``. A
potential loads, predicts and retrains as it would without the block, and
the ``VERBOSE`` of its record does not set the log level. A potential
without a record loads in the same way. Two programs
read the block itself: ``tadah refit --edit``, to copy it into the edit
(:ref:`refit_edit`), and ``tadah hpo``, to print the balance table of the
``best_pot.tadah`` it wrote when the run ends. The edit is refused, naming
the file and the lines, when the block is malformed: an opening line
without its end line (since every writer puts the key lines right after
the record, deleting the end line makes the refusal name the opening line
and the potential's first key line), an end line without its opening line,
or a line inside the block that does not start with ``#``.

A potential written by Tadah! 1.5.x or older may have some of these keys
as key lines. An ``M_BLR`` potential may have its ``DBFILE``. A
``best_pot.tadah`` may have its run's ``HPOTARGET`` and ``VERBOSE``, and
its ``VALIDATION`` when the run had a validation file. Every potential of
``tadah train`` or ``tadah hpo`` may have the keys that prepared its
training data: ``ESHIFT_ATOM false``, ``ESHIFT_DBATOM false`` and
``ZERO_COM_FORCE false`` when its configuration set none, and the retired
``WDBFILE_AUTO 0``. A potential of ``tadah train`` may also have the
retired ``UNCERTAINTY``.

Such a potential loads, and no run takes any of these keys from it as a
setting. ``tadah refit --edit`` lists them in the edit's record instead,
except the retired lines, which it keeps as they are
(:ref:`fit_objective_conversion`). ``tadah predict`` ignores the keys that
prepared the training data (:ref:`load_transforms_predict`). A retrain,
``tadah hpo -c`` and ``tadah analysis check_bounds -c`` read no line of the
keys of the record but ``DBFILE`` (:ref:`retrain_from_potential`). No
program that loads a potential reads its *run settings*, ``VALIDATION``,
``HPOTARGET``, ``CHECK``, ``VERBOSE``, ``NUMERIC``, ``OUTFILE``, ``AUDIT``,
``AUDIT_SAMPLE`` and ``HEALTH_LOG``: the parser of every configuration and
potential file does not store a line of them in a file whose key lines have
``WEIGHTS``.

So a potential's
``VERBOSE`` line sets the Tadah log level of no program that loads it
(``tadah predict``, ``pair_style tadah`` in LAMMPS, ``tadah refit --edit``,
a retrain, ``tadah hpo -c``, ``tadah analysis check_bounds -c``), nor the
verbosity of the model built from
the file: that is the program's own, the default ``VERBOSE 1`` or its
``-v`` (a kernel such as ``Kern_RBF`` prints its parameters, ``Kern_RBF |
sigma: 1 | gamma: 1``, when it is built at ``VERBOSE`` 1 or more: for every
model that ``tadah train``, ``tadah predict`` or ``pair_style tadah``
builds; a ``tadah hpo`` run prints it once, for its
``-c`` model, though it builds a model for every evaluation).
``pair_style tadah`` ignores these lines without a message: LAMMPS's screen
and ``log.lammps`` are the input script's output, and none of the lines
changes a force or an energy.

.. _retrain_from_potential:

Retraining from a potential
~~~~~~~~~~~~~~~~~~~~~~~~~~~

A potential's key lines are the model's settings and its fit, not the
training data: no potential that ``tadah train``, ``tadah hpo`` or ``tadah
refit`` writes has a ``DBFILE`` line, whichever the model (its
:ref:`training record <training_record>`, a comment, names the datasets its
coefficients were fitted to, when there were any: a refit has none, and a
potential written by Tadah! 1.5.x or older has no record). A potential is a
training configuration without its
datasets, so it retrains with the ``DBFILE`` lines of the retrain added to a
copy of it::

   cp pot.tadah retrain.train
   echo "DBFILE db.train" >> retrain.train
   tadah train -c retrain.train -o pot2.tadah

``pot2.tadah`` records the retrain: its training record names the
``DBFILE`` of the retrain (``db.train``) and the retrain's other keys, not
the datasets ``pot.tadah`` was trained on. The record of ``pot.tadah``, a
comment in the copy, is neither read nor copied: it describes another fit,
of other coefficients, which ``pot.tadah`` itself keeps.

Without the ``DBFILE`` line ``tadah train`` refuses the configuration
before it reads anything::

   The training configuration has no DBFILE: tadah train fits the datasets
   it lists. A potential has no DBFILE line (no potential tadah writes one;
   its training record, the comment block above its key lines, lists the
   datasets its weights were fitted to in a '# DBFILE' line when there were
   any: a refit has none, and a potential of an earlier version has no
   record): a retrain from one takes a copy of it with a DBFILE line added;
   a potential of tadah refit or refit --edit also pins every weight with
   its FIXINDEX and FIXWEIGHT lines, the seed of a tadah hpo coefficient
   search, and the fit changes the weights when both are deleted from the
   copy (the Training page of the documentation, Retraining from a
   potential).

A potential that ``tadah refit`` or ``tadah refit --edit`` writes, and the
``best_pot.tadah`` of a ``tadah hpo`` run that pins every coefficient, has
``FIXINDEX`` and ``FIXWEIGHT`` lines for every coefficient: the seed of a
``tadah hpo`` coefficient search (:ref:`nfp_search_space_coefficients`).  A fit whose
``FIXINDEX`` pins every coefficient changes none, so both lines are deleted
from the copy for a retrain as well::

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
   its best_pot.tadah: the weights are fitted to the DBFILE data when both
   lines are deleted from the training configuration (the Training page of
   the documentation, Retraining from a potential).

It is a warning, not a refusal: such a fit can be deliberate.

``tadah hpo`` runs without a training ``DBFILE`` only when ``FIXINDEX`` and
``FIXWEIGHT`` pin every coefficient (:ref:`nfp_training_hpo_fit`). Its refusals of a
``-c`` configuration without one end with the same advice where the
coefficients cannot be determined (no ``FIXINDEX``/``FIXWEIGHT``, pins that
do not cover every coefficient, a basis drawn from the training data) and
where ``ATOM`` is missing. With every coefficient pinned,
``GRAD_STRATEGY ANALYTICAL`` is refused
for the missing ``DBFILE`` with the advice to set ``GRAD_STRATEGY HYBRID``
instead: with a ``DBFILE`` line the pins would be refused next (``FIXINDEX
fixes coefficients of the model``).

The fit replaces what the potential holds of its own fit (``WEIGHTS``,
those ``FIXINDEX`` pins excepted, the ``BASIS`` of a non-linear kernel,
``NMEAN``, ``NSTDEV``); none of
it is read when the model is built, so the retrain may also change, for
example, ``SBASIS`` or the descriptor. The other key lines are the model of
the first fit, with its ``LSCALE`` and ``ESHIFT``: the energy reference of
the retrain is the potential's. Its ``ESIGMA``, ``FSIGMA``, ``SSIGMA``,
``ENORM`` and ``ENERGY`` lines are the error scales of the first fit, which
the retrain fits with (:ref:`fit_objective`). A potential that ``tadah
train`` or ``tadah hpo`` of Tadah! 1.0.0 to 1.5.0 wrote has ``EWEIGHT``,
``FWEIGHT`` and ``SWEIGHT`` lines instead (and from 1.3.0 a
``WDBFILE_AUTO`` line; one of ``tadah train`` an ``UNCERTAINTY``
line): a retrain from it is refused, naming each line and its conversion,
until the copy has the converted keys in their place or, for a line the
refusal marks (``UNCERTAINTY``, a quantity the fit leaves out,
``WDBFILE_AUTO 0``, or any line under M_KRR implementation 2), none
(:ref:`fit_objective_conversion` gives the conversions of the keys that
weighted the training data). A potential of an ``OALGO 3`` fit of Tadah!
1.0.0 to 1.5.0 also has a :ref:`SIGMA` line, a deprecated key: the same
refusal names it, and the line is deleted from the copy. A potential of
Tadah! 1.0.0 to 1.5.0 whose only descriptor is ``D2_EAM`` has a
:ref:`WATOM` line of the atomic numbers, which no descriptor reads: the
same refusal names it, and the line is deleted from the copy. The
``WDBFILE`` values of the
conversion of ``WDBFILE_AUTO`` are not read in the copy: they are given as
the retrain's own, below.

The keys that prepared the training data of the first fit (``EFILTER``,
``FFILTER``, ``ESHIFT_ATOM``, ``ESHIFT_DBATOM``, ``ZERO_COM_FORCE``,
``WDBFILE``) are in its training record, not among its key lines, and a
retrain does not take them from the potential, whatever version wrote it: a
retrain works with any number of datasets, and with none of the first fit's
filters or dataset priorities. The
retrain's own are given as in any training run, and are recorded in the new
potential's record. They go on the command line:

.. code-block:: bash

   tadah train -c retrain.train -o pot2.tadah --wdbfile 1 0.5 --efilter -9 -2

A line of a configuration file that includes the potential with
``CONFIG`` gives them too, before or after the ``CONFIG`` line, as in
``outer.train``, which ``tadah train -c outer.train`` reads:

.. code-block:: ini

   CONFIG pot.tadah
   DBFILE db1.train db2.train db3.train
   WDBFILE 1 0.5 1
   EFILTER -9 -2
   OUTFILE pot2.tadah

A key that the potential's own lines set, such as :ref:`BIAS <BIAS>`,
:ref:`MODEL <MODEL>` or :ref:`RCUT2B <RCUT2B>`, is changed by editing its
line in a copy of the potential (``retrain.train`` above), not by a second
line. A line of the including file is a second line of the key: the
included lines are read in place of the ``CONFIG`` line
(:ref:`CONFIG <CONFIG>`). A second line of ``BIAS``, a key of one value, is
refused, and so is a ``MODEL`` line, which gives ``MODEL`` more than three
values.

An ``RCUT2B`` line, above or below the ``CONFIG`` line, adds its values to
the potential's. The key is then read as in a training configuration. Each
descriptor takes one value, so one value more than descriptors is refused,
for a single descriptor and a ``D2_mJoin`` line alike. The refusal names the
lines of the key and ends with the lines of :ref:`TYPE2B <TYPE2B>`. Below,
``cutoff.train`` includes a potential whose ``D2_mJoin`` line joins
``D2_Blip`` and ``D2_BP``. In the refusal, ``<n>`` is the ``RCUT2B`` line of
``pot.tadah`` and ``<t>`` the first of its three ``TYPE2B`` lines:

.. code-block:: ini

   RCUT2B 6.0
   CONFIG pot.tadah
   DBFILE db.train

.. code-block:: text

   TYPE2B names 2 descriptors (D2_Blip, D2_BP) but RCUT2B has 3 values, set
   on line 1 of cutoff.train and on line <n> of pot.tadah (CONFIG on line 2
   of cutoff.train): each descriptor takes one RCUT2B value, in the order of
   the TYPE2B lines (TYPE2B on line <t> of pot.tadah (CONFIG on line 2 of
   cutoff.train), ...)

A line of another key of the descriptors (:ref:`RCTYPE2B <RCTYPE2B>`,
:ref:`CGRID2B <CGRID2B>`, :ref:`RCUTMB <RCUTMB>` and the rest) is read the
same way, and so is every such key of a :ref:`TYPE2B <TYPE2B>` or
:ref:`TYPEMB <TYPEMB>` line to which the including file adds a descriptor.

A potential has one line for each key of its descriptors
(:ref:`RCTYPE2B <RCTYPE2B>`, :ref:`RCUT2B <RCUT2B>`, :ref:`CGRID2B
<CGRID2B>` and the rest, as above). It has its :ref:`TYPE2B <TYPE2B>` and
:ref:`TYPEMB <TYPEMB>` keys on the lines tadah writes: one after another,
with only comments or blank lines between them, and a new line at each
descriptor's name and at each ``DESCRIPTOR`` block's ID (``@ta``). A
``D2_mJoin`` or ``DM_mJoin`` key has a line with the name of the join, then
one line per descriptor it joins.

A second line of such a key in the copy, such as ``RCUT2B 6.0`` appended to
``retrain.train``, is refused, naming both lines. So is a ``TYPE2B`` or
``TYPEMB`` line other than those tadah writes, such as a descriptor's line
appended to a ``D2_mJoin`` potential. Every command that reads the copy
with ``-c``, or a file that includes it with ``CONFIG``, refuses it:
``tadah train``, ``tadah hpo``, ``tadah refit`` (``--retarget``
included) and the analysis commands that take ``-c``. So does
``tadah refit --edit`` of the copy. In the refusal, ``<a>`` and ``<b>``
are the two ``RCUT2B`` lines:

.. code-block:: text

   Key RCUT2B is set on line <a> of retrain.train and on line <b> of
   retrain.train, lines of one potential (a file with WEIGHTS): tadah train
   reads a key of a potential's descriptors from the line the potential
   has for it, and a potential's value is changed by editing its line in a
   copy of the potential (the Training page of the documentation,
   Retraining from a potential)

The ``WARNING`` for values a potential's descriptors do not take
(:ref:`composite_descriptors`) is given for values on one line of the
potential file. ``tadah predict`` and ``pair_style tadah`` read a second
line of such a key as if its values were on that one line: a single
descriptor then takes the largest ``RCUT2B`` value, with the ``WARNING``.

A file that has ``WEIGHTS`` (a potential or a copy of one) is a potential,
wherever the run reads it: given with ``-c``, or included with ``CONFIG`` by
a configuration file (a ``tadah hpo`` seed too). Its lines of the keys of the training record but
``DBFILE`` are not read: those of the six, with or without an ``ESHIFT``
line, and its run settings, ``VALIDATION``,
``HPOTARGET``, ``CHECK``, ``VERBOSE``, ``NUMERIC``, ``OUTFILE``, ``AUDIT``,
``AUDIT_SAMPLE`` and ``HEALTH_LOG`` (a ``best_pot.tadah`` of tadah 1.5.x has
``HPOTARGET``, ``VERBOSE`` and ``NUMERIC``, and ``VALIDATION`` when its run
had a validation file); every other line
of the run, its ``DBFILE`` included, and the command line, applies as it
would without them. A ``WARNING`` names the potential's lines that are not
at their default (the command's default of the key when it has one, such as
tadah train's ``OUTFILE pot.tadah``, else the key's: ``VERBOSE 1``,
``NUMERIC 18``), that the run does not give
itself and that its command reads (``tadah hpo`` reads no ``OUTFILE``,
``tadah train`` no ``VALIDATION``, ``HPOTARGET``, ``CHECK`` or
``HEALTH_LOG``): a potential written by Tadah! 1.5.x or older has them as key lines,
and so has a copy with one added::

   WARNING: tadah train: the configuration file retrain.train is a potential
   (it has WEIGHTS): its EFILTER 5 6 and WDBFILE 1 1 prepared the training
   data of the fit that wrote it, not of this run, and are ignored (tadah
   keeps such keys in a potential's training record, not as key lines). This
   run takes its own EFILTER or WDBFILE from --efilter or --wdbfile on the
   command line, or from an EFILTER or WDBFILE line in a configuration file
   that includes the potential with CONFIG (the Training page of the
   documentation, Retraining from a potential).

The run settings get a sentence of their own. When the command has an
option for only some of the keys, the last sentence names the line for every
key first. The keys that have an option follow, with their options
(``tadah train`` has no ``--numeric``)::

   WARNING: tadah train: the configuration file retrain.train is a potential
   (it has WEIGHTS): its VERBOSE 2 and NUMERIC 12 were settings of the run
   that wrote it, not of this run, and are ignored (tadah keeps such keys in
   a potential's training record, not as key lines). This run takes its own
   VERBOSE or NUMERIC from a VERBOSE or NUMERIC line in a configuration file
   that includes the potential with CONFIG, or VERBOSE from --verbose on the
   command line (the Training page of the documentation, Retraining from a
   potential).

For a potential that a file includes, the warning names both (``old.tadah,
which the configuration file outer.train includes with CONFIG, is a
potential ...``) and says where the run's own go: an option of the command
line, or a line of the including configuration file. A run that needs a
setting the potential's line gave, and has none of its own, is refused
saying where it goes and naming the line that was not read: ``tadah
analysis check_bounds -c`` of a seed potential without ``--hpotarget``
(``tadah analysis check_bounds: no HPOTARGET: the target file is given with
--hpotarget, or with an HPOTARGET line in a configuration file without
WEIGHTS. old_best.tadah is a potential (it has WEIGHTS): its HPOTARGET
config.hpo line was a setting of the run that wrote it and is not
read.``).

A retrain derives ``ESHIFT`` from its own data, instead of using the
potential's, when the ``ESHIFT`` line is deleted from the copy and the run
is given ``ESHIFT_ATOM`` or ``ESHIFT_DBATOM``: on the command line, or on a
line of a configuration file that includes the copy with ``CONFIG``::

   grep -v '^ESHIFT ' pot.tadah > retrain.train
   echo "DBFILE db.train" >> retrain.train
   tadah train -c retrain.train --eshift-dbatom -o pot2.tadah

A toggle line of the copy itself is not read, with or without its
``ESHIFT`` line. The warning names it when it is ``true``, among the keys
the run can give, unless the run gives that toggle itself or sets the other
one ``true``: the two choose one thing, the source of the shift, and exclude
each other, so the run's own toggle is its choice (``--eshift-dbatom``
beside a copy's ``ESHIFT_ATOM true`` derives the shift by the least-squares
fit, and no warning names the copy's line).

An ``ESHIFT`` of the run overrules a toggle the run gives, as in any
configuration (``Multiple ESHIFT sources set; precedence: explicit > ATOM >
DBATOM.``). A warning that names a toggle then ends by saying when the run
derives the shift. Beside an ``ESHIFT`` line it ends ``The ESHIFT of the
configuration, the energy reference of the model, overrules ESHIFT_ATOM and
ESHIFT_DBATOM: ESHIFT is derived from the data of this run when the ESHIFT
line is deleted and the run is given one of them``. With ``--eshift`` it
ends ``The ESHIFT given with --eshift, ...: ESHIFT is derived from the data
of this run when --eshift is left out and the run is given one of them``.
With ``--eshift`` over an ``ESHIFT`` line of the configuration it ends
``... when --eshift is left out, the ESHIFT line is deleted and the run is
given one of them``.

A retrain reproduces the first fit when it is given the keys its record
lists, on the command line or in the file that includes the copy.

With ``LAMBDA -1`` the potential records the ``ALPHA`` and ``BETA``
the evidence approximation found (the fit's :math:`\lambda = \alpha/\beta`;
they are the approximation's starting guesses in a configuration), so a
retrain starts the approximation from them (on the same datasets it stops
there, to its convergence tolerance).

A ``DBFILE`` that lists one dataset file twice (the same path, two spellings
of it, or a link to it) is refused, naming both entries, by ``tadah train``
and, at startup, by ``tadah hpo``: each entry is a dataset, so each of its
structures would be fitted twice. ``WDBFILE`` gives a dataset a higher
priority.

A potential that holds the ``DBFILE`` line it was trained on is refused in
the same way when a retrain from its copy adds that ``DBFILE`` again. The
line is deleted from the copy before the ``DBFILE`` of the retrain is added
(``tadah predict`` and ``pair_style tadah`` do not read it).

.. _pot_number_format:

Number format
~~~~~~~~~~~~~

Every non-integer number of a potential's key/value lines is written
with ``NUMERIC`` **significant** digits (``printf`` ``%.{N}g``; those of
the cutoffs and grids exactly, as described below). The default, ``18``, is exact:
17 significant digits identify every double, so a potential reloaded from
``pot.tadah`` is bit-identical to the one in memory. That is why a value typed as ``5.3`` comes back as
``5.29999999999999982`` — the exact double nearest to 5.3 — and why
``1.0`` is written as ``1``. Integers, names, booleans, index lists and
grid generators (``LIN`` / ``GEOM``) are written as they are stored, so
integers are always exact. ``#`` comment lines (such as the header
``tadah hpo`` writes) are annotations that ``NUMERIC`` does not govern.

* ``NUMERIC`` is never a key line of a potential; its training record (a
  comment block that is never read as keys) lists the ``NUMERIC`` its run
  was given.
* An explicit ``NUMERIC`` below 17 is honoured (the numbers are rounded to
  that many significant digits) but ``tadah`` prints a WARNING: the
  written potential then differs slightly from the fitted one.
* The cutoffs and the radial grids, ``RCUT2B``, ``RCUTMB``, ``CGRID2B``,
  ``CGRIDMB``, ``SGRID2B`` and ``SGRIDMB``, are written exactly whatever
  ``NUMERIC`` is (below 17 as the shortest text that reads back as the
  same value: ``RCUT2B 5.58190742024`` at ``NUMERIC 6``). A load checks
  them exactly: ``D2_EAM`` and ``DM_EAM`` refuse a cutoff other than that
  of their setfl file, a knot channel alone on its line has its largest
  knot as its cutoff (a ``D2_mJoin`` or ``DM_mJoin`` line keeps the stated
  cutoffs), and the knot and Blip descriptors warn of a knot or a bump
  past the cutoff.
  So a potential written with a low ``NUMERIC`` loads with the cutoffs and
  grids it was fitted with; only the fitted numbers (``WEIGHTS``,
  ``NMEAN``, ``NSTDEV``, ...) and the other settings are rounded. The
  configuration-keys page (:ref:`ConfigSection`) marks these keys (*In a
  potential file*).
* ``NUMERIC`` never limits the values held in memory: calculations
  always run at full double precision, whatever it is set to. Every
  ``tadah`` reader of a file ``tadah`` writes (potentials, configurations,
  datasets, POSCAR exports) accepts what the writers produce, including
  exponent forms and subnormal values such as ``1e-310``.
* Dataset files written by ``tadah data convert``, ``merge``, ``split``,
  ``sample``, ``dedup`` and ``balance`` follow the same rule: every
  number with ``NUMERIC`` significant digits (default ``18``, exact — a
  rewritten dataset trains on exactly the values the command computed,
  e.g. the energies ``balance`` shifted), and a WARNING below 17.
  So do the LAMMPS data and VASP POSCAR exports of ``tadah data write``
  (both formats read E-notation; ``tadah`` reads its POSCAR export back
  exactly); the CASTEP cell export keeps ``NUMERIC`` decimal places, with
  no warning.
* In predictions, in the tables of ``tadah analysis descriptor``,
  ``bfunc`` and ``cutoff``, and in those of ``tadah properties
  pairwise``, ``NUMERIC`` sets the digits after the decimal point, with
  the default ``18``; its entry in :ref:`ConfigSection` names the notation
  of each output.

The same rules apply to ``tadah refit`` (a ``NUMERIC`` in the refit
configuration sets the precision of ``pot.tadah``), to ``tadah refit
--edit`` (a ``NUMERIC`` set for the run is honoured — the edit is then
exact only up to that rounding, and a WARNING says so; the edited
potential's own ``NUMERIC`` line is not read) and to the potentials ``tadah
hpo`` writes (:ref:`nfp_output_archives`).

.. note::

   Potentials written by ``tadah hpo`` 1.5.0 carry an automatically
   inserted ``NUMERIC 12`` line. No run reads it: a retrain, ``tadah hpo
   -c`` and ``tadah refit --edit`` of such a file write exact potentials
   (the first two name the line in a WARNING, the edit moves it into its
   training record).

Diagnostic output (verbose 2)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

With ``--verbose 2`` (INFO level) the engine logs progress to stdout:
load-time transform summaries, neighbour-finding wall-time, the
pre-flight audit report, and the regression solver status. None of
these lines are written to files; shell redirection captures them if
needed (``tadah train -c config.train -v 2 2>&1 | tee train.log``).

.. _training_hpo:

Training inside tadah hpo
-------------------------

A ``tadah hpo`` run (:ref:`nested_fitting`) trains one potential in each
:term:`evaluation`, unless a check rejects its candidate before training. It
builds each one from its :term:`training configuration`, the ``--config``
file, which has the format and the keys of a ``tadah train`` configuration
(:ref:`config_file`, :ref:`ConfigSection`). This section describes what the
run takes from that file, where the search starts, what a search parameter
changes, the fit of each evaluation, the lines the run refuses, a potential
as the training configuration and the validation data. The
:term:`target file` has a page of its own (:ref:`nfp_target_file`).

.. _nfp_training_hpo_reads:

What the run reads
~~~~~~~~~~~~~~~~~~

The training configuration sets the model, its descriptors, the settings
of the fit and the training datasets (:ref:`DBFILE <dbfile>`), as it does
for ``tadah train``. An option of ``tadah hpo`` that sets a training key,
such as ``--force``, replaces the line of the file (:ref:`config_file`;
options in :ref:`cli_hpo`).

The run reads the training and validation datasets once, when it starts.
The load-time transforms (:ref:`load_transforms`) prepare the training data
once, and every evaluation fits the data so prepared. A key that prepares
the data therefore keeps its value from the training configuration in every
evaluation, and :hpo:key:`OPTIM` refuses it as a search parameter.

.. _nfp_training_hpo_start:

The start of the search
~~~~~~~~~~~~~~~~~~~~~~~

The values that the training configuration gives the search parameters are
the starting point of the search. An :hpo:block:`INIT` block with
:hpo:key:`METHOD <INIT.METHOD>` lines replaces that point with the points
of its lines, and keeps it only with a :hpo:key:`METHOD CONFIG <METHOD.CONFIG>`
line (:ref:`nfp_optimizer_block_initialisation`). An optimiser that reads
no starting point does not start from it, but the run refuses at startup a
starting point outside its box. An :hpo:block:`!INIT` block without a
:hpo:key:`!METHOD CONFIG <METHOD.CONFIG>` line leaves out the values of the
training configuration, which are then not checked against the boxes. The
boxes that :hpo:key:`AUTO <OPTIM.AUTO>` derives come from the same values
(:ref:`nfp_search_space_bounds`).

.. _nfp_training_hpo_candidate:

What a search parameter changes
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Each evaluation starts from the training configuration and replaces the
values that the :hpo:key:`OPTIM` lines select with those of its candidate.
Every other value keeps its value from the file, including the values of
a searched key at indices that no :hpo:key:`!OPTIM` line selects. The
cutoffs of a knot descriptor are the exception: they follow its largest
knot in every evaluation (:ref:`refit_rcut_contract`).

The potential of the evaluation, with its fitted coefficients, is the one
every loss term measures: the validation metrics, the
:hpo:key:`PC_LAMMPS` scripts and the :hpo:key:`PC_OBSERVABLE` terms
(:ref:`nfp_targets`). ``best_pot.tadah`` holds the values of the best
candidate in its key lines, a searched :ref:`WDBFILE <wdbfile>` in its
:ref:`training record <training_record>` (:ref:`nfp_output_best`).

.. _nfp_training_hpo_fit:

The fit inside each evaluation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Each evaluation builds the model from its candidate configuration and fits
its coefficients to the training data as ``tadah train`` does
(:ref:`fit_objective`). The fit uses the error scales, priorities,
:ref:`LAMBDA` and :ref:`OALGO` of that configuration. No fit takes
anything from an earlier evaluation. A candidate that :hpo:key:`BUMPGATE`,
:hpo:key:`RHOGATE` or :hpo:key:`RHOBARGATE` rejects is not fitted
(:ref:`nfp_search_space_feasibility`). A ``LAMBDA -1`` fit starts its evidence
approximation from the :ref:`ALPHA` and :ref:`BETA` of the training
configuration every time.

:ref:`FORCE <force>` and :ref:`STRESS <stress>` (or ``-F`` and ``-S``)
decide whether the fit includes forces and stresses. The validation metrics
decide what the validation data are predicted for: forces whenever the
target file has :hpo:key:`PC_FRMSE`, stresses whenever it has
:hpo:key:`PC_SRMSE_GPA`, whatever ``FORCE`` and ``STRESS`` say.

The ``POTENTIAL SETUP`` section of the journal, ``hpo_run.log``
(:ref:`nfp_output_histories`), states both switches, where each value comes
from, and what the validation data are predicted for. In the search of
:ref:`nfp_unit_2` it reads:

.. output: unit2 hpo_run.log

.. code-block:: text

   -- POTENTIAL SETUP -----------------------------------------------------
   ...
   Fit forces (FORCE)    : yes  (configuration file config.train)
   Fit stresses (STRESS) : yes  (configuration file config.train)
   Validation predicts   : energies, forces (PC_FRMSE), stresses (PC_SRMSE_GPA)

The coefficients are fitted, not searched: :hpo:key:`OPTIM` refuses
:ref:`WEIGHTS`. :ref:`FIXINDEX` and :ref:`FIXWEIGHT` pin chosen
coefficients at given values, and the fit determines the others;
:hpo:key:`!OPTIM` ``FIXWEIGHT`` searches the pinned values
(:ref:`nfp_search_space_coefficients`). When ``FIXINDEX`` pins every
coefficient, no fit runs, the coefficients are the ``FIXWEIGHT`` values,
and the training configuration needs no ``DBFILE``: :ref:`ATOM` then names
the elements. An ``M_KRR`` model is the exception. With a kernel other than
``Kern_Linear`` it draws its basis vectors from the training data, so it
needs a ``DBFILE``, and every evaluation fits; implementation 2
(``MODEL M_KRR <kernel> 2``) refuses ``FIXINDEX`` and ``FIXWEIGHT``.

A fit that fails for a candidate, such as an evidence approximation that the
fit refuses, makes the evaluation a failed one
(:ref:`nfp_targets_failures`).

.. _nfp_training_hpo_refused:

Lines refused when the run starts
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Before the first evaluation the run makes the checks that every fit of the
run would make, and refuses a training configuration that would fail them,
as ``tadah train`` refuses it. A line of the first three kinds below is
refused with its key, its line and its file; a value that every fit refuses
is refused with the key and its values, without a line, or, for a model
that cannot be built, with the reason, which can name lines and the file.
The run refuses:

- a retired key, with its conversion or the keys that take its place
  (:ref:`fit_objective_conversion`);
- a line that has no effect on the fit: a deprecated key such as
  :ref:`MBLOCK`, :ref:`NBLOCK`, :ref:`MPIWPCKG` or :ref:`SIGMA`;
  :ref:`EMBF0` and :ref:`RHOFORM` without a DM_REAM block
  (:ref:`DM_REAM <dm_ream_only_keys>`); a :ref:`WATOM` other than 1 when no
  descriptor reads it (:ref:`ConfigSection`);
- a command key of another command, such as ``ANALYTICS`` (``-A`` of
  ``tadah predict``), named with the commands that read it
  (:ref:`config_file`);
- a value that every fit refuses, such as a model that cannot be built
  and, when the evaluations fit the model (with training data, unless
  ``FIXINDEX`` pins every coefficient), a negative ``LAMBDA`` other than
  -1, an ``OALGO`` other than 1 to 4 or a second ``OALGO`` value that its
  solve does not take.

The checks that need the data, such as a priority below 0, follow once the
data are read, still before the first evaluation. ``tadah hpo --check``
makes all of these checks and runs no evaluation (:ref:`cli_hpo`).

.. _nfp_training_hpo_potential:

A potential as the training configuration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

A potential, such as the ``best_pot.tadah`` of an earlier run, can be the
training configuration: its key lines give the model and the start of the
search. No potential that ``tadah`` writes has a :ref:`DBFILE <dbfile>`
line (the run reads and adds that of a potential of Tadah! 1.5.x,
:ref:`retrain_from_potential`), and the run reads its :ref:`LSCALE <lscale>`
and :ref:`ESHIFT <eshift>`, the model's, but no line of the other keys that
prepared its training data, :ref:`WDBFILE <wdbfile>` included. A
configuration that includes the potential with :ref:`CONFIG <CONFIG>` gives
those. In the directory of :ref:`nfp_unit_2`, after its search,
``start.train`` below is the ``--config`` file of a search from its best
potential:

.. code-block:: ini

   CONFIG best_pot.tadah
   DBFILE ../../data/ta_md_train.tadahdb

The :ref:`WEIGHTS` of the potential are never read: every evaluation fits
its coefficients again, unless ``FIXINDEX`` pins every coefficient, as in a
potential of ``tadah refit`` (:ref:`nfp_training_hpo_fit`). The
:hpo:key:`WARM <METHOD.WARM>` method of an :hpo:block:`INIT` block takes
the values of the search parameters from a potential, not its model
(:ref:`nfp_optimizer_block_initialisation`).

.. _nfp_training_hpo_validation:

The validation data
~~~~~~~~~~~~~~~~~~~

The :term:`validation configuration` (``--validation``) lists the validation
datasets and their filters (:ref:`validation`); :ref:`nfp_targets_validation`
describes how to choose them. The run predicts these structures and never fits
them.

The run gives the validation data the length scale and the energy reference
of the training data. These are the :ref:`LSCALE <lscale>` of the training
configuration and the :ref:`ESHIFT <eshift>` resolved from the training
data; without training data, the ``ESHIFT`` of the configuration. The
training filters and :ref:`ZERO_COM_FORCE <zero_com_force>` do not apply to
the validation data; the ``EFILTER`` and ``FFILTER`` of the validation
configuration do. With training data, every element of the validation data
must occur in them.

The validation configuration can be left out when :hpo:key:`PC_LAMMPS` or
:hpo:key:`PC_OBSERVABLE` lines give the loss. A target file with a
validation metric and no validation configuration is refused.

.. _see_also:

Related pages
-------------

* :ref:`training` — quickstart introduction.
* :ref:`ConfigSection` — every configuration key, with defaults.
* :ref:`fit_objective` — the sum a fit minimises, its error scales and
  priorities.
* :ref:`prediction` — using a ``pot.tadah`` with ``tadah predict``.
* :ref:`nested_fitting` — driving training from an outer
  hyperparameter optimiser.
