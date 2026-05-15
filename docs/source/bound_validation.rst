.. _bound_validation:

HPO bound validation
====================

.. note::

   This page documents the HPO bound-validation pipeline introduced in
   the *merry-peacock* line of work. It catches the BASINS class of
   silent training failures (e.g. ``CGRIDMB`` fixed at zero forcing the
   many-body block to collapse to identically zero weights) at every
   entry point: ``tadah analysis``, ``tadah train``, and ``tadah hpo``.

Motivation
----------

A typical silent failure: an HPO over a ``DM_mBlipEnv`` model converges
to a finite loss; energies and forces look reasonable; closer
inspection of the trained weights shows the entire many-body block is
either exactly zero or two large numbers cancelling. The root cause is
almost always a parameter the user fixed in ``train.tadah`` that puts
the descriptor outside the data's support — a CGRIDMB centred at 0.0
when the dataset's nearest-neighbour distance is 2.86 Å, for instance.
The HPO then optimises over a parameter sub-space where the descriptor
is dead, and the regressor obligingly returns zero weights for that
block.

The bound-validation pipeline turns these silent failures into loud
diagnostics with data-driven recommendations. Three new analysis
subcommands and one runtime audit hook cover the lifecycle:

==================================== ============================================
``tadah analysis dataset_stats``     Produce the canonical dataset statistics blob
``tadah analysis suggest_bounds``    Propose data-driven OPTIM bounds per descriptor
``tadah analysis check_bounds``      Corner-sample an OPTIM box; CI-friendly dry-run
``AUDIT`` context key                Per-train and per-HPO pre-flight audit
==================================== ============================================

The pipeline is **data-anchored**: every numerical threshold is either
a dataset percentile or a per-descriptor metadata declaration. Nothing
in framework code compares against a literal threshold.

``tadah analysis dataset_stats``
--------------------------------

Walks every configured ``DBFILE``, calls ``NNFinder``, and produces a
versioned ``DataStats`` blob containing:

- per-element neighbour-count percentiles
- per-pair (i, j) ``r_ij`` percentiles + KDE peak finder
- per-atom energy and force-norm percentiles
- a heuristic ``class_label`` (one of ``metal_like``, ``covalent_or_ionic``,
  ``molecular_crystal``, ``amorphous_or_liquid``, ``cluster_or_dimer``,
  ``unclassified``)

::

   tadah analysis dataset_stats -c train.tadah --outfile stats.tadah

The output file is the canonical input for the suggester (Phase 4) and
the audit (Phase 5). It is line-based plaintext (``TADAH_DATASTATS_V1``
header) and round-trippable via ``DataStats::save`` /
``DataStats::load`` in C++.

``tadah analysis suggest_bounds``
---------------------------------

For every configured descriptor whose ``meta()`` is implemented, this
subcommand walks the parameter schema and emits one ``OPTIM`` bound per
``ParamMeta``. Output goes to stdout as a human-readable table and (when
``--outfile`` is given) to disk as a directly-usable OPTIM block::

   tadah analysis suggest_bounds -c train.tadah --outfile suggested.hpo

Sample output (Ta + ``DM_mBlipEnv``)::

   D2_Blip.RCUT2B               3.4780e+00 .. 5.5384e+00   Å       CRITICAL
   D2_Blip.CGRID2B[0..3]        0.0000e+00 .. 5.5384e+00   Å       recommended
   DM_mBlipEnv.CGRIDMB          2.1201e+00 .. 3.6448e+00   Å       CRITICAL
   DM_mBlipEnv.SGRIDMB          4.3016e-01 .. 1.5486e+01   Å^-2    CRITICAL

Every line carries a ``rationale`` comment in the OPTIM block so the
user can review and override before running HPO.

``tadah analysis check_bounds``
-------------------------------

CI-friendly dry-run. Reads an HPOTARGET file's ``OPTIM <KEY> (range) lo hi``
rows, samples corners (full ``2^k`` for ``k <= 12``, otherwise
``min(2^k, ceil(k log2 k))`` Latin-hypercube samples), and runs the
audit at each corner::

   tadah analysis check_bounds -c train.tadah --hpotarget config.hpo

Returns exit code ``0`` if every corner passes, ``1`` if any corner
trips a ``FAIL`` finding. Pair this with a CI gate to catch bound-set
regressions before they reach an HPO run.

The ``AUDIT`` context key
-------------------------

The pre-flight audit fires inside ``tadah train`` and ``tadah hpo``
once the dataset stats are available. It walks every configured
descriptor's ``meta()``, runs the per-parameter and cross-parameter
``HealthCheck``\ s, and emits findings at three levels — ``OK``, ``WARN``,
``FAIL``. Behaviour is driven by the ``AUDIT`` key in the seed config:

==========  ========================================================================
``off``     (default) skip the audit entirely
``warn``    emit findings via the Logger; never gate execution
``error``   emit findings; if any ``FAIL`` is present, exit non-zero before training
==========  ========================================================================

The audit defaults to ``off`` because the data-stats scan that backs it
runs ``NNFinder`` over the training DB and dominates startup cost on
large training sets. Opt in explicitly: add ``AUDIT warn`` to
``train.tadah`` (or your HPO seed config) for diagnostic findings, or
``AUDIT error`` for a hard refusal-to-launch on degenerate parameter
combinations — the latter is the typical CI setting.

