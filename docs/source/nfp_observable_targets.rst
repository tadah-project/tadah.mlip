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
relaxation at zero pressure, to the pressure tolerance
:hpo:key:`--ptol <PC_OBSERVABLE.cohesive_energy.--ptol>`.
``--validation`` is optional unless
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
   positive value enables it. Zero leaves :hpo:key:`--fmax-converged
   <PC_OBSERVABLE.cohesive_energy.--fmax-converged>` and
   :hpo:key:`--ptol <PC_OBSERVABLE.cohesive_energy.--ptol>` alone to
   decide convergence.

``--max-wall-time <seconds>``
   Default ``0``. A positive value caps the wall-clock time of each
   relaxation; zero disables the cap.

``--fmax-converged <force>``
   Default ``0.1``. Force tolerance of the atoms, in eV/Å, above 0: the
   atoms have relaxed when the largest atomic force falls below it. In
   the bcc cell the atomic forces vanish by symmetry, so the atoms stay
   in place and :hpo:key:`!--ptol <PC_OBSERVABLE.cohesive_energy.--ptol>`
   decides when the relaxation has converged.

``--ptol <pressure>``
   Default ``1e-4``. Pressure tolerance of the cell, in GPa, above 0: the
   cell has relaxed when the magnitude of its pressure falls below it.
   The reference entry of
   :hpo:key:`--ptol <PC_OBSERVABLE.cohesive_energy.--ptol>` gives how the
   pressure is measured, the error a residual pressure leaves and the
   smallest tolerance that can be met.

``--unconverged const <value>``
   Default ``const 100``. The finite value an unconverged relaxation
   reports.

``--unconverged force-scaled <slope>``
   An unconverged relaxation reports instead the cohesive energy of the
   cell it ends at, moved away from the target by ``slope * fmax``.
   ``fmax`` is the larger of the largest atomic force, in eV/Å, and the
   magnitude of the cell's pressure, in eV/Å³, at the end. The loss of the
   term is then at least that of the cell's cohesive energy, and tends to
   it as the relaxation nears convergence. The slope is a finite number of
   at least 0.

The cell moves by a search for the zero of its pressure. The search uses
the pressure alone, so a tight
:hpo:key:`!--ptol <PC_OBSERVABLE.cohesive_energy.--ptol>` is reached also
where the energy is too flat or too noisy to show a step. The atoms move
by a line search on the energy.

A relaxation is converged when the cell it ends at meets both
tolerances, also when it stops there at
:hpo:key:`--maxiter <PC_OBSERVABLE.cohesive_energy.--maxiter>`,
:hpo:key:`--maxeval <PC_OBSERVABLE.cohesive_energy.--maxeval>` or
:hpo:key:`--max-wall-time <PC_OBSERVABLE.cohesive_energy.--max-wall-time>`.
It is unconverged when it reaches one of these limits first, or when
the line search of the atoms finds no lower energy. So is one whose step
of the cell falls below 1e-12 of the lattice constant before the
pressure meets :hpo:key:`!--ptol <PC_OBSERVABLE.cohesive_energy.--ptol>`.
One that :hpo:key:`--etol <PC_OBSERVABLE.cohesive_energy.--etol>` stops
is converged when its largest atomic force is below
:hpo:key:`!--fmax-converged
<PC_OBSERVABLE.cohesive_energy.--fmax-converged>`, whatever its pressure.

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
   A term takes the top-level :hpo:key:`LOSS` above its line; 1.5.0 took
   ``L2`` unless :hpo:key:`--loss <PC_OBSERVABLE.cohesive_energy.--loss>`
   named another. The cell relaxes to
   :hpo:key:`!--ptol <PC_OBSERVABLE.cohesive_energy.--ptol>` and the atoms
   to :hpo:key:`!--fmax-converged
   <PC_OBSERVABLE.cohesive_energy.--fmax-converged>`; 1.5.0 relaxed both
   to a fixed 0.1 (16 GPa for the cell), and ``force-scaled`` reported the
   target plus the slope times the largest atomic force.

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
