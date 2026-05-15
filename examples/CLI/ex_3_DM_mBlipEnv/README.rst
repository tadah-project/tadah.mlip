Example 3 - DM_mBlipEnv: Train and Predict
------------------------------------------

This example exercises the ``DM_mBlipEnv`` many-body descriptor.

What the descriptor does
........................

``DM_mBlipEnv`` mixes two different basis families:

* The atomic **density** ``rho`` is built from a **Gaussian** radial
  basis - Gaussian-type orbitals ``exp(-eta*(r - r_s)^2)`` summed over
  the neighbours of each atom. ``CGRIDMB`` sets the centre ``r_s`` and
  ``SGRIDMB`` the inverse width ``eta``.
* The **embedding** function ``F(rho)`` is built from **blip** (cubic
  B-spline) basis functions in ``rho``-space, each multiplied by an
  inverse-cutoff envelope (``Cut_SinInv``) of width ``RCUTENV``.
  ``CEMBFUNC`` / ``SEMBFUNC`` set the blip centres and widths.

So the descriptor is a *Gaussian density* fed through a *blip embedding*.
The blip embedding is a flexible spline basis: with enough blips it can
represent essentially **any EAM-style embedding function** ``F(rho)``.
The envelope enforces the physical EAM boundary condition
``F(rho) -> 0`` as ``rho -> 0`` - an atom with no neighbours contributes
no embedding energy - by smoothly ramping ``F`` down over the
low-density region ``rho < RCUTENV``.

Restriction on ``L``
....................

The ``L`` integer in ``TYPEMB`` is the maximum orbital angular momentum.
``DM_mBlipEnv`` is rotationally invariant **only for L = 0**: its
embedding function is not ``F_SQ`` (``rho^2``), so for ``L > 0`` the
embedding is applied to individual orbital densities before the
invariance-restoring quadratic sum and the descriptor is **not**
rotationally invariant (the code emits a runtime ``WARNING``). ``Lmax``
is also hard-capped at 6. This example therefore uses ``L = 0``. For a
rotationally invariant ``L > 0`` many-body descriptor use the
``DM_PS_*`` family instead.

Files provided
..............

* ``config.train`` - training settings using ``DM_mBlipEnv``.
* ``tdata.db``     - 50 thermally displaced (MD) snapshots of bcc Ta,
  each a 54-atom 3x3x3 conventional supercell (lattice constant
  ``a0 ~ 3.32`` Angstrom). The set is intentionally small so the
  example runs in seconds.

The ``TYPEMB`` line
...................

``TYPEMB DM_mBlipEnv 0 1 1 5 5 Ta Ta`` - the integers, in order, are:

* ``0`` - ``L``, the maximum orbital angular momentum (see above);
* ``1`` - ``size(CGRIDMB)``, number of radial Gaussian centres;
* ``1`` - ``size(SGRIDMB)``, number of radial Gaussian widths;
* ``5`` - ``size(CEMBFUNC)``, number of embedding blip centres;
* ``5`` - ``size(SEMBFUNC)``, number of embedding blip widths;

followed by the element pair the descriptor block applies to (``Ta Ta``).
``CGRIDMB`` / ``SGRIDMB`` must have size 1 - the dimension-expanding blip
embedding needs a single radial basis function; the descriptor output
dimension comes from ``CEMBFUNC`` / ``SEMBFUNC`` instead.

Density range and ``WATOM``
...........................

With the radial basis in ``config.train`` (``CGRIDMB = 2.88``,
``SGRIDMB = 8.0``, ``RCUTMB = 5.0``, ``Cut_Cos``) the *raw* per-atom
density of this dataset has a median of ~3.24. The ``WATOM`` key applies
a per-species weight to every neighbour's contribution to ``rho``;
``WATOM = 0.309`` (i.e. ``1/3.24``) rescales the density so it is ~1.
After scaling the per-atom density lies in ``rho ~ [0.89, 1.10]`` with
median ~1.00. The 5 blips (``CEMBFUNC 0.6 ... 1.4``) span ``[0.6, 1.4]``,
bracketing that band with margin, and ``RCUTENV = 0.9`` places the
envelope ramp just below the band so it enforces ``F(0)=0`` without
reshaping the embedding where training data is present.

Training
........

.. code-block:: bash

   tadah train -c config.train -v 2

The expected output ends with a trained ``pot.tadah`` potential file in
the working directory (a 5-column design matrix, one column per blip).

Prediction
..........

To verify the round-trip, predict back on the training set:

.. code-block:: bash

   tadah predict -p pot.tadah -d tdata.db -v 2 -A

``-A`` prints RMSE / R^2 analytics.

Expected results
................

The dataset is a near-uniform set of thermal bcc-Ta snapshots, so the
total energy spread is tiny (standard deviation ~7.3 meV/atom). On this
set the configuration above gives roughly:

* Energy R^2 ~ 0.72
* Energy RMSE ~ 3.9 meV/atom

i.e. the descriptor captures most of the (small) energy variance. This
is a genuine, working fit - the descriptor block is non-zero and
informative. Because the snapshots are all dense bcc environments,
``rho`` barely varies and the envelope is essentially saturated across
the data; its role here is purely to enforce ``F(0)=0`` for
transferability to low-density environments (surfaces, vacancies,
dimers) rather than to reshape the fit. For high-accuracy fits on this
dataset the two-body ``D2_Blip`` baseline in ``ex_1`` remains a strong
choice.
