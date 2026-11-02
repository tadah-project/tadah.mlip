.. _nfp_observable_targets:

Typed Observable Targets
========================

This page describes how a :hpo:key:`PC_OBSERVABLE` line adds a loss term
on a built-in physical property. It assumes a
:doc:`target file <nfp_configuration>`.

:hpo:key:`!PC_OBSERVABLE` evaluates a built-in physical property with the
candidate potential inside the ``tadah hpo`` run, without LAMMPS. It is a
top-level key of the target file, and each line adds one loss term. The
only implemented kind is ``cohesive_energy``; its crystal is bcc. The
parser rejects files that mix these targets with :hpo:key:`PC_LAMMPS` or
:hpo:key:`PC_FAILPROBE`.

.. code-block:: none

   PC_OBSERVABLE cohesive_energy --species Ta --target 8.10 --weight 100 \
                 --hotvar a0 2.8 3.8 3.30

The value is :math:`E_\mathrm{isolated}-E_\mathrm{bulk}/N` in eV/atom,
positive for a bound crystal. The bulk cell undergoes isotropic
relaxation at zero pressure, to the pressure tolerance
:hpo:key:`--ptol <PC_OBSERVABLE.cohesive_energy.--ptol>`.
The option ``--validation`` (see :doc:`cli/cli`) is optional unless
:doc:`validation metrics <nfp_validation_targets>` are also requested.

.. _pc_observable:

Property and loss options
-------------------------

The reference entry of each option gives its syntax, range and default.

:hpo:key:`--species <PC_OBSERVABLE.cohesive_energy.--species>`
   Required. The element of the crystal and of the isolated atom, an
   element of the potential.

:hpo:key:`--target <PC_OBSERVABLE.cohesive_energy.--target>`
   Required. The target cohesive energy in eV/atom.

:hpo:key:`--weight <PC_OBSERVABLE.cohesive_energy.--weight>`
   Required. The loss-term weight of the term.

:hpo:key:`--hotvar <PC_OBSERVABLE.cohesive_energy.--hotvar>`
   Required. The window of the lattice constant ``a0`` in Å; ``a0`` is
   the only variable supported. The initial value defaults to the
   midpoint of the window.

:hpo:key:`--lattice <PC_OBSERVABLE.cohesive_energy.--lattice>`
   ``bcc`` is the only lattice implemented.

:hpo:key:`--supercell <PC_OBSERVABLE.cohesive_energy.--supercell>`
   The number of cubic cells along each axis.

:hpo:key:`--loss <PC_OBSERVABLE.cohesive_energy.--loss>`
   The loss function of the term. It defaults to the top-level
   :hpo:key:`LOSS` read above the line (``L2`` without one). One
   :doc:`loss name <nfp_losses>`, with the parameter of ``HUBER`` and
   ``TUKEY``.

Relaxation options
------------------

:hpo:key:`--maxiter <PC_OBSERVABLE.cohesive_energy.--maxiter>`
   The iteration cap of a relaxation.

:hpo:key:`--maxeval <PC_OBSERVABLE.cohesive_energy.--maxeval>`
   The cap on the energy and force evaluations of a relaxation.

:hpo:key:`--etol <PC_OBSERVABLE.cohesive_energy.--etol>`
   The relative energy-change convergence tolerance; a positive value
   enables it. Zero leaves :hpo:key:`--fmax-converged
   <PC_OBSERVABLE.cohesive_energy.--fmax-converged>` and
   :hpo:key:`--ptol <PC_OBSERVABLE.cohesive_energy.--ptol>` alone to
   decide convergence.

:hpo:key:`--max-wall-time <PC_OBSERVABLE.cohesive_energy.--max-wall-time>`
   A positive value caps the wall-clock time of each relaxation, in
   seconds; zero disables the cap.

:hpo:key:`!--fmax-converged <PC_OBSERVABLE.cohesive_energy.--fmax-converged>`
   The force tolerance of the atoms, in eV/Å: the atoms have relaxed
   when the largest atomic force falls below it. In the bcc cell the
   atomic forces vanish by symmetry, so the atoms stay in place and
   :hpo:key:`!--ptol <PC_OBSERVABLE.cohesive_energy.--ptol>` decides when
   the relaxation has converged.

:hpo:key:`!--ptol <PC_OBSERVABLE.cohesive_energy.--ptol>`
   The pressure tolerance of the cell, in GPa: the cell has relaxed when
   the magnitude of its pressure falls below it. The reference entry
   gives how the pressure is measured, the error a residual pressure
   leaves and the smallest tolerance that can be met.

:hpo:key:`--unconverged <PC_OBSERVABLE.cohesive_energy.--unconverged>` ``const <value>``
   The finite value an unconverged relaxation reports.

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

A relaxation is converged when its final cell meets both tolerances,
even if it stops at
:hpo:key:`--maxiter <PC_OBSERVABLE.cohesive_energy.--maxiter>`,
:hpo:key:`--maxeval <PC_OBSERVABLE.cohesive_energy.--maxeval>` or
:hpo:key:`--max-wall-time <PC_OBSERVABLE.cohesive_energy.--max-wall-time>`.
It is unconverged when it reaches one of these limits before meeting
both tolerances. It is also unconverged when the line search of the atoms
finds no lower energy. The same holds when the step of the cell falls
below 1e-12 of the lattice constant before the pressure meets
:hpo:key:`!--ptol <PC_OBSERVABLE.cohesive_energy.--ptol>`. A relaxation
stopped by :hpo:key:`--etol <PC_OBSERVABLE.cohesive_energy.--etol>` is
converged when its largest atomic force is below
:hpo:key:`!--fmax-converged
<PC_OBSERVABLE.cohesive_energy.--fmax-converged>`, whatever its pressure.

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

.. _nfp_observable_gradients:

Gradient compatibility
----------------------

For a phase that uses gradients, an omitted
:hpo:key:`GRAD_STRATEGY <OPTIMIZER.GRAD_STRATEGY>` resolves to
``ANALYTICAL`` when typed observables are present. Explicit
``ANALYTICAL`` and ``HYBRID`` are accepted; ``NUMERICAL`` is refused.
Derivative-free phases use loss values alone.

``ANALYTICAL`` needs the settings of the analytical gradient listed under
:hpo:key:`!GRAD_STRATEGY <OPTIMIZER.GRAD_STRATEGY>`. For typed
observables, ``--unconverged force-scaled`` requires the numerical
contribution provided by ``HYBRID``. A term has an analytical derivative
under every :doc:`loss function <nfp_losses>`, ``MAPE`` and ``RMSLE``
included.

.. versionchanged:: 1.6.0
   1.5.0 took finite differences for a term under ``MAPE`` or ``RMSLE``.

:hpo:key:`!GRAD_STRATEGY <OPTIMIZER.GRAD_STRATEGY>` belongs to the phase
owning the objective. An NLopt :hpo:block:`INNER` search obtains
gradients through that enclosing phase.
