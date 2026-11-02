.. _hpo_noise_block:

.. _noise-block:

Noise Settings: NOISE
=====================

This page explains how the :hpo:block:`NOISE` block derives the
finite-difference step from the search space and measures the noise of the
loss before the search. It builds on the :hpo:block:`OPTIMIZER` block (see
:doc:`nfp_optimizer`).

:hpo:block:`!NOISE` is an optional block inside :hpo:block:`!OPTIMIZER`.
Its two keys are independent: :hpo:key:`AUTO <NOISE.AUTO>` chooses a finite-difference
step from the search box, while :hpo:key:`CALIBRATION <NOISE.CALIBRATION>` repeats a starting
point to measure loss variation. The syntax, values and defaults of the
two keys are in their reference entries.

.. code-block:: none

   NOISE
     AUTO true
     CALIBRATION 5
   ENDNOISE

Automatic finite-difference step
--------------------------------

For ``d`` dimensions, :hpo:key:`!AUTO <NOISE.AUTO>` derives::

   fd_step = max(1e-3 * box_diagonal / sqrt(d), 1e-6)

The diagonal is measured in the coordinates of the search, after the bound
transforms (see :doc:`nfp_search_space`). This box-size heuristic depends
solely on the search bounds and dimension count.

The step is the finite-difference step of the optimiser named in the
:hpo:block:`!OPTIMIZER` block, including each ``MULTISTART`` search
or ``PRESTAGE`` refinement. The :hpo:block:`EXPLORE` block retains
its own finite-difference settings. :hpo:key:`!AUTO <NOISE.AUTO>` requires an optimiser
that takes finite differences and an omitted ``PARAM fd_step``. The
resulting step must fit every search box; see :doc:`nfp_gradients`.

Repeated evaluations
--------------------

:hpo:key:`!CALIBRATION <NOISE.CALIBRATION>` ``N`` evaluates the first starting point of that
optimiser ``N`` times before the optimiser runs. These evaluations appear
in the evaluation history and can update the best potential. They are
outside the :hpo:key:`MAXEVAL <OPTIMIZER.MAXEVAL>` budget of the search, so
the total cost of a run includes them. If no starting point exists,
calibration is skipped with a warning.

The journal reports the losses and suggests an
:hpo:key:`FTOL_REL <OPTIMIZER.FTOL_REL>` from their spread. Applying the
suggested tolerance requires an explicit setting; see
:doc:`nfp_stopping_settings` for supported tolerances.
