.. _pc_observable:

Typed observable targets
========================

``PC_OBSERVABLE`` evaluates a built-in physical property in the Tadah
process. It belongs at the root of the
:doc:`target file <nfp_configuration>`, and each line adds one loss
term. The only implemented kind is ``cohesive_energy``; its crystal is
bcc. The parser rejects files that mix these targets with ``PC_LAMMPS``
or ``PC_FAILPROBE``.

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
     - Element used to construct the crystal and isolated atom: an
       element symbol, and an element of the potential.
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
       to the midpoint; one outside the window is refused.
   * - ``--lattice bcc``
     - ``bcc``
     - Only ``bcc`` is implemented.
   * - ``--supercell <nx> <ny> <nz>``
     - ``4 4 4``
     - Positive integer repeats along each axis, up to 2147483647.
   * - ``--loss <name> [<parameter>]``
     - Root ``LOSS``
     - One :doc:`loss name <nfp_losses>`, with the parameter of
       ``HUBER`` and ``TUKEY``. Without it the term takes the root
       ``LOSS`` read above the line (``L2`` without one).

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
     - Force tolerance of the relaxation, above 0: the relaxation
       stops when the largest atomic force, in eV/Å, and the strain
       force of the cell both fall below it. The strain force is
       :math:`|\partial E/\partial\varepsilon|/(3V)` for the
       isotropic strain :math:`\varepsilon` and the cell volume
       :math:`V`; in the bcc cell the atomic forces vanish by
       symmetry, so the strain force decides. A relaxation that stops
       before then is unconverged.
   * - ``--unconverged const <value>``
     - ``const 100``
     - Report this finite value for an unconverged relaxation.
   * - ``--unconverged force-scaled <slope>``
     - Unset
     - Instead report ``target + slope * fmax`` for an unconverged
       relaxation. The slope must be finite.

The two ``--unconverged`` forms select alternative behaviours.
An unconverged relaxation returning a finite substitute still supplies
that value to its configured loss. A relaxed lattice constant outside
the allowed window, or a non-finite result or loss, fails the term and
therefore the evaluation. Each option is given once on a line, and a
target that the term's loss cannot measure is refused
(:doc:`nfp_losses`).

Each relaxation starts from the lattice constant that the previous
evaluation of its sequence relaxed to. Each replica of
``TADAH:ANNEAL_PT``, each walker of ``TADAH:ANNEAL_PA`` and each position
in an ASA block has its own sequence. Every other optimiser evaluates on
the first one, so the lattice constant carries over from one
``MULTISTART`` start, and from one ``PRESTAGE`` phase, to the next.

.. versionchanged:: 1.6.0
   A term takes the root ``LOSS`` above its line; 1.5.0 took ``L2``
   unless ``--loss`` named another. ``--fmax-converged`` sets the
   tolerance of the relaxation; 1.5.0 relaxed to 0.1 and the option
   changed nothing in the bcc cell.

Gradient compatibility
----------------------

For a phase that uses gradients, an omitted ``GRAD_STRATEGY`` resolves
to ``ANALYTICAL`` when typed observables are present. Explicit
``ANALYTICAL`` and ``HYBRID`` are accepted; ``NUMERICAL`` is refused.
Derivative-free phases use loss values alone.

``ANALYTICAL`` requires a supported linear fit with training data,
``OALGO 4``, fixed ``LAMBDA >= 0``, ``NORM false``, ``STRESS false``
and no ``FIXINDEX`` pins or ``PC_SRMSE_GPA`` target. Startup also checks
the selected descriptor parameters. For typed observables,
``--unconverged force-scaled`` requires the numerical contribution
provided by ``HYBRID``. A term has an analytical derivative under every
:doc:`loss function <nfp_losses>`, ``MAPE`` and ``RMSLE`` included.

.. versionchanged:: 1.6.0
   1.5.0 took finite differences for a term under ``MAPE`` or ``RMSLE``.

``GRAD_STRATEGY`` belongs to the phase owning the objective. An NLopt
``INNER`` search obtains gradients through that enclosing phase.
