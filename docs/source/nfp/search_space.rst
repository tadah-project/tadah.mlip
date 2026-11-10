.. _nfp_search_space:

Search Space
============

This guide shows how to choose the search parameters of a nested fitting
search, their bounds and their coordinates. It builds on the
:ref:`nfp_quickstart` and :ref:`nfp_target_file`; the entry of :hpo:key:`OPTIM`
gives the syntax of every :hpo:key:`!OPTIM` line shown here.

.. contents::
   :local:
   :depth: 1
   :backlinks: none

.. _nfp_search_space_parameters:

Selecting search parameters
---------------------------

An :hpo:key:`OPTIM` line selects values of a key of the :term:`training
configuration`. Each selected value becomes a :term:`search parameter`, one
dimension of the :term:`search space`. Every :term:`evaluation` trains the
potential with the candidate values in place of those of the training
configuration (:ref:`nfp_training_hpo_candidate`).

The recommended first search space is small: the search parameters the loss
depends on most, such as the cutoffs and the many-body density width. The
budget of a search grows with their number (:ref:`nfp_choosing_budget`).
:ref:`Unit 2 <nfp_unit_2>` selects the cutoffs and the density width:

.. literalinclude:: files/tutorial/unit2/config.hpo
   :language: none
   :start-at: # The box of the pair cutoff begins
   :end-at: SGRIDMB (1)

A line names a key of the training configuration (:ref:`ConfigSection`) and an
index list; the :hpo:key:`!OPTIM` entry gives how the indices count. For a grid
written as a generator, such as ``CGRID2B LIN 17 2.0 6.0`` in Unit 2, the
indices select the values it generates.

Only continuous values can be searched. They include descriptor parameters such
as cutoffs, centres and widths, :ref:`WATOM`, the penalty :ref:`LAMBDA` and
the kernel parameters :ref:`MPARAMS`. The :term:`error scales <error scale>`
and dataset :term:`priorities <priority>` of the fit (:ref:`fit_objective`)
can be searched too. ``tadah hpo`` refuses, among others, a choice, an
integer, a key that prepares the data once (:ref:`nfp_training_hpo_reads`)
and :ref:`WEIGHTS` (:ref:`nfp_training_hpo_fit`). The :hpo:key:`!OPTIM` entry
gives the general refusals; a :hpo:key:`list <OPTIM.TARGET.WEIGHTS>` gives
the keys refused for a reason of their own.

.. _nfp_search_space_order:

Order of the search parameters
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The search parameters are sorted by key name (upper case, character by
character), then by index, whatever the order of the :hpo:key:`!OPTIM` lines;
for :ref:`FIXWEIGHT` the position of the coefficient in :ref:`FIXINDEX` orders
them. ``tadah hpo --check`` lists them in this order, which numbers the columns
of ``params.tadah``, the rows of ``hpo_search_space.tsv`` and the labels in the
journal (:ref:`nfp_output_histories`). It also orders the per-dimension
``PARAM`` keys of :hpo:opt:`ASA:ASA` (:hpo:param:`PARAM.ASA:ASA`) and the grid
of :hpo:opt:`TADAH:GRID`.

.. _nfp_search_space_bounds:

Choosing bounds
---------------

Each search parameter is searched inside a box. The box is written on the line
(:hpo:key:`range <OPTIM.range>`), or :hpo:key:`OPTIM.AUTO` derives it from the
value of the training configuration. It keeps to the lower limit of its key.

Explicit bounds are the recommended choice for a first search, where the values
of the training configuration are guesses. Physics then sets them: neighbour
shells and the support of the blips for a cutoff, the density ratio at two
shells for a width, as the comments of :ref:`Unit 2 <nfp_unit_2>` show.
:ref:`RCUT2B` and :ref:`RCUTMB` take explicit bounds only: one neighbour list
is built, for the largest upper bound of their boxes.

:hpo:key:`!OPTIM.AUTO` suits a search around a trusted start, such as the
result of an earlier search. :hpo:key:`RATIO` gives a geometric box and
:hpo:key:`FRAC` a linear one; the :hpo:key:`!OPTIM.AUTO` entry gives the
default for each key. :ref:`Unit 3 <nfp_unit_3>` searches the density width
by a factor either side of the result of :ref:`Unit 2 <nfp_unit_2>`:

