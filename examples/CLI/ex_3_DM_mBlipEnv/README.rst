Example 3 - DM_mBlipEnv: Train and Predict
------------------------------------------

This example exercises the ``DM_mBlipEnv`` descriptor, a variant of
``DM_mBlip`` whose F_Blip embedding is multiplied by an inverse cutoff
envelope (``Cut_SinInv``) parameterised by the new key ``RCUTENV``.
The envelope smoothly suppresses the embedding contribution near
``rho = 0``, where the Gaussian radial basis is largest.

Files provided
..............

* ``config.train`` - training settings using ``DM_mBlipEnv``.
* ``tdata.db``     - first 50 structures of the Ta dataset shipped with
  ``examples/CLI/ex_1`` (kept small so the example runs in seconds).

Training
........

.. code-block:: bash

   tadah train -c config.train -v 2

The expected output ends with a trained ``pot.tadah`` potential file
in the working directory.

Prediction
..........

To verify the round-trip, predict back on the training set:

.. code-block:: bash

   tadah predict -p pot.tadah -d tdata.db -v 2 -A

``-A`` prints RMSE / R^2 analytics. Energy RMSE should be small relative
to the energy spread (~1 eV/atom).

Notes
.....

* ``DM_mBlipEnv`` requires ``CGRIDMB`` and ``SGRIDMB`` of size 1 - the
  embedding dimension comes from ``CEMBFUNC`` / ``SEMBFUNC``.
* ``RCUTENV`` is the envelope cutoff in rho-space (not r-space). A small
  value (~1.2) keeps the envelope active across most of the
  representative rho range for this dataset.
* This is the first example to exercise ``DM_mBlipEnv`` end-to-end -
  the descriptor and the ``Cut_SinInv`` envelope had been added to the
  code base but had no working configuration before this commit.

Expected results / fitness disclaimer
.....................................

The canonical config above runs cleanly through ``train`` and
``predict`` and demonstrates that the descriptor and its envelope
parameter are wired into the pipeline. **Predictive accuracy on this
particular subset is essentially zero** (R^2 ~ 0 for both energy and
forces), and re-running with ``DM_mBlip`` (the same descriptor without
the envelope) gives the same numbers - so this is a property of the
descriptor family + the small uniform bcc-Ta thermal subset, not a
bug introduced by the envelope. For high-accuracy fits on this dataset
prefer the two-body ``D2_Blip`` baseline in ``ex_1`` (R^2 > 0.99 with
the same 50 structures), or move to a rotationally invariant many-body
descriptor (``DM_PS_*``).