The ``AUDIT_SAMPLE`` context key
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

When the audit is active, the dataset scan is sampled by default so the
audit cost stays bounded on large training sets (a 6000-structure
training DB no longer pays a 50+ second NN sweep). ``AUDIT_SAMPLE`` (an
integer, default ``256``) controls how many structures contribute:

==========  ========================================================================
``256``     (default) deterministic random sample of 256 structures
``N``       any positive ``N`` samples ``min(N, stdb.size())`` structures
``0``       use the entire ``StructureDB`` (the previous behaviour)
==========  ========================================================================

The seed for the random sample is ``stdb.size()``, so repeated audits
on the same DB hit the same subset and findings are reproducible. The
sample is large enough to drive percentile-based health checks; bump it
(or set ``AUDIT_SAMPLE 0``) if you want every structure to contribute,
e.g. when a dataset has rare outliers you need the audit to see.

The ``HEALTH_LOG`` context key
------------------------------

During HPO, an ``IterHealthMonitor`` records cheap per-block weight
statistics (``max_abs``, ``median_abs``, non-zero fraction, NaN/Inf
flag) on every evaluation. ``HEALTH_LOG`` controls the cadence of
warnings:

.. list-table::
   :widths: 12 80

   * - ``summary``
     - (default) at most one warning per (block, kind) per ~100 evals
   * - ``full``
     - every offending iter produces a warning
   * - ``off``
     - monitor inactive

The cost per iter is ``O(P + K)`` (one linear scan over the weights,
partitioned by the design matrix's column layout) — under 0.5% of a
typical evaluation. The monitor catches scenarios where an HPO move
silently kills a descriptor block partway through optimisation.

How the audit decides what is "wrong"
-------------------------------------

Each descriptor's ``D_Base::meta()`` returns a ``DescriptorMeta``
listing every parameter under HPO control. Each ``ParamMeta`` declares:

- ``key`` and ``arg_pos`` (scalar vs vector entry)
- ``Scale`` (``Distance``, ``InverseDistanceSq``, ``Density``,
  ``InverseDensity``, ``Energy``, etc.) — drives suggester anchoring
- ``Criticality`` (``Critical``, ``Recommended``, ``Optional``)
- ``HardLimit`` — physically impossible values only (e.g. ``SGRIDMB > 0``)
- ``BoundSuggester`` closure — reads ``DataStats`` to propose ``(lo, hi)``
- ``HealthCheck`` closure — validates the seed value against ``DataStats``

Whole-descriptor ``cross_param_checks`` capture combinations: the
ρ-collapse test on ``DM_mBlipEnv`` for instance evaluates whether the
chosen ``CGRIDMB`` and ``SGRIDMB`` together leave any descriptor
amplitude on the training pool.

**Coverage**: every registered descriptor with continuous parameters
ships ``meta()``. The 22 descriptors covered are:

- 2-body grid: ``D2_Blip``, ``D2_BP`` — full schema
  (``RCUT2B`` + ``CGRID2B`` / ``SGRID2B``).
- 2-body closed-form: ``D2_LJ``, ``D2_MIE``, ``D2_ZBL``, ``D2_EAM``
  — minimal schema (``RCUT2B`` only; learnable ε / σ / etc. are linear
  weights that the audit doesn't gate).
- Many-body grid: ``DM_Blip``, ``DM_EAD``, ``DM_mBlipEnv``,
  ``DM_mRLR``, ``DM_mSQRT``, ``DM_mBlip`` — full schema
  (``RCUTMB`` + ``CGRIDMB`` / ``SGRIDMB``).
- Many-body closed-form: ``DM_EAM`` — ``RCUTMB`` only.
- ``DM_Poly`` family (12 instantiations): ``DM_PS_*``, ``DM_GPS_*``,
  ``DM_OW_*``, ``DM_GOW_*`` — full schema.

The 4 placeholder / meta-descriptors that intentionally return
``nullopt`` (no parameters of their own) are
``D2_Dummy``, ``D2_mJoin``, ``DM_Dummy``, ``DM_mJoin``. They delegate
to nested children, which are audited separately when configured.

A regression test (``descriptor coverage: every registered descriptor
has meta()``) ensures any future descriptor lands with a populated
schema or is added to the explicit exempt list.

End-to-end demo
---------------

The repository ships four end-to-end demos under
``hpo_audit/demos/``:

==============================  ===================================================
``01_ta_basins/run_demo.sh``    Ta with ``D2_Blip + DM_mBlipEnv`` (the BASINS config)
``02_ch4_blip/run_demo.sh``     CH₄ with ``D2_BP + DM_Blip`` (binary, multi-pair)
``03_ta_d2bp/run_demo.sh``      Ta with ``D2_BP`` only — closed-form BP demo
``04_ch4_dmead/run_demo.sh``    CH₄ with ``DM_EAD`` (vectorial GTOs, Lmax > 0)
==============================  ===================================================

Each script is self-contained: copy a small example dataset into a
working directory, run all five subcommands, and tail the audit log.
Use ``RUN_TRAIN=1 bash run_demo.sh`` to exercise the full
train + audit-error path on a small dataset.