.. literalinclude:: files/tutorial/unit3/config.hpo
   :language: none
   :start-at: AUTO RATIO
   :end-at: AUTO RATIO

A starting point lies inside its box; ``tadah hpo`` stops at startup otherwise
(:ref:`nfp_training_hpo_start`). With ``CGRID2B 1.0 2.0`` in the training
configuration and no :hpo:block:`INIT` block, the line below stops the run:

.. hpo:refusal::

   OPTIM CGRID2B (2) 2.5 3.5

A :hpo:key:`WARM <METHOD.WARM>` seed outside its box is handled as
:hpo:key:`WARM_ON_MISMATCH` sets. ``tadah hpo --check``
(:ref:`cli_hpo`) prints every box, :hpo:key:`!OPTIM.AUTO` boxes resolved; a
run writes them to its output files (:ref:`nfp_output_files`).

When the best value lies within 2 % of the width of its box from a bound,
measured in the coordinate of its shape (the logarithm for a :hpo:key:`LOG`
box), the minimum may lie beyond that bound. ``DIAGNOSTICS`` in the journal
lists each such search parameter (:ref:`Unit 3 <nfp_unit_3>`) and marks with
``LO*`` a lower bound that cannot move, such as an amplitude at 0.

A wider box changes a local search even when its best lies inside the box.
The first step, a fixed :hpo:key:`STEP` fraction of the box, grows with it,
and under :hpo:key:`OPTIM_SCALE` ``AUTO`` the start takes other coordinates
(:ref:`nfp_search_space_scaling`): the search takes another path and, on a
flat loss, can end at another point. A higher upper bound of a cutoff
lengthens the neighbour list and slows every evaluation.

Two commands help with the bounds of a descriptor
(:ref:`cli_analysis_suggest_bounds`, :ref:`cli_analysis_check_bounds`). ``tadah
analysis suggest_bounds`` proposes :hpo:key:`OPTIM` lines from the training
data and configuration, as suggestions to check. ``tadah analysis
check_bounds`` runs the pre-flight audit (:ref:`AUDIT`) at the box corners
(sampled above 12 search parameters) and exits with an error if one fails.

.. _nfp_search_space_transforms:

Choosing shapes
---------------

A tag at the end of an :hpo:key:`OPTIM` line can set the shape of the
coordinate the optimiser moves along; the entries of :hpo:key:`LIN`,
:hpo:key:`LOG` and :hpo:key:`ASINH` give each shape and the default.

:hpo:key:`!LOG` is the recommended shape for a positive value whose box spans
a large ratio, such as a width, an error scale or :ref:`LAMBDA`. Equal steps
then change the value by equal factors, so a step resolves the small end of
the box as finely as the large end (:ref:`nfp_choosing_step`).

:hpo:key:`!ASINH` suits a box that crosses zero and spans orders of magnitude
on either side. It is linear within about ``c`` of zero and logarithmic
beyond, so ``c`` is about the smallest magnitude that matters.

:hpo:key:`LOG_HP` log-scales every untagged box above zero whose ratio of
bounds reaches its value; a tag on the line takes precedence, :hpo:key:`!LIN`
included. A tag on each line is the recommended choice: the shape of each
search parameter is then written next to its box. :hpo:key:`!LOG_HP` suits a
target file of many lines written by a tool.

.. versionchanged:: 1.6.0
   A box tagged :hpo:key:`!LIN` stays linear under :hpo:key:`!LOG_HP`;
   1.5.0 log-scaled it as an untagged box.

.. _nfp_search_space_scaling:

Scaling the search space
------------------------

:hpo:key:`OPTIM_SCALE` sets the coordinates the optimiser works in. The
default, ``AUTO``, maps every box, in the coordinate its shape gives, onto the
unit interval. A tolerance on the search parameters or a finite-difference step
then means the same fraction of every box: :hpo:key:`XTOL_ABS` and
:hpo:param:`fd_step` hold for all search parameters at once.

