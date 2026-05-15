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
median ~1.00.

Choosing the blip basis (``CEMBFUNC`` / ``SEMBFUNC``)
.....................................................

The blips must give an embedding ``F(rho)`` that is **finite for every
rho > 0** and goes to zero only *at* ``rho = 0``. Two facts drive the
choice:

* A blip contributes nothing outside its ``rho``-support
  ``mu +/- 2/eta``. If no blip has support at a given ``rho``, the
  embedding is identically zero there - a flat *dead zone*.
* A linear fit can only constrain a blip whose support overlaps the
  data band (``rho ~ 0.89-1.10``). A blip with no data overlap gets a
  zero weight from the regulariser, so adding narrow low-``rho`` blips
  does **not** remove the dead zone - they are simply zeroed.

The fix is to make the blips **wide**: ``SEMBFUNC = 2.0`` gives each
blip a support of width ``4/eta = 2.0``. Every blip in
``CEMBFUNC = 0.5 0.75 1.0 1.25 1.5`` then overlaps the data band (so its
weight is fit from data) *and* the lower blips reach all the way down to
``rho = 0``. The fitted ``F(rho)`` is consequently finite and non-zero
for all ``rho > 0``.

These wide blips give a non-zero embedding at ``rho = 0`` itself, and
``RCUTENV = 0.6`` is exactly what cancels it: the ``Cut_SinInv`` envelope
ramps from 0 at ``rho = 0`` up to 1 at ``rho = RCUTENV``, smoothly
pulling ``F -> 0`` as ``rho -> 0``. ``RCUTENV = 0.6`` lies below the data
band, so the envelope is saturated to 1 across ``rho ~ 0.89-1.10`` and
does not reshape the fit where training data is present.

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

* Energy R^2 ~ 0.70
* Energy RMSE ~ 4.0 meV/atom

i.e. the descriptor captures most of the (small) energy variance. This
is a genuine, working fit - the descriptor block is non-zero and
informative.

A caveat on the embedding tails: this dataset only samples ``rho ~ 1``
(dense bcc environments). The fitted ``F(rho)`` is finite and non-zero
for all ``rho > 0`` - there is no dead zone - but its *shape* away from
``rho ~ 1`` is set by the wide-blip basis and the regulariser, not by
data. The values of ``F`` at low and high ``rho`` are therefore a smooth
extrapolation, not a physically constrained embedding; pinning those
regions would need training structures that actually sample low- and
high-density environments (surfaces, vacancies, dimers, compression).
For high-accuracy fits on this dataset the two-body ``D2_Blip`` baseline
in ``ex_1`` remains a strong choice.
