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

``--species <element>``
   Required. The element of the crystal and of the isolated atom: an
   element symbol, and an element of the potential.

``--target <energy>``
   Required. Finite target cohesive energy in eV/atom.

``--weight <weight>``
   Required. Finite, nonnegative loss weight.

``--hotvar a0 <lo> <hi> [<initial>]``
   Required. Lattice-constant window in Å, with finite bounds and
   ``hi > lo``. Only ``a0`` is supported. The initial value defaults to
   the midpoint; one outside the window is refused.

``--lattice bcc``
   Default ``bcc``, the only lattice implemented.

``--supercell <nx> <ny> <nz>``
   Default ``4 4 4``. Positive integer repeats along each axis, up to
   2147483647.

``--loss <name> [<parameter>]``
   Default: the top-level :hpo:key:`LOSS` read above the line (``L2``
   without one). One :doc:`loss name <nfp_losses>`, with the parameter
   of ``HUBER`` and ``TUKEY``.

Relaxation options
------------------

``--maxiter <n>``
   Default ``5000``. Relaxation iteration cap; integer from 0 to
   2147483647.

``--maxeval <n>``
   Default ``5000``. Energy/force evaluation cap; integer from 0 to
   2147483647.

``--etol <value>``
   Default ``0``. Relative energy-change convergence tolerance; a
   positive value enables it. Zero leaves force-based convergence.

``--max-wall-time <seconds>``
   Default ``0``. A positive value caps the wall-clock time of each
   relaxation; zero disables the cap.

``--fmax-converged <force>``
   Default ``0.1``. Force tolerance of the relaxation, above 0: the
   relaxation stops when the largest atomic force, in eV/Å, and the
   strain force of the cell,
   :math:`|\partial E/\partial\varepsilon|/(3V)` for the isotropic
   strain :math:`\varepsilon` and the cell volume :math:`V`, both fall
   below it. The strain force is the magnitude of the cell's pressure,
   in eV/Å³: the default 0.1 allows 0.1 eV/Å³, about 16 GPa. In the bcc
   cell the atomic forces vanish by symmetry, so the strain force
   decides.

   A relaxation that ends at ``--maxiter``, ``--maxeval`` or
   ``--max-wall-time`` before both fall below it, or when no step lowers
   its energy, is unconverged; one that ``--etol`` stops is converged
   when its largest atomic force is below the tolerance.

``--unconverged const <value>``
   Default ``const 100``. The finite value an unconverged relaxation
   reports.

``--unconverged force-scaled <slope>``
   An unconverged relaxation reports ``target + slope * fmax`` instead,
   ``fmax`` being the larger of the largest atomic force and the strain
   force at the end. The slope must be finite.

The two ``--unconverged`` forms select alternative behaviours.
An unconverged relaxation returning a finite substitute still supplies
that value to its configured loss. A relaxed lattice constant outside
the allowed window, or a non-finite result or loss, fails the term and
therefore the evaluation. Each option is given once on a line, and a
target that the term's loss cannot measure is refused
(:doc:`nfp_losses`).

Each relaxation starts from the lattice constant at which the previous
relaxation ended, within the same replica, walker or ASA trial-state slot
where the optimiser has them (see
:hpo:key:`cohesive_energy <PC_OBSERVABLE.cohesive_energy>`).

.. versionchanged:: 1.6.0
   A term takes the top-level ``LOSS`` above its line; 1.5.0 took ``L2``
   unless ``--loss`` named another. ``--fmax-converged`` sets the
   tolerance of the relaxation, and ``force-scaled`` also scales with
   the strain force; 1.5.0 relaxed to 0.1, and compared the one and
   scaled the other with the atomic forces alone.

Gradient compatibility
----------------------

For a phase that uses gradients, an omitted ``GRAD_STRATEGY`` resolves
to ``ANALYTICAL`` when typed observables are present. Explicit
``ANALYTICAL`` and ``HYBRID`` are accepted; ``NUMERICAL`` is refused.
Derivative-free phases use loss values alone.

``ANALYTICAL`` needs the settings of the analytical gradient listed under
:hpo:key:`GRAD_STRATEGY <OPTIMIZER.GRAD_STRATEGY>`. For typed
observables, ``--unconverged force-scaled`` requires the numerical
contribution provided by ``HYBRID``. A term has an analytical derivative
under every :doc:`loss function <nfp_losses>`, ``MAPE`` and ``RMSLE``
included.

.. versionchanged:: 1.6.0
   1.5.0 took finite differences for a term under ``MAPE`` or ``RMSLE``.

``GRAD_STRATEGY`` belongs to the phase owning the objective. An NLopt
``INNER`` search obtains gradients through that enclosing phase.