``AUTO`` is the recommended choice. ``NONE`` keeps each search parameter in its
own units (value, logarithm or ``asinh(x/c)``), for tolerances in those units.

.. _nfp_search_space_components:

Searching descriptor components
-------------------------------

A :ref:`DM_Embed <dm_embed>` many-body term is searched through the paths of
its parameters, not through flat many-body keys (:ref:`dm_embed_parameters`).

A :ref:`DM_REAM <dm_ream>` term searches its density through :ref:`CGRIDMB`,
:ref:`SGRIDMB`, :ref:`AMPGRIDMB` and :ref:`RCUTMB`. The amplitudes are search
parameters like the others; the fit does not solve them. The density form
:ref:`RHOFORM` and the embedding reference :ref:`EMBF0` are choices and cannot
be searched.

Under ``NONNEG`` and ``MONOTONE`` (:ref:`density forms <dm_ream_rhoform>`)
every amplitude box lies at or above zero, and a linear :hpo:key:`OPTIM.AUTO`
box is cut at zero: the search gives no negative density. Under ``SIGNED`` a
density can fall below zero; :hpo:key:`RHOGATE`, when set, rejects a candidate
whose density falls below zero by more than its tolerance
(:ref:`nfp_search_space_feasibility`).

.. _nfp_search_space_coefficients:

Searching pinned coefficients
-----------------------------

Coefficients that the training data do not constrain, such as those of
embedding functions above the densities of the data, can be pinned instead
of fitted and searched against the validation data.

:ref:`FIXINDEX` and :ref:`FIXWEIGHT` in the training configuration pin the
coefficients, and :hpo:key:`OPTIM FIXWEIGHT <OPTIM.TARGET.FIXWEIGHT>`
searches their values. The indices of that line are coefficient numbers,
those :ref:`FIXINDEX` lists, not positions in the :ref:`FIXWEIGHT` list.
The :ref:`nfp_example_pinned_coefficients` example pins the embedding blips
above the training densities:

.. literalinclude:: files/examples/pinned_coefficients/config.train
   :language: ini
   :start-at: FIXINDEX
   :end-at: FIXWEIGHT

and searches one of them:

.. literalinclude:: files/examples/pinned_coefficients/config.hpo
   :language: none
   :start-at: OPTIM
   :end-at: OPTIM

The fit solves the free coefficients, and the other pinned ones stay fixed;
:ref:`nfp_training_hpo_fit` covers a configuration that pins every one.

.. _nfp_search_space_feasibility:

Checking feasibility
--------------------

Some candidates in a box give a potential that is not usable. The checks
below reject such a candidate before training; it costs no fit and scores as
a failed evaluation (:ref:`nfp_targets_failures`).

:hpo:key:`BUMPGATE` acts on compact bump functions under a discontinuous cutoff,
such as ``Cut_Dummy``, and on a ``DM_Embed`` density without a cutoff, whose
``RADIUS`` takes its place. A bump that reaches past the cutoff ends in a step
of the energy. ``CANDIDATE``, the default, is the recommended choice: it rejects
each candidate with such a bump and lets the search use the rest of the box.
``STRICT`` refuses at startup a box that can give such a bump, so that no
evaluation is rejected; the entry gives the startup checks of both. A smooth
cutoff gives no step, and :hpo:key:`!BUMPGATE` has nothing to check.

:hpo:key:`RHOGATE` and :hpo:key:`RHOBARGATE` are off unless the target file
sets them. They check the density of a single ``DM_REAM`` term or a single
``DM_Embed`` component, and need a search of a density key besides :ref:`WATOM`:
a target file without one is refused (:hpo:topic:`RULE.density_key_arming`).
The entries give the startup checks of the starting points.

- :hpo:key:`!RHOGATE` rejects a ``SIGNED`` density that falls below zero by
  more than a set fraction of its largest value.
- :hpo:key:`!RHOBARGATE` rejects, under every form, a density whose sum over
  a perfect lattice does not fall as the lattice expands, over a set range
  of lattice constants. A lattice density that rises with the lattice
  constant lets an expanded and a compressed crystal share one density, and
  so one embedding energy.
