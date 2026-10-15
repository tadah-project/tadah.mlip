.. _pc_observable:

Typed observable targets
========================

``PC_OBSERVABLE`` evaluates a built-in physical property in the Tadah
process. It belongs at the root of the
:doc:`target file <nfp_configuration>`, and each line adds one loss
term. The only implemented kind is ``cohesive_energy``; its crystal is
bcc. A file containing these targets cannot also contain
``PC_LAMMPS`` or ``PC_FAILPROBE``.

.. code-block:: none

   PC_OBSERVABLE cohesive_energy --species Ta --target 8.10 --weight 100 \
                 --hotvar a0 2.8 3.8 3.30

The value is :math:`E_\mathrm{isolated}-E_\mathrm{bulk}/N` in eV/atom,
positive for a bound crystal. The bulk cell undergoes isotropic
relaxation at zero pressure. ``--validation`` is optional unless
:doc:`validation metrics <nfp_validation_targets>` are also requested.

Property and loss options
-------------------------

.. list-table:: Required and optional property settings
   :header-rows: 1
   :widths: 33 19 48

   * - Option
     - Default
     - Meaning
   * - ``--species <element>``
     - Required
     - Element used to construct the crystal and isolated atom.
   * - ``--target <energy>``
     - Required
     - Finite target cohesive energy in eV/atom.
   * - ``--weight <weight>``
     - Required
     - Finite, nonnegative loss weight.
   * - ``--hotvar a0 <lo> <hi> [<initial>]``
     - Required
     - Lattice-constant window in Å, with finite bounds and
       ``hi > lo``. Only ``a0`` is supported. The initial value defaults
       to the midpoint and is clamped to this window.
   * - ``--lattice bcc``
     - ``bcc``
     - Only ``bcc`` is implemented.
   * - ``--supercell <nx> <ny> <nz>``
     - ``4 4 4``
     - Positive integer repeats along each axis, up to 2147483647.
   * - ``--loss <name>``
     - ``L2``
     - One :doc:`loss name <nfp_losses>`, independent of root ``LOSS``.
       ``HUBER`` and ``TUKEY`` use their default parameters; this option
       takes no parameter value.

Relaxation options
------------------

.. list-table:: Relaxation controls
   :header-rows: 1
   :widths: 34 18 48

   * - Option
     - Default
     - Meaning
   * - ``--maxiter <n>``
     - ``5000``
     - Relaxation iteration cap; integer from 0 to 2147483647.
   * - ``--maxeval <n>``
     - ``5000``
     - Energy/force evaluation cap; integer from 0 to 2147483647.
   * - ``--etol <value>``
     - ``0``
     - Relative energy-change convergence tolerance; a positive value
       enables it. Zero leaves force-based convergence.
   * - ``--max-wall-time <seconds>``
     - ``0``
     - Positive values impose a per-relaxation wall-clock cap.
       Zero disables it.
   * - ``--fmax-converged <force>``
     - ``0.1``
     - Maximum accepted atomic force in eV/Å after relaxation.
   * - ``--unconverged const <value>``
     - ``const 100``
     - Report this finite value when relaxation does not converge.
   * - ``--unconverged force-scaled <slope>``
     - Unset
     - Instead report ``target + slope * fmax`` for an unconverged
       relaxation. The slope must be finite.

The two ``--unconverged`` forms select alternative behaviours.
An unconverged relaxation returning a finite substitute still supplies
that value to its configured loss. A relaxed lattice constant outside
the allowed window, or a non-finite result or loss, fails the term and
therefore the evaluation.

Gradient compatibility
----------------------

For a phase that uses gradients, an omitted ``GRAD_STRATEGY`` resolves
to ``ANALYTICAL`` when typed observables are present. Explicit
``ANALYTICAL`` and ``HYBRID`` are accepted; ``NUMERICAL`` is refused.
Derivative-free phases do not need a gradient setting.

``ANALYTICAL`` requires a supported linear fit with training data,
``OALGO 4``, fixed ``LAMBDA >= 0``, ``NORM false``, ``STRESS false``
and no ``FIXINDEX`` pins or ``PC_SRMSE`` target. Startup also checks
the selected descriptor parameters. For typed observables, ``MAPE``,
``RMSLE`` and ``--unconverged force-scaled`` need ``HYBRID`` because
the observable-gradient path does not differentiate those cases.

Set ``GRAD_STRATEGY`` on the phase owning the objective, rather than
inside an NLopt ``INNER`` block.
