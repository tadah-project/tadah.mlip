.. _hpo_noise_block:

.. _noise-block:

Noise settings: NOISE
=====================

``NOISE`` is an optional direct child of ``OPTIMIZER``. Its two settings
are independent: ``AUTO`` chooses a finite-difference step from the
search box, while ``CALIBRATION`` repeats a starting point to measure
loss variation.

.. code-block:: none

   NOISE
     AUTO true
     CALIBRATION 5
   ENDNOISE

Keys
----

.. list-table::
   :header-rows: 1
   :widths: 28 72

   * - Key
     - Values and effect
   * - ``AUTO [boolean]``
     - Default ``false``. ``AUTO`` alone means ``true``. Accepted
       booleans are ``true/false``, ``1/0``, ``yes/no`` and ``on/off``
       (case-insensitive).
   * - ``CALIBRATION N``
     - Nonnegative integer: ``0`` disables it (default); otherwise
       ``N >= 2``. ``1`` is rejected.

Automatic finite-difference step
--------------------------------

For ``d`` dimensions, ``AUTO`` derives::

   fd_step = max(1e-3 * box_diagonal / sqrt(d), 1e-6)

The diagonal is measured in transformed optimiser coordinates. This
box-size heuristic depends solely on the search bounds and dimension count.

It configures the main optimiser's finite differences, including each
``MULTISTART`` search or ``PRESTAGE`` refinement. ``EXPLORE`` retains
its own finite-difference settings. ``AUTO`` requires a main optimiser
that takes finite differences and an omitted ``PARAM fd_step``. The resulting
step must fit every search box; see :doc:`nfp_gradients`.

Repeated evaluations
--------------------

``CALIBRATION N`` evaluates the first main starting point ``N`` times
before the optimiser runs. These evaluations appear in the evaluation
history and can update the best potential. They are outside the search's
``MAXEVAL`` budget, so include them when estimating total cost. If no
starting point exists, calibration is skipped with a warning.

The journal reports the losses and suggests an ``FTOL_REL`` from their
spread. Applying the suggested tolerance requires an explicit setting;
see :doc:`nfp_stopping_settings` for supported tolerances.
